// Shapes of parts in the detailing: the outline and the blank of a shaped part, and the check
// that no hole or groove falls into material a feature takes away.
//
// Reference: the default wardrobe 800 x 2000 x 560. Its left wall seen from inside: x up the
// wall (W1 the bottom end), y from the rear edge L1 to the banded front edge L2; the back panel
// groove runs at y = 16...20.

#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/detailing/ShapeCheck.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rc::core;
using namespace rc::detailing;
using Catch::Approx;

namespace {

std::vector<ShapeConflict::Reason> conflictsOf(Product& product)
{
    applyDetailing(product);
    std::vector<ShapeConflict::Reason> reasons;
    for (const ShapeConflict& c : findShapeConflicts(product)) {
        reasons.push_back(c.reason);
    }
    return reasons;
}

} // namespace

TEST_CASE("A notched wall unfolds with its real outline and its blank", "[detailing][shape]")
{
    const auto wardrobe = WardrobeBuilder().build();
    SideWall& wall = wardrobe->carcass()->leftSide();
    wall.setShape({CornerNotch{PanelCorner::L1W1, 100.0, 15.0}});
    applyDetailing(*wardrobe);
    const FlatPart flat = flatten(*wall.panel(), wardrobe->rules());

    REQUIRE(flat.shaped());
    REQUIRE_FALSE(flat.flipped); // seen from inside the left wall is seen as the panel is
    REQUIRE(flat.contour.size() == 6);
    CHECK(flat.contour[0].start == Vec2(0.0, 15.0));
    CHECK(flat.contour[0].kind == OutlineSideKind::Notch);
    CHECK_FALSE(flat.contour[0].band.present()); // the edgebander cannot follow the step
    CHECK(flat.contour[4].start == Vec2(1982.0, 560.0));
    CHECK(flat.contour[4].band.name == "ABS 2"); // the front edge
    // The blank: the front edge 1 mm inside (2 mm band, 1 mm pre-milling), the notch as drawn.
    CHECK(flat.blank == std::vector<Vec2>{{0, 15}, {100, 15}, {100, 0}, {1982, 0}, {1982, 559},
                                          {0, 559}});
    CHECK(flat.cut.max.y == Approx(559.0));

    // A plain part stays a rectangle.
    CHECK_FALSE(flatten(*wardrobe->carcass()->rightSide().panel(), wardrobe->rules()).shaped());
}

TEST_CASE("Holes and grooves must stay in material", "[detailing][shape]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    Carcass& c = *wardrobe->carcass();
    CHECK(conflictsOf(*wardrobe).empty());

    SECTION("a 15 mm skirting notch passes beside the back panel groove")
    {
        c.leftSide().setShape({CornerNotch{PanelCorner::L1W1, 100.0, 15.0}});
        CHECK(conflictsOf(*wardrobe).empty());
    }
    SECTION("a 20 mm one would cut through the groove")
    {
        c.leftSide().setShape({CornerNotch{PanelCorner::L1W1, 100.0, 20.0}});
        CHECK(conflictsOf(*wardrobe)
              == std::vector<ShapeConflict::Reason>{ShapeConflict::Reason::MachiningInCut});
    }
    SECTION("a cable hole over the front row of shelf supports")
    {
        const Real support = c.interior().shelves()[0]->box().min().y - 2.5; // the hole under it
        c.leftSide().setShape({RoundHole{PanelCorner::L1W1, support, 560.0 - 37.0 - 30.0, 60.0}});
        CHECK(conflictsOf(*wardrobe)
              == std::vector<ShapeConflict::Reason>{ShapeConflict::Reason::MachiningInCut});
    }
    SECTION("a corner notch where the top's connectors enter the end of the wall")
    {
        // The overlay top is screwed into the wall's upper end (edge W2), 50 from the front.
        c.leftSide().setShape({CornerNotch{PanelCorner::L2W2, 30.0, 80.0}});
        CHECK(conflictsOf(*wardrobe)
              == std::vector<ShapeConflict::Reason>{ShapeConflict::Reason::MachiningInCut});
    }
    SECTION("two features in one corner")
    {
        c.leftSide().setShape({CornerRound{PanelCorner::L2W1, 30.0}, CornerChamfer{PanelCorner::L2W1}});
        CHECK(conflictsOf(*wardrobe)
              == std::vector<ShapeConflict::Reason>{ShapeConflict::Reason::DoesNotFit});
    }
}

TEST_CASE("A shelf with rounded corners is a part of its own", "[detailing][shape][cutlist]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).build();
    wardrobe->carcass()->interior().shelves()[3]->setShape(
        {CornerRound{PanelCorner::L2W1, 80.0}, CornerRound{PanelCorner::L2W2, 80.0}});
    applyDetailing(*wardrobe);
    std::vector<int> shelves;
    for (const CutListRow& row : buildCutList(*wardrobe, wardrobe->rules())) {
        if (row.kind == ComponentKind::Shelf) {
            shelves.push_back(row.quantity);
        }
    }
    CHECK(shelves == std::vector<int>{3, 1});
}
