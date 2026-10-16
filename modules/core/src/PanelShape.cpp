#include "readycad/core/PanelShape.hpp"

#include "readycad/core/Polygon.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace rc::core {

namespace {

constexpr Real kTouch = 1e-6;

std::size_t slot(EdgeSide e) noexcept
{
    return static_cast<std::size_t>(e);
}

bool alongLength(EdgeSide e) noexcept
{
    return e == EdgeSide::L1 || e == EdgeSide::L2;
}

/// A corner seen from the outline running counter-clockwise: where it is (finished and on the
/// grown edge lines), the direction of the edge coming in and of the edge going out.
struct CornerFrame {
    Vec2 finished;
    Vec2 grown;
    Vec2 in;
    Vec2 out;
    EdgeSide inEdge;
    EdgeSide outEdge;
};

std::array<CornerFrame, 4> cornerFrames(Real l, Real w, const std::array<Real, 4>& g)
{
    const Real l1 = g[slot(EdgeSide::L1)];
    const Real l2 = g[slot(EdgeSide::L2)];
    const Real w1 = g[slot(EdgeSide::W1)];
    const Real w2 = g[slot(EdgeSide::W2)];
    return {{
        {Vec2(0.0, 0.0), Vec2(-w1, -l1), Vec2(0.0, -1.0), Vec2(1.0, 0.0), EdgeSide::W1, EdgeSide::L1},
        {Vec2(l, 0.0), Vec2(l + w2, -l1), Vec2(1.0, 0.0), Vec2(0.0, 1.0), EdgeSide::L1, EdgeSide::W2},
        {Vec2(l, w), Vec2(l + w2, w + l2), Vec2(0.0, 1.0), Vec2(-1.0, 0.0), EdgeSide::W2, EdgeSide::L2},
        {Vec2(0.0, w), Vec2(-w1, w + l2), Vec2(-1.0, 0.0), Vec2(0.0, -1.0), EdgeSide::L2, EdgeSide::W1},
    }};
}

/// The point of the edge line through `corner` along `direction` that is nearest to `point`.
Vec2 onLine(Vec2 corner, Vec2 direction, Vec2 point)
{
    return corner + direction * glm::dot(point - corner, direction);
}

PanelCorner cornerOf(const ShapeFeature& feature, bool& isCorner)
{
    isCorner = true;
    if (const auto* n = std::get_if<CornerNotch>(&feature)) {
        return n->corner;
    }
    if (const auto* r = std::get_if<CornerRound>(&feature)) {
        return r->corner;
    }
    if (const auto* c = std::get_if<CornerChamfer>(&feature)) {
        return c->corner;
    }
    isCorner = false;
    return PanelCorner::L1W1;
}

/// The notch's interval along its edge, in the edge's own coordinate (from W1 or from L1).
std::pair<Real, Real> notchSpan(const EdgeNotch& n, Real edgeLength)
{
    return n.fromEnd ? std::pair{edgeLength - n.offset - n.length, edgeLength - n.offset}
                     : std::pair{n.offset, n.offset + n.length};
}

/// Centre of an opening measured from the edges of the corner `from`.
Vec2 centreFrom(PanelCorner from, Real alongL, Real alongW, Real l, Real w)
{
    const bool atW1 = from == PanelCorner::L1W1 || from == PanelCorner::L2W1;
    const bool atL1 = from == PanelCorner::L1W1 || from == PanelCorner::L1W2;
    return {atW1 ? alongL : l - alongL, atL1 ? alongW : w - alongW};
}

void require(bool condition)
{
    if (!condition) {
        throw std::invalid_argument("shape feature: invalid size");
    }
}

bool finite(std::initializer_list<Real> values)
{
    return std::all_of(values.begin(), values.end(), [](Real v) { return std::isfinite(v); });
}

} // namespace

bool changesOutline(const ShapeFeature& feature) noexcept
{
    return !std::holds_alternative<InnerCutout>(feature) && !std::holds_alternative<RoundHole>(feature);
}

void checkFeature(const ShapeFeature& feature)
{
    std::visit(
        [](const auto& f) {
            using T = std::decay_t<decltype(f)>;
            if constexpr (std::is_same_v<T, CornerNotch> || std::is_same_v<T, CornerChamfer>) {
                require(finite({f.alongLength, f.alongWidth}) && f.alongLength > 0.0
                        && f.alongWidth > 0.0);
            } else if constexpr (std::is_same_v<T, CornerRound>) {
                require(std::isfinite(f.radius) && f.radius > 0.0);
            } else if constexpr (std::is_same_v<T, EdgeNotch>) {
                require(finite({f.offset, f.length, f.depth}) && f.offset >= 0.0 && f.length > 0.0
                        && f.depth > 0.0);
            } else if constexpr (std::is_same_v<T, InnerCutout>) {
                require(finite({f.alongLength, f.alongWidth, f.length, f.width, f.radius})
                        && f.alongLength >= 0.0 && f.alongWidth >= 0.0 && f.length > 0.0
                        && f.width > 0.0 && f.radius >= 0.0
                        && 2.0 * f.radius <= std::min(f.length, f.width) + kTouch);
            } else {
                require(finite({f.alongLength, f.alongWidth, f.diameter}) && f.alongLength >= 0.0
                        && f.alongWidth >= 0.0 && f.diameter > 0.0);
            }
        },
        feature);
}

