#include "readycad/cam/NestingCenter.hpp"

#include "readycad/cam/Route.hpp"
#include "readycad/i18n/Labels.hpp"
#include "readycad/i18n/Localization.hpp"
#include "readycad/i18n/Utf8.hpp"

#include <clipper2/clipper.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <optional>
#include <system_error>

namespace rc::cam {

using i18n::TextId;

std::vector<Tool> defaultTools()
{
    //      no kind              Ø     length rpm    feed     plunge  step   name
    return {
        {1, ToolKind::Router, 12.0, 42.0, 18000, 12000.0, 3000.0, 25.0, "Kompresyon frezesi D12"},
        {2, ToolKind::Router, 4.0, 15.0, 22000, 4000.0, 1500.0, 5.0, "Kanal frezesi D4"},
        {11, ToolKind::Drill, 5.0, 35.0, 4000, 0.0, 2500.0, 0.0, "Matkap D5"},
        {12, ToolKind::Drill, 7.0, 35.0, 4000, 0.0, 2500.0, 0.0, "Matkap D7"},
        {13, ToolKind::Drill, 8.0, 35.0, 4000, 0.0, 2500.0, 0.0, "Matkap D8"},
        {14, ToolKind::Drill, 15.0, 20.0, 3000, 0.0, 1500.0, 0.0, "Matkap D15"},
        {15, ToolKind::Drill, 35.0, 20.0, 2500, 0.0, 1000.0, 0.0, "Menteşe matkabı D35"},
    };
}

NestingCenterProfile defaultProfile()
{
    return NestingCenterProfile{};
}

void validate(const NestingCenterProfile& profile)
{
    const auto require = [](bool ok, const std::string& what) {
        if (!ok) {
            throw CamError("invalid CNC settings: " + what);
        }
    };
    for (std::size_t i = 0; i < profile.tools.size(); ++i) {
        const Tool& t = profile.tools[i];
        const std::string name = "T" + std::to_string(t.number);
        require(t.number > 0, "tool numbers must be positive");
        require(t.diameter > 0.0 && t.cuttingLength > 0.0, name + ": size <= 0");
        require(t.rpm > 0 && t.plungeFeed > 0.0, name + ": speed <= 0");
        require(t.kind == ToolKind::Drill || (t.feed > 0.0 && t.stepDown > 0.0),
                name + ": feed or step down <= 0");
        for (std::size_t j = i + 1; j < profile.tools.size(); ++j) {
            require(profile.tools[j].number != t.number, name + " twice in the tool table");
        }
    }
    const auto contour = std::find_if(profile.tools.begin(), profile.tools.end(),
                                      [&](const Tool& t) { return t.number == profile.contourTool; });
    require(contour != profile.tools.end() && contour->kind == ToolKind::Router,
            "the contour tool must be a router of the tool table");
    require(profile.spoilboardCut >= 0.0 && profile.safeHeight > 0.0 && profile.clearance > 0.0
                && profile.clearance <= profile.safeHeight,
            "heights");
    require(profile.rapidSpeed > 0.0 && profile.toolChangeSeconds >= 0.0, "speeds");
    require(profile.dialect.decimals >= 0 && profile.dialect.decimals <= 6, "decimals 0..6");
}

namespace {

constexpr Real kEps = 1e-6;
constexpr Real kDiameterTolerance = 0.05;
constexpr int kCircleSegments = 48;

std::string txt(TextId id)
{
    return std::string(i18n::text(id));
}

const Tool* findTool(const NestingCenterProfile& p, int number)
{
    for (const Tool& t : p.tools) {
        if (t.number == number) {
            return &t;
        }
    }
    return nullptr;
}

/// The drill of the head with this diameter.
const Tool* drillFor(const NestingCenterProfile& p, Real diameter, Real depth)
{
    for (const Tool& t : p.tools) {
        if (t.kind == ToolKind::Drill && std::abs(t.diameter - diameter) <= kDiameterTolerance
            && t.cuttingLength + kEps >= depth) {
            return &t;
        }
    }
    return nullptr;
}

/// The largest router that fits: smaller than a hole, at most as wide as a groove.
const Tool* routerFor(const NestingCenterProfile& p, Real width, Real depth, bool mayBeEqual)
{
    const Tool* best = nullptr;
    for (const Tool& t : p.tools) {
        const bool fits = mayBeEqual ? t.diameter <= width + kEps : t.diameter < width - kEps;
        if (t.kind == ToolKind::Router && fits && t.cuttingLength + kEps >= depth
            && (best == nullptr || t.diameter > best->diameter)) {
            best = &t;
        }
    }
    return best;
}

/// Depth levels from the top down to `bottom`, none deeper than `step` below the previous.
std::vector<Real> levels(Real top, Real bottom, Real step)
{
    const Real depth = top - bottom;
    const int passes = std::max(1, static_cast<int>(std::ceil(depth / std::max(step, 0.1) - 1e-9)));
    std::vector<Real> out;
    for (int i = 1; i <= passes; ++i) {
        out.push_back(top - depth * i / passes);
    }
    return out;
}

/// Path of the tool centre around a part: the outline grown by the tool radius, mitred corners
/// (the tool never touches the part, the part keeps its sharp corners), counter-clockwise. A
/// negative radius gives the path inside an opening: the outline shrunk (empty if the tool does
/// not fit in).
nesting::Polygon toolPath(const nesting::Polygon& outline, Real radius)
{
    constexpr Real kScale = 1000.0; // micrometres, as in the nesting module
    Clipper2Lib::Path64 path;
    for (const Vec2& p : outline) {
        path.emplace_back(std::llround(p.x * kScale), std::llround(p.y * kScale));
    }
    const Clipper2Lib::Paths64 grown = Clipper2Lib::InflatePaths(
        {path}, radius * kScale, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon, 2.0);
    const auto largest = std::max_element(grown.begin(), grown.end(), [](const auto& a, const auto& b) {
        return Clipper2Lib::Area(a) < Clipper2Lib::Area(b);
    });
    nesting::Polygon out;
    if (largest != grown.end()) {
        for (const auto& p : *largest) {
            out.push_back({static_cast<Real>(p.x) / kScale, static_cast<Real>(p.y) / kScale});
        }
    }
    return out.size() < 3 ? nesting::Polygon{} : nesting::normalized(out);
}

/// Emits steps and keeps track of where the tool is.
class Builder {
public:
    Builder(Program& program, const NestingCenterProfile& profile)
        : program_(program)
        , safe_(program.thickness + profile.safeHeight)
        , clearance_(program.thickness + profile.clearance)
    {
    }

