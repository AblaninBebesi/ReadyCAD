#include "readycad/core/Aabb.hpp"

#include <glm/common.hpp>

#include <limits>

namespace rc::core {

namespace {
constexpr Real kInf = std::numeric_limits<Real>::infinity();
}

Aabb::Aabb() noexcept : min_(kInf), max_(-kInf) {}

Aabb::Aabb(const Vec3& cornerA, const Vec3& cornerB) noexcept
    : min_(glm::min(cornerA, cornerB)), max_(glm::max(cornerA, cornerB))
{
}

Aabb Aabb::fromOriginAndSize(const Vec3& origin, const Vec3& size) noexcept
{
    return Aabb(origin, origin + size);
}

bool Aabb::isEmpty() const noexcept
{
    return min_.x > max_.x || min_.y > max_.y || min_.z > max_.z;
}

Vec3 Aabb::size() const noexcept
{
    return isEmpty() ? Vec3(0.0) : max_ - min_;
}

Vec3 Aabb::center() const noexcept
{
    return isEmpty() ? Vec3(0.0) : (min_ + max_) * 0.5;
}

Real Aabb::volume() const noexcept
{
    const Vec3 s = size();
    return s.x * s.y * s.z;
}

bool Aabb::contains(const Vec3& point, Real eps) const noexcept
{
    return !isEmpty()
        && point.x >= min_.x - eps && point.x <= max_.x + eps
        && point.y >= min_.y - eps && point.y <= max_.y + eps
        && point.z >= min_.z - eps && point.z <= max_.z + eps;
}

bool Aabb::contains(const Aabb& other, Real eps) const noexcept
{
    return !other.isEmpty() && contains(other.min_, eps) && contains(other.max_, eps);
}

bool Aabb::intersects(const Aabb& other, Real eps) const noexcept
{
    if (isEmpty() || other.isEmpty()) {
        return false;
    }
    // Overlap length on each axis must be strictly larger than the tolerance.
    for (int axis = 0; axis < 3; ++axis) {
        const Real overlap = glm::min(max_[axis], other.max_[axis])
                           - glm::max(min_[axis], other.min_[axis]);
        if (overlap <= eps) {
            return false;
        }
    }
    return true;
}

Aabb Aabb::intersection(const Aabb& other) const noexcept
{
    if (isEmpty() || other.isEmpty()) {
        return {};
    }
    const Vec3 lo = glm::max(min_, other.min_);
    const Vec3 hi = glm::min(max_, other.max_);
    if (lo.x > hi.x || lo.y > hi.y || lo.z > hi.z) {
        return {};
    }
    return Aabb(lo, hi);
}

Aabb Aabb::translated(const Vec3& offset) const noexcept
{
    if (isEmpty()) {
        return *this;
    }
    return Aabb(min_ + offset, max_ + offset);
}

void Aabb::expand(const Vec3& point) noexcept
{
    min_ = glm::min(min_, point);
    max_ = glm::max(max_, point);
}

void Aabb::expand(const Aabb& other) noexcept
{
    if (other.isEmpty()) {
        return;
    }
    expand(other.min_);
    expand(other.max_);
}

} // namespace rc::core
