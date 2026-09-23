#pragma once
/// @file Notation.hpp
/// @brief THE single source for draughts algebraic notation. History panel,
///        save/load, and the opening book all call this.

#include "Board.hpp"
#include "RuleSet.hpp"
#include "Types.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace draughts::core {

/// Format a complete move as FMJD algebraic notation:
///   quiet move    -> "32-28"
///   capture chain -> "32x19x10"   (or "32x19" for a single jump)
/// Requires the board *before* the move to reconstruct chain landings.
[[nodiscard]] std::string formatMove(const Board& boardBefore,
                                     Color        side,
                                     const Move&  move);

/// Number a ply as "1." / "1..." - Red moves are "N.", Yellow are "N...".
[[nodiscard]] std::string formatMoveNumber(int ply, Color mover);

/// Parse a single move token in FMJD algebraic notation. Returns nullopt if
/// the token is malformed, or if the move is not legal in the given position
/// under the given rule set.
///
/// Quiet moves:   "32-28"
/// Capture chains: "32x19x10"  (full intermediate landings)
///                 "32x19"     (single-jump chain only)
[[nodiscard]] std::optional<Move> parseMove(const Board&     board,
                                            Color            side,
                                            RuleSet          rules,
                                            std::string_view token);

} // namespace draughts::core
