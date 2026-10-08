#include "Genetic.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <map>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <utility>

namespace rc::nesting::detail {

namespace {

using Random = std::mt19937_64;

struct Individual {
    Genome genome;
    Cost cost;
    bool evaluated = false;
};

[[nodiscard]] Real chance(Random& random)
{
    return std::uniform_real_distribution<Real>(0.0, 1.0)(random);
}

[[nodiscard]] int pick(Random& random, int lo, int hi)
{
    return std::uniform_int_distribution<int>(lo, hi)(random);
}

/// Swaps neighbours in the order and changes turns, each gene with probability `rate`.
void mutate(Genome& g, const Problem& problem, Real rate, Random& random)
{
    const auto n = g.order.size();
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (chance(random) < rate) {
            std::swap(g.order[i], g.order[i + 1]);
        }
    }
    for (std::size_t piece = 0; piece < n; ++piece) {
        const auto turns = static_cast<int>(problem.turns[problem.pieces[piece].part].size());
        if (turns > 1 && chance(random) < rate) {
            int other = pick(random, 0, turns - 2);
            if (other >= g.turn[piece]) {
                ++other; // a turn different from the current one
            }
            g.turn[piece] = other;
        }
    }
}

/// One-point order crossover: the child starts with the first part of A's order and continues
/// with the remaining pieces in B's order; each piece keeps the turn of the parent it came from.
[[nodiscard]] Genome crossover(const Genome& a, const Genome& b, Random& random)
{
    const auto n = static_cast<int>(a.order.size());
    const int cut = n < 2 ? n : pick(random, 1, n - 1);
    Genome child;
    child.turn = b.turn;
    child.order.reserve(a.order.size());
    std::vector<char> taken(a.order.size(), 0);
    for (int i = 0; i < cut; ++i) {
        const auto piece = static_cast<std::size_t>(a.order[static_cast<std::size_t>(i)]);
        child.order.push_back(static_cast<int>(piece));
        child.turn[piece] = a.turn[piece];
        taken[piece] = 1;
    }
    for (const int piece : b.order) {
        if (taken[static_cast<std::size_t>(piece)] == 0) {
            child.order.push_back(piece);
        }
    }
    return child;
}

/// Tournament of three: the population is sorted, so the smallest index wins.
[[nodiscard]] const Genome& select(const std::vector<Individual>& population, Random& random)
{
    const int last = static_cast<int>(population.size()) - 1;
    int best = pick(random, 0, last);
    for (int i = 0; i < 2; ++i) {
        best = std::min(best, pick(random, 0, last));
    }
    return population[static_cast<std::size_t>(best)].genome;
}

/// Decodes the individuals that have no cost yet, on `threads` threads. Genomes seen before
/// are looked up in `known`. Returns the number of genomes decoded.
int evaluate(std::vector<Individual>& population, const Decoder& decoder, int threads,
             std::map<Genome, Cost>& known)
{
    std::vector<Individual*> todo;
    for (Individual& individual : population) {
        if (individual.evaluated) {
            continue;
        }
        if (const auto it = known.find(individual.genome); it != known.end()) {
            individual.cost = it->second;
            individual.evaluated = true;
        } else {
            todo.push_back(&individual);
        }
    }
    // The same new genome twice in one generation: decode it once.
    std::vector<Individual*> unique;
    for (Individual* individual : todo) {
        const bool seen = std::any_of(unique.begin(), unique.end(), [&](const Individual* u) {
            return u->genome == individual->genome;
        });
        if (!seen) {
            unique.push_back(individual);
        }
    }

    std::atomic<std::size_t> next{0};
    std::exception_ptr failure;
    std::mutex failureMutex;
    const auto work = [&] {
        for (;;) {
            const std::size_t i = next++;
            if (i >= unique.size()) {
                return;
            }
            try {
                unique[i]->cost = decoder.decode(unique[i]->genome).cost;
            } catch (...) {
                const std::lock_guard lock(failureMutex);
                failure = std::current_exception();
                next = unique.size(); // stop the others too
            }
        }
    };
    const auto workers = std::min<std::size_t>(static_cast<std::size_t>(std::max(1, threads)),
                                               unique.size());
    if (workers <= 1) {
        work();
    } else {
        std::vector<std::jthread> pool;
        pool.reserve(workers);
        for (std::size_t t = 0; t < workers; ++t) {
            pool.emplace_back(work);
        }
    } // jthreads join here
    if (failure) {
        std::rethrow_exception(failure);
    }

    for (Individual* individual : unique) {
        individual->evaluated = true;
        known.emplace(individual->genome, individual->cost);
    }
    for (Individual* individual : todo) {
        if (!individual->evaluated) {
            individual->cost = known.at(individual->genome);
            individual->evaluated = true;
        }
    }
    return static_cast<int>(unique.size());
}

void sortByCost(std::vector<Individual>& population)
{
    std::stable_sort(population.begin(), population.end(),
                     [](const Individual& a, const Individual& b) { return a.cost < b.cost; });
}

} // namespace

