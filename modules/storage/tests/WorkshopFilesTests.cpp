// Stage 7: files of the workshop — the stock of sheets and remnants, the cutting settings —
// and the safe (write, then rename) saving of every file.

#include "readycad/storage/Storage.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>

using namespace rc::storage;
using namespace rc::nesting;
using rc::core::Material;
using rc::core::MaterialType;

TEST_CASE("The stock survives save and load", "[storage][stock]")
{
    StockManager stock;
    stock.standardLength = 3660;
    stock.standardWidth = 1830;
    stock.remnantRules.minLength = 400;
    stock.remnantRules.minWidth = 150;
    const SheetId full = stock.add(
        Sheet{0, Material{"Suntalam 18", MaterialType::Chipboard, 18.0, true}, 2800, 2100, 12,
              false, {}});
    const SheetId remnant = stock.add(
        Sheet{0, Material{"MDF Lam 18", MaterialType::MDF, 18.0, false}, 1430.5, 610, 1, true,
              "Şifonyer · 08.10.2026"});

    const StockManager loaded = loadStock(saveStock(stock));
    REQUIRE(loaded.sheets().size() == 2);
    CHECK(loaded.standardLength == 3660);
    CHECK(loaded.standardWidth == 1830);
    CHECK(loaded.remnantRules.minLength == 400);
    CHECK(loaded.remnantRules.minWidth == 150);
    const Sheet* a = loaded.find(full);
    const Sheet* b = loaded.find(remnant);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(a->quantity == 12);
    CHECK(a->material.hasGrain);
    CHECK(a->material.type == MaterialType::Chipboard);
    CHECK(b->length == 1430.5);
    CHECK(b->remnant);
    CHECK(b->note == "Şifonyer · 08.10.2026");

    // New sheets get ids after the loaded ones.
    StockManager next = loaded;
    CHECK(next.add(Sheet{0, a->material, 1000, 500, 1, true, "x"}) > std::max(full, remnant));
}

TEST_CASE("Broken stock files are rejected", "[storage][stock]")
{
    CHECK_THROWS_AS(loadStock("{"), StorageError);
    CHECK_THROWS_AS(loadStock(R"({"format": "readycad-profile", "version": 1})"), StorageError);
    CHECK_THROWS_AS(loadStock(R"({"format": "readycad-stock", "version": 99})"), StorageError);
    CHECK_THROWS_WITH(
        loadStock(R"({"format": "readycad-stock", "version": 1,
                      "sheets": [{"id": 1, "length": 100, "width": 100},
                                 {"id": 1, "length": 200, "width": 100}]})"),
        Catch::Matchers::ContainsSubstring("Inconsistent stock file"));
    // An empty stock is fine: the defaults.
    const StockManager empty = loadStock(R"({"format": "readycad-stock", "version": 1})");
    CHECK(empty.sheets().empty());
    CHECK(empty.standardLength == 2800);
}

TEST_CASE("Cutting settings survive save and load", "[storage][settings]")
{
    AppSettings settings;
    NestingSettings& s = settings.nesting;
    s.machine = Machine::NestingCenter;
    s.useRemnants = false;
    s.saw.kerf = 3.2;
    s.saw.maxStages = 4;
    s.saw.firstCuts = CutDirection::Auto;
    s.router.toolDiameter = 10.0;
    s.router.extraSpacing = 1.5;
    s.optimizer.populationSize = 50;
    s.optimizer.threads = 6;
    s.optimizer.memoryLimitMb = 1024;
    s.optimizer.seed = 123456789012345ULL;

    const NestingSettings l = loadSettings(saveSettings(settings)).nesting;
    CHECK(l.machine == Machine::NestingCenter);
    CHECK_FALSE(l.useRemnants);
    CHECK(l.saw.kerf == 3.2);
    CHECK(l.saw.maxStages == 4);
    CHECK(l.saw.firstCuts == CutDirection::Auto);
    CHECK(l.router.toolDiameter == 10.0);
    CHECK(l.router.extraSpacing == 1.5);
    CHECK(l.optimizer.populationSize == 50);
    CHECK(l.optimizer.threads == 6);
    CHECK(l.optimizer.memoryLimitMb == 1024);
    CHECK(l.optimizer.seed == 123456789012345ULL);

    // Older files without a field keep the default; impossible values are refused.
    const AppSettings partial = loadSettings(
        R"({"format": "readycad-settings", "version": 1, "nesting": {"saw": {"kerf": 4.0}}})");
    CHECK(partial.nesting.saw.kerf == 4.0);
    CHECK(partial.nesting.saw.trim == NestingSettings{}.saw.trim);
    CHECK(partial.nesting.useRemnants); // a file from before the choice: remnants are used
    CHECK(partial.cnc == rc::cam::defaultProfile()); // a stage 7 file: the default machine
    CHECK_THROWS_AS(loadSettings(R"({"format": "readycad-settings", "version": 1,
                                     "nesting": {"saw": {"maxStages": 7}}})"),
                    StorageError);
}

