#pragma once

#include "readycad/core/model/Product.hpp"
#include "readycad/render/Camera.hpp"

#include <glm/vec4.hpp>

#include <optional>
#include <vector>

namespace rc::render {

enum class ItemCategory { Carcass, Back, Interior, Front, DrawerBox, Handle };

/// One axis-aligned box to draw (a panel or a handle), in float millimetres.
struct SceneBox {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    glm::vec4 color{1.0f};
    ItemCategory category = ItemCategory::Carcass;
    core::ComponentId component; // what gets selected when this box is clicked
    bool selected = false;
};

/// A hole, drawn as a small dark disc on the panel surface.
struct SceneHole {
    glm::vec3 center{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f}; // pointing out of the panel
    float radius = 2.5f;
};

/// Everything the renderer draws: built from the model, no OpenGL inside (unit-tested).
struct Scene {
    std::vector<SceneBox> boxes;
    std::vector<SceneHole> holes;
    core::Aabb bounds;                  // all boxes; used to frame the camera and the floor grid
    std::optional<core::Aabb> highlight; // box of a selected composite (section, drawer, ...)
};

struct SceneOptions {
    bool showFronts = true; // doors, drawer fronts and handles
    bool showHoles = false;
    std::optional<core::ComponentId> selection;
};

[[nodiscard]] Scene buildScene(const core::Product& product, const SceneOptions& options);

/// Nearest box hit by the ray; nullopt if the ray misses everything.
[[nodiscard]] std::optional<core::ComponentId> pick(const Scene& scene, const Ray& ray);

/// Distance along the ray to the box, or nullopt (slab method).
[[nodiscard]] std::optional<float> intersect(const Ray& ray, const glm::vec3& min,
                                             const glm::vec3& max);

} // namespace rc::render
