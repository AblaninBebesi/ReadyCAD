#pragma once

#include "readycad/core/model/Product.hpp"
#include "readycad/detailing/CutList.hpp"
#include "readycad/nesting/Geometry.hpp"
#include "readycad/nesting/Stock.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace rc::nesting {

// ---------------------------------------------------------------------------------------------
// Machines and settings
// ---------------------------------------------------------------------------------------------

/// The two machines that cut sheets into parts in a furniture workshop.
enum class Machine {
    /// Panel saw (пильный центр): straight cuts right through the piece (guillotine cuts),
    /// several identical sheets cut at once as a stack. Rectangles only.
    PanelSaw,
    /// CNC nesting centre: a router mills any outline through the sheet into a sacrificial
    /// spoilboard; parts are spaced by the cutter diameter. Drills the faces in the same set-up.
    NestingCenter,
};

/// Direction of the first (stage 1) saw cuts: they divide the sheet into strips.
enum class CutDirection {
    AlongLength, ///< strips as long as the sheet (rip cuts first)
    AlongWidth,  ///< strips across the sheet
    Auto,        ///< tries both, keeps the better layout
};

struct PanelSawSettings {
    Real kerf = 4.4;   ///< saw blade width: lost in every cut
    Real trim = 10.0;  ///< cut off every edge of a full sheet (chipped factory edges)
    int maxStages = 3; ///< 2: strips and pieces; 3: + recuts across a piece; 4: + trims along
    Real maxStackHeight = 60.0; ///< identical sheets are cut as one stack up to this height
    CutDirection firstCuts = CutDirection::AlongLength;

    friend bool operator==(const PanelSawSettings&, const PanelSawSettings&) = default;
};

struct NestingCenterSettings {
    Real toolDiameter = 12.0; ///< contour cutter: neighbouring parts at least this far apart
    Real extraSpacing = 0.0;  ///< added to the tool diameter between parts
    Real margin = 10.0;       ///< from the sheet edges to the parts

    [[nodiscard]] Real spacing() const noexcept { return toolDiameter + extraSpacing; }

    friend bool operator==(const NestingCenterSettings&, const NestingCenterSettings&) = default;
};

/// Genetic algorithm, as in deepnest: an individual is the order in which parts are placed plus
/// the rotation of every part; a greedy placement turns it into a layout whose cost is its
/// fitness. More individuals and generations: better layouts, longer runs.
struct OptimizerSettings {
    int populationSize = 24;
    int generations = 40;
    Real mutationRate = 0.10; ///< per gene: chance to swap with the next part / to rotate
    int elites = 2;           ///< best individuals copied unchanged into the next generation
    int threads = 0;          ///< threads evaluating individuals; 0 = all cores but one
    int memoryLimitMb = 256;  ///< for the No-Fit Polygon cache; beyond it NFPs are recomputed
    std::uint64_t seed = 1;   ///< same seed and input: same result, on any number of threads

    friend bool operator==(const OptimizerSettings&, const OptimizerSettings&) = default;
};

struct NestingSettings {
    Machine machine = Machine::PanelSaw;
    PanelSawSettings saw;
    NestingCenterSettings router;
    OptimizerSettings optimizer;

    friend bool operator==(const NestingSettings&, const NestingSettings&) = default;
};

/// Throws std::invalid_argument for impossible settings (negative kerf, population < 2, ...).
void validate(const NestingSettings& settings);

/// Threads the optimiser really uses for a `threads` setting (0 = all cores but one).
[[nodiscard]] int effectiveThreads(int threads) noexcept;

/// How many sheets of this thickness the panel saw cuts in one stack (at least 1).
[[nodiscard]] int sheetsPerStack(const PanelSawSettings& saw, Real thickness) noexcept;

// ---------------------------------------------------------------------------------------------
// Job
// ---------------------------------------------------------------------------------------------

/// A part to cut, `quantity` times.
struct NestingPart {
    std::string label; ///< written on the layout, e.g. the cut list number "3"
    core::Material material;
    Polygon outline; ///< contour to cut, in the part's own coordinates
    int quantity = 1;
    bool keepGrain = false; ///< decor with a direction: turned by 0° or 180° only
    std::optional<std::size_t> cutListRow; ///< the cut list row it comes from
};

