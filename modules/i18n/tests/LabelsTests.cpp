#include "readycad/i18n/Labels.hpp"

#include "readycad/core/builders/WardrobeBuilder.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rc;
using namespace rc::i18n;

TEST_CASE("Lengths are shown without useless decimals", "[i18n][labels]")
{
    CHECK(mm(1982.0) == "1982");
    CHECK(mm(559.5) == "559.5");
    CHECK(mm(761.2000000001) == "761.2");
    CHECK(mm(18.04) == "18");
}

TEST_CASE("Tree names are numbered only among siblings of the same kind", "[i18n][labels]")
{
    const auto wardrobe = core::WardrobeBuilder().shelvesPerSection(3).doorsPerSection(1).build();
    const core::Section& interior = wardrobe->carcass()->interior();

    CHECK(displayName(*interior.shelves()[1]) == "Raf 2");
    CHECK(displayName(*interior.doors()[0]) == "Kapak"); // the only door: no number
    CHECK(displayName(*wardrobe) == "Ürün");

    interior.shelves()[0]->setName("Ayakkabı rafı");
    CHECK(displayName(*interior.shelves()[0]) == "Ayakkabı rafı");
}