std::vector<OutlineSide> panelOutline(Real length, Real width,
                                      const std::vector<ShapeFeature>& features,
                                      const std::array<Real, 4>& grow, int segments)
{
    std::vector<OutlineSide> out;
    auto push = [&out](Vec2 p, OutlineSideKind kind, EdgeSide edge, PanelCorner corner) {
        if (!out.empty() && glm::length(out.back().start - p) <= kTouch) {
            out.back() = {p, kind, edge, corner}; // a side of no length: the later one counts
            return;
        }
        out.push_back({p, kind, edge, corner});
    };

    const std::array<CornerFrame, 4> frames = cornerFrames(length, width, grow);
    for (std::size_t k = 0; k < frames.size(); ++k) {
        const CornerFrame& c = frames[k];
        const auto corner = static_cast<PanelCorner>(k);

        // 1. The corner itself.
        const ShapeFeature* feature = nullptr;
        for (const ShapeFeature& f : features) {
            bool isCorner = false;
            if (cornerOf(f, isCorner) == corner && isCorner) {
                feature = &f;
                break;
            }
        }
        const Vec2 f = c.finished;
        if (feature == nullptr) {
            push(c.grown, OutlineSideKind::Edge, c.outEdge, corner);
        } else if (const auto* round = std::get_if<CornerRound>(feature)) {
            const Real r = round->radius;
            const Real grown = r + (grow[slot(c.inEdge)] + grow[slot(c.outEdge)]) / 2.0;
            const Vec2 centre = f - c.in * r + c.out * r;
            const int pieces = std::max(1, segments);
            for (int i = 0; i <= pieces; ++i) {
                const Real t = glm::radians(90.0 * i / pieces);
                Vec2 p = centre + grown * (std::cos(t) * -c.out + std::sin(t) * c.in);
                if (i == 0) {
                    p = onLine(c.grown, c.in, p); // exactly on the grown edges
                } else if (i == pieces) {
                    p = onLine(c.grown, c.out, p);
                }
                push(p, i < pieces ? OutlineSideKind::Corner : OutlineSideKind::Edge, c.outEdge,
                     corner);
            }
        } else {
            const bool notch = std::holds_alternative<CornerNotch>(*feature);
            const Real alongL = notch ? std::get<CornerNotch>(*feature).alongLength
                                      : std::get<CornerChamfer>(*feature).alongLength;
            const Real alongW = notch ? std::get<CornerNotch>(*feature).alongWidth
                                      : std::get<CornerChamfer>(*feature).alongWidth;
            const Real sIn = alongLength(c.inEdge) ? alongL : alongW;
            const Real sOut = alongLength(c.outEdge) ? alongL : alongW;
            push(onLine(c.grown, c.in, f - c.in * sIn),
                 notch ? OutlineSideKind::Notch : OutlineSideKind::Corner, c.outEdge, corner);
            if (notch) {
                push(f - c.in * sIn + c.out * sOut, OutlineSideKind::Notch, c.outEdge, corner);
            }
            push(onLine(c.grown, c.out, f + c.out * sOut), OutlineSideKind::Edge, c.outEdge,
                 corner);
        }

        // 2. Notches in the edge that leaves the corner, in the order they are passed.
        const EdgeSide edge = c.outEdge;
        const Real edgeLength = alongLength(edge) ? length : width;
        const bool forward = edge == EdgeSide::L1 || edge == EdgeSide::W2;
        std::vector<std::tuple<Real, Real, Real>> notches; // from, to (along the run), depth
        for (const ShapeFeature& feat : features) {
            if (const auto* n = std::get_if<EdgeNotch>(&feat); n != nullptr && n->edge == edge) {
                const auto [u0, u1] = notchSpan(*n, edgeLength);
                notches.emplace_back(forward ? u0 : edgeLength - u1, forward ? u1 : edgeLength - u0,
                                     n->depth);
            }
        }
        std::sort(notches.begin(), notches.end());
        const Vec2 inward(-c.out.y, c.out.x);
        for (const auto& [s0, s1, depth] : notches) {
            push(onLine(c.grown, c.out, f + c.out * s0), OutlineSideKind::Notch, edge, corner);
            push(f + c.out * s0 + inward * depth, OutlineSideKind::Notch, edge, corner);
            push(f + c.out * s1 + inward * depth, OutlineSideKind::Notch, edge, corner);
            push(onLine(c.grown, c.out, f + c.out * s1), OutlineSideKind::Edge, edge, corner);
        }
    }
    if (out.size() > 1 && glm::length(out.front().start - out.back().start) <= kTouch) {
        out.pop_back();
    }
    return out;
}

std::vector<Vec2> outlinePoints(const std::vector<OutlineSide>& outline)
{
    std::vector<Vec2> points;
    points.reserve(outline.size());
    for (const OutlineSide& side : outline) {
        points.push_back(side.start);
    }
    return points;
}

