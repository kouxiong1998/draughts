#include "GameState.hpp"

namespace draughts::core {

    void GameState::reset(Color first, RuleSet rules) noexcept {
        side_ = first;
        rules_ = rules;
        ply_ = 0;
        halfmoveClock_ = 0;
        reps_.clear();
    }

    void GameState::applyMoveBookkeeping(Color mover, bool movedKing, bool wasCapture) noexcept {
        // 25-move king-only rule counts *plies*, but only while both sides are
        // making non-capture king moves. Any man move or capture resets it.
        if (wasCapture || !movedKing) halfmoveClock_ = 0;
        else                          ++halfmoveClock_;

        side_ = opposite(mover);
        ++ply_;
    }

    void GameState::notePosition(std::uint64_t positionHash) noexcept {
        ++reps_[positionHash];
        hash_ = positionHash;
    }

    int GameState::repetitionCount(std::uint64_t positionHash) const noexcept {
        if (const auto it = reps_.find(positionHash); it != reps_.end())
            return it->second;
        return 0;
    }

    bool GameState::isThreefoldRepetition(std::uint64_t positionHash) const noexcept {
        return repetitionCount(positionHash) >= 3;
    }

} // namespace draughts::core
