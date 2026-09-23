#include "MoveGenerator.hpp"
#include "CaptureRules.hpp"
#include "BoardConstants.hpp"
#include <algorithm>

namespace draughts::core {
    namespace {

        void generateManQuiet(const Board& board, Square sq, Color side, MoveSpan& out) noexcept {
            const int dA = bc::forwardDirA(side);
            const int dB = bc::forwardDirB(side);
            for (int d : {dA, dB}) {
                const Square to = bc::step(sq, d);
                if (to == kInvalidSquare) continue;
                if (!board.empty(to))     continue;
                Move m;
                m.from = sq;
                m.to = to;
                m.isPromotion = bc::rowOf(to) == bc::promotionRow(side);
                out.push_back(m);
            }
        }

        // After generating all captures, keep only the maximal-length chains if the
        // active RuleSet demands it.
        void applyMajority(MoveSpan& in) noexcept {
            if (in.empty()) return;
            int best = 0;
            for (std::size_t i = 0; i < in.size(); ++i)
                best = std::max(best, in[i].captureCount());
            std::size_t w = 0;
            for (std::size_t i = 0; i < in.size(); ++i)
                if (in[i].captureCount() == best) in.moves[w++] = in.moves[i];
            in.count = w;
        }

    } // namespace

    MoveSpan generateLegalMoves(const Board& board,
        Color        side,
        RuleSet      rules) noexcept
    {
        MoveSpan out{};

        // 1) Captures first (they always win priority in draughts).
        MoveSpan caps{};
        Bitboard own = board.colorMask(side);
        while (own) {
            const Square sq = static_cast<Square>(std::countr_zero(own));
            own &= own - 1;
            generateCapturesFrom(board, sq, side, rules, caps);
        }

        if (!caps.empty()) {
            if (maxCaptureEnabled(rules)) applyMajority(caps);
            return caps;
        }

        // 2) No captures ? quiet moves.
        Bitboard pieces = board.colorMask(side);
        while (pieces) {
            const Square sq = static_cast<Square>(std::countr_zero(pieces));
            pieces &= pieces - 1;
            const Piece p = board.at(sq);
            if (p.kind == PieceKind::Man) {
                generateManQuiet(board, sq, side, out);
            }
            else {
                // Flying king.
                for (int d = 0; d < bc::kNumDirs; ++d) {
                    for (int k = 0; k < kBoardSize; ++k) {
                        const Square to = bc::ray(sq, d, k);
                        if (to == kInvalidSquare) break;
                        if (!board.empty(to))     break;
                        Move m; m.from = sq; m.to = to;
                        out.push_back(m);
                    }
                }
            }
        }
        return out;
    }

    bool isLegalMove(const Board& board, Color side, RuleSet rules, const Move& m) noexcept {
        const MoveSpan legal = generateLegalMoves(board, side, rules);
        for (std::size_t i = 0; i < legal.size(); ++i) {
            const Move& cand = legal[i];
            if (cand.from == m.from && cand.to == m.to && cand.captured == m.captured)
                return true;
        }
        return false;
    }

    // ---- Chain landing replay --------------------------------------------------
    namespace {
        bool manReplay(const Board& b, Color side, Square cursor, Bitboard remaining,
               Square target, Square* out, int cap, int& n)
        {
            if (remaining == 0) {
            if (cap > 0 && cursor == target) { out[n++] = cursor; return true; }
            return false;
        }
            for (int d = 0; d < bc::kNumDirs; ++d) {
                const Square v = bc::step(cursor, d);
                if (v == kInvalidSquare || !(remaining & Board::bit(v))) continue;
                const Square landing = bc::step(v, d);
                if (landing == kInvalidSquare) continue;
                // Intermediate landing may be occupied by a not-yet-removed ghost;
                // we trust MoveGenerator's legality and only need the landing squares
                // for animation, so allow ghosts.
                out[n++] = landing;
                if (manReplay(b, side, landing, remaining & ~Board::bit(v), target,
                              out, cap + 1, n))
                    return true;
                --n;
            }
            return false;
        }

        bool kingReplay(const Board& b, Color side, Square cursor, Bitboard remaining,
                Square target, Square* out, int cap, int& n)
        {
            if (remaining == 0) {
            if (cap > 0 && cursor == target) { out[n++] = cursor; return true; }
            return false;
        }
            for (int d = 0; d < bc::kNumDirs; ++d) {
                Square victim = kInvalidSquare;
                for (int k = 0; k < kBoardSize; ++k) {
                    const Square s = bc::ray(cursor, d, k);
                    if (s == kInvalidSquare) break;
                    if (remaining & Board::bit(s)) { victim = s; break; }
                    if (!b.empty(s) && !(remaining & Board::bit(s))) break;
                }
                if (victim == kInvalidSquare) continue;
                for (int k = 0; k < kBoardSize; ++k) {
                    const Square landing = bc::ray(victim, d, k);
                    if (landing == kInvalidSquare) break;
                    if (!b.empty(landing) && !(remaining & Board::bit(landing))) break;
                    out[n++] = landing;
                    if (kingReplay(b, side, landing, remaining & ~Board::bit(victim), target,
                                  out, cap + 1, n))
                        return true;
                    --n;
                }
            }
            return false;
        }
    } // namespace

    bool expandChainLandings(const Board& board, Color side, const Move& move,
        Square* out, int outCapacity) noexcept
    {
        if (!move.isCapture() || outCapacity < 2) return false;
        int n = 0;
        out[n++] = move.from;
        const Piece p = board.at(move.from);
        if (p.empty() || p.color != side) return false;
        const bool ok = (p.kind == PieceKind::Man)
            ? manReplay(board, side, move.from, move.captured, move.to, out, 0, n)
            : kingReplay(board, side, move.from, move.captured, move.to, out, 0, n);
        return ok;
    }

} // namespace draughts::core
