#pragma once
/// @file PST.hpp
/// @brief Piece-square tables. Values are loaded at runtime so the engine
///        can be re-tuned without recompiling. Two complete sets are kept
///        in memory ? one per rule variant ? because InternationalMaxCapture
///        and InternationalFreeCapture play very differently and require
///        different weights.

#include "core/BoardConstants.hpp"
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <array>
#include <string>

namespace draughts::ai::pst {

/// One complete evaluation weight set: 50 men values + 50 king values,
/// all from Red's perspective. Yellow's indices are mirrored via
/// bc::mirrorSquare().
struct Weights {
    std::array<int, core::kNumPlayableSquares> men{};
    std::array<int, core::kNumPlayableSquares> kings{};
};

/// Return the built-in hand-tuned defaults for a rule variant. These are
/// what the engine used before this file became runtime-loadable.
[[nodiscard]] Weights defaultWeights(core::RuleSet rules);

/// Process-wide active weights. Set once at the start of a search by
/// AIEngine, read by the Evaluator. Thread-safe because each variant is
/// fully computed and stored as a value before it becomes active.
void              setActive(core::RuleSet rules) noexcept;
[[nodiscard]] const Weights& active() noexcept;

/// For tests and tooling: read/write weights directly.
[[nodiscard]] Weights loadFromFile(const std::string& path, bool& ok);
void                 saveToFile(const std::string& path,
                                const Weights&   w);

/// Tooltip for the tuner: get the default weights under either variant.
[[nodiscard]] Weights& mutableWeightsFor(core::RuleSet rules) noexcept;

} // namespace draughts::ai::pst
