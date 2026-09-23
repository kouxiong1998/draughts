#include "TimeManager.hpp"

namespace draughts::ai {

TimeManager::TimeManager(TimeBudget budget, std::atomic<bool>* externalStop)
    : budget_(budget), externalStop_(externalStop) {}

void TimeManager::start() noexcept {
    start_   = std::chrono::steady_clock::now();
    running_ = true;
}

std::chrono::milliseconds TimeManager::elapsed() const noexcept {
    if (!running_) return std::chrono::milliseconds{0};
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_);
}

bool TimeManager::shouldStop() const noexcept {
    if (externalStop_ && externalStop_->load(std::memory_order_relaxed)) return true;
    return elapsed() >= budget_.hard;
}

bool TimeManager::canStartNextIter() const noexcept {
    if (externalStop_ && externalStop_->load(std::memory_order_relaxed)) return false;
    return elapsed() < budget_.soft;
}

} // namespace draughts::ai
