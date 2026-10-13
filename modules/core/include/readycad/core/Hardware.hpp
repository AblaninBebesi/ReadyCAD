#pragma once

#include "readycad/core/Math.hpp"

#include <string>
#include <vector>

namespace rc::core {

enum class HandleType {
    Bar,  // скоба: two screws at `holeSpacing`
    Knob, // кнопка: one screw
};

/// A handle model from a supplier's catalogue. This is workshop DATA: every workshop enters the
/// handles of its own supplier; furniture refers to them by `id`.
/// Kinds of hardware (фурнитура) in the hardware list of a product.
enum class HardwareKind {
    Confirmat,
    Dowel,
    Eccentric,
    ShelfSupport,
    Hinge,
    Handle,
    DrawerSlide,
    Leg,        // adjustable leg under a base cabinet
    PlinthClip, // holds the plinth on a front leg
    Hanger,     // cabinet hanger of a wall cabinet
};

struct HandleModel {
    std::string id;       // stable key referenced by fronts, e.g. "bar-128"
    std::string name;     // shown in the UI and in the hardware list
    std::string supplier;
    std::string article;  // supplier's article number, for ordering
    HandleType type = HandleType::Bar;
    Real holeSpacing = 128.0; // centre-to-centre (межосевое), usually a multiple of 32; bars only
    Real holeDiameter = 5.0;  // through hole for an M4 screw
    Real length = 148.0;      // overall size along the holes (knob: its diameter)
    Real width = 12.0;        // overall size across
    Real projection = 30.0;   // how far it stands out of the front

    [[nodiscard]] int holeCount() const noexcept { return type == HandleType::Bar ? 2 : 1; }
};

/// A small generic catalogue so that the program works out of the box: bars 96...320 mm and a
/// 30 mm knob. Workshops replace it with their supplier's handles.
[[nodiscard]] std::vector<HandleModel> defaultHandleCatalog();

} // namespace rc::core
