#include "Evaluator.hpp"
#include "PST.hpp"
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

    // Yellow pieces ? mirror square for PST lookup.
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

} // namespace

int evaluate(const core::Board& board, core::Color sideToMove) noexcept {
    const int red = materialAndPosition(board);
    return sideToMove == core::Color::Red ? red : -red;
}

} // namespace draughts::ai
