// Stage 8: the postprocessor — for a part with holes, a groove and a contour the program has the
// right commands in the right order (move, plunge, drill), and replaying it shows that the tool
// never cuts into a part, travels only above the sheet and cuts every part through.

#include "TestJob.hpp"
#include "readycad/test/GCodeReader.hpp"
#include "readycad/cam/NestingCenter.hpp"
#include "readycad/cam/Postprocessor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <algorithm>

using namespace rc;
using namespace rc::cam;
using Catch::Approx;

namespace {

std::string writeTestJob(const GCodeDialect& dialect, const NestingCenterProfile& profile = defaultProfile())
{
    return GCodePostprocessor(dialect).write(planSheet(sample::testJob(), profile).program);
}

/// Distance from p to the segment ab.
Real segmentDistance(Vec2 p, Vec2 a, Vec2 b)
{
    const Vec2 ab = b - a;
    const Real len2 = glm::dot(ab, ab);
    const Real t = len2 > 0.0 ? std::clamp(glm::dot(p - a, ab) / len2, 0.0, 1.0) : 0.0;
    return glm::length(p - (a + t * ab));
}

bool inside(Vec2 p, const nesting::Polygon& poly)
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

/// Least distance from a tool path segment to a part (0 if it enters the part).
Real distanceToPart(Vec2 a, Vec2 b, const nesting::Polygon& part)
{
    if (inside(a, part) || inside(b, part)) {
        return 0.0;
    }
    Real d = 1e300;
    for (std::size_t i = 0; i < part.size(); ++i) {
        const Vec2 p = part[i];
        const Vec2 q = part[(i + 1) % part.size()];
        d = std::min({d, segmentDistance(p, a, b), segmentDistance(q, a, b), segmentDistance(a, p, q),
                      segmentDistance(b, p, q)});
    }
    return d;
}

} // namespace

TEST_CASE("A part with holes and a contour: the commands in order", "[cam][gcode]")
{
    const std::string text = writeTestJob(isoDialect());
    const rc::test::Replay r = rc::test::replay(text);
    CHECK(r.percentFirst);
    CHECK(r.percentLast);
    CHECK_FALSE(r.lineNumbers);
    REQUIRE(r.lines.size() > 10);

    // Header, then the first drill: load, spindle, up to the safe height, over the first hole.
    CHECK(r.lines[0] == "G21 G90 G17");
    CHECK(r.lines[1] == "G54");
    CHECK(r.lines[2] == "T11 M6");
    CHECK(r.lines[3] == "S4000 M3");
    CHECK(r.lines[4] == "G0 Z38.000");
    CHECK(r.lines[5] == "G0 X100.000 Y100.000");
    // Drill cycle: down to 5 (18 - 13), back to R = 21 (3 above the sheet); the next hole
    // needs only its position.
    CHECK(r.lines[6] == "G99 G81 X100.000 Y100.000 Z5.000 R21.000 F2500");
    CHECK(r.lines[7] == "X200.000 Y100.000");
    // The cycle ends before the tool change, through holes go into the spoilboard.
    const std::size_t g80 = r.indexOf("G80");
    CHECK(g80 < r.indexOf("T12 M6"));
    CHECK(r.indexOf("G99 G81 X300.000 Y200.000 Z-0.300 R21.000 F2500") < r.lines.size());

    // The contour router comes last: plunge to -0.3 (through), cut, lift.
    const std::size_t router = r.indexOf("T1 M6");
    REQUIRE(router < r.lines.size());
    CHECK(r.lines[router + 1] == "S18000 M3");
    CHECK(r.indexOf("T2 M6") < router); // the groove before the contours
    const auto plunge = std::find_if(r.lines.begin() + static_cast<std::ptrdiff_t>(router),
                                     r.lines.end(), [](const std::string& l) {
                                         return l.starts_with("G1 Z-0.300");
                                     });
    REQUIRE(plunge != r.lines.end());
    CHECK(*plunge == "G1 Z-0.300 F3000");
    CHECK((std::next(plunge)->starts_with("G1 X") || std::next(plunge)->starts_with("G1 Y")));

    // The end: spindle off, park, program end.
    CHECK(r.lines[r.lines.size() - 3] == "M5");
    CHECK(r.lines[r.lines.size() - 2] == "G0 X0 Y0"); // the dialect's own footer line
    CHECK(r.lines.back() == "M30");
    CHECK(r.tools == std::vector<int>{11, 12, 15, 2, 1});
}

