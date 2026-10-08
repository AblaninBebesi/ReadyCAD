#pragma once

#include "readycad/core/Math.hpp"
#include "readycad/core/Panel.hpp"
#include "readycad/detailing/FlatPart.hpp"
#include "readycad/nesting/Nesting.hpp"

#include <string>
#include <variant>
#include <vector>

/// Stage 8: machining operations, toolpaths and CNC programs.
///
/// Three layers, each replaceable on its own:
///   operations    WHAT is made — holes, grooves, contours — independent of any machine;
///   planner       HOW a machine type makes them — tools, order, depths, travel (toolpaths);
///   postprocessor the dialect of one controller — the text of the program file.
namespace rc::cam {

using core::Real;
using core::Vec2;
using core::Vec3;

// ---------------------------------------------------------------------------------------------
// Operations: what to machine
// ---------------------------------------------------------------------------------------------

/// Faces of a workpiece lying on a machine table: x along its length, y along its width, z up
/// (0 on the bottom face, the thickness on the top face).
enum class Face {
    Top,    ///< z = thickness, machined from above
    Bottom, ///< z = 0: the part has to be turned over
    Left,   ///< x = min
    Right,  ///< x = max
    Front,  ///< y = min
    Back,   ///< y = max
};

/// Outward normal of an edge face in plan: Left (-1, 0), Right (1, 0), Front (0, -1),
/// Back (0, 1); zero for Top and Bottom.
[[nodiscard]] Vec2 planNormal(Face face) noexcept;
/// The edge face whose outward normal is closest to `normal`.
[[nodiscard]] Face edgeFace(Vec2 normal) noexcept;
/// Direction into the material.
[[nodiscard]] Vec3 inward(Face face) noexcept;

/// A hole, drilled from `face` into the material.
struct Drilling {
    Face face = Face::Top;
    Vec3 entry{0.0}; ///< centre of the hole where it enters the face
    Real diameter = 0.0;
    Real depth = 0.0;
    core::FastenerType fastener = core::FastenerType::Dowel;
};

/// A straight groove in a big face (back panel grooves).
struct Groove {
    Face face = Face::Top; ///< Top or Bottom
    Vec2 start{0.0};       ///< centre line in plan
    Vec2 end{0.0};
    Real width = 0.0;
    Real depth = 0.0;
};

/// Cutting a part out along its outline, through the whole workpiece.
struct Contour {
    nesting::Polygon outline; ///< counter-clockwise, in plan; the tool stays outside
};

using Operation = std::variant<Drilling, Groove, Contour>;

/// Everything to machine on one part. In a part's own frame (FlatPart: x along the length, y
/// along the width, z from the bottom face to its view face) or, once placed, in sheet
/// coordinates.
struct PartMachining {
    std::string label; ///< "3": the cut list number
    std::string name;  ///< "Raf"
    Real thickness = 0.0;
    std::vector<Operation> operations;
};

/// All machining of a part, machine-independent: holes in both faces and the edges, grooves,
/// and the contour of `blank` (the sawn blank to cut out; usually FlatPart::cut).
[[nodiscard]] PartMachining partMachining(const detailing::FlatPart& part,
                                          const nesting::Polygon& blank, std::string label,
                                          std::string name);

/// The operation moved like a part on a sheet: turned in plan by quarter turns, then moved;
/// z and the depths stay.
[[nodiscard]] Operation transformed(const Operation& operation, const nesting::Transform& t);

// ---------------------------------------------------------------------------------------------
// Sheet jobs: one sheet of a cutting plan for a CNC nesting centre
// ---------------------------------------------------------------------------------------------

struct SheetJob {
    std::string name;     ///< "02_Suntalam_18_2800x2100_x2": also the program's file name
    std::string material; ///< "Suntalam 18"
    Real length = 0.0;
    Real width = 0.0;
    Real thickness = 0.0;
    int count = 1;          ///< identical sheets made with this program
    Real partSpacing = 0.0; ///< the nesting kept parts this far apart; 0 = unknown
    std::vector<PartMachining> parts; ///< in sheet coordinates
};

/// The sheet `layout` of a nesting centre result with every part's machining moved to its
/// place. Parts of the cut list bring their holes and grooves (FlatPart of `parts`); other
/// parts only their contour. Throws std::out_of_range for a wrong layout index.
[[nodiscard]] SheetJob sheetJob(const nesting::NestingResult& result, std::size_t layout,
                                const detailing::PartSource* parts);

} // namespace rc::cam
