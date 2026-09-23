#pragma once
/// @file AIEngine.hpp
/// @brief Lazy-SMP multithreaded AI. Owns a shared transposition table and
///        spawns N worker threads that all search the root position. The
///        deepest fully-completed result across all workers wins.
///
/// An optional OpeningBook can be attached; if the current position is in
/// the book, `think()` returns the book move immediately (via the done
/// callback) without spawning any worker threads.
///
/// A "silent" think (`silent = true`) runs the same search but suppresses
/// both progress and done callbacks. Used for pondering: the engine warms
/// the transposition table in the background while it is the opponent's
/// turn, then a real (non-silent) think benefits from the warmer TT.

#include "OpeningBook.hpp"
#include "EndgameTablebase.hpp"
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

    /// Attach (or detach with nullptr) an opening book.
    void setOpeningBook(const OpeningBook* book) noexcept { book_ = book; }
    [[nodiscard]] const OpeningBook* openingBook() const noexcept { return book_; }

    /// Launch a search. Non-blocking; result via done callback.
    /// When `silent` is true, neither the progress nor the done callback
    /// is fired. This is how ponder warming works: the search populates
    /// the shared TT but produces no visible output.
    void think(const core::Board& board,
               core::Color        side,
               core::RuleSet      rules,
               TimeBudget         budget = {},
               bool               silent = false);

    bool stop(std::chrono::milliseconds wait = std::chrono::milliseconds(500));

    [[nodiscard]] bool thinking() const noexcept { return thinking_.load(); }

    void clearTT() noexcept { tt_.clear(); }

private:
    void runSMP(core::Board board, core::Color side, core::RuleSet rules,
                TimeBudget budget, std::uint64_t generation, bool silent);
    void runWorker(core::Board board, core::Color side, core::RuleSet rules,
                   TimeBudget budget, int threadId, bool silent);
    void onWorkerProgress(const SearchStats& s, int threadId);

    TranspositionTable tt_{1u << 24};  // 16M entries, ~400 MB
    EndgameTablebase   tb_;            // solved in-memory cache, grows on demand

    std::jthread               worker_;
    std::atomic<bool>          stopFlag_{false};
    std::atomic<bool>          thinking_{false};
    std::atomic<std::uint64_t> generation_{0};
    int                        threadCount_{4};

    const OpeningBook*         book_{nullptr};
    std::mt19937_64            bookRng_{std::random_device{}()};

    std::mutex cbMutex_;
    ProgressFn progressCb_;
    DoneFn     doneCb_;

    std::mutex  resultMutex_;
    SearchStats bestStats_{};
    int         lastReportedDepth_{-1};
};

} // namespace draughts::ai
