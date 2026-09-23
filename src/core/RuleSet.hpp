#pragma once
/// @file RuleSet.hpp
/// @brief The two supported rule variants. Everything rule-related keys off
///        this enum — never off ad-hoc booleans scattered around the tree.

namespace draughts::core {

    enum class RuleSet : unsigned char {
        /// Standard FMJD International Draughts: majority rule (must play the
        /// maximal capture sequence). Ties → player may choose.
        InternationalMaxCapture = 0,
        /// Custom variant: player may take any capture direction, but the chain
        /// must not perform an immediate 180° reversal after a capture step.
        InternationalFreeCapture = 1,
    };

    [[nodiscard]] constexpr bool maxCaptureEnabled(RuleSet r) noexcept {
        return r == RuleSet::InternationalMaxCapture;
    }

} // namespace draughts::core
