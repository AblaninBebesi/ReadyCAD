#pragma once

// Genetic algorithm over placement orders and rotations (the deepnest scheme).

#include "Problem.hpp"

#include <functional>
#include <stop_token>

namespace rc::nesting::detail {

/// The first individual: largest parts first (the classic "decreasing size" heuristic), every
/// part in its first allowed turn.
[[nodiscard]] Genome initialGenome(const Problem& problem);

struct Evolution {
    Genome best;
    Cost cost;
    int generations = 0; ///< completed after the initial population
    int evaluations = 0; ///< genomes decoded (repeated genomes are looked up, not decoded)
};

/// Called after the initial population (generation 0) and after every generation.
using GenerationCallback = std::function<void(int generation, const Genome& best, const Cost& cost)>;

/// Runs settings.generations generations or until `stop` is requested. Elitism keeps the best
/// individuals, so the best cost never gets worse from one generation to the next. Individuals
/// are evaluated on several threads; the result does not depend on how many.
[[nodiscard]] Evolution evolve(const Problem& problem, const Decoder& decoder,
                               const OptimizerSettings& settings,
                               const GenerationCallback& onGeneration, std::stop_token stop);

} // namespace rc::nesting::detail
