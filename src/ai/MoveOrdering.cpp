#include "MoveOrdering.hpp"

#include <algorithm>

namespace draughts::ai {

namespace {
constexpr int kTTMoveScore    = 1'000'000;
constexpr int kKiller1Score   =    900'000;
constexpr int kKiller2Score   =    800'000;
constexpr int kCaptureBase    =    500'000;   // + 100 per captured piece
constexpr int kPromotionBonus =     50'000;
}

void sortByScore(ScoredMoveList& moves, std::size_t count) noexcept {
    std::sort(moves.begin(), moves.begin() + static_cast<std::ptrdiff_t>(count),
              [](const ScoredMove& a, const ScoredMove& b) {
                  return a.score > b.score;
              });
}

int scoreMove(const core::Move& move,
              const core::Move& ttMove,
              const core::Move* killersAtPly,
              const std::array<std::array<int, 50>, 50>& historyTable,
              int /*ply*/) noexcept
{
    if (move.from == ttMove.from && move.to == ttMove.to
        && move.captured == ttMove.captured) {
        return kTTMoveScore;
    }

    if (killersAtPly) {
        if (move.from == killersAtPly[0].from && move.to == killersAtPly[0].to
            && move.captured == killersAtPly[0].captured) {
            return kKiller1Score;
        }
        if (move.from == killersAtPly[1].from && move.to == killersAtPly[1].to
            && move.captured == killersAtPly[1].captured) {
            return kKiller2Score;
        }
    }

    if (move.isCapture()) {
        return kCaptureBase + 100 * move.captureCount()
             + (move.isPromotion ? kPromotionBonus : 0);
    }
    if (move.isPromotion) return kPromotionBonus;

    // Defensive: never index history_ with a non-square value. A TT move
    // stored from a fail-low node has from/to == 0xFF, which would overflow
    // history_ [50][50] and stomp the stack canary.
    if (move.from < 50u && move.to < 50u)
        return historyTable[move.from][move.to];
    return 0;
}

} // namespace draughts::ai
