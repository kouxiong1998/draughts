#include "Evaluator.hpp"
#include "PST.hpp"
#include "core/MoveGenerator.hpp"
#include "core/BoardConstants.hpp"

namespace draughts::ai {

namespace {

int materialAndPosition(const core::Board& b) noexcept {
    const auto& w = pst::active();

    // King value scales with board density: a flying king dominates a
    // sparse board but can be blocked in a crowded one. At the opening
    // (40 pieces) we keep the classic 3-men value; as pieces come off,
    // the king becomes relatively more valuable.
    const int total     = std::popcount(b.occupied());
    const int kingValue = std::max(250, kKingValue + (40 - total) * 5);

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
    scan(redKings, [&](core::Square sq){ score +=  kingValue + w.kings[sq]; });

    scan(yellMen, [&](core::Square sq){
        const auto m = core::bc::mirrorSquare(sq);
        score -= kManValue + w.men[m];
    });
    scan(yellKings, [&](core::Square sq){
        const auto m = core::bc::mirrorSquare(sq);
        score -= kingValue + w.kings[m];
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


    // Endgame terms - only active in sparse positions to avoid
    // changing the midgame. Both are exactly antisymmetric between
    // Red and Yellow, preserving the negamax convention.
    {
        const int totalPieces = std::popcount(board.occupied());

        // Man advance: every row closer to promotion is worth a little.
        // Gives the engine a gradient to push men home in endings.
        if (totalPieces <= kManAdvanceLimit) {
            core::Bitboard rm = board.menMask(core::Color::Red);
            while (rm) {
                const auto sq = static_cast<core::Square>(std::countr_zero(rm));
                rm &= rm - 1;
                red += manAdvanceWeight(rules) * core::bc::rowOf(sq);
            }
            core::Bitboard ym = board.menMask(core::Color::Yellow);
            while (ym) {
                const auto sq = static_cast<core::Square>(std::countr_zero(ym));
                ym &= ym - 1;
                red -= manAdvanceWeight(rules) * (9 - core::bc::rowOf(sq));
            }
        }

        // King centralization: in sparse positions a central king
        // dominates, an edge king is nearly trapped. Distance-to-edge
        // in 0..4 gives a natural gradient.
        if (totalPieces <= kKingEdgeLimit) {
            auto edgeDist = [](core::Square sq) {
                const int r = core::bc::rowOf(sq);
                const int c = core::bc::colOf(sq);
                return std::min(std::min(r, 9 - r), std::min(c, 9 - c));
            };
            core::Bitboard rk = board.kingsMask(core::Color::Red);
            while (rk) {
                const auto sq = static_cast<core::Square>(std::countr_zero(rk));
                rk &= rk - 1;
                red += kingEdgeWeight(rules) * edgeDist(sq);
            }
            core::Bitboard yk = board.kingsMask(core::Color::Yellow);
            while (yk) {
                const auto sq = static_cast<core::Square>(std::countr_zero(yk));
                yk &= yk - 1;
                red -= kingEdgeWeight(rules) * edgeDist(sq);
            }
        }
    }

    return sideToMove == core::Color::Red ? red : -red;
}

} // namespace draughts::ai
