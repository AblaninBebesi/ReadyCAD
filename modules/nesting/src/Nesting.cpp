#include "readycad/nesting/Nesting.hpp"

#include "Genetic.hpp"
#include "Problem.hpp"
#include "readycad/detailing/FlatPart.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

namespace rc::nesting {

// ---------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------

void validate(const NestingSettings& s)
{
    const auto require = [](bool ok, const char* what) {
        if (!ok) {
            throw std::invalid_argument(std::string("invalid nesting settings: ") + what);
        }
    };
    require(s.saw.kerf >= 0.0, "kerf < 0");
    require(s.saw.trim >= 0.0, "trim < 0");
    require(s.saw.maxStages >= 2 && s.saw.maxStages <= 4, "stages must be 2, 3 or 4");
    require(s.saw.maxStackHeight > 0.0, "stack height <= 0");
    require(s.router.toolDiameter > 0.0, "tool diameter <= 0");
    require(s.router.extraSpacing >= 0.0, "extra spacing < 0");
    require(s.router.margin >= 0.0, "margin < 0");
    require(s.optimizer.populationSize >= 2, "population < 2");
    require(s.optimizer.generations >= 0, "generations < 0");
    require(s.optimizer.mutationRate >= 0.0 && s.optimizer.mutationRate <= 1.0,
            "mutation rate outside 0..1");
    require(s.optimizer.elites >= 0, "elites < 0");
    require(s.optimizer.threads >= 0, "threads < 0");
    require(s.optimizer.memoryLimitMb >= 0, "memory limit < 0");
}

int effectiveThreads(int threads) noexcept
{
    if (threads > 0) {
        return threads;
    }
    const auto cores = static_cast<int>(std::thread::hardware_concurrency());
    return std::max(1, cores - 1); // one core stays free for the window
}

int sheetsPerStack(const PanelSawSettings& saw, Real thickness) noexcept
{
    if (thickness <= 0.0) {
        return 1;
    }
    return std::max(1, static_cast<int>(std::floor(saw.maxStackHeight / thickness + 1e-9)));
}

// ---------------------------------------------------------------------------------------------
// Job
// ---------------------------------------------------------------------------------------------

std::vector<NestingPart> partsFromCutList(const core::Product& product,
                                          const std::vector<detailing::CutListRow>& cutList,
                                          int productCount)
{
    if (productCount < 1) {
        throw std::invalid_argument("product count must be at least 1");
    }
    std::vector<NestingPart> parts;
    parts.reserve(cutList.size());
    for (std::size_t i = 0; i < cutList.size(); ++i) {
        const detailing::CutListRow& row = cutList[i];
        if (!row.inCuttingPlan) {
            continue; // bought cut to size or cut elsewhere
        }
        const detailing::FlatPart flat = detailing::flatPartOf(product, row);
        NestingPart part;
        part.label = std::to_string(i + 1);
        part.material = row.material;
        // A shaped part goes in with its real blank: the nesting centre fits parts by their shape
        // (No-Fit Polygon), the panel saw takes its bounding rectangle.
        part.outline = flat.shaped() ? flat.blank : rectangle(Rect{flat.cut.min, flat.cut.max});
        part.quantity = row.quantity * productCount;
        part.keepGrain = row.keepGrainDirection;
        part.cutListRow = i;
        parts.push_back(std::move(part));
    }
    return parts;
}

// ---------------------------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------------------------

Real SheetLayout::utilization() const noexcept
{
    const Real a = sheet.area();
    return a > 0.0 ? partsArea / a : 0.0;
}

Real MaterialUsage::utilization() const noexcept
{
    return sheetArea > 0.0 ? partsArea / sheetArea : 0.0;
}

int NestingResult::sheetCount() const noexcept
{
    int n = 0;
    for (const SheetLayout& layout : layouts) {
        n += layout.count;
    }
    return n;
}

int NestingResult::placedCount() const noexcept
{
    int n = 0;
    for (const SheetLayout& layout : layouts) {
        n += layout.count * static_cast<int>(layout.parts.size());
    }
    return n;
}

Real NestingResult::utilization() const noexcept
{
    Real sheets = 0.0;
    Real used = 0.0;
    for (const SheetLayout& layout : layouts) {
        sheets += layout.count * layout.sheet.area();
        used += layout.count * layout.partsArea;
    }
    return sheets > 0.0 ? used / sheets : 0.0;
}

std::vector<MaterialUsage> NestingResult::usage() const
{
    std::vector<MaterialUsage> out;
    for (const SheetLayout& layout : layouts) {
        auto it = std::find_if(out.begin(), out.end(), [&](const MaterialUsage& u) {
            return sameMaterial(u.material, layout.sheet.material);
        });
        if (it == out.end()) {
            out.push_back({layout.sheet.material});
            it = std::prev(out.end());
        }
        switch (layout.source) {
        case SheetSource::Remnant:  it->remnants += layout.count; break;
        case SheetSource::Stock:    it->stock += layout.count; break;
        case SheetSource::Purchase: it->purchase += layout.count; break;
        }
        it->sheetArea += layout.count * layout.sheet.area();
        it->partsArea += layout.count * layout.partsArea;
        it->newRemnants += layout.count * static_cast<int>(layout.remnants.size());
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// Shared by the decoders
// ---------------------------------------------------------------------------------------------

namespace detail {

std::vector<int> allowedTurns(const NestingPart& part, Machine machine)
{
    const bool box = machine == Machine::PanelSaw || isRectangle(part.outline);
    if (box) {
        return part.keepGrain ? std::vector<int>{0} : std::vector<int>{0, 1};
    }
    return part.keepGrain ? std::vector<int>{0, 2} : std::vector<int>{0, 1, 2, 3};
}

Rect usableArea(const Sheet& sheet, SheetSource source, const NestingSettings& settings) noexcept
{
    Real inset = 0.0;
    if (settings.machine == Machine::PanelSaw) {
        inset = source == SheetSource::Remnant ? 0.0 : settings.saw.trim;
    } else {
        inset = settings.router.margin;
    }
    return {{inset, inset}, {sheet.length - inset, sheet.width - inset}};
}

} // namespace detail

// ---------------------------------------------------------------------------------------------
// Optimisation
// ---------------------------------------------------------------------------------------------

namespace {

using detail::Problem;

/// True if one piece of `part` fits on some sheet of the offers, in some allowed turn.
bool fitsSomewhere(const Problem& problem, std::size_t part)
{
    const Rect box = bounds((*problem.parts)[part].outline);
    for (const SheetOffer& offer : problem.offers) {
        const Rect usable = detail::usableArea(offer.sheet, offer.source, problem.settings);
        for (const int turn : problem.turns[part]) {
            const Vec2 size = turn % 2 == 0 ? box.size() : Vec2(box.height(), box.width());
            if (size.x <= usable.width() + 1e-6 && size.y <= usable.height() + 1e-6) {
                return true;
            }
        }
    }
    return false;
}

/// Same sheet and same parts at the same places (copies aside): cut as one layout × count.
bool sameLayout(const SheetLayout& a, const SheetLayout& b)
{
    if (a.sheet.id != b.sheet.id || a.source != b.source || a.parts.size() != b.parts.size()
        || a.sheet.length != b.sheet.length || a.sheet.width != b.sheet.width) {
        return false;
    }
    const auto key = [](const PlacedPart& p) {
        return std::tuple(p.part, p.transform.quarterTurns, std::llround(p.transform.offset.x * 1000),
                          std::llround(p.transform.offset.y * 1000));
    };
    std::vector<decltype(key(a.parts.front()))> ka;
    std::vector<decltype(key(a.parts.front()))> kb;
    for (std::size_t i = 0; i < a.parts.size(); ++i) {
        ka.push_back(key(a.parts[i]));
        kb.push_back(key(b.parts[i]));
    }
    std::sort(ka.begin(), ka.end());
    std::sort(kb.begin(), kb.end());
    return ka == kb;
}

void appendGrouped(std::vector<SheetLayout>& to, std::vector<SheetLayout> sheets)
{
    const std::size_t first = to.size(); // only group within the same material
    for (SheetLayout& sheet : sheets) {
        const auto same = std::find_if(to.begin() + static_cast<std::ptrdiff_t>(first), to.end(),
                                       [&](const SheetLayout& g) { return sameLayout(g, sheet); });
        if (same != to.end()) {
            same->count += sheet.count;
        } else {
            to.push_back(std::move(sheet));
        }
    }
}

void addUnplaced(NestingResult& result, std::size_t part, int quantity)
{
    for (NestingResult::Unplaced& u : result.unplaced) {
        if (u.part == part) {
            u.quantity += quantity;
            return;
        }
    }
    result.unplaced.push_back({part, quantity});
}

} // namespace

NestingResult optimize(const std::vector<NestingPart>& parts, const StockManager& stock,
                       const NestingSettings& settings, const ProgressCallback& progress,
                       std::stop_token stop)
{
    validate(settings);
    NestingResult result;
    result.settings = settings;
    result.parts = parts;
    for (NestingPart& part : result.parts) {
        if (part.quantity < 0) {
            throw std::invalid_argument("part quantity < 0: " + part.label);
        }
        part.outline = normalized(std::move(part.outline)); // throws for degenerate outlines
    }

    // One independent problem per material: parts can only come from sheets of their own.
    std::vector<std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < result.parts.size(); ++i) {
        if (result.parts[i].quantity == 0) {
            continue;
        }
        auto group = std::find_if(groups.begin(), groups.end(), [&](const auto& g) {
            return sameMaterial(result.parts[g.front()].material, result.parts[i].material);
        });
        if (group == groups.end()) {
            groups.push_back({i});
        } else {
            group->push_back(i);
        }
    }

    for (std::size_t g = 0; g < groups.size(); ++g) {
        Problem problem;
        problem.parts = &result.parts;
        problem.settings = settings;
        problem.offers = stock.offers(result.parts[groups[g].front()].material);
        if (!settings.useRemnants) {
            std::erase_if(problem.offers,
                          [](const SheetOffer& o) { return o.source == SheetSource::Remnant; });
        }
        problem.remnantRules = stock.remnantRules;
        problem.turns.resize(result.parts.size());
        for (const std::size_t part : groups[g]) {
            problem.turns[part] = detail::allowedTurns(result.parts[part], settings.machine);
            if (!fitsSomewhere(problem, part)) {
                addUnplaced(result, part, result.parts[part].quantity);
                continue;
            }
            for (int copy = 0; copy < result.parts[part].quantity; ++copy) {
                problem.pieces.push_back({part, copy});
            }
        }
        if (problem.pieces.empty()) {
            continue;
        }

        const auto decoder = settings.machine == Machine::PanelSaw
                                 ? detail::makeGuillotineDecoder(problem)
                                 : detail::makeFreeDecoder(problem);
        // A stop before this material started still gives it a layout: the first individual.
        OptimizerSettings optimizer = settings.optimizer;
        if (stop.stop_requested()) {
            optimizer.generations = 0;
        }
        detail::Genome shown;
        NestingResult snapshot;
        const auto onGeneration = [&](int generation, const detail::Genome& best,
                                      const detail::Cost&) {
            if (!progress) {
                return;
            }
            if (generation == 0 || best != shown) {
                shown = best;
                snapshot = result;
                appendGrouped(snapshot.layouts, decoder->decode(best).sheets);
            }
            const NestingProgress p{g,
                                    groups.size(),
                                    generation,
                                    optimizer.generations,
                                    snapshot.utilization(),
                                    decoder->cacheBytes()};
            progress(p, snapshot);
        };
        const detail::Evolution evolution =
            detail::evolve(problem, *decoder, optimizer, onGeneration, stop);
        detail::Decoded best = decoder->decode(evolution.best);
        for (const std::size_t piece : best.unplaced) {
            addUnplaced(result, problem.pieces[piece].part, 1);
        }
        appendGrouped(result.layouts, std::move(best.sheets));
        result.evaluations += evolution.evaluations;
    }
    return result;
}

} // namespace rc::nesting
