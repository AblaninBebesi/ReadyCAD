// Stage 4: properties that must hold for EVERY product and fastener choice: holes stay inside
// their panels, are not deeper than the material, and never cut into each other.

#include "readycad/core/builders/Catalog.hpp"
#include "readycad/detailing/Detailing.hpp"
#include "readycad/detailing/PanelFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>

using namespace rc::core;
using namespace rc::detailing;

namespace {

constexpr Real kTol = 1e-6;

/// The whole hole (its circle) lies on its surface of the panel.
bool holeFits(const Drilling& d, const Vec3& size)
{
    const Real r = d.diameter / 2.0;
    auto within = [r](Real v, Real extent) { return v - r >= -kTol && v + r <= extent + kTol; };
    switch (d.face) {
    case PanelFace::Top:
    case PanelFace::Bottom:
        return within(d.position.x, size.x) && within(d.position.y, size.y);
    case PanelFace::EdgeL1:
    case PanelFace::EdgeL2:
        return within(d.position.x, size.x) && within(d.position.z, size.z);
    case PanelFace::EdgeW1:
    case PanelFace::EdgeW2:
        return within(d.position.y, size.y) && within(d.position.z, size.z);
    }
    return false;
}

Real depthLimit(const Drilling& d, const Vec3& size)
{
    if (d.isFaceHole()) {
        return size.z;
    }
    return d.face == PanelFace::EdgeL1 || d.face == PanelFace::EdgeL2 ? size.y : size.x;
}

} // namespace

TEST_CASE("Every catalogue product gets valid machining with every fastener",
          "[detailing][sanity]")
{
    const CatalogItem item = GENERATE(from_range(catalogItems().begin(), catalogItems().end()));
    const auto fastener = GENERATE(ConstructionRules::JointFastener::Confirmat,
                                   ConstructionRules::JointFastener::Eccentric,
                                   ConstructionRules::JointFastener::Dowel);
    const bool dowels = GENERATE(false, true);
    CAPTURE(static_cast<int>(item), static_cast<int>(fastener), dowels);

    ConstructionRules rules;
    rules.joints.fastener = fastener;
    rules.joints.alignmentDowels = dowels;
    const auto product = createProduct(item, rules);

    const DetailingReport report = applyDetailing(*product);
    CHECK(report.warnings.empty());
    CHECK(report.joints >= 4);

    for (const PanelComponent* part : findAll<PanelComponent>(*product)) {
        const PanelFrame frame(*part);
        const Vec3 size = frame.extents();
        const auto& drillings = part->panel()->drillings();

        for (const Drilling& d : drillings) {
            INFO(toString(part->kind()) << " hole at (" << d.position.x << ", " << d.position.y
                                        << ", " << d.position.z << ") d=" << d.diameter);
            CHECK(holeFits(d, size));
            CHECK(d.depth > 0.0);
            CHECK(d.depth <= depthLimit(d, size) + kTol);
        }

        // Two holes in the same face must not cut into each other.
        for (std::size_t i = 0; i < drillings.size(); ++i) {
            for (std::size_t j = i + 1; j < drillings.size(); ++j) {
                const Drilling& a = drillings[i];
                const Drilling& b = drillings[j];
                if (a.face != b.face || !a.isFaceHole()) {
                    continue;
                }
                const Real distance =
                    std::hypot(a.position.x - b.position.x, a.position.y - b.position.y);
                INFO(toString(part->kind()) << " holes " << i << " / " << j);
                CHECK(distance >= (a.diameter + b.diameter) / 2.0 - kTol);
            }
        }

        for (const Groove& g : part->panel()->grooves()) {
            CHECK(g.start.x >= -kTol);
            CHECK(g.end.x <= size.x + kTol);
            CHECK(g.depth < size.z);
        }
    }
}