TEST_CASE("The languages survive save and load", "[storage][settings]")
{
    AppSettings settings;
    CHECK(settings.language == "tr");
    CHECK(settings.documentLanguage.empty()); // the documents follow the program
    settings.language = "ru";
    settings.documentLanguage = "en";
    const AppSettings loaded = loadSettings(saveSettings(settings));
    CHECK(loaded.language == "ru");
    CHECK(loaded.documentLanguage == "en");

    // A language this program does not know falls back instead of failing the whole file.
    const AppSettings unknown = loadSettings(R"({"format": "readycad-settings", "version": 1,
                                               "language": "de", "documentLanguage": "fr"})");
    CHECK(unknown.language == "tr");
    CHECK(unknown.documentLanguage.empty());
    const AppSettings older = loadSettings(R"({"format": "readycad-settings", "version": 1})");
    CHECK(older.language == "tr");
}

TEST_CASE("The CNC nesting centre survives save and load", "[storage][settings]")
{
    AppSettings settings;
    rc::cam::NestingCenterProfile& cnc = settings.cnc;
    cnc.dialect = rc::cam::kdtDialect();
    cnc.dialect.zero = rc::cam::ZeroPlane::Top;
    cnc.dialect.header = {"G90 G54", "(KDT {name})"};
    cnc.tools.push_back({16, rc::cam::ToolKind::Drill, 10.0, 30.0, 3500, 0.0, 2000.0, 0.0, "Matkap D10"});
    cnc.tools.front().feed = 15000.0;
    cnc.spoilboardCut = 0.5;
    cnc.climbMilling = false;

    const AppSettings loaded = loadSettings(saveSettings(settings));
    CHECK(loaded.cnc == cnc);
    CHECK(loaded.cnc.dialect.name == "KDT (NcStudio)");
    CHECK(loaded.cnc.tools.back().name == "Matkap D10");

    // A tool table without a router for the contours cannot make programs.
    CHECK_THROWS_AS(loadSettings(R"({"format": "readycad-settings", "version": 1,
                                     "cnc": {"contourTool": 11}})"),
                    StorageError);
}

TEST_CASE("Files are replaced in one step", "[storage][file]")
{
    const rc::test::TempFolder folder("atomic_write");
    const auto file = folder.path() / "depo.rcstock";
    writeTextFile(file, "first version, longer than the second");
    writeTextFile(file, "second");
    CHECK(readTextFile(file) == "second");
    CHECK_FALSE(std::filesystem::exists(folder.path() / "depo.rcstock.tmp"));
    CHECK_THROWS_AS(writeTextFile(folder.path() / "missing" / "x.json", "{}"), StorageError);
}

TEST_CASE("A workshop folder without files gives an empty stock and default settings",
          "[storage][file]")
{
    const rc::test::TempFolder folder("workshop_files");
    const WorkshopFiles files = WorkshopFiles::in(folder.path() / "atolye");
    CHECK(files.stock.filename() == "stock.rcstock");
    CHECK(files.settings.filename() == "settings.json");
    CHECK(loadStockFile(files.stock).sheets().empty());
    CHECK(loadSettingsFile(files.settings).nesting == NestingSettings{});

    // Saving creates the folder; loading gives back what was saved.
    StockManager stock;
    stock.add(Sheet{0, Material{"Suntalam 18", MaterialType::Chipboard, 18.0, true}, 2800, 2100,
                    5, false, {}});
    AppSettings settings;
    settings.nesting.machine = Machine::NestingCenter;
    saveStockFile(stock, files.stock);
    saveSettingsFile(settings, files.settings);
    CHECK(loadStockFile(files.stock).sheets().size() == 1);
    CHECK(loadSettingsFile(files.settings).nesting.machine == Machine::NestingCenter);

    // The default folder is the user's.
    CHECK(WorkshopFiles::in().stock.parent_path() == userDataFolder());

    // A broken file is an error, not an empty stock: the stock must never be lost silently.
    writeTextFile(files.stock, "{ not json");
    CHECK_THROWS_AS(loadStockFile(files.stock), StorageError);
}

TEST_CASE("User files live in a ReadyCAD folder", "[storage][file]")
{
    const std::filesystem::path folder = userDataFolder();
    CHECK(folder.filename() == "ReadyCAD");
    CHECK(folder.is_absolute());
}
