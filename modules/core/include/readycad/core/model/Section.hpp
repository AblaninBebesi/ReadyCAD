#pragma once

#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Parts.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace rc::core {

class DrawerStack;

/// Отсек: a rectangular region of a carcass interior. A Section is EITHER
///
///  * split — into sub-sections along X (separated by Partitions) or along Y (separated by
///    FixedShelves). Sub-sections are auto-sized (share the free space equally) or have a fixed
///    size along the split axis; or
///  * a leaf — holding adjustable Shelves or one DrawerStack.
///
/// Any section may also carry Doors that close its whole front.
///
/// This replaces "a class per carcass type" (with shelves / with doors / with drawers): those
/// combinations are compositions of the same parts, so new combinations need no new classes.
class Section final : public FurnitureComponent {
public:
    Section();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Section; }

    // --- Size along the parent's split axis ---------------------------------------------------
    [[nodiscard]] const std::optional<Real>& fixedSize() const noexcept { return fixedSize_; }
    /// nullopt = automatic (share of the free space).
    void setFixedSize(std::optional<Real> size);

    // --- Splitting ----------------------------------------------------------------------------
    [[nodiscard]] Axis splitAxis() const noexcept { return splitAxis_; }
    /// X or Y. Only allowed while the section has no sub-sections or separators.
    void setSplitAxis(Axis axis);
    [[nodiscard]] bool isSplit() const;

    /// Splits into `columns` sub-sections, left to right, with Partitions between them.
    std::vector<std::shared_ptr<Section>> splitHorizontally(int columns);
    /// Splits into `rows` sub-sections, bottom to top, with FixedShelves between them.
    std::vector<std::shared_ptr<Section>> splitVertically(int rows);

    // --- Content ------------------------------------------------------------------------------
    std::shared_ptr<Shelf> addShelf(std::optional<Real> elevation = std::nullopt);
    /// Replaces all automatic shelves with `count` new ones (fixed shelves are kept).
    void setAutoShelfCount(int count);
    /// Replaces doors: 0 = none, 1 = single (hinge left), 2 = pair (hinges outside).
    std::vector<std::shared_ptr<Door>> setDoors(int count);
    /// Replaces the drawer stack: 0 = none.
    std::shared_ptr<DrawerStack> setDrawers(int count);

    [[nodiscard]] std::vector<Section*> subsections() const;
    [[nodiscard]] std::vector<Shelf*> shelves() const;
    [[nodiscard]] std::vector<Door*> doors() const;
    [[nodiscard]] DrawerStack* drawerStack() const;

    [[nodiscard]] bool canAccept(const FurnitureComponent& child) const override;

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    void layoutSplit(const LayoutSlot& slot);
    void layoutShelves(const LayoutSlot& slot);
    void layoutFronts(const LayoutSlot& slot);
    std::vector<std::shared_ptr<Section>> split(Axis axis, int count);

    std::optional<Real> fixedSize_;
    Axis splitAxis_ = Axis::X;
};

} // namespace rc::core
