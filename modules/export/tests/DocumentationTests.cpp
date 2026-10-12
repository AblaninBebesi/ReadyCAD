// Stage 6: the PDF documents as pages of drawings — table data reaches the pages, pagination,
// paper size and orientation, title blocks, scales.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/HardwareList.hpp"
#include "readycad/export/Documentation.hpp"
#include "readycad/export/PartDrawing.hpp"
#include "readycad/export/SheetDrawing.hpp"
#include "readycad/i18n/Labels.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace rc;
using namespace rc::exporting;
using Catch::Approx;

namespace {

/// A product with its derived data, as the editor's Document keeps it.
struct Fixture {
    std::shared_ptr<core::Product> product;
    detailing::DetailingReport report;
    std::vector<detailing::CutListRow> cutList;
    std::vector<detailing::EdgeBandUsage> edges;

    explicit Fixture(std::shared_ptr<core::Product> p) : product(std::move(p))
    {
        report = detailing::applyDetailing(*product);
        cutList = detailing::buildCutList(*product, product->rules());
        edges = detailing::edgeBandUsage(cutList, product->rules());
    }
    [[nodiscard]] DocumentationInput input() const { return {*product, cutList, edges, report}; }
};

std::vector<std::string> textsOf(const Page& page)
{
    std::vector<std::string> texts;
    for (const Entity& e : page.drawing.entities()) {
        if (const auto* t = std::get_if<Text>(&e.shape)) {
            texts.push_back(t->text);
        }
    }
    return texts;
}

bool has(const std::vector<std::string>& texts, const std::string& value)
{
    return std::find(texts.begin(), texts.end(), value) != texts.end();
}

std::string txt(i18n::TextId id)
{
    return std::string(i18n::text(id));
}

DocumentationOptions only(bool summary, bool cutList, bool hardware, bool assembly, bool parts)
{
    DocumentationOptions o;
    o.summary = summary;
    o.cutList = cutList;
    o.hardware = hardware;
    o.assembly = assembly;
    o.partDrawings = parts;
    o.date = "07.10.2026";
    return o;
}

const ApproxTextMetrics kMetrics;

} // namespace

TEST_CASE("Paper size and orientation", "[export][docs]")
{
    CHECK(pageSize({PaperSize::A4, Orientation::Portrait}) == Vec2(210.0, 297.0));
    CHECK(pageSize({PaperSize::A4, Orientation::Landscape}) == Vec2(297.0, 210.0));
    CHECK(pageSize({PaperSize::A3, Orientation::Portrait}) == Vec2(297.0, 420.0));
    CHECK(pageSize({PaperSize::A1, Orientation::Landscape}) == Vec2(841.0, 594.0));
    CHECK(toString(PaperSize::A2) == "A2");

    const Fixture f(core::createProduct(core::CatalogItem::Wardrobe));
    for (const PaperSize size : {PaperSize::A4, PaperSize::A3}) {
        for (const Orientation orientation : {Orientation::Portrait, Orientation::Landscape}) {
            DocumentationOptions options = only(true, true, true, true, true);
            options.format = {size, orientation};
            const PageSet pages = buildDocumentation(f.input(), options, kMetrics);
            REQUIRE_FALSE(pages.pages.empty());
            for (const Page& page : pages.pages) {
                CHECK(page.size == pageSize(options.format));
                // Everything stays on the sheet.
                const Bounds b = page.drawing.bounds(kMetrics);
                CHECK(b.min.x >= 0.0);
                CHECK(b.min.y >= 0.0);
                CHECK(b.max.x <= page.size.x);
                CHECK(b.max.y <= page.size.y);
            }
        }
    }
}

TEST_CASE("Every page has a title block with its number", "[export][docs]")
{
    const Fixture f(core::createProduct(core::CatalogItem::WardrobeWithDrawers));
    DocumentationOptions options = only(true, true, true, true, true);
    options.company = "Örnek Mobilya";
    options.productName = "Yatak odası dolabı";
    const PageSet pages = buildDocumentation(f.input(), options, kMetrics);
    const std::string count = std::to_string(pages.pages.size());
    for (std::size_t i = 0; i < pages.pages.size(); ++i) {
        const auto texts = textsOf(pages.pages[i]);
        CHECK(has(texts, std::to_string(i + 1) + " / " + count));
        CHECK(has(texts, "Yatak odası dolabı"));
        CHECK(has(texts, "Örnek Mobilya"));
        CHECK(has(texts, "07.10.2026"));
    }
    CHECK(pages.title.starts_with("Yatak odası dolabı"));
    CHECK(pages.author == "Örnek Mobilya");
}

