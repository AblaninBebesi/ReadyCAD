#pragma once

#include "readycad/cam/Operations.hpp"
#include "readycad/cam/Postprocessor.hpp"
#include "readycad/cam/Program.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace rc::cam {

/// A program cannot be made: the machine lacks what the sheet needs, or a file cannot be written.
class CamError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// A Ø12 compression router (T1), a Ø4 groove router (T2) and drills for the holes of the
/// default construction: Ø5, Ø7, Ø8, Ø15, Ø35 (T11-T15).
[[nodiscard]] std::vector<Tool> defaultTools();

/// A CNC nesting centre: router spindle(s), a vertical drilling head, the controller's dialect
/// and how it should work. The workshop sets it up once (settings file).
struct NestingCenterProfile {
    std::vector<Tool> tools = defaultTools(); ///< routers and drills
    int contourTool = 1;       ///< the router that cuts the parts out
    Real spoilboardCut = 0.3;  ///< through cuts and holes go this deep into the spoilboard
    Real safeHeight = 20.0;    ///< above the sheet: travel between operations
    Real clearance = 3.0;      ///< above the sheet: plunges and drill cycles start here
    bool climbMilling = true;  ///< outside contours clockwise (spindle turning clockwise)
    Real rapidSpeed = 30000.0; ///< mm/min, for the time estimate
    Real toolChangeSeconds = 8.0;
    GCodeDialect dialect = isoDialect();

    friend bool operator==(const NestingCenterProfile&, const NestingCenterProfile&) = default;
};

[[nodiscard]] NestingCenterProfile defaultProfile();

/// Throws CamError for a profile no program can be made with: tool numbers twice, sizes or
/// speeds <= 0, no router for the contours, negative heights.
void validate(const NestingCenterProfile& profile);

/// Machining of a part the nesting centre leaves to other machines (horizontal drilling,
/// turning the part over) or cannot do with its tools.
struct Leftover {
    std::string label;
    std::string name;
    int edgeHoles = 0;     ///< holes in the edges
    int bottomHoles = 0;   ///< holes in the face lying on the table
    int bottomGrooves = 0;
    int noTool = 0;        ///< no drill or router fits

    [[nodiscard]] int total() const noexcept { return edgeHoles + bottomHoles + bottomGrooves + noTool; }
};

struct SheetProgram {
    Program program;
    std::vector<Leftover> leftovers;
    int milledHoles = 0; ///< holes without a matching drill, milled with a smaller router
    ProgramStats stats;
};

/// The program of one sheet:
///   1. holes from above, drill by drill, each drill's route shortened (nearest neighbour + 2-opt),
///      while the sheet is still in one piece;
///   2. holes without a drill and grooves, with routers;
///   3. contours, small parts first (they hold worst on the vacuum table once free), each
///      starting at the corner nearest to the tool, climb or conventional, in passes.
/// Throws CamError if the contour router is missing, wider than the nesting spacing (it would
/// cut into the neighbours) or too short for the sheet.
[[nodiscard]] SheetProgram planSheet(const SheetJob& job, const NestingCenterProfile& profile);

/// "02_Suntalam_18_2800x2100_x2.nc"
[[nodiscard]] std::string programFileName(const SheetJob& job, const Postprocessor& post);

/// One program file per sheet layout of a nesting centre result in `folder` (created if
/// missing), written by `post`. Throws CamError for a panel saw result or a write error.
std::vector<std::filesystem::path> saveSheetPrograms(const nesting::NestingResult& result,
                                                     const detailing::PartSource* parts,
                                                     const NestingCenterProfile& profile,
                                                     const Postprocessor& post,
                                                     const std::filesystem::path& folder,
                                                     std::vector<SheetProgram>* programs = nullptr);

} // namespace rc::cam
