#pragma once
/// @file GameState.hpp
/// @brief Per-position data that is not a board: side to move, ply, active
///        rule set, repetition table, 25-move counter, and the FMJD
///        small-endgame counters. One source of truth.

#include "Types.hpp"
#include "RuleSet.hpp"
#include <unordered_map>
#include <vector>

namespace draughts::core {

    class GameState {
    public:
        explicit GameState(RuleSet rules = RuleSet::InternationalMaxCapture) noexcept
            : rules_(rules) {
        }

        [[nodiscard]] Color   sideToMove()  const noexcept { return side_; }
        [[nodiscard]] RuleSet rules()       const noexcept { return rules_; }
        [[nodiscard]] int     ply()         const noexcept { return ply_; }
        [[nodiscard]] int     halfmoveClock() const noexcept { return halfmoveClock_; }
        [[nodiscard]] std::uint64_t hash()  const noexcept { return hash_; }

        void reset(Color first, RuleSet rules) noexcept;

        /// Called by GameEngine after every applied move. Updates:
        ///  • side to move / ply
        ///  • the ply counter / repetition map
        ///  • the 25-move king-only counter
        ///  • the small-endgame counters
        void applyMoveBookkeeping(Color mover, bool movedKing, bool wasCapture) noexcept;

        /// Register a position in the repetition map; call AFTER bookkeeping.
        void notePosition(std::uint64_t positionHash) noexcept;

        [[nodiscard]] int repetitionCount(std::uint64_t positionHash) const noexcept;
        [[nodiscard]] bool isThreefoldRepetition(std::uint64_t positionHash) const noexcept;
        [[nodiscard]] bool isTwentyFiveMoveDraw() const noexcept { return halfmoveClock_ >= 50; }

        void setRules(RuleSet r) noexcept { rules_ = r; }

    private:
        RuleSet rules_{ RuleSet::InternationalMaxCapture };
        Color   side_{ Color::Red };
        int     ply_{ 0 };
        int     halfmoveClock_{ 0 };      // plies (not moves) since last man-move or capture
        std::uint64_t hash_{ 0 };
        std::unordered_map<std::uint64_t, std::uint16_t> reps_{};
    };

} // namespace draughts::core