#include "readycad/core/Placement.hpp"

namespace rc::core {

namespace {

int normalizedTurns(int turns) noexcept
{
    return ((turns % 4) + 4) % 4;
}

} // namespace

Vec3 Placement::direction(const Vec3& v) const noexcept
{
    switch (normalizedTurns(quarterTurns)) {
    case 1: return {v.z, v.y, -v.x};
    case 2: return {-v.x, v.y, -v.z};
    case 3: return {-v.z, v.y, v.x};
    default: return v;
    }
}

Vec3 Placement::point(const Vec3& p) const noexcept
{
    return direction(p) + offset;
}

Aabb Placement::box(const Aabb& b) const noexcept
{
    if (b.isEmpty()) {
        return b;
    }
    return Aabb(point(b.min()), point(b.max())); // opposite corners stay opposite
}

Axis Placement::axis(Axis a) const noexcept
{
    if (a == Axis::Y || normalizedTurns(quarterTurns) % 2 == 0) {
        return a;
    }
    return a == Axis::X ? Axis::Z : Axis::X;
}

Placement Placement::within(const Placement& outer) const noexcept
{
    return {normalizedTurns(quarterTurns + outer.quarterTurns), outer.point(offset)};
}

bool Placement::isIdentity() const noexcept
{
    return normalizedTurns(quarterTurns) == 0 && offset == Vec3(0.0);
}

} // namespace rc::core
