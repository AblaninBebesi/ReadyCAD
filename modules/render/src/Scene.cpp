#include "readycad/render/Scene.hpp"

#include "readycad/core/Polygon.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/detailing/PanelFrame.hpp"
#include "readycad/render/Triangulate.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>

namespace rc::render {

using core::ComponentKind;

namespace {

glm::vec3 toFloat(const core::Vec3& v)
{
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

ItemCategory categoryOf(ComponentKind kind)
{
    switch (kind) {
    case ComponentKind::BackPanel:
        return ItemCategory::Back;
    case ComponentKind::Shelf:
    case ComponentKind::FixedShelf:
    case ComponentKind::Partition:
        return ItemCategory::Interior;
    case ComponentKind::Door:
    case ComponentKind::DrawerFront:
    case ComponentKind::BlindPanel:
        return ItemCategory::Front;
    case ComponentKind::DrawerBoxSide:
    case ComponentKind::DrawerBoxEnd:
    case ComponentKind::DrawerBottom:
        return ItemCategory::DrawerBox;
    case ComponentKind::Worktop:
        return ItemCategory::Worktop;
    case ComponentKind::Plinth:
        return ItemCategory::Plinth;
    default:
        return ItemCategory::Carcass;
    }
}

glm::vec4 colorOf(ItemCategory category)
{
    switch (category) {
    case ItemCategory::Carcass:   return {0.86f, 0.77f, 0.62f, 1.0f}; // light oak decor
    case ItemCategory::Interior:  return {0.84f, 0.75f, 0.60f, 1.0f};
    case ItemCategory::Back:      return {0.66f, 0.55f, 0.42f, 1.0f}; // HDF
    case ItemCategory::Front:     return {0.94f, 0.94f, 0.92f, 1.0f}; // white MDF
    case ItemCategory::DrawerBox: return {0.78f, 0.78f, 0.76f, 1.0f};
    case ItemCategory::Handle:    return {0.55f, 0.57f, 0.60f, 1.0f}; // brushed metal
    case ItemCategory::Worktop:   return {0.36f, 0.35f, 0.34f, 1.0f}; // anthracite laminate
    case ItemCategory::Plinth:    return {0.42f, 0.40f, 0.38f, 1.0f};
    }
    return glm::vec4(1.0f);
}

constexpr glm::vec4 kSelectedTint{1.0f, 0.58f, 0.18f, 1.0f};

glm::vec3 faceNormal(const detailing::PanelFrame& frame, core::PanelFace face)
{
    glm::vec3 n(0.0f);
    auto set = [&n](core::Axis axis, float sign) { n[core::index(axis)] = sign; };
    switch (face) {
    case core::PanelFace::Top:    set(frame.thicknessAxis(), 1.0f); break;
    case core::PanelFace::Bottom: set(frame.thicknessAxis(), -1.0f); break;
    case core::PanelFace::EdgeL1: set(frame.widthAxis(), -1.0f); break;
    case core::PanelFace::EdgeL2: set(frame.widthAxis(), 1.0f); break;
    case core::PanelFace::EdgeW1: set(frame.lengthAxis(), -1.0f); break;
    case core::PanelFace::EdgeW2: set(frame.lengthAxis(), 1.0f); break;
    }
    return n;
}

bool isFrontPart(ComponentKind kind)
{
    return kind == ComponentKind::Door || kind == ComponentKind::DrawerFront
        || kind == ComponentKind::BlindPanel;
}

/// A shaped panel as a prism: its face (outline less openings) triangulated at both sides,
/// walls along the outline and around the openings, edges where the outline turns. `place`:
/// where the panel's coordinates are in the product (a kitchen run on a side wall).
ScenePrism prismOf(const core::FurnitureComponent& c, const core::Placement& place)
{
    const core::Panel& panel = *c.panel();
    const detailing::PanelFrame frame(c);
    const core::Vec3 size = frame.extents();
    const auto world = [&frame, &place](core::Vec2 p, core::Real z) {
        return toFloat(place.point(frame.toWorld(core::Vec3(p.x, p.y, z))));
    };
    const auto direction = [&frame, &place](core::Vec2 d) {
        core::Vec3 v(0.0);
        v[core::index(frame.lengthAxis())] = d.x;
        v[core::index(frame.widthAxis())] = d.y;
        return toFloat(place.direction(v));
    };
    core::Vec3 thickness(0.0);
    thickness[core::index(frame.thicknessAxis())] = 1.0;
    const glm::vec3 up = toFloat(place.direction(thickness));

    std::vector<core::Vec2> outer = panel.outline().empty()
                                        ? std::vector<core::Vec2>{{0.0, 0.0}, {size.x, 0.0},
                                                                  {size.x, size.y}, {0.0, size.y}}
                                        : core::outlinePoints(panel.outline());
    std::vector<std::vector<core::Vec2>> holes;
    for (const core::PanelCutout& opening : panel.cutouts()) {
        holes.push_back(core::openingOutline(opening));
    }

    ScenePrism prism;
    for (const Triangle2& t : triangulate(outer, holes)) {
        for (const core::Real z : {0.0, size.z}) {
            for (const core::Vec2& p : t) {
                prism.triangles.push_back(world(p, z));
            }
            prism.normals.push_back(z > 0.0 ? up : -up);
        }
    }
    // Walls: the outline runs counter-clockwise, so the outside is on its right; around an
    // opening (also counter-clockwise) the material is on the right and the wall looks left.
    const auto walls = [&](const std::vector<core::Vec2>& loop, bool opening) {
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const core::Vec2 a = loop[i];
            const core::Vec2 b = loop[(i + 1) % loop.size()];
            const core::Vec2 d = b - a;
            if (glm::length(d) <= core::kEpsilon) {
                continue;
            }
            const core::Vec2 right = glm::normalize(core::Vec2(d.y, -d.x));
            const glm::vec3 normal = direction(opening ? -right : right);
            for (const auto& tri : {std::array{world(a, 0.0), world(b, 0.0), world(b, size.z)},
                                    std::array{world(a, 0.0), world(b, size.z), world(a, size.z)}}) {
                prism.triangles.insert(prism.triangles.end(), tri.begin(), tri.end());
                prism.normals.push_back(normal);
            }
            // The outline at both faces; upright edges where it turns (not inside an arc).
            for (const core::Real z : {0.0, size.z}) {
                prism.lines.push_back(world(a, z));
                prism.lines.push_back(world(b, z));
            }
            const core::Vec2 before = loop[(i + loop.size() - 1) % loop.size()];
            const core::Vec2 in = a - before;
            if (glm::length(in) > core::kEpsilon
                && glm::dot(glm::normalize(in), glm::normalize(d)) < 0.94) {
                prism.lines.push_back(world(a, 0.0));
                prism.lines.push_back(world(a, size.z));
            }
        }
    };
    if (core::signedArea(outer) < 0.0) {
        std::reverse(outer.begin(), outer.end());
    }
    walls(outer, false);
    for (auto& hole : holes) {
        if (core::signedArea(hole) < 0.0) {
            std::reverse(hole.begin(), hole.end());
        }
        walls(hole, true);
    }
    return prism;
}

/// Distance along the ray to the triangle (Möller–Trumbore), or nullopt.
std::optional<float> intersectTriangle(const Ray& ray, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    const glm::vec3 e1 = b - a;
    const glm::vec3 e2 = c - a;
    const glm::vec3 p = glm::cross(ray.direction, e2);
    const float det = glm::dot(e1, p);
    if (std::abs(det) < 1e-9f) {
        return std::nullopt; // parallel
    }
    const float inv = 1.0f / det;
    const glm::vec3 s = ray.origin - a;
    const float u = glm::dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }
    const glm::vec3 q = glm::cross(s, e1);
    const float v = glm::dot(ray.direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }
    const float t = glm::dot(e2, q) * inv;
    return t >= 0.0f ? std::optional(t) : std::nullopt;
}

} // namespace

