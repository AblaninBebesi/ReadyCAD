// Panel saw: every cut goes straight through the piece it is made in (guillotine cut), so a
// layout is a tree of cuts. Workshops use it in stages, and so does this decoder:
//
//   stage 1  the sheet is cut into STRIPS (along its length by default);
//   stage 2  each strip is cut across into COLUMNS, one per part width;
//   stage 3  a column is cut into the parts STACKED in it (or the part from the waste above);
//   stage 4  a part narrower than its column is trimmed (only with maxStages = 4).
//
// The kerf (blade width) is lost between neighbours. Coordinates run in a "strip frame": u along
// the strips, v across them; for strips across the sheet the frame is the sheet transposed.

#include "Problem.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace rc::nesting::detail {

namespace {

constexpr Real kEps = 1e-6;

struct Item {
    std::size_t piece = 0;
    int turn = 0;
    Real u = 0.0;
    Real v = 0.0;
    Real du = 0.0;
    Real dv = 0.0;
};

struct Column {
    Real u0 = 0.0;
    Real width = 0.0;
    Real vNext = 0.0; ///< where the next stacked part would start
    std::vector<Item> items;
};

struct Strip {
    Real v0 = 0.0;
    Real height = 0.0;
    Real uNext = 0.0; ///< where the next column would start
    std::vector<Column> columns;

    [[nodiscard]] Real top() const noexcept { return v0 + height; }
};

struct SheetState {
    std::size_t offer = 0;
    Real u0 = 0.0; ///< usable area in the strip frame
    Real u1 = 0.0;
    Real v0 = 0.0;
    Real v1 = 0.0;
    Real vNext = 0.0; ///< where the next strip would start
    std::vector<Strip> strips;
};

class GuillotineDecoder final : public Decoder {
public:
    explicit GuillotineDecoder(const Problem& problem) : problem_(problem) {}

    [[nodiscard]] Decoded decode(const Genome& genome) const override
    {
        switch (problem_.settings.saw.firstCuts) {
        case CutDirection::AlongLength: return decodeIn(genome, true);
        case CutDirection::AlongWidth:  return decodeIn(genome, false);
        case CutDirection::Auto:        break;
        }
        Decoded along = decodeIn(genome, true);
        Decoded across = decodeIn(genome, false);
        return across.cost < along.cost ? std::move(across) : std::move(along);
    }

private:
    /// Size of a piece in the strip frame for a quarter turn (0 or 1).
    [[nodiscard]] std::array<Real, 2> size(std::size_t piece, int turn, bool along) const
    {
        const Rect b = transformed(bounds(problem_.part(piece).outline), Transform{turn, {}});
        return along ? std::array{b.width(), b.height()} : std::array{b.height(), b.width()};
    }

    [[nodiscard]] bool stackOn(Column& column, const Strip& strip, Item item) const;
    [[nodiscard]] bool newColumn(Strip& strip, const SheetState& sheet, Item item) const;
    [[nodiscard]] bool newStrip(SheetState& sheet, Item item) const;
    [[nodiscard]] Decoded decodeIn(const Genome& genome, bool along) const;
    void finish(const SheetState& state, bool along, Decoded& out) const;

