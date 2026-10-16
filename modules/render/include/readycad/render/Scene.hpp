#pragma once

#include "readycad/core/model/Product.hpp"
#include "readycad/render/Camera.hpp"

#include <glm/vec4.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rc::render {

enum class ItemCategory { Carcass, Back, Interior, Front, DrawerBox, Handle, Worktop, Plinth };

/// The surface of a panel: the picture of its decor, laid along the panel's length (the grain).
struct SceneSurface {
    std::string texture;      ///< file in the texture folder; empty: the plain colour
    glm::vec4 tint{1.0f};     ///< the picture is multiplied by it (orange when selected)
    glm::vec3 grain{1.0f, 0.0f, 0.0f};  ///< the panel's length axis: the picture's left to right
    glm::vec3 across{0.0f, 0.0f, 1.0f}; ///< its width axis
};

/// One axis-aligned box to draw (a panel or a handle), in float millimetres.
struct SceneBox {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    glm::vec4 color{1.0f}; ///< plain colour; also what a picture that cannot be loaded shows
    ItemCategory category = ItemCategory::Carcass;
    core::ComponentId component; // what gets selected when this box is clicked
    bool selected = false;
    SceneSurface surface;
};

/// A panel that is not a plain box — notches, shaped corners, openings: its faces as triangles
/// and its outline as lines, in float millimetres.
struct ScenePrism {
    std::vector<glm::vec3> triangles; // three corners per triangle
    std::vector<glm::vec3> normals;   // one per triangle
    std::vector<glm::vec3> lines;     // two ends per line
    glm::vec4 color{1.0f};
    ItemCategory category = ItemCategory::Carcass;
    core::ComponentId component;
    bool selected = false;
    SceneSurface surface;
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
    std::vector<ScenePrism> prisms;
    std::vector<SceneHole> holes;
    core::Aabb bounds;                  // all boxes; used to frame the camera and the floor grid
    std::optional<core::Aabb> highlight; // box of a selected composite (section, drawer, ...)
    /// Different for every scene built: the renderer uploads the prisms again when it changes.
    std::uint64_t serial = 0;
};

struct SceneOptions {
    bool showFronts = true; // doors, drawer fronts and handles
    bool showHoles = false;
    std::optional<core::ComponentId> selection;
};

[[nodiscard]] Scene buildScene(const core::Product& product, const SceneOptions& options);

/// Nearest box or prism hit by the ray; nullopt if the ray misses everything.
[[nodiscard]] std::optional<core::ComponentId> pick(const Scene& scene, const Ray& ray);

/// Distance along the ray to the box, or nullopt (slab method).
[[nodiscard]] std::optional<float> intersect(const Ray& ray, const glm::vec3& min,
                                             const glm::vec3& max);

} // namespace rc::render
