#include "AIEngine.hpp"
#include "core/MoveGenerator.hpp"
#include "PST.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace draughts::ai {

AIEngine::AIEngine() {
    const unsigned hw = std::thread::hardware_concurrency();
    threadCount_ = std::clamp(static_cast<int>(hw == 0 ? 2u : hw) * 7 / 10, 2, 32);
}

AIEngine::~AIEngine() {
    stop(std::chrono::milliseconds(2000));
}

void AIEngine::setProgressCallback(ProgressFn cb) {
    std::lock_guard lk(cbMutex_);
    progressCb_ = std::move(cb);
}

void AIEngine::setDoneCallback(DoneFn cb) {
    std::lock_guard lk(cbMutex_);
    doneCb_ = std::move(cb);
}

void AIEngine::setThreadCount(int n) {
    threadCount_ = std::clamp(n, 1, 32);
}

bool AIEngine::stop(std::chrono::milliseconds wait) {
    stopFlag_.store(true, std::memory_order_relaxed);

    const auto deadline = std::chrono::steady_clock::now() + wait;
    while (thinking_.load(std::memory_order_relaxed)
           && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return !thinking_.load(std::memory_order_relaxed);
}

void AIEngine::think(const core::Board& board,
                     core::Color        side,
                     core::RuleSet      rules,
                     TimeBudget         budget,
                     bool               silent)
{
    const std::uint64_t myGen = ++generation_;

    // Select the correct PST weight set for this rule variant before any
    // evaluation or search runs. Called on every think() so a rule change
    // takes effect on the very next move.
    pst::setActive(rules);

    stopFlag_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) worker_.join();

    {
        std::lock_guard lk(resultMutex_);
        bestStats_         = SearchStats{};
        bestStats_.depth   = -1;
        lastReportedDepth_ = -1;
    }

    stopFlag_.store(false, std::memory_order_relaxed);

    // Opening book fast path: only applies to non-silent (real) searches.
    // A ponder search never uses the book because the book has nothing to
    // add to TT warming, and we do not want to shortcut the AI's real move.
    if (!silent && book_ && !book_->empty()) {
        auto bookMove = book_->pickMove(board, side, rules, bookRng_);
        if (bookMove) {
            SearchStats s;
            s.bestMove  = *bookMove;
            s.depth     = 0;
            s.score     = 0;
            s.completed = true;

            worker_ = std::jthread([this, s, myGen]() {
                thinking_.store(true, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                thinking_.store(false, std::memory_order_relaxed);

                if (generation_.load(std::memory_order_relaxed) != myGen) return;

                DoneFn cb;
                {
                    std::lock_guard lk(cbMutex_);
                    cb = doneCb_;
                }
                if (cb) cb(s.bestMove, s);
            });
            return;
        }
    }

    worker_ = std::jthread(
        [this, board, side, rules, budget, myGen, silent]() {
            runSMP(board, side, rules, budget, myGen, silent);
        });
}

void AIEngine::runSMP(core::Board board, core::Color side, core::RuleSet rules,
                      TimeBudget budget, std::uint64_t myGen, bool silent)
{
    thinking_.store(true, std::memory_order_relaxed);

    const int n = std::max(1, threadCount_);

    {
        std::vector<std::jthread> workers;
        workers.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            workers.emplace_back([this, board, side, rules, budget, i, silent]() {
                runWorker(board, side, rules, budget, i, silent);
            });
        }
    }

    thinking_.store(false, std::memory_order_relaxed);

    if (generation_.load(std::memory_order_relaxed) != myGen) return;

    // A silent search produces no callback output ? its only purpose was
    // to warm the shared transposition table.
    if (silent) return;

    SearchStats best;
    {
        std::lock_guard lk(resultMutex_);
        best = bestStats_;
    }

    DoneFn cb;
    {
        std::lock_guard lk(cbMutex_);
        cb = doneCb_;
    }
    if (cb) cb(best.bestMove, best);
}

void AIEngine::runWorker(core::Board board, core::Color side, core::RuleSet rules,
                         TimeBudget budget, int threadId, bool silent)
{
    try {
        EndgameTablebase* tb = (threadId == 0) ? &tb_ : nullptr;
        auto search = std::make_unique<Search>(
            &tt_, tb, &stopFlag_,
            [this, threadId, silent](const SearchStats& s) {
                onWorkerProgress(s, threadId);
            });

        (void)search->think(board, side, rules, budget, threadId);
    } catch (...) {
    }
}

void AIEngine::onWorkerProgress(const SearchStats& s, int threadId) {
    bool shouldEmit = false;
    {
        std::lock_guard lk(resultMutex_);

        if (threadId == 0) {
            if (s.depth > bestStats_.depth ||
                (s.depth == bestStats_.depth && s.score > bestStats_.score)) {
                bestStats_ = s;
            }
        }
        if (s.depth > lastReportedDepth_) {
            lastReportedDepth_ = s.depth;
            shouldEmit = true;
        }
    }
    if (!shouldEmit) return;

    ProgressFn cb;
    {
        std::lock_guard lk(cbMutex_);
        cb = progressCb_;
    }
    if (cb) cb(s);
}

} // namespace draughts::ai
