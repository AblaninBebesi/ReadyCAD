// Stage 4: finish vs cut sizes, cut list, edge banding consumption.

#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace rc::core;
using namespace rc::detailing;
using Catch::Approx;

namespace {
const Material kChipboard18{"Suntalam 18", MaterialType::Chipboard, 18.0, true};
const Material kChipboard16{"Suntalam 16", MaterialType::Chipboard, 16.0, true};
const EdgeBand kAbs2{2.0, "ABS 2"};
const EdgeBand kPvc04{0.4, "PVC 0.4"};
} // namespace

TEST_CASE("Cut size of an 18 mm shelf with a 2 mm front edge", "[detailing][cut]")
{
    Panel shelf(kChipboard18, Axis::Y, Axis::X);
    shelf.fitTo(Aabb::fromOriginAndSize(Vec3(0), Vec3(762, 18, 519)));
    shelf.setEdge(EdgeSide::L2, kAbs2);
    ConstructionRules rules;

    SECTION("with 1 mm pre-milling: 519 - 2 + 1")
    {
        const CutSize cut = cutSize(shelf, rules);
        CHECK(cut.length == Approx(762.0)); // no edges on the ends
        CHECK(cut.width == Approx(518.0));
    }

    SECTION("edgebander without pre-milling: 519 - 2")
    {
        rules.cutting.premillAllowance = 0.0;
        const CutSize cut = cutSize(shelf, rules);
        CHECK(cut.width == Approx(517.0));
    }
}

TEST_CASE("Cut size of a 16 mm drawer side with a thin top edge", "[detailing][cut]")
{
    Panel side(kChipboard16, Axis::X, Axis::Z); // length along the depth, width = height
    side.fitTo(Aabb::fromOriginAndSize(Vec3(0), Vec3(16, 160, 400)));
    side.setEdge(EdgeSide::L2, kPvc04);

    const CutSize cut = cutSize(side, ConstructionRules{});
    CHECK(cut.length == Approx(400.0));
    CHECK(cut.width == Approx(160.6)); // 160 - 0.4 + 1
}

TEST_CASE("Cut size of a door banded all around", "[detailing][cut]")
{
    Panel door({"MDF 18", MaterialType::MDF, 18.0, false}, Axis::Z, Axis::Y);
    door.fitTo(Aabb::fromOriginAndSize(Vec3(0), Vec3(397, 1997, 18)));
    for (const EdgeSide side : {EdgeSide::L1, EdgeSide::L2, EdgeSide::W1, EdgeSide::W2}) {
        door.setEdge(side, kAbs2);
    }
    const CutSize cut = cutSize(door, ConstructionRules{});
    CHECK(cut.length == Approx(1995.0)); // 1997 - 2*2 + 2*1
    CHECK(cut.width == Approx(395.0));   //  397 - 2*2 + 2*1
}

TEST_CASE("Cut sizes are rounded to the configured step", "[detailing][cut]")
{
    Panel panel(kChipboard18, Axis::Y, Axis::X);
    panel.fitTo(Aabb::fromOriginAndSize(Vec3(0), Vec3(762.33, 18, 519.26)));
    ConstructionRules rules;

    CHECK(cutSize(panel, rules).length == Approx(762.3));
    CHECK(cutSize(panel, rules).width == Approx(519.3));
    rules.cutting.rounding = 0.5;
    CHECK(cutSize(panel, rules).length == Approx(762.5));
}

