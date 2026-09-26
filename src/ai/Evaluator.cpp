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

    return sideToMove == core::Color::Red ? red : -red;
}

} // namespace draughts::ai