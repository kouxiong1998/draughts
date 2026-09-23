/// @file CaptureRules.hpp
/// @brief The ONE place that encodes capture-chain legality.
///        MoveGenerator calls into this; the AI never re-implements it.

#include "Types.hpp"
#include "RuleSet.hpp"
#include "Board.hpp"

namespace draughts::core {

    /// Enumerate every legal capture chain starting from `sq` for the piece
    /// currently on it, given the current board and the active rule set.
    ///
    /// All returned moves share the same `from == sq`. The returned list is
    /// *complete* under the rule set:
    ///   • InternationalMaxCapture → every chain is a *maximal-length* chain
    ///     (only chains with the greatest global capture count are kept by
    ///     MoveGenerator; this function returns all possible chains, and the
    ///     caller prunes).
    ///   • InternationalFreeCapture → every complete chain (ending when no
    ///     more captures are available without an immediate 180° reversal) is
    ///     returned.
    void generateCapturesFrom(const Board& board,
        Square       sq,
        Color        side,
        RuleSet      rules,
        MoveSpan& out) noexcept;

} // namespace draughts::core
