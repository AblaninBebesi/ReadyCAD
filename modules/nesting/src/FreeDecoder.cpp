// Nesting centre: parts of any shape, placed with No-Fit Polygons (the deepnest approach).
//
// For a piece to place on a sheet:
//   inner fit   = positions where the piece lies inside the sheet's margins (a rectangle);
//   forbidden   = union of the NFPs of the pieces already on the sheet (each moved to its piece);
//   free        = inner fit − forbidden (Clipper difference, exact on integers).
// Every vertex of `free` is a position where the piece touches a neighbour or a margin; the one
// that keeps the parts furthest to the left (then lowest) wins. Placing to the left keeps the
// right end of the sheet free: one large remnant instead of scattered offcuts.

#include "Clip.hpp"
#include "NfpCache.hpp"
#include "Problem.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <tuple>

namespace rc::nesting::detail {

namespace {

using namespace clip;

/// Exactly touching positions must stay inside a free region with an area, or Clipper drops it
/// (a piece that fits a gap exactly would never be put there). NFPs shrink, inner fits grow by
/// this much: a micrometre.
constexpr std::int64_t kTouch = 1;

class FreeDecoder final : public Decoder {
public:
    explicit FreeDecoder(const Problem& problem)
        : problem_(problem)
        , cache_(static_cast<std::size_t>(std::max(0, problem.settings.optimizer.memoryLimitMb))
                 * 1024U * 1024U)
    {
        const Real grow = problem.settings.router.spacing() / 2.0;
        shapes_.resize(problem.parts->size() * 4);
        for (std::size_t part = 0; part < problem.turns.size(); ++part) {
            for (const int turn : problem.turns[part]) {
                shapes_[index(part, turn)] = makeShape((*problem.parts)[part].outline, turn, grow);
            }
        }
        for (const SheetOffer& offer : problem.offers) {
            usable_.push_back(box(usableArea(offer.sheet, offer.source, problem.settings)));
        }
    }

    [[nodiscard]] Decoded decode(const Genome& genome) const override;
    [[nodiscard]] std::size_t cacheBytes() const noexcept override { return cache_.bytes(); }

private:
    struct Placed {
        std::uint32_t shape = 0;
        Point64 offset;
        std::size_t piece = 0;
    };

    struct OpenSheet {
        std::size_t offer = 0;
        std::vector<Placed> placed;
        std::int64_t maxX = std::numeric_limits<std::int64_t>::min();
    };

    [[nodiscard]] static std::uint32_t index(std::size_t part, int turn) noexcept
    {
        return static_cast<std::uint32_t>(part * 4 + static_cast<std::size_t>(turn));
    }

    /// Best position of `shape` on `sheet`, if it fits there.
    [[nodiscard]] std::optional<Point64> position(const OpenSheet& sheet,
                                                  std::uint32_t shape) const;

