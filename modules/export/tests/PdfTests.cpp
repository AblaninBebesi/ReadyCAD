// Stage 6: the PDF files themselves — structure, page sizes, text, bookmarks, fonts.
//
// Pages are written uncompressed, and the built-in font keeps text as readable strings, so the
// tests can look into the file.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/export/Documentation.hpp"
#include "readycad/export/Dxf.hpp"
#include "readycad/export/Pdf.hpp"
#include "readycad/test/PdfReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <regex>
#include <string>

using namespace rc;
using namespace rc::exporting;
using Catch::Approx;
using rc::test::occurrences;

namespace {

/// "Kesim" -> "FEFF004B006500730069006D": an ASCII bookmark title in UTF-16BE hex.
std::string utf16Hex(const std::string& ascii)
{
    std::string hex = "FEFF";
    for (const char c : ascii) {
        char buffer[8];
        std::snprintf(buffer, sizeof buffer, "%04X", static_cast<unsigned>(c));
        hex += buffer;
    }
    return hex;
}

/// Two pages: a line and some text, A4 landscape and A3 portrait.
PageSet samplePages()
{
    PageSet set;
    set.title = "Test";
    for (const PageFormat format : {PageFormat{PaperSize::A4, Orientation::Landscape},
                                    PageFormat{PaperSize::A3, Orientation::Portrait}}) {
        Page page;
        page.size = pageSize(format);
        const std::size_t layer = page.drawing.addLayer(
            {"TEXT", 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thin});
        page.drawing.line(layer, Vec2(20.0, 20.0), Vec2(100.0, 20.0));
        page.drawing.circle(layer, Vec2(50.0, 50.0), 2.5);
        page.drawing.text(layer, {Vec2(20.0, 30.0), "Kesim listesi", 3.0, 0.0, HAlign::Left,
                                  VAlign::Baseline, false});
        page.drawing.text(layer, {Vec2(20.0, 40.0), "Çekmece yanı", 3.0, 90.0, HAlign::Center,
                                  VAlign::Middle, true});
        page.bookmark = format.size == PaperSize::A4 ? "Kesim listesi" : "";
        set.pages.push_back(std::move(page));
    }
    return set;
}

} // namespace

TEST_CASE("PDF structure: header, pages, sizes, end", "[export][pdf]")
{
    const std::string pdf = toPdf(samplePages(), PdfFonts{});
    CHECK(pdf.starts_with("%PDF-1."));
    CHECK(pdf.find("%%EOF") != std::string::npos);
    CHECK(pdf.find("%%EOF") > pdf.size() - 8);
    CHECK(rc::test::pdfPageCount(pdf) == 2);

    // Page sizes in points (1 mm = 72 / 25.4 pt): A4 landscape and A3 portrait.
    const std::regex mediaBox(R"(/MediaBox \[ ?0 0 ([0-9.]+) ([0-9.]+) ?\])");
    std::vector<std::pair<double, double>> sizes;
    for (auto it = std::sregex_iterator(pdf.begin(), pdf.end(), mediaBox);
         it != std::sregex_iterator(); ++it) {
        sizes.emplace_back(std::stod((*it)[1]), std::stod((*it)[2]));
    }
    REQUIRE(sizes.size() == 2);
    CHECK(sizes[0].first == Approx(297.0 * 72.0 / 25.4).margin(0.01));
    CHECK(sizes[0].second == Approx(210.0 * 72.0 / 25.4).margin(0.01));
    CHECK(sizes[1].first == Approx(297.0 * 72.0 / 25.4).margin(0.01));
    CHECK(sizes[1].second == Approx(420.0 * 72.0 / 25.4).margin(0.01));
}

TEST_CASE("PDF text: Turkish letters with the built-in font", "[export][pdf]")
{
    const std::string pdf = toPdf(samplePages(), PdfFonts{});
    CHECK(occurrences(pdf, "(Kesim listesi) Tj") == 2);
    // "Ç" and "ı" in Windows-1254 are bytes 0xC7 and 0xFD, written as octal escapes.
    CHECK(occurrences(pdf, "(\\307ekmece yan\\375) Tj") == 2);
    CHECK(pdf.find("/Encoding /WinAnsiEncoding") == std::string::npos);
}

