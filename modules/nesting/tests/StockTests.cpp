// Stage 7: the stock of sheets and remnants (StockManager, Sheet).

#include "readycad/nesting/Nesting.hpp"
#include "readycad/nesting/Stock.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace rc::nesting;
using rc::core::Material;
using rc::core::MaterialType;

namespace {

const Material kChipboard{"Suntalam 18", MaterialType::Chipboard, 18.0, true};
const Material kHdf{"HDF 3", MaterialType::HDF, 3.0, false};

Sheet sheet(const Material& material, Real length, Real width, int quantity, bool remnant = false)
{
    Sheet s;
    s.material = material;
    s.length = length;
    s.width = width;
    s.quantity = quantity;
    s.remnant = remnant;
    return s;
}

} // namespace

TEST_CASE("Sheets get ids, equal sheets are counted together", "[nesting][stock]")
{
    StockManager stock;
    const SheetId full = stock.add(sheet(kChipboard, 2800, 2100, 5));
    const SheetId same = stock.add(sheet(kChipboard, 2800, 2100, 3));
    const SheetId other = stock.add(sheet(kChipboard, 3660, 1830, 2));
    const SheetId remnant = stock.add(sheet(kChipboard, 2800, 2100, 1, true));

    CHECK(full == same);
    CHECK(full != other);
    CHECK(remnant != full); // a remnant of full size is still a remnant
    REQUIRE(stock.find(full));
    CHECK(stock.find(full)->quantity == 8);
    CHECK(stock.sheets().size() == 3);

    Sheet changed = *stock.find(other);
    changed.quantity = 0;
    stock.update(changed);
    CHECK(stock.find(other)->quantity == 0);
    stock.remove(other);
    CHECK(stock.find(other) == nullptr);

    CHECK_THROWS_AS(stock.add(sheet(kChipboard, 0, 2100, 1)), std::invalid_argument);
    CHECK_THROWS_AS(stock.add(sheet(kChipboard, 2800, 2100, -1)), std::invalid_argument);
    Sheet unknown = sheet(kHdf, 2800, 2070, 1);
    unknown.id = 999;
    CHECK_THROWS_AS(stock.update(unknown), std::invalid_argument);
}

TEST_CASE("A saved stock keeps its ids", "[nesting][stock]")
{
    Sheet a = sheet(kChipboard, 2800, 2100, 4);
    a.id = 7;
    Sheet b = sheet(kHdf, 2800, 2070, 10);
    b.id = 3;
    StockManager stock({a, b});
    CHECK(stock.find(7) != nullptr);
    CHECK(stock.add(sheet(kChipboard, 1000, 600, 1, true)) == 8); // new ids continue after 7

    Sheet twin = b;
    CHECK_THROWS_AS(StockManager({a, b, twin}), std::invalid_argument);
    Sheet zero = a;
    zero.id = 0;
    CHECK_THROWS_AS(StockManager({zero}), std::invalid_argument);
}

TEST_CASE("Offers: remnants first, then stock, then sheets to buy", "[nesting][stock]")
{
    StockManager stock;
    stock.add(sheet(kChipboard, 3660, 1830, 0)); // none left, but tells the size to buy
    stock.add(sheet(kChipboard, 2800, 2100, 2));
    stock.add(sheet(kChipboard, 1200, 700, 1, true));
    stock.add(sheet(kChipboard, 900, 500, 1, true));
    stock.add(sheet(kHdf, 2800, 2070, 6));

    const std::vector<SheetOffer> offers = stock.offers(kChipboard);
    REQUIRE(offers.size() == 4);
    CHECK(offers[0].source == SheetSource::Remnant);
    CHECK(offers[0].sheet.length == 900); // smallest remnant first
    CHECK(offers[1].sheet.length == 1200);
    CHECK(offers[2].source == SheetSource::Stock);
    CHECK(offers[2].available == 2);
    CHECK(offers[3].source == SheetSource::Purchase);
    CHECK(offers[3].available == SheetOffer::kUnlimited);
    CHECK(offers[3].sheet.length == 3660); // the material's first full sheet entry
    CHECK(offers[3].sheet.width == 1830);

    // Nothing on stock: buy the standard size.
    const Material mdf{"MDF Lam 18", MaterialType::MDF, 18.0, false};
    const std::vector<SheetOffer> none = stock.offers(mdf);
    REQUIRE(none.size() == 1);
    CHECK(none.front().source == SheetSource::Purchase);
    CHECK(none.front().sheet.length == 2800);
    CHECK(none.front().sheet.width == 2100);

    // Same name, other thickness: another material.
    const Material thin{"Suntalam 18", MaterialType::Chipboard, 16.0, true};
    CHECK(stock.offers(thin).size() == 1);
}

TEST_CASE("Small offcuts are waste, large ones are remnants", "[nesting][stock]")
{
    const RemnantRules rules; // 300 x 200
    CHECK(rules.isUsable(300, 200));
    CHECK(rules.isUsable(200, 300));
    CHECK(rules.isUsable(2780, 450));
    CHECK_FALSE(rules.isUsable(299, 250));
    CHECK_FALSE(rules.isUsable(2780, 150));
}

TEST_CASE("Cutting takes sheets out of the stock and puts remnants in", "[nesting][stock]")
{
    StockManager stock;
    const SheetId full = stock.add(sheet(kChipboard, 2800, 2100, 3));
    const SheetId old = stock.add(sheet(kChipboard, 1200, 700, 1, true));

    NestingResult result;
    SheetLayout a;
    a.sheet = *stock.find(full);
    a.source = SheetSource::Stock;
    a.count = 2;
    a.remnants = {Rect{{10, 1500}, {2790, 2090}}}; // 2780 x 590 left on each sheet
    SheetLayout b;
    b.sheet = *stock.find(old);
    b.source = SheetSource::Remnant;
    SheetLayout c;
    c.sheet = sheet(kChipboard, 2800, 2100, 0);
    c.source = SheetSource::Purchase;
    c.count = 4;
    result.layouts = {a, b, c};

    stock.consume(result, "Gardırop · 08.10.2026");
    CHECK(stock.find(full)->quantity == 1);
    CHECK(stock.find(old) == nullptr); // the used remnant is gone
    const auto remnants = stock.offers(kChipboard);
    REQUIRE(remnants.front().source == SheetSource::Remnant);
    CHECK(remnants.front().sheet.length == 2780); // along X: the grain stays along the length
    CHECK(remnants.front().sheet.width == 590);
    CHECK(remnants.front().available == 2);
    CHECK(remnants.front().sheet.note == "Gardırop · 08.10.2026");

    SECTION("a plan that needs more than the stock has changes nothing")
    {
        const std::vector<Sheet> before = stock.sheets();
        CHECK_THROWS_AS(stock.consume(result, "again"), std::invalid_argument);
        CHECK(stock.sheets().size() == before.size());
        CHECK(stock.find(full)->quantity == 1);
    }
}
