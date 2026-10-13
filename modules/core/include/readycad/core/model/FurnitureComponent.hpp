#pragma once

#include "readycad/core/Aabb.hpp"
#include "readycad/core/Panel.hpp"
#include "readycad/core/model/ComponentKind.hpp"
#include "readycad/core/model/LayoutSlot.hpp"

#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rc::core {

/// Unique, never reused id of a component (names are for people and may repeat).
struct ComponentId {
    std::uint64_t value = 0;

    [[nodiscard]] bool valid() const noexcept { return value != 0; }
    friend auto operator<=>(const ComponentId&, const ComponentId&) = default;
};

/// Node of the furniture tree — the Composite pattern.
///
/// Ownership: a parent owns its children through shared_ptr; a child knows its parent through a
/// plain non-owning pointer (an owning back-reference would create a cycle and leak).
///
/// Geometry: `box()` is in PRODUCT coordinates (origin = left-bottom-rear corner of the
/// product, mm). Layout is recomputed top-down from the root after every change, so absolute
/// coordinates are simpler than parent-relative ones and cost nothing.
///
/// Layout is a Template Method (see layout()):
///   1. updateSize(slot)            — the component's OWN size formula;
///   2. updatePosition(slot, size)  — its OWN placement formula;
///   3. updatePanel(slot)           — refresh the physical part, if the component has one;
///   4. layoutChildren(slot)        — composites compute slots for children and recurse.
/// Each concrete class overrides only the steps where it has special rules.
class FurnitureComponent {
public:
    using Ptr = std::shared_ptr<FurnitureComponent>;

    virtual ~FurnitureComponent();

    FurnitureComponent(const FurnitureComponent&) = delete;
    FurnitureComponent& operator=(const FurnitureComponent&) = delete;
    FurnitureComponent(FurnitureComponent&&) = delete;
    FurnitureComponent& operator=(FurnitureComponent&&) = delete;

    // --- Identity -----------------------------------------------------------------------------
    [[nodiscard]] ComponentId id() const noexcept { return id_; }
    [[nodiscard]] virtual ComponentKind kind() const noexcept = 0;
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // --- Tree ---------------------------------------------------------------------------------
    [[nodiscard]] FurnitureComponent* parent() const noexcept { return parent_; }
    [[nodiscard]] FurnitureComponent& root() noexcept;
    [[nodiscard]] const FurnitureComponent& root() const noexcept;
    [[nodiscard]] const std::vector<Ptr>& children() const noexcept { return children_; }

    /// Whether this node may own `child` (e.g. a Section accepts Shelves, a SideWall nothing).
    [[nodiscard]] virtual bool canAccept(const FurnitureComponent& child) const;

    /// Appends a child and re-lays out the product.
    /// Throws std::invalid_argument for null, a cycle, or a child this node does not accept,
    /// and std::logic_error if the child already has a parent.
    FurnitureComponent& add(Ptr child);

    /// Detaches a direct child; returns it (or nullptr if not found) and re-lays out the product.
    Ptr remove(ComponentId childId);

    /// Depth-first search in this subtree, including this node.
    [[nodiscard]] FurnitureComponent* find(ComponentId id) noexcept;
    [[nodiscard]] const FurnitureComponent* find(ComponentId id) const noexcept;

    [[nodiscard]] bool isAncestorOf(const FurnitureComponent& other) const noexcept;

    /// Pre-order traversal (parent before children) of this subtree.
    void visit(const std::function<void(FurnitureComponent&)>& fn);
    void visit(const std::function<void(const FurnitureComponent&)>& fn) const;

    // --- Geometry -----------------------------------------------------------------------------
    [[nodiscard]] const Aabb& box() const noexcept { return box_; }
    [[nodiscard]] Size3D size() const noexcept { return Size3D::fromVec3(box_.size()); }
    [[nodiscard]] Vec3 position() const noexcept { return box_.min(); }
    [[nodiscard]] bool isLaidOut() const noexcept { return lastSlot_.has_value(); }

    // --- Layout -------------------------------------------------------------------------------
    /// Recomputes this node and, recursively, its whole subtree inside `slot`.
    void layout(const LayoutSlot& slot);

    /// Re-lays out the whole product from its root (no-op until the root was laid out once).
    /// Called automatically by every mutating method.
    void requestLayout();

    // --- Physical part ------------------------------------------------------------------------
    /// At most one panel per component (composites have none).
    [[nodiscard]] bool hasPanel() const noexcept { return panel_.has_value(); }
    [[nodiscard]] const Panel* panel() const noexcept { return panel_ ? &*panel_ : nullptr; }
    [[nodiscard]] Panel* panel() noexcept { return panel_ ? &*panel_ : nullptr; }

    /// True if this part may legitimately intersect `other` (e.g. a back panel in a groove).
    [[nodiscard]] virtual bool allowsOverlapWith(const FurnitureComponent& other) const;

protected:
    explicit FurnitureComponent(std::string name = {});

    /// Step 1. Default: take the whole slot.
    virtual Vec3 updateSize(const LayoutSlot& slot);
    /// Step 2. Default: the slot's minimum corner.
    virtual Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size);
    /// Step 3. Default: nothing (no panel).
    virtual void updatePanel(const LayoutSlot& slot);
    /// Step 4. Default: every child gets this component's box as its slot.
    virtual void layoutChildren(const LayoutSlot& slot);

    /// Attaches a child without the canAccept() check and without relayout — for parts a
    /// composite creates itself. Call requestLayout() once after a batch of changes.
    void adopt(Ptr child);
    /// Like adopt(), but at a position among the children (clamped to the end).
    void adoptAt(std::size_t index, Ptr child);
    /// Moves the direct child at `from` to position `to` (both clamped), without relayout.
    void moveChild(std::size_t from, std::size_t to);

    /// Detaches all direct children matching `predicate`, without relayout.
    std::vector<Ptr> detachChildren(const std::function<bool(const FurnitureComponent&)>& predicate);

    std::optional<Panel> panel_;

private:
    ComponentId id_;
    std::string name_;
    FurnitureComponent* parent_ = nullptr;
    std::vector<Ptr> children_;
    Aabb box_;
    std::optional<LayoutSlot> lastSlot_;
};

/// Typed search helpers.
template <typename T>
[[nodiscard]] std::vector<T*> findAll(FurnitureComponent& root)
{
    std::vector<T*> result;
    root.visit([&result](FurnitureComponent& c) {
        if (auto* typed = dynamic_cast<T*>(&c)) {
            result.push_back(typed);
        }
    });
    return result;
}

template <typename T>
[[nodiscard]] std::vector<const T*> findAll(const FurnitureComponent& root)
{
    std::vector<const T*> result;
    root.visit([&result](const FurnitureComponent& c) {
        if (const auto* typed = dynamic_cast<const T*>(&c)) {
            result.push_back(typed);
        }
    });
    return result;
}

} // namespace rc::core
