#pragma once
/// @file SaveGame.hpp
/// @brief Serializes a GameEngine's full state to a human-readable text file
///        and reconstructs it on load. Uses core::Notation for move
///        formatting, and core::MoveGenerator for disambiguating capture
///        chains during parse.

#include "core/GameEngine.hpp"
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <QString>

namespace draughts::ui {

class SaveGame {
public:
    /// Write `engine` to `path` in text format. Returns true on success.
    static bool save(const core::GameEngine& engine,
                     core::Color             playerColor,
                     const QString&          path,
                     QString*                errorOut = nullptr);

    /// Restore from `path` into `engine`. `playerColorOut` receives the
    /// saved player colour. Returns true on success.
    static bool load(core::GameEngine& engine,
                     core::Color&      playerColorOut,
                     const QString&    path,
                     QString*          errorOut = nullptr);
};

} // namespace draughts::ui
