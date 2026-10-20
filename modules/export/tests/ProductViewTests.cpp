// Stage 6: views of the product for the assembly drawing (hidden lines, section, dimensions).

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/Kitchen.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/core/model/Section.hpp"
#include "readycad/export/ProductViews.hpp"
#include "readycad/i18n/Labels.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>

using namespace rc;
using namespace rc::exporting;
using Catch::Approx;

namespace {

std::shared_ptr<core::Product> referenceWardrobe()
{
    return core::WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
}

std::vector<Line> linesOn(const Drawing& d, std::string_view layer)
{
    std::vector<Line> lines;
    for (const Entity& e : d.entities()) {
        if (d.layers()[e.layer].name == layer) {
            if (const auto* line = std::get_if<Line>(&e.shape)) {
                lines.push_back(*line);
            }
        }
    }
    return lines;
}

std::multiset<std::string> dimensionValues(const Drawing& d)
{
    std::multiset<std::string> values;
    for (const Entity& e : d.entities()) {
        if (const auto* dim = std::get_if<Dimension>(&e.shape)) {
            values.insert(dim->text.empty() ? dimensionText(*dim) : dim->text);
        }
    }
    return values;
}

bool strictlyInside(Vec2 p, const core::Aabb& box)
{
    constexpr Real e = 1e-6;
    return p.x > box.min().x + e && p.x < box.max().x - e && p.y > box.min().y + e
        && p.y < box.max().y - e;
}

} // namespace

TEST_CASE("Front view: nothing behind the doors is drawn", "[export][views]")
{
    const auto wardrobe = referenceWardrobe();
    ProductViewOptions options;
    options.dimensions = false;
    const Drawing front = drawProductView(*wardrobe, ProductView::Front, options);

    const auto doors = wardrobe->carcass()->interior().doors();
    REQUIRE(doors.size() == 2);
    const auto lines = linesOn(front, viewLayers::kVisible);
    REQUIRE_FALSE(lines.empty());
    for (const Line& line : lines) {
        const Vec2 middle = (line.a + line.b) * 0.5;
        for (const core::Door* door : doors) {
            INFO("line (" << line.a.x << ", " << line.a.y << ") - (" << line.b.x << ", " << line.b.y
                          << ")");
            CHECK_FALSE(strictlyInside(middle, door->box()));
        }
    }
    // Two handles, one per door.
    CHECK(front.count<Polyline>(viewLayers::kHandles) == 2);
}

TEST_CASE("Open front view: shelves and the trace of section A-A", "[export][views]")
{
    const auto wardrobe = referenceWardrobe();
    const Drawing open = drawProductView(*wardrobe, ProductView::FrontOpen, {25.0, false});

    // The top face of every shelf is a visible horizontal line.
    const auto lines = linesOn(open, viewLayers::kVisible);
    for (const core::Shelf* shelf : wardrobe->carcass()->interior().shelves()) {
        const core::Aabb& box = shelf->box();
        const bool found = std::any_of(lines.begin(), lines.end(), [&](const Line& l) {
            return std::abs(l.a.y - box.max().y) < 1e-6 && std::abs(l.b.y - box.max().y) < 1e-6
                && std::min(l.a.x, l.b.x) <= box.min().x + 1e-6
                && std::max(l.a.x, l.b.x) >= box.max().x - 1e-6;
        });
        CHECK(found);
    }
    // No handles without fronts; one cut line at the plane of A-A.
    CHECK(open.count<Polyline>(viewLayers::kHandles) == 0);
    const auto cut = linesOn(open, viewLayers::kCutLine);
    REQUIRE(cut.size() == 1);
    CHECK(cut[0].a.x == Approx(sectionPlaneX(*wardrobe)));
    // Inside the first compartment, not in the gap between the doors.
    CHECK(sectionPlaneX(*wardrobe) == Approx(18.0 + 0.4 * 764.0));
}

TEST_CASE("Section A-A: cut parts are shaded, seen from the left", "[export][views]")
{
    const auto wardrobe = referenceWardrobe();
    const Drawing section = drawProductView(*wardrobe, ProductView::Section, {25.0, false});

    // Cut by the plane: top, bottom, back, 4 shelves, the left door.
    CHECK(section.count<Polyline>(viewLayers::kSectionFill) == 3 + 4 + 1);
    // Depth runs to the right: back at 0, fronts at the far right.
    const Bounds extent = viewExtent(*wardrobe, ProductView::Section);
    CHECK(extent.min.x == Approx(0.0));
    CHECK(extent.max.x == Approx(560.0 + 18.0));
    CHECK(extent.size().y == Approx(2000.0));
}

