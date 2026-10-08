#pragma once

// No-Fit Polygons of prepared part shapes, and a thread-safe cache for them with a memory limit.

#include "Clip.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace rc::nesting::detail {

/// A part in one rotation, ready for nesting.
struct Shape {
    clip::Path64 path;   ///< outline grown by half the part spacing, counter-clockwise
    clip::Box bounds;    ///< of `path`
    clip::Box outline;   ///< of the rotated outline itself: this must stay inside the sheet
    bool rectangle = false;
};

/// `outline` turned by quarterTurns and grown by `grow` mm on every side. Two shapes grown by
/// half the spacing that only touch keep their outlines `spacing` apart (or more at corners:
/// mitred, so never less).
[[nodiscard]] Shape makeShape(const Polygon& outline, int quarterTurns, Real grow);

/// Positions of `moving`'s origin at which it overlaps `fixed` (placed at the origin): the outer
/// boundaries of fixed ⊕ (−moving). `shrink` units are taken off every side, so a position
/// that touches exactly is still inside a free region of non-zero area.
struct Nfp {
    clip::Paths64 paths;
    clip::Box bounds;
};

[[nodiscard]] Nfp computeNfp(const Shape& fixed, const Shape& moving, std::int64_t shrink);

/// NFPs depend only on the two shapes, not on where they lie: computed once, reused by every
/// layout of every generation. Shared by the evaluation threads. Above the memory limit new NFPs
/// are no longer stored (computed again when needed): slower, never wrong.
///
/// A plain mutex, held only for the map lookup: std::shared_mutex of MinGW (a pthread rwlock)
/// sometimes failed when several threads used it for the first time at once.
class NfpCache {
public:
    explicit NfpCache(std::size_t limitBytes) : limit_(limitBytes) {}

    [[nodiscard]] std::shared_ptr<const Nfp> get(std::uint32_t fixed, std::uint32_t moving,
                                                 const std::function<Nfp()>& compute) const;
    [[nodiscard]] std::size_t bytes() const noexcept { return bytes_.load(); }

private:
    mutable std::mutex mutex_;
    mutable std::unordered_map<std::uint64_t, std::shared_ptr<const Nfp>> map_;
    mutable std::atomic<std::size_t> bytes_{0};
    std::size_t limit_;
};

} // namespace rc::nesting::detail
