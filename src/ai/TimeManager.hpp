#pragma once
/// @file TimeManager.hpp
/// @brief Wall-clock budget for one AI move. Uses std::chrono::steady_clock.

#include <atomic>
#include <chrono>

namespace draughts::ai {

struct TimeBudget {
    std::chrono::milliseconds soft{4000};   ///< start next iteration if reached before this
    std::chrono::milliseconds hard{5500};   ///< absolute ceiling
    std::chrono::milliseconds minimum{300}; ///< always spend at least this long
};

class TimeManager {
public:
    TimeManager(TimeBudget budget, std::atomic<bool>* externalStop);

    void start() noexcept;

    [[nodiscard]] bool shouldStop()       const noexcept; ///< hard limit or external stop
    [[nodiscard]] bool canStartNextIter() const noexcept; ///< still under soft limit
    [[nodiscard]] std::chrono::milliseconds elapsed() const noexcept;

private:
    TimeBudget                  budget_;
    std::atomic<bool>*          externalStop_{nullptr};
    std::chrono::steady_clock::time_point start_{};
    bool                        running_{false};
};

} // namespace draughts::ai
