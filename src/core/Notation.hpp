#pragma once
/// @file Notation.hpp
/// @brief THE single source for draughts algebraic notation.
///        History panel, PGN export, and logs all call this.

#include "Types.hpp"
#include "Board.hpp"
#include <string>
#include <string_view>

namespace draughts::core {

    /// Format a complete move as FMJD algebraic notation:
    ///   quiet move       → "32-28"
    ///   capture chain    → "32x19x10"   (or "28x19" for a single jump)
    /// Requires the board *before* the move to reconstruct chain landings.
    [[nodiscard]] std::string formatMove(const Board& boardBefore,
        Color        side,
        const Move& move);

    /// Number a ply as "1." / "1..." — Red moves are "N.", Yellow are "N...".
    [[nodiscard]] std::string formatMoveNumber(int ply, Color mover);

} // namespace draughts::core
