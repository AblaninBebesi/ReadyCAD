// Stage 7: the genetic algorithm — reproducible, independent of the thread count, never worse
// than its first generation, stoppable.

#include "LayoutCheck.hpp"
#include "readycad/nesting/Nesting.hpp"

#include <catch2/catch_test_macros.hpp>

#include <random>
#include <stop_token>

using namespace rc::nesting;
using rc::core::Material;
using rc::core::MaterialType;

namespace {

const Material kChipboard{"Suntalam 16", MaterialType::Chipboard, 16.0, false};

std::vector<NestingPart> mixedParts()
{
    std::mt19937 random(11);
    std::uniform_int_distribution<int> size(100, 900);
    std::vector<NestingPart> parts;
    for (int i = 0; i < 12; ++i) {
        NestingPart p;
        p.label = std::to_string(i + 1);
        p.material = kChipboard;
        p.outline = i % 3 == 0
                        ? Polygon{{0, 0}, {Real(size(random)), 0}, {120, Real(size(random))}}
                        : rectangle(Rect{{0, 0}, {Real(size(random)), Real(size(random))}});
        p.quantity = 1 + i % 3;
        parts.push_back(std::move(p));
    }
    return parts;
}

NestingSettings settingsFor(Machine machine, int threads)
{
    NestingSettings s;
    s.machine = machine;
    s.optimizer.populationSize = 10;
    s.optimizer.generations = 5;
    s.optimizer.threads = threads;
    s.optimizer.seed = 42;
    return s;
}

/// A layout reduced to what matters: sheet, count and every placement.
std::string fingerprint(const NestingResult& r)
{
    std::string out;
    for (const SheetLayout& sheet : r.layouts) {
        out += std::to_string(sheet.sheet.length) + "x" + std::to_string(sheet.count) + ":";
        for (const PlacedPart& p : sheet.parts) {
            out += std::to_string(p.part) + "/" + std::to_string(p.transform.quarterTurns) + "@"
                 + std::to_string(std::llround(p.transform.offset.x * 1000)) + ","
                 + std::to_string(std::llround(p.transform.offset.y * 1000)) + " ";
        }
        out += "\n";
    }
    return out;
}

} // namespace

TEST_CASE("Same seed gives the same layout on any number of threads", "[nesting][genetic]")
{
    for (const Machine machine : {Machine::PanelSaw, Machine::NestingCenter}) {
        const std::vector<NestingPart> parts = mixedParts();
        const NestingResult one = optimize(parts, StockManager{}, settingsFor(machine, 1));
        const NestingResult four = optimize(parts, StockManager{}, settingsFor(machine, 4));
        const NestingResult again = optimize(parts, StockManager{}, settingsFor(machine, 4));
        CHECK(fingerprint(one) == fingerprint(four));
        CHECK(fingerprint(four) == fingerprint(again));
        CHECK(one.evaluations > 0);

        // Without the NFP cache the same NFPs are computed again: same result, more work.
        NestingSettings uncached = settingsFor(machine, 2);
        uncached.optimizer.memoryLimitMb = 0;
        CHECK(fingerprint(optimize(parts, StockManager{}, uncached)) == fingerprint(one));
    }
}

TEST_CASE("The best layout never gets worse from one generation to the next",
          "[nesting][genetic]")
{
    for (const Machine machine : {Machine::PanelSaw, Machine::NestingCenter}) {
        NestingSettings s = settingsFor(machine, 2);
        s.optimizer.generations = 8;
        s.optimizer.elites = 0; // even without elitism the best ever is kept
        std::vector<Real> utilization;
        std::vector<int> generations;
        const NestingResult r = optimize(
            mixedParts(), StockManager{}, s,
            [&](const NestingProgress& p, const NestingResult& best) {
                utilization.push_back(best.utilization());
                generations.push_back(p.generation);
                CHECK(p.utilization == best.utilization());
            });
        REQUIRE(generations.size() == 9); // the initial population and 8 generations
        CHECK(generations.front() == 0);
        CHECK(generations.back() == 8);
        for (std::size_t i = 1; i < utilization.size(); ++i) {
            CHECK(utilization[i] >= utilization[i - 1] - 1e-12);
        }
        CHECK(r.utilization() >= utilization.front() - 1e-12);
    }
}

TEST_CASE("A stopped optimisation still lays out every part", "[nesting][genetic]")
{
    std::stop_source stop;
    stop.request_stop();
    for (const Machine machine : {Machine::PanelSaw, Machine::NestingCenter}) {
        NestingSettings s = settingsFor(machine, 2);
        s.optimizer.generations = 1000; // would take long
        const std::vector<NestingPart> parts = mixedParts();
        const NestingResult r = optimize(parts, StockManager{}, s, {}, stop.get_token());
        int quantity = 0;
        for (const NestingPart& p : parts) {
            quantity += p.quantity;
        }
        CHECK(r.placedCount() == quantity);
        CHECK(r.evaluations <= s.optimizer.populationSize);
    }
}

TEST_CASE("Invalid settings and parts are rejected", "[nesting][genetic]")
{
    const std::vector<NestingPart> parts = mixedParts();
    NestingSettings s;
    s.optimizer.populationSize = 1;
    CHECK_THROWS_AS(optimize(parts, StockManager{}, s), std::invalid_argument);
    s = {};
    s.saw.maxStages = 5;
    CHECK_THROWS_AS(optimize(parts, StockManager{}, s), std::invalid_argument);
    s = {};
    s.router.toolDiameter = 0.0;
    CHECK_THROWS_AS(optimize(parts, StockManager{}, s), std::invalid_argument);

    std::vector<NestingPart> flat = parts;
    flat.front().outline = {{0, 0}, {10, 0}, {20, 0}};
    CHECK_THROWS_AS(optimize(flat, StockManager{}, NestingSettings{}), std::invalid_argument);
}