    const Problem& problem_;
};

bool GuillotineDecoder::stackOn(Column& column, const Strip& strip, Item item) const
{
    const int stages = problem_.settings.saw.maxStages;
    if (stages < 3) {
        return false; // stacking needs a stage-3 cut between the parts
    }
    const bool widthFits = stages >= 4 ? item.du <= column.width + kEps
                                       : std::abs(item.du - column.width) <= kEps;
    if (!widthFits || column.vNext + item.dv > strip.top() + kEps) {
        return false;
    }
    item.u = column.u0;
    item.v = column.vNext;
    column.vNext += item.dv + problem_.settings.saw.kerf;
    column.items.push_back(item);
    return true;
}

bool GuillotineDecoder::newColumn(Strip& strip, const SheetState& sheet, Item item) const
{
    // With 2 stages a part must fill the strip's height: there is no recut to free it.
    const bool heightFits = problem_.settings.saw.maxStages >= 3
                                ? item.dv <= strip.height + kEps
                                : std::abs(item.dv - strip.height) <= kEps;
    if (!heightFits || strip.uNext + item.du > sheet.u1 + kEps) {
        return false;
    }
    const Real kerf = problem_.settings.saw.kerf;
    item.u = strip.uNext;
    item.v = strip.v0;
    Column column{item.u, item.du, item.v + item.dv + kerf, {item}};
    strip.uNext += item.du + kerf;
    strip.columns.push_back(std::move(column));
    return true;
}

bool GuillotineDecoder::newStrip(SheetState& sheet, Item item) const
{
    if (sheet.vNext + item.dv > sheet.v1 + kEps || sheet.u0 + item.du > sheet.u1 + kEps) {
        return false;
    }
    Strip strip{sheet.vNext, item.dv, sheet.u0, {}};
    sheet.vNext += item.dv + problem_.settings.saw.kerf;
    const bool placed = newColumn(strip, sheet, item);
    sheet.strips.push_back(std::move(strip));
    return placed;
}

Decoded GuillotineDecoder::decodeIn(const Genome& genome, bool along) const
{
    const std::vector<SheetOffer>& offers = problem_.offers;
    std::vector<SheetState> sheets;
    std::vector<int> used(offers.size(), 0);
    Decoded out;

    for (const int pieceIndex : genome.order) {
        const auto piece = static_cast<std::size_t>(pieceIndex);
        const std::vector<int>& turns = problem_.turns[problem_.pieces[piece].part];
        // The genome's turn first, the other one (if the grain allows) as a fallback.
        std::vector<int> tries{turns[static_cast<std::size_t>(genome.turn[piece])]};
        for (const int t : turns) {
            if (t % 2 != tries.front() % 2) {
                tries.push_back(t);
                break;
            }
        }
        const auto item = [&](int turn) {
            const auto [du, dv] = size(piece, turn, along);
            return Item{piece, turn, 0.0, 0.0, du, dv};
        };

        bool placed = false;
        // 1. Into a strip that is already there: on top of a column, or as a new column.
        for (const int turn : tries) {
            for (SheetState& sheet : sheets) {
                for (Strip& strip : sheet.strips) {
                    for (Column& column : strip.columns) {
                        if (stackOn(column, strip, item(turn))) {
                            placed = true;
                            break;
                        }
                    }
                    if (placed || newColumn(strip, sheet, item(turn))) {
                        placed = true;
                        break;
                    }
                }
                if (placed) {
                    break;
                }
            }
            if (placed) {
                break;
            }
        }
        // 2. A new strip on a sheet already in use.
        for (std::size_t t = 0; !placed && t < tries.size(); ++t) {
            for (SheetState& sheet : sheets) {
                if (newStrip(sheet, item(tries[t]))) {
                    placed = true;
                    break;
                }
            }
        }
        // 3. A new sheet: remnants first, then full sheets on stock, then sheets to buy.
        for (std::size_t o = 0; !placed && o < offers.size(); ++o) {
            if (offers[o].available != SheetOffer::kUnlimited && used[o] >= offers[o].available) {
                continue;
            }
            const Rect usable = usableArea(offers[o].sheet, offers[o].source, problem_.settings);
            SheetState sheet;
            if (along) {
                sheet = {o, usable.min.x, usable.max.x, usable.min.y, usable.max.y, usable.min.y, {}};
            } else {
                sheet = {o, usable.min.y, usable.max.y, usable.min.x, usable.max.x, usable.min.x, {}};
            }
            for (const int turn : tries) {
                if (newStrip(sheet, item(turn))) {
                    sheets.push_back(std::move(sheet));
                    ++used[o];
                    placed = true;
                    break;
                }
            }
        }
        if (!placed) {
            out.unplaced.push_back(piece);
        }
    }

    for (const SheetState& sheet : sheets) {
        finish(sheet, along, out);
    }
    out.cost.unplaced = static_cast<int>(out.unplaced.size());
    return out;
}

/// Turns a filled sheet into its layout: part transforms, saw cuts in order, remnants.
void GuillotineDecoder::finish(const SheetState& s, bool along, Decoded& out) const
{
    const SheetOffer& offer = problem_.offers[s.offer];
    const auto toSheet = [along](Real u, Real v) { return along ? Vec2(u, v) : Vec2(v, u); };
    const auto rect = [&](Real u0, Real v0, Real u1, Real v1) {
        const Vec2 a = toSheet(u0, v0);
        const Vec2 b = toSheet(u1, v1);
        return Rect{glm::min(a, b), glm::max(a, b)};
    };

    SheetLayout layout;
    layout.sheet = offer.sheet;
    layout.source = offer.source;
    layout.usable = rect(s.u0, s.v0, s.u1, s.v1);
    const auto cut = [&](int stage, Real u0, Real v0, Real u1, Real v1) {
        layout.cuts.push_back({stage, toSheet(u0, v0), toSheet(u1, v1)});
    };

    // Stage 0: trimming the factory edges (a remnant was cut clean before).
    const Real sheetU = along ? offer.sheet.length : offer.sheet.width;
    const Real sheetV = along ? offer.sheet.width : offer.sheet.length;
    if (s.v0 > kEps) {
        cut(0, 0.0, s.v0, sheetU, s.v0);
    }
    if (s.v1 < sheetV - kEps) {
        cut(0, 0.0, s.v1, sheetU, s.v1);
    }
    if (s.u0 > kEps) {
        cut(0, s.u0, s.v0, s.u0, s.v1);
    }
    if (s.u1 < sheetU - kEps) {
        cut(0, s.u1, s.v0, s.u1, s.v1);
    }
    // Stage 1: strips.
    for (const Strip& strip : s.strips) {
        if (strip.top() < s.v1 - kEps) {
            cut(1, s.u0, strip.top(), s.u1, strip.top());
        }
    }
    const RemnantRules& rules = problem_.remnantRules;
    const auto keep = [&](Real u0, Real v0, Real u1, Real v1) {
        if (u1 - u0 > kEps && v1 - v0 > kEps && rules.isUsable(u1 - u0, v1 - v0)) {
            layout.remnants.push_back(rect(u0, v0, u1, v1));
        }
    };
    Real reach = s.v0;
    for (const Strip& strip : s.strips) {
        reach = std::max(reach, strip.top());
        // Stage 2: columns.
        for (const Column& column : strip.columns) {
            if (column.u0 + column.width < s.u1 - kEps) {
                cut(2, column.u0 + column.width, strip.v0, column.u0 + column.width, strip.top());
            }
        }
        for (const Column& column : strip.columns) {
            for (const Item& item : column.items) {
                // Stage 3: between stacked parts / off the waste above.
                if (item.v + item.dv < strip.top() - kEps) {
                    cut(3, column.u0, item.v + item.dv, column.u0 + column.width,
                        item.v + item.dv);
                }
                // Stage 4: a part narrower than its column.
                if (item.du < column.width - kEps) {
                    cut(4, item.u + item.du, item.v, item.u + item.du, item.v + item.dv);
                }
                const Piece& piece = problem_.pieces[item.piece];
                const Polygon& outline = (*problem_.parts)[piece.part].outline;
                const Rect turned = transformed(bounds(outline), Transform{item.turn, {}});
                const Rect at = rect(item.u, item.v, item.u + item.du, item.v + item.dv);
                layout.parts.push_back(
                    {piece.part, piece.copy, Transform{item.turn, at.min - turned.min}});
                layout.partsArea += area(outline);
            }
            keep(column.u0, column.vNext, column.u0 + column.width, strip.top());
        }
        keep(strip.uNext, strip.v0, s.u1, strip.top());
    }
    keep(s.u0, s.vNext, s.u1, s.v1);

    out.cost.sheetArea += std::llround(offer.sheet.area());
    out.cost.compactness += (reach - s.v0) / std::max(kEps, s.v1 - s.v0);
    out.sheets.push_back(std::move(layout));
}

} // namespace

std::unique_ptr<Decoder> makeGuillotineDecoder(const Problem& problem)
{
    return std::make_unique<GuillotineDecoder>(problem);
}

} // namespace rc::nesting::detail
