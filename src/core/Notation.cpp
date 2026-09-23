#include "Notation.hpp"
#include "MoveGenerator.hpp"
#include <array>
#include <sstream>

namespace draughts::core {

    std::string formatMove(const Board& boardBefore, Color side, const Move& move) {
        std::ostringstream os;
        auto sq1 = [](Square s) { return static_cast<int>(s) + 1; };

        if (!move.isCapture()) {
            os << sq1(move.from) << '-' << sq1(move.to);
            return os.str();
        }

        // Reconstruct intermediate landings for the chain.
        std::array<Square, 32> landings{};
        const bool ok = expandChainLandings(boardBefore, side, move,
            landings.data(),
            static_cast<int>(landings.size()));
        os << sq1(move.from);
        if (ok) {
            // landings[0] == move.from, landings[1..n-1] are intermediate/final.
            // We already wrote `from`; iterate the rest, stopping at move.to.
            for (std::size_t i = 1; i < landings.size(); ++i) {
                os << 'x' << sq1(landings[i]);
                if (landings[i] == move.to) break;
            }
        }
        else {
            // Fallback: at least print from x to.
            os << 'x' << sq1(move.to);
        }
        return os.str();
    }

    std::string formatMoveNumber(int ply, Color mover) {
        std::ostringstream os;
        const int n = ply / 2 + 1;
        os << n << (mover == Color::Red ? "." : "...");
        return os.str();
    }

} // namespace draughts::core
