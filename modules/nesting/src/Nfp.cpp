#include "readycad/nesting/Nfp.hpp"

#include "Clip.hpp"
#include "NfpCache.hpp"

#include <clipper2/clipper.minkowski.h>
#include <glm/common.hpp>

#include <algorithm>
#include <limits>
#include <mutex>

namespace rc::nesting {

// ---------------------------------------------------------------------------------------------
// Clipper conversions
// ---------------------------------------------------------------------------------------------

namespace clip {

Path64 path(const Polygon& polygon)
{
    Path64 out;
    out.reserve(polygon.size());
    for (const Vec2& p : polygon) {
        out.push_back(point(p));
    }
    return out;
}

Polygon polygon(const Path64& path)
{
    Polygon out;
    out.reserve(path.size());
    for (const Point64& p : path) {
        out.push_back(vec(p));
    }
    return out;
}

Path64 boxPath(const Box& b)
{
    return {Point64(b.x0, b.y0), Point64(b.x1, b.y0), Point64(b.x1, b.y1), Point64(b.x0, b.y1)};
}

Box boundsOf(const Path64& path) noexcept
{
    Box b{std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max(),
          std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::min()};
    for (const Point64& p : path) {
        b.x0 = std::min(b.x0, p.x);
        b.y0 = std::min(b.y0, p.y);
        b.x1 = std::max(b.x1, p.x);
        b.y1 = std::max(b.y1, p.y);
    }
    return b;
}

Box boundsOf(const Paths64& paths) noexcept
{
    Box b{std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max(),
          std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::min()};
    for (const Path64& path : paths) {
        const Box p = boundsOf(path);
        b = {std::min(b.x0, p.x0), std::min(b.y0, p.y0), std::max(b.x1, p.x1),
             std::max(b.y1, p.y1)};
    }
    return b;
}

Box box(const Rect& rect) noexcept
{
    return {units(rect.min.x), units(rect.min.y), units(rect.max.x), units(rect.max.y)};
}

} // namespace clip

// ---------------------------------------------------------------------------------------------
// Shapes and No-Fit Polygons
// ---------------------------------------------------------------------------------------------

namespace detail {

using namespace clip;

Shape makeShape(const Polygon& outline, int quarterTurns, Real grow)
{
    const Polygon turned = transformed(normalized(outline), Transform{quarterTurns, Vec2(0.0)});
    Shape shape;
    shape.rectangle = isRectangle(turned);
    const Path64 exact = path(turned);
    shape.outline = boundsOf(exact);
    const std::int64_t delta = units(grow);
    if (shape.rectangle) {
        shape.path = boxPath(shape.outline.grown(delta)); // exact, 4 points
    } else if (delta == 0) {
        shape.path = exact;
    } else {
        // Mitred corners contain the rounded offset, so parts never come closer than the
        // spacing; the miter limit squares off very sharp spikes.
        const Paths64 grown = Clipper2Lib::InflatePaths(Paths64{exact}, static_cast<double>(delta),
                                                        Clipper2Lib::JoinType::Miter,
                                                        Clipper2Lib::EndType::Polygon, 2.0);
        // A simple polygon grows into one outer path (plus holes where a cavity closed up).
        const auto largest = std::max_element(grown.begin(), grown.end(),
                                              [](const Path64& a, const Path64& b) {
                                                  return Clipper2Lib::Area(a)
                                                       < Clipper2Lib::Area(b);
                                              });
        shape.path = largest != grown.end() ? *largest : exact;
    }
    shape.bounds = boundsOf(shape.path);
    return shape;
}

Nfp computeNfp(const Shape& fixed, const Shape& moving, std::int64_t shrink)
{
    Nfp nfp;
    if (fixed.rectangle && moving.rectangle) {
        // Two boxes overlap while moving's origin is inside this box: no Clipper needed.
        const Box& f = fixed.bounds;
        const Box& m = moving.bounds;
        const Box b{f.x0 - m.x1 + shrink, f.y0 - m.y1 + shrink, f.x1 - m.x0 - shrink,
                    f.y1 - m.y0 - shrink};
        if (b.x0 < b.x1 && b.y0 < b.y1) {
            nfp.paths.push_back(boxPath(b));
        }
    } else {
        // fixed ⊕ (−moving): Clipper sweeps `moving` along the boundary of `fixed`. The outer
        // paths (positive area) are the NFP; holes are where moving would lie inside fixed, or
        // cavities reachable only through it — dropped, i.e. filled.
        const Paths64 sum = Clipper2Lib::MinkowskiDiff(moving.path, fixed.path, true);
        for (const Path64& p : sum) {
            if (Clipper2Lib::IsPositive(p)) {
                nfp.paths.push_back(p);
            }
        }
        if (shrink > 0 && !nfp.paths.empty()) {
            nfp.paths = Clipper2Lib::InflatePaths(nfp.paths, -static_cast<double>(shrink),
                                                  Clipper2Lib::JoinType::Miter,
                                                  Clipper2Lib::EndType::Polygon, 2.0);
        }
    }
    nfp.bounds = boundsOf(nfp.paths);
    return nfp;
}

namespace {

std::size_t footprint(const Nfp& nfp) noexcept
{
    std::size_t bytes = sizeof(Nfp) + 64; // the entry and its map node
    for (const Path64& p : nfp.paths) {
        bytes += sizeof(Path64) + p.size() * sizeof(Point64);
    }
    return bytes;
}

} // namespace

std::shared_ptr<const Nfp> NfpCache::get(std::uint32_t fixed, std::uint32_t moving,
                                         const std::function<Nfp()>& compute) const
{
    const std::uint64_t key = (static_cast<std::uint64_t>(fixed) << 32U) | moving;
    {
        const std::lock_guard lock(mutex_);
        if (const auto it = map_.find(key); it != map_.end()) {
            return it->second;
        }
    }
    auto nfp = std::make_shared<const Nfp>(compute()); // outside the lock: other threads go on
    const std::size_t size = footprint(*nfp);
    const std::lock_guard lock(mutex_);
    if (const auto it = map_.find(key); it != map_.end()) {
        return it->second; // another thread was faster
    }
    if (bytes_.load() + size <= limit_) {
        map_.emplace(key, nfp);
        bytes_ += size;
    }
    return nfp;
}

} // namespace detail

// ---------------------------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------------------------

std::vector<Polygon> noFitPolygon(const Polygon& fixed, const Polygon& moving)
{
    const detail::Shape a = detail::makeShape(fixed, 0, 0.0);
    const detail::Shape b = detail::makeShape(moving, 0, 0.0);
    std::vector<Polygon> out;
    for (const clip::Path64& p : detail::computeNfp(a, b, 0).paths) {
        out.push_back(clip::polygon(p));
    }
    return out;
}

std::optional<Rect> innerFitRectangle(const Rect& container, const Polygon& moving)
{
    const Rect b = bounds(moving);
    const Rect fit{container.min - b.min, container.max - b.max};
    constexpr Real eps = 1e-9;
    if (fit.width() < -eps || fit.height() < -eps) {
        return std::nullopt;
    }
    return Rect{fit.min, glm::max(fit.min, fit.max)};
}

} // namespace rc::nesting