Scene buildScene(const core::Product& product, const SceneOptions& options)
{
    static std::atomic<std::uint64_t> serials{0};
    Scene scene;
    scene.serial = ++serials;
    const core::ConstructionRules& rules = product.rules();

    const core::FurnitureComponent* selected =
        options.selection ? product.find(*options.selection) : nullptr;
    if (selected != nullptr && !selected->hasPanel() && selected->kind() != ComponentKind::Product) {
        scene.highlight = selected->productBox();
    }

    product.visit([&](const core::FurnitureComponent& c) {
        if (!c.hasPanel()) {
            return;
        }
        if (!options.showFronts && isFrontPart(c.kind())) {
            return;
        }
        const bool isSelected = selected != nullptr && (selected == &c || selected->isAncestorOf(c));
        const ItemCategory category = categoryOf(c.kind());
        // A part of a kitchen run on a side wall: its coordinates turned onto that wall.
        const core::Placement place = c.placement();
        const auto axisVector = [&place](core::Axis axis) {
            glm::vec3 v(0.0f);
            v[core::index(place.axis(axis))] = 1.0f;
            return v;
        };
        // The decor of its material: its picture along the grain, or its colour; without a decor
        // the colour of its kind of part.
        SceneSurface surface;
        glm::vec4 plain = colorOf(category);
        if (const core::Decor* decor = core::findDecor(rules.decors, c.panel()->material().decor)) {
            plain = glm::vec4(decor->r / 255.0f, decor->g / 255.0f, decor->b / 255.0f, 1.0f);
            surface.texture = decor->texture;
            const detailing::PanelFrame frame(c);
            surface.grain = axisVector(frame.lengthAxis());
            surface.across = axisVector(frame.widthAxis());
        }
        if (isSelected) {
            surface.tint = glm::mix(glm::vec4(1.0f), kSelectedTint, 0.55f);
        }
        const glm::vec4 color = isSelected ? glm::mix(plain, kSelectedTint, 0.55f) : plain;
        const core::Aabb placed = place.box(c.box());
        if (c.panel()->outline().empty() && c.panel()->cutouts().empty()) {
            SceneBox box;
            box.min = toFloat(placed.min());
            box.max = toFloat(placed.max());
            box.category = category;
            box.color = color;
            box.component = c.id();
            box.selected = isSelected;
            box.surface = surface;
            scene.boxes.push_back(box);
        } else {
            ScenePrism prism = prismOf(c, place);
            prism.category = category;
            prism.color = color;
            prism.component = c.id();
            prism.selected = isSelected;
            prism.surface = surface;
            scene.prisms.push_back(std::move(prism));
        }
        scene.bounds.expand(placed);

        // Handle: a bar standing out of the front's outer face.
        if (const auto* front = dynamic_cast<const core::FrontPanel*>(&c)) {
            if (const auto handle = front->handleGeometry(rules)) {
                const auto& m = *handle->model;
                const bool alongY = handle->along == core::Axis::Y;
                const core::Vec3 half((alongY ? m.width : m.length) / 2.0,
                                      (alongY ? m.length : m.width) / 2.0, 0.0);
                const core::Vec3& center = handle->center;
                const core::Aabb local(center - half, center + half + core::Vec3(0.0, 0.0, m.projection));
                const core::Aabb onWall = place.box(local);
                SceneBox bar;
                bar.min = toFloat(onWall.min());
                bar.max = toFloat(onWall.max());
                bar.category = ItemCategory::Handle;
                bar.color = isSelected ? glm::mix(colorOf(ItemCategory::Handle), kSelectedTint, 0.4f)
                                       : colorOf(ItemCategory::Handle);
                bar.component = c.id();
                bar.selected = isSelected;
                scene.boxes.push_back(bar);
            }
        }

        if (options.showHoles) {
            const detailing::PanelFrame frame(c);
            for (const core::Drilling& d : c.panel()->drillings()) {
                SceneHole hole;
                hole.normal = toFloat(place.direction(core::Vec3(faceNormal(frame, d.face))));
                hole.center = toFloat(place.point(frame.toWorld(d.position))) + hole.normal * 0.3f; // no z-fight
                hole.radius = static_cast<float>(d.diameter / 2.0);
                scene.holes.push_back(hole);
            }
        }
    });
    return scene;
}

