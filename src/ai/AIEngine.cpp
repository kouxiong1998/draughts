#include "AIEngine.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace draughts::ai {

AIEngine::AIEngine() {
    const unsigned hw = std::thread::hardware_concurrency();
    threadCount_ = std::clamp(static_cast<int>(hw == 0 ? 2 : hw), 2, 8);
}

AIEngine::~AIEngine() {
    stop(std::chrono::milliseconds(2000));
    // worker_ (jthread) joins on destruction automatically.
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
                     TimeBudget         budget)
{
    // Bump generation so any in-flight coordinator suppresses its done signal.
    const std::uint64_t myGen = ++generation_;

    // Signal any in-flight search to stop.
    stopFlag_.store(true, std::memory_order_relaxed);

    // Destroy the previous coordinator (jthread joins it). Bounded because
    // workers should exit within ~0.1 ms of seeing stopFlag_.
    if (worker_.joinable()) worker_.join();

    // Reset shared result state.
    {
        std::lock_guard lk(resultMutex_);
        bestStats_        = SearchStats{};
        bestStats_.depth  = -1;
        lastReportedDepth_ = -1;
    }

    stopFlag_.store(false, std::memory_order_relaxed);

    worker_ = std::jthread([this, board, side, rules, budget, myGen]() {
        runSMP(board, side, rules, budget, myGen);
    });
}

void AIEngine::runSMP(core::Board board, core::Color side, core::RuleSet rules,
                      TimeBudget budget, std::uint64_t myGen)
{
    thinking_.store(true, std::memory_order_relaxed);

    const int n = std::max(1, threadCount_);

    // Spawn N workers inside a nested scope so they are FULLY JOINED before
    // we read bestStats_ or fire the done callback. Firing done before the
    // workers have run produces a default-constructed move (255 -> 255),
    // which the UI then falls back to playing moves[0] ? the "AI plays
    // dumb instant moves" symptom.
    {
        std::vector<std::jthread> workers;
        workers.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            workers.emplace_back([this, board, side, rules, budget, i]() {
                runWorker(board, side, rules, budget, i);
            });
        }
    }   // ? all workers joined here

    thinking_.store(false, std::memory_order_relaxed);

    // If a newer think() superseded us, don't fire done.
    if (generation_.load(std::memory_order_relaxed) != myGen) return;

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
                         TimeBudget budget, int threadId)
{
    try {
        auto search = std::make_unique<Search>(
            &tt_, &stopFlag_,
            [this, threadId](const SearchStats& s) { onWorkerProgress(s, threadId); });

        (void)search->think(board, side, rules, budget, threadId);
    } catch (...) {
        // Never let an exception escape a worker thread.
    }
}

void AIEngine::onWorkerProgress(const SearchStats& s, int threadId) {
    bool shouldEmit = false;
    {
        std::lock_guard lk(resultMutex_);

        // Proper Lazy SMP: thread 0 is authoritative. Threads 1..N exist
        // only to populate the shared transposition table faster, which in
        // turn makes thread 0's search deeper. Reporting any other thread's
        // best move can pick a move from a search tree that used a different
        // history/killer table and prune too aggressively ? a common cause
        // of "AI blunders for no reason" bugs.
        if (threadId == 0) {
            if (s.depth > bestStats_.depth ||
                (s.depth == bestStats_.depth && s.score > bestStats_.score)) {
                bestStats_ = s;
            }
        }
        // Progress display can still update from any thread.
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
