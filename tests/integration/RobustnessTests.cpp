// Stage 9, end to end: no critical failures. Every catalogue model goes through the whole
// pipeline on both machines; and random sequences of edits — the user trying things, also
// impossible ones — never leave a broken product behind, and the pipeline keeps producing files.
//
// The random sequences are reproducible: a failure names its seed and step.

#include "readycad/core/model/Drawer.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/production/Pipeline.hpp"
#include "readycad/storage/Storage.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace rc;

namespace {

nesting::NestingSettings quick(nesting::Machine machine)
{
    nesting::NestingSettings s;
    s.machine = machine;
    s.optimizer.populationSize = 4;
    s.optimizer.generations = 2;
    return s;
}

/// Runs the whole pipeline on the document into `folder`; checks what must always hold.
void producePackage(const editor::Document& doc, nesting::Machine machine,
                    const std::filesystem::path& folder)
{
    const production::Order order{doc, 1};
    const nesting::NestingResult plan =
        production::planCutting(order, nesting::StockManager{}, quick(machine));
    const production::Package package = production::writePackage(
        order, &plan, cam::defaultProfile(), {}, exporting::PdfFonts{}, folder);

    int pieces = 0;
    for (const nesting::NestingPart& p : plan.parts) {
        pieces += p.quantity;
    }
    int unplaced = 0;
    for (const auto& u : plan.unplaced) {
        unplaced += u.quantity;
    }
    CHECK(plan.placedCount() + unplaced == pieces);
    CHECK(package.partDrawings.size() == doc.cutList().size());
    CHECK(package.sheetDrawings.size() == plan.layouts.size());
    CHECK(package.programs.size()
          == (machine == nesting::Machine::NestingCenter ? plan.layouts.size() : 0U));
    CHECK(std::filesystem::exists(package.summary));
}

/// Everything a user can click on, looked up in the current tree (undo/redo rebuild it).
struct Targets {
    std::vector<core::Section*> sections;
    std::vector<core::Shelf*> shelves;
    std::vector<core::Door*> doors;
    std::vector<core::FrontPanel*> fronts;
    std::vector<core::PanelComponent*> panels;
    std::vector<core::FurnitureComponent*> removable; ///< and some that are not
};

Targets targetsOf(core::Product& product)
{
    Targets t;
    product.visit([&t](core::FurnitureComponent& c) {
        switch (c.kind()) {
        case core::ComponentKind::Section:
            t.sections.push_back(static_cast<core::Section*>(&c));
            break;
        case core::ComponentKind::Shelf:
            t.shelves.push_back(static_cast<core::Shelf*>(&c));
            t.removable.push_back(&c);
            break;
        case core::ComponentKind::Door:
            t.doors.push_back(static_cast<core::Door*>(&c));
            t.removable.push_back(&c);
            break;
        case core::ComponentKind::DrawerStack:
        case core::ComponentKind::SideWall: // cannot be removed: the edit must be refused
        case core::ComponentKind::Partition:
            t.removable.push_back(&c);
            break;
        default:
            break;
        }
        if (auto* front = dynamic_cast<core::FrontPanel*>(&c)) {
            t.fronts.push_back(front);
        }
        if (auto* panel = dynamic_cast<core::PanelComponent*>(&c)) {
            t.panels.push_back(panel);
        }
    });
    return t;
}

class RandomUser {
public:
    explicit RandomUser(unsigned seed) : rng_(seed) {}

    int number(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng_); }
    bool coin() { return number(0, 1) == 1; }
    template <typename T>
    T* any(const std::vector<T*>& items)
    {
        return items.empty() ? nullptr
                             : items[static_cast<std::size_t>(number(0, static_cast<int>(items.size()) - 1))];
    }

