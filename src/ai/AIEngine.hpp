#pragma once
/// @file AIEngine.hpp
/// @brief Lazy-SMP multithreaded AI. Owns a shared transposition table and
///        spawns N worker threads that all search the root position. The
///        deepest fully-completed result across all workers wins.

#include "Search.hpp"
#include "TranspositionTable.hpp"
#include "core/Board.hpp"
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace draughts::ai {

class AIEngine {
public:
    using ProgressFn = std::function<void(const SearchStats&)>;
    using DoneFn     = std::function<void(const core::Move&, const SearchStats&)>;

    AIEngine();
    ~AIEngine();

    AIEngine(const AIEngine&) = delete;
    AIEngine& operator=(const AIEngine&) = delete;

    void setProgressCallback(ProgressFn cb);
    void setDoneCallback    (DoneFn     cb);

    /// Number of SMP worker threads (default: hardware_concurrency, capped
    /// at 8, minimum 2).
    void setThreadCount(int n);
    [[nodiscard]] int threadCount() const noexcept { return threadCount_; }

    /// Launch a search. Non-blocking; result via done callback.
    void think(const core::Board& board,
               core::Color        side,
               core::RuleSet      rules,
               TimeBudget         budget = {});

    /// Cancel any in-flight search. Bounded wait.
    bool stop(std::chrono::milliseconds wait = std::chrono::milliseconds(500));

    [[nodiscard]] bool thinking() const noexcept { return thinking_.load(); }

    /// Wipe the shared transposition table (e.g. on New Game).
    void clearTT() noexcept { tt_.clear(); }

private:
    void runSMP(core::Board board, core::Color side, core::RuleSet rules,
                TimeBudget budget, std::uint64_t generation);
    void runWorker(core::Board board, core::Color side, core::RuleSet rules,
                   TimeBudget budget, int threadId);
    void onWorkerProgress(const SearchStats& s, int threadId);

    /// Shared across all workers. Persists across moves so we keep the
    /// benefit of accumulated search info.
    TranspositionTable tt_{1u << 20};   // 1M entries, ~32 MB

    std::jthread          worker_;      // the coordinator
    std::atomic<bool>     stopFlag_{false};
    std::atomic<bool>     thinking_{false};
    std::atomic<std::uint64_t> generation_{0};
    int                   threadCount_{4};

    std::mutex cbMutex_;
    ProgressFn progressCb_;
    DoneFn     doneCb_;

    std::mutex  resultMutex_;
    SearchStats bestStats_{};
    int         lastReportedDepth_{-1};
};

} // namespace draughts::ai
