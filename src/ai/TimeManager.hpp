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

    /// Optional depth cap. 0 = "no cap - go until time budget expires".
    /// When set to N > 0, iterative deepening stops as soon as depth N is
    /// fully completed, regardless of remaining time. Used by the
    /// opening-book generator to produce fixed-depth searches.
    int maxDepth{0};

    // Optional: for opening-book generation. When > 0, after search
    // completes, re-search every root move at the last completed depth
    // and pick randomly among moves within `randomEps` centipawns of the
    // best. Produces the game-to-game variety needed for a useful book.
    int randomTopN{0};   // 0 = disabled, return only the best move
    int randomEps{30};   // centipawn window
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
