// Stage 8, across modules: from the editor to the files a CNC nesting centre runs.
// editor -> nesting (plan for the router) -> cam (operations, planning, G-code) -> files, and the
// files replayed: every part is cut out without touching another, every hole from above is
// drilled, everything else is reported for other machines.

#include "readycad/cam/NestingCenter.hpp"
#include "readycad/editor/Document.hpp"
#include "readycad/editor/Operations.hpp"
#include "readycad/test/GCodeReader.hpp"
#include "readycad/test/TempFolder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <algorithm>

using namespace rc;

namespace {

bool inside(core::Vec2 p, const nesting::Polygon& poly)
{
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].y > p.y) != (poly[j].y > p.y)
            && p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y)
                         + poly[i].x) {
            in = !in;
        }
    }
    return in;
}

/// Least distance between a move (in plan) and a part's outline; 0 if the move enters it.
core::Real distance(core::Vec2 a, core::Vec2 b, const nesting::Polygon& part)
{
    if (inside(a, part) || inside(b, part)) {
        return 0.0;
    }
    const auto segment = [](core::Vec2 p, core::Vec2 s, core::Vec2 e) {
        const core::Vec2 se = e - s;
        const core::Real len2 = glm::dot(se, se);
        const core::Real t = len2 > 0.0 ? std::clamp(glm::dot(p - s, se) / len2, 0.0, 1.0) : 0.0;
        return glm::length(p - (s + t * se));
    };
    core::Real d = 1e300;
    for (std::size_t i = 0; i < part.size(); ++i) {
        const core::Vec2 p = part[i];
        const core::Vec2 q = part[(i + 1) % part.size()];
        d = std::min({d, segment(p, a, b), segment(q, a, b), segment(a, p, q), segment(b, p, q)});
    }
    return d;
}

} // namespace

TEST_CASE("A wardrobe with drawers becomes CNC programs that cut every part cleanly",
          "[integration][cam]")
{
    editor::Document doc = editor::Document::fromCatalog(core::CatalogItem::WardrobeWithDrawers);
    nesting::NestingSettings settings;
    settings.machine = nesting::Machine::NestingCenter;
    settings.optimizer.generations = 3;
    settings.optimizer.populationSize = 8;
    const nesting::NestingResult plan = nesting::optimize(
        nesting::partsFromCutList(doc.product(), doc.cutList()), nesting::StockManager{}, settings);
    REQUIRE(plan.unplaced.empty());

    const detailing::PartSource source{doc.product(), doc.cutList()};
    const cam::NestingCenterProfile profile = cam::defaultProfile();
    const cam::GCodePostprocessor post(profile.dialect);
    const rc::test::TempFolder folder("cnc_programs");
    std::vector<cam::SheetProgram> programs;
    const auto files = cam::saveSheetPrograms(plan, &source, profile, post, folder.path(), &programs);
    REQUIRE(files.size() == plan.layouts.size());

    int topHoles = 0;
    int accounted = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        INFO(files[i].filename().string());
        const cam::SheetJob job = cam::sheetJob(plan, i, &source);
        CHECK(files[i].filename().string() == job.name + ".nc");
        const rc::test::Replay r = rc::test::replay(rc::test::readFile(files[i]));
        CHECK(r.lines.back() == "M30");

        std::vector<nesting::Polygon> parts;
        std::vector<core::Vec2> holes;
        int all = 0;
        for (const cam::PartMachining& p : job.parts) {
            for (const cam::Operation& op : p.operations) {
                if (const auto* c = std::get_if<cam::Contour>(&op)) {
                    parts.push_back(c->outline);
                } else if (const auto* d = std::get_if<cam::Drilling>(&op)) {
                    ++all;
                    if (d->face == cam::Face::Top) {
                        holes.push_back({d->entry.x, d->entry.y});
                    }
                }
            }
        }
        topHoles += static_cast<int>(holes.size());
        // Every hole is drilled, milled or reported for another machine.
        int left = 0;
        for (const cam::Leftover& l : programs[i].leftovers) {
            left += l.edgeHoles + l.bottomHoles + l.noTool;
        }
        accounted += left;
        CHECK(programs[i].stats.holes + programs[i].milledHoles + left == all);

        // Replay: the contour router keeps a radius away from every part and goes through each.
        const core::Real top = job.thickness;
        std::vector<bool> through(parts.size(), false);
        for (const rc::test::Move& m : r.moves) {
            if (!rc::test::known(m.from) || m.rapid || m.tool != profile.contourTool) {
                continue;
            }
            const core::Vec2 a(m.from.x, m.from.y);
            const core::Vec2 b(m.to.x, m.to.y);
            if (glm::distance(a, b) < 1e-9 || m.to.z >= top) {
                continue;
            }
            // 10 µm of tolerance: the nesting lets neighbours touch by up to 1 µm (so that parts
            // fitting a gap exactly are found), far below what a machine can hold.
            for (std::size_t k = 0; k < parts.size(); ++k) {
                const core::Real d = distance(a, b, parts[k]);
                CHECK(d >= 6.0 - 0.01);
                through[k] = through[k] || (m.to.z <= -0.3 + 1e-6 && d < 6.0 + 0.01);
            }
        }
        CHECK(std::all_of(through.begin(), through.end(), [](bool b) { return b; }));
        // Every hole from above: a drill went down at its centre.
        for (const core::Vec2 h : holes) {
            CHECK(std::any_of(r.moves.begin(), r.moves.end(), [&](const rc::test::Move& m) {
                return !m.rapid && glm::distance(core::Vec2(m.to.x, m.to.y), h) < 1e-3
                    && m.to.z < top;
            }));
        }
    }
    CHECK(topHoles > 0);
    CHECK(accounted > 0); // edge and bottom holes exist and were reported, not lost
}
