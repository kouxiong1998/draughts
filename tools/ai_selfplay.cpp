// tools/ai_selfplay.cpp
// AI vs AI self-play at short time control. Prints every move so we can
// see exactly what the search produces over a full game.

#include "core/GameEngine.hpp"
#include "core/Notation.hpp"
#include "ai/AIEngine.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace draughts;

int main(int argc, char** argv) {
    int maxPlies = 30;
    if (argc > 1) maxPlies = std::atoi(argv[1]);

    core::GameEngine engine;
    engine.newGame(core::RuleSet::InternationalMaxCapture, core::Color::Red);

    ai::AIEngine ai;
    ai.setThreadCount(4);

    ai::TimeBudget budget;
    budget.soft    = std::chrono::milliseconds(800);
    budget.hard    = std::chrono::milliseconds(1200);
    budget.minimum = std::chrono::milliseconds(200);

    std::atomic<bool> done{false};
    core::Move lastChosen{};
    ai::SearchStats lastStats{};

    ai.setDoneCallback([&](const core::Move& m, const ai::SearchStats& s) {
        lastChosen = m;
        lastStats  = s;
        done.store(true);
    });

    std::printf("AI self-play, %d plies at 1s/move\n\n", maxPlies);
    std::fflush(stdout);

    for (int ply = 0; ply < maxPlies && engine.result() == core::GameResult::Ongoing; ++ply) {
        const core::Board preBoard = engine.board();
        const core::Color mover = engine.sideToMove();

        done.store(false);
        ai.think(preBoard, mover, engine.rules(), budget);

        const auto start = std::chrono::steady_clock::now();
        while (!done.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(3))
                break;
        }
        ai.stop();

        const auto notation = core::formatMove(preBoard, mover, lastChosen);
        std::printf("ply %2d  %-6s  %-10s  depth %2d  score %6d  nodes %10llu\n",
                    ply,
                    mover == core::Color::Red ? "Red" : "Yellow",
                    notation.c_str(),
                    lastStats.depth, lastStats.score,
                    (unsigned long long)lastStats.nodes);
        std::fflush(stdout);

        if (!engine.tryApply(lastChosen)) {
            std::printf("!! tryApply FAILED for %s\n", notation.c_str());
            return 1;
        }
    }

    std::printf("\nFinal result: %d  (0=ongoing 1=RedWins 2=YellowWins 3=Draw)\n",
                (int)engine.result());
    std::printf("Final board: %s\n", engine.board().debugString().c_str());
    return 0;
}