/// One part per cut list row, `productCount` times its quantity. The outline is the sawn blank
/// (cut size) in FlatPart coordinates: origin at the lower-left corner of the finished part, seen
/// from the face with more machining. A CNC drilling from above can then take the holes of the
/// FlatPart straight onto the sheet with the same transform.
[[nodiscard]] std::vector<NestingPart>
partsFromCutList(const core::Product& product, const std::vector<detailing::CutListRow>& cutList,
                 int productCount = 1);

// ---------------------------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------------------------

struct PlacedPart {
    std::size_t part = 0; ///< index into NestingResult::parts
    int copy = 0;         ///< which of its `quantity` pieces
    Transform transform;  ///< part outline -> sheet
};

/// One straight saw cut in sheet coordinates. SheetLayout::cuts lists them in the order the
/// saw makes them.
struct SawCut {
    int stage = 1; ///< 0 trimming the sheet, 1 strips, 2 pieces, 3 recuts, 4 trimming recuts
    Vec2 from{0.0};
    Vec2 to{0.0};
};

/// How one sheet is cut.
struct SheetLayout {
    Sheet sheet; ///< a copy of what is cut: material, size, stock id (0 for a sheet to buy)
    SheetSource source = SheetSource::Stock;
    int count = 1; ///< identical sheets cut with this layout
    Rect usable;   ///< where parts may lie: inside the trims (saw) or the margins (nesting)
    std::vector<PlacedPart> parts;
    std::vector<Rect> remnants; ///< reusable offcuts of one sheet, sheet coordinates
    std::vector<SawCut> cuts;   ///< panel saw only
    Real partsArea = 0.0;       ///< of one sheet

    [[nodiscard]] Real utilization() const noexcept;
};

/// Sheets of one material a result needs.
struct MaterialUsage {
    core::Material material;
    int remnants = 0; ///< remnants from stock
    int stock = 0;    ///< full sheets from stock
    int purchase = 0; ///< sheets to buy
    Real sheetArea = 0.0;
    Real partsArea = 0.0;
    int newRemnants = 0; ///< reusable offcuts produced

    [[nodiscard]] int sheets() const noexcept { return remnants + stock + purchase; }
    [[nodiscard]] Real utilization() const noexcept;
};

struct NestingResult {
    struct Unplaced {
        std::size_t part = 0;
        int quantity = 0;
    };

    NestingSettings settings; ///< what the layouts were made with: machine, kerf, cutter...
    std::vector<NestingPart> parts;
    std::vector<SheetLayout> layouts; ///< grouped by material, in the order of the parts
    std::vector<Unplaced> unplaced;   ///< larger than every sheet of their material
    int evaluations = 0;              ///< layouts tried by the genetic algorithm

    [[nodiscard]] int sheetCount() const noexcept;
    [[nodiscard]] int placedCount() const noexcept;
    [[nodiscard]] Real utilization() const noexcept; ///< parts area / sheet area, all sheets
    [[nodiscard]] std::vector<MaterialUsage> usage() const;
};

// ---------------------------------------------------------------------------------------------
// Optimisation
// ---------------------------------------------------------------------------------------------

struct NestingProgress {
    std::size_t material = 0; ///< material group being optimised
    std::size_t materials = 0;
    int generation = 0;
    int generations = 0;
    Real utilization = 0.0;     ///< of the best layout so far
    std::size_t cacheBytes = 0; ///< memory held by the No-Fit Polygon cache
};

/// Called after every generation with the best result so far (from the optimising thread).
using ProgressCallback =
    std::function<void(const NestingProgress& progress, const NestingResult& best)>;

/// Lays the parts out on sheets from `stock`, material by material, and improves the layouts
/// with the genetic algorithm. When `stop` is requested it finishes quickly with the best
/// layouts found so far. Throws std::invalid_argument for invalid settings or parts.
[[nodiscard]] NestingResult optimize(const std::vector<NestingPart>& parts,
                                     const StockManager& stock, const NestingSettings& settings,
                                     const ProgressCallback& progress = {},
                                     std::stop_token stop = {});

} // namespace rc::nesting
