#pragma once

#include "readycad/cam/Operations.hpp"

#include <string>
#include <variant>
#include <vector>

namespace rc::cam {

// ---------------------------------------------------------------------------------------------
// Tools
// ---------------------------------------------------------------------------------------------

enum class ToolKind {
    Router, ///< spindle with a cutter: contours, grooves, holes it is smaller than
    Drill,  ///< vertical drill of the drilling head: one diameter, straight down
};

struct Tool {
    int number = 1; ///< T-number in the machine's tool table
    ToolKind kind = ToolKind::Router;
    Real diameter = 12.0;
    Real cuttingLength = 40.0; ///< deepest the tool can cut
    int rpm = 18000;
    Real feed = 12000.0;     ///< mm/min along the path
    Real plungeFeed = 3000.0; ///< mm/min downwards
    Real stepDown = 25.0;    ///< routers: deepest single pass
    std::string name;

    friend bool operator==(const Tool&, const Tool&) = default;
};

// ---------------------------------------------------------------------------------------------
// Programs: toolpaths, still independent of the controller
// ---------------------------------------------------------------------------------------------

struct Comment {
    std::string text;
};

/// Load a tool and start its spindle.
struct ToolChange {
    Tool tool;
};

/// Move at full speed, not cutting (G0).
struct Rapid {
    Vec3 to{0.0};
};

/// Move cutting at `feed` mm/min (G1); straight down = plunge.
struct Feed {
    Vec3 to{0.0};
    Real feed = 0.0;
};

/// One hole of the drilling head: move above it at the clearance height, drill down to
/// `bottom`, come back to `clearance` (G81).
struct DrillCycle {
    Vec2 at{0.0};
    Real clearance = 0.0;
    Real bottom = 0.0;
    Real feed = 0.0;
};

using Step = std::variant<Comment, ToolChange, Rapid, Feed, DrillCycle>;

/// What a machine does, step by step. Coordinates: workpiece, mm, z = 0 on the table (the bottom
/// of the sheet); the postprocessor maps them to the controller's zero.
struct Program {
    std::string name;
    Real length = 0.0; ///< workpiece
    Real width = 0.0;
    Real thickness = 0.0;
    std::vector<Step> steps;
};

struct ProgramStats {
    Real rapidLength = 0.0; ///< mm travelled without cutting
    Real cutLength = 0.0;   ///< mm travelled cutting (plunges included)
    int holes = 0;          ///< drill cycles
    int toolChanges = 0;
    Real seconds = 0.0;     ///< estimated machining time
};

/// Lengths, counts and an estimated time: cutting moves at their feed, rapid moves at
/// `rapidSpeed` mm/min, `toolChangeSeconds` per tool change.
[[nodiscard]] ProgramStats analyse(const Program& program, Real rapidSpeed,
                                   Real toolChangeSeconds);

} // namespace rc::cam
