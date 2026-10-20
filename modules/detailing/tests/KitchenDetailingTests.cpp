// Kitchen stage: drilling, grooves and hardware of a kitchen run.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/KitchenBuilder.hpp"
#include "readycad/core/model/Product.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/HardwareList.hpp"
#include "readycad/detailing/Joints.hpp"
#include "readycad/detailing/ShapeCheck.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rc::core;
using namespace rc::detailing;
using Catch::Approx;

namespace {

int quantityOf(const std::vector<HardwareItem>& list, HardwareKind kind)
{
    int total = 0;
    for (const HardwareItem& item : list) {
        total += item.kind == kind ? item.quantity : 0;
    }
    return total;
}

int count(const Panel& panel, FastenerType type)
{
    int n = 0;
    for (const Drilling& d : panel.drillings()) {
        n += d.fastener == type ? 1 : 0;
    }
    return n;
}

} // namespace

TEST_CASE("Rails are screwed to the side walls; the rear flat rail holds the back",
          "[detailing][kitchen]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    const DetailingReport report = applyDetailing(*product);
    CHECK(report.warnings.empty());

    const CabinetRow& row = product->kitchen()->baseRow();
    const Carcass& doors = *row.cabinets()[0]; // flat rails
    const Carcass& sink = *row.cabinets()[1];  // upright rails

    for (const Carcass* c : {&doors, &sink}) {
        for (const Rail* rail : {c->frontRail(), c->rearRail()}) {
            // One connector at each end: a 100 mm joint is shorter than two end offsets.
            CHECK(count(*rail->panel(), FastenerType::Confirmat) == 2);
        }
    }
    // The back goes into a groove of the flat rear rail, not of the front one.
    REQUIRE(doors.rearRail()->panel()->grooves().size() == 1);
    CHECK(doors.rearRail()->panel()->grooves().front().face == PanelFace::Bottom);
    CHECK(doors.frontRail()->panel()->grooves().empty());
    // Upright rails stand in front of the back: no groove in them, the back is nailed to them.
    CHECK(sink.frontRail()->panel()->grooves().empty());
    CHECK(sink.rearRail()->panel()->grooves().empty());
    CHECK(sink.leftSide().panel()->grooves().size() == 1);
}

TEST_CASE("Cabinets of a kitchen are drilled each on its own", "[detailing][kitchen]")
{
    // Two cabinets side by side with top panels: the left one's lies on its walls, the right
    // one's between them. So the end of the left top touches the face of the right cabinet's
    // wall — it looks like a joint, but they are separate boxes: no connector may join them.
    auto product = KitchenBuilder()
                       .length(1200)
                       .add(CabinetModule::BaseDoors, 600)
                       .add(CabinetModule::BaseDoors)
                       .build();
    const auto cabinets = product->kitchen()->baseRow().cabinets();
    for (Carcass* c : cabinets) {
        c->setTopStyle(CarcassTop::Panel);
    }
    cabinets[0]->setTopJoint(JointType::Overlay);
    cabinets[1]->setTopJoint(JointType::Inset);

    const DetailingReport report = applyDetailing(*product);
    int perCabinet = 0;
    for (Carcass* c : cabinets) {
        perCabinet += static_cast<int>(findJoints(*c).size());
    }
    CHECK(static_cast<int>(findJoints(*product).size()) == perCabinet + 1); // the false one
    CHECK(report.joints == perCabinet);
    CHECK(report.warnings.empty());
}

TEST_CASE("Kitchen hardware: legs, plinth clips, hangers", "[detailing][kitchen][hardware]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    applyDetailing(*product);
    const auto list = hardwareList(*product);
    CHECK(quantityOf(list, HardwareKind::Leg) == 5 * 4);       // five base cabinets
    CHECK(quantityOf(list, HardwareKind::PlinthClip) == 5 * 2); // on the front legs
    CHECK(quantityOf(list, HardwareKind::Hanger) == 5 * 2);     // five wall cabinets
    CHECK(quantityOf(list, HardwareKind::DrawerSlide) == 3);

    // A cabinet wider than a metre gets a middle pair of legs.
    product->setDimensions({3400, 2120, 560}); // the last cabinet: 1400
    applyDetailing(*product);
    CHECK(quantityOf(hardwareList(*product), HardwareKind::Leg) == 4 * 4 + 6);
}

