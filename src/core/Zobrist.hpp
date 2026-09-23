#pragma once
/// @file Zobrist.hpp
/// @brief Deterministic 64-bit Zobrist keys for the 50-square board.
///
/// Keys are drawn from a fixed splitmix64 stream so the same seed always
/// produces the same table — required for the "same seed ⇒ same game"
/// guarantee and for reproducible TT behaviour in tests.

#include "Types.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace draughts::core {

    /// Immutable, seeded Zobrist table. One instance is shared by every module.
    class Zobrist {
    public:
        static const Zobrist& instance() noexcept;

        [[nodiscard]] std::uint64_t piece(Color c, PieceKind k, Square sq) const noexcept {
            return piece_[toIndex(c)][kindIndex(k)][sq];
        }
        [[nodiscard]] std::uint64_t side(Color c) const noexcept { return side_[toIndex(c)]; }

    private:
        explicit Zobrist(std::uint64_t seed) noexcept;

        static constexpr std::size_t kindIndex(PieceKind k) noexcept {
            return static_cast<std::size_t>(k);
        }

        std::array<std::array<std::array<std::uint64_t, kNumPlayableSquares>, 3>, 2> piece_{};
        std::array<std::uint64_t, 2> side_{};

        friend struct ZobristAccess;
    };

} // namespace draughts::core