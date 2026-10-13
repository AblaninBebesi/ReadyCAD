#include "readycad/render/Scene.hpp"

#include "readycad/core/model/Parts.hpp"
#include "readycad/detailing/PanelFrame.hpp"

#include <glm/common.hpp>

#include <algorithm>
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
    return kind == ComponentKind::Door || kind == ComponentKind::DrawerFront;
}

/// The panel's box without its cut-outs (a worktop around its sink), as boxes: the cut-outs'
/// edges cut the panel into a grid and the cells outside them remain. Rounded corners of a
/// cut-out are drawn square.
std::vector<core::Aabb> solidParts(const core::FurnitureComponent& c)
{
    const auto& cutouts = c.panel()->cutouts();
    if (cutouts.empty()) {
        return {c.box()};
    }
    const detailing::PanelFrame frame(c);
    const core::Vec3 size = frame.extents();
    std::vector<core::Real> us{0.0, size.x};
    std::vector<core::Real> vs{0.0, size.y};
    for (const core::PanelCutout& cut : cutouts) {
        for (const core::Real sign : {-1.0, 1.0}) {
            us.push_back(std::clamp(cut.center.x + sign * cut.length / 2.0, 0.0, size.x));
            vs.push_back(std::clamp(cut.center.y + sign * cut.width / 2.0, 0.0, size.y));
        }
    }
    for (auto* cuts : {&us, &vs}) {
        std::sort(cuts->begin(), cuts->end());
        cuts->erase(std::unique(cuts->begin(), cuts->end()), cuts->end());
    }
    const int l = core::index(frame.lengthAxis());
    const int w = core::index(frame.widthAxis());
    std::vector<core::Aabb> parts;
    for (std::size_t i = 0; i + 1 < us.size(); ++i) {
        for (std::size_t j = 0; j + 1 < vs.size(); ++j) {
            const core::Real u = (us[i] + us[i + 1]) / 2.0;
            const core::Real v = (vs[j] + vs[j + 1]) / 2.0;
            const bool open = std::any_of(cutouts.begin(), cutouts.end(), [&](const auto& cut) {
                return std::abs(u - cut.center.x) < cut.length / 2.0
                    && std::abs(v - cut.center.y) < cut.width / 2.0;
            });
            if (open) {
                continue;
            }
            core::Vec3 lo = c.box().min();
            core::Vec3 hi = c.box().max();
            hi[l] = lo[l] + us[i + 1];
            lo[l] += us[i];
            hi[w] = lo[w] + vs[j + 1];
            lo[w] += vs[j];
            parts.emplace_back(lo, hi);
        }
    }
    return parts;
}

} // namespace

Scene buildScene(const core::Product& product, const SceneOptions& options)
{
    Scene scene;
    const core::ConstructionRules& rules = product.rules();

    const core::FurnitureComponent* selected =
        options.selection ? product.find(*options.selection) : nullptr;
    if (selected != nullptr && !selected->hasPanel() && selected->kind() != ComponentKind::Product) {
        scene.highlight = selected->box();
    }

    product.visit([&](const core::FurnitureComponent& c) {
        if (!c.hasPanel()) {
            return;
        }
        if (!options.showFronts && isFrontPart(c.kind())) {
            return;
        }
        const bool isSelected = selected != nullptr && (selected == &c || selected->isAncestorOf(c));
        for (const core::Aabb& part : solidParts(c)) {
            SceneBox box;
            box.min = toFloat(part.min());
            box.max = toFloat(part.max());
            box.category = categoryOf(c.kind());
            box.color = isSelected ? glm::mix(colorOf(box.category), kSelectedTint, 0.55f)
                                   : colorOf(box.category);
            box.component = c.id();
            box.selected = isSelected;
            scene.boxes.push_back(box);
        }
        scene.bounds.expand(c.box());

        // Handle: a bar standing out of the front's outer face.
        if (const auto* front = dynamic_cast<const core::FrontPanel*>(&c)) {
            if (const auto handle = front->handleGeometry(rules)) {
                const auto& m = *handle->model;
                const bool alongY = handle->along == core::Axis::Y;
                const glm::vec3 half(static_cast<float>((alongY ? m.width : m.length) / 2.0),
                                     static_cast<float>((alongY ? m.length : m.width) / 2.0), 0.0f);
                const glm::vec3 center = toFloat(handle->center);
                SceneBox bar;
                bar.min = glm::vec3(center.x - half.x, center.y - half.y, center.z);
                bar.max = glm::vec3(center.x + half.x, center.y + half.y,
                                    center.z + static_cast<float>(m.projection));
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
                hole.normal = faceNormal(frame, d.face);
                hole.center = toFloat(frame.toWorld(d.position)) + hole.normal * 0.3f; // no z-fight
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
    return best;
}

} // namespace rc::render