TEST_CASE("The cut list reaches the pages row by row", "[export][docs]")
{
    const Fixture f(core::createProduct(core::CatalogItem::WardrobeWithDrawers));
    const PageSet pages = buildDocumentation(f.input(), only(false, true, false, false, false), kMetrics);
    REQUIRE(pages.pages.size() == 1);
    const auto texts = textsOf(pages.pages[0]);
    CHECK(pages.pages[0].bookmark == txt(i18n::TextId::PageCutList));

    int total = 0;
    for (std::size_t i = 0; i < f.cutList.size(); ++i) {
        const detailing::CutListRow& row = f.cutList[i];
        INFO("row " << i + 1 << ": " << partName(row));
        CHECK(has(texts, partName(row)));
        CHECK(has(texts, i18n::mm(row.finishLength) + " × " + i18n::mm(row.finishWidth)));
        CHECK(has(texts, i18n::mm(row.cut.length) + " × " + i18n::mm(row.cut.width)));
        CHECK(has(texts, row.material.name));
        total += row.quantity;
    }
    // Header and the total line.
    CHECK(has(texts, txt(i18n::TextId::ColCut)));
    CHECK(has(texts, txt(i18n::TextId::EdgeL1)));
    CHECK(has(texts, txt(i18n::TextId::TotalParts)));
    CHECK(has(texts, std::to_string(total)));
}

TEST_CASE("Parts left out of the cutting plan are marked in the cut list", "[export][docs]")
{
    auto product = core::createProduct(core::CatalogItem::Wardrobe);
    {
        const Fixture plain(product);
        const PageSet pages =
            buildDocumentation(plain.input(), only(false, true, false, false, false), kMetrics);
        CHECK_FALSE(has(textsOf(pages.pages[0]), txt(i18n::TextId::NotInCuttingPlanNote)));
    }
    product->carcass()->back().setInCuttingPlan(false);
    const Fixture f(product);
    const PageSet pages = buildDocumentation(f.input(), only(false, true, false, false, false), kMetrics);
    REQUIRE(pages.pages.size() == 1);
    const auto texts = textsOf(pages.pages[0]);
    for (const detailing::CutListRow& row : f.cutList) {
        CHECK(has(texts, partName(row) + (row.inCuttingPlan ? "" : " *")));
    }
    CHECK(has(texts, txt(i18n::TextId::NotInCuttingPlanNote)));
}

TEST_CASE("A long table continues on the next pages with its header", "[export][docs]")
{
    Fixture f(core::createProduct(core::CatalogItem::Wardrobe));
    // 90 distinct parts: more than one A4 page can hold.
    std::vector<detailing::CutListRow> rows;
    for (int i = 0; i < 90; ++i) {
        detailing::CutListRow row = f.cutList.front();
        row.name = "Parça " + std::to_string(i + 1);
        rows.push_back(row);
    }
    f.cutList = rows;
    DocumentationOptions options = only(false, true, false, false, false);
    options.format = {PaperSize::A4, Orientation::Portrait};
    const PageSet pages = buildDocumentation(f.input(), options, kMetrics);
    REQUIRE(pages.pages.size() >= 2);

    std::vector<std::string> all;
    for (std::size_t p = 0; p < pages.pages.size(); ++p) {
        const auto texts = textsOf(pages.pages[p]);
        CHECK(has(texts, txt(i18n::TextId::ColPart))); // header repeated on every page
        if (p > 0) {
            CHECK(has(texts, txt(i18n::TextId::PageCutList) + " " + txt(i18n::TextId::Continued)));
            CHECK(pages.pages[p].bookmark.empty());
        }
        all.insert(all.end(), texts.begin(), texts.end());
    }
    for (int i = 0; i < 90; ++i) {
        const std::string name = "Parça " + std::to_string(i + 1);
        CHECK(std::count(all.begin(), all.end(), name) == 1); // every row exactly once
    }
}

TEST_CASE("Hardware page lists edge bands and fasteners", "[export][docs]")
{
    const Fixture f(core::WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build());
    const PageSet pages = buildDocumentation(f.input(), only(false, false, true, false, false), kMetrics);
    REQUIRE(pages.pages.size() == 1);
    const auto texts = textsOf(pages.pages[0]);
    for (const detailing::EdgeBandUsage& usage : f.edges) {
        CHECK(has(texts, usage.band.name));
    }
    CHECK(has(texts, txt(i18n::TextId::HwConfirmat)));
    CHECK(has(texts, "12"));
    CHECK(has(texts, txt(i18n::TextId::HwShelfSupport)));
    CHECK(has(texts, "16"));
    CHECK(has(texts, txt(i18n::TextId::HwHinge)));
}

TEST_CASE("Assembly drawing: three views at a standard scale", "[export][docs]")
{
    const Fixture f(core::createProduct(core::CatalogItem::WardrobeWithDrawers)); // 1600 x 2200
    DocumentationOptions options = only(false, false, false, true, false);
    const PageSet landscape = buildDocumentation(f.input(), options, kMetrics);
    REQUIRE(landscape.pages.size() == 1);
    auto texts = textsOf(landscape.pages[0]);
    CHECK(has(texts, txt(i18n::TextId::ViewFrontClosed)));
    CHECK(has(texts, txt(i18n::TextId::ViewFrontOpen)));
    CHECK(has(texts, txt(i18n::TextId::ViewSection)));
    CHECK(has(texts, "1:20"));
    CHECK(landscape.pages[0].drawing.count<Dimension>() > 10);

    // On a bigger sheet the same views get a bigger scale.
    options.format = {PaperSize::A3, Orientation::Landscape};
    texts = textsOf(buildDocumentation(f.input(), options, kMetrics).pages[0]);
    CHECK((has(texts, "1:10") || has(texts, "1:15")));
}

