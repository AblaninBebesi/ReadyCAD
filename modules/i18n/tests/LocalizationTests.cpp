#include "readycad/i18n/Localization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using namespace rc::i18n;

namespace {

constexpr auto kTextCount = static_cast<std::size_t>(TextId::Count);

/// "Kesim planı###nesting" -> "###nesting": ImGui's id of a window.
std::string_view windowId(std::string_view value)
{
    const std::size_t at = value.find("###");
    return at == std::string_view::npos ? std::string_view() : value.substr(at);
}

/// The places of a template: "{}", "{1}"..."{9}".
std::size_t placeCount(std::string_view value)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i + 1 < value.size(); ++i) {
        if (value[i] == '{'
            && (value[i + 1] == '}'
                || (i + 2 < value.size() && value[i + 1] >= '1' && value[i + 1] <= '9'
                    && value[i + 2] == '}'))) {
            ++count;
        }
    }
    return count;
}

std::size_t formCount(std::string_view value)
{
    return static_cast<std::size_t>(std::count(value.begin(), value.end(), '|')) + 1;
}

} // namespace

TEST_CASE("Every text id has a non-empty Turkish translation", "[i18n]")
{
    setLanguage(Language::Turkish);
    for (std::size_t i = 0; i < kTextCount; ++i) {
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

TEST_CASE("Every language translates every text id", "[i18n][languages]")
{
    REQUIRE(languages().size() == 3);
    for (const Language language : languages()) {
        setLanguage(language);
        for (std::size_t i = 0; i < kTextCount; ++i) {
            INFO(languageCode(language) << " TextId index " << i);
            CHECK(text(static_cast<TextId>(i)) != "???");
        }
    }
    setLanguage(Language::Turkish);
}

TEST_CASE("Window ids, template places and word forms agree between languages",
          "[i18n][languages]")
{
    for (std::size_t i = 0; i < kTextCount; ++i) {
        const auto id = static_cast<TextId>(i);
        setLanguage(Language::Turkish);
        const std::string turkish(text(id));
        setLanguage(Language::Russian);
        const std::string russian(text(id));
        setLanguage(Language::English);
        const std::string english(text(id));
        INFO("TextId index " << i << ": " << turkish);

        // The window keeps its place and size when the language changes.
        CHECK(windowId(russian) == windowId(turkish));
        CHECK(windowId(english) == windowId(turkish));
        CHECK(placeCount(russian) == placeCount(turkish));
        CHECK(placeCount(english) == placeCount(turkish));

        // A counted word: one form in Turkish, two in English, three in Russian.
        CHECK(formCount(turkish) == 1);
        const bool counted = formCount(russian) > 1 || formCount(english) > 1;
        CHECK(formCount(russian) == (counted ? 3u : 1u));
        CHECK(formCount(english) == (counted ? 2u : 1u));
    }
    setLanguage(Language::Turkish);
}

TEST_CASE("Languages are known by their codes", "[i18n][languages]")
{
    for (const Language language : languages()) {
        CHECK(languageFromCode(languageCode(language)) == language);
    }
    CHECK(languageCode(Language::Russian) == "ru");
    CHECK(languageName(Language::Russian) == "Русский");
    CHECK(languageName(Language::Turkish) == "Türkçe");
    CHECK_FALSE(languageFromCode("de").has_value());
    CHECK_FALSE(languageFromCode("").has_value());
}

TEST_CASE("Texts follow the chosen language", "[i18n][languages]")
{
    setLanguage(Language::Russian);
    CHECK(currentLanguage() == Language::Russian);
    CHECK(text(TextId::MenuFile) == "Файл");
    setLanguage(Language::English);
    CHECK(std::string_view(tr(TextId::MenuFile)) == "File");
    setLanguage(Language::Turkish);
    CHECK(text(TextId::MenuFile) == "Dosya");
}

TEST_CASE("A language scope gives the language back", "[i18n][languages]")
{
    setLanguage(Language::Turkish);
    {
        const LanguageScope documents(Language::English);
        CHECK(text(TextId::SummaryCutting) == "Cutting plan");
        {
            const LanguageScope inner(Language::Russian);
            CHECK(currentLanguage() == Language::Russian);
        }
        CHECK(currentLanguage() == Language::English);
    }
    CHECK(currentLanguage() == Language::Turkish);
}

TEST_CASE("Templates put the values in the language's order", "[i18n][languages]")
{
    setLanguage(Language::Turkish);
    CHECK(format(TextId::ShapeFromEdge, {"G1", "100"}) == "G1'den 100");
    setLanguage(Language::English);
    CHECK(format(TextId::ShapeFromEdge, {"W1", "100"}) == "100 from W1");
    CHECK(format(TextId::ShapeFromCorner, {"L1–W1", "200 / 100"}) == "200 / 100 from corner L1–W1");
    setLanguage(Language::Russian);
    CHECK(format(TextId::ShapeFromEdge, {"Ш1", "100"}) == "100 от Ш1");

    // Too few values leave the places empty, too many are ignored: no crash either way.
    CHECK(format(TextId::ShapeFromEdge, {"Ш1"}) == " от Ш1");
    CHECK(format(TextId::ShapeFromEdge, {"Ш1", "100", "extra"}) == "100 от Ш1");
    CHECK(format(TextId::MenuFile, {"unused"}) == "Файл");
    setLanguage(Language::Turkish);
}

TEST_CASE("Counted words take the form the number asks for", "[i18n][languages]")
{
    setLanguage(Language::Russian);
    CHECK(countText(1, TextId::SheetsCount) == "1 лист");
    CHECK(countText(2, TextId::SheetsCount) == "2 листа");
    CHECK(countText(4, TextId::SheetsCount) == "4 листа");
    CHECK(countText(5, TextId::SheetsCount) == "5 листов");
    CHECK(countText(11, TextId::SheetsCount) == "11 листов");
    CHECK(countText(12, TextId::SheetsCount) == "12 листов");
    CHECK(countText(21, TextId::SheetsCount) == "21 лист");
    CHECK(countText(22, TextId::SheetsCount) == "22 листа");
    CHECK(countText(111, TextId::SheetsCount) == "111 листов");
    CHECK(countText(0, TextId::FilesWord) == "0 файлов");

    setLanguage(Language::English);
    CHECK(countText(1, TextId::SheetsCount) == "1 sheet");
    CHECK(countText(0, TextId::SheetsCount) == "0 sheets");
    CHECK(countText(21, TextId::SheetsCount) == "21 sheets");
    CHECK(countText(3, TextId::ProgramsWord) == "3 CNC programs");

    // Turkish keeps the noun singular after a number.
    setLanguage(Language::Turkish);
    CHECK(countText(1, TextId::SheetsCount) == "1 levha");
    CHECK(countText(7, TextId::SheetsCount) == "7 levha");
    // A text with a single form is the same for every number.
    CHECK(plural(TextId::MenuFile, 5) == "Dosya");
}

TEST_CASE("Package names stay ASCII in every language", "[i18n][languages]")
{
    for (const Language language : languages()) {
        setLanguage(language);
        for (const TextId id : {TextId::FolderProduction, TextId::FolderCutting,
                                TextId::FileSummary, TextId::FileCutList}) {
            const std::string_view name = text(id);
            INFO(languageCode(language) << ": " << name);
            CHECK(std::all_of(name.begin(), name.end(), [](char c) {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
            }));
        }
    }
    setLanguage(Language::Russian);
    CHECK(text(TextId::FolderProduction) == "PROIZVODSTVO");
    setLanguage(Language::Turkish);
    CHECK(text(TextId::FolderProduction) == "URETIM");
}