    /// One edit, described for the log. May be refused by the editor (that is fine).
    std::string edit(editor::Document& doc)
    {
        Targets t = targetsOf(doc.product());
        switch (number(0, 16)) {
        case 0: {
            // Sometimes impossible on purpose: zero or negative sizes are refused.
            const core::Size3D size{number(0, 9) == 0 ? -100.0 : number(300, 2800) * 1.0,
                                    number(300, 2700) * 1.0, number(250, 750) * 1.0};
            editor::setDimensions(doc, size);
            return "dimensions";
        }
        case 1:
            if (core::Section* s = any(t.sections)) {
                editor::splitSection(doc, s->id(), coin() ? core::Axis::X : core::Axis::Y,
                                     number(1, 3));
            }
            return "split";
        case 2:
            if (core::Section* s = any(t.sections)) {
                editor::clearSection(doc, s->id());
            }
            return "clear";
        case 3:
            if (core::Section* s = any(t.sections)) {
                editor::setShelfCount(doc, s->id(), number(0, 5),
                                      coin() ? core::ShelfMount::Supports : core::ShelfMount::Confirmat);
            }
            return "shelves";
        case 4:
            if (core::Section* s = any(t.sections)) {
                editor::setDoorCount(doc, s->id(), number(0, 2));
            }
            return "doors";
        case 5:
            if (core::Section* s = any(t.sections)) {
                editor::setDrawerCount(doc, s->id(), number(0, 4));
            }
            return "drawers";
        case 6:
            if (core::Section* s = any(t.sections)) {
                editor::setSectionFixedSize(doc, s->id(),
                                            coin() ? std::nullopt
                                                   : std::optional<core::Real>(number(100, 900)));
            }
            return "fixed size";
        case 7:
            if (core::Shelf* s = any(t.shelves)) {
                editor::setShelfMount(doc, s->id(),
                                      coin() ? core::ShelfMount::Supports : core::ShelfMount::Confirmat);
            }
            return "shelf mount";
        case 8:
            if (core::Shelf* s = any(t.shelves)) {
                editor::setShelfElevation(doc, s->id(),
                                          coin() ? std::nullopt
                                                 : std::optional<core::Real>(number(0, 1500)));
            }
            return "shelf elevation";
        case 9:
            if (core::Door* d = any(t.doors)) {
                editor::setHinge(doc, d->id(), coin() ? core::HingeSide::Left : core::HingeSide::Right);
            }
            return "hinge";
        case 10:
            if (core::FrontPanel* f = any(t.fronts)) {
                static const char* const kModels[] = {"knob-30", "bar-128", "bar-256", "no-such"};
                editor::setHandle(doc, f->id(),
                                  coin() ? std::nullopt
                                         : std::optional<core::HandlePlacement>(
                                               core::HandlePlacement{kModels[number(0, 3)]}));
            }
            return "handle";
        case 11:
            if (core::PanelComponent* p = any(t.panels)) {
                const int pick = number(0, 3);
                editor::setMaterialOverride(
                    doc, p->id(),
                    pick == 0 ? std::nullopt
                              : std::optional<core::Material>(core::Material{
                                    "Suntalam " + std::to_string(pick == 1 ? 16 : pick == 2 ? 18 : 25),
                                    core::MaterialType::Chipboard, pick == 1 ? 16.0 : pick == 2 ? 18.0 : 25.0,
                                    true}));
            }
            return "material";
        case 12:
            if (core::FurnitureComponent* c = any(t.removable)) {
                editor::removeComponent(doc, c->id());
            }
            return "remove";
        case 13:
            if (core::PanelComponent* p = any(t.panels)) {
                editor::setInCuttingPlan(doc, {p->id()}, coin());
            }
            return "cutting plan";
        case 14:
            switch (number(0, 2)) {
            case 0:
                editor::setTopJoint(doc, coin() ? core::JointType::Overlay : core::JointType::Inset);
                break;
            case 1:
                editor::setBottomJoint(doc, coin() ? core::JointType::Overlay : core::JointType::Inset);
                break;
            default:
                editor::setBackMount(doc, static_cast<core::BackMountType>(number(0, 2)));
                break;
            }
            return "construction";
        case 15:
            doc.undo();
            return "undo";
        default:
            doc.redo();
            return "redo";
        }
    }

private:
    std::mt19937 rng_;
};