TEST_CASE("Part drawings: one sheet per part with its holes", "[export][docs]")
{
    const Fixture f(core::WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build());
    const PageSet pages = buildDocumentation(f.input(), only(false, false, false, false, true), kMetrics);
    REQUIRE(pages.pages.size() >= f.cutList.size());

    // Every part has a page with a bookmark "n. name".
    std::size_t bookmarks = 0;
    for (const Page& page : pages.pages) {
        bookmarks += page.bookmark.empty() ? 0 : 1;
    }
    CHECK(bookmarks == f.cutList.size());

    // The side wall's page lists every hole and the groove: rows numbered 1..n.
    const detailing::FlatPart wall = detailing::flatPartOf(*f.product, f.cutList.front());
    const auto texts = textsOf(pages.pages.front());
    CHECK(has(texts, "1. " + partName(f.cutList.front())));
    const std::size_t rows = wall.holes.size() + wall.grooves.size();
    std::vector<std::string> all;
    for (const Page& page : pages.pages) {
        const auto t = textsOf(page);
        all.insert(all.end(), t.begin(), t.end());
    }
    CHECK(has(all, std::to_string(rows)));
    CHECK(has(texts, txt(i18n::TextId::HoleTable)));
    CHECK(has(texts, txt(i18n::TextId::FaceA)));
    // The drawing itself: contour and hole layers of the part.
    CHECK(pages.pages.front().drawing.hasLayer(layers::kContour));
    CHECK(pages.pages.front().drawing.count<Polyline>(layers::kContour) == 1);
}

TEST_CASE("Sections can be switched off", "[export][docs]")
{
    const Fixture f(core::createProduct(core::CatalogItem::Bookcase));
    CHECK(buildDocumentation(f.input(), only(false, false, false, false, false), kMetrics)
              .pages.empty());
    const PageSet summary = buildDocumentation(f.input(), only(true, false, false, false, false), kMetrics);
    REQUIRE(summary.pages.size() == 1);
    const auto texts = textsOf(summary.pages[0]);
    CHECK(has(texts, txt(i18n::TextId::NoWarnings)));
    CHECK(has(texts, "900 × 1800 × 320"));
}

TEST_CASE("The cutting plan: an overview and a page per sheet layout", "[export][docs][cutting]")
{
    const Fixture f(core::createProduct(core::CatalogItem::Wardrobe));
    nesting::NestingSettings settings;
    settings.optimizer.generations = 2;
    settings.optimizer.populationSize = 6;
    const nesting::NestingResult cutting = nesting::optimize(
        nesting::partsFromCutList(*f.product, f.cutList), nesting::StockManager{}, settings);
    DocumentationInput input = f.input();
    input.cutting = &cutting;

    const PageSet pages =
        buildDocumentation(input, only(false, false, false, false, false), kMetrics);
    REQUIRE(pages.pages.size() == 1 + cutting.layouts.size());
    const auto overview = textsOf(pages.pages[0]);
    CHECK(has(overview, txt(i18n::TextId::PageCutting)));
    CHECK(has(overview, txt(i18n::TextId::MachinePanelSaw)));
    CHECK(has(overview, std::to_string(cutting.sheetCount())));
    CHECK(pages.pages[0].bookmark == txt(i18n::TextId::PageCutting));

    for (std::size_t i = 0; i < cutting.layouts.size(); ++i) {
        const Page& page = pages.pages[i + 1];
        const auto texts = textsOf(page);
        CHECK(has(texts, txt(i18n::TextId::PageCutting) + ": " + sheetTitle(cutting, i)));
        CHECK(page.bookmark == txt(i18n::TextId::SheetLabel) + " " + std::to_string(i + 1));
        CHECK(page.drawing.count<Polyline>(layers::kSheet) == 1);
        for (const nesting::PlacedPart& p : cutting.layouts[i].parts) {
            CHECK(has(texts, cutting.parts[p.part].label)); // on the map and in the table
        }
        CHECK_FALSE(has(texts, "1:200")); // a sheet fits A4 at a usual scale
    }

    DocumentationOptions off = only(false, false, false, false, false);
    off.cuttingPlan = false;
    CHECK(buildDocumentation(input, off, kMetrics).pages.empty());
}

TEST_CASE("The largest standard scale that fits is chosen", "[export][docs]")
{
    CHECK(chooseScale([](Real s) { return 1000.0 * s <= 250.0; }) == 5); // 1:2 is too big
    CHECK(chooseScale([](Real s) { return 2200.0 * s <= 150.0; }) == 15);
    CHECK(chooseScale([](Real) { return true; }) == 1);
    CHECK(chooseScale([](Real) { return false; }) == 200);
    CHECK(scaleText(20) == "1:20");
}