    [[nodiscard]] Vec2 at() const noexcept { return at_; }
    [[nodiscard]] Real z() const noexcept { return z_.value_or(safe_); }

    void comment(std::string text) { program_.steps.emplace_back(Comment{std::move(text)}); }

    void tool(const Tool& tool)
    {
        if (current_ == tool.number) {
            return;
        }
        retract();
        program_.steps.emplace_back(ToolChange{tool});
        current_ = tool.number;
    }

    /// Up to the safe height, over to `xy`, down to the clearance above the sheet.
    void approach(Vec2 xy)
    {
        retract();
        rapid({xy.x, xy.y, safe_});
        rapid({xy.x, xy.y, clearance_});
    }

    /// Up to the safe height. At the start the position is unknown: the next rapid move goes
    /// up first anyway (the postprocessor lifts before it travels).
    void retract()
    {
        if (z_ && *z_ < safe_ - kEps) {
            rapid({at_.x, at_.y, safe_});
        }
    }

    void feed(Vec3 to, Real feed)
    {
        if (z_ && glm::distance(Vec3(at_.x, at_.y, *z_), to) < kEps) {
            return; // already there
        }
        program_.steps.emplace_back(Feed{to, feed});
        at_ = {to.x, to.y};
        z_ = to.z;
    }

