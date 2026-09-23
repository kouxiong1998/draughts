#include "EndgameTablebase.hpp"
#include "core/MoveGenerator.hpp"
#include "core/Zobrist.hpp"

#include <algorithm>
#include <bit>
#include <climits>

namespace draughts::ai {

namespace {

/// Apply a move to `board` in place. Same logic as the search's
/// applyMoveInPlace ? repeated here to keep this file self-contained.
void applyMoveInPlace(core::Board& board, const core::Move& m) noexcept {
    core::Bitboard cap = m.captured;
    while (cap) {
        const auto s = static_cast<core::Square>(std::countr_zero(cap));
        cap &= cap - 1;
        board.removePiece(s);
    }
    const auto p = board.at(m.from);
    board.removePiece(m.from);
    board.setPiece(m.to, p.color, p.kind);
    if (p.kind == core::PieceKind::Man && m.isPromotion) board.promote(m.to);
}

} // namespace

std::uint64_t EndgameTablebase::hashOf(const core::Board& board,
                                        core::Color        side,
                                        core::RuleSet      rules) const noexcept
{
    std::uint64_t h = board.pieceHash();
    h ^= core::Zobrist::instance().side(side);
    h ^= static_cast<std::uint64_t>(rules) << 61;
    return h;
}

int EndgameTablebase::pieceCount(const core::Board& board) const noexcept {
    return std::popcount(board.occupied());
}

std::optional<TBProbe> EndgameTablebase::probe(const core::Board& board,
                                                core::Color        side,
                                                core::RuleSet      rules)
{
    if (pieceCount(board) > kMaxPieces) return std::nullopt;

    ++probes_;
    budgetStart_     = std::chrono::steady_clock::now();
    budgetExceeded_  = false;

    const int raw = solveRec(board, side, rules, kMaxPlies);

    if (budgetExceeded_ || raw == INT_MIN) return std::nullopt;

    TBProbe out{};
    if (raw > 0) {
        out.result   = TBResult::Win;
        out.distance = raw;
    } else if (raw < 0) {
        out.result   = TBResult::Loss;
        out.distance = -raw - 1;
    } else {
        out.result   = TBResult::Draw;
        out.distance = 0;
    }
    return out;
}

int EndgameTablebase::solveRec(const core::Board& board,
                                core::Color        side,
                                core::RuleSet      rules,
                                int                depthLimit)
{
    ++nodes_;

    // Budget check every 1024 nodes.
    if ((nodes_ & 1023u) == 0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - budgetStart_).count();
        if (elapsed > kBudgetMs) {
            budgetExceeded_ = true;
            return INT_MIN;
        }
    }

    // Cache lookup.
    const std::uint64_t key = hashOf(board, side, rules);
    if (const auto it = cache_.find(key); it != cache_.end()) {
        return it->second;
    }

    // Generate legal moves.
    const auto moves = core::generateLegalMoves(board, side, rules);

    // Terminal: side to move has no legal moves -> loss in 0 plies.
    if (moves.empty()) {
        cache_.emplace(key, static_cast<std::int32_t>(-1));
        return -1;
    }

    // Depth limit: treat as draw (imperfect but keeps recursion finite).
    if (depthLimit <= 0) {
        cache_.emplace(key, static_cast<std::int32_t>(0));
        return 0;
    }

    const core::Color opp = core::opposite(side);

    // Standard minimax over the encoding:
    //   * any move that wins in k plies -> side wins in k+1 plies
    //   * all moves lose -> side loses in (max_child_distance + 1)
    //   * otherwise -> draw
    int bestWin  = INT_MAX;   // smallest k where this side wins
    int worstLoss = 0;         // largest k where this side loses
    bool anyDraw = false;
    bool anyUnknown = false;

    for (std::size_t i = 0; i < moves.size(); ++i) {
        core::Board next = board;
        applyMoveInPlace(next, moves[i]);

        const int child = solveRec(next, opp, rules, depthLimit - 1);
        if (budgetExceeded_) return INT_MIN;
        if (child == INT_MIN) { anyUnknown = true; continue; }

        // Child value is from the opponent's perspective.
        //   child > 0  -> opponent wins in `child` plies
        //   child < 0  -> opponent loses in `-child - 1` plies
        //   child == 0 -> draw
        if (child > 0) {
            // Opponent wins: we lose in (child + 1) plies.
            const int ourLoss = child + 1;
            if (ourLoss > worstLoss) worstLoss = ourLoss;
        } else if (child < 0) {
            // Opponent loses: we win in (-child - 1) + 1 = -child plies.
            const int ourWin = -child;
            if (ourWin < bestWin) bestWin = ourWin;
        } else {
            anyDraw = true;
        }

        // Early exit: if we've already found a winning continuation,
        // no need to look further ? we just want the SHORTEST win.
        if (bestWin == 1) break;
    }

    int encoded = 0;
    if (bestWin != INT_MAX) {
        // Encode win: +plies to mate.
        encoded = bestWin;
    } else if (anyDraw) {
        encoded = 0;
    } else if (anyUnknown) {
        // Couldn't fully analyse; treat as draw to avoid false mate claims.
        encoded = 0;
    } else {
        // Every branch loses. Encode loss: -(plies_to_terminal) - 1.
        encoded = -(worstLoss) - 1;
    }

    // Bound the cache size.
    if (cache_.size() >= kMaxEntries) cache_.clear();
    cache_.emplace(key, static_cast<std::int32_t>(encoded));
    return encoded;
}

} // namespace draughts::ai
