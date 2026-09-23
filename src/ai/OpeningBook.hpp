#pragma once
/// @file OpeningBook.hpp
/// @brief Small text-backed opening book. Each line in the source file is a
///        space-separated sequence of FMJD notation moves (32-28, 28x19).
///        The class replays each line from the standard start position and
///        records every distinct next move for every position along the way.
///
/// At query time, the AI looks up the current position and, if a book line
/// passes through it, picks randomly among the recorded moves. This gives
/// variation across games without the AI having to search the opening.

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

    /// Load lines from a text file. Returns true if the file was opened.
    /// Lines that fail to parse (illegal moves, malformed tokens) are
    /// silently skipped.
    bool loadFromFile(const std::string& path);

    /// Load from an in-memory string (used by tests).
    void loadFromString(const std::string& text);

    [[nodiscard]] std::size_t lineCount()     const noexcept { return lineCount_; }
    [[nodiscard]] std::size_t positionCount() const noexcept { return table_.size(); }
    [[nodiscard]] bool        empty()         const noexcept { return table_.empty(); }

    /// Pick a random move for `board`, or nullopt if the position is not
    /// in the book. `rng` provides variety across games.
    [[nodiscard]] std::optional<core::Move> pickMove(
        const core::Board& board,
        core::Color        side,
        core::RuleSet      rules,
        std::mt19937_64&   rng) const;

private:
    struct Key {
        std::uint64_t hash{0};
        core::Color   side{core::Color::Red};
        core::RuleSet rules{core::RuleSet::InternationalMaxCapture};

        bool operator==(const Key& o) const noexcept {
            return hash == o.hash && side == o.side && rules == o.rules;
        }
    };
    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept {
            return static_cast<std::size_t>(
                k.hash
                ^ (static_cast<std::uint64_t>(k.side)  << 62)
                ^ (static_cast<std::uint64_t>(k.rules) << 60));
        }
    };

    std::unordered_map<Key, std::vector<core::Move>, KeyHash> table_;
    std::size_t lineCount_{0};

    void parseAndInsert(const std::string& line);
};

} // namespace draughts::ai
