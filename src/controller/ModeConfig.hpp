#pragma once
/// @file ModeConfig.hpp
/// @brief How the current game is being played.

namespace draughts::controller {

enum class GameMode : unsigned char {
    HumanVsHuman = 0,
    HumanVsAI    = 1,
};

} // namespace draughts::controller
