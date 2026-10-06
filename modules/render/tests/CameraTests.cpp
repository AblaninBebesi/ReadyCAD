// Stage 5: camera math of the 3D view (no OpenGL needed).

#include "readycad/render/Camera.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace rc;
using Catch::Approx;

TEST_CASE("Framing a wardrobe looks at its centre and fits it", "[render][camera]")
{
    render::OrbitCamera camera;
    const core::Aabb wardrobe(core::Vec3(0, 0, 0), core::Vec3(800, 2000, 560));
    camera.frame(wardrobe);

    CHECK(camera.target().x == Approx(400.0f));
    CHECK(camera.target().y == Approx(1000.0f));
    CHECK(camera.target().z == Approx(280.0f));
    // The bounding sphere fits into the vertical field of view.
    const float radius = static_cast<float>(glm::length(wardrobe.size())) / 2.0f;
    CHECK(camera.distance() * std::sin(camera.fovY() / 2.0f) >= radius);
    // We look from the front: the eye is in front of the product.
    CHECK(camera.position().z > 560.0f);
}

TEST_CASE("Orbit keeps the distance, zoom changes it within limits", "[render][camera]")
{
    render::OrbitCamera camera;
    camera.frame(core::Aabb(core::Vec3(0), core::Vec3(800, 2000, 560)));
    const float distance = camera.distance();

    camera.orbit(120.0f, -40.0f);
    CHECK(glm::length(camera.position() - camera.target()) == Approx(distance));

    camera.zoom(1.0f);
    CHECK(camera.distance() < distance);
    for (int i = 0; i < 200; ++i) {
        camera.zoom(5.0f);
    }
    CHECK(camera.distance() == Approx(50.0f)); // never inside the model's surface

    camera.orbit(0.0f, 100000.0f); // pitch is clamped: no flipping over the top
    CHECK(camera.position().y > camera.target().y);
}

TEST_CASE("The ray through the viewport centre goes through the target", "[render][camera]")
{
    render::OrbitCamera camera;
    camera.frame(core::Aabb(core::Vec3(0), core::Vec3(800, 2000, 560)));
    const glm::vec2 viewport(1280.0f, 720.0f);

    const render::Ray ray = camera.rayThrough(viewport / 2.0f, viewport);
    const glm::vec3 toTarget = glm::normalize(camera.target() - ray.origin);
    CHECK(glm::dot(ray.direction, toTarget) == Approx(1.0f).margin(1e-4));

    // A pixel to the right gives a ray that goes to the right of the target.
    const render::Ray right = camera.rayThrough({1000.0f, 360.0f}, viewport);
    CHECK(glm::dot(glm::cross(ray.direction, right.direction), glm::vec3(0, 1, 0)) < 0.0f);
}

TEST_CASE("Pan moves the target with the mouse", "[render][camera]")
{
    render::OrbitCamera camera;
    camera.frame(core::Aabb(core::Vec3(0), core::Vec3(800, 2000, 560)));
    const glm::vec3 before = camera.target();
    const float distance = camera.distance();

    camera.pan(0.0f, 100.0f, {1280.0f, 720.0f}); // drag the mouse down -> the view moves up
    CHECK(camera.target().y > before.y);
    CHECK(camera.distance() == Approx(distance));
}
