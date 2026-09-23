#pragma once
/// @file PST.hpp
/// @brief Piece-square tables, indexed by square 0..49 from Red's
///        perspective. Yellow mirrors via bc::mirrorSquare(). Data only.

#include "core/BoardConstants.hpp"
#include "core/Types.hpp"

namespace draughts::ai::pst {

/// Advancement + edge penalty for men. Red's back rank = row 0.
inline constexpr int MAN_RED[core::kNumPlayableSquares] = {
    // row 0 (red's back rank ? defensive)
    -10, -10, -10, -10, -10,
    // row 1
     -6,  -6,  -6,  -6,  -6,
    // row 2
     -2,   0,  -2,   0,  -2,
    // row 3
      2,   2,   4,   2,   2,
    // row 4 (centre)
      6,   6,   6,   6,   6,
    // row 5
     10,  10,  10,  10,  10,
    // row 6
     14,  14,  14,  14,  14,
    // row 7
     18,  18,  18,  18,  18,
    // row 8
     24,  24,  24,  24,  24,
    // row 9 (promotion row ? should be King, but just in case)
     30,  30,  30,  30,  30,
};

/// Small centre bonus for kings; corners slightly worse.
inline constexpr int KING[core::kNumPlayableSquares] = {
     6,  8, 10,  8,  6,
     8, 10, 12, 10,  8,
    10, 12, 14, 12, 10,
    10, 12, 14, 12, 10,
    10, 12, 14, 12, 10,
    10, 12, 14, 12, 10,
    10, 12, 14, 12, 10,
    10, 12, 14, 12, 10,
     8, 10, 12, 10,  8,
     6,  8, 10,  8,  6,
};

} // namespace draughts::ai::pst
