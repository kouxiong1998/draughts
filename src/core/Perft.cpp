#include "Perft.hpp"
#include "MoveGenerator.hpp"
#include "Notation.hpp"
#include <chrono>

namespace draughts::core {
    namespace {
        using Clock = std::chrono::steady_clock;

        void perftRec(const Board& board, Color side, RuleSet rules, int depth,
            std::uint64_t& nodes, std::uint64_t& caps, std::uint64_t& proms)
        {
            if (depth == 0) { ++nodes; return; }
            const MoveSpan moves = generateLegalMoves(board, side, rules);
            if (depth == 1) {
                for (std::size_t i = 0; i < moves.size(); ++i) {
                    ++nodes;
                    if (moves[i].isCapture())  ++caps;
                    if (moves[i].isPromotion)  ++proms;
                }
                return;
            }
            for (std::size_t i = 0; i < moves.size(); ++i) {
                const Move& m = moves[i];
                if (m.isCapture())  ++caps;   // count at every ply for a rough total
                if (m.isPromotion)  ++proms;

                Board next = board;
                next.applyMove(m);

                perftRec(next, opposite(side), rules, depth - 1, nodes, caps, proms);
            }
        }
    } // namespace

    PerftResult perft(const Board& board, Color side, RuleSet rules, int depth) {
        PerftResult r; r.depth = depth;
        const auto t0 = Clock::now();
        perftRec(board, side, rules, depth, r.nodes, r.captures, r.promotions);
        const auto t1 = Clock::now();
        r.milliseconds = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return r;
    }

    std::vector<PerftDivideEntry>
        perftDivide(const Board& board, Color side, RuleSet rules, int depth) {
        std::vector<PerftDivideEntry> out;
        const MoveSpan moves = generateLegalMoves(board, side, rules);
        for (std::size_t i = 0; i < moves.size(); ++i) {
            const Move& m = moves[i];
            Board next = board;
            next.applyMove(m);

            std::uint64_t n = 0, cc = 0, pp = 0;
            perftRec(next, opposite(side), rules, depth - 1, n, cc, pp);
            out.push_back({ formatMove(board, side, m), n });
        }
        return out;
    }

} // namespace draughts::core