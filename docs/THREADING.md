# Threading

## Overview

The game runs on multiple threads. The rules are strict: the UI thread
must NEVER block, and the AI must be cancellable within milliseconds
at any moment. This document describes the exact contract.

## Thread model

There are three kinds of threads:

1. **The UI thread.** This is the Qt main thread. It runs the event
   loop, processes mouse and keyboard events, repaints the board, and
   runs every widget callback. Nothing on this thread may block for
   more than a few milliseconds.

2. **The AI coordinator thread.** Exactly one per AIEngine instance.
   Its job is to spawn worker threads, join them, and fire the done
   callback. It does no search itself. It is a `std::jthread` owned
   by `AIEngine`.

3. **The AI worker threads.** N of them (default 2-8). Each runs an
   independent `Search::think()` instance. All workers share one
   `TranspositionTable` and one `std::atomic<bool> stopFlag_`. Worker
   threads are `std::jthread` objects owned by a `std::vector` inside
   the coordinator's `runSMP()` scope.

Thread count:

    int AIEngine::threadCount() {
        const unsigned hw = std::thread::hardware_concurrency();
        return clamp(hw == 0 ? 2 : hw, 2, 8);
    }

## Who owns what

    UI thread                 Coordinator thread           Worker threads
    ----------                ------------------           --------------
    QApplication              AIEngine::worker_            AIEngine::runWorker
    QMainWindow                  (std::jthread)                (per thread,
    GameController            AIEngine::runSMP()                N instances)
    GameEngine
    BoardWidget
    SidePanel
    HistoryPanel
    SoundManager
    Animator

Objects that live on the UI thread only:

- The Qt widget tree
- `core::GameEngine` (the live game state)
- `GameController` and all its signals/slots

Objects shared between threads:

- `ai::AIEngine::stopFlag_` (std::atomic<bool>, relaxed)
- `ai::AIEngine::thinking_` (std::atomic<bool>, relaxed)
- `ai::AIEngine::generation_` (std::atomic<uint64_t>, relaxed)
- `ai::AIEngine::tt_` (thread-safe via 4096-way striping)
- `ai::AIEngine::bestStats_` (mutex-protected)
- `ai::AIEngine::progressCb_` and `doneCb_` (mutex-protected)

The `core::Board` passed to `AIEngine::think()` is copied by value
into the coordinator thread and then into each worker thread. No
`Board` reference crosses a thread boundary.

## Cancelation contract

The stop flag is the primary cancelation mechanism. It is set by the
UI thread and read by workers every ~1000 nodes.

    std::atomic<bool> stopFlag_;

    // UI side (any thread):
    stopFlag_.store(true, std::memory_order_relaxed);

    // Worker side (inside negamax, quiescence):
    if ((nodes_ & 1023u) == 0 && timeMgr_.shouldStop()) return 0;

Every public method that can run while the AI is thinking first calls
`ai_.stop(...)`:

    void GameController::undo() {
        ai_.stop(std::chrono::milliseconds(1500));
        // ... then mutate engine state ...
    }

The same pattern applies to: `newGame`, `redo`, `resign`, `setMode`,
`adoptEngine`. The order is always: stop first, then mutate.

### Cancelation timing

From `stopFlag_.store(true)` to the worker threads returning:

    - Up to 1023 nodes of work in progress:   ~1 microsecond
    - Coordinating join of N workers:         ~10-50 microseconds
    - AIEngine::stop() polling overhead:      ~2 milliseconds per check

End-to-end, a cancel lands within 5 milliseconds. From the player's
perspective, Undo during AI thinking is instant.

`AIEngine::stop()` is bounded: it polls for up to `wait` (default
500 ms). If the worker has not exited by then, `stop()` returns false
and the caller proceeds anyway. The worker's eventual exit is not
required for correctness - the worker checks `generation_` before
firing the done callback.

### What sets the stop flag

| Event                          | Set by                   |
|--------------------------------|--------------------------|
| User clicks Undo               | `GameController::undo`   |
| User clicks Redo               | `GameController::redo`   |
| User clicks New Game           | `GameController::newGame`|
| User clicks Give Up            | `GameController::resign` |
| User switches mode             | `GameController::setMode`|
| User loads a save file         | `GameController::adoptEngine` |
| A new AI turn begins           | `AIEngine::think` (via previous search) |
| AIEngine destructor            | `~AIEngine`              |

## Callback contract

Two callbacks are registered by `GameController`:

    ai_.setProgressCallback([this](const SearchStats& s) { ... });
    ai_.setDoneCallback    ([this](const Move&, const SearchStats&) { ... });

### Progress callback

Fires once per completed depth, on a worker thread. `GameController`
re-emits it on the UI thread via `QMetaObject::invokeMethod(...,
Qt::QueuedConnection)`.

The progress callback receives a snapshot of `SearchStats`:

    struct SearchStats {
        int           depth;
        uint64_t      nodes;
        uint64_t      ttHits;
        int           score;
        Move          bestMove;
        milliseconds  elapsed;
        bool          completed;
    };

