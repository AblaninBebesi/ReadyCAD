// Stage 7: the optimisation on a background thread, as the UI runs it.

#include "readycad/nesting/NestingRun.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

using namespace rc::nesting;
using rc::core::Material;
using rc::core::MaterialType;

namespace {

std::vector<NestingPart> parts(int quantity)
{
    NestingPart p;
    p.label = "1";
    p.material = Material{"Suntalam 18", MaterialType::Chipboard, 18.0, false};
    p.outline = rectangle(Rect{{0, 0}, {760, 520}});
    p.quantity = quantity;
    NestingPart q = p;
    q.label = "2";
    q.outline = rectangle(Rect{{0, 0}, {1990, 560}});
    q.quantity = 2;
    return {p, q};
}

} // namespace

TEST_CASE("A background run reports progress and ends with the full result", "[nesting][run]")
{
    NestingSettings s;
    s.optimizer.generations = 5;
    s.optimizer.populationSize = 8;
    NestingRun run(parts(6), StockManager{}, s);
    run.wait();
    REQUIRE(run.finished());
    CHECK(run.error().empty());
    CHECK(run.version() > 0);
    const auto result = run.result();
    REQUIRE(result);
    CHECK(result->placedCount() == 8);
    CHECK(run.progress().generation == 5);
}

TEST_CASE("Cancelling stops soon with a complete layout", "[nesting][run]")
{
    NestingSettings s;
    s.machine = Machine::NestingCenter;
    s.optimizer.generations = 100000;
    s.optimizer.populationSize = 30;
    NestingRun run(parts(40), StockManager{}, s);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto start = std::chrono::steady_clock::now();
    run.cancel();
    run.wait();
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(30));
    REQUIRE(run.finished());
    const auto result = run.result();
    REQUIRE(result);
    CHECK(result->placedCount() == 42);
}

TEST_CASE("Destroying a running job cancels it", "[nesting][run]")
{
    NestingSettings s;
    s.optimizer.generations = 100000;
    {
        NestingRun run(parts(20), StockManager{}, s);
    } // must not hang
    SUCCEED();
}

TEST_CASE("A failing run reports its error", "[nesting][run]")
{
    NestingSettings s;
    s.saw.kerf = -1.0;
    NestingRun run(parts(1), StockManager{}, s);
    run.wait();
    CHECK(run.finished());
    CHECK_FALSE(run.error().empty());
    CHECK_FALSE(run.result());
}
