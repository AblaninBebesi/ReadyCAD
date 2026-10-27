// Stage 9: the production pipeline — what goes onto the sheets, the check that a cutting plan
// still belongs to the product, the single outputs and the package that holds them all.

#include "readycad/editor/Operations.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/production/Pipeline.hpp"
#include "readycad/production/Summary.hpp"
#include "readycad/storage/Storage.hpp"
#include "readycad/test/DxfReader.hpp"
#include "readycad/test/PdfReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace rc;
using namespace rc::production;
using Catch::Matchers::ContainsSubstring;

namespace {

nesting::NestingSettings quick(nesting::Machine machine)
{
    nesting::NestingSettings s;
    s.machine = machine;
    s.optimizer.populationSize = 6;
    s.optimizer.generations = 3;
    return s;
}

int pieces(const std::vector<nesting::NestingPart>& parts)
{
    int n = 0;
    for (const nesting::NestingPart& p : parts) {
        n += p.quantity;
    }
    return n;
}

std::size_t filesIn(const std::filesystem::path& folder)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) {
        return 0;
    }
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(folder),
                                                  std::filesystem::directory_iterator()));
}

std::vector<std::string> lines(const std::string& text)
{
    std::vector<std::string> out;
    std::string line;
    for (const char c : text) {
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            out.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        out.push_back(line);
    }
    return out;
}

const exporting::PdfFonts kBuiltInFonts{}; // the PDF's own font: no font files needed

std::string txt(i18n::TextId id)
{
    return std::string(i18n::text(id));
}

} // namespace

TEST_CASE("The cutting parts follow the cut list, the quantity and the parts left out",
          "[production][pipeline]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::WardrobeWithDrawers);
    int total = 0;
    for (const auto& row : doc.cutList()) {
        total += row.quantity;
    }
    CHECK(pieces(cuttingParts({doc, 1})) == total);
    CHECK(pieces(cuttingParts({doc, 3})) == 3 * total);

    const core::ComponentId back = doc.product().carcass()->back().id();
    editor::setInCuttingPlan(doc, {back}, false);
    CHECK(pieces(cuttingParts({doc, 3})) == 3 * (total - 1));
    for (const nesting::NestingPart& p : cuttingParts({doc, 1})) {
        CHECK(doc.cutList()[*p.cutListRow].inCuttingPlan);
    }
    CHECK_THROWS_AS(cuttingParts({doc, 0}), std::invalid_argument);
}

TEST_CASE("A cutting plan belongs to the product as it was", "[production][pipeline]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    const nesting::NestingResult plan =
        planCutting({doc, 1}, nesting::StockManager{}, quick(nesting::Machine::NestingCenter));
    CHECK(plan.unplaced.empty());
    CHECK(planMatches({doc, 1}, plan));
    CHECK_FALSE(planMatches({doc, 2}, plan)); // another quantity
    CHECK_FALSE(planMatches({doc, 0}, plan));

    const rc::test::TempFolder folder("stale_plan");
    editor::setDimensions(doc, {900, 2000, 560});
    CHECK_FALSE(planMatches({doc, 1}, plan));
    // Files from a stale plan would cut parts that no longer exist.
    CHECK_THROWS_AS(writeSheetDxfs({doc, 1}, plan, folder.path() / "KESIM"), ProductionError);
    CHECK_THROWS_AS(writeCncPrograms({doc, 1}, plan, cam::defaultProfile(), folder.path() / "CNC"),
                    ProductionError);
    CHECK_THROWS_AS(writePackage({doc, 1}, &plan, cam::defaultProfile(), {}, kBuiltInFonts,
                                 folder.path() / "paket"),
                    ProductionError);
    CHECK(filesIn(folder.path()) == 0);

    doc.undo(); // the same product again: the plan is valid again
    CHECK(planMatches({doc, 1}, plan));
}

