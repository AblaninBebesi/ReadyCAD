// Stage 3: parametric recalculation. Change the overall size (or one compartment) and check that
// every dependent part followed — and that nothing ended up overlapping.

#include "readycad/test/TestHelpers.hpp"

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Queries.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <stdexcept>
#include <vector>

using namespace rc::core;
using Catch::Approx;
using rc::test::IsBox;
using rc::test::rulesWithoutSnapping;

namespace {

std::vector<Real> clearGapsBetweenShelves(const Section& section)
{
    std::vector<Real> gaps;
    Real previousTop = section.box().min().y;
    for (const Shelf* shelf : section.shelves()) {
        gaps.push_back(shelf->box().min().y - previousTop);
        previousTop = shelf->box().max().y;
    }
    gaps.push_back(section.box().max().y - previousTop);
    return gaps;
}

void requireNoOverlaps(const FurnitureComponent& root)
{
    const auto overlaps = findOverlaps(root);
    for (const Overlap& o : overlaps) {
        UNSCOPED_INFO(toString(o.first->kind()) << " overlaps " << toString(o.second->kind()));
    }
    CHECK(overlaps.empty());
}

} // namespace

TEST_CASE("Wardrobe width 800 -> 900: everything follows", "[core][parametric]")
{
    const auto wardrobe = WardrobeBuilder()
                              .dimensions({800, 2000, 560})
                              .shelvesPerSection(4)
                              .doorsPerSection(2)
                              .build();
    Carcass& c = *wardrobe->carcass();
    const auto shelvesBefore = c.interior().shelves();
    std::vector<Real> shelfHeights;
    for (const Shelf* shelf : shelvesBefore) {
        shelfHeights.push_back(shelf->box().min().y);
    }

    wardrobe->setDimensions({900, 2000, 560});

    // Walls: the left one stays, the right one moves by +100.
    CHECK_THAT(c.leftSide().box(), IsBox({0, 0, 0}, {18, 1982, 560}));
    CHECK_THAT(c.rightSide().box(), IsBox({882, 0, 0}, {900, 1982, 560}));
    CHECK(c.top().size().width == Approx(900.0));
    CHECK(c.bottom().size().width == Approx(864.0));
    CHECK(c.back().size().width == Approx(877.0));

    // Shelves: +100 wider, same heights (the height did not change).
    const auto shelves = c.interior().shelves();
    REQUIRE(shelves.size() == shelfHeights.size());
    for (std::size_t i = 0; i < shelves.size(); ++i) {
        CHECK(shelves[i]->size().width == Approx(862.0));
        CHECK(shelves[i]->panel()->finishLength() == Approx(862.0));
        CHECK(shelves[i]->box().min().y == Approx(shelfHeights[i]));
        CHECK(shelves[i]->box().min().x == Approx(19.0));
    }

    // Doors: (897 - 3) / 2 = 447 each.
    for (const Door* door : c.interior().doors()) {
        CHECK(door->size().width == Approx(447.0));
    }

    requireNoOverlaps(*wardrobe);
}

TEST_CASE("Height change redistributes automatic shelves evenly", "[core][parametric]")
{
    const auto wardrobe =
        WardrobeBuilder().rules(rulesWithoutSnapping()).shelvesPerSection(4).build();
    wardrobe->setDimensions({800, 2400, 560});

    const Section& interior = wardrobe->carcass()->interior();
    CHECK(wardrobe->carcass()->leftSide().size().height == Approx(2382.0));

    const auto gaps = clearGapsBetweenShelves(interior);
    REQUIRE(gaps.size() == 5);
    for (const Real gap : gaps) {
        CHECK(gap == Approx((2364.0 - 4 * 18.0) / 5.0));
    }
    requireNoOverlaps(*wardrobe);
}

TEST_CASE("Depth change: shelves get deeper, fronts move with the carcass front",
          "[core][parametric]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(2).doorsPerSection(1).build();
    wardrobe->setDimensions({800, 2000, 600});

    const Carcass& c = *wardrobe->carcass();
    CHECK(c.leftSide().size().depth == Approx(600.0));
    CHECK(c.back().box().min().z == Approx(16.0)); // groove position is from the rear: unchanged
    for (const Shelf* shelf : c.interior().shelves()) {
        CHECK(shelf->size().depth == Approx(600.0 - 19.0 - 20.0 - 2.0));
    }
    CHECK(c.interior().doors().front()->box().min().z == Approx(600.0));
    requireNoOverlaps(*wardrobe);
}

