#pragma once

#include "readycad/core/Math.hpp"

#include <string>

namespace rc::core {

enum class MaterialType {
    Chipboard, // laminated chipboard: ЛДСП / TR: suntalam
    MDF,       // МДФ / TR: MDF lam
    HDF,       // ДВП, ХДФ / TR: duralit, HDF
    Plywood,   // фанера / TR: kontrplak
};

/// Sheet material a panel is cut from.
struct Material {
    std::string name;  // shown in cut lists, e.g. "Suntalam 18 Beyaz"
    MaterialType type = MaterialType::Chipboard;
    Real thickness = 18.0;
    bool hasGrain = false; // decor with a direction (wood texture): panels must not be rotated
    std::string decor{};   // id in the workshop's decor catalogue; empty: the look of its kind

    friend bool operator==(const Material&, const Material&) = default;
};

/// Edge banding (кромка) glued to one edge of a panel.
struct EdgeBand {
    Real thickness = 0.0; // 0 = no edge
    std::string name;

    [[nodiscard]] bool present() const noexcept { return thickness > 0.0; }

    friend bool operator==(const EdgeBand&, const EdgeBand&) = default;
};

} // namespace rc::core
