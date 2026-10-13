// Stage 8: planning a sheet on the nesting centre — tools, order, depths, toolpaths.

#include "TestJob.hpp"
#include "readycad/cam/NestingCenter.hpp"
#include "readycad/cam/Route.hpp"
#include "readycad/detailing/FlatPart.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <glm/geometric.hpp>

#include <algorithm>
#include <random>

using namespace rc;
using namespace rc::cam;
using Catch::Approx;

namespace {

std::vector<int> toolOrder(const Program& p)
{
    std::vector<int> tools;
    for (const Step& s : p.steps) {
        if (const auto* t = std::get_if<ToolChange>(&s)) {
            tools.push_back(t->tool.number);
        }
    }
    return tools;
}

std::vector<DrillCycle> drills(const Program& p)
{
    std::vector<DrillCycle> out;
    for (const Step& s : p.steps) {
        if (const auto* d = std::get_if<DrillCycle>(&s)) {
            out.push_back(*d);
        }
    }
    return out;
}

/// Cutting moves made with `tool`, as (from, to).
std::vector<std::pair<Vec3, Vec3>> cuts(const Program& p, int tool)
{
    std::vector<std::pair<Vec3, Vec3>> out;
    int current = 0;
    Vec3 at(0.0);
    for (const Step& s : p.steps) {
        if (const auto* t = std::get_if<ToolChange>(&s)) {
            current = t->tool.number;
        } else if (const auto* r = std::get_if<Rapid>(&s)) {
            at = r->to;
        } else if (const auto* f = std::get_if<Feed>(&s)) {
            if (current == tool) {
                out.emplace_back(at, f->to);
            }
            at = f->to;
        } else if (const auto* d = std::get_if<DrillCycle>(&s)) {
            at = Vec3(d->at.x, d->at.y, d->clearance);
        }
    }
    return out;
}

} // namespace

TEST_CASE("Drills first, then grooves, then contours", "[cam][planner]")
{
    const SheetProgram sp = planSheet(sample::testJob(), defaultProfile());
    // T11 Ø5, T12 Ø7, T15 Ø35 of the drilling head, T2 for the groove, T1 cuts the parts out.
    CHECK(toolOrder(sp.program) == std::vector<int>{11, 12, 15, 2, 1});
    CHECK(sp.stats.toolChanges == 5);
    CHECK(sp.stats.holes == 4);
    CHECK(sp.milledHoles == 0);

    const auto holes = drills(sp.program);
    REQUIRE(holes.size() == 4);
    CHECK(holes[0].at == Vec2(100, 100)); // the nearest to the start comes first
    CHECK(holes[0].bottom == Approx(18.0 - 13.0));
    CHECK(holes[0].clearance == Approx(18.0 + 3.0));
    CHECK(holes[2].at == Vec2(300, 200));
    CHECK(holes[2].bottom == Approx(-0.3)); // through: into the spoilboard
    CHECK(holes[3].bottom == Approx(5.0));  // the hinge cup

    // What the machine cannot do from above stays for another machine.
    REQUIRE(sp.leftovers.size() == 1);
    CHECK(sp.leftovers[0].label == "1");
    CHECK(sp.leftovers[0].edgeHoles == 1);
    CHECK(sp.leftovers[0].bottomHoles == 1);
    CHECK(sp.leftovers[0].total() == 2);
}

TEST_CASE("Contours: around the part at the tool radius, through, small parts first",
          "[cam][planner]")
{
    NestingCenterProfile profile = defaultProfile();
    for (const bool climb : {true, false}) {
        profile.climbMilling = climb;
        const SheetProgram sp = planSheet(sample::testJob(), profile);
        const auto moves = cuts(sp.program, 1);
        REQUIRE_FALSE(moves.empty());
        // The small part (2) is cut first.
        CHECK(moves.front().second.x >= 540.0 - 6.0 - 1e-6);

        // The path around part 1 at full depth: the outline grown by 6 mm (Ø12).
        std::vector<Vec2> loop;
        for (const auto& [from, to] : moves) {
            if (to.z == Approx(-0.3) && from.z == Approx(-0.3) && to.x < 530.0) {
                loop.push_back({to.x, to.y});
            }
        }
        REQUIRE(loop.size() >= 4);
        for (const Vec2& p : loop) {
            const bool onX = std::abs(p.x - 14.0) < 1e-6 || std::abs(p.x - 526.0) < 1e-6;
            const bool onY = std::abs(p.y - 14.0) < 1e-6 || std::abs(p.y - 326.0) < 1e-6;
            CHECK((onX || onY));
        }
        // Climb milling goes clockwise around an outside contour.
        Real twice = 0.0;
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const Vec2 a = loop[i];
            const Vec2 b = loop[(i + 1) % loop.size()];
            twice += a.x * b.y - b.x * a.y;
        }
        CHECK((climb ? twice < 0.0 : twice > 0.0));
    }
}

