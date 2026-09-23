#pragma once
/// @file Board.hpp
/// @brief The ONE 10×10 draughts board. Bitboard-backed for speed, but the
///        public API is expressed in `Square` terms. UI, AI and tests all
///        share this class — no second board representation exists.

#include "Types.hpp"
#include "BoardConstants.hpp"
#include <optional>
#include <string>

namespace draughts::core {

    class Board {
    public:
        Board() noexcept = default;

        /// Reset to the standard International Draughts starting position.
        void resetStandard() noexcept;

        /// Clear the board to empty (used by tests / setup editors).
        void clear() noexcept;

        // ── Piece access ────────────────────────────────────────────────────────
        [[nodiscard]] Piece at(Square sq) const noexcept;
        [[nodiscard]] bool  empty(Square sq) const noexcept { return (occupied_ & bit(sq)) == 0; }
        [[nodiscard]] bool  hasPiece(Color c, Square sq) const noexcept {
            return (colorMask(c) & bit(sq)) != 0;
        }
        [[nodiscard]] bool isKing(Square sq) const noexcept { return (kings_ & bit(sq)) != 0; }

        // ── Raw bitboards (used by AI / hashing) ────────────────────────────────
        [[nodiscard]] Bitboard occupied()   const noexcept { return occupied_; }
        [[nodiscard]] Bitboard colorMask(Color c) const noexcept {
            return c == Color::Red ? red_ : yellow_;
        }
        [[nodiscard]] Bitboard menMask(Color c)   const noexcept { return colorMask(c) & ~kings_; }
        [[nodiscard]] Bitboard kingsMask(Color c) const noexcept { return colorMask(c) & kings_; }
        [[nodiscard]] Bitboard allKings()   const noexcept { return kings_; }

        // ── Mutation (used by GameEngine, test helpers, move-gen make/unmake) ───
        void setPiece(Square sq, Color c, PieceKind k) noexcept;
        void removePiece(Square sq) noexcept;
        void promote(Square sq) noexcept;

        // ── Zobrist ─────────────────────────────────────────────────────────────
        /// Hash of the current position (pieces only; side/rule are mixed in by
        /// GameState::hash()).
        [[nodiscard]] std::uint64_t pieceHash() const noexcept { return hash_; }
        void recomputeHash() noexcept;

        // ── Counters (cheap since they use bitboards) ───────────────────────────
        [[nodiscard]] int count(Color c, PieceKind k) const noexcept;

        /// FMJD-style string like "B:W18,20,22:B1,3." – intended for debug only.
        [[nodiscard]] std::string debugString() const;

        [[nodiscard]] static constexpr Bitboard bit(Square sq) noexcept {
            return Bitboard{ 1 } << sq;
        }

    private:
        Bitboard red_{ 0 };
        Bitboard yellow_{ 0 };
        Bitboard kings_{ 0 };     // colour-agnostic
        Bitboard occupied_{ 0 };  // == red_ | yellow_
        std::uint64_t hash_{ 0 };
    };

} // namespace draughts::core
