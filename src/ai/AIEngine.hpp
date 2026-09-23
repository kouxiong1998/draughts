#pragma once
/// @file AIEngine.hpp
/// @brief Lazy-SMP multithreaded AI. Owns a shared transposition table and
///        spawns N worker threads that all search the root position. The
///        deepest fully-completed result across all workers wins.
///
/// An optional OpeningBook can be attached; if the current position is in
/// the book, `think()` returns the book move immediately (via the done
/// callback) without spawning any worker threads.

#include "OpeningBook.hpp"
#include "Search.hpp"
#include "TranspositionTable.hpp"
#include "core/Board.hpp"
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
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

    void setThreadCount(int n);
    [[nodiscard]] int threadCount() const noexcept { return threadCount_; }

    /// Attach (or detach with nullptr) an opening book. The engine does not
    /// take ownership; the book must outlive the engine or be cleared first.
    void setOpeningBook(const OpeningBook* book) noexcept { book_ = book; }
    [[nodiscard]] const OpeningBook* openingBook() const noexcept { return book_; }

    /// Launch a search. Non-blocking; result via done callback.
    void think(const core::Board& board,
               core::Color        side,
               core::RuleSet      rules,
               TimeBudget         budget = {});

    bool stop(std::chrono::milliseconds wait = std::chrono::milliseconds(500));

    [[nodiscard]] bool thinking() const noexcept { return thinking_.load(); }

    void clearTT() noexcept { tt_.clear(); }

private:
    void runSMP(core::Board board, core::Color side, core::RuleSet rules,
                TimeBudget budget, std::uint64_t generation);
    void runWorker(core::Board board, core::Color side, core::RuleSet rules,
                   TimeBudget budget, int threadId);
    void onWorkerProgress(const SearchStats& s, int threadId);

    TranspositionTable tt_{1u << 20};

    std::jthread              worker_;
    std::atomic<bool>         stopFlag_{false};
    std::atomic<bool>         thinking_{false};
    std::atomic<std::uint64_t> generation_{0};
    int                       threadCount_{4};

    const OpeningBook*        book_{nullptr};
    std::mt19937_64           bookRng_{std::random_device{}()};

    std::mutex cbMutex_;
    ProgressFn progressCb_;
    DoneFn     doneCb_;

    std::mutex  resultMutex_;
    SearchStats bestStats_{};
    int         lastReportedDepth_{-1};
};

} // namespace draughts::ai
