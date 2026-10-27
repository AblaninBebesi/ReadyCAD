#include "readycad/i18n/Labels.hpp"

#include "readycad/core/PanelShape.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/i18n/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace rc;
using namespace rc::i18n;

TEST_CASE("Lengths are shown without useless decimals", "[i18n][labels]")
{
    CHECK(mm(1982.0) == "1982");
    CHECK(mm(559.5) == "559.5");
    CHECK(mm(761.2000000001) == "761.2");
    CHECK(mm(18.04) == "18");
}

TEST_CASE("Units and percentages are written as the language writes them", "[i18n][labels]")
{
    setLanguage(Language::Turkish);
    CHECK(mm(720.0) + mmUnit() == "720 mm");
    CHECK(percent(0.784) == "%78");
    setLanguage(Language::Russian);
    CHECK(mm(720.0) + mmUnit() == "720 мм");
    CHECK(percent(0.784) == "78%");
    setLanguage(Language::English);
    CHECK(percent(1.0) == "100%");
    setLanguage(Language::Turkish);
}

TEST_CASE("Cut-outs are described in the program's language", "[i18n][labels]")
{
    const core::ShapeFeature notch = core::EdgeNotch{};
    const core::ShapeFeature cutout = core::InnerCutout{};

    setLanguage(Language::Turkish);
    CHECK(featureText(notch) == "Kenar kesimi U1: G1'den 100, 100 × 30");
    setLanguage(Language::English);
    CHECK(featureText(notch) == "Edge notch L1: 100 from W1, 100 × 30");
    CHECK(featureText(cutout) == "Inner cut-out 100 × 60 R5: 200 / 100 from corner L1–W1");
    setLanguage(Language::Russian);
    CHECK(featureText(notch) == std::string(text(TextId::ShapeEdgeNotch)) + " Д1: 100 от Ш1, 100 × 30");
    setLanguage(Language::Turkish);
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
