#include "Evaluator.hpp"
#include "PST.hpp"
#include "core/MoveGenerator.hpp"
#include "core/BoardConstants.hpp"

namespace draughts::ai {

namespace {

int materialAndPosition(const core::Board& b) noexcept {
    const auto& w = pst::active();

    int score = 0;

    const core::Bitboard redMen    = b.menMask(core::Color::Red);
    const core::Bitboard redKings  = b.kingsMask(core::Color::Red);
    const core::Bitboard yellMen   = b.menMask(core::Color::Yellow);
    const core::Bitboard yellKings = b.kingsMask(core::Color::Yellow);

    auto scan = [](core::Bitboard bb, auto fn) {
        while (bb) {
            const auto sq = static_cast<core::Square>(std::countr_zero(bb));
            bb &= bb - 1;
            fn(sq);
        }
    };

    scan(redMen,   [&](core::Square sq){ score +=  kManValue  + w.men[sq];   });
    scan(redKings, [&](core::Square sq){ score +=  kKingValue + w.kings[sq]; });

    scan(yellMen, [&](core::Square sq){
        const auto m = core::bc::mirrorSquare(sq);
        score -= kManValue + w.men[m];
    });
    scan(yellKings, [&](core::Square sq){
        const auto m = core::bc::mirrorSquare(sq);
        score -= kKingValue + w.kings[m];
    });

    return score;
}

inline int moveCount(const core::Board& b, core::Color side, core::RuleSet rules) noexcept {
    return static_cast<int>(core::generateLegalMoves(b, side, rules).size());
}

// A man is "blocked" when neither of its forward diagonals is an empty
// playable square. Both neighbours occupied, or off-board, means the piece
// cannot advance - the classic symptom of a position that will suffocate.
inline int blockedMen(const core::Board& b, core::Color c) noexcept {
    const core::Bitboard men = b.menMask(c);
    const int dA = core::bc::forwardDirA(c);
    const int dB = core::bc::forwardDirB(c);
    int count = 0;
    core::Bitboard bb = men;
    while (bb) {
        const auto sq = static_cast<core::Square>(std::countr_zero(bb));
        bb &= bb - 1;
        const auto a = core::bc::step(sq, dA);
        const auto c2 = core::bc::step(sq, dB);
        const bool aFree = (a != core::kInvalidSquare) && b.empty(a);
        const bool bFree = (c2 != core::kInvalidSquare) && b.empty(c2);
        if (!aFree && !bFree) ++count;
    }
    return count;
}

} // namespace

int evaluate(const core::Board& board,
             core::Color        sideToMove,
             core::RuleSet      rules) noexcept
{
    int red = materialAndPosition(board);

    // Mobility: legal-move count difference. Computed for both sides so the
    // term is exactly antisymmetric between Red and Yellow - this preserves
    // the negamax convention and the symmetry test.
    //
    // Rationale: in draughts, a side with fewer legal moves is worse even
    // when material is equal. This is what prevents the engine from walking
    // into positions where its pieces are blocked and it has nothing to play.
    const int redMob = moveCount(board, core::Color::Red,    rules);
    const int yelMob = moveCount(board, core::Color::Yellow, rules);
    red += kMobilityWeight * (redMob - yelMob);

    // Simplify-when-ahead: the side with more material benefits from trades,
    // because fewer pieces = easier to convert the advantage. The weaker
    // side prefers a crowded board to keep complications alive. Capped at
    // 20 trades so the term stays a nudge, not a dominant factor.
    {
        const int total     = std::popcount(board.occupied());
        const int traded    = std::min(40 - total, 20);
        const int redMat    = (board.count(core::Color::Red,    core::PieceKind::Man)
                               + 3 * board.count(core::Color::Red,    core::PieceKind::King))
                            - (board.count(core::Color::Yellow, core::PieceKind::Man)
                               + 3 * board.count(core::Color::Yellow, core::PieceKind::King));
        if (redMat > 0)      red += traded * kSimplifyWeight;
        else if (redMat < 0) red -= traded * kSimplifyWeight;
    }

    // Blocked-man penalty: men with neither forward diagonal empty.
    // Computed for both sides so the term is antisymmetric between Red
    // and Yellow - this preserves the negamax convention.
    {
        const int redBlocked = blockedMen(board, core::Color::Red);
        const int yelBlocked = blockedMen(board, core::Color::Yellow);
        red -= kBlockedManPenalty * (redBlocked - yelBlocked);
    }


    return sideToMove == core::Color::Red ? red : -red;
}

} // namespace draughts::ai
