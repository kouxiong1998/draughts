// tools/ai_ui_sim.cpp ? verbose diagnostic
#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>

#include <chrono>
#include <cstdio>
#include <thread>

#include "ui/GameController.hpp"
#include "controller/ModeConfig.hpp"
#include "core/MoveGenerator.hpp"
#include "core/Notation.hpp"

using namespace draughts;

static qint64 msSince(const std::chrono::steady_clock::time_point& t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t).count();
}

static void printHistory(ui::GameController& c) {
    std::printf("  history(%llu/%llu):\n",
        (unsigned long long)c.historyCursor(),
        (unsigned long long)c.history().size());
    core::Board b; b.resetStandard();
    core::Color side = core::Color::Red;
    for (std::size_t i = 0; i < c.historyCursor(); ++i) {
        const auto& rec = c.history()[i];
        const auto notn = core::formatMove(b, side, rec.move);
        std::printf("    [%llu] %s   (%d->%d, cap=%d)\n",
            (unsigned long long)i, notn.c_str(),
            (int)rec.move.from, (int)rec.move.to, rec.move.captureCount());
        core::Board next = b;
        core::Bitboard cap = rec.move.captured;
        while (cap) { auto s = (core::Square)std::countr_zero(cap); cap &= cap-1; next.removePiece(s); }
        auto p = next.at(rec.move.from);
        next.removePiece(rec.move.from);
        next.setPiece(rec.move.to, p.color, p.kind);
        if (p.kind == core::PieceKind::Man && rec.move.isPromotion) next.promote(rec.move.to);
        b = next;
        side = core::opposite(side);
    }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("DraughtsApp");
    QCoreApplication::setApplicationName("Draughts");

    ui::GameController controller;

    QObject::connect(&controller, &ui::GameController::aiThinkingChanged,
        [](bool t){ std::printf("[t] AI thinking = %d\n", (int)t); std::fflush(stdout); });
    QObject::connect(&controller, &ui::GameController::aiProgress,
        [](int d, quint64 n, int s, qint64 ms){
            std::printf("[p] depth=%d nodes=%llu score=%d ms=%lld\n",
                d, (unsigned long long)n, s, (long long)ms);
            std::fflush(stdout);
        });
    QObject::connect(&controller, &ui::GameController::moveApplied,
        [](const core::MoveRecord& rec, const core::Board&){
            std::printf("[a] MOVE APPLIED: %d->%d cap=%d\n",
                (int)rec.move.from, (int)rec.move.to, rec.move.captureCount());
            std::fflush(stdout);
        });

    std::printf("Initial: ply=%d side=%d human=%d ai=%d\n",
        controller.ply(), (int)controller.sideToMove(),
        (int)controller.humanColor(), (int)controller.aiColor());
    std::fflush(stdout);

    std::printf("Switching to Human vs AI...\n"); std::fflush(stdout);
    controller.setMode(controller::GameMode::HumanVsAI);

    auto waitIdle = [&](const char* label) {
        auto start = std::chrono::steady_clock::now();
        std::size_t lastHist = controller.history().size();
        while (msSince(start) < 12000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (controller.history().size() != lastHist && !controller.aiThinking()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                if (!controller.aiThinking()) break;
            }
        }
        std::printf("[%s] exited at %lld ms; histSize=%llu side=%d aiThinking=%d\n",
            label, (long long)msSince(start),
            (unsigned long long)controller.history().size(),
            (int)controller.sideToMove(),
            (int)controller.aiThinking());
        std::fflush(stdout);
    };

    std::printf("\n--- Phase 1: waiting for AI's first move ---\n"); std::fflush(stdout);
    waitIdle("phase1");
    printHistory(controller);

    std::printf("\n--- Phase 2: human move via handleSquareClick ---\n"); std::fflush(stdout);
    {
        std::printf("[h] state before click: side=%d aiTurn=%d aiThinking=%d histSize=%llu\n",
            (int)controller.sideToMove(), (int)controller.isAITurn(),
            (int)controller.aiThinking(),
            (unsigned long long)controller.history().size());
        std::fflush(stdout);

        const auto myMoves = core::generateLegalMoves(
            controller.board(), controller.sideToMove(), controller.rules());
        std::printf("[h] legal moves: %llu\n", (unsigned long long)myMoves.size());
        std::fflush(stdout);
        if (!myMoves.empty()) {
            const auto& mv = myMoves[0];
            std::printf("[h] picking: from=%d to=%d cap=%d\n",
                (int)mv.from, (int)mv.to, mv.captureCount());
            std::fflush(stdout);

            std::printf("[h] click(%d)...\n", (int)mv.from); std::fflush(stdout);
            controller.handleSquareClick(mv.from);
            std::printf("[h]   selected=%d  selectionMoves=%llu\n",
                (int)controller.selectedSquare().value_or(255),
                (unsigned long long)controller.selectionMoves().size());
            std::fflush(stdout);

            std::printf("[h] click(%d)...\n", (int)mv.to); std::fflush(stdout);
            controller.handleSquareClick(mv.to);
            std::printf("[h]   histSize=%llu side=%d aiThinking=%d\n",
                (unsigned long long)controller.history().size(),
                (int)controller.sideToMove(),
                (int)controller.aiThinking());
            std::fflush(stdout);
        }
    }
    printHistory(controller);

    std::printf("\n--- Phase 3: waiting for AI's response ---\n"); std::fflush(stdout);
    waitIdle("phase3");
    printHistory(controller);

    std::printf("\nDone.\n");
    return 0;
}
