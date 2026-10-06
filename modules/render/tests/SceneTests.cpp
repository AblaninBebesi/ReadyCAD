// Stage 5: what gets drawn and what gets picked (no OpenGL needed).

#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/render/Scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rc;
using namespace rc::core;
using render::ItemCategory;
using Catch::Approx;

namespace {

std::size_t countCategory(const render::Scene& scene, ItemCategory category)
{
    std::size_t n = 0;
    for (const auto& box : scene.boxes) {
        n += box.category == category ? 1 : 0;
    }
    return n;
}

/// A ray from far in front of the product straight towards -Z at (x, y).
render::Ray rayFromFront(float x, float y)
{
    return {glm::vec3(x, y, 5000.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
}

} // namespace

TEST_CASE("Every panel and every handle becomes a box", "[render][scene]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    const render::Scene scene = render::buildScene(*wardrobe, {});

    const std::size_t panels = collectPanels(*wardrobe).size();
    CHECK(scene.boxes.size() == panels + 2); // + 2 door handles
    CHECK(countCategory(scene, ItemCategory::Front) == 2);
    CHECK(countCategory(scene, ItemCategory::Handle) == 2);
    CHECK(countCategory(scene, ItemCategory::Back) == 1);
    CHECK(scene.holes.empty()); // holes are off by default
    CHECK(scene.bounds.size().z == Approx(578.0)); // carcass + door thickness
}

TEST_CASE("Hiding the fronts shows the interior", "[render][scene]")
{
    const auto wardrobe = WardrobeBuilder().doorsPerSection(2).build();
    render::SceneOptions options;
    options.showFronts = false;
    const render::Scene scene = render::buildScene(*wardrobe, options);
    CHECK(countCategory(scene, ItemCategory::Front) == 0);
    CHECK(countCategory(scene, ItemCategory::Handle) == 0);
}

TEST_CASE("Holes are drawn on the panel surfaces", "[render][scene]")
{
    const auto wardrobe = WardrobeBuilder().build();
    const auto report = detailing::applyDetailing(*wardrobe);
    render::SceneOptions options;
    options.showHoles = true;
    const render::Scene scene = render::buildScene(*wardrobe, options);
    CHECK(static_cast<int>(scene.holes.size()) == report.drillings);

    // The first handle hole of the left door: on its outer face, looking at the viewer.
    bool found = false;
    for (const auto& hole : scene.holes) {
        if (hole.normal.z > 0.5f && hole.center.z > 578.0f && hole.radius == Approx(2.5f)) {
            CHECK(hole.center.x == Approx(358.5f));
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("Selection tints the selected subtree", "[render][scene]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(3).doorsPerSection(0).build();
    const Section& interior = wardrobe->carcass()->interior();

    render::SceneOptions options;
    options.selection = interior.id();
    const render::Scene scene = render::buildScene(*wardrobe, options);

    std::size_t selected = 0;
    for (const auto& box : scene.boxes) {
        selected += box.selected ? 1 : 0;
    }
    CHECK(selected == 3);                    // the shelves inside the section
    REQUIRE(scene.highlight.has_value());   // plus a frame around the section itself
    CHECK(scene.highlight->min().x == Approx(18.0));
}

TEST_CASE("Picking finds the nearest part under the mouse", "[render][picking]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(4).doorsPerSection(2).build();
    const Section& interior = wardrobe->carcass()->interior();

    SECTION("the door is in front of everything")
    {
        const render::Scene scene = render::buildScene(*wardrobe, {});
        CHECK(render::pick(scene, rayFromFront(200.0f, 1000.0f)) == interior.doors()[0]->id());
        CHECK(render::pick(scene, rayFromFront(600.0f, 1000.0f)) == interior.doors()[1]->id());
        // Clicking the handle selects its door.
        CHECK(render::pick(scene, rayFromFront(358.5f, 1000.0f)) == interior.doors()[0]->id());
        CHECK_FALSE(render::pick(scene, rayFromFront(2000.0f, 1000.0f)).has_value());
    }

    SECTION("without fronts the click reaches a shelf or the back panel")
    {
        render::SceneOptions options;
        options.showFronts = false;
        const render::Scene scene = render::buildScene(*wardrobe, options);
        const Shelf& shelf = *interior.shelves()[0];
        const float shelfY = static_cast<float>(shelf.box().center().y);
        CHECK(render::pick(scene, rayFromFront(400.0f, shelfY)) == shelf.id());
        CHECK(render::pick(scene, rayFromFront(400.0f, 200.0f))
              == wardrobe->carcass()->back().id());
    }
}

TEST_CASE("Ray-box intersection", "[render][picking]")
{
    const glm::vec3 min(0.0f);
    const glm::vec3 max(10.0f);
    CHECK(render::intersect({glm::vec3(5, 5, 20), glm::vec3(0, 0, -1)}, min, max) == Approx(10.0f));
    CHECK_FALSE(render::intersect({glm::vec3(15, 5, 20), glm::vec3(0, 0, -1)}, min, max));
    CHECK_FALSE(render::intersect({glm::vec3(5, 5, 20), glm::vec3(0, 0, 1)}, min, max)); // behind
    CHECK(render::intersect({glm::vec3(5, 5, 5), glm::vec3(1, 0, 0)}, min, max) == Approx(0.0f));
}
