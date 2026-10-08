// Stage 7: panel saw layouts — guillotine cuts in stages, kerf, trims, grain, stacks, remnants.

#include "LayoutCheck.hpp"
#include "readycad/nesting/Nesting.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <random>

using namespace rc::nesting;
using Catch::Approx;
using rc::core::Material;
using rc::core::MaterialType;

namespace {

const Material kChipboard{"Suntalam 18", MaterialType::Chipboard, 18.0, true};

NestingPart part(const std::string& label, Real length, Real width, int quantity,
                 bool keepGrain = false)
{
    NestingPart p;
    p.label = label;
    p.material = kChipboard;
    p.outline = rectangle(Rect{{0, 0}, {length, width}});
    p.quantity = quantity;
    p.keepGrain = keepGrain;
    return p;
}

NestingSettings sawSettings(int generations = 10)
{
    NestingSettings s;
    s.machine = Machine::PanelSaw;
    s.saw.kerf = 4.0;
    s.saw.trim = 10.0;
    s.optimizer.generations = generations;
    s.optimizer.populationSize = 12;
    s.optimizer.threads = 2;
    return s;
}

std::vector<Rect> placedRects(const NestingResult& result, const SheetLayout& layout)
{
    std::vector<Rect> rects;
    for (const PlacedPart& p : layout.parts) {
        rects.push_back(bounds(test::placedOutline(result, p)));
    }
    return rects;
}

int placedPieces(const NestingResult& result)
{
    return result.placedCount();
}

} // namespace

TEST_CASE("Two parts on one sheet: strip, columns, cuts and remnants", "[nesting][saw]")
{
    const NestingResult r =
        optimize({part("1", 1000, 500, 2)}, StockManager{}, sawSettings(0));
    REQUIRE(r.layouts.size() == 1);
    const SheetLayout& sheet = r.layouts.front();
    CHECK(sheet.source == SheetSource::Purchase);
    CHECK(sheet.usable == Rect{{10, 10}, {2790, 2090}});
    REQUIRE(sheet.parts.size() == 2);

    // One strip 500 high along the sheet; the second part right of the first, one kerf away.
    const std::vector<Rect> rects = placedRects(r, sheet);
    CHECK(rects[0] == Rect{{10, 10}, {1010, 510}});
    CHECK(rects[1] == Rect{{1014, 10}, {2014, 510}});

    int trims = 0;
    int strips = 0;
    int pieces = 0;
    for (const SawCut& cut : sheet.cuts) {
        trims += cut.stage == 0;
        strips += cut.stage == 1;
        pieces += cut.stage == 2;
    }
    CHECK(trims == 4);
    CHECK(strips == 1); // y = 510
    CHECK(pieces == 2); // x = 1010, x = 2014
    CHECK(sheet.cuts.front().stage == 0);

    // Above the strip: 2780 x 1576; at its end: 772 x 500.
    REQUIRE(sheet.remnants.size() == 2);
    CHECK(sheet.remnants[0] == Rect{{2018, 10}, {2790, 510}});
    CHECK(sheet.remnants[1] == Rect{{10, 514}, {2790, 2090}});
    CHECK(sheet.utilization() == Approx(2 * 1000.0 * 500.0 / (2800.0 * 2100.0)));
}

TEST_CASE("Saw layouts can be cut with guillotine cuts in the allowed stages", "[nesting][saw]")
{
    std::mt19937 random(7);
    std::uniform_int_distribution<int> length(150, 1400);
    std::uniform_int_distribution<int> width(100, 700);
    for (const int stages : {2, 3, 4}) {
        for (const CutDirection direction : {CutDirection::AlongLength, CutDirection::AlongWidth}) {
            std::vector<NestingPart> parts;
            for (int i = 0; i < 14; ++i) {
                parts.push_back(part(std::to_string(i + 1), length(random), width(random),
                                     1 + i % 3, i % 4 == 0));
            }
            NestingSettings settings = sawSettings(6);
            settings.saw.maxStages = stages;
            settings.saw.firstCuts = direction;
            const NestingResult r = optimize(parts, StockManager{}, settings);

            INFO("stages " << stages << ", along length " << (direction == CutDirection::AlongLength));
            int quantity = 0;
            for (const NestingPart& p : parts) {
                quantity += p.quantity;
            }
            CHECK(r.unplaced.empty());
            CHECK(placedPieces(r) == quantity);
            for (const SheetLayout& sheet : r.layouts) {
                CHECK(test::checkSheet(r, sheet, sheet.usable, settings.saw.kerf).empty());
                const int needed = test::guillotineStages(
                    placedRects(r, sheet), settings.saw.kerf,
                    direction == CutDirection::AlongLength);
                CHECK(needed >= 0);
                CHECK(needed <= stages);
                for (const SawCut& cut : sheet.cuts) {
                    CHECK(cut.stage <= stages);
                }
                for (const PlacedPart& p : sheet.parts) {
                    if (r.parts[p.part].keepGrain) {
                        CHECK(p.transform.quarterTurns == 0); // length along the sheet's grain
                    }
                }
            }
        }
    }
}

