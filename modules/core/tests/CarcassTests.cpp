// Stage 2/3: carcass parts and their own formulas for every joint / back-mount combination.
//
// Reference carcass: 800 x 2000 x 560, 18 mm chipboard, 3 mm HDF back, default rules
// (groove 8 deep, 16 from the rear, 1.5 expansion gap per side).

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/model/Product.hpp"
#include "readycad/core/model/Queries.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace rc::core;
using rc::test::IsBox;

namespace {

std::shared_ptr<Product> makeCarcass(BackMountType mount = BackMountType::Groove,
                                     JointType top = JointType::Overlay,
                                     JointType bottom = JointType::Inset,
                                     Size3D size = {800, 2000, 560})
{
    auto product = std::make_shared<Product>();
    auto carcass = std::make_shared<Carcass>();
    carcass->setTopJoint(top);
    carcass->setBottomJoint(bottom);
    carcass->setBackMount(mount);
    product->add(carcass);
    product->setDimensions(size);
    return product;
}

} // namespace

TEST_CASE("Default carcass: overlay top, inset bottom, back in a groove", "[core][carcass]")
{
    const auto product = makeCarcass();
    const Carcass& c = *product->carcass();

    // Sides stand on the floor and end under the overlay top.
    CHECK_THAT(c.leftSide().box(), IsBox({0, 0, 0}, {18, 1982, 560}));
    CHECK_THAT(c.rightSide().box(), IsBox({782, 0, 0}, {800, 1982, 560}));
    // Top lies on the sides over the full width; bottom sits between the sides.
    CHECK_THAT(c.top().box(), IsBox({0, 1982, 0}, {800, 2000, 560}));
    CHECK_THAT(c.bottom().box(), IsBox({18, 0, 0}, {782, 18, 560}));
    // Back: opening 764 x 1964 plus (8 - 1.5) = 6.5 mm into every groove; 16 mm from the rear.
    CHECK_THAT(c.back().box(), IsBox({11.5, 11.5, 16}, {788.5, 1988.5, 19}));
    // Interior: between the walls, in front of the back.
    CHECK_THAT(c.interiorBox(), IsBox({18, 18, 19}, {782, 1982, 560}));

    CHECK(findOverlaps(*product).empty()); // the back in its groove is a legal intersection
}

TEST_CASE("Panels of the carcass carry finish sizes and default edges", "[core][carcass][panel]")
{
    const auto product = makeCarcass();
    const Carcass& c = *product->carcass();

    const Panel& side = *c.leftSide().panel();
    CHECK(side.finishLength() == 1982.0); // along Y
    CHECK(side.finishWidth() == 560.0);   // along Z
    CHECK(side.edge(EdgeSide::L2).thickness == 2.0); // front edge
    CHECK(side.cutWidth() == 558.0);

    const Panel& back = *c.back().panel();
    CHECK(back.finishLength() == 1977.0);
    CHECK(back.finishWidth() == 777.0);
    CHECK(back.thickness() == 3.0);

    // An overlay top shows its ends on the outside, so they are banded.
    const Panel& top = *c.top().panel();
    CHECK(top.edge(EdgeSide::W1).present());
    CHECK(top.edge(EdgeSide::W2).present());
    CHECK(top.cutLength() == 796.0);
    // The inset bottom's ends are hidden against the sides.
    CHECK_FALSE(c.bottom().panel()->edge(EdgeSide::W1).present());
}

TEST_CASE("Back mount strategies", "[core][carcass][back]")
{
    SECTION("overlay: HDF nailed onto the rear edges, carcass panels start in front of it")
    {
        const auto product = makeCarcass(BackMountType::Overlay);
        const Carcass& c = *product->carcass();
        CHECK_THAT(c.back().box(), IsBox({2, 2, 0}, {798, 1998, 3}));
        CHECK_THAT(c.leftSide().box(), IsBox({0, 0, 3}, {18, 1982, 560}));
        CHECK_THAT(c.top().box(), IsBox({0, 1982, 3}, {800, 2000, 560}));
        CHECK(c.interiorBox().min().z == 3.0);
        CHECK(findOverlaps(*product).empty());
    }

    SECTION("inset: back fills the rear opening between the walls")
    {
        const auto product = makeCarcass(BackMountType::Inset);
        const Carcass& c = *product->carcass();
        CHECK_THAT(c.back().box(), IsBox({18, 18, 0}, {782, 1982, 3}));
        CHECK(c.leftSide().box().min().z == 0.0);
        CHECK(c.interiorBox().min().z == 3.0);
        CHECK(findOverlaps(*product).empty());
    }

    SECTION("only the groove mount may enter the walls")
    {
        const auto groove = makeCarcass(BackMountType::Groove);
        const Carcass& c = *groove->carcass();
        CHECK(c.back().box().intersects(c.leftSide().box()));
        CHECK(c.back().allowsOverlapWith(c.leftSide()));

        const auto overlay = makeCarcass(BackMountType::Overlay);
        CHECK_FALSE(overlay->carcass()->back().allowsOverlapWith(overlay->carcass()->leftSide()));
    }
}

