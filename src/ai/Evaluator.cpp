#include "Evaluator.hpp"
#include "PST.hpp"
#include "core/BoardConstants.hpp"

namespace draughts::ai {

namespace {

int materialAndPosition(const core::Board& b) noexcept {
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

    // Red pieces
    scan(redMen,   [&](core::Square sq){ score +=  kManValue  + pst::MAN_RED[sq]; });
    scan(redKings, [&](core::Square sq){ score +=  kKingValue + pst::KING[sq];   });

    // Yellow pieces ? mirror square for PST
    scan(yellMen,  [&](core::Square sq){
        const auto m = core::bc::mirrorSquare(sq);
        score -= kManValue + pst::MAN_RED[m];
    });
    scan(yellKings,[&](core::Square sq){
        const auto m = core::bc::mirrorSquare(sq);
        score -= kKingValue + pst::KING[m];
    });

    return score;
}

} // namespace

int evaluate(const core::Board& board, core::Color sideToMove) noexcept {
    const int red = materialAndPosition(board);
    return sideToMove == core::Color::Red ? red : -red;
}

} // namespace draughts::ai
