#pragma once
/// @file OpeningBook.hpp
/// @brief Opening book that reads the merged .bin format produced by
///        tools/bookmerge. Each position maps to up to 5 weighted moves.
///        At query time the engine rolls a weighted die to pick one,
///        which produces natural variation between games while keeping
///        every stored move at the search depth used during generation.

#include "core/Board.hpp"
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace draughts::ai {

class OpeningBook {
public:
    OpeningBook() = default;

    /// Load a merged .bin book (produced by tools/bookmerge). Returns true
    /// if the file was opened and parsed successfully. Missing file is not
    /// an error - the book just stays empty and the engine falls back to
    /// search on every move.
    bool loadFromFile(const std::string& path);

    /// Legacy loader for the old text format (kept for tests).
    bool loadFromTextFile(const std::string& path);

    [[nodiscard]] bool        empty()         const noexcept { return positions_.empty(); }
    [[nodiscard]] std::size_t positionCount() const noexcept { return positions_.size(); }
    [[nodiscard]] std::size_t moveCount()     const noexcept { return totalMoves_; }

    /// Pick a move for the given position, or nullopt if not in the book.
    /// Uses weighted random selection among the stored top-5 moves.
    [[nodiscard]] std::optional<core::Move> pickMove(
        const core::Board& board,
        core::Color        side,
        core::RuleSet      rules,
        std::mt19937_64&   rng) const;

private:
    struct MoveEntry {
        core::Move   move;
        std::uint32_t weight;   // 0-1000 scale
    };
    struct PositionEntry {
        MoveEntry moves[5];
        std::uint8_t count{0};
    };

    std::unordered_map<std::uint64_t, PositionEntry> positions_;
    std::size_t totalMoves_{0};

    [[nodiscard]] static std::uint64_t hashOf(const core::Board& board,
                                              core::Color        side,
                                              core::RuleSet      rules) noexcept;
};

} // namespace draughts::ai