    void drill(Vec2 xy, Real bottom, Real feed)
    {
        if (!z_ || *z_ < clearance_ - kEps) {
            retract();
            rapid({xy.x, xy.y, safe_}); // over the hole before the cycle starts
        }
        program_.steps.emplace_back(DrillCycle{xy, clearance_, bottom, feed});
        at_ = xy;
        z_ = clearance_;
    }

private:
    void rapid(Vec3 to)
    {
        program_.steps.emplace_back(Rapid{to});
        at_ = {to.x, to.y};
        z_ = to.z;
    }

    Program& program_;
    Real safe_;
    Real clearance_;
    Vec2 at_{0.0};
    std::optional<Real> z_;
    std::optional<int> current_;
};

struct Hole {
    Vec2 at{0.0};
    Real bottom = 0.0;
};

struct MilledHole {
    Vec2 at{0.0};
    Real radius = 0.0;
    Real bottom = 0.0;
};

struct GrooveCut {
    Vec2 a{0.0};
    Vec2 b{0.0};
    Real width = 0.0;
    Real bottom = 0.0;
};

struct Cutout {
    nesting::Polygon outline;
    Real area = 0.0;
    std::string title;
    bool inner = false; ///< an opening in a part
};

/// Circular pocket: plunge in the middle, then rings outwards, at every depth level.
void millHole(Builder& b, const Tool& tool, const MilledHole& hole, Real top)
{
    const Real r = tool.diameter / 2.0;
    const Real outer = hole.radius - r;
    std::vector<Real> rings;
    for (Real ring = outer; ring > 1e-3; ring -= 1.6 * r) {
        rings.push_back(ring);
    }
    std::reverse(rings.begin(), rings.end());
    b.approach(hole.at);
    for (const Real z : levels(top, hole.bottom, tool.stepDown)) {
        b.feed({hole.at.x, hole.at.y, z}, tool.plungeFeed);
        for (const Real ring : rings) {
            for (int k = 0; k <= kCircleSegments; ++k) {
                const Real a = 2.0 * std::numbers::pi * k / kCircleSegments; // counter-clockwise
                b.feed({hole.at.x + ring * std::cos(a), hole.at.y + ring * std::sin(a), z}, tool.feed);
            }
        }
        if (!rings.empty()) {
            b.feed({hole.at.x, hole.at.y, z}, tool.feed);
        }
    }
    b.retract();
}

/// A groove: parallel lines across its width, zigzag, level by level.
void millGroove(Builder& b, const Tool& tool, const GrooveCut& g, Real top)
{
    const Vec2 along = g.b - g.a;
    const Real length = glm::length(along);
    if (length < kEps) {
        return;
    }
    const Vec2 u = along / length;
    const Vec2 n(-u.y, u.x);
    const Real spare = std::max(0.0, g.width - tool.diameter);
    const int lines = spare < 1e-3 ? 1 : 1 + static_cast<int>(std::ceil(spare / (0.8 * tool.diameter)));
    std::vector<Real> offsets;
    for (int i = 0; i < lines; ++i) {
        offsets.push_back(lines == 1 ? 0.0 : -spare / 2.0 + spare * i / (lines - 1));
    }
    bool forward = true;
    bool first = true;
    for (const Real z : levels(top, g.bottom, tool.stepDown)) {
        for (const Real offset : offsets) {
            const Vec2 from = (forward ? g.a : g.b) + n * offset;
            const Vec2 to = (forward ? g.b : g.a) + n * offset;
            if (first) {
                b.approach(from);
                first = false;
            } else {
                b.feed({from.x, from.y, b.z()}, tool.feed); // across to the next line, in the groove
            }
            b.feed({from.x, from.y, z}, tool.plungeFeed); // down to this level (if deeper)
            b.feed({to.x, to.y, z}, tool.feed);
            forward = !forward;
        }
        std::reverse(offsets.begin(), offsets.end()); // the next level comes back the same way
    }
    b.retract();
}

/// A part cut out: the tool centre path, starting at its corner nearest to the tool.
void cutOut(Builder& b, const Tool& tool, const Cutout& part, Real top, Real bottom, bool climb)
{
    const Real r = tool.diameter / 2.0;
    nesting::Polygon path = toolPath(part.outline, part.inner ? -r : r);
    // Climb milling with a clockwise spindle: clockwise around a part, counter-clockwise inside
    // an opening (the material is on the other side of the tool).
    if (climb != part.inner) {
        std::reverse(path.begin(), path.end());
    }
    std::size_t start = 0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (glm::distance(path[i], b.at()) < glm::distance(path[start], b.at())) {
            start = i;
        }
    }
    std::rotate(path.begin(), path.begin() + static_cast<std::ptrdiff_t>(start), path.end());
    b.comment(part.title);
    b.approach(path.front());
    for (const Real z : levels(top, bottom, tool.stepDown)) {
        b.feed({path.front().x, path.front().y, z}, tool.plungeFeed);
        for (std::size_t i = 1; i <= path.size(); ++i) {
            const Vec2 p = path[i % path.size()];
            b.feed({p.x, p.y, z}, tool.feed);
        }
    }
    b.retract();
}

std::string toolTitle(TextId what, const Tool& tool, std::size_t count)
{
    return txt(what) + " T" + std::to_string(tool.number) + " D" + i18n::mm(tool.diameter) + " - "
         + std::to_string(count);
}

} // namespace