TEST_CASE("Views carry overall and assembly dimensions", "[export][views]")
{
    const auto wardrobe = referenceWardrobe();
    const ProductViewOptions options{10.0, true};

    const auto front = dimensionValues(drawProductView(*wardrobe, ProductView::Front, options));
    CHECK(front.contains("800"));
    CHECK(front.contains("2000"));

    // Open view: clear width and the clear heights between the shelves.
    const auto open = dimensionValues(drawProductView(*wardrobe, ProductView::FrontOpen, options));
    CHECK(open.contains("764"));
    const auto shelves = wardrobe->carcass()->interior().shelves();
    Real lowestShelf = 1e9;
    for (const core::Shelf* s : shelves) {
        lowestShelf = std::min(lowestShelf, s->box().min().y);
    }
    const Real bottomTop = wardrobe->carcass()->bottom().box().max().y;
    CHECK(open.contains(i18n::mm(lowestShelf - bottomTop)));
    CHECK(open.count("2000") == 1);

    const auto section = dimensionValues(drawProductView(*wardrobe, ProductView::Section, options));
    CHECK(section.contains("578")); // with the doors
    CHECK(section.contains("560")); // carcass
}

TEST_CASE("Every catalogue item has all three views", "[export][views]")
{
    for (const core::CatalogItem item : core::catalogItems()) {
        const auto product = core::createProduct(item);
        for (const ProductView view :
             {ProductView::Front, ProductView::FrontOpen, ProductView::Section}) {
            const Drawing d = drawProductView(*product, view);
            CHECK(d.count<Line>() > 4);
            CHECK_FALSE(viewExtent(*product, view).empty);
        }
    }
}

TEST_CASE("A corner kitchen: a plan of the room, an elevation per wall", "[export][views][corner]")
{
    const auto product = core::createProduct(core::CatalogItem::KitchenL);
    const core::Kitchen& kitchen = *product->kitchen();

    // From above: the back wall at the top, the right wall down the right side; the worktops'
    // cut-outs (the sink at the back, the hob at the side) seen through.
    const Drawing plan = drawProductView(*product, ProductView::Plan, {25.0, true});
    const Bounds extent = viewExtent(*product, ProductView::Plan);
    CHECK(extent.min.x == Approx(0.0));
    CHECK(extent.max.x == Approx(3000.0));
    CHECK(extent.max.y == Approx(0.0));
    CHECK(extent.min.y == Approx(-2400.0));
    CHECK(plan.count<Polyline>() >= 2);
    const auto planDims = dimensionValues(plan);
    CHECK(planDims.count("3000") == 1);
    CHECK(planDims.count("2400") == 1);
    // The front edge of the back worktop, from wall to wall: the side worktop butts against it.
    const std::vector<Line> visible = linesOn(plan, viewLayers::kVisible);
    CHECK(std::any_of(visible.begin(), visible.end(), [](const Line& l) {
        return std::abs(l.a.y + 600.0) < 1e-6 && std::abs(l.b.y + 600.0) < 1e-6
            && std::min(l.a.x, l.b.x) < 1e-6 && std::max(l.a.x, l.b.x) > 3000.0 - 1e-6;
    }));

    // Each wall as seen standing in front of it, in its own coordinates.
    const core::KitchenRun& side = *kitchen.sideRun(core::Side::Right);
    const Bounds back = viewExtent(kitchen.backRun(), ProductView::Front);
    CHECK(back.min.x == Approx(0.0));
    CHECK(back.max.x == Approx(3000.0));
    const Bounds right = viewExtent(side, ProductView::Front);
    CHECK(right.min.x == Approx(338.0)); // its wall cabinets start in front of the corner's
    CHECK(right.max.x == Approx(2400.0));
    for (const ProductView view : {ProductView::Front, ProductView::FrontOpen, ProductView::Section}) {
        CHECK(drawProductView(side, view).count<Line>() > 4);
    }
    // The open view leaves the blind panel out like the doors: the corner cabinet's shelf shows.
    CHECK(viewExtent(kitchen.backRun(), ProductView::FrontOpen).max.x == Approx(3000.0));
}
