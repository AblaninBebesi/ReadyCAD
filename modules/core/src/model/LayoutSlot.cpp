#include "readycad/core/model/LayoutSlot.hpp"

namespace rc::core {

std::vector<Real> distributeSizes(Real available, const std::vector<std::optional<Real>>& fixed)
{
    Real fixedTotal = 0.0;
    int autoCount = 0;
    for (const std::optional<Real>& size : fixed) {
        if (size) {
            fixedTotal += *size;
        } else {
            ++autoCount;
        }
    }
    Real fixedScale = 1.0;
    Real autoSize = 0.0;
    if (autoCount > 0) {
        if (fixedTotal > available) {
            fixedScale = available / fixedTotal;
        } else {
            autoSize = (available - fixedTotal) / autoCount;
        }
    } else if (fixedTotal > 0.0) {
        fixedScale = available / fixedTotal;
    }
    std::vector<Real> sizes;
    sizes.reserve(fixed.size());
    for (const std::optional<Real>& size : fixed) {
        sizes.push_back(size ? *size * fixedScale : autoSize);
    }
    return sizes;
}

Aabb LayoutSlot::frontRect() const noexcept
{
    const Vec3& lo = box.min();
    const Vec3& hi = box.max();
    return Aabb(Vec3(lo.x - overlays.left, lo.y - overlays.bottom, lo.z),
                Vec3(hi.x + overlays.right, hi.y + overlays.top, hi.z));
}

LayoutSlot LayoutSlot::withBox(const Aabb& newBox) const noexcept
{
    LayoutSlot copy = *this;
    copy.box = newBox;
    copy.overlays = {};
    return copy;
}

} // namespace rc::core
