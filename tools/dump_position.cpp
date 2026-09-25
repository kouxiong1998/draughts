// tools/dump_position.cpp
// Hard-coded position from the screenshot: red king at label 1, with
// various red men and yellow men. Prints every legal move for the red
// king, showing the full chain path and each landing square.

#include "core/Board.hpp"
#include "core/BoardConstants.hpp"
#include "core/MoveGenerator.hpp"

#include <array>
#include <cstdio>
#include <vector>

using namespace draughts;

static int lab(core::Square s) { return core::bc::displayLabel(s); }

int main() {
    core::Board b; b.clear();

    // Red king at label 1
    b.setPiece(core::bc::fromDisplayLabel(1), core::Color::Red,
               core::PieceKind::King);

    // Red men (from the screenshot)
    for (int l : {26, 31, 32, 33, 39, 40, 41, 42, 43, 44, 47, 48, 49, 50}) {
        b.setPiece(core::bc::fromDisplayLabel(l), core::Color::Red,
                   core::PieceKind::Man);
    }

    // Yellow men (from the screenshot)
    for (int l : {3, 4, 6, 7, 8, 9, 10, 11, 13, 18, 19, 29}) {
        b.setPiece(core::bc::fromDisplayLabel(l), core::Color::Yellow,
                   core::PieceKind::Man);
    }

    std::printf("Position: red king at 1\n");
    std::printf("  Red men:    26 31 32 33 39 40 41 42 43 44 47 48 49 50\n");
    std::printf("  Yellow men: 3 4 6 7 8 9 10 11 13 18 19 29\n");
    std::printf("  Rules: FreeCapture (MaxCapture OFF)\n\n");

    const auto moves = core::generateLegalMoves(
        b, core::Color::Red, core::RuleSet::InternationalFreeCapture);

    // Show only moves originating from the red king at label 1.
    std::printf("Legal moves from square 1 (red king):\n");
    int shown = 0;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        if (lab(m.from) != 1) continue;
        ++shown;

        std::array<core::Square, 32> landings{};
        const bool ok = core::expandChainLandings(b, core::Color::Red, m,
            landings.data(), static_cast<int>(landings.size()));

        std::printf("  %2d)  1", shown);
        if (ok) {
            for (std::size_t k = 1; k < landings.size(); ++k) {
                if (landings[k] == core::kInvalidSquare) break;
                std::printf(" -> %d", lab(landings[k]));
                if (landings[k] == m.to) break;
            }
        } else {
            std::printf(" -> %d (no path)", lab(m.to));
        }
        std::printf("    captures=%d\n", m.captureCount());
    }
    std::printf("\nTotal: %d legal chains from square 1\n", shown);

    return 0;
}
