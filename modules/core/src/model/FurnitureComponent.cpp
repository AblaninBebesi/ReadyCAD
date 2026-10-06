#include "readycad/core/model/FurnitureComponent.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <utility>

namespace rc::core {

namespace {

ComponentId nextId() noexcept
{
    static std::atomic<std::uint64_t> counter{0};
    return ComponentId{++counter};
}

} // namespace

FurnitureComponent::FurnitureComponent(std::string name) : id_(nextId()), name_(std::move(name)) {}

FurnitureComponent::~FurnitureComponent()
{
    // Children may outlive us if someone else still holds them: do not leave dangling pointers.
    for (const Ptr& child : children_) {
        child->parent_ = nullptr;
        child->lastSlot_.reset();
    }
}

FurnitureComponent& FurnitureComponent::root() noexcept
{
    FurnitureComponent* node = this;
    while (node->parent_ != nullptr) {
        node = node->parent_;
    }
    return *node;
}

const FurnitureComponent& FurnitureComponent::root() const noexcept
{
    const FurnitureComponent* node = this;
    while (node->parent_ != nullptr) {
        node = node->parent_;
    }
    return *node;
}

bool FurnitureComponent::canAccept(const FurnitureComponent&) const
{
    return false;
}

FurnitureComponent& FurnitureComponent::add(Ptr child)
{
    if (!child) {
        throw std::invalid_argument("add: child is null");
    }
    if (child.get() == this || child->isAncestorOf(*this)) {
        throw std::invalid_argument("add: would create a cycle in the furniture tree");
    }
    if (child->parent_ != nullptr) {
        throw std::logic_error("add: child already has a parent, remove it first");
    }
    if (!canAccept(*child)) {
        throw std::invalid_argument(std::string("add: ") + std::string(toString(kind()))
                                    + " does not accept " + std::string(toString(child->kind())));
    }
    FurnitureComponent& added = *child;
    adopt(std::move(child));
    requestLayout();
    return added;
}

void FurnitureComponent::adopt(Ptr child)
{
    child->parent_ = this;
    children_.push_back(std::move(child));
}

std::vector<FurnitureComponent::Ptr> FurnitureComponent::detachChildren(
    const std::function<bool(const FurnitureComponent&)>& predicate)
{
    std::vector<Ptr> detached;
    std::erase_if(children_, [&](const Ptr& child) {
        if (!predicate(*child)) {
            return false;
        }
        child->parent_ = nullptr;
        child->lastSlot_.reset();
        detached.push_back(child);
        return true;
    });
    return detached;
}

FurnitureComponent::Ptr FurnitureComponent::remove(ComponentId childId)
{
    auto detached =
        detachChildren([childId](const FurnitureComponent& c) { return c.id() == childId; });
    if (detached.empty()) {
        return nullptr;
    }
    requestLayout();
    return detached.front();
}

FurnitureComponent* FurnitureComponent::find(ComponentId id) noexcept
{
    if (id_ == id) {
        return this;
    }
    for (const Ptr& child : children_) {
        if (FurnitureComponent* found = child->find(id)) {
            return found;
        }
    }
    return nullptr;
}

const FurnitureComponent* FurnitureComponent::find(ComponentId id) const noexcept
{
    return const_cast<FurnitureComponent*>(this)->find(id);
}

bool FurnitureComponent::isAncestorOf(const FurnitureComponent& other) const noexcept
{
    for (const FurnitureComponent* node = other.parent_; node != nullptr; node = node->parent_) {
        if (node == this) {
            return true;
        }
    }
    return false;
}

void FurnitureComponent::visit(const std::function<void(FurnitureComponent&)>& fn)
{
    fn(*this);
    for (const Ptr& child : children_) {
        child->visit(fn);
    }
}

void FurnitureComponent::visit(const std::function<void(const FurnitureComponent&)>& fn) const
{
    fn(*this);
    for (const Ptr& child : children_) {
        std::as_const(*child).visit(fn);
    }
}

void FurnitureComponent::layout(const LayoutSlot& slot)
{
    lastSlot_ = slot;
    const Vec3 newSize = updateSize(slot);
    const Vec3 newPosition = updatePosition(slot, newSize);
    box_ = Aabb::fromOriginAndSize(newPosition, glm::max(newSize, Vec3(0.0)));
    updatePanel(slot);
    layoutChildren(slot);
}

void FurnitureComponent::requestLayout()
{
    FurnitureComponent& top = root();
    if (top.lastSlot_) {
        const LayoutSlot slot = *top.lastSlot_; // copy: layout() overwrites lastSlot_
        top.layout(slot);
    }
}

bool FurnitureComponent::allowsOverlapWith(const FurnitureComponent&) const
{
    return false;
}

Vec3 FurnitureComponent::updateSize(const LayoutSlot& slot)
{
    return slot.box.size();
}

Vec3 FurnitureComponent::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

void FurnitureComponent::updatePanel(const LayoutSlot&) {}

void FurnitureComponent::layoutChildren(const LayoutSlot& slot)
{
    for (const Ptr& child : children_) {
        child->layout(slot.withBox(box_));
    }
}

} // namespace rc::core