Genome initialGenome(const Problem& problem)
{
    Genome genome;
    genome.order.resize(problem.pieces.size());
    std::iota(genome.order.begin(), genome.order.end(), 0);
    const auto boxArea = [&](int piece) {
        return bounds(problem.part(static_cast<std::size_t>(piece)).outline).area();
    };
    std::stable_sort(genome.order.begin(), genome.order.end(),
                     [&](int a, int b) { return boxArea(a) > boxArea(b); });
    genome.turn.assign(problem.pieces.size(), 0);
    return genome;
}

Evolution evolve(const Problem& problem, const Decoder& decoder, const OptimizerSettings& settings,
                 const GenerationCallback& onGeneration, std::stop_token stop)
{
    Random random(settings.seed);
    const auto size = static_cast<std::size_t>(std::max(2, settings.populationSize));
    const int threads = effectiveThreads(settings.threads);
    std::map<Genome, Cost> known;
    Evolution result;

    // Generation 0: the largest-first order and mutants of it. Mutating harder than later
    // spreads the population around a good start instead of starting from random orders.
    const Genome adam = initialGenome(problem);
    std::vector<Individual> population{{adam, {}, false}};
    const Real startRate = std::clamp(3.0 * settings.mutationRate, 0.2, 1.0);
    while (population.size() < size) {
        Genome g = adam;
        mutate(g, problem, startRate, random);
        population.push_back({std::move(g), {}, false});
    }
    result.evaluations += evaluate(population, decoder, threads, known);
    sortByCost(population);
    result.best = population.front().genome;
    result.cost = population.front().cost;
    if (onGeneration) {
        onGeneration(0, result.best, result.cost);
    }

    const auto elites = std::min(static_cast<std::size_t>(std::max(0, settings.elites)), size - 1);
    for (int generation = 1; generation <= settings.generations; ++generation) {
        if (stop.stop_requested()) {
            break;
        }
        std::vector<Individual> next(population.begin(),
                                     population.begin() + static_cast<std::ptrdiff_t>(elites));
        while (next.size() < size) {
            const Genome& a = select(population, random);
            const Genome& b = select(population, random);
            Genome child = crossover(a, b, random);
            mutate(child, problem, settings.mutationRate, random);
            next.push_back({std::move(child), {}, false});
        }
        result.evaluations += evaluate(next, decoder, threads, known);
        sortByCost(next);
        population = std::move(next);
        result.generations = generation;
        if (population.front().cost < result.cost) { // also without elites: best ever kept
            result.best = population.front().genome;
            result.cost = population.front().cost;
        }
        if (onGeneration) {
            onGeneration(generation, result.best, result.cost);
        }
    }
    return result;
}

} // namespace rc::nesting::detail
