#pragma once

#include "readycad/core/Aabb.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace rc::render {

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f}; // normalised
};

/// Orbit camera around a target point, the usual CAD navigation:
/// left drag = orbit, right/middle drag = pan, wheel = zoom.
///
/// Pure math, no OpenGL calls, so it is unit-tested. float precision is enough here: this is
/// only the view, all geometry stays in double millimetres in the core.
class OrbitCamera {
public:
    OrbitCamera();

    /// Looks at the box from the front-left-above and fits it into the view.
    void frame(const core::Aabb& bounds);

    void orbit(float dxPixels, float dyPixels);
    void pan(float dxPixels, float dyPixels, glm::vec2 viewportSize);
    void zoom(float wheelSteps);

    [[nodiscard]] glm::vec3 position() const;
    [[nodiscard]] glm::vec3 target() const noexcept { return target_; }
    [[nodiscard]] float distance() const noexcept { return distance_; }
    [[nodiscard]] float fovY() const noexcept { return fovY_; }

    [[nodiscard]] glm::mat4 view() const;
    [[nodiscard]] glm::mat4 projection(float aspect) const;

    /// Ray from the eye through a pixel of the viewport (origin at the top-left corner).
    [[nodiscard]] Ray rayThrough(glm::vec2 pixel, glm::vec2 viewportSize) const;

private:
    glm::vec3 target_{0.0f};
    float distance_ = 3000.0f; // mm
    float yaw_;                // around +Y; 0 = looking from the front (+Z)
    float pitch_;              // above the horizon
    float fovY_;
};

} // namespace rc::render