TEST_CASE("The worktop is one cut list row with its cut-outs, out of the cutting plan",
          "[detailing][kitchen][cutlist]")
{
    const auto product = createProduct(CatalogItem::Kitchen);
    applyDetailing(*product);
    int worktops = 0;
    for (const CutListRow& row : buildCutList(*product, product->rules())) {
        if (row.kind == ComponentKind::Worktop) {
            ++worktops;
            CHECK(row.quantity == 1);
            CHECK_FALSE(row.inCuttingPlan);
            CHECK(row.finishLength == Approx(2400.0));
            CHECK(row.finishWidth == Approx(600.0));
            CHECK(row.thickness == Approx(38.0));
        }
    }
    CHECK(worktops == 1);
}

TEST_CASE("An L corner cabinet is drilled: joints, supports at both ends, hinges and folding hinges",
          "[detailing][kitchen][lcorner]")
{
    const auto product = KitchenBuilder()
                             .length(3000)
                             .sideWall(Side::Right, 2400, CornerType::LShaped)
                             .add(CabinetModule::BaseDoors)
                             .add(CabinetModule::WallDoors)
                             .add(CabinetModule::BaseDoors, std::nullopt, KitchenWall::Right)
                             .add(CabinetModule::WallDoors, std::nullopt, KitchenWall::Right)
                             .build();
    const DetailingReport report = applyDetailing(*product);
    CHECK(report.warnings.empty());
    CHECK(findShapeConflicts(*product).empty()); // nothing drilled into the notch

    const CornerCabinet& l = *product->kitchen()->backRun().corner(Side::Right)->lCabinet(RowLevel::Base);
    // Both walls are screwed to the bottom and the top (the walls stand on the bottom, the top
    // between them).
    CHECK(count(*l.side().panel(), FastenerType::Confirmat) >= 4);
    CHECK(count(*l.endWall().panel(), FastenerType::Confirmat) >= 4);
    CHECK(count(*l.bottom().panel(), FastenerType::Confirmat) >= 4);
    CHECK(count(*l.top().panel(), FastenerType::Confirmat) >= 4);
    // The shelf rests on two pins in each end wall.
    CHECK(count(*l.side().panel(), FastenerType::ShelfSupport) == 2);
    CHECK(count(*l.endWall().panel(), FastenerType::ShelfSupport) == 2);
    // The first door on the back leg's wall: cups and plates; the second on the first: cups.
    CHECK(count(*l.door().panel(), FastenerType::Hinge) == 2);
    CHECK(count(*l.side().panel(), FastenerType::Hinge) == 4);
    CHECK(count(*l.foldingDoor().panel(), FastenerType::Hinge) == 2);
    CHECK(count(*l.foldingDoor().panel(), FastenerType::Handle) == 2);
    CHECK(count(*l.door().panel(), FastenerType::Handle) == 0);
    // The backs are nailed on: no grooves.
    CHECK(l.back().panel()->grooves().empty());

    const auto list = hardwareList(*product);
    CHECK(quantityOf(list, HardwareKind::FoldingHinge) == 2 + 2); // base and wall corner
    // The back run's cabinet 2100 wide (8 legs), the side run's 1500 (6), the L cabinet's 6.
    CHECK(quantityOf(list, HardwareKind::Leg) == 8 + 6 + 6);
    CHECK(quantityOf(list, HardwareKind::Hanger) == 2 + 2 + 2);
}

TEST_CASE("A blind wall corner cabinet hangs on hangers too", "[detailing][kitchen][hardware]")
{
    const auto product = createProduct(CatalogItem::KitchenL);
    applyDetailing(*product);
    // Wall cabinets: three on the back wall, two on the side wall, the blind corner one.
    CHECK(quantityOf(hardwareList(*product), HardwareKind::Hanger) == (3 + 2 + 1) * 2);
}
