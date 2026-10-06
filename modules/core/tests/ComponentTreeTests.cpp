// Stage 2: the Composite tree itself — structure, adding/removing children, recursive layout.

#include "TestHelpers.hpp"

#include "readycad/core/builders/WardrobeBuilder.hpp"
#include "readycad/core/model/Product.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace rc::core;
using rc::test::IsBox;

namespace {

/// Test double: a composite that records every layout step it performs.
class Spy final : public FurnitureComponent {
public:
    Spy(std::string name, std::vector<std::string>& log)
        : FurnitureComponent(std::move(name)), log_(log)
    {
    }
    ComponentKind kind() const noexcept override { return ComponentKind::Section; }
    bool canAccept(const FurnitureComponent&) const override { return true; }

protected:
    Vec3 updateSize(const LayoutSlot& slot) override
    {
        log_.push_back(name() + ".size");
        return FurnitureComponent::updateSize(slot);
    }
    Vec3 updatePosition(const LayoutSlot& slot, const Vec3& size) override
    {
        log_.push_back(name() + ".position");
        return FurnitureComponent::updatePosition(slot, size);
    }

private:
    std::vector<std::string>& log_;
};

LayoutSlot slotFor(const Vec3& size)
{
    LayoutSlot slot;
    slot.box = Aabb::fromOriginAndSize(Vec3(0.0), size);
    return slot;
}

} // namespace

TEST_CASE("Components get unique ids and start detached", "[core][tree]")
{
    std::vector<std::string> log;
    const auto a = std::make_shared<Spy>("a", log);
    const auto b = std::make_shared<Spy>("b", log);

    CHECK(a->id().valid());
    CHECK(a->id() != b->id());
    CHECK(a->parent() == nullptr);
    CHECK(&a->root() == a.get());
    CHECK(a->children().empty());
    CHECK_FALSE(a->isLaidOut());
}

TEST_CASE("Adding, finding and removing children", "[core][tree]")
{
    std::vector<std::string> log;
    auto root = std::make_shared<Spy>("root", log);
    auto child = std::make_shared<Spy>("child", log);
    auto grandchild = std::make_shared<Spy>("grandchild", log);

    root->add(child);
    child->add(grandchild);

    REQUIRE(root->children().size() == 1);
    CHECK(child->parent() == root.get());
    CHECK(&grandchild->root() == root.get());
    CHECK(root->isAncestorOf(*grandchild));
    CHECK_FALSE(grandchild->isAncestorOf(*root));
    CHECK(root->find(grandchild->id()) == grandchild.get());

    const auto removed = root->remove(child->id());
    CHECK(removed == child);
    CHECK(root->children().empty());
    CHECK(child->parent() == nullptr);
    CHECK(root->find(grandchild->id()) == nullptr);
    CHECK(child->find(grandchild->id()) == grandchild.get()); // subtree moved out intact

    CHECK(root->remove(ComponentId{999999}) == nullptr);
}

TEST_CASE("Invalid tree operations are rejected", "[core][tree]")
{
    std::vector<std::string> log;
    auto root = std::make_shared<Spy>("root", log);
    auto child = std::make_shared<Spy>("child", log);
    root->add(child);

    CHECK_THROWS_AS(root->add(nullptr), std::invalid_argument);
    CHECK_THROWS_AS(child->add(root), std::invalid_argument); // cycle
    CHECK_THROWS_AS(root->add(child), std::logic_error);      // already has a parent

    SECTION("leaf parts accept no children")
    {
        SideWall side(Side::Left);
        CHECK_FALSE(side.canAccept(Shelf{}));
    }

    SECTION("a product accepts exactly one carcass and nothing else")
    {
        auto product = std::make_shared<Product>();
        CHECK_THROWS_AS(product->add(std::make_shared<Shelf>()), std::invalid_argument);
        product->add(std::make_shared<Carcass>());
        CHECK_THROWS_AS(product->add(std::make_shared<Carcass>()), std::invalid_argument);
    }
}

TEST_CASE("Layout recurses from parents to children in pre-order", "[core][tree][layout]")
{
    std::vector<std::string> log;
    auto root = std::make_shared<Spy>("root", log);
    auto a = std::make_shared<Spy>("a", log);
    auto b = std::make_shared<Spy>("b", log);
    auto a1 = std::make_shared<Spy>("a1", log);
    root->add(a);
    root->add(b);
    a->add(a1);
    CHECK(log.empty()); // nothing laid out yet, so adding does not trigger layout

    root->layout(slotFor(Vec3(800, 2000, 560)));

    const std::vector<std::string> expected = {
        "root.size", "root.position", "a.size", "a.position",
        "a1.size",   "a1.position",   "b.size", "b.position",
    };
    CHECK(log == expected);

    // The default composite gives every child its own box as the slot.
    CHECK_THAT(a1->box(), IsBox({0, 0, 0}, {800, 2000, 560}));
    CHECK(a1->isLaidOut());
}

TEST_CASE("Changing a laid-out tree re-lays it out from the root", "[core][tree][layout]")
{
    std::vector<std::string> log;
    auto root = std::make_shared<Spy>("root", log);
    root->layout(slotFor(Vec3(100, 100, 100)));
    log.clear();

    auto late = std::make_shared<Spy>("late", log);
    root->add(late);

    CHECK(late->isLaidOut());
    CHECK(log.front() == "root.size");
    CHECK_THAT(late->box(), IsBox({0, 0, 0}, {100, 100, 100}));
}

TEST_CASE("A child that outlives its parent has no dangling parent pointer", "[core][tree]")
{
    std::vector<std::string> log;
    auto child = std::make_shared<Spy>("child", log);
    {
        auto parent = std::make_shared<Spy>("parent", log);
        parent->add(child);
    }
    CHECK(child->parent() == nullptr);
    CHECK_NOTHROW(child->requestLayout());
}

TEST_CASE("Only physical parts own a panel, at most one each", "[core][tree][panel]")
{
    const auto wardrobe = WardrobeBuilder().build();
    Carcass& carcass = *wardrobe->carcass();

    CHECK_FALSE(wardrobe->hasPanel());
    CHECK_FALSE(carcass.hasPanel());
    CHECK_FALSE(carcass.interior().hasPanel());
    CHECK(carcass.leftSide().hasPanel());
    CHECK(carcass.leftSide().panel()->material().thickness == 18.0);
    CHECK(carcass.back().panel()->material().type == MaterialType::HDF);
}

TEST_CASE("visit and findAll walk the whole tree", "[core][tree]")
{
    const auto wardrobe = WardrobeBuilder().shelvesPerSection(3).doorsPerSection(2).build();

    int nodes = 0;
    std::as_const(*wardrobe).visit([&nodes](const FurnitureComponent&) { ++nodes; });
    // product + carcass + 5 carcass parts + interior section + 3 shelves + 2 doors
    CHECK(nodes == 13);

    CHECK(findAll<Shelf>(*wardrobe).size() == 3);
    CHECK(findAll<Door>(*wardrobe).size() == 2);
    CHECK(findAll<SideWall>(*wardrobe).size() == 2);
}
