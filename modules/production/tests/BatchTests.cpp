// Stage 9: the command line without a window — a project in, a production package out, the
// workshop's stock only read.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/production/Batch.hpp"
#include "readycad/storage/Storage.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>

using namespace rc;
using namespace rc::production;
using Catch::Matchers::ContainsSubstring;

namespace {

/// A workshop folder with a few sheets on stock and a quick optimiser.
storage::WorkshopFiles workshop(const std::filesystem::path& folder, nesting::Machine machine)
{
    const storage::WorkshopFiles files = storage::WorkshopFiles::in(folder);
    nesting::StockManager stock;
    stock.add(nesting::Sheet{0, core::ConstructionRules{}.carcassMaterial, 2800, 2100, 4, false, {}});
    storage::saveStockFile(stock, files.stock);
    storage::AppSettings settings;
    settings.nesting.machine = machine;
    settings.nesting.optimizer.populationSize = 6;
    settings.nesting.optimizer.generations = 3;
    storage::saveSettingsFile(settings, files.settings);
    return files;
}

} // namespace

TEST_CASE("The command line writes a production package without a window", "[production][batch]")
{
    const rc::test::TempFolder temp("batch");
    const storage::WorkshopFiles files =
        workshop(temp.path() / "atolye", nesting::Machine::NestingCenter);
    const std::string stockBefore = rc::test::readFile(files.stock);

    BatchOptions options;
    options.catalogItem = 1; // wardrobe with drawers
    options.quantity = 2;
    options.dataFolder = temp.path() / "atolye";
    options.package = temp.path() / "paket";
    std::ostringstream out;
    std::ostringstream err;
    REQUIRE(runBatch(options, out, err) == 0);
    CHECK(err.str().empty());

    CHECK_THAT(out.str(), ContainsSubstring(std::string(i18n::text(i18n::TextId::PackageTitle))));
    CHECK(std::filesystem::exists(options.package / "OZET.txt"));
    CHECK(std::filesystem::is_directory(options.package / "CNC")); // the machine of the settings
    CHECK(std::filesystem::is_directory(options.package / "KESIM"));
    // Named after the catalogue model, since there is no project file.
    CHECK(std::filesystem::exists(options.package / "Cekmeceli_gardirop.pdf"));
    // The stock is only read: taking sheets out stays the user's decision.
    CHECK(rc::test::readFile(files.stock) == stockBefore);

    // The machine on the command line wins over the settings file.
    options.machine = nesting::Machine::PanelSaw;
    options.package = temp.path() / "paket_testere";
    REQUIRE(runBatch(options, out, err) == 0);
    CHECK_FALSE(std::filesystem::exists(options.package / "CNC"));
    CHECK(std::filesystem::is_directory(options.package / "KESIM"));
}

TEST_CASE("A saved project goes through the command line", "[production][batch]")
{
    const rc::test::TempFolder temp("batch_project");
    workshop(temp.path() / "atolye", nesting::Machine::PanelSaw);
    const auto project = temp.path() / "Müşteri Yılmaz.rcproj";
    storage::writeTextFile(project,
                           storage::saveProduct(*core::createProduct(core::CatalogItem::Bookcase)));

    BatchOptions options;
    options.project = project;
    options.dataFolder = temp.path() / "atolye";
    options.package = temp.path() / "paket";
    std::ostringstream out;
    std::ostringstream err;
    REQUIRE(runBatch(options, out, err) == 0);
    CHECK(std::filesystem::exists(options.package / "Musteri_Yilmaz.pdf"));
}

TEST_CASE("A request the command line cannot fulfil ends with a reason and code 1",
          "[production][batch]")
{
    const rc::test::TempFolder temp("batch_errors");
    workshop(temp.path() / "atolye", nesting::Machine::PanelSaw);
    BatchOptions good;
    good.dataFolder = temp.path() / "atolye";
    good.package = temp.path() / "paket";

    const auto fails = [](const BatchOptions& options, const std::string& reason) {
        std::ostringstream out;
        std::ostringstream err;
        CHECK(runBatch(options, out, err) == 1);
        CHECK_THAT(err.str(), ContainsSubstring(reason));
    };
    BatchOptions noModel = good;
    noModel.catalogItem = 99;
    fails(noModel, "catalogue model 99");

    BatchOptions noFile = good;
    noFile.project = temp.path() / "yok.rcproj";
    fails(noFile, "yok.rcproj");

    BatchOptions noQuantity = good;
    noQuantity.quantity = 0;
    fails(noQuantity, "quantity");

    BatchOptions noFolder = good;
    noFolder.package.clear();
    fails(noFolder, "folder");

    std::filesystem::create_directories(good.package);
    storage::writeTextFile(good.package / "eski.txt", "");
    fails(good, "not empty");
}