The worker thread does NOT touch any Qt widget. It only emits a signal,
which Qt delivers to the UI thread's event loop.

### Done callback

Fires on the coordinator thread after all workers have joined. This is
the ONLY callback that can trigger a move. It receives the final
`Move` and `SearchStats`.

`GameController::onAISearchDone` runs on the UI thread (thanks to the
queued invocation) and:

1. Validates that the position has not changed since the search began
   (the state could have been mutated by Undo/New Game).
2. Calls `engine_.tryApply(move)`. If the move is no longer legal, it
   falls back to `moves[0]` so the AI never hangs the game.
3. Emits `moveApplied` and `changed`, which the board and panels
   react to on the UI thread.

## The generation counter

Every `AIEngine::think()` increments `generation_`:

    const uint64_t myGen = ++generation_;
    // ... start workers ...
    if (generation_.load() != myGen) return;   // superseded, don't fire done

This prevents a race where the AI's first move arrives after the user
has already clicked "New Game" and the board state is different. The
old search's done callback is suppressed because `generation_` moved
past it.

## UI responsiveness during AI thinking

While the AI is thinking, the board stays fully interactive:

| Action                          | Allowed? |
|---------------------------------|----------|
| Hover pieces                    | Yes      |
| Click to select                 | Yes      |
| Rotate 180 degrees              | Yes      |
| Scroll move history             | Yes      |
| Resize the window               | Yes      |
| Open Settings dialog            | Yes      |
| Click Undo                      | Yes (cancels the search) |
| Click New Game                  | Yes (cancels the search) |
| Move a piece                    | No (AI's turn) |

The lock on making a move is enforced in `GameController::handleSquareClick`:

    if (aiThinking_ || isAITurn()) return;

Everything else is free. The board repaints at 60 FPS, animations run
normally, buttons remain responsive.

## Invariants

These are checked during development and must never be violated:

1. **No Qt object is touched from a non-UI thread.** All cross-thread
   communication uses `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`
   or `std::atomic`.

2. **The UI thread never blocks on a worker join.** All `join()`
   calls happen either on the coordinator thread or on the UI thread
   with a bounded wait (1500 ms) that in practice returns in <5 ms.

3. **`core::GameEngine` is mutated only on the UI thread.** The AI
   reads a value copy of `Board` and never writes to `engine_`.

4. **All TT accesses go through `probe()` / `store()`.** Direct
   access to `entries_` is not allowed outside `TranspositionTable.cpp`.
   This is what makes striping safe.

5. **The stop flag is honored every ~1000 nodes.** No unbounded loop
   exists inside the search. The only place a worker can spend time
   without checking `stopFlag_` is a single `generateLegalMoves()`
   call, which completes in microseconds.

6. **The done callback fires exactly once per `think()` call**, or
   never if the search was superseded by a newer `think()`.

## Watchdog

There is no separate watchdog thread. The bounded `stop()` wait plus
the generation counter provide the same guarantees:

- If the worker hangs (a bug), `stop()` returns after `wait` ms with
  `false`, and the UI proceeds. The hung worker never fires `done`
  because `generation_` has moved on.
- If the coordinator hangs, its `jthread` destructor joins it on the
  next `think()` call, which the UI has already bounded with `stop()`.
- The AIEngine destructor calls `stop(2000ms)` before deleting the
  coordinator.

In the worst case (a genuinely stuck thread), the process keeps
running and the UI stays responsive. The engine will accumulate a
zombie thread, but the game does not freeze.

## Failure modes we have hit and fixed

### 1. TT corruption under SMP

Two threads reading and writing the same TT entry simultaneously.
Reader saw a hash from position A and a bestMove from position B,
trusted a phantom score, played a terrible move.

**Fix:** striped locks (4096 mutexes). See `AI_DESIGN.md`.

### 2. Done callback firing before workers join

The coordinator fired `done` at t=0 with a default-constructed move
because workers were spawned but not yet joined.

**Fix:** nested scope for the worker vector, so `join()` runs before
the callback.

### 3. Fast-path bypassing the search

`GameController::launchAISearch` short-circuited when only one legal
move existed, playing `moves[0]`. The player saw the AI play
arbitrary moves with no depth display and concluded the AI was broken.

**Fix:** delete the fast path. The search now runs for every AI move,
with a 300 ms minimum enforced in `Search::think()`.

All three bugs produced plausible-looking symptoms (weak moves,
instant replies) that took several diagnostics to isolate. Each is
now prevented by the invariants listed above.

## What to do if you extend the threading model

If you add a new background task (ponder mode, PST tuner, opening book
loader), follow the same contract:

1. Run it on a `std::jthread`, not the UI thread.
2. Give it an `std::atomic<bool>*` stop flag it polls regularly.
3. Deliver results to the UI via `QMetaObject::invokeMethod(...,
   Qt::QueuedConnection)`, never by direct signal emission from the
   worker.
4. If the task mutates shared state, protect that state with a mutex
   or make it `std::atomic`.
5. Bound every join. Never block the UI thread on an unbounded wait.

This pattern has worked for the AI search; it will work for anything
else you add.
