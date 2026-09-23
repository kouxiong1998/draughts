#pragma once
/// @file Perft.hpp
/// @brief Move-generation correctness harness. Compares our generator
///        against published FMJD perft numbers.

#include "Board.hpp"
#include "RuleSet.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace draughts::core {

    struct PerftResult {
        int      depth{ 0 };
        std::uint64_t nodes{ 0 };
        std::uint64_t captures{ 0 };
        std::uint64_t promotions{ 0 };
        double   milliseconds{ 0.0 };
    };

    /// Count leaf nodes of the legal-move tree for `depth` plies.
    [[nodiscard]] PerftResult perft(const Board& board,
        Color        side,
        RuleSet      rules,
        int          depth);

    /// Divide (per-move node counts) — great for debugging generator regressions.
    struct PerftDivideEntry { std::string move; std::uint64_t nodes{ 0 }; };
    [[nodiscard]] std::vector<PerftDivideEntry>
        perftDivide(const Board& board, Color side, RuleSet rules, int depth);

} // namespace draughts::core