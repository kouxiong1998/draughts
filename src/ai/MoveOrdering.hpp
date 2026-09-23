#pragma once
/// @file MoveOrdering.hpp
/// @brief Scoring and sorting helpers for the alpha-beta move loop.

#include "core/Types.hpp"

#include <array>

namespace draughts::ai {

/// A move along with its ordering score (higher = searched first).
struct ScoredMove {
    core::Move move;
    int        score{0};
};

using ScoredMoveList = std::array<ScoredMove, 128>;

/// Sort `moves` in place by score descending. Bounded by `count`.
void sortByScore(ScoredMoveList& moves, std::size_t count) noexcept;

/// Score a move given the current ordering context. `historyTable` is
/// indexed [from][to].
[[nodiscard]] int scoreMove(const core::Move& move,
                            const core::Move& ttMove,
                            const core::Move* killersAtPly,
                            const std::array<std::array<int, 50>, 50>& historyTable,
                            int ply) noexcept;

} // namespace draughts::ai