TEST_CASE("The production package holds everything the workshop needs",
          "[production][package]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::WardrobeWithDrawers);
    editor::setProductName(doc, "Yatak odası dolabı");
    editor::setInCuttingPlan(doc, {doc.product().carcass()->back().id()}, false);
    const Order order{doc, 2};
    const nesting::NestingResult plan =
        planCutting(order, nesting::StockManager{}, quick(nesting::Machine::NestingCenter));
    REQUIRE(plan.unplaced.empty());

    const rc::test::TempFolder temp("package_router");
    PackageOptions options;
    options.documentation.date = "12.10.2026";
    const Package p = writePackage(order, &plan, cam::defaultProfile(), options, kBuiltInFonts,
                                   temp.path() / "paket");

    // Documents with the cutting plan pages.
    CHECK(p.documents.filename() == "Yatak_odasi_dolabi.pdf");
    const std::string pdf = rc::test::readFile(p.documents);
    CHECK(rc::test::isCompletePdf(pdf));
    CHECK(rc::test::pdfPageCount(pdf) > plan.layouts.size());

    // One drawing per part and per sheet, one program per sheet.
    CHECK(p.partDrawings.size() == doc.cutList().size());
    CHECK(filesIn(p.folder / "DXF") == doc.cutList().size());
    CHECK(p.sheetDrawings.size() == plan.layouts.size());
    CHECK(p.programs.size() == plan.layouts.size());
    CHECK(p.cnc.size() == plan.layouts.size());
    CHECK(filesIn(p.folder / "CNC") == plan.layouts.size());
    for (const auto& file : p.sheetDrawings) {
        const auto pairs = rc::test::parseDxf(rc::test::readFile(file));
        CHECK(pairs.back().value == "EOF");
    }

    // The design itself: the same product comes back from it.
    const auto product = storage::loadProduct(rc::test::readFile(p.project));
    CHECK(product->name() == "Yatak odası dolabı");
    CHECK_FALSE(product->carcass()->back().inCuttingPlan());

    // The cut list for the saw's optimiser: a row per cut list row, quantities of the order.
    const std::string csv = rc::test::readFile(p.cutListCsv);
    CHECK(csv.rfind("\xEF\xBB\xBF", 0) == 0);
    const auto rows = lines(csv.substr(3));
    REQUIRE(rows.size() == doc.cutList().size() + 1);
    CHECK_THAT(rows[0], ContainsSubstring(txt(i18n::TextId::InCuttingPlan)));
    for (std::size_t i = 0; i < doc.cutList().size(); ++i) {
        const auto& row = doc.cutList()[i];
        const std::string quantity = ";" + std::to_string(2 * row.quantity) + ";";
        CHECK_THAT(rows[i + 1], ContainsSubstring(quantity));
        CHECK_THAT(rows[i + 1],
                   ContainsSubstring(txt(row.inCuttingPlan ? i18n::TextId::Yes : i18n::TextId::No)));
    }

    // The summary, for Notepad.
    const std::string summary = rc::test::readFile(p.summary);
    CHECK(summary.rfind("\xEF\xBB\xBF", 0) == 0);
    CHECK(summary.find("\r\n") != std::string::npos);
    CHECK_THAT(summary, ContainsSubstring("12.10.2026"));
    CHECK_THAT(summary, ContainsSubstring(txt(i18n::TextId::OutOfPlanTitle)));
    CHECK_THAT(summary, ContainsSubstring("CNC/"));
    CHECK_THAT(p.summaryText, ContainsSubstring(p.programs.front().filename().string()));
}

TEST_CASE("A panel saw package has sheet drawings but no CNC programs", "[production][package]")
{
    const editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    const nesting::NestingResult plan =
        planCutting({doc, 1}, nesting::StockManager{}, quick(nesting::Machine::PanelSaw));
    const rc::test::TempFolder temp("package_saw");
    const Package p = writePackage({doc, 1}, &plan, cam::defaultProfile(), {}, kBuiltInFonts,
                                   temp.path() / "paket");
    CHECK(p.sheetDrawings.size() == plan.layouts.size());
    CHECK(p.programs.empty());
    CHECK_FALSE(std::filesystem::exists(p.folder / "CNC"));
    CHECK_THAT(p.summaryText, ContainsSubstring(txt(i18n::TextId::SawProgramsNote)));
}

TEST_CASE("Without a cutting plan the package has the documents and part drawings",
          "[production][package]")
{
    const editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Bookcase);
    const rc::test::TempFolder temp("package_noplan");
    const Package p = writePackage({doc, 1}, nullptr, cam::defaultProfile(), {}, kBuiltInFonts,
                                   temp.path() / "paket");
    CHECK(std::filesystem::exists(p.documents));
    CHECK(p.partDrawings.size() == doc.cutList().size());
    CHECK(p.sheetDrawings.empty());
    CHECK(p.programs.empty());
    CHECK_THAT(p.summaryText, ContainsSubstring(txt(i18n::TextId::NoCuttingPlan)));
}

TEST_CASE("A package goes into an empty folder, all or nothing", "[production][package]")
{
    const editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    const nesting::NestingResult plan =
        planCutting({doc, 1}, nesting::StockManager{}, quick(nesting::Machine::NestingCenter));
    const rc::test::TempFolder temp("package_folder");

    // A folder with files in it: an old package must not be mixed with the new one.
    const auto used = temp.path() / "eski";
    std::filesystem::create_directories(used);
    storage::writeTextFile(used / "01_eski.nc", "M30");
    CHECK_THROWS_AS(writePackage({doc, 1}, &plan, cam::defaultProfile(), {}, kBuiltInFonts, used),
                    ProductionError);
    CHECK(filesIn(used) == 1);

    // An empty folder is fine.
    const auto empty = temp.path() / "bos";
    std::filesystem::create_directories(empty);
    CHECK_NOTHROW(
        (void)writePackage({doc, 1}, &plan, cam::defaultProfile(), {}, kBuiltInFonts, empty));
    CHECK(std::filesystem::exists(empty / "OZET.txt"));

    // A router wider than the gap the nesting left would cut into the neighbours: the package
    // fails and leaves nothing behind.
    cam::NestingCenterProfile thick = cam::defaultProfile();
    for (cam::Tool& t : thick.tools) {
        if (t.number == thick.contourTool) {
            t.diameter = 20.0;
        }
    }
    const auto failed = temp.path() / "hatali";
    CHECK_THROWS(writePackage({doc, 1}, &plan, thick, {}, kBuiltInFonts, failed));
    CHECK_FALSE(std::filesystem::exists(failed));
}

TEST_CASE("A new package never lands in the folder of an older one", "[production][package]")
{
    const rc::test::TempFolder temp("package_names");
    const auto first = newPackageFolder(temp.path(), "Gardırop 1/2");
    CHECK(first.filename() == "Gardirop_1_2_URETIM");
    std::filesystem::create_directories(first);
    const auto second = newPackageFolder(temp.path(), "Gardırop 1/2");
    CHECK(second.filename() == "Gardirop_1_2_URETIM_2");
    std::filesystem::create_directories(second);
    CHECK(newPackageFolder(temp.path(), "Gardırop 1/2").filename() == "Gardirop_1_2_URETIM_3");
}

TEST_CASE("The order is named after the project file, else after the product",
          "[production][pipeline]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    CHECK(orderName(doc) == txt(i18n::TextId::Untitled));
    editor::setProductName(doc, "Antre dolabı");
    CHECK(orderName(doc) == "Antre dolabı");
    const rc::test::TempFolder temp("order_name");
    doc.saveAs(temp.path() / "Müşteri Yılmaz.rcproj");
    CHECK(orderName(doc) == "Müşteri Yılmaz");
}

