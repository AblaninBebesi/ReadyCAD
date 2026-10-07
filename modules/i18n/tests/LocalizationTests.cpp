#include "readycad/i18n/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string_view>

using namespace rc::i18n;

TEST_CASE("Every text id has a non-empty Turkish translation", "[i18n]")
{
    setLanguage(Language::Turkish);
    for (std::size_t i = 0; i < static_cast<std::size_t>(TextId::Count); ++i) {
        INFO("TextId index " << i);
        CHECK(text(static_cast<TextId>(i)) != "???"); // "???" marks a missing translation
    }
}

TEST_CASE("Turkish strings keep their special letters (UTF-8)", "[i18n]")
{
    setLanguage(Language::Turkish);
    CHECK(text(TextId::MenuExit) == "Çıkış");
    CHECK(text(TextId::MenuHelp) == "Yardım");
    // "ı" (dotless i) is two bytes in UTF-8: the string must be longer than its letter count.
    CHECK(text(TextId::MenuExit).size() > std::string_view("Cikis").size());
}

TEST_CASE("Out-of-range id does not crash", "[i18n]")
{
    CHECK(text(TextId::Count) == "???");
}
