#pragma once

#include "readycad/nesting/Nesting.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace rc::nesting {

/// optimize() on a background thread, so the window stays responsive: the UI polls the
/// progress and shows the best layout found so far while the genetic algorithm runs.
/// The destructor cancels the run and waits for the thread.
class NestingRun {
public:
    /// Starts at once. The run works on its own copies of the parts, the stock and the settings.
    NestingRun(std::vector<NestingPart> parts, StockManager stock, NestingSettings settings);
    ~NestingRun();

    NestingRun(const NestingRun&) = delete;
    NestingRun& operator=(const NestingRun&) = delete;

    /// Asks the optimiser to finish soon with the best layouts so far.
    void cancel() noexcept;
    /// Blocks until the run has finished.
    void wait();

    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] NestingProgress progress() const;
    /// Increases whenever result() changes: the UI copies the result only then.
    [[nodiscard]] std::uint64_t version() const noexcept;
    /// The best result so far (none before the first generation); the final one once finished.
    [[nodiscard]] std::optional<NestingResult> result() const;
    /// Non-empty if the optimisation failed (invalid settings or parts).
    [[nodiscard]] std::string error() const;

private:
    struct Shared;
    std::shared_ptr<Shared> shared_;
    std::jthread thread_; // last: joined first on destruction, while shared_ is still alive
};

} // namespace rc::nesting
