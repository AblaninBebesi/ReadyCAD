// Stage 5: what gets drawn and what gets picked (no OpenGL needed).

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/render/Scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <algorithm>

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

TEST_CASE("A worktop is drawn around its cut-outs", "[render][scene][kitchen]")
{
    const auto kitchen = createProduct(CatalogItem::Kitchen);
    const render::Scene scene = render::buildScene(*kitchen, {});
    const Worktop& worktop = kitchen->kitchen()->worktop();

    // The sink is open: a ray from above into it (in front of the wall cabinets) reaches the
    // cabinet below, not the worktop.
    const render::Ray intoTheSink{glm::vec3(700.0f, 3000.0f, 450.0f), glm::vec3(0.0f, -1.0f, 0.0f)};
    const auto hit = render::pick(scene, intoTheSink);
    REQUIRE(hit.has_value());
    CHECK(*hit != worktop.id());
    // Next to it the worktop is solid.
    const render::Ray beside{glm::vec3(1200.0f, 3000.0f, 450.0f), glm::vec3(0.0f, -1.0f, 0.0f)};
    CHECK(render::pick(scene, beside) == worktop.id());

    // A prism: its top face is the worktop without the two openings (rounded corners as
    // straight pieces: a fraction of a square millimetre each).
    const auto prism = std::find_if(scene.prisms.begin(), scene.prisms.end(),
                                    [&](const render::ScenePrism& p) { return p.component == worktop.id(); });
    REQUIRE(prism != scene.prisms.end());
    CHECK(prism->category == ItemCategory::Worktop);
    double top = 0.0;
    for (std::size_t i = 0; i < prism->normals.size(); ++i) {
        if (prism->normals[i].y > 0.5f) {
            const glm::dvec3 a(prism->triangles[3 * i]);
            const glm::dvec3 b(prism->triangles[3 * i + 1]);
            const glm::dvec3 c(prism->triangles[3 * i + 2]);
            top += glm::length(glm::cross(b - a, c - a)) / 2.0;
        }
    }
    const double pi = 3.14159265358979;
    const double sink = 560.0 * 480.0 - (4.0 - pi) * 15.0 * 15.0;
    const double hob = 560.0 * 490.0 - (4.0 - pi) * 5.0 * 5.0;
    CHECK(top == Approx(2400.0 * 600.0 - sink - hob).epsilon(1e-4));
    CHECK(countCategory(scene, ItemCategory::Plinth) == 5);
}

TEST_CASE("A notched side wall is a prism; plain panels stay boxes", "[render][scene][shape]")
{
    const auto wardrobe = WardrobeBuilder().build();
    Carcass& carcass = *wardrobe->carcass();
    // A skirting board notch at the rear bottom corner (L1 is the rear edge, W1 the bottom end).
    carcass.leftSide().setShape({CornerNotch{PanelCorner::L1W1, 100.0, 15.0}});
    const render::Scene scene = render::buildScene(*wardrobe, {});
    REQUIRE(scene.prisms.size() == 1);
    CHECK(scene.boxes.size() == collectPanels(*wardrobe).size() - 1 + 2); // + 2 door handles

    // The notch is empty: a ray along X through it at the rear bottom misses the wall...
    const render::Ray throughNotch{glm::vec3(-100.0f, 50.0f, 7.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
    const auto hit = render::pick(scene, throughNotch);
    CHECK((!hit || *hit != carcass.leftSide().id()));
    // ...above it the wall is there.
    const render::Ray aboveNotch{glm::vec3(-100.0f, 150.0f, 7.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
    CHECK(render::pick(scene, aboveNotch) == carcass.leftSide().id());
}

TEST_CASE("Panels show the picture of their decor, along their length", "[render][scene][decor]")
{
    const auto wardrobe = WardrobeBuilder().build();
    const render::Scene scene = render::buildScene(*wardrobe, {});
    const Carcass& c = *wardrobe->carcass();
    const auto boxOf = [&](ComponentId id) {
        return *std::find_if(scene.boxes.begin(), scene.boxes.end(),
                             [&](const render::SceneBox& b) { return b.component == id; });
    };
    // The side wall: beige wood, the grain up the wall (its length runs along Y).
    const render::SceneBox wall = boxOf(c.leftSide().id());
    CHECK(wall.surface.texture == "Wood_Beige.jpg");
    CHECK(wall.surface.grain == glm::vec3(0.0f, 1.0f, 0.0f));
    CHECK(wall.surface.across == glm::vec3(0.0f, 0.0f, 1.0f));
    // The top panel: the grain across the wardrobe.
    CHECK(boxOf(c.top().id()).surface.grain == glm::vec3(1.0f, 0.0f, 0.0f));
    // Doors white; the back without a decor: the colour of its kind.
    CHECK(boxOf(c.interior().doors()[0]->id()).surface.texture == "Plastic_White.jpg");
    CHECK(boxOf(c.back().id()).surface.texture.empty());
    // The colour shown while a picture is missing: its average.
    CHECK(wall.color.r == Approx(167.0f / 255.0f));
}