TEST_CASE("A fixed-width compartment keeps its width when the product grows",
          "[core][parametric][section]")
{
    const auto wardrobe = WardrobeBuilder()
                              .dimensions({1200, 2000, 560})
                              .sections(2)
                              .sectionWidth(0, 400.0)
                              .shelvesPerSection(3)
                              .doorsPerSection(1)
                              .build();
    Section& interior = wardrobe->carcass()->interior();

    wardrobe->setDimensions({1600, 2000, 560});
    auto columns = interior.subsections();
    CHECK(columns[0]->size().width == Approx(400.0));
    CHECK(columns[1]->size().width == Approx(1546.0 - 400.0));
    requireNoOverlaps(*wardrobe);

    SECTION("resizing the compartment itself moves the partition and the neighbour")
    {
        columns[0]->setFixedSize(500.0);
        CHECK(columns[0]->size().width == Approx(500.0));
        CHECK(columns[1]->size().width == Approx(1046.0));
        CHECK(findAll<Partition>(*wardrobe).front()->box().min().x == Approx(518.0));
        // Shelves of the right column became narrower by 100.
        CHECK(columns[1]->shelves().front()->size().width == Approx(1046.0 - 2.0));
        requireNoOverlaps(*wardrobe);
    }

    SECTION("back to automatic: both columns share the space equally")
    {
        columns[0]->setFixedSize(std::nullopt);
        CHECK(columns[0]->size().width == Approx(773.0));
        CHECK(columns[1]->size().width == Approx(773.0));
    }
}

TEST_CASE("Editing after build: shelf count and back mount", "[core][parametric]")
{
    const auto wardrobe =
        WardrobeBuilder().rules(rulesWithoutSnapping()).shelvesPerSection(2).build();
    Carcass& c = *wardrobe->carcass();

    c.interior().setAutoShelfCount(6);
    CHECK(c.interior().shelves().size() == 6);
    const auto gaps = clearGapsBetweenShelves(c.interior());
    for (const Real gap : gaps) {
        CHECK(gap == Approx(gaps.front()));
    }

    c.setBackMount(BackMountType::Overlay);
    CHECK(c.leftSide().size().depth == Approx(557.0));
    CHECK(c.interior().shelves().front()->size().depth == Approx(560.0 - 3.0 - 20.0 - 2.0));
    requireNoOverlaps(*wardrobe);
}

TEST_CASE("Thicker material propagates through the formulas", "[core][parametric]")
{
    ConstructionRules rules;
    rules.carcassMaterial.thickness = 16.0;
    const auto wardrobe = WardrobeBuilder().rules(rules).shelvesPerSection(1).build();

    const Carcass& c = *wardrobe->carcass();
    CHECK_THAT(c.rightSide().box(), IsBox({784, 0, 0}, {800, 1984, 560}));
    CHECK(c.interiorBox().size().x == Approx(768.0));
    CHECK(c.interior().shelves().front()->size().width == Approx(766.0));
    requireNoOverlaps(*wardrobe);
}

TEST_CASE("Invalid dimensions are rejected and leave the model untouched", "[core][parametric]")
{
    const auto wardrobe = WardrobeBuilder().build();
    CHECK_THROWS_AS(wardrobe->setDimensions({0, 2000, 560}), std::invalid_argument);
    CHECK_THROWS_AS(wardrobe->setDimensions({800, -1, 560}), std::invalid_argument);
    CHECK(wardrobe->dimensions() == Size3D{800, 2000, 560});
    CHECK(wardrobe->carcass()->rightSide().box().max().x == Approx(800.0));
}

TEST_CASE("Every catalogue item is collision-free, also after resizing",
          "[core][parametric][catalog]")
{
    const CatalogItem item = GENERATE(from_range(catalogItems().begin(), catalogItems().end()));
    CAPTURE(static_cast<int>(item));

    const auto product = createProduct(item);
    REQUIRE(product->carcass() != nullptr);
    CHECK(collectPanels(*product).size() > 5);
    requireNoOverlaps(*product);

    const Size3D base = product->dimensions();
    const Real delta = GENERATE(-100.0, 100.0);
    product->setDimensions({base.width + delta, base.height + delta, base.depth + delta});
    requireNoOverlaps(*product);

    // Carcass parts stay inside the product box; fronts stand in front of it.
    for (const FurnitureComponent* part : collectPanels(*product)) {
        const bool isFront =
            part->kind() == ComponentKind::Door || part->kind() == ComponentKind::DrawerFront;
        if (isFront) {
            CHECK(part->box().min().z == Approx(product->dimensions().depth));
        } else {
            CHECK(product->box().contains(part->box()));
        }
    }
}
