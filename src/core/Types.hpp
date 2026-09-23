#pragma once
/// @file Types.hpp
/// @brief Fundamental value types shared across the whole engine.
///
/// This is the ONLY place where `Square`, `Color`, `PieceKind`, `Piece`,
/// and `Move` are defined. No other module re-declares them.

#include <array>
#include <bit>
#include <cstdint>

namespace draughts::core {

    /// Playable-square index in the range [0, 49] matching FMJD 1..50 numbering.
    /// Square 0 == "1" == top-left dark square; square 49 == "50" == bottom-right dark square.
    using Square = std::uint8_t;
    inline constexpr Square kInvalidSquare = 0xFF;

    inline constexpr int kNumPlayableSquares = 50;
    inline constexpr int kBoardSize = 10;
    inline constexpr int kMaxCaptureChain = 20;   ///< at most every enemy piece

    /// Bitboard over the 50 playable squares (bit i ? square i occupied).
    using Bitboard = std::uint64_t;
    inline constexpr Bitboard kAllSquares = (Bitboard{ 1 } << kNumPlayableSquares) - 1;

    /// Which side a piece or a turn belongs to.
    enum class Color : std::uint8_t { Red = 0, Yellow = 1 };

    [[nodiscard]] constexpr Color opposite(Color c) noexcept {
        return c == Color::Red ? Color::Yellow : Color::Red;
    }
    [[nodiscard]] constexpr std::size_t toIndex(Color c) noexcept {
        return static_cast<std::size_t>(c);
    }

    /// What kind of piece sits on a square.
    enum class PieceKind : std::uint8_t { None = 0, Man = 1, King = 2 };

    /// A concrete piece on the board.
    struct Piece {
        Color     color{ Color::Red };
        PieceKind kind{ PieceKind::None };
        [[nodiscard]] constexpr bool empty() const noexcept { return kind == PieceKind::None; }
    };

    /// One complete move (a single step OR a full multi-capture chain).
    ///
    /// For a quiet man move: `captured == 0`, `to` is one square diagonally
    /// forward of `from`.
    /// For a capture chain: `captured` is a 50-bit bitboard of every enemy
    /// piece removed at the end of the chain; `from`/`to` are the chain's
    /// start and final landing square. Intermediate landing squares are
    /// recoverable on demand via MoveGenerator (needed only by Notation and
    /// the Animator, not by Search).
    struct Move {
        Square   from{ kInvalidSquare };
        Square   to{ kInvalidSquare };
        Bitboard captured{ 0 };
        bool     isPromotion{ false };

        [[nodiscard]] constexpr int  captureCount() const noexcept {
            return std::popcount(captured);
        }
        [[nodiscard]] constexpr bool isCapture() const noexcept { return captured != 0; }

        friend constexpr bool operator==(const Move&, const Move&) noexcept = default;
    };

    /// Convenience alias used by generators.
    using MoveList = std::array<Move, 128>;  ///< bounded; still > max legal moves
    struct MoveSpan {
        MoveList   moves{};
        std::size_t count{ 0 };

        void push_back(const Move& m) noexcept {
        if (count < moves.size()) moves[count++] = m;
    }
        [[nodiscard]] std::size_t size()  const noexcept { return count; }
        [[nodiscard]] bool        empty() const noexcept { return count == 0; }
        [[nodiscard]] const Move& operator[](std::size_t i) const noexcept { return moves[i]; }
        [[nodiscard]] Move& operator[](std::size_t i)       noexcept { return moves[i]; }
        [[nodiscard]] auto begin() const noexcept { return moves.begin(); }
        [[nodiscard]] auto end()   const noexcept { return moves.begin() + count; }
    };

} // namespace draughts::core