/// What must hold after any edit, refused or not.
void checkProduct(const editor::Document& doc)
{
    const core::Product& product = doc.product();
    for (const core::FurnitureComponent* c : core::collectPanels(product)) {
        INFO(core::toString(c->kind()) << " " << c->name());
        const core::Panel& panel = *c->panel();
        CHECK(std::isfinite(panel.finishLength()));
        CHECK(std::isfinite(panel.finishWidth()));
        CHECK(panel.finishLength() >= 0.0);
        CHECK(panel.finishWidth() >= 0.0);
    }
    int quantity = 0;
    for (const detailing::CutListRow& r : doc.cutList()) {
        quantity += r.quantity;
    }
    CHECK(quantity == static_cast<int>(core::collectPanels(product).size()));
    for (const core::Overlap& o : core::findOverlaps(product, 0.01)) {
        FAIL_CHECK("overlap " << core::toString(o.first->kind()) << " / "
                              << core::toString(o.second->kind()) << " size "
                              << o.region.size().x << " x " << o.region.size().y << " x "
                              << o.region.size().z);
    }
    // The file of the product opens again as the same product.
    const std::string saved = storage::saveProduct(product);
    CHECK(storage::saveProduct(*storage::loadProduct(saved)) == saved);
}

/// Returns how many edits were accepted (the others were refused as impossible).
int randomSession(unsigned seed, int steps, int pipelineEvery)
{
    int accepted = 0;
    const rc::test::TempFolder temp("random_" + std::to_string(seed));
    RandomUser user(seed);
    const auto items = core::catalogItems();
    editor::Document doc = editor::Document::fromCatalog(
        items[static_cast<std::size_t>(user.number(0, static_cast<int>(items.size()) - 1))]);

    for (int step = 1; step <= steps; ++step) {
        const std::string before = storage::saveProduct(doc.product());
        std::string what = "?";
        try {
            what = user.edit(doc);
            ++accepted;
        } catch (const std::logic_error& refused) {
            // A refused edit (invalid argument, impossible change) leaves the product as it was.
            INFO("seed " << seed << ", step " << step << ": refused " << refused.what());
            CHECK(storage::saveProduct(doc.product()) == before);
        }
        INFO("seed " << seed << ", step " << step << ": " << what);
        checkProduct(doc);
        if (step % pipelineEvery == 0) {
            const auto machine = (step / pipelineEvery) % 2 == 0 ? nesting::Machine::PanelSaw
                                                                 : nesting::Machine::NestingCenter;
            producePackage(doc, machine, temp.path() / ("step" + std::to_string(step)));
        }
    }
    return accepted;
}

} // namespace

TEST_CASE("Every catalogue model goes from the catalogue to the files on both machines",
          "[integration][robustness]")
{
    const rc::test::TempFolder temp("catalogue_pipeline");
    for (const core::CatalogItem item : core::catalogItems()) {
        const editor::Document doc = editor::Document::fromCatalog(item);
        for (const nesting::Machine machine :
             {nesting::Machine::PanelSaw, nesting::Machine::NestingCenter}) {
            INFO("item " << static_cast<int>(item) << ", machine " << static_cast<int>(machine));
            producePackage(doc, machine,
                           temp.path()
                               / (std::to_string(static_cast<int>(item)) + "_"
                                  + std::to_string(static_cast<int>(machine))));
        }
    }
}

TEST_CASE("Random edits never break the product or the pipeline", "[integration][robustness]")
{
    for (const unsigned seed : {1U, 2U, 3U}) {
        // Most edits must really change something, or the test proves little.
        CHECK(randomSession(seed, 40, 20) > 20);
    }
}

TEST_CASE("Random edits, long run", "[.][soak]")
{
    int accepted = 0;
    for (unsigned seed = 1; seed <= 40; ++seed) {
        accepted += randomSession(seed, 80, 20);
    }
    WARN("accepted edits: " << accepted << " of " << 40 * 80);
}
