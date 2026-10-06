#include "readycad/render/Camera.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace rc::render {

namespace {
constexpr float kMinDistance = 50.0f;    // mm
constexpr float kMaxDistance = 50000.0f; // mm
constexpr float kOrbitSpeed = 0.008f;    // radians per pixel
} // namespace

OrbitCamera::OrbitCamera()
    : yaw_(glm::radians(-30.0f)), pitch_(glm::radians(20.0f)), fovY_(glm::radians(35.0f))
{
}

void OrbitCamera::frame(const core::Aabb& bounds)
{
    if (bounds.isEmpty()) {
        return;
    }
    yaw_ = glm::radians(-30.0f);
    pitch_ = glm::radians(20.0f);
    const core::Vec3 c = bounds.center();
    target_ = glm::vec3(static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z));
    const float radius = static_cast<float>(glm::length(bounds.size())) / 2.0f;
    distance_ = std::clamp(radius / std::sin(fovY_ / 2.0f) * 1.05f, kMinDistance, kMaxDistance);
}

void OrbitCamera::orbit(float dxPixels, float dyPixels)
{
    yaw_ -= dxPixels * kOrbitSpeed;
    pitch_ = std::clamp(pitch_ + dyPixels * kOrbitSpeed, glm::radians(-89.0f), glm::radians(89.0f));
}

void OrbitCamera::pan(float dxPixels, float dyPixels, glm::vec2 viewportSize)
{
    if (viewportSize.y <= 0.0f) {
        return;
    }
    // Millimetres per pixel at the target's depth: the target follows the mouse exactly.
    const float mmPerPixel = 2.0f * distance_ * std::tan(fovY_ / 2.0f) / viewportSize.y;
    const glm::vec3 forward = glm::normalize(target_ - position());
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    const glm::vec3 up = glm::cross(right, forward);
    target_ += (-dxPixels * right + dyPixels * up) * mmPerPixel;
}

void OrbitCamera::zoom(float wheelSteps)
{
    distance_ = std::clamp(distance_ * std::pow(0.85f, wheelSteps), kMinDistance, kMaxDistance);
}

glm::vec3 OrbitCamera::position() const
{
    const glm::vec3 offset(std::cos(pitch_) * std::sin(yaw_), std::sin(pitch_),
                           std::cos(pitch_) * std::cos(yaw_));
    return target_ + offset * distance_;
}

glm::mat4 OrbitCamera::view() const
{
    return glm::lookAt(position(), target_, glm::vec3(0, 1, 0));
}

glm::mat4 OrbitCamera::projection(float aspect) const
{
    // Near/far follow the distance so that depth precision is good at any zoom.
    return glm::perspective(fovY_, std::max(aspect, 0.01f), distance_ / 100.0f, distance_ * 20.0f);
}

Ray OrbitCamera::rayThrough(glm::vec2 pixel, glm::vec2 viewportSize) const
{
    const float aspect = viewportSize.x / std::max(viewportSize.y, 1.0f);
    const glm::mat4 inverse = glm::inverse(projection(aspect) * view());
    const float x = 2.0f * pixel.x / viewportSize.x - 1.0f;
    const float y = 1.0f - 2.0f * pixel.y / viewportSize.y;

    glm::vec4 nearPoint = inverse * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farPoint = inverse * glm::vec4(x, y, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    return {glm::vec3(nearPoint), glm::normalize(glm::vec3(farPoint - nearPoint))};
}

} // namespace rc::render
