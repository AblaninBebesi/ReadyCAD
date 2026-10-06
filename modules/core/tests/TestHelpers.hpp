#pragma once

#include "readycad/core/Aabb.hpp"

#include <catch2/catch_tostring.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include <sstream>
#include <string>

// Readable output of glm vectors and boxes in failed assertions.
namespace Catch {
template <>
struct StringMaker<rc::core::Vec3> {
    static std::string convert(const rc::core::Vec3& v)
    {
        std::ostringstream out;
        out << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return out.str();
    }
};

template <>
struct StringMaker<rc::core::Aabb> {
    static std::string convert(const rc::core::Aabb& box)
    {
        return "[" + StringMaker<rc::core::Vec3>::convert(box.min()) + " .. "
             + StringMaker<rc::core::Vec3>::convert(box.max()) + "]";
    }
};
} // namespace Catch

namespace rc::test {

using core::Aabb;
using core::Vec3;

constexpr core::Real kTol = 1e-6;

/// CHECK_THAT(vec, IsNear(Vec3(1, 2, 3)))
struct IsNear : Catch::Matchers::MatcherGenericBase {
    explicit IsNear(const Vec3& expected) : expected_(expected) {}
    bool match(const Vec3& actual) const { return core::nearlyEqual(actual, expected_, kTol); }
    std::string describe() const override
    {
        return "is near " + Catch::StringMaker<Vec3>::convert(expected_);
    }

private:
    Vec3 expected_;
};

/// CHECK_THAT(component.box(), IsBox({x0, y0, z0}, {x1, y1, z1}))
struct IsBox : Catch::Matchers::MatcherGenericBase {
    IsBox(const Vec3& min, const Vec3& max) : expected_(min, max) {}
    bool match(const Aabb& actual) const
    {
        return core::nearlyEqual(actual.min(), expected_.min(), kTol)
            && core::nearlyEqual(actual.max(), expected_.max(), kTol);
    }
    std::string describe() const override
    {
        return "is box " + Catch::StringMaker<Aabb>::convert(expected_);
    }

private:
    Aabb expected_;
};

} // namespace rc::test