TEST_CASE("Replaying the program: no cut into a part, travel above the sheet, parts cut out",
          "[cam][gcode]")
{
    const SheetJob job = sample::testJob();
    const auto parts = sample::outlines(job);
    for (const GCodeDialect& dialect : dialectPresets()) {
        for (const ZeroPlane zero : {ZeroPlane::Table, ZeroPlane::Top}) {
            GCodeDialect d = dialect;
            d.zero = zero;
            INFO(d.name << ", zero on " << (zero == ZeroPlane::Table ? "table" : "top"));
            const rc::test::Replay r = rc::test::replay(GCodePostprocessor(d).write(
                planSheet(job, defaultProfile()).program));
            const Real shift = zero == ZeroPlane::Top ? -18.0 : 0.0; // machine Z of the table
            const Real top = 18.0 + shift;
            std::vector<bool> cutThrough(parts.size(), false);
            for (const rc::test::Move& m : r.moves) {
                if (!rc::test::known(m.from) || !rc::test::known(m.to)) {
                    continue;
                }
                const Vec2 a(m.from.x, m.from.y);
                const Vec2 b(m.to.x, m.to.y);
                const bool travels = glm::distance(a, b) > 1e-9;
                if (m.rapid && travels) {
                    // Rapid moves sideways only above the sheet.
                    CHECK(m.from.z >= top + 3.0 - 1e-6);
                }
                if (!m.rapid && m.tool == 1 && travels && m.to.z < top) {
                    // The contour router never touches a part: its centre stays a radius away.
                    for (const nesting::Polygon& part : parts) {
                        CHECK(distanceToPart(a, b, part) >= 6.0 - 1e-3);
                    }
                    for (std::size_t i = 0; i < parts.size(); ++i) {
                        if (m.to.z <= -0.3 + shift + 1e-6 && distanceToPart(a, b, parts[i]) < 6.0 + 1e-3) {
                            cutThrough[i] = true;
                        }
                    }
                }
            }
            for (std::size_t i = 0; i < parts.size(); ++i) {
                CHECK(cutThrough[i]);
            }
            // Every hole from above is drilled to its depth.
            for (const Vec2 hole : {Vec2(100, 100), Vec2(200, 100), Vec2(300, 200), Vec2(400, 150)}) {
                const bool drilled = std::any_of(r.moves.begin(), r.moves.end(), [&](const rc::test::Move& m) {
                    return !m.rapid && m.to.x == Approx(hole.x) && m.to.y == Approx(hole.y)
                        && m.to.z <= 5.0 + shift + 1e-6;
                });
                CHECK(drilled);
            }
        }
    }
}

TEST_CASE("Dialects: KDT numbers its lines and drills with plain moves", "[cam][gcode]")
{
    const std::string text = writeTestJob(kdtDialect());
    const rc::test::Replay r = rc::test::replay(text);
    CHECK(r.lineNumbers);
    CHECK(text.starts_with("N10 ("));
    CHECK_FALSE(r.percentFirst);
    CHECK(text.find("G81") == std::string::npos);
    CHECK(r.indexOf("T11 M06") < r.lines.size());
    CHECK(r.indexOf("M05") < r.indexOf("T11 M06"));
    // The first hole as moves: over it, down to R, drill, back to R.
    const std::size_t at = r.indexOf("G0 X100.000 Y100.000");
    REQUIRE(at + 3 < r.lines.size());
    CHECK(r.lines[at + 1] == "G0 Z21.000");
    CHECK(r.lines[at + 2] == "G1 Z5.000 F2500");
    CHECK(r.lines[at + 3] == "G0 Z21.000");
    CHECK(r.lines.back() == "M30");
}

TEST_CASE("Dialects: number format, comments, zero on top", "[cam][gcode]")
{
    GCodeDialect d = isoDialect();
    d.decimals = 2;
    d.comments = CommentStyle::Semicolon;
    d.zero = ZeroPlane::Top;
    d.header = {"G21 G90", "(PROGRAM {name} {length}x{width}x{thickness})"};
    SheetJob job = sample::testJob();
    job.name = "Çekmece yanı (sol)";
    const std::string text = GCodePostprocessor(d).write(planSheet(job, defaultProfile()).program);
    CHECK(text.find("G0 Z20.00") != std::string::npos);       // safe: 20 above the top
    CHECK(text.find("G1 Z-18.30 F3000") != std::string::npos); // through on Z0 = top
    CHECK(text.find("; ReadyCAD Cekmece yani  sol") != std::string::npos);
    CHECK(text.find("(PROGRAM Çekmece yanı (sol) 1000.00x600.00x18.00)") != std::string::npos);
    CHECK(text.find("-0.00") == std::string::npos);
    // Comments are ASCII only (the header template above is the user's own text).
    for (std::size_t start = 0; start < text.size();) {
        const std::size_t end = text.find('\n', start);
        const std::string line = text.substr(start, end - start);
        if (line.starts_with(";")) {
            CHECK(std::all_of(line.begin(), line.end(),
                              [](char c) { return static_cast<unsigned char>(c) < 0x80; }));
        }
        start = end + 1;
    }
}
