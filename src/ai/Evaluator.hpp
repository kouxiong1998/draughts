#pragma once
/// @file Evaluator.hpp
/// @brief Static evaluation from the side-to-move's perspective (negamax
///        convention: positive = good for side to move).

#include "core/Board.hpp"
#include "core/Types.hpp"

namespace draughts::ai {

inline constexpr int kManValue  = 100;
inline constexpr int kKingValue = 300;

/// Score the position for `sideToMove`. Return value is in centipawns;
/// one man ? 100.
[[nodiscard]] int evaluate(const core::Board& board, core::Color sideToMove) noexcept;

} // namespace draughts::ai
