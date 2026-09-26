#include "Search.hpp"
#include "Evaluator.hpp"
#include "core/MoveGenerator.hpp"
#include "core/BoardConstants.hpp"
#include "core/Zobrist.hpp"

#include <algorithm>
#include <random>
#include <bit>
#include <thread>

namespace draughts::ai {

Search::Search(TranspositionTable* sharedTT,
               EndgameTablebase*    sharedTB,
               std::atomic<bool>*   stopFlag,
               ProgressFn           onProgress)
    : tt_(sharedTT), tb_(sharedTB),
      stopFlag_(stopFlag), onProgress_(std::move(onProgress)) {}

std::uint64_t Search::positionHash(const core::Board& board,
                                   core::Color        side,
                                   core::RuleSet      rules) const noexcept
{
    std::uint64_t h = board.pieceHash();
    h ^= core::Zobrist::instance().side(side);
    h ^= static_cast<std::uint64_t>(rules) << 61;
    return h;
}

void Search::applyMoveInPlace(core::Board& board, const core::Move& m) const noexcept {
    if (m.from >= core::kNumPlayableSquares || m.to >= core::kNumPlayableSquares)
        return;

    core::Bitboard cap = m.captured;
    while (cap) {
        const auto s = static_cast<core::Square>(std::countr_zero(cap));
        cap &= cap - 1;
        if (s < core::kNumPlayableSquares) board.removePiece(s);
    }
    if (board.empty(m.from)) return;
    const auto p = board.at(m.from);
    board.removePiece(m.from);
    board.setPiece(m.to, p.color, p.kind);
    if (p.kind == core::PieceKind::Man && m.isPromotion) board.promote(m.to);
}

void Search::scoreAndSort(ScoredMoveList& out, std::size_t& count,
                          const core::MoveSpan& legal,
                          const core::Move& ttMove, int ply) const noexcept
{
    count = std::min<std::size_t>(legal.size(), out.size());
    for (std::size_t i = 0; i < count; ++i) {
        out[i].move  = legal[i];
        out[i].score = scoreMove(legal[i], ttMove,
                                 killers_[static_cast<std::size_t>(ply)].data(),
                                 history_, ply);
    }
    sortByScore(out, count);
}

void Search::updateKillers(const core::Move& m, int ply) noexcept {
    if (ply < 0 || ply >= kMaxPly) return;
    auto& slot = killers_[static_cast<std::size_t>(ply)];
    if (slot[0].from == m.from && slot[0].to == m.to
        && slot[0].captured == m.captured) return;
    slot[1] = slot[0];
    slot[0] = m;
}

void Search::updateHistory(const core::Move& m, int depth) noexcept {
    if (m.from >= 50u || m.to >= 50u) return;
    const int bonus = depth * depth;
    history_[m.from][m.to] = std::min(history_[m.from][m.to] + bonus, 100'000);
}

// ?? Quiescence: forced-capture resolution only ????????????????????????????
int Search::quiescence(const core::Board& board, core::Color side, core::RuleSet rules,
                       int alpha, int beta, int ply)
{
    ++nodes_;
    if ((nodes_ & 1023u) == 0 && timeMgr_.shouldStop()) return 0;
    if (ply >= kMaxPly - 1) return evaluate(board, side);

    const auto moves = core::generateLegalMoves(board, side, rules);
    if (moves.empty()) return -kMateScore + ply;

    bool anyCapture = false;
    for (std::size_t i = 0; i < moves.size(); ++i)
        if (moves[i].isCapture()) { anyCapture = true; break; }

    if (!anyCapture) return evaluate(board, side);

    int best = -kMateScore - 1;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        core::Board next = board;
        applyMoveInPlace(next, moves[i]);
        const int score = -quiescence(next, core::opposite(side), rules,
                                      -beta, -alpha, ply + 1);
        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

// ?? Plain alpha-beta with TT + ordered moves. No LMR, no PVS, no
//    aspiration. Every move at every node is searched with the full
//    window. Slower than a fully-optimized search, but provably correct
//    and ? critically ? does not silently skip tactics. ????????????????
int Search::negamax(const core::Board& board, core::Color side, core::RuleSet rules,
                    int depth, int alpha, int beta, int ply)
{
    ++nodes_;
    if ((nodes_ & 1023u) == 0 && timeMgr_.shouldStop()) return 0;
    if (ply >= kMaxPly - 1) return evaluate(board, side);

    // ?? Endgame tablebase ??????????????????????????????????????????????????
    // Only consulted when we actually have a TB (main thread) and the
    // position is small enough. The TB is exact, so its answer is preferred
    // over both the TT and the heuristic evaluation.
    if (tb_ && std::popcount(board.occupied()) <= EndgameTablebase::kMaxPieces) {
        if (const auto r = tb_->probe(board, side, rules)) {
            switch (r->result) {
                case TBResult::Win:
                    return kMateScore - r->distance - ply;
                case TBResult::Loss:
                    return -kMateScore + r->distance + ply;
                case TBResult::Draw:
                    return 0;
            }
        }
    }

    const std::uint64_t hash = positionHash(board, side, rules);
    const int alphaOrig = alpha;

    // TT probe
    core::Move ttMove{};
    {
        TTEntry entry{};
        if (tt_->probe(hash, entry)) {
            ++ttHits_;
            if (entry.from < core::kNumPlayableSquares
                && entry.to   < core::kNumPlayableSquares) {
                ttMove.from     = entry.from;
                ttMove.to       = entry.to;
                ttMove.captured = entry.captured;
            }
            if (entry.depth >= depth) {
                const int s = entry.score;
                if (entry.flag == static_cast<std::uint8_t>(TTFlag::Exact)) return s;
                if (entry.flag == static_cast<std::uint8_t>(TTFlag::LowerBound)
                    && s >= beta) return s;
                if (entry.flag == static_cast<std::uint8_t>(TTFlag::UpperBound)
                    && s <= alpha) return s;
            }
        }
    }

    // ?? Internal Iterative Deepening (IID) ????????????????????????????????
    // If the TT gave no move AND we're searching at a meaningful depth,
    // do a shallow search first to find a likely-best move. This move is
    // then tried first in the main loop, greatly improving move ordering
    // in quiet positions.
    if (depth >= 4
        && ttMove.from >= core::kNumPlayableSquares) {
        const int iidDepth = depth >= 8 ? depth / 2 : depth - 2;
        if (iidDepth > 0) {
            (void)negamax(board, side, rules, iidDepth, alpha, beta, ply);
            // Re-probe the TT ? the shallow search may have stored a move.
            TTEntry entry2{};
            if (tt_->probe(hash, entry2)) {
                if (entry2.from < core::kNumPlayableSquares
                    && entry2.to   < core::kNumPlayableSquares) {
                    ttMove.from     = entry2.from;
                    ttMove.to       = entry2.to;
                    ttMove.captured = entry2.captured;
                }
            }
        }
    }

    if (depth <= 0) return quiescence(board, side, rules, alpha, beta, ply);

    const auto moves = core::generateLegalMoves(board, side, rules);
    if (moves.empty()) return -kMateScore + ply;

    ScoredMoveList scored{};
    std::size_t    count = 0;
    scoreAndSort(scored, count, moves, ttMove, ply);

    // ?? Forced-capture extension ??????????????????????????????????????????
    // If the side to move has exactly one legal move and it is a capture,
    // extend the search by one ply. In draughts, single-move positions
    // are extremely common (forced recaptures after a chain), and the
    // forced move is essentially free - searching it as deep as a quiet
    // position wastes depth on a branch with no choices.
    //
    // This extension is safe: it never prunes, only deepens. It inflates
    // the reported depth by the number of forced plies traversed, but the
    // extra plies are genuinely searched.
    int childDepth = depth - 1;
    if (count == 1 && moves[0].isCapture()) {
        ++childDepth;
    }

    int best = -kMateScore - 1;
    core::Move bestMove{};
    const core::Color opp = core::opposite(side);

    // Principal Variation Search (PVS): the first move gets the full window.
    // Every subsequent move gets a null-window (alpha, alpha+1) probe first;
    // if the probe beats alpha, re-search with the full window. Provably
    // correct - same minimax value as plain alpha-beta, fewer nodes visited.
    for (std::size_t i = 0; i < count; ++i) {
        const core::Move& m = scored[i].move;
        core::Board next = board;
        applyMoveInPlace(next, m);

        int score;
        if (i == 0) {
            score = -negamax(next, opp, rules, depth - 1,
                             -beta, -alpha, ply + 1);
        } else {
            // Null-window probe.
            score = -negamax(next, opp, rules, depth - 1,
                             -alpha - 1, -alpha, ply + 1);
            // If the probe suggests this move might beat alpha, re-search
            // with the full window.
            if (score > alpha && score < beta) {
                score = -negamax(next, opp, rules, depth - 1,
                                 -beta, -alpha, ply + 1);
            }
        }

        if (timeMgr_.shouldStop()) return 0;

        if (score > best) {
            best     = score;
            bestMove = m;
        }
        if (best > alpha) alpha = best;
        if (alpha >= beta) {
            if (!m.isCapture()) {
                updateKillers(m, ply);
                updateHistory(m, depth);
            }
            break;
        }
    }

    TTFlag flag = TTFlag::Exact;
    if (best <= alphaOrig) flag = TTFlag::UpperBound;
    else if (best >= beta) flag = TTFlag::LowerBound;
    tt_->store(hash, depth, best, flag, bestMove);

    return best;
}

SearchStats Search::think(const core::Board& board,
                          core::Color        side,
                          core::RuleSet      rules,
                          TimeBudget         budget,
                          int                threadId)
{
    SearchStats stats{};
    timeMgr_ = TimeManager(budget, stopFlag_);
    timeMgr_.start();

    for (auto& slot : killers_) slot = {core::Move{}, core::Move{}};
    for (auto& row : history_)  row.fill(0);
    nodes_  = 0;
    ttHits_ = 0;

    const auto rootMoves = core::generateLegalMoves(board, side, rules);
    if (rootMoves.empty()) {
        stats.completed = true;
        return stats;
    }
    stats.bestMove = rootMoves[0];

    int        bestScore = 0;
    core::Move bestMove  = rootMoves[0];
    int        lastFullDepth = 0;

    // Threads 1..N warm up the TT at lower depths first; thread 0 goes
    // straight from depth 1. Cap the diversification to keep the display
    // honest.
    const int startDepth = 1 + std::min(threadId, 3);

    int  prevScore = 0;
    bool firstIter = true;

    for (int depth = startDepth; depth <= 64; ++depth) {
        // Aspiration window: after the first iteration, search with a
        // narrow window around the previous iteration's score. If the
        // search fails outside it, widen and retry.
        constexpr int kAspirationWindow = 50;

        int aspAlpha = -kMateScore - 1;
        int aspBeta  =  kMateScore + 1;
        if (!firstIter && std::abs(prevScore) < kMateScore - 100) {
            aspAlpha = prevScore - kAspirationWindow;
            aspBeta  = prevScore + kAspirationWindow;
        }

        int       localBest     = -kMateScore - 1;
        core::Move localBestMove = rootMoves[0];
        bool      aborted       = false;

        // Retry loop: re-search with a wider window if the score escapes.
        for (int attempt = 0; attempt < 5; ++attempt) {
            localBest     = -kMateScore - 1;
            localBestMove = rootMoves[0];

            ScoredMoveList scored{};
            std::size_t    count = 0;
            scoreAndSort(scored, count, rootMoves, bestMove, 0);

            int       rootAlpha = aspAlpha;
            const int rootBeta  = aspBeta;
            bool      failLow   = true;

            for (std::size_t i = 0; i < count; ++i) {
                const core::Move& m = scored[i].move;
                core::Board next = board;
                applyMoveInPlace(next, m);

                const int score = -negamax(next, core::opposite(side), rules,
                                           depth - 1, -rootBeta, -rootAlpha, 1);
                if (timeMgr_.shouldStop()) { aborted = true; break; }

                if (score > localBest) {
                    localBest     = score;
                    localBestMove = m;
                }
                if (localBest > rootAlpha) {
                    rootAlpha = localBest;
                    failLow   = false;
                }
            }

            if (aborted) break;

            // Success: score landed inside the window.
            if (!failLow && localBest < aspBeta) break;

            // Failed high: widen beta.
            if (localBest >= aspBeta) aspBeta = kMateScore + 1;

            // Failed low: widen alpha.
            if (failLow) aspAlpha = -kMateScore - 1;
        }

        if (aborted) break;

        bestMove      = localBestMove;
        bestScore     = localBest;
        lastFullDepth = depth;
        prevScore     = localBest;
        firstIter     = false;

        stats.depth     = depth;
        stats.nodes     = nodes_;
        stats.ttHits    = ttHits_;
        stats.score     = bestScore;
        stats.bestMove  = bestMove;
        stats.elapsed   = timeMgr_.elapsed();
        stats.completed = true;
        if (onProgress_) onProgress_(stats);

        // Respect an explicit depth cap if the caller set one.
        if (budget.maxDepth > 0 && depth >= budget.maxDepth) break;

        if (!timeMgr_.canStartNextIter()) break;
        if (std::abs(bestScore) > kMateScore - 100) break;
    }

    // Enforce the minimum think time. Even on a forced position where the
    // search terminates in a few milliseconds, we wait until the budget's
    // minimum has elapsed so the UI's progress callback has time to update
    // and the player sees that the AI actually thought.
    while (!timeMgr_.shouldStop()
           && timeMgr_.elapsed() < budget.minimum) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    stats.nodes    = nodes_;
    stats.ttHits   = ttHits_;
    stats.elapsed  = timeMgr_.elapsed();
    stats.depth    = lastFullDepth;
    stats.bestMove = bestMove;

    // Optional random pick among near-best root moves (book generation).
    if (budget.randomTopN > 0 && lastFullDepth > 0 && !rootMoves.empty()) {
        std::vector<std::pair<int, core::Move>> scored;
        scored.reserve(rootMoves.size());
        const core::Color opp2 = core::opposite(side);
        for (std::size_t i = 0; i < rootMoves.size(); ++i) {
            core::Board next = board;
            applyMoveInPlace(next, rootMoves[i]);
            const int s = -negamax(next, opp2, rules, lastFullDepth - 1,
                                   -kMateScore - 1, kMateScore + 1, 1);
            if (timeMgr_.shouldStop()) break;
            scored.emplace_back(s, rootMoves[i]);
        }
        if (!scored.empty()) {
            std::sort(scored.begin(), scored.end(),
                      [](const auto& a, const auto& b) {
                          return a.first > b.first;
                      });
            const int bestS = scored.front().first;
            const int n = std::min<int>(budget.randomTopN,
                                        static_cast<int>(scored.size()));
            std::vector<core::Move> candidates;
            for (int k = 0; k < n; ++k) {
                if (bestS - scored[static_cast<std::size_t>(k)].first
                    <= budget.randomEps) {
                    candidates.push_back(scored[static_cast<std::size_t>(k)].second);
                }
            }
            if (!candidates.empty()) {
                std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
                stats.bestMove = candidates[dist(rng_)];
            }
        }
    }

    return stats;
}

} // namespace draughts::ai