TEST_CASE("PDF text: Russian letters with the built-in font come out in Latin ones",
          "[export][pdf]")
{
    PageSet set = samplePages();
    Page& page = set.pages.front();
    const std::size_t layer =
        page.drawing.addLayer({"RU", 7, {0, 0, 0}, LineType::Continuous, LineWeight::Thin});
    page.drawing.text(layer, {Vec2(20.0, 50.0), "Карта раскроя", 3.0, 0.0, HAlign::Left,
                              VAlign::Baseline, false});
    const std::string pdf = toPdf(set, PdfFonts{});
    CHECK(occurrences(pdf, "(Karta raskroia) Tj") == 1); // Helvetica has no Cyrillic
    CHECK(pdf.find("?????") == std::string::npos);
}

TEST_CASE("PDF bookmarks for the pages that have one", "[export][pdf]")
{
    const std::string pdf = toPdf(samplePages(), PdfFonts{});
    CHECK(pdf.find("/Outlines") != std::string::npos);
    CHECK(occurrences(pdf, "/Title <" + utf16Hex("Kesim listesi") + ">") == 1);
}

TEST_CASE("PDF with an embedded TrueType font", "[export][pdf]")
{
    const PdfFonts fonts = findPdfFonts();
    if (fonts.builtIn()) {
        SKIP("no TrueType font with Turkish letters on this machine");
    }
    const std::string pdf = toPdf(samplePages(), fonts);
    CHECK(rc::test::pdfPageCount(pdf) == 2);
    CHECK(pdf.find("/FontFile2") != std::string::npos);  // the font travels with the file
    CHECK(pdf.find("/ToUnicode") != std::string::npos);  // text can be searched and copied
    CHECK(pdf.find("/CIDFontType2") != std::string::npos);
    CHECK(pdf.size() < 2'000'000); // only the used glyphs are embedded
}

TEST_CASE("Font metrics measure like the PDF prints", "[export][pdf]")
{
    const PdfFontMetrics builtIn(PdfFonts{});
    CHECK(builtIn.width("Kesim", 2.5, false) > 0.0);
    CHECK(builtIn.width("Kesim listesi", 2.5, false) > builtIn.width("Kesim", 2.5, false));
    CHECK(builtIn.width("Kesim", 2.5, true) > builtIn.width("Kesim", 2.5, false));
    CHECK(builtIn.width("Kesim", 5.0, false) == Approx(2.0 * builtIn.width("Kesim", 2.5, false)));
    CHECK(builtIn.width("ş", 2.5, false) > 0.0);
    CHECK(builtIn.width("", 2.5, false) == 0.0);

    const PdfFonts fonts = findPdfFonts();
    if (!fonts.builtIn()) {
        const PdfFontMetrics ttf(fonts);
        CHECK(ttf.width("Çekmece yanı", 2.5, false) > ttf.width("Çekmece", 2.5, false));
    }
}

TEST_CASE("Documentation saved as PDF", "[export][pdf]")
{
    const auto product = core::createProduct(core::CatalogItem::ChestOfDrawers);
    const auto report = detailing::applyDetailing(*product);
    const auto cutList = detailing::buildCutList(*product, product->rules());
    const auto edges = detailing::edgeBandUsage(cutList, product->rules());
    const DocumentationInput input{*product, cutList, edges, report};
    DocumentationOptions options;
    options.date = "07.10.2026";

    const test::TempFolder folder("pdf");
    const std::filesystem::path file = folder.path() / "Şifonyer.pdf"; // non-ASCII path
    saveDocumentationPdf(input, options, file, PdfFonts{});
    REQUIRE(std::filesystem::exists(file));
    const std::string pdf = test::readFile(file);
    const PageSet pages = buildDocumentation(input, options, PdfFontMetrics(PdfFonts{}));
    CHECK(rc::test::pdfPageCount(pdf) == pages.pages.size());
    CHECK(pdf.find("(Kesim listesi) Tj") != std::string::npos);

    // A folder where the file should be: a clear error instead of a broken file.
    std::filesystem::create_directories(folder.path() / "taken.pdf");
    CHECK_THROWS_AS(savePdf(pages, folder.path() / "taken.pdf", PdfFonts{}), ExportError);
}

TEST_CASE("Missing font file is reported", "[export][pdf]")
{
    PdfFonts fonts;
    fonts.regular = "no/such/font.ttf";
    CHECK_THROWS_AS(PdfFontMetrics(fonts), ExportError);
    CHECK_THROWS_AS((void)toPdf(samplePages(), fonts), ExportError);
}
