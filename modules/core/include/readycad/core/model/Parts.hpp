#pragma once

#include "readycad/core/model/BackMount.hpp"
#include "readycad/core/model/PanelComponent.hpp"

#include <optional>
#include <string>
#include <vector>

namespace rc::core {

enum class Side { Left, Right };

// =============================================================================================
// Carcass parts. They need slot.frame (the enclosing Carcass) and throw std::logic_error
// when laid out anywhere else.
// =============================================================================================

/// Боковина. Full carcass depth; its height depends on whether top/bottom overlay it.
class SideWall final : public PanelComponent {
public:
    explicit SideWall(Side side);
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::SideWall; }
    [[nodiscard]] Side side() const noexcept { return side_; }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;

private:
    Side side_;
};

/// Крышка. Overlay (on top of the sides) or inset (between them).
class TopPanel final : public PanelComponent {
public:
    TopPanel();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::TopPanel; }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;
};

/// Дно. Inset between the sides by default, or overlay under them.
class BottomPanel final : public PanelComponent {
public:
    BottomPanel();
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::BottomPanel;
    }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;
};

/// Задняя стенка. Its geometry is delegated to the carcass's BackMount strategy.
class BackPanel final : public PanelComponent {
public:
    BackPanel();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::BackPanel; }
    [[nodiscard]] bool allowsOverlapWith(const FurnitureComponent& other) const override;

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide, const LayoutSlot&) const override { return {}; }

private:
    [[nodiscard]] Aabb computeBox(const LayoutSlot& slot) const;

    std::optional<BackMountType> mountType_; // remembered for allowsOverlapWith()
};

// =============================================================================================
// Interior parts. The slot is the band a Section reserved for the part.
// =============================================================================================

/// How a shelf is held in place. Changes its size formula now and its drilling in stage 4.
enum class ShelfMount {
    Supports,  // на полкодержателях: removable, narrower than the opening
    Confirmat, // на евровинтах: fixed to the walls, fills the opening width
};

/// Полка внутри отсека.
///  * Supports  — 1 mm side gaps (drops in between the walls), set back from the front edge,
///                kept off the back panel;
///  * Confirmat — tight between the walls, with its own front/back offsets.
/// Unlike FixedShelf, a shelf never divides its section: shelves of both mounts can be mixed
/// in one compartment behind one door.
class Shelf final : public PanelComponent {
public:
    /// elevation: fixed distance from the section bottom to the shelf's underside;
    /// nullopt = distributed evenly with the other automatic shelves.
    explicit Shelf(std::optional<Real> elevation = std::nullopt,
                   ShelfMount mount = ShelfMount::Supports);
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Shelf; }

    [[nodiscard]] const std::optional<Real>& elevation() const noexcept { return elevation_; }
    void setElevation(std::optional<Real> elevation);

    [[nodiscard]] ShelfMount mount() const noexcept { return mount_; }
    void setMount(ShelfMount mount);

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;

private:
    std::optional<Real> elevation_;
    ShelfMount mount_;
};

/// Стационарная полка: structural, fills the opening completely (fixed with confirmats).
/// Divides a Section vertically into sub-sections.
class FixedShelf final : public PanelComponent {
public:
    FixedShelf();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::FixedShelf; }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;
};

/// Вертикальная перегородка (средник). Divides a Section horizontally into sub-sections.
class Partition final : public PanelComponent {
public:
    Partition();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Partition; }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;
};

// =============================================================================================
// Fronts: cover slot.frontRect() (opening + overlays) and stand in front of slot.frontZ.
// =============================================================================================

enum class HandleOrientation { Vertical, Horizontal };
enum class HandleAnchor { Top, Center, Bottom };

/// Where a handle sits on a front. Offsets are measured from the front's edge to the NEAREST
/// screw hole centre.
struct HandlePlacement {
    std::string modelId; // empty = the workshop's default handle for this kind of front
    HandleOrientation orientation = HandleOrientation::Vertical;
    HandleAnchor anchor = HandleAnchor::Center; // along the front's height
    Real edgeOffset = 40.0;   // doors: from the opening edge (opposite the hinges)
    Real anchorOffset = 60.0; // from the top / bottom edge for the Top / Bottom anchors
};

/// A handle resolved against the catalogue and placed on its front.
struct HandleGeometry {
    const HandleModel* model = nullptr;
    Vec3 center{0.0};        // on the outer face, product coordinates
    Axis along = Axis::Y;    // direction from one screw hole to the other
    std::vector<Vec3> holes; // screw hole centres on the outer face
    bool fits = true;        // body and holes are inside the front, holes keep minEdgeDistance
};

class FrontPanel : public PanelComponent {
public:
    /// nullopt = this front has no handle.
    [[nodiscard]] const std::optional<HandlePlacement>& handle() const noexcept { return handle_; }
    void setHandle(std::optional<HandlePlacement> placement);

    /// Catalogue model of the handle; nullptr if there is no handle or its id is unknown.
    [[nodiscard]] const HandleModel* handleModel(const ConstructionRules& rules) const;
    /// Position of the handle and its holes; nullopt if handleModel() is nullptr.
    [[nodiscard]] std::optional<HandleGeometry> handleGeometry(const ConstructionRules& rules) const;

protected:
    FrontPanel(Axis lengthAxis, HandlePlacement defaultPlacement);
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;

    [[nodiscard]] virtual const std::string& defaultHandleId(const ConstructionRules& r) const = 0;
    /// X of the handle centre. Default: the middle of the front.
    [[nodiscard]] virtual Real handleCenterX(const HandlePlacement& placement,
                                             const HandleModel& model) const;

private:
    std::optional<HandlePlacement> handle_;
};

enum class HingeSide { Left, Right };

/// Накладной распашной фасад (дверь). Default handle: vertical, mid-height, 40 mm from the
/// opening edge (the edge opposite the hinges).
class Door final : public FrontPanel {
public:
    explicit Door(HingeSide hinge = HingeSide::Left);
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Door; }
    [[nodiscard]] HingeSide hinge() const noexcept { return hinge_; }
    void setHinge(HingeSide hinge);

protected:
    const std::string& defaultHandleId(const ConstructionRules& r) const override;
    Real handleCenterX(const HandlePlacement& placement, const HandleModel& model) const override;

private:
    HingeSide hinge_;
};

/// Фасад ящика. Default handle: horizontal, in the middle of the front.
class DrawerFront final : public FrontPanel {
public:
    DrawerFront();
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::DrawerFront;
    }

protected:
    const std::string& defaultHandleId(const ConstructionRules& r) const override;
};

// =============================================================================================
// Drawer box parts: the slot box is the outer box of the drawer body.
// =============================================================================================

/// Боковина ящика: full box depth, stands on the bottom panel.
class DrawerBoxSide final : public PanelComponent {
public:
    explicit DrawerBoxSide(Side side);
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::DrawerBoxSide;
    }
    [[nodiscard]] Side side() const noexcept { return side_; }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;

private:
    Side side_;
};

enum class DrawerEnd { Front, Back };

/// Передняя/задняя стенка короба ящика: between the box sides.
class DrawerBoxEnd final : public PanelComponent {
public:
    explicit DrawerBoxEnd(DrawerEnd end);
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::DrawerBoxEnd;
    }
    [[nodiscard]] DrawerEnd end() const noexcept { return end_; }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;

private:
    DrawerEnd end_;
};

/// Дно ящика (HDF), nailed under the whole box.
class DrawerBottom final : public PanelComponent {
public:
    DrawerBottom();
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::DrawerBottom;
    }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide, const LayoutSlot&) const override { return {}; }
};

} // namespace rc::core
