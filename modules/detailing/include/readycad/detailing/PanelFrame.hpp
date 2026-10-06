#pragma once

#include "readycad/core/Panel.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"

namespace rc::detailing {

/// The panel's own coordinate system, in which drillings and grooves are stored:
///   x — along the panel length    (world axis Panel::lengthAxis()),
///   y — along the panel width     (world axis Panel::widthAxis()),
///   z — across the thickness      (world axis Panel::thicknessAxis()),
/// origin at the minimum corner of the component's box.
///
/// Note: depending on the axes this frame may be mirrored compared to "looking at the Top face".
/// The CAM module (stage 8) normalises it for each machine.
class PanelFrame {
public:
    /// Throws std::invalid_argument if the component has no panel.
    explicit PanelFrame(const core::FurnitureComponent& component);

    [[nodiscard]] const core::Aabb& box() const noexcept { return box_; }
    [[nodiscard]] core::Axis lengthAxis() const noexcept { return length_; }
    [[nodiscard]] core::Axis widthAxis() const noexcept { return width_; }
    [[nodiscard]] core::Axis thicknessAxis() const noexcept { return thickness_; }

    /// Extents of the box along the panel's axes, mm.
    [[nodiscard]] core::Vec3 extents() const noexcept;

    [[nodiscard]] core::Vec3 toLocal(const core::Vec3& world) const noexcept;
    [[nodiscard]] core::Vec3 toWorld(const core::Vec3& local) const noexcept;

    /// Surface of the panel at the min (atMax = false) or max end of a world axis.
    [[nodiscard]] core::PanelFace faceAt(core::Axis worldAxis, bool atMax) const noexcept;

    /// The Top or Bottom face, whichever looks towards `worldPoint`.
    [[nodiscard]] core::PanelFace faceToward(const core::Vec3& worldPoint) const noexcept;

    /// Moves a local point exactly onto the given surface.
    [[nodiscard]] core::Vec3 projectOnto(core::PanelFace face, core::Vec3 local) const noexcept;

private:
    core::Aabb box_;
    core::Axis length_;
    core::Axis width_;
    core::Axis thickness_;
};

[[nodiscard]] core::PanelFace opposite(core::PanelFace face) noexcept;

} // namespace rc::detailing
