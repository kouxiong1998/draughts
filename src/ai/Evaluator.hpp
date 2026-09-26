#pragma once
/// @file Evaluator.hpp
/// @brief Static evaluation from the side-to-move's perspective (negamax
///        convention: positive = good for side to move).

#include "core/Board.hpp"
#include "core/Types.hpp"
#include "core/RuleSet.hpp"

namespace draughts::ai {

inline constexpr int kManValue  = 100;
inline constexpr int kKingValue = 300;

/// Mobility weight: score added per unit of legal-move advantage.
/// Empirically 1-2 cp per extra move works well. Higher = more positional
/// play, but too high causes the engine to prefer moving over material.
inline constexpr int kMobilityWeight = 2;

/// Score the position for `sideToMove`. Return value is in centipawns;
/// one man = 100.
[[nodiscard]] int evaluate(const core::Board& board,
                           core::Color        sideToMove,
                           core::RuleSet      rules) noexcept;

} // namespace draughts::ai