SheetProgram planSheet(const SheetJob& job, const NestingCenterProfile& profile)
{
    validate(profile);
    if (job.thickness <= 0.0) {
        throw CamError("the sheet has no thickness");
    }
    const Tool* router = findTool(profile, profile.contourTool);
    if (router == nullptr || router->kind != ToolKind::Router) {
        throw CamError("contour tool T" + std::to_string(profile.contourTool)
                       + " is not a router of the tool table");
    }
    if (job.partSpacing > 0.0 && router->diameter > job.partSpacing + kEps) {
        throw CamError("the contour router (D" + i18n::mm(router->diameter)
                       + ") is wider than the part spacing of the cutting plan ("
                       + i18n::mm(job.partSpacing) + " mm): it would cut into the neighbours");
    }
    const Real top = job.thickness;
    const Real through = -profile.spoilboardCut;
    if (router->cuttingLength + kEps < top - through) {
        throw CamError("the contour router (cutting length " + i18n::mm(router->cuttingLength)
                       + " mm) cannot cut through " + i18n::mm(top - through) + " mm");
    }

    SheetProgram out;
    Program& program = out.program;
    program.name = job.name;
    program.length = job.length;
    program.width = job.width;
    program.thickness = job.thickness;

    // --- What this machine does, by tool -------------------------------------------------------
    std::vector<std::pair<const Tool*, Hole>> holes;
    std::vector<std::pair<const Tool*, MilledHole>> milled;
    std::vector<std::pair<const Tool*, GrooveCut>> grooves;
    std::vector<Cutout> cutouts;
    for (const PartMachining& part : job.parts) {
        Leftover left{part.label, part.name};
        for (const Operation& op : part.operations) {
            if (const auto* d = std::get_if<Drilling>(&op)) {
                if (d->face == Face::Bottom) {
                    ++left.bottomHoles;
                    continue;
                }
                if (d->face != Face::Top) {
                    ++left.edgeHoles;
                    continue;
                }
                const bool isThrough = d->depth >= top - 1e-3;
                const Real bottom = isThrough ? through : top - d->depth;
                const Vec2 at(d->entry.x, d->entry.y);
                if (const Tool* drill = drillFor(profile, d->diameter, top - bottom)) {
                    holes.push_back({drill, {at, bottom}});
                } else if (const Tool* mill = routerFor(profile, d->diameter, top - bottom, false)) {
                    milled.push_back({mill, {at, d->diameter / 2.0, bottom}});
                    ++out.milledHoles;
                } else {
                    ++left.noTool;
                }
            } else if (const auto* g = std::get_if<Groove>(&op)) {
                if (g->face != Face::Top) {
                    ++left.bottomGrooves;
                } else if (const Tool* mill = routerFor(profile, g->width, g->depth, true)) {
                    grooves.push_back({mill, {g->start, g->end, g->width, top - g->depth}});
                } else {
                    ++left.noTool;
                }
            } else if (const auto* c = std::get_if<Contour>(&op)) {
                const Real r = router->diameter / 2.0;
                if (c->inner && toolPath(c->outline, -r).size() < 3) {
                    ++left.noTool; // the contour router does not fit into the opening
                    continue;
                }
                // A notch narrower than the router: going round the part the tool passes over
                // it. Grown by the radius and shrunk back, the outline then comes back fuller.
                if (!c->inner) {
                    const nesting::Polygon closed = toolPath(toolPath(c->outline, r), -r);
                    if (nesting::area(closed) > nesting::area(c->outline) + 1.0) {
                        ++left.noTool;
                    }
                }
                const nesting::Rect box = nesting::bounds(c->outline);
                cutouts.push_back({c->outline, nesting::area(c->outline),
                                   part.label + " " + part.name + " " + i18n::mm(box.width()) + "x"
                                       + i18n::mm(box.height()),
                                   c->inner});
            }
        }
        if (left.total() > 0) {
            out.leftovers.push_back(left);
        }
    }

    Builder b(program, profile);
    b.comment(job.material + " " + i18n::mm(job.length) + "x" + i18n::mm(job.width) + "x"
              + i18n::mm(job.thickness) + (job.count > 1 ? " - " + i18n::countText(job.count, TextId::SheetsCount) : ""));

    // --- 1. Drilling head, drill by drill --------------------------------------------------------
    for (const Tool& tool : profile.tools) {
        std::vector<Vec2> points;
        std::vector<Real> bottoms;
        for (const auto& [drill, hole] : holes) {
            if (drill->number == tool.number) {
                points.push_back(hole.at);
                bottoms.push_back(hole.bottom);
            }
        }
        if (points.empty()) {
            continue;
        }
        b.tool(tool);
        b.comment(toolTitle(TextId::CamDrilling, tool, points.size()));
        for (const std::size_t i : shortRoute(points, b.at())) {
            b.drill(points[i], bottoms[i], tool.plungeFeed);
        }
        b.retract();
    }

    // --- 2./3. Router work, router by router; the contour router last -----------------------------
    std::vector<const Tool*> routers;
    for (const Tool& tool : profile.tools) {
        if (tool.kind == ToolKind::Router && tool.number != router->number) {
            routers.push_back(&tool);
        }
    }
    routers.push_back(router);
    for (const Tool* tool : routers) {
        std::vector<MilledHole> myHoles;
        for (const auto& [mill, hole] : milled) {
            if (mill == tool) {
                myHoles.push_back(hole);
            }
        }
        std::vector<GrooveCut> myGrooves;
        for (const auto& [mill, groove] : grooves) {
            if (mill == tool) {
                myGrooves.push_back(groove);
            }
        }
        const bool contours = tool == router && !cutouts.empty();
        if (myHoles.empty() && myGrooves.empty() && !contours) {
            continue;
        }
        b.tool(*tool);
        if (!myHoles.empty()) {
            b.comment(toolTitle(TextId::CamMilledHoles, *tool, myHoles.size()));
            std::vector<Vec2> points;
            for (const MilledHole& h : myHoles) {
                points.push_back(h.at);
            }
            for (const std::size_t i : shortRoute(points, b.at())) {
                millHole(b, *tool, myHoles[i], top);
            }
        }
        if (!myGrooves.empty()) {
            b.comment(toolTitle(TextId::CamGrooves, *tool, myGrooves.size()));
            // Greedy: the groove with the nearest end next, started from that end.
            std::vector<char> done(myGrooves.size(), 0);
            for (std::size_t k = 0; k < myGrooves.size(); ++k) {
                std::size_t best = 0;
                bool reversed = false;
                Real bestDistance = std::numeric_limits<Real>::max();
                for (std::size_t i = 0; i < myGrooves.size(); ++i) {
                    if (done[i] != 0) {
                        continue;
                    }
                    for (const bool flip : {false, true}) {
                        const Real d = glm::distance(b.at(), flip ? myGrooves[i].b : myGrooves[i].a);
                        if (d < bestDistance) {
                            bestDistance = d;
                            best = i;
                            reversed = flip;
                        }
                    }
                }
                done[best] = 1;
                GrooveCut g = myGrooves[best];
                if (reversed) {
                    std::swap(g.a, g.b);
                }
                millGroove(b, *tool, g, top);
            }
        }
        if (contours) {
            b.comment(toolTitle(TextId::CamContours, *tool, cutouts.size()));
            // Openings first, while every part still hangs in the sheet; then small parts
            // first; among those of about the same size the nearest one.
            std::vector<char> done(cutouts.size(), 0);
            std::size_t openings = 0;
            for (const Cutout& c : cutouts) {
                openings += c.inner ? 1 : 0;
            }
            for (std::size_t k = 0; k < cutouts.size(); ++k) {
                const bool innerTurn = k < openings;
                Real smallest = std::numeric_limits<Real>::max();
                for (std::size_t i = 0; i < cutouts.size(); ++i) {
                    if (done[i] == 0 && cutouts[i].inner == innerTurn) {
                        smallest = std::min(smallest, cutouts[i].area);
                    }
                }
                std::size_t best = 0;
                Real bestDistance = std::numeric_limits<Real>::max();
                for (std::size_t i = 0; i < cutouts.size(); ++i) {
                    if (done[i] != 0 || cutouts[i].inner != innerTurn
                        || cutouts[i].area > smallest * 1.2 + kEps) {
                        continue;
                    }
                    for (const Vec2& p : cutouts[i].outline) {
                        if (const Real d = glm::distance(p, b.at()); d < bestDistance) {
                            bestDistance = d;
                            best = i;
                        }
                    }
                }
                done[best] = 1;
                cutOut(b, *tool, cutouts[best], top, through, profile.climbMilling);
            }
        }
    }
    b.retract();
    out.stats = analyse(program, profile.rapidSpeed, profile.toolChangeSeconds);
    return out;
}

