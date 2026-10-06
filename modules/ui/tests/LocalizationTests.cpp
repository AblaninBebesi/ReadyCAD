#include "readycad/ui/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string_view>

using namespace rc::ui;

TEST_CASE("Every text id has a non-empty Turkish translation", "[ui][i18n]")
{
    setLanguage(Language::Turkish);
    for (std::size_t i = 0; i < static_cast<std::size_t>(TextId::Count); ++i) {
        INFO("TextId index " << i);
        CHECK_FALSE(text(static_cast<TextId>(i)).empty());
    }
}

TEST_CASE("Turkish strings keep their special letters (UTF-8)", "[ui][i18n]")
{
    setLanguage(Language::Turkish);
    CHECK(text(TextId::MenuExit) == "Çıkış");
    CHECK(text(TextId::MenuHelp) == "Yardım");
    // "ı" (dotless i) is two bytes in UTF-8: the string must be longer than its letter count.
    CHECK(text(TextId::MenuExit).size() > std::string_view("Cikis").size());
}

TEST_CASE("Out-of-range id does not crash", "[ui][i18n]")
{
    CHECK(text(TextId::Count) == "???");
}
