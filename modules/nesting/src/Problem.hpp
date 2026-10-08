#pragma once

// What the decoders and the genetic algorithm share: one material group of a job.

#include "readycad/nesting/Nesting.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

namespace rc::nesting::detail {

/// One physical piece: copy `copy` of part `part`.
struct Piece {
    std::size_t part = 0; ///< index into Problem::parts
    int copy = 0;
};

/// The parts of one material and the sheets they may be cut from.
struct Problem {
    const std::vector<NestingPart>* parts = nullptr; ///< all parts of the job
    std::vector<Piece> pieces;
    std::vector<std::vector<int>> turns; ///< allowed quarter turns, per part (empty: other group)
    std::vector<SheetOffer> offers;
    RemnantRules remnantRules;
    NestingSettings settings;

    [[nodiscard]] const NestingPart& part(std::size_t piece) const
    {
        return (*parts)[pieces[piece].part];
    }
};

/// Allowed quarter turns of a part on a machine. Rectangles look the same after 180°, so they
/// need 0° and 90° at most; the grain of a decor forbids 90° and 270°.
[[nodiscard]] std::vector<int> allowedTurns(const NestingPart& part, Machine machine);

/// Where parts may lie on a sheet: inside the trims of a full sheet on the saw (a remnant was
/// already cut clean), inside the margins on the nesting centre.
[[nodiscard]] Rect usableArea(const Sheet& sheet, SheetSource source,
                              const NestingSettings& settings) noexcept;

/// An individual of the genetic algorithm: in which order the pieces are placed and how each
/// is turned (an index into Problem::turns of its part).
struct Genome {
    std::vector<int> order;
    std::vector<int> turn;

    friend bool operator==(const Genome&, const Genome&) = default;
    friend auto operator<=>(const Genome&, const Genome&) = default;
};

/// Lower is better, compared in this order: pieces left over, sheet area used (mm², whole
/// numbers so that sums in a different order compare equal), and how far the parts reach into
/// each sheet (the rest of the last sheet stays one large remnant).
struct Cost {
    int unplaced = 0;
    std::int64_t sheetArea = 0;
    Real compactness = 0.0;

    friend bool operator<(const Cost& a, const Cost& b) noexcept
    {
        return std::tie(a.unplaced, a.sheetArea, a.compactness)
             < std::tie(b.unplaced, b.sheetArea, b.compactness);
    }
};

struct Decoded {
    std::vector<SheetLayout> sheets; ///< one per physical sheet (count = 1)
    std::vector<std::size_t> unplaced; ///< pieces that found no sheet
    Cost cost;
};

/// Turns a genome into a layout greedily: every piece, in genome order, goes to the first sheet
/// where it fits, at the best position there. Must be thread-safe: the genetic algorithm decodes
/// several genomes at once.
class Decoder {
public:
    virtual ~Decoder() = default;
    [[nodiscard]] virtual Decoded decode(const Genome& genome) const = 0;
    [[nodiscard]] virtual std::size_t cacheBytes() const noexcept { return 0; }
};

/// Panel saw: guillotine strips, pieces and recuts (rectangles; other outlines by their box).
[[nodiscard]] std::unique_ptr<Decoder> makeGuillotineDecoder(const Problem& problem);
/// Nesting centre: true shapes placed with No-Fit Polygons, spaced by the cutter.
[[nodiscard]] std::unique_ptr<Decoder> makeFreeDecoder(const Problem& problem);

} // namespace rc::nesting::detail