std::vector<PanelCutout> featureOpenings(Real length, Real width,
                                         const std::vector<ShapeFeature>& features)
{
    std::vector<PanelCutout> openings;
    for (const ShapeFeature& feature : features) {
        if (const auto* c = std::get_if<InnerCutout>(&feature)) {
            openings.push_back({centreFrom(c->from, c->alongLength, c->alongWidth, length, width),
                                c->length, c->width, c->radius});
        } else if (const auto* h = std::get_if<RoundHole>(&feature)) {
            openings.push_back({centreFrom(h->from, h->alongLength, h->alongWidth, length, width),
                                h->diameter, h->diameter, h->diameter / 2.0});
        }
    }
    return openings;
}

std::vector<Vec2> openingOutline(const PanelCutout& opening, int segments)
{
    const Real r = std::clamp(opening.radius, 0.0, std::min(opening.length, opening.width) / 2.0);
    const Real hx = opening.length / 2.0 - r;
    const Real hy = opening.width / 2.0 - r;
    // Centres of the corner arcs, counter-clockwise from the lower right, and where each arc
    // starts (degrees).
    const std::array<std::pair<Vec2, Real>, 4> corners{{{Vec2(hx, -hy), -90.0},
                                                        {Vec2(hx, hy), 0.0},
                                                        {Vec2(-hx, hy), 90.0},
                                                        {Vec2(-hx, -hy), 180.0}}};
    std::vector<Vec2> points;
    for (const auto& [corner, start] : corners) {
        if (r <= kEpsilon || segments < 1) {
            // A square corner: the corner of the rectangle itself.
            points.push_back(opening.center
                             + Vec2(std::copysign(opening.length / 2.0, corner.x),
                                    std::copysign(opening.width / 2.0, corner.y)));
            continue;
        }
        for (int i = 0; i <= segments; ++i) {
            const Real angle = glm::radians(start + 90.0 * i / segments);
            const Vec2 p = opening.center + corner + r * Vec2(std::cos(angle), std::sin(angle));
            if (points.empty() || glm::length(points.back() - p) > kTouch) {
                points.push_back(p); // a full circle: neighbouring arcs share their ends
            }
        }
    }
    if (points.size() > 1 && glm::length(points.front() - points.back()) <= kTouch) {
        points.pop_back();
    }
    return points;
}

bool shapeFits(Real length, Real width, const std::vector<ShapeFeature>& features,
               const std::vector<PanelCutout>& openings, Real bridge)
{
    // Corners: one feature each, smaller than the panel.
    std::array<int, 4> perCorner{};
    for (const ShapeFeature& feature : features) {
        bool isCorner = false;
        const PanelCorner corner = cornerOf(feature, isCorner);
        if (!isCorner) {
            continue;
        }
        if (++perCorner[static_cast<std::size_t>(corner)] > 1) {
            return false;
        }
        Real alongL = 0.0;
        Real alongW = 0.0;
        if (const auto* r = std::get_if<CornerRound>(&feature)) {
            alongL = alongW = r->radius;
        } else if (const auto* n = std::get_if<CornerNotch>(&feature)) {
            alongL = n->alongLength;
            alongW = n->alongWidth;
        } else {
            alongL = std::get<CornerChamfer>(feature).alongLength;
            alongW = std::get<CornerChamfer>(feature).alongWidth;
        }
        if (alongL >= length - kTouch || alongW >= width - kTouch) {
            return false;
        }
    }
    // Edge notches: inside their edge, not through the panel.
    for (const ShapeFeature& feature : features) {
        if (const auto* n = std::get_if<EdgeNotch>(&feature)) {
            const Real edgeLength = alongLength(n->edge) ? length : width;
            const Real across = alongLength(n->edge) ? width : length;
            const auto [u0, u1] = notchSpan(*n, edgeLength);
            if (u0 <= kTouch || u1 >= edgeLength - kTouch || n->depth >= across - kTouch) {
                return false;
            }
        }
    }
    // The outline does not cut through itself: features neither meet nor overlap.
    const std::vector<Vec2> outline = outlinePoints(panelOutline(length, width, features));
    if (!isSimple(outline, kTouch) || signedArea(outline) <= 0.0) {
        return false;
    }
    // Openings: inside, with material around them.
    std::vector<std::vector<Vec2>> holes;
    for (const PanelCutout& opening : openings) {
        holes.push_back(openingOutline(opening));
    }
    for (std::size_t i = 0; i < holes.size(); ++i) {
        const bool inside = std::all_of(holes[i].begin(), holes[i].end(),
                                        [&](Vec2 p) { return contains(outline, p); });
        if (!inside || outlineDistance(holes[i], outline) < bridge - kTouch) {
            return false;
        }
        for (std::size_t j = i + 1; j < holes.size(); ++j) {
            if (contains(holes[j], holes[i].front()) || contains(holes[i], holes[j].front())
                || outlineDistance(holes[i], holes[j]) < bridge - kTouch) {
                return false;
            }
        }
    }
    return true;
}

} // namespace rc::core
