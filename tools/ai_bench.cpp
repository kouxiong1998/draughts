// tools/ai_bench.cpp
// Standalone AI benchmark. Runs one search on the standard opening
// position and prints per-depth progress. Used to diagnose whether a
// strength regression lives in the search or in the UI layer.

#include "core/Board.hpp"
#include "ai/AIEngine.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace draughts;

int main() {
    core::Board board; board.resetStandard();

    ai::AIEngine engine;
    engine.setThreadCount(4);

    std::atomic<bool> done{false};

    engine.setProgressCallback([](const ai::SearchStats& s){
        std::printf("depth %2d  nodes %12llu  tt %10llu  score %6d  elapsed %6lld ms\n",
                    s.depth,
                    (unsigned long long)s.nodes,
                    (unsigned long long)s.ttHits,
                    s.score,
                    (long long)s.elapsed.count());
        std::fflush(stdout);
    });

    engine.setDoneCallback([&done](const core::Move& m, const ai::SearchStats& s){
        std::printf("\n=== DONE ===\n");
        std::printf("best move   %d -> %d   captures %d   promo %d\n",
                    (int)m.from, (int)m.to, m.captureCount(), (int)m.isPromotion);
        std::printf("final depth %d   score %d   nodes %llu   elapsed %lld ms\n",
                    s.depth, s.score,
                    (unsigned long long)s.nodes,
                    (long long)s.elapsed.count());
        std::fflush(stdout);
        done.store(true);
    });

    std::printf("Starting search on opening position, 5 s budget, 4 threads...\n\n");
    std::fflush(stdout);
    engine.think(board, core::Color::Red, core::RuleSet::InternationalMaxCapture);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    engine.stop();
    return 0;
}
