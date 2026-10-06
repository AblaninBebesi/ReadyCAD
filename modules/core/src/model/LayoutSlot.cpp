#include "readycad/core/model/LayoutSlot.hpp"

namespace rc::core {

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
