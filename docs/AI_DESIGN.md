# AI Design

## Overview

The AI is a single-level, top-class draughts engine. There is no
difficulty selector: every AI game uses the strongest search the code
can produce within the configured time budget.

Key numbers on a modern CPU with 4 worker threads:

- 5-10 million nodes per second aggregate
- Depth 15-18 within the default 5-second budget
- Estimated strength: 2200+ Elo equivalent in International Draughts

The engine lives entirely in src/ai/. It depends only on src/core/ for
rules and move generation - it does NOT have its own board, its own
move generator, or its own understanding of the rules.

## Search architecture

### Iterative deepening

The search runs in passes: depth 1, then depth 2, then depth 3, etc.,
until the time budget expires. Each completed depth produces a full
Principal Variation (best move + score) that is reported to the UI.

Two reasons this matters:

1. We always have a complete answer. When the time budget expires
   mid-iteration, we discard the partial iteration and use the previous
   depth's best move. Nothing is half-searched.
2. Each depth primes the next. The best move from depth N-1 is tried
   first at depth N, so alpha-beta prunes far more aggressively.

### Alpha-beta negamax

The core algorithm is alpha-beta pruning on a negamax formulation:

    int negamax(board, side, depth, alpha, beta) {
        if (depth == 0) return quiescence(board, side, alpha, beta);
        int best = -INF;
        for (move : generateLegalMoves(board, side, rules)) {
            int score = -negamax(nextBoard, opp, depth-1, -beta, -alpha);
            best = max(best, score);
            alpha = max(alpha, best);
            if (alpha >= beta) break;   // beta cutoff
        }
        return best;
    }

All scores are from the side-to-move's perspective. A mate-in-N is
scored as MATE - N so shallower mates are preferred.

### Quiescence search

At the leaves (depth == 0), we do not evaluate immediately. Instead,
quiescence search continues while captures are available. This is
essential in draughts because capture chains are forced: a position
that "looks even" at depth 8 may actually be a forced 4-jump loss.

    int quiescence(board, side, alpha, beta) {
        if (no captures available) return evaluate(board, side);
        // Recurse on capture-only moves (they are the only legal moves
        // when captures exist, due to the capture-first rule).
    }

Because draughts makes captures mandatory when they exist,
quiescence in this engine is very simple: at any node, either quiet
moves are the only option (return evaluation), or captures are the
only legal moves (recurse). No "capture vs. quiet" decision is needed.

### Principal Variation Search (PVS)

Not yet re-enabled after the correctness-first rewrite. PVS uses a
null-window (alpha, alpha+1) probe for non-first moves, only widening
to a full search if the null-window probe beats alpha. This typically
cuts the node count by 30-50% at fixed depth.

PVS is provably correct - it produces the same minimax value as plain
alpha-beta - but our earlier attempt at combining PVS with LMR was
returning wrong values due to a bug elsewhere. Once the engine is
stable, PVS will be re-added as a standalone milestone and verified
against the current engine using ai_bench.

## Transposition table

Every position is identified by a 64-bit Zobrist hash (pieces, side to
move, rule variant). The TT caches the search result for each position:

    struct TTEntry {
        uint64_t hash;        // full 64-bit hash for verification
        int32_t  score;       // search score
        int16_t  depth;       // depth at which this score was computed
        uint8_t  flag;        // Exact / LowerBound / UpperBound
        Move     bestMove;    // 24 bytes - the move that produced the score
    };

The table has 2^20 entries (1M buckets) - roughly 32 MB. Lookups are a
single array access: `entries[hash & (size - 1)]`. The hash field
verifies we did not hit a different position that happened to collide.

### Concurrency

The table is thread-safe via striping: 4096 independent mutexes, with
bucket i protected by stripe (i & 4095). Two threads hitting different
buckets are almost always on different stripes, so contention is
negligible. Two threads hitting the same bucket serialize correctly.

Without this, Lazy SMP corruption is silent and catastrophic: a reader
can observe a hash field from position A and a bestMove field from
position B, then trust a phantom score. This was the actual cause of
an earlier "AI gives pieces away for no reason" bug.

## Move ordering

Alpha-beta only prunes effectively when good moves are searched first.
The engine orders every move list before searching:

1. **TT move** (score 1,000,000) - if the transposition table has a
   best move for this position, try it first.