TEST_CASE("Top and bottom joints change side height and panel widths", "[core][carcass][joints]")
{
    SECTION("inset top: sides run full height, top sits between them")
    {
        const auto product = makeCarcass(BackMountType::Groove, JointType::Inset, JointType::Inset);
        const Carcass& c = *product->carcass();
        CHECK_THAT(c.leftSide().box(), IsBox({0, 0, 0}, {18, 2000, 560}));
        CHECK_THAT(c.top().box(), IsBox({18, 1982, 0}, {782, 2000, 560}));
        CHECK_FALSE(c.top().panel()->edge(EdgeSide::W1).present());
    }

    SECTION("overlay bottom: sides stand on the bottom")
    {
        const auto product =
            makeCarcass(BackMountType::Groove, JointType::Overlay, JointType::Overlay);
        const Carcass& c = *product->carcass();
        CHECK_THAT(c.leftSide().box(), IsBox({0, 18, 0}, {18, 1982, 560}));
        CHECK_THAT(c.bottom().box(), IsBox({0, 0, 0}, {800, 18, 560}));
    }
}

TEST_CASE("No combination of joints and back mounts produces overlapping parts",
          "[core][carcass][overlap]")
{
    const auto mount =
        GENERATE(BackMountType::Groove, BackMountType::Overlay, BackMountType::Inset);
    const auto top = GENERATE(JointType::Overlay, JointType::Inset);
    const auto bottom = GENERATE(JointType::Overlay, JointType::Inset);

    const auto product = makeCarcass(mount, top, bottom);
    const auto overlaps = findOverlaps(*product);
    CAPTURE(static_cast<int>(mount), static_cast<int>(top), static_cast<int>(bottom));
    CHECK(overlaps.empty());

    // Every carcass part stays inside the product's box.
    const Aabb outer = product->box();
    for (const FurnitureComponent* part : collectPanels(*product)) {
        CHECK(outer.contains(part->box()));
    }
}

TEST_CASE("A thicker top, bottom or side: its neighbours use its real thickness",
          "[core][carcass][material]")
{
    // Stage 9: found by the random-edit test — the neighbours used the carcass material's
    // thickness and overlapped a 25 mm top by 7 mm.
    const Material thick{"Suntalam 25", MaterialType::Chipboard, 25.0, true};
    const auto mount =
        GENERATE(BackMountType::Groove, BackMountType::Overlay, BackMountType::Inset);
    const auto top = GENERATE(JointType::Overlay, JointType::Inset);
    const auto bottom = GENERATE(JointType::Overlay, JointType::Inset);
    CAPTURE(static_cast<int>(mount), static_cast<int>(top), static_cast<int>(bottom));

    const auto product = makeCarcass(mount, top, bottom);
    Carcass& c = *product->carcass();
    c.top().setMaterialOverride(thick);
    c.bottom().setMaterialOverride(thick);
    c.leftSide().setMaterialOverride(thick);
    CHECK(findOverlaps(*product).empty());

    // The parts close the carcass without gaps: they still touch where they meet.
    if (top == JointType::Overlay) {
        CHECK(c.leftSide().box().max().y == c.top().box().min().y);
    } else {
        CHECK(c.top().box().min().x == c.leftSide().box().max().x);
        CHECK(c.top().box().max().x == c.rightSide().box().min().x);
    }
    if (bottom == JointType::Overlay) {
        CHECK(c.leftSide().box().min().y == c.bottom().box().max().y);
    } else {
        CHECK(c.bottom().box().size().x == 800.0 - 25.0 - 18.0);
    }
    // The opening is what the parts leave free.
    CHECK(c.interiorBox().min().x == 25.0);
    CHECK(c.interiorBox().max().y == 2000.0 - 25.0);
    CHECK(c.interiorBox().min().y == 25.0);
}

TEST_CASE("Carcass parts refuse to be laid out outside a carcass", "[core][carcass]")
{
    SideWall side(Side::Left);
    LayoutSlot slot;
    slot.box = Aabb::fromOriginAndSize(Vec3(0.0), Vec3(800, 2000, 560));
    CHECK_THROWS_AS(side.layout(slot), std::logic_error);
}
