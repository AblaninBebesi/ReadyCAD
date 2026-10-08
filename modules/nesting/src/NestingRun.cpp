#include "readycad/nesting/NestingRun.hpp"

#include <atomic>
#include <exception>
#include <mutex>
#include <utility>

namespace rc::nesting {

/// What the optimising thread and the UI thread share.
struct NestingRun::Shared {
    mutable std::mutex mutex;
    NestingProgress progress;
    std::optional<NestingResult> result;
    std::string error;
    std::atomic<std::uint64_t> version{0};
    std::atomic<bool> finished{false};
};

NestingRun::NestingRun(std::vector<NestingPart> parts, StockManager stock, NestingSettings settings)
    : shared_(std::make_shared<Shared>())
{
    thread_ = std::jthread([shared = shared_, parts = std::move(parts), stock = std::move(stock),
                            settings](std::stop_token stop) {
        try {
            NestingResult result = optimize(
                parts, stock, settings,
                [&shared](const NestingProgress& progress, const NestingResult& best) {
                    const std::lock_guard lock(shared->mutex);
                    shared->progress = progress;
                    shared->result = best;
                    ++shared->version;
                },
                stop);
            const std::lock_guard lock(shared->mutex);
            shared->result = std::move(result);
            ++shared->version;
        } catch (const std::exception& e) {
            const std::lock_guard lock(shared->mutex);
            shared->error = e.what();
            shared->result.reset();
            ++shared->version;
        }
        shared->finished = true;
    });
}

NestingRun::~NestingRun() = default; // std::jthread: request_stop() + join()

void NestingRun::cancel() noexcept
{
    thread_.request_stop();
}

void NestingRun::wait()
{
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool NestingRun::finished() const noexcept
{
    return shared_->finished.load();
}

NestingProgress NestingRun::progress() const
{
    const std::lock_guard lock(shared_->mutex);
    return shared_->progress;
}

std::uint64_t NestingRun::version() const noexcept
{
    return shared_->version.load();
}

std::optional<NestingResult> NestingRun::result() const
{
    const std::lock_guard lock(shared_->mutex);
    return shared_->result;
}

std::string NestingRun::error() const
{
    const std::lock_guard lock(shared_->mutex);
    return shared_->error;
}

} // namespace rc::nesting
