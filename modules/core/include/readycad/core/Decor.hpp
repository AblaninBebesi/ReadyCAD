#pragma once

#include "readycad/core/Material.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rc::core {

/// Desen: the look of a board's surface — a picture (wood, marble) or a plain colour. Workshop
/// DATA like the handles: every workshop lists the decors of its supplier; materials refer to
/// them by `id`.
struct Decor {
    std::string id;      ///< stable key referenced by materials, e.g. "wood-beige"
    std::string name;    ///< shown, and added to the name of a material in this decor
    std::string texture; ///< picture in assets/textures (seamless, the grain left to right);
                         ///< empty: a plain colour
    std::uint8_t r = 200; ///< the plain colour, or the picture's average colour (what is shown
    std::uint8_t g = 200; ///< while the picture is missing)
    std::uint8_t b = 200;
    bool grain = false; ///< a direction (wood): the grain runs along the parts' length and the
                        ///< cutting plan does not turn them

    friend bool operator==(const Decor&, const Decor&) = default;
};

/// The decors the program comes with: white, two plain colours, four woods, three marbles and
/// raw chipboard. The pictures are CC0 textures from ambientCG (assets/textures/SOURCES.txt).
[[nodiscard]] std::vector<Decor> defaultDecorCatalog();

/// The decor of `id` in `catalog`; nullptr if there is none (or `id` is empty).
[[nodiscard]] const Decor* findDecor(const std::vector<Decor>& catalog, const std::string& id) noexcept;

/// The material in another decor. Its name becomes the material's own name followed by the
/// decor's ("Suntalam 18" -> "Suntalam 18 Ceviz"), the name of a previous decor replaced: boards
/// of different decors are different materials for the cut list, the cutting plan and the
/// stock, which tell materials apart by name. The grain follows the decor. With nullptr the
/// material loses its decor (and keeps its grain).
[[nodiscard]] Material withDecor(const Material& material, const Decor* decor,
                                 const std::vector<Decor>& catalog);

} // namespace rc::core