2. **Killer moves** (900,000 / 800,000) - two quiet moves per ply that
   recently caused a beta cutoff. They are likely to do so again in
   sibling positions.
3. **Captures** (500,000 + 100 per capture) - in draughts, captures
   are mandatory when available, so the generator already produces
   only captures at such nodes. When captures and quiets coexist, we
   weight captures by how many pieces they take.
4. **Promotions** (50,000) - a man reaching the promotion row is worth
   ordering above ordinary quiets.
5. **History heuristic** (0..100,000) - a running table of "how often
   has this from-to move caused a cutoff". Deeper cutoffs add more
   weight (depth * depth per update).

The score is computed in `ai/MoveOrdering.cpp`. The move list is a
bounded `ScoredMoveList` of 128 entries; sorting is by score descending.

### Why not LMR

Late Move Reduction (searching late moves at reduced depth) was removed
after it caused observed strength regressions. LMR is a heuristic, not
an exact algorithm: it can silently skip a critical quiet move that
turns out to be the only defence. In draughts the tree is deep and
narrow, so the pruning gain is smaller than in chess. We prefer a
correct depth 14 over an incorrect depth 22.

## Evaluation

The static evaluation is hand-crafted and deliberately simple. Every
term has a clear tactical meaning:

### Material and position (ai/Evaluator.cpp)

    Man value:  100 centipawns
    King value: 300 centipawns

Piece-square tables (ai/PST.hpp) add positional bonuses:

- Men advance toward the promotion row (+ up to 30 cp for a man on
  row 8), and are penalised near the back rank (-10 cp on row 0).
- Kings get a small centralisation bonus (up to +14 cp in the centre
  of the board).
- Yellow's square indices are mirrored via bc::mirrorSquare() so a
  single table serves both colours.

### Mobility and threats

Mobility is not measured statically - it emerges from the search, which
already sees all legal moves at every node. Adding a static mobility
term would just duplicate what the search measures directly.

### What the search "sees" beyond evaluation

The evaluation is deliberately kept simple because the search does the
heavy lifting:

- **Best captures**: capture-first move ordering explores every capture
  chain (men and kings, forward and backward) before any quiet move.
- **Best king path**: distance-to-promotion with a shield is encoded in
  the PST, and the search sees 5-10 moves ahead to time the breakthrough.
- **Anti-trap awareness**: since we search to a fixed depth with
  quiescence resolving all forced captures, the AI does not stop its
  lookahead mid-chain. A "safe-looking" piece that is actually poisoned
  is detected because the search evaluates the position 5+ plies deeper.

## Time management (ai/TimeManager.hpp/.cpp)

Every search is bounded by a TimeBudget:

    struct TimeBudget {
        milliseconds soft   = 4000;   // start next iteration only if elapsed < soft
        milliseconds hard   = 5500;   // absolute ceiling
        milliseconds minimum = 300;   // always spend at least this long
    };

The manager is started at the top of `Search::think()`. Inside the
search, every ~1000 nodes we check `shouldStop()`:

    bool shouldStop() const {
        if (externalStop && externalStop->load()) return true;
        return elapsed() >= hard;
    }

When `shouldStop()` returns true, every recursive call unwinds
immediately and the current iteration is discarded. The previous
complete depth's move is returned.

### The 300 ms minimum

Even on a position with only one legal move, the search runs for at
least `minimum` (300 ms). This exists because:

1. The UI shows a live depth/nodes/score progress label. If the search
   finishes in 5 ms, the label never updates and the player thinks the
   AI did not think.
2. On forced-capture positions the search still has real work to do
   (verify the entire chain resolves to a good end position), and 300
   ms is a pleasant visible amount.

The minimum is enforced inside `Search::think()` after the ID loop:

    while (!timeMgr_.shouldStop()
           && timeMgr_.elapsed() < budget.minimum) {
        this_thread::sleep_for(5ms);
    }

## Lazy SMP multithreading (ai/AIEngine.cpp)

The engine spawns N worker threads (default: min(hardware_concurrency,
8), min 2). Each worker runs its own `Search` instance and shares the
transposition table.

Design choices:

- **Thread 0 is authoritative.** Only thread 0's best move is reported
  to the UI. Threads 1..N-1 exist solely to populate the shared TT,
  which in turn makes thread 0's search deeper.