std::string programFileName(const SheetJob& job, const Postprocessor& post)
{
    return job.name + "." + post.fileExtension();
}

std::vector<std::filesystem::path> saveSheetPrograms(const nesting::NestingResult& result,
                                                     const detailing::PartSource* parts,
                                                     const NestingCenterProfile& profile,
                                                     const Postprocessor& post,
                                                     const std::filesystem::path& folder,
                                                     std::vector<SheetProgram>* programs)
{
    if (result.settings.machine != nesting::Machine::NestingCenter) {
        throw CamError("CNC programs need a cutting plan made for the nesting centre");
    }
    // Plan everything first: a sheet that cannot be machined writes no file at all.
    std::vector<SheetJob> jobs;
    std::vector<SheetProgram> planned;
    for (std::size_t i = 0; i < result.layouts.size(); ++i) {
        jobs.push_back(sheetJob(result, i, parts));
        planned.push_back(planSheet(jobs.back(), profile));
    }
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        throw CamError("cannot create folder " + i18n::pathToUtf8(folder) + ": " + ec.message());
    }
    std::vector<std::filesystem::path> files;
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        const std::filesystem::path file = folder / programFileName(jobs[i], post);
        const std::string text = post.write(planned[i].program);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            throw CamError("cannot write " + i18n::pathToUtf8(file));
        }
        files.push_back(file);
    }
    if (programs != nullptr) {
        *programs = std::move(planned);
    }
    return files;
}

} // namespace rc::cam
