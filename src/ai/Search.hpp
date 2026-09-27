#pragma once
/// @file Search.hpp
/// @brief PVS + aspiration + shared transposition table + killers + history
///        + LMR. Stateless with respect to the TT so it can be instantiated
///        once per SMP worker thread.

#include "TimeManager.hpp"
#include "TranspositionTable.hpp"
#include "MoveOrdering.hpp"
#include "core/Board.hpp"
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>
#include <functional>
#include <random>

namespace draughts::ai {

inline constexpr int kMateScore = 100000;
inline constexpr int kMaxPly    = 48;

struct SearchStats {
    int           depth{0};
    std::uint64_t nodes{0};
    std::uint64_t ttHits{0};
    int           score{0};
    core::Move    bestMove{};
    std::chrono::milliseconds elapsed{0};
    bool          completed{false};
};

using ProgressFn = std::function<void(const SearchStats&)>;

/// Path-dependent draw counters carried through the search. Every ply
/// these advance exactly like GameEngine does, so a position that would
/// be declared a draw by the rules is scored as a draw inside the search.
struct DrawContext {
    int halfmove {0};   // 25-move rule: 50 consecutive king-only plies
    int kings3v1 {0};   // FMJD 3v1 king endgame: 32 plies
    int kings2v1 {0};   // FMJD 2v1 king endgame: 10 plies
    int fourPiece{0};   // FMJD 4-piece rule: 40 plies
};

class Search {
public:
    /// `sharedTT` must outlive this Search. `stopFlag` is shared across
    /// threads so any worker's cancelation cancels all of them.
    Search(TranspositionTable* sharedTT,
           std::atomic<bool>*   stopFlag,
           ProgressFn           onProgress);

    /// Reseed the internal RNG. Used by the opening-book generator to
    /// make each game deterministic given a per-game seed, so a run can
    /// be resumed after a crash without replaying previous games.
    void setSeed(std::uint64_t s) noexcept { rng_.seed(s); }

    /// Contempt factor: a small score bonus/penalty applied to drawn
    /// positions. Positive values make the engine avoid draws; negative
    /// values make it seek draws; zero (the default) treats a draw as
    /// a neutral outcome.
    void setContempt(int c) noexcept { contempt_ = c; }

    /// Draw counters for the current game path. The search advances
    /// these on every ply and treats counter limits as draws.
    void setDrawContext(const DrawContext& dc) noexcept { rootDraw_ = dc; }

    /// Run a fixed-depth search and return the top-N scored moves,
    /// sorted by score descending. Used by the opening-book filler.
    [[nodiscard]] std::vector<std::pair<core::Move, int>>
    topMoves(const core::Board& board,
             core::Color        side,
             core::RuleSet      rules,
             int                depth,
             int                N);

    /// Iterative-deepening search. `threadId` is used only to diversify the
    /// starting depth across workers (0, 1, 2, ...). Returns the deepest
    /// fully-completed result this worker achieved.
    SearchStats think(const core::Board& board,
                      core::Color        side,
                      core::RuleSet      rules,
                      TimeBudget         budget,
                      int                threadId = 0);

private:
    TranspositionTable* tt_{nullptr};
    std::atomic<bool>*  stopFlag_{nullptr};
    ProgressFn          onProgress_;
    std::mt19937_64    rng_{std::random_device{}()};
    TimeManager         timeMgr_{TimeBudget{}, nullptr};

    std::array<std::array<core::Move, 2>, kMaxPly> killers_{};
    std::array<std::array<int, 50>, 50>            history_{};
    std::uint64_t nodes_{0};
    std::uint64_t ttHits_{0};
    core::Color rootSide_{core::Color::Red};
    int         contempt_{0};
    std::array<std::uint64_t, kMaxPly> pathHashes_{};
    DrawContext rootDraw_{};
    bool                     lastWasNull_{false};

    int  negamax(const core::Board& board, core::Color side, core::RuleSet rules,
                 int depth, int alpha, int beta, int ply, DrawContext dc);
    int  quiescence(const core::Board& board, core::Color side, core::RuleSet rules,
                    int alpha, int beta, int ply);

    void applyMoveInPlace(core::Board& board, const core::Move& m) const noexcept;

    [[nodiscard]] std::uint64_t positionHash(const core::Board& board,
                                             core::Color        side,
                                             core::RuleSet      rules) const noexcept;

    void scoreAndSort(ScoredMoveList& out, std::size_t& count,
                      const core::MoveSpan& legal,
                      const core::Move& ttMove, int ply) const noexcept;

    void updateKillers(const core::Move& m, int ply) noexcept;
    void updateHistory(const core::Move& m, int depth) noexcept;
    [[nodiscard]] bool isRepetitionInPath(std::uint64_t hash, int ply) const noexcept;
    [[nodiscard]] static DrawContext advanceDrawContext(const core::Board& pre,
                                                      const core::Board& post,
                                                      const core::Move&  m,
                                                      DrawContext        dc) noexcept;
};

} // namespace draughts::ai
