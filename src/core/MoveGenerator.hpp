#pragma once
/// @file MoveGenerator.hpp
/// @brief THE legal move generator — the single source of truth for what
///        moves are legal under each RuleSet. AI reuses this verbatim.

#include "Types.hpp"
#include "RuleSet.hpp"
#include "Board.hpp"

namespace draughts::core {

    /// Generate every legal move for `side` on `board` under `rules`.
    /// Returns a MoveSpan (bounded, no allocations).
    [[nodiscard]] MoveSpan generateLegalMoves(const Board& board,
        Color        side,
        RuleSet      rules) noexcept;

    /// Convenience predicate — used by GameEngine and UI to reject illegal clicks.
    [[nodiscard]] bool isLegalMove(const Board& board,
        Color        side,
        RuleSet      rules,
        const Move& move) noexcept;

    /// Replay a chain move and report its landing squares (used only by Notation
    /// and the Animator — never by Search).
    [[nodiscard]] bool expandChainLandings(const Board& board,
        Color        side,
        const Move& move,
        Square* out,
        int          outCapacity) noexcept;

} // namespace draughts::core