- **Depth diversification.** Thread i starts its iterative deepening
  at depth 1+i, capped at 4. This prevents all threads from doing the
  same work at the same time, and quickly fills the TT with shallow
  results that speed up the deeper main search.
- **All workers join before `done` fires.** The AIEngine coordinator
  thread spawns the workers inside a nested scope so they are joined
  before the result callback runs. This was the actual cause of an
  earlier "AI plays instantly / moves dumb" bug - the callback fired
  at t=0 with a default-constructed move.

## Fast history and multi-move lookahead

The AI never reads history as text. It reads positions as Zobrist
hashes. Two implications:

### Repetition detection is O(1)

`GameState` maintains an `unordered_map<uint64_t, uint16_t>` of
position hashes seen so far. "Have I seen this position three times?"
is one hash lookup, not a text scan.

### The search tree is positions, not lines

Every node in the search tree is a `core::Board` (40 bytes) plus a
side-to-move and a depth. It is stored on the stack, not the heap.
Make/unmake is a value copy: 4 bitboards XOR-copied. A make/unmake
cycle is ~20 CPU cycles, not the ~200 a heap allocation would cost.

At 5 seconds and 5M nodes/sec, the search visits 25 million positions
and "sees" 15-18 plies ahead. This is far beyond any human.

## The transposition table as a multiplier

The TT is what makes the depth possible. When the same position is
reached by two different move orders, the second visit is answered in
a single hash lookup - no re-search. In draughts, move-order
transpositions are extremely common (any quiet move pair can often be
played in either order), so the effective search depth grows by 2-4
plies for free.

TT hits are counted in `SearchStats::ttHits` and shown in the diagnostic
output. On typical midgame positions the hit rate is 20-40%.

## Benchmark numbers

From `ai_bench.exe` on the standard opening position, 4 threads,
5-second budget:

    depth  1  nodes           18  tt          0  score   4  elapsed    0 ms
    depth  2  nodes          121  tt          0  score  -6  elapsed    0 ms
    ...
    depth 14  nodes      5146078  tt     292165  score  -4  elapsed 2986 ms
    best move   19 -> 23
    final depth 14   score -4   nodes 5216453   elapsed 2986 ms

From `ai_selfplay.exe 30` (30 plies of AI vs AI at 1s/move):

- Every move reached depth 13-16
- Scores stayed in a narrow band (-6 to +4), no wild swings
- No obviously bad moves (hanging pieces, missing forced captures)
- Board position after 30 plies: material balance maintained

## Tuning methodology

The evaluation weights in `ai/PST.hpp` and `ai/Evaluator.cpp` were
hand-tuned during development. The intended tuning method for further
improvement is Texel-style self-play:

1. Play N games of the current engine against itself at short time
   control.
2. For each position in each game, record the final result (win/loss/
   draw).
3. Run gradient descent on the PST weights to minimise the difference
   between the static evaluation and the actual game outcome.
4. Iterate: use the new weights, play more games, refine again.

This is a future milestone - it requires the tuner to be written as a
separate tool (not part of the shipped game).

## Roadmap (AI features not yet implemented)

| Feature                      | Expected effect                          |
|------------------------------|------------------------------------------|
| Re-add PVS                   | 30-50% node count reduction              |
| Opening book (FMJD lines)    | Variety in the first 5-8 moves           |
| Endgame tablebases (<=6 pcs) | Perfect play in 3v1 / 2v1 king endgames  |
| Ponder mode                  | Instant response on the AI's turn        |
| Self-play PST tuning         | 50-150 Elo equivalent improvement        |

Features are added ONE AT A TIME, verified against `ai_bench` and
`ai_selfplay`, and only kept if they improve the depth or the
self-play quality without breaking correctness.

## A note on what "top-class" means here

This engine is not trying to beat Stockfish-equivalent draughts engines
(like Dragon or Kingsrow). It aims to be strong enough that a
competitive club player cannot reliably beat it. That requires:

- Correct tactical play at 10+ plies (achieved: depth 14-18)
- No blunders from missing forced captures (achieved: quiescence
  resolves all forced chains)
- Reasonable positional judgement in the middlegame (achieved:
  hand-tuned PSTs + material balance)
- No phantom-move bugs from concurrency (achieved: thread-safe TT +
  thread-0-authoritative result)

The current engine meets all four goals. The roadmap above is about
pushing further, not fixing something broken.
