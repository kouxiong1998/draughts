// tools/dump_moves.cpp
// Loads a .drgt position and prints all legal moves with board labels
// (1 at bottom-left, 50 at top-right). Used to diagnose capture chains.

#include "core/Board.hpp"
#include "core/BoardConstants.hpp"
#include "core/GameEngine.hpp"
#include "core/MoveGenerator.hpp"
#include "core/Notation.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace draughts;

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: dump_moves <file.drgt>\n"); return 1; }
    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }

    core::RuleSet rules = core::RuleSet::InternationalMaxCapture;
    std::vector<std::string> tokens;
    bool inMoves = false;
    std::string line;

    while (std::getline(in, line)) {
        while (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line == "moves:") { inMoves = true; continue; }
        if (!inMoves) {
            if (line.rfind("rules=", 0) == 0) {
                const std::string v = line.substr(6);
                rules = (v == "freecapture")
                    ? core::RuleSet::InternationalFreeCapture
                    : core::RuleSet::InternationalMaxCapture;
            }
        } else {
            tokens.push_back(line);
        }
    }

    core::GameEngine engine;
    engine.newGame(rules, core::Color::Red);
    for (const auto& tok : tokens) {
        const auto m = core::parseMove(engine.board(), engine.sideToMove(),
                                       engine.rules(), tok);
        if (!m) { std::fprintf(stderr, "parse failed: %s\n", tok.c_str()); return 2; }
        if (!engine.tryApply(*m)) { std::fprintf(stderr, "apply failed: %s\n", tok.c_str()); return 3; }
    }

    std::printf("Rules: %s\n",
        engine.rules() == core::RuleSet::InternationalMaxCapture
            ? "MaxCapture ON" : "FreeCapture OFF");
    std::printf("Side to move: %s\n",
        engine.sideToMove() == core::Color::Red ? "Red" : "Yellow");
    std::printf("Ply: %d\n\n", engine.ply());

    std::printf("Board (labels with pieces):\n");
    for (int label = 1; label <= 50; ++label) {
        const auto sq = core::bc::fromDisplayLabel(label);
        const auto p = engine.board().at(sq);
        if (!p.empty()) {
            std::printf("  label %2d: %s %s\n",
                label,
                p.color == core::Color::Red ? "Red" : "Yellow",
                p.kind == core::PieceKind::King ? "King" : "Man");
        }
    }

    std::printf("\n");
    const auto moves = core::generateLegalMoves(
        engine.board(), engine.sideToMove(), engine.rules());

    std::printf("Legal moves (%zu):\n", moves.size());
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        std::printf("  %2zu:  from %2d -> to %2d   captures=%d\n",
            i + 1,
            core::bc::displayLabel(m.from),
            core::bc::displayLabel(m.to),
            m.captureCount());
    }
    return 0;
}