    const Problem& problem_;
    std::vector<std::optional<Shape>> shapes_; ///< [part * 4 + turn]
    std::vector<Box> usable_;                  ///< per offer
    NfpCache cache_;
};

std::optional<Point64> FreeDecoder::position(const OpenSheet& sheet, std::uint32_t shape) const
{
    const Shape& s = *shapes_[shape];
    const Box& area = usable_[sheet.offer];
    const Box fit{area.x0 - s.outline.x0, area.y0 - s.outline.y0, area.x1 - s.outline.x1,
                  area.y1 - s.outline.y1};
    if (!fit.isValid()) {
        return std::nullopt;
    }
    const Box region = fit.grown(kTouch);

    std::vector<Point64> candidates;
    Paths64 forbidden;
    for (const Placed& q : sheet.placed) {
        const auto nfp = cache_.get(q.shape, shape, [&] {
            return computeNfp(*shapes_[q.shape], s, kTouch);
        });
        if (nfp->paths.empty() || !nfp->bounds.moved(q.offset).overlaps(region)) {
            continue; // too far away to matter here
        }
        for (const Path64& p : nfp->paths) {
            forbidden.push_back(Clipper2Lib::TranslatePath(p, q.offset.x, q.offset.y));
        }
    }
    if (forbidden.empty()) {
        candidates = boxPath(fit); // nothing in the way: the exact corners of the margins
    } else {
        const Paths64 free =
            Clipper2Lib::Difference(Paths64{boxPath(region)}, forbidden,
                                    Clipper2Lib::FillRule::NonZero);
        for (const Path64& p : free) {
            candidates.insert(candidates.end(), p.begin(), p.end());
        }
    }
    if (candidates.empty()) {
        return std::nullopt;
    }
    // Leftmost reach of all parts, then lowest, then leftmost position.
    const auto score = [&](const Point64& c) {
        return std::tuple(std::max(sheet.maxX, c.x + s.outline.x1), c.y, c.x);
    };
    return *std::min_element(candidates.begin(), candidates.end(),
                             [&](const Point64& a, const Point64& b) {
                                 return score(a) < score(b);
                             });
}

Decoded FreeDecoder::decode(const Genome& genome) const
{
    const std::vector<SheetOffer>& offers = problem_.offers;
    std::vector<OpenSheet> sheets;
    std::vector<int> used(offers.size(), 0);
    Decoded out;

    for (const int pieceIndex : genome.order) {
        const auto piece = static_cast<std::size_t>(pieceIndex);
        const std::size_t part = problem_.pieces[piece].part;
        const int turn = problem_.turns[part][static_cast<std::size_t>(genome.turn[piece])];
        const std::uint32_t shape = index(part, turn);

        bool placed = false;
        for (OpenSheet& sheet : sheets) {
            if (const auto at = position(sheet, shape)) {
                sheet.placed.push_back({shape, *at, piece});
                sheet.maxX = std::max(sheet.maxX, at->x + shapes_[shape]->outline.x1);
                placed = true;
                break;
            }
        }
        for (std::size_t o = 0; !placed && o < offers.size(); ++o) {
            if (offers[o].available != SheetOffer::kUnlimited && used[o] >= offers[o].available) {
                continue;
            }
            OpenSheet fresh{o, {}, std::numeric_limits<std::int64_t>::min()};
            if (const auto at = position(fresh, shape)) {
                fresh.placed.push_back({shape, *at, piece});
                fresh.maxX = at->x + shapes_[shape]->outline.x1;
                sheets.push_back(std::move(fresh));
                ++used[o];
                placed = true;
            }
        }
        if (!placed) {
            out.unplaced.push_back(piece);
        }
    }

    const Real spacing = problem_.settings.router.spacing();
    for (const OpenSheet& open : sheets) {
        const SheetOffer& offer = offers[open.offer];
        SheetLayout layout;
        layout.sheet = offer.sheet;
        layout.source = offer.source;
        layout.usable = usableArea(offer.sheet, offer.source, problem_.settings);
        for (const Placed& p : open.placed) {
            const Piece& piece = problem_.pieces[p.piece];
            const int turn = static_cast<int>(p.shape % 4);
            layout.parts.push_back({piece.part, piece.copy, Transform{turn, vec(p.offset)}});
            layout.partsArea += area((*problem_.parts)[piece.part].outline);
        }
        // The cutter separating the rest of the sheet runs half the spacing right of the last
        // part; the remnant starts where its cut ends.
        const Rect rest{{mm(open.maxX) + spacing, 0.0}, {offer.sheet.length, offer.sheet.width}};
        if (!rest.isEmpty() && problem_.remnantRules.isUsable(rest.width(), rest.height())) {
            layout.remnants.push_back(rest);
        }
        const Box& u = usable_[open.offer];
        out.cost.sheetArea += std::llround(offer.sheet.area());
        out.cost.compactness += static_cast<Real>(open.maxX - u.x0)
                              / static_cast<Real>(std::max<std::int64_t>(1, u.x1 - u.x0));
        out.sheets.push_back(std::move(layout));
    }
    out.cost.unplaced = static_cast<int>(out.unplaced.size());
    return out;
}

} // namespace

std::unique_ptr<Decoder> makeFreeDecoder(const Problem& problem)
{
    return std::make_unique<FreeDecoder>(problem);
}

} // namespace rc::nesting::detail
