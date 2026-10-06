#pragma once

#include "readycad/core/model/BackMount.hpp"
#include "readycad/core/model/PanelComponent.hpp"

#include <optional>

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

/// Полка на полкодержателях: narrower than the opening (side gaps), set back from the front
/// and kept off the back panel.
class Shelf final : public PanelComponent {
public:
    /// elevation: fixed distance from the section bottom to the shelf's underside;
    /// nullopt = distributed evenly with the other automatic shelves.
    explicit Shelf(std::optional<Real> elevation = std::nullopt);
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Shelf; }

    [[nodiscard]] const std::optional<Real>& elevation() const noexcept { return elevation_; }
    void setElevation(std::optional<Real> elevation);

protected:
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;

private:
    std::optional<Real> elevation_;
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

class FrontPanel : public PanelComponent {
protected:
    explicit FrontPanel(Axis lengthAxis);
    Vec3 updateSize(const LayoutSlot& slot) override;
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override;
    const Material& defaultMaterial(const ConstructionRules& r) const override;
    EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const override;
};

enum class HingeSide { Left, Right };

/// Накладной распашной фасад (дверь).
class Door final : public FrontPanel {
public:
    explicit Door(HingeSide hinge = HingeSide::Left);
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Door; }
    [[nodiscard]] HingeSide hinge() const noexcept { return hinge_; }
    void setHinge(HingeSide hinge) noexcept { hinge_ = hinge; }

private:
    HingeSide hinge_;
};

/// Фасад ящика.
class DrawerFront final : public FrontPanel {
public:
    DrawerFront();
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::DrawerFront;
    }
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
