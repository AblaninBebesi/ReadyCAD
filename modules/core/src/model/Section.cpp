#include "readycad/core/model/Section.hpp"

#include "readycad/core/model/Drawer.hpp"

#include <algorithm>
#include <stdexcept>

namespace rc::core {

namespace {

bool isSeparator(ComponentKind kind) noexcept
{
    return kind == ComponentKind::Partition || kind == ComponentKind::FixedShelf;
}

template <typename T>
std::vector<T*> childrenOfKind(const std::vector<FurnitureComponent::Ptr>& children,
                               ComponentKind kind)
{
    std::vector<T*> result;
    for (const auto& child : children) {
        if (child->kind() == kind) {
            result.push_back(static_cast<T*>(child.get()));
        }
    }
    return result;
}

/// Box equal to `box` except that along `axis` it spans [from, to].
Aabb sliceAlong(const Aabb& box, Axis axis, Real from, Real to)
{
    Vec3 lo = box.min();
    Vec3 hi = box.max();
    lo[index(axis)] = from;
    hi[index(axis)] = to;
    return Aabb(lo, hi);
}

} // namespace

Section::Section() = default;

void Section::setFixedSize(std::optional<Real> size)
{
    if (size && *size < 0.0) {
        throw std::invalid_argument("Section: fixed size must be >= 0");
    }
    fixedSize_ = size;
    requestLayout();
}

void Section::setSplitAxis(Axis axis)
{
    if (axis == Axis::Z) {
        throw std::invalid_argument("Section: can only be split along X or Y");
    }
    if (isSplit() && axis != splitAxis_) {
        throw std::logic_error("Section: remove sub-sections before changing the split axis");
    }
    splitAxis_ = axis;
}

bool Section::isSplit() const
{
    return std::any_of(children().begin(), children().end(), [](const Ptr& c) {
        return c->kind() == ComponentKind::Section || isSeparator(c->kind());
    });
}

std::vector<std::shared_ptr<Section>> Section::split(Axis axis, int count)
{
    if (count < 1) {
        throw std::invalid_argument("Section: split count must be >= 1");
    }
    if (isSplit() || !shelves().empty() || drawerStack() != nullptr) {
        throw std::logic_error("Section: only an empty section can be split");
    }
    splitAxis_ = axis;
    std::vector<std::shared_ptr<Section>> sections;
    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            if (axis == Axis::X) {
                adopt(std::make_shared<Partition>());
            } else {
                adopt(std::make_shared<FixedShelf>());
            }
        }
        auto section = std::make_shared<Section>();
        adopt(section);
        sections.push_back(std::move(section));
    }
    requestLayout();
    return sections;
}

std::vector<std::shared_ptr<Section>> Section::splitHorizontally(int columns)
{
    return split(Axis::X, columns);
}

std::vector<std::shared_ptr<Section>> Section::splitVertically(int rows)
{
    return split(Axis::Y, rows);
}

std::shared_ptr<Shelf> Section::addShelf(std::optional<Real> elevation)
{
    auto shelf = std::make_shared<Shelf>(elevation);
    add(shelf);
    return shelf;
}

void Section::setAutoShelfCount(int count)
{
    if (count < 0) {
        throw std::invalid_argument("Section: shelf count must be >= 0");
    }
    if (count > 0 && (isSplit() || drawerStack() != nullptr)) {
        throw std::logic_error("Section: shelves need a leaf section without drawers");
    }
    detachChildren([](const FurnitureComponent& c) {
        return c.kind() == ComponentKind::Shelf
            && !static_cast<const Shelf&>(c).elevation().has_value();
    });
    for (int i = 0; i < count; ++i) {
        adopt(std::make_shared<Shelf>());
    }
    requestLayout();
}

std::vector<std::shared_ptr<Door>> Section::setDoors(int count)
{
    if (count < 0) {
        throw std::invalid_argument("Section: door count must be >= 0");
    }
    if (count > 0 && drawerStack() != nullptr) {
        throw std::logic_error("Section: a section with drawers cannot have doors");
    }
    detachChildren([](const FurnitureComponent& c) { return c.kind() == ComponentKind::Door; });
    std::vector<std::shared_ptr<Door>> doors;
    for (int i = 0; i < count; ++i) {
        // A pair opens from the middle: the right door is hinged on the right.
        const bool hingeRight = count > 1 && i == count - 1;
        auto door = std::make_shared<Door>(hingeRight ? HingeSide::Right : HingeSide::Left);
        adopt(door);
        doors.push_back(std::move(door));
    }
    requestLayout();
    return doors;
}

