#pragma once

#include "readycad/core/model/ComponentKind.hpp"
#include "readycad/core/model/LayoutSlot.hpp"

#include <memory>

namespace rc::core {

enum class BackMountType {
    Groove,  // в паз: HDF slides into grooves of sides, top and bottom (default)
    Overlay, // накладная: HDF nailed/stapled onto the rear edges of the carcass
    Inset,   // вкладная: a thick panel (chipboard) between sides, top and bottom
};

/// Strategy pattern: how the back panel is attached to the carcass.
///
/// The choice changes several formulas at once (back panel size, where the other carcass panels
/// start in depth, which panels get a groove), so it is one object instead of `if`s spread over
/// SideWall/TopPanel/BottomPanel/BackPanel. A new mounting type = one new subclass.
class BackMount {
public:
    virtual ~BackMount() = default;

    [[nodiscard]] virtual BackMountType type() const noexcept = 0;

    /// Offset in Z from the carcass rear at which sides/top/bottom start.
    [[nodiscard]] virtual Real carcassRearOffset(Real backThickness) const noexcept = 0;

    /// Box of the back panel in product coordinates.
    [[nodiscard]] virtual Aabb backPanelBox(const CarcassFrame& frame,
                                            const ConstructionRules& rules,
                                            Real backThickness) const = 0;

    /// True if the back panel legitimately enters that part (it sits in its groove).
    [[nodiscard]] virtual bool isEmbeddedInto(ComponentKind kind) const noexcept = 0;
};

[[nodiscard]] std::unique_ptr<BackMount> makeBackMount(BackMountType type);

} // namespace rc::core