TEST_CASE("A package in Russian names its files in Latin letters", "[production][package]")
{
    const editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    const Order order{doc, 1};
    const nesting::NestingResult plan =
        planCutting(order, nesting::StockManager{}, quick(nesting::Machine::PanelSaw));
    const rc::test::TempFolder temp("package_ru");

    const i18n::LanguageScope russian(i18n::Language::Russian);
    CHECK(newPackageFolder(temp.path(), "Шкаф-купе").filename() == "Shkaf-kupe_PROIZVODSTVO");
    PackageOptions options;
    options.documentation.date = "27.10.2026";
    const Package p = writePackage(order, &plan, cam::defaultProfile(), options, kBuiltInFonts,
                                   temp.path() / "paket");

    // Machines and old programs read these names: ASCII, in the language's words.
    CHECK(p.summary.filename() == "SVODKA.txt");
    CHECK(p.cutListCsv.filename() == "Bez_imeni_spetsifikatsiya.csv");
    CHECK(std::filesystem::is_directory(p.folder / "RASKROY"));
    CHECK(p.sheetDrawings.size() == plan.layouts.size());

    // The summary itself speaks Russian, with the right forms after the numbers.
    const std::string summary = rc::test::readFile(p.summary);
    CHECK_THAT(summary, ContainsSubstring(txt(i18n::TextId::SummaryCutting)));
    CHECK_THAT(summary, ContainsSubstring("RASKROY/"));
    CHECK_THAT(summary, ContainsSubstring(i18n::countText(
                            static_cast<long long>(plan.layouts.size()), i18n::TextId::FilesWord)));
    CHECK_THAT(summary, !ContainsSubstring("Kesim planı"));
}

TEST_CASE("The cut list CSV opens in an English spreadsheet", "[production][csv]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    editor::setDimensions(doc, {801, 2000, 560}); // doors 397.5 wide
    const auto shelf = doc.product().carcass()->interior().shelves().front()->id();
    doc.edit("Name", [&](core::Product& p) { p.find(shelf)->setName("Shelf, glass; top"); });
    const rc::test::TempFolder temp("csv_en");

    const i18n::LanguageScope english(i18n::Language::English);
    writeCutListCsv({doc, 1}, temp.path() / "list.csv");
    const std::string csv = rc::test::readFile(temp.path() / "list.csv");
    CHECK(lines(csv.substr(3)).front().rfind("No,Part,Material,Thickness,", 0) == 0);
    // Commas separate the cells; a ';' inside a name needs no quotes here.
    CHECK_THAT(csv, ContainsSubstring(",\"Shelf, glass; top\","));
    // Decimal points: 397.5 stays 397.5.
    int fractions = 0;
    for (const auto& row : doc.cutList()) {
        for (const core::Real size : {row.cut.width, row.finishWidth}) {
            const std::string text = i18n::mm(size);
            if (text.find('.') != std::string::npos) {
                ++fractions;
                CHECK(csv.find("," + text + ",") != std::string::npos);
            }
        }
    }
    CHECK(fractions > 0);
}

TEST_CASE("The cut list CSV opens in a Turkish spreadsheet", "[production][csv]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::Wardrobe);
    editor::setDimensions(doc, {801, 2000, 560}); // doors 397.5 wide
    const auto shelf = doc.product().carcass()->interior().shelves().front()->id();
    doc.edit("Name", [&](core::Product& p) { p.find(shelf)->setName("Raf; \"cam\""); });
    const rc::test::TempFolder temp("csv");
    writeCutListCsv({doc, 1}, temp.path() / "liste.csv");
    const std::string csv = rc::test::readFile(temp.path() / "liste.csv");
    // The separator and quotes inside a name are quoted as Excel expects.
    CHECK_THAT(csv, ContainsSubstring(";\"Raf; \"\"cam\"\"\";"));
    // Decimal commas: 559.5 would be read as a date or text by a Turkish Excel.
    int fractions = 0;
    for (const auto& row : doc.cutList()) {
        for (const core::Real size : {row.cut.width, row.finishWidth}) {
            const std::string text = i18n::mm(size);
            if (text.find('.') != std::string::npos) {
                ++fractions;
                std::string comma = text;
                std::replace(comma.begin(), comma.end(), '.', ',');
                CHECK(csv.find(";" + text + ";") == std::string::npos);
                CHECK(csv.find(";" + comma + ";") != std::string::npos);
            }
        }
    }
    CHECK(fractions > 0);
    CHECK(lines(csv).size() == doc.cutList().size() + 1);
}