std::shared_ptr<DrawerStack> Section::setDrawers(int count)
{
    if (count < 0) {
        throw std::invalid_argument("Section: drawer count must be >= 0");
    }
    if (count > 0 && (isSplit() || !shelves().empty() || !doors().empty())) {
        throw std::logic_error("Section: drawers need a leaf section without shelves and doors");
    }
    detachChildren(
        [](const FurnitureComponent& c) { return c.kind() == ComponentKind::DrawerStack; });
    if (count == 0) {
        requestLayout();
        return nullptr;
    }
    auto stack = std::make_shared<DrawerStack>(count);
    adopt(stack);
    requestLayout();
    return stack;
}

std::vector<Section*> Section::subsections() const
{
    return childrenOfKind<Section>(children(), ComponentKind::Section);
}

std::vector<Shelf*> Section::shelves() const
{
    return childrenOfKind<Shelf>(children(), ComponentKind::Shelf);
}

std::vector<Door*> Section::doors() const
{
    return childrenOfKind<Door>(children(), ComponentKind::Door);
}

DrawerStack* Section::drawerStack() const
{
    const auto stacks = childrenOfKind<DrawerStack>(children(), ComponentKind::DrawerStack);
    return stacks.empty() ? nullptr : stacks.front();
}

bool Section::canAccept(const FurnitureComponent& child) const
{
    const bool hasFill = !shelves().empty() || drawerStack() != nullptr;
    switch (child.kind()) {
    case ComponentKind::Section:
        return !hasFill;
    case ComponentKind::Partition:
        return !hasFill && splitAxis_ == Axis::X;
    case ComponentKind::FixedShelf:
        return !hasFill && splitAxis_ == Axis::Y;
    case ComponentKind::Shelf:
        return !isSplit() && drawerStack() == nullptr;
    case ComponentKind::DrawerStack:
        return !isSplit() && !hasFill && doors().empty();
    case ComponentKind::Door:
        return drawerStack() == nullptr;
    default:
        return false;
    }
}

void Section::layoutChildren(const LayoutSlot& slot)
{
    if (isSplit()) {
        layoutSplit(slot);
    } else {
        layoutShelves(slot);
        if (DrawerStack* stack = drawerStack()) {
            LayoutSlot stackSlot = slot.withBox(box());
            stackSlot.overlays = slot.overlays;
            stack->layout(stackSlot);
        }
    }
    layoutFronts(slot);
}

