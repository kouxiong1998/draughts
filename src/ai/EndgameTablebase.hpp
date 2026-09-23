#pragma once
/// @file EndgameTablebase.hpp
/// @brief On-demand exact solver for small endgames (<=4 pieces).
///
/// Rather than shipping multi-gigabyte tablebase files, we solve positions
/// on demand with a memoized minimax. The result is cached in a hash map
/// so subsequent probes for the same position are O(1).
///
/// Typical use: inside the search, before evaluating a leaf that has
/// <= 4 pieces, ask the tablebase. If it knows the answer, return it
/// directly. Otherwise fall through to the normal evaluation.
///
/// Encoding: `solveRec` returns an int from the side-to-move perspective:
///   value > 0  : side to move wins in exactly `value` plies
///   value < 0  : side to move loses in exactly `-value - 1` plies
///   value == 0 : draw
/// So the terminal (no legal moves) case is `-1` (loss in 0 plies).

#include "core/Board.hpp"
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace draughts::ai {

enum class TBResult : std::uint8_t {
    Draw = 0,
    Win  = 1,   ///< side to move has a forced win
    Loss = 2,   ///< side to move is lost with perfect play
};

struct TBProbe {
    TBResult result;
    int      distance;   ///< plies to terminal (Win/Loss); 0 for draw
};

class EndgameTablebase {
public:
    /// Positions with more than this many pieces are ignored by `probe`.
    static constexpr int kMaxPieces = 4;

    /// Maximum search depth inside `solveRec`. Past this we treat a
    /// branch as a draw to keep the recursion finite.
    static constexpr int kMaxPlies = 40;

    /// Upper bound on cached entries (roughly 16 MB at 16 bytes each).
    static constexpr std::size_t kMaxEntries = 1u << 20;

    /// Time budget for a single top-level `probe()` call.
    static constexpr int kBudgetMs = 500;

    EndgameTablebase() = default;

    /// Look up or compute the exact result of `board` for `side` under
    /// `rules`. Returns nullopt if:
    ///   - the position has more than kMaxPieces pieces, or
    ///   - the solve budget was exceeded without a definitive answer, or
    ///   - `rules` is anything other than the two supported variants
    ///     (future-proofing; both are handled today).
    [[nodiscard]] std::optional<TBProbe> probe(const core::Board& board,
                                                core::Color        side,
                                                core::RuleSet      rules);

    [[nodiscard]] std::size_t size()   const noexcept { return cache_.size(); }
    [[nodiscard]] std::size_t probes() const noexcept { return probes_; }
    [[nodiscard]] std::size_t nodes()  const noexcept { return nodes_;  }
    void clear() noexcept { cache_.clear(); nodes_ = 0; }

private:
    /// `result` uses TBResult's underlying values; `distance` is in plies.
    struct Entry {
        std::uint8_t result;    ///< TBResult
        std::uint8_t pad0;
        std::int16_t distance;
        std::uint32_t pad1;
    };

    std::unordered_map<std::uint64_t, std::int32_t> cache_;
    std::size_t probes_{0};
    std::size_t nodes_{0};

    // Per-probe budget state.
    std::chrono::steady_clock::time_point budgetStart_{};
    bool                                  budgetExceeded_{false};

    [[nodiscard]] std::uint64_t hashOf(const core::Board& board,
                                        core::Color        side,
                                        core::RuleSet      rules) const noexcept;

    [[nodiscard]] int pieceCount(const core::Board& board) const noexcept;

    /// Recursive minimax. Encodes distance as documented at the top of
    /// this file. Returns INT_MIN on budget exceedance.
    [[nodiscard]] int solveRec(const core::Board& board,
                                core::Color        side,
                                core::RuleSet      rules,
                                int                depthLimit);
};

} // namespace draughts::ai
