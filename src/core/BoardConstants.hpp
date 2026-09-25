#pragma once
/// @file BoardConstants.hpp
/// @brief All compile-time geometry for the 10x10 board.
///
/// Coordinate system:
///   row 0 = top, row 9 = bottom
///   col 0 = left, col 9 = right
///   Playable squares (rendered BLACK): (row + col) is ODD.
///   Longest playable diagonal: bottom-left (9,0) -> top-right (0,9).
///
/// Red men start in rows 0-3 (top). Yellow men start in rows 6-9 (bottom).

#include "Types.hpp"
#include <array>

namespace draughts::core::bc {

// -- Coordinate conversion ----------------------------------------------------
[[nodiscard]] constexpr bool isPlayable(int row, int col) noexcept {
    return row >= 0 && row < kBoardSize && col >= 0 && col < kBoardSize
        && ((row + col) & 1) == 1;   // playable = ODD parity (black squares)
}

[[nodiscard]] constexpr Square indexFromRowCol(int row, int col) noexcept {
    // row even -> col must be odd.  index offset = (col - 1) / 2
    // row odd  -> col must be even. index offset =  col      / 2
    return static_cast<Square>(row * 5 + (col - ((row + 1) & 1)) / 2);
}

[[nodiscard]] constexpr int rowOf(Square sq) noexcept { return sq / 5; }
[[nodiscard]] constexpr int colOf(Square sq) noexcept {
    const int r = sq / 5;
    const int c = sq % 5;
    return c * 2 + ((r + 1) & 1);
}

// -- Directions ---------------------------------------------------------------
enum Dir : int { NE = 0, NW = 1, SE = 2, SW = 3 };
inline constexpr int kNumDirs = 4;
inline constexpr int kDRow[kNumDirs] = { -1, -1, +1, +1 };
inline constexpr int kDCol[kNumDirs] = { +1, -1, +1, -1 };

[[nodiscard]] constexpr int forwardDirA(Color c) noexcept {
    return c == Color::Red ? SE : NE;
}
[[nodiscard]] constexpr int forwardDirB(Color c) noexcept {
    return c == Color::Red ? SW : NW;
}
[[nodiscard]] constexpr bool isForward(Color c, int dir) noexcept {
    return dir == forwardDirA(c) || dir == forwardDirB(c);
}

// -- Promotion row ------------------------------------------------------------
[[nodiscard]] constexpr int promotionRow(Color c) noexcept {
    return c == Color::Red ? 9 : 0;
}

// -- Precomputed neighbour tables ---------------------------------------------
namespace detail {
struct NeighbourTable {
    std::array<std::array<Square, kNumDirs>, kNumPlayableSquares> step{};
    constexpr NeighbourTable() {
        for (int sq = 0; sq < kNumPlayableSquares; ++sq)
            for (int d = 0; d < kNumDirs; ++d) step[sq][d] = kInvalidSquare;
        for (int row = 0; row < kBoardSize; ++row) {
            for (int col = 0; col < kBoardSize; ++col) {
                if (!isPlayable(row, col)) continue;
                const Square from = indexFromRowCol(row, col);
                for (int d = 0; d < kNumDirs; ++d) {
                    const int nr = row + kDRow[d];
                    const int nc = col + kDCol[d];
                    if (isPlayable(nr, nc)) step[from][d] = indexFromRowCol(nr, nc);
                }
            }
        }
    }
};

struct RayTable {
    std::array<std::array<std::array<Square, kBoardSize>, kNumDirs>,
               kNumPlayableSquares> ray{};
    constexpr RayTable() {
        for (auto& a : ray) for (auto& b : a) for (auto& v : b) v = kInvalidSquare;
        for (int sq = 0; sq < kNumPlayableSquares; ++sq) {
            const int r0 = rowOf(static_cast<Square>(sq));
            const int c0 = colOf(static_cast<Square>(sq));
            for (int d = 0; d < kNumDirs; ++d) {
                int r = r0, c = c0;
                for (int k = 0; k < kBoardSize; ++k) {
                    r += kDRow[d]; c += kDCol[d];
                    if (!isPlayable(r, c)) break;
                    ray[sq][d][k] = indexFromRowCol(r, c);
                }
            }
        }
    }
};

inline constexpr NeighbourTable kNeighbourTable{};
inline constexpr RayTable       kRayTable{};
} // namespace detail

[[nodiscard]] constexpr Square step(Square sq, int dir) noexcept {
    return detail::kNeighbourTable.step[sq][dir];
}
[[nodiscard]] constexpr Square ray(Square sq, int dir, int k) noexcept {
    return detail::kRayTable.ray[sq][dir][k];
}

/// 180-degree rotation of a square (preserves playability under either
/// parity, since mirror changes (r+c) to 18-(r+c), same parity).
[[nodiscard]] constexpr Square mirrorSquare(Square sq) noexcept {
    const int r = rowOf(sq);
    const int c = colOf(sq);
    return indexFromRowCol(kBoardSize - 1 - r, kBoardSize - 1 - c);
}

// Square labels (match the numbers drawn on the board).
// 1 at bottom-left, 5 at bottom-right of the bottom row, then 6-10 on
// the row above, and so on, up to 50 at the top-right corner.
[[nodiscard]] constexpr int displayLabel(Square sq) noexcept {
    const int row = rowOf(sq);
    const int col = colOf(sq);
    const int firstPlayableCol = (row % 2 == 0) ? 1 : 0;
    const int withinRow        = (col - firstPlayableCol) / 2;
    return (kBoardSize - 1 - row) * 5 + withinRow + 1;
}

/// Inverse of displayLabel.
[[nodiscard]] constexpr Square fromDisplayLabel(int label) noexcept {
    const int row        = kBoardSize - 1 - (label - 1) / 5;
    const int withinRow  = (label - 1) % 5;
    const int firstCol   = (row % 2 == 0) ? 1 : 0;
    const int col        = firstCol + withinRow * 2;
    return indexFromRowCol(row, col);
}
// -- Initial setup bitboards --------------------------------------------------
inline constexpr Bitboard kInitialRed    = (Bitboard{1} << 20) - 1;
inline constexpr Bitboard kInitialYellow = ((Bitboard{1} << 20) - 1) << 30;

static_assert(std::popcount(kInitialRed)    == 20);
static_assert(std::popcount(kInitialYellow) == 20);

} // namespace draughts::core::bc