TEST_CASE("Identical sheets are one layout cut as a stack", "[nesting][saw]")
{
    // Four 1380 x 1030 parts fill a sheet: 2 x 1380 + 4 = 2764 <= 2780, 2 x 1030 + 4 <= 2080.
    const NestingResult r =
        optimize({part("1", 1380, 1030, 12, true)}, StockManager{}, sawSettings(0));
    REQUIRE(r.layouts.size() == 1);
    CHECK(r.layouts.front().count == 3);
    CHECK(r.layouts.front().parts.size() == 4);
    CHECK(r.sheetCount() == 3);
    CHECK(sheetsPerStack(PanelSawSettings{}, 18.0) == 3); // 60 mm / 18 mm
    CHECK(sheetsPerStack(PanelSawSettings{}, 3.0) == 20);
}

TEST_CASE("Remnants on stock are used first and need no trim", "[nesting][saw]")
{
    StockManager stock;
    Sheet remnant;
    remnant.material = kChipboard;
    remnant.length = 1100;
    remnant.width = 600;
    remnant.remnant = true;
    const SheetId id = stock.add(remnant);
    stock.add(Sheet{0, kChipboard, 2800, 2100, 5, false, {}});

    const NestingResult r = optimize({part("1", 1100, 600, 1), part("2", 900, 400, 1)}, stock,
                                     sawSettings(4));
    REQUIRE(r.layouts.size() == 2);
    const auto onRemnant = std::find_if(r.layouts.begin(), r.layouts.end(), [&](const auto& l) {
        return l.source == SheetSource::Remnant;
    });
    REQUIRE(onRemnant != r.layouts.end());
    CHECK(onRemnant->sheet.id == id);
    CHECK(onRemnant->usable == Rect{{0, 0}, {1100, 600}}); // cut clean before: no trims
    CHECK(onRemnant->parts.size() == 1);
    const auto usage = r.usage();
    REQUIRE(usage.size() == 1);
    CHECK(usage.front().remnants == 1);
    CHECK(usage.front().stock == 1);
    CHECK(usage.front().purchase == 0);
}

TEST_CASE("Automatic cut direction is never worse than a fixed one", "[nesting][saw]")
{
    std::vector<NestingPart> parts{part("1", 2050, 640, 3), part("2", 900, 1300, 2),
                                   part("3", 450, 330, 7)};
    // Without evolution all three see the same individuals: Auto takes the better direction
    // of each one, so its best is at least as good.
    int sheets[3] = {};
    int i = 0;
    for (const CutDirection d :
         {CutDirection::AlongLength, CutDirection::AlongWidth, CutDirection::Auto}) {
        NestingSettings s = sawSettings(0);
        s.saw.firstCuts = d;
        sheets[i++] = optimize(parts, StockManager{}, s).sheetCount();
    }
    CHECK(sheets[2] <= std::min(sheets[0], sheets[1]));
}

TEST_CASE("Parts larger than every sheet are reported, the rest is laid out", "[nesting][saw]")
{
    const NestingResult r = optimize({part("big", 3000, 2200, 1), part("ok", 500, 400, 2)},
                                     StockManager{}, sawSettings(0));
    REQUIRE(r.unplaced.size() == 1);
    CHECK(r.parts[r.unplaced.front().part].label == "big");
    CHECK(r.unplaced.front().quantity == 1);
    CHECK(r.placedCount() == 2);
}
