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

/// Simplify-when-ahead weight: score bonus for the stronger side per traded
/// piece. Higher = stronger side trades more eagerly. 3 cp per trade is a
/// gentle nudge; too high and the engine sacrifices material for trades.
inline constexpr int kSimplifyWeight = 3;

/// Blocked-man penalty: score penalty for each man whose both forward
/// diagonal neighbours are occupied or off-board. These are "camped"
/// pieces - they contribute no mobility and are the #1 symptom of a
/// position that looks fine on material but is positionally lost.
inline constexpr int kBlockedManPenalty = 15;

/// Man-advance weight: score per row a man has advanced toward promotion.
/// Only active when the board has <= kManAdvanceLimit pieces so it never
/// interferes with the midgame.
inline constexpr int kManAdvanceWeight = 6;
inline constexpr int kManAdvanceLimit  = 16;

/// King-edge-pressure weight: bonus for pushing the opponent's kings
/// toward the edge in king-only endings, and for keeping mine central.
/// Only active when one side has no men and total pieces <= 10.
inline constexpr int kKingEdgeWeight  = 8;
inline constexpr int kKingEdgeLimit   = 10;

/// Score the position for `sideToMove`. Return value is in centipawns;
/// one man = 100.
[[nodiscard]] int evaluate(const core::Board& board,
                           core::Color        sideToMove,
                           core::RuleSet      rules) noexcept;

} // namespace draughts::ai