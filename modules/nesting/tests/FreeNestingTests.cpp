// Stage 7: nesting centre — rectangular and shaped parts on sheets of a given size, never
// overlapping, spaced by the cutter, inside the margins.

#include "LayoutCheck.hpp"
#include "readycad/core/builders/Catalog.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/nesting/Nesting.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>

using namespace rc::nesting;
using Catch::Approx;
using rc::core::Material;
using rc::core::MaterialType;

namespace {

const Material kMdf{"MDF Lam 18", MaterialType::MDF, 18.0, false};
const Material kHdf{"HDF 3", MaterialType::HDF, 3.0, false};

NestingPart shaped(const std::string& label, Polygon outline, int quantity,
                   const Material& material = kMdf, bool keepGrain = false)
{
    NestingPart p;
    p.label = label;
    p.material = material;
    p.outline = std::move(outline);
    p.quantity = quantity;
    p.keepGrain = keepGrain;
    return p;
}

NestingPart box(const std::string& label, Real length, Real width, int quantity,
                const Material& material = kMdf)
{
    return shaped(label, rectangle(Rect{{0, 0}, {length, width}}), quantity, material);
}

NestingSettings routerSettings(int generations = 6)
{
    NestingSettings s;
    s.machine = Machine::NestingCenter;
    s.router.toolDiameter = 12.0;
    s.router.margin = 10.0;
    s.optimizer.generations = generations;
    s.optimizer.populationSize = 10;
    s.optimizer.threads = 2;
    return s;
}

/// Every sheet: parts inside the margins and a cutter apart.
void checkAllSheets(const NestingResult& r, const NestingSettings& s)
{
    for (const SheetLayout& sheet : r.layouts) {
        const Rect inside{{s.router.margin, s.router.margin},
                          {sheet.sheet.length - s.router.margin,
                           sheet.sheet.width - s.router.margin}};
        CHECK(sheet.usable == inside);
        const std::string problem = test::checkSheet(r, sheet, inside, s.router.spacing());
        INFO(problem);
        CHECK(problem.empty());
    }
}

int quantity(const std::vector<NestingPart>& parts)
{
    int n = 0;
    for (const NestingPart& p : parts) {
        n += p.quantity;
    }
    return n;
}

const Polygon kL{{0, 0}, {600, 0}, {600, 250}, {250, 250}, {250, 600}, {0, 600}};
const Polygon kTriangle{{0, 0}, {700, 0}, {200, 450}};
const Polygon kU{{0, 0}, {800, 0}, {800, 500}, {600, 500}, {600, 200}, {200, 200}, {200, 500},
                 {0, 500}};
const Polygon kHexagon{{150, 0}, {450, 0}, {600, 260}, {450, 520}, {150, 520}, {0, 260}};

} // namespace

TEST_CASE("Rectangles: all placed, spaced by the cutter, inside the margins", "[nesting][router]")
{
    std::mt19937 random(3);
    std::uniform_int_distribution<int> size(120, 1100);
    std::vector<NestingPart> parts;
    for (int i = 0; i < 16; ++i) {
        parts.push_back(box(std::to_string(i + 1), size(random), size(random), 1 + i % 2));
    }
    const NestingSettings s = routerSettings();
    const NestingResult r = optimize(parts, StockManager{}, s);
    CHECK(r.unplaced.empty());
    CHECK(r.placedCount() == quantity(parts));
    checkAllSheets(r, s);
}

TEST_CASE("Shaped parts are nested by their true outline", "[nesting][router]")
{
    const std::vector<NestingPart> parts{shaped("L", kL, 4), shaped("T", kTriangle, 5),
                                         shaped("U", kU, 3), shaped("H", kHexagon, 3),
                                         box("B", 350, 150, 6)};
    const NestingSettings s = routerSettings();
    const NestingResult r = optimize(parts, StockManager{}, s);
    CHECK(r.unplaced.empty());
    CHECK(r.placedCount() == quantity(parts));
    checkAllSheets(r, s);
    for (const SheetLayout& sheet : r.layouts) {
        CHECK(sheet.utilization() > 0.0);
        CHECK(sheet.utilization() < 1.0);
    }
}

TEST_CASE("A small part goes into the notch of an L: true shapes beat boxes", "[nesting][router]")
{
    // One sheet 620 x 620 with a 10 mm margin leaves 600 x 600: exactly the L's box. A box
    // nester would need a second sheet for the square; the free corner of the L takes it.
    StockManager stock;
    stock.standardLength = 620;
    stock.standardWidth = 620;
    NestingSettings s = routerSettings(0);
    s.router.toolDiameter = 8.0;
    const NestingResult r =
        optimize({shaped("L", kL, 1), box("Q", 330, 330, 1)}, stock, s);
    CHECK(r.unplaced.empty());
    CHECK(r.sheetCount() == 1);
    checkAllSheets(r, s);
}