TEST_CASE("Cut list merges identical parts", "[detailing][cutlist]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    const auto rows = buildCutList(*wardrobe, wardrobe->rules());

    auto row = [&rows](ComponentKind kind) -> const CutListRow& {
        for (const CutListRow& r : rows) {
            if (r.kind == kind) {
                return r;
            }
        }
        FAIL("no row for kind " << toString(kind));
        return rows.front();
    };

    // Side walls and doors are mirror pairs (left / right): a row each.
    CHECK(rows.size() == 8);
    CHECK(row(ComponentKind::SideWall).quantity == 1);
    CHECK(row(ComponentKind::Shelf).quantity == 4);
    CHECK(row(ComponentKind::Door).quantity == 1);
    CHECK(row(ComponentKind::BackPanel).quantity == 1);

    const CutListRow& side = row(ComponentKind::SideWall);
    CHECK(side.hand == Side::Left);
    CHECK(side.finishLength == Approx(1982.0));
    CHECK(side.cut.length == Approx(1982.0));
    CHECK(side.cut.width == Approx(559.0)); // 560 - 2 + 1
    CHECK(side.keepGrainDirection);
    CHECK(side.components.size() == 1);
    CHECK(rows[1].kind == ComponentKind::SideWall);
    CHECK(rows[1].hand == Side::Right);
    CHECK_FALSE(row(ComponentKind::Shelf).hand.has_value());

    const CutListRow& top = row(ComponentKind::TopPanel);
    CHECK(top.cut.length == Approx(798.0)); // overlay top: both ends banded
    CHECK(top.cut.width == Approx(559.0));

    const CutListRow& door = row(ComponentKind::Door);
    CHECK(door.cut.length == Approx(1995.0));
    CHECK(door.cut.width == Approx(395.0));
    CHECK_FALSE(door.keepGrainDirection); // plain MDF

    const CutListRow& back = row(ComponentKind::BackPanel);
    CHECK(back.cut.length == Approx(1977.0));
    CHECK(back.cut.width == Approx(777.0));
    CHECK(back.thickness == Approx(3.0));
}

TEST_CASE("A part left out of the cutting plan gets a row of its own", "[detailing][cutlist]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    const auto shelves = wardrobe->carcass()->interior().shelves();
    shelves[1]->setInCuttingPlan(false); // e.g. a glass shelf ordered cut to size
    const auto rows = buildCutList(*wardrobe, wardrobe->rules());

    REQUIRE(rows.size() == 9);
    int inPlan = 0;
    int outOfPlan = 0;
    for (const CutListRow& r : rows) {
        if (r.kind == ComponentKind::Shelf) {
            (r.inCuttingPlan ? inPlan : outOfPlan) += r.quantity;
        } else {
            CHECK(r.inCuttingPlan);
        }
    }
    CHECK(inPlan == 3);
    CHECK(outOfPlan == 1);
}

TEST_CASE("Edge banding consumption", "[detailing][edges]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    const auto usage = edgeBandUsage(buildCutList(*wardrobe, wardrobe->rules()), wardrobe->rules());

    REQUIRE(usage.size() == 1); // only ABS 2 in this wardrobe
    CHECK(usage.front().band.name == "ABS 2");
    // sides 2 x 2012 + top 830 + 2 x 590 + bottom 794 + shelves 4 x 792 + doors 2 x 4908
    CHECK(usage.front().totalLength == Approx(19812.0));
    CHECK(usage.front().pieces == 18);
}

TEST_CASE("Parts of one size are one row only if their holes are the same", "[detailing][cutlist]")
{
    // Three equal columns, one door each (hinged left): each partition carries the hinge plates
    // of the door to its right and shelf supports on both faces — the two partitions are alike.
    const auto wardrobe = WardrobeBuilder()
                              .dimensions({1800, 2000, 560})
                              .sections(3)
                              .shelvesPerSection(2)
                              .doorsPerSection(1)
                              .build();
    applyDetailing(*wardrobe);
    auto partitionRows = [&wardrobe] {
        std::vector<int> quantities;
        for (const CutListRow& r : buildCutList(*wardrobe, wardrobe->rules())) {
            if (r.kind == ComponentKind::Partition) {
                quantities.push_back(r.quantity);
            }
        }
        return quantities;
    };
    CHECK(partitionRows() == std::vector<int>{2});

    // Shelves on supports carry no holes: all six are one row.
    for (const CutListRow& r : buildCutList(*wardrobe, wardrobe->rules())) {
        if (r.kind == ComponentKind::Shelf) {
            CHECK(r.quantity == 6);
        }
    }

    // Without the third door the second partition has no hinge plates: same board, other part.
    wardrobe->carcass()->interior().subsections()[2]->setDoors(0);
    applyDetailing(*wardrobe);
    CHECK(partitionRows() == std::vector<int>{1, 1});
}