std::optional<float> intersect(const Ray& ray, const glm::vec3& min, const glm::vec3& max)
{
    float tNear = 0.0f;
    float tFar = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        const float origin = ray.origin[axis];
        const float dir = ray.direction[axis];
        if (std::abs(dir) < 1e-12f) {
            if (origin < min[axis] || origin > max[axis]) {
                return std::nullopt; // parallel to the slab and outside it
            }
            continue;
        }
        float t0 = (min[axis] - origin) / dir;
        float t1 = (max[axis] - origin) / dir;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tNear = std::max(tNear, t0);
        tFar = std::min(tFar, t1);
        if (tNear > tFar) {
            return std::nullopt;
        }
    }
    return tNear;
}

std::optional<core::ComponentId> pick(const Scene& scene, const Ray& ray)
{
    std::optional<core::ComponentId> best;
    float bestDistance = std::numeric_limits<float>::max();
    for (const SceneBox& box : scene.boxes) {
        if (const auto t = intersect(ray, box.min, box.max); t && *t < bestDistance) {
            bestDistance = *t;
            best = box.component;
        }
    }
    for (const ScenePrism& prism : scene.prisms) {
        for (std::size_t i = 0; i + 2 < prism.triangles.size(); i += 3) {
            const auto t = intersectTriangle(ray, prism.triangles[i], prism.triangles[i + 1],
                                             prism.triangles[i + 2]);
            if (t && *t < bestDistance) {
                bestDistance = *t;
                best = prism.component;
            }
        }
    }
    return best;
}

} // namespace rc::render