TEST_CASE("Parts that fit a gap exactly are put there", "[nesting][router]")
{
    // 1000 wide without margin; two 494-wide parts and a 12 mm cut between them: 1000 exactly.
    StockManager stock;
    stock.standardLength = 1000;
    stock.standardWidth = 500;
    NestingSettings s = routerSettings(0);
    s.router.margin = 0.0;
    const NestingResult r = optimize({box("1", 494, 500, 2)}, stock, s);
    CHECK(r.sheetCount() == 1);
    checkAllSheets(r, s);
}

TEST_CASE("Grain allows shaped parts only 0 and 180 degrees", "[nesting][router]")
{
    const NestingSettings s = routerSettings(4);
    const NestingResult r = optimize({shaped("T", kTriangle, 6, kMdf, true)}, StockManager{}, s);
    for (const SheetLayout& sheet : r.layouts) {
        for (const PlacedPart& p : sheet.parts) {
            CHECK(p.transform.quarterTurns % 2 == 0);
        }
    }
    checkAllSheets(r, s);
}

TEST_CASE("Each material gets its own sheets; the free end is a remnant", "[nesting][router]")
{
    const NestingSettings s = routerSettings(2);
    const NestingResult r =
        optimize({box("1", 700, 500, 2), box("2", 1500, 400, 1, kHdf)}, StockManager{}, s);
    REQUIRE(r.layouts.size() == 2);
    CHECK(rc::nesting::sameMaterial(r.layouts[0].sheet.material, kMdf));
    CHECK(rc::nesting::sameMaterial(r.layouts[1].sheet.material, kHdf));
    for (const SheetLayout& sheet : r.layouts) {
        REQUIRE(sheet.remnants.size() == 1);
        const Rect& rest = sheet.remnants.front();
        Real reach = 0.0;
        for (const PlacedPart& p : sheet.parts) {
            reach = std::max(reach, bounds(test::placedOutline(r, p)).max.x);
        }
        CHECK(rest.min.x == Approx(reach + s.router.spacing()).margin(0.01));
        CHECK(rest.max.x == sheet.sheet.length);
        CHECK(rest.height() == sheet.sheet.width); // the whole width of the sheet
    }
    const auto usage = r.usage();
    REQUIRE(usage.size() == 2);
    CHECK(usage[0].purchase == 1);
    CHECK(usage[0].newRemnants == 1);
}

TEST_CASE("A cut list becomes nesting parts in FlatPart coordinates", "[nesting][cutlist]")
{
    const auto product = rc::core::createProduct(rc::core::CatalogItem::Wardrobe);
    rc::detailing::applyDetailing(*product);
    const auto cutList = rc::detailing::buildCutList(*product, product->rules());
    const std::vector<NestingPart> parts = partsFromCutList(*product, cutList, 3);
    REQUIRE(parts.size() == cutList.size());
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const rc::detailing::FlatPart flat = rc::detailing::flatPartOf(*product, cutList[i]);
        CHECK(parts[i].label == std::to_string(i + 1));
        CHECK(parts[i].quantity == 3 * cutList[i].quantity);
        CHECK(parts[i].keepGrain == cutList[i].keepGrainDirection);
        CHECK(parts[i].cutListRow == i);
        CHECK(bounds(parts[i].outline) == Rect{flat.cut.min, flat.cut.max});
        CHECK(bounds(parts[i].outline).width() == Approx(cutList[i].cut.length));
        CHECK(bounds(parts[i].outline).height() == Approx(cutList[i].cut.width));
    }
    CHECK_THROWS_AS(partsFromCutList(*product, cutList, 0), std::invalid_argument);
}

TEST_CASE("Parts left out of the cutting plan are not nested", "[nesting][cutlist]")
{
    const auto product = rc::core::createProduct(rc::core::CatalogItem::Wardrobe);
    product->carcass()->back().setInCuttingPlan(false); // HDF back bought cut to size
    const auto cutList = rc::detailing::buildCutList(*product, product->rules());
    const std::vector<NestingPart> parts = partsFromCutList(*product, cutList);

    REQUIRE(parts.size() == cutList.size() - 1);
    for (const NestingPart& p : parts) {
        REQUIRE(p.cutListRow.has_value());
        const auto& row = cutList[*p.cutListRow];
        CHECK(row.inCuttingPlan);
        CHECK(p.label == std::to_string(*p.cutListRow + 1)); // the number of its cut list row
        CHECK(row.kind != rc::core::ComponentKind::BackPanel);
    }
}