TEST_CASE("Deep cuts are made in passes", "[cam][planner]")
{
    NestingCenterProfile profile = defaultProfile();
    profile.tools.front().stepDown = 10.0; // 18.3 mm in two passes of 9.15
    const SheetProgram sp = planSheet(sample::testJob(), profile);
    std::vector<Real> depths;
    for (const auto& [from, to] : cuts(sp.program, 1)) {
        if (from.x == to.x && from.y == to.y && to.z < from.z) { // plunges
            depths.push_back(to.z);
        }
    }
    REQUIRE(depths.size() == 4); // 2 parts x 2 passes
    CHECK(depths[0] == Approx(8.85));
    CHECK(depths[1] == Approx(-0.3));
}

TEST_CASE("A hole without its drill is milled by a smaller router", "[cam][planner]")
{
    NestingCenterProfile profile = defaultProfile();
    std::erase_if(profile.tools, [](const Tool& t) { return t.number == 15; }); // no Ø35 drill
    const SheetProgram sp = planSheet(sample::testJob(), profile);
    CHECK(sp.milledHoles == 1);
    CHECK(sp.stats.holes == 3);
    // The largest router narrower than the cup (T1, Ø12) mills it, staying inside it.
    bool inside = true;
    bool reachedBottom = false;
    for (const auto& [from, to] : cuts(sp.program, 1)) {
        if (glm::distance(Vec2(to.x, to.y), Vec2(400, 150)) < 20.0) {
            inside = inside && glm::distance(Vec2(to.x, to.y), Vec2(400, 150)) <= 17.5 - 6.0 + 1e-6;
            reachedBottom = reachedBottom || to.z == Approx(5.0);
        }
    }
    CHECK(inside);
    CHECK(reachedBottom);
}

TEST_CASE("Operations no tool can make are reported", "[cam][planner]")
{
    SheetJob job = sample::testJob();
    // No Ø3 drill, and every router is wider than the hole.
    job.parts[0].operations.push_back(Drilling{Face::Top, {150, 250, 18}, 3.0, 12.0});
    const SheetProgram sp = planSheet(job, defaultProfile());
    REQUIRE(sp.leftovers.size() == 1);
    CHECK(sp.leftovers[0].noTool == 1);
}

TEST_CASE("Machines that cannot do the sheet are refused", "[cam][planner]")
{
    NestingCenterProfile profile = defaultProfile();
    profile.tools.front().diameter = 14.0; // wider than the 12 mm the nesting left free
    CHECK_THROWS_WITH(planSheet(sample::testJob(), profile),
                      Catch::Matchers::ContainsSubstring("cut into the neighbours"));
    profile = defaultProfile();
    profile.tools.front().cuttingLength = 15.0;
    CHECK_THROWS_AS(planSheet(sample::testJob(), profile), CamError);
    profile = defaultProfile();
    profile.contourTool = 99;
    CHECK_THROWS_AS(planSheet(sample::testJob(), profile), CamError);
}

