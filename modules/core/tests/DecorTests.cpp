// Decors: the workshop's catalogue and the names of materials in a decor.

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/Decor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace rc::core;

TEST_CASE("The decor catalogue: unique ids, the default materials use it", "[core][decor]")
{
    const std::vector<Decor> catalog = defaultDecorCatalog();
    std::set<std::string> ids;
    for (const Decor& d : catalog) {
        CHECK(ids.insert(d.id).second);
        CHECK_FALSE(d.name.empty());
    }
    const ConstructionRules rules;
    for (const Material* m : {&rules.carcassMaterial, &rules.frontMaterial, &rules.worktopMaterial}) {
        const Decor* decor = findDecor(rules.decors, m->decor);
        REQUIRE(decor != nullptr);
        CHECK(m->hasGrain == decor->grain); // a wood carcass keeps its grain in the cutting plan
    }
    CHECK(findDecor(catalog, "") == nullptr);
    CHECK(findDecor(catalog, "no-such") == nullptr);
}

TEST_CASE("A material in another decor is another material", "[core][decor]")
{
    const std::vector<Decor> catalog = defaultDecorCatalog();
    const Material carcass = ConstructionRules{}.carcassMaterial; // Suntalam 18, beige wood

    // The default name has no decor in it: another decor is added...
    const Material white = withDecor(carcass, findDecor(catalog, "white"), catalog);
    CHECK(white.name == "Suntalam 18 Beyaz");
    CHECK(white.decor == "white");
    CHECK_FALSE(white.hasGrain); // a plain colour: parts may be turned
    CHECK(white.thickness == carcass.thickness);

    // ...and replaced by the next one, not stacked.
    const Material walnut = withDecor(white, findDecor(catalog, "wood-dark"), catalog);
    CHECK(walnut.name == "Suntalam 18 Koyu kahve ahşap");
    CHECK(walnut.hasGrain);

    // Without a decor: the material's own name, the grain as it was.
    const Material plain = withDecor(walnut, nullptr, catalog);
    CHECK(plain.name == "Suntalam 18");
    CHECK(plain.decor.empty());
    CHECK(plain.hasGrain);
}