void Section::layoutSplit(const LayoutSlot& slot)
{
    const Axis axis = splitAxis_;
    const int ai = index(axis);
    const Real gap = slot.r().frontGap;

    std::vector<FurnitureComponent*> items;
    for (const Ptr& child : children()) {
        if (child->kind() == ComponentKind::Section || isSeparator(child->kind())) {
            items.push_back(child.get());
        }
    }

    auto separatorThickness = [&slot](const FurnitureComponent* item) {
        return static_cast<const PanelComponent*>(item)->thickness(slot);
    };

    // 1. Free space = extent along the axis minus all separators.
    Real separators = 0.0;
    Real fixedTotal = 0.0;
    int autoCount = 0;
    for (const FurnitureComponent* item : items) {
        if (isSeparator(item->kind())) {
            separators += separatorThickness(item);
        } else if (const auto& fixed = static_cast<const Section*>(item)->fixedSize()) {
            fixedTotal += *fixed;
        } else {
            ++autoCount;
        }
    }
    const Real available = std::max(0.0, box().size()[ai] - separators);

    // 2. Fixed sections keep their size; automatic ones share the rest equally. If the fixed
    //    sizes do not fit (or nothing is automatic), fixed sizes are scaled to fill the space.
    Real fixedScale = 1.0;
    Real autoSize = 0.0;
    if (autoCount > 0) {
        if (fixedTotal > available) {
            fixedScale = available / fixedTotal;
        } else {
            autoSize = (available - fixedTotal) / autoCount;
        }
    } else if (fixedTotal > 0.0) {
        fixedScale = available / fixedTotal;
    }

    // 3. Front overlay at a boundary: half of the shared separator minus half the gap.
    auto overlayTowards = [&](const FurnitureComponent* neighbour) {
        const Real shared = isSeparator(neighbour->kind()) ? separatorThickness(neighbour) : 0.0;
        return (shared - gap) / 2.0;
    };

    Real cursor = box().min()[ai];
    for (std::size_t i = 0; i < items.size(); ++i) {
        FurnitureComponent* item = items[i];
        if (isSeparator(item->kind())) {
            const Real t = separatorThickness(item);
            item->layout(slot.withBox(sliceAlong(box(), axis, cursor, cursor + t)));
            cursor += t;
            continue;
        }
        const auto& fixed = static_cast<const Section*>(item)->fixedSize();
        const Real extent = fixed ? *fixed * fixedScale : autoSize;

        LayoutSlot childSlot = slot.withBox(sliceAlong(box(), axis, cursor, cursor + extent));
        childSlot.overlays = slot.overlays;
        childSlot.overlays.atMin(axis) =
            i == 0 ? slot.overlays.atMin(axis) : overlayTowards(items[i - 1]);
        childSlot.overlays.atMax(axis) =
            i + 1 == items.size() ? slot.overlays.atMax(axis) : overlayTowards(items[i + 1]);
        item->layout(childSlot);
        cursor += extent;
    }
}

void Section::layoutShelves(const LayoutSlot& slot)
{
    // Shelves are ordered bottom -> top as they appear among the children. Shelves with a fixed
    // elevation are anchors; automatic shelves between two anchors (or the section's bottom/top)
    // are spread so that all clear gaps between them are equal.
    const std::vector<Shelf*> shelves = this->shelves();
    const Real bottom = box().min().y;
    const Real top = box().max().y;

    auto layoutAt = [&](Shelf* shelf, Real y) {
        const Real t = shelf->thickness(slot);
        shelf->layout(slot.withBox(sliceAlong(box(), Axis::Y, y, y + t)));
    };

    Real cursor = bottom; // top face of the previous shelf (or the section bottom)
    std::size_t i = 0;
    while (i < shelves.size()) {
        std::size_t anchor = i;
        while (anchor < shelves.size() && !shelves[anchor]->elevation()) {
            ++anchor;
        }

        Real limit = top;
        if (anchor < shelves.size()) {
            const Real t = shelves[anchor]->thickness(slot);
            limit = std::clamp(bottom + *shelves[anchor]->elevation(), bottom,
                               std::max(bottom, top - t));
        }

        Real thicknessSum = 0.0;
        for (std::size_t k = i; k < anchor; ++k) {
            thicknessSum += shelves[k]->thickness(slot);
        }
        const auto gaps = static_cast<Real>(anchor - i + 1);
        const Real clearGap = (limit - cursor - thicknessSum) / gaps;

        Real y = cursor;
        for (std::size_t k = i; k < anchor; ++k) {
            y += clearGap;
            layoutAt(shelves[k], y);
            y += shelves[k]->thickness(slot);
        }
        if (anchor < shelves.size()) {
            layoutAt(shelves[anchor], limit);
            cursor = limit + shelves[anchor]->thickness(slot);
        }
        i = anchor + 1;
    }
}

void Section::layoutFronts(const LayoutSlot& slot)
{
    const std::vector<Door*> doors = this->doors();
    if (doors.empty()) {
        return;
    }
    LayoutSlot own = slot.withBox(box());
    own.overlays = slot.overlays;
    const Aabb rect = own.frontRect();

    const Real gap = slot.r().frontGap;
    const auto count = static_cast<Real>(doors.size());
    const Real width = (rect.size().x - (count - 1.0) * gap) / count;

    Real x = rect.min().x;
    for (Door* door : doors) {
        // The door's slot is exactly its rectangle, so no further overlay is needed.
        door->layout(slot.withBox(sliceAlong(rect, Axis::X, x, x + width)));
        x += width + gap;
    }
}

} // namespace rc::core