TEST_CASE("Holes are drilled along a short route", "[cam][planner]")
{
    SheetJob job = sample::testJob();
    std::mt19937 random(3);
    std::uniform_real_distribution<Real> x(40.0, 500.0);
    std::uniform_real_distribution<Real> y(40.0, 300.0);
    std::vector<Vec2> given;
    for (int i = 0; i < 40; ++i) {
        given.push_back({x(random), y(random)});
        job.parts[0].operations.push_back(Drilling{Face::Top, {given.back().x, given.back().y, 18}, 8.0, 12.0});
    }
    const SheetProgram sp = planSheet(job, defaultProfile());
    std::vector<Vec2> drilled;
    for (const DrillCycle& d : drills(sp.program)) {
        if (std::find(given.begin(), given.end(), d.at) != given.end()) {
            drilled.push_back(d.at);
        }
    }
    REQUIRE(drilled.size() == given.size());
    std::vector<std::size_t> asGiven(given.size());
    std::vector<std::size_t> asDrilled(drilled.size());
    for (std::size_t i = 0; i < given.size(); ++i) {
        asGiven[i] = i;
        asDrilled[i] = i;
    }
    CHECK(routeLength(drilled, asDrilled, Vec2(0.0)) < 0.5 * routeLength(given, asGiven, Vec2(0.0)));
    CHECK(sp.stats.seconds > 0.0);
    CHECK(sp.stats.cutLength > 0.0);
}

namespace {

/// A worktop 2400 x 600 x 38 with a sink opening, and a small part beside it.
SheetJob worktopJob(Real openingLength = 560.0)
{
    SheetJob job;
    job.name = "worktop";
    job.material = "Tezgah 38";
    job.length = 3000.0;
    job.width = 700.0;
    job.thickness = 38.0;
    job.partSpacing = 12.0;
    PartMachining worktop;
    worktop.label = "1";
    worktop.name = "Tezgah";
    worktop.thickness = 38.0;
    const detailing::FlatCutout sink{Vec2(720.0, 320.0), openingLength, 480.0, 15.0};
    worktop.operations = {
        Contour{nesting::normalized(detailing::cutoutOutline(sink)), true},
        Contour{nesting::rectangle(nesting::Rect{{20.0, 20.0}, {2420.0, 620.0}})},
    };
    PartMachining small;
    small.label = "2";
    small.name = "Baza";
    small.thickness = 38.0;
    small.operations = {Contour{nesting::rectangle(nesting::Rect{{2440.0, 20.0}, {2640.0, 120.0}})}};
    job.parts = {worktop, small};
    return job;
}

} // namespace

TEST_CASE("Openings are cut from the inside, before any part comes loose",
          "[cam][planner][kitchen]")
{
    NestingCenterProfile profile = defaultProfile();
    for (const bool climb : {true, false}) {
        profile.climbMilling = climb;
        const SheetProgram sp = planSheet(worktopJob(), profile);
        const auto moves = cuts(sp.program, profile.contourTool);
        REQUIRE_FALSE(moves.empty());

        // The very first cut is in the opening, even though the small part is smaller.
        const Vec3 first = moves.front().second;
        CHECK(first.x > 440.0);
        CHECK(first.x < 1000.0);

        // At full depth the tool centre runs inside the opening, 6 mm (its radius) from the
        // edge: the opening keeps its size.
        std::vector<Vec2> loop;
        for (const auto& [from, to] : moves) {
            if (to.z == Approx(-0.3) && from.z == Approx(-0.3) && to.x < 1000.0 && to.y > 80.0
                && to.y < 560.0) {
                loop.push_back({to.x, to.y});
            }
        }
        REQUIRE(loop.size() >= 8);
        for (const Vec2& p : loop) {
            CHECK(p.x >= 440.0 + 6.0 - 1e-3);
            CHECK(p.x <= 1000.0 - 6.0 + 1e-3);
            CHECK(p.y >= 80.0 + 6.0 - 1e-3);
            CHECK(p.y <= 560.0 - 6.0 + 1e-3);
        }
        // Climb milling inside an opening runs counter-clockwise.
        Real twice = 0.0;
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const Vec2 a = loop[i];
            const Vec2 b = loop[(i + 1) % loop.size()];
            twice += a.x * b.y - b.x * a.y;
        }
        CHECK((climb ? twice > 0.0 : twice < 0.0));
    }
    CHECK(planSheet(worktopJob(), profile).leftovers.empty());
}

TEST_CASE("An opening narrower than the router is left for someone else", "[cam][planner][kitchen]")
{
    const SheetProgram sp = planSheet(worktopJob(10.0), defaultProfile());
    REQUIRE(sp.leftovers.size() == 1);
    CHECK(sp.leftovers.front().label == "1");
    CHECK(sp.leftovers.front().noTool == 1);
}
