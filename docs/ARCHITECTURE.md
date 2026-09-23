# Architecture

## Design philosophy: one thing, one place

The single most important rule of this codebase is:

> Every concept lives in exactly one file. Nothing is duplicated across
> the core, the AI, and the UI.

Concrete consequences:

- The 10x10 board is defined in core::Board and core::BoardConstants
  and NOWHERE else. The UI renders it. The AI mutates it. Neither has
  its own board representation.
- Legal move generation lives only in core::MoveGenerator and
  core::CaptureRules. The AI search calls the same functions the UI
  calls. There is no parallel "AI move generator".
- Rule variants (max-capture / free-capture) are chosen via one enum,
  core::RuleSet, and implemented in core::CaptureRules. Neither the
  AI nor the UI ever reimplements a rule.
- Piece colours and rendering are defined in ui::Theme and
  ui::PieceRenderer. Every screen uses the same renderer.
- Notation (32-28, 28x19x10) lives in core::Notation. The history
  panel, the clipboard exporter, and any log all call it.
- Undo/redo state lives in core::GameEngine. The UI Undo button
  calls engine.undo().

## Module dependency graph

    +---------------+
    |   ui::*       |  Qt 6 widgets
    +-------+-------+
            |
            | depends on
            v
    +---------------+       +---------------+
    | controller::* | ----> |   ai::*       |
    +-------+-------+       +-------+-------+
            |                       |
            | both depend on        |
            v                       v
    +---------------------------------------+
    |             core::*                   |
    |   (pure rules engine, no GUI, no AI)  |
    +---------------------------------------+

The dependency arrows only point DOWN. core never includes anything
from ai, ui, or controller. ai never includes anything from ui or
controller. This is enforced by construction - any violation would
fail to link.

## Layer detail: core

- No dependencies other than the C++ standard library.
- Board is the position. It is copyable (cheap - four bitboards).
- GameState is per-position metadata: side to move, ply, repetition
  counters, draw trackers.
- GameEngine owns a Board and a GameState and is the ONLY thing that
  mutates game state.
- The ONE move generator: core::generateLegalMoves(board, side, rules)
  returns a bounded MoveSpan of every legal move. Both the UI (for
  highlighting) and the AI (for search) call it.
- Zobrist hashing is incremental: setting or removing a piece XORs one
  key. Board::pieceHash() is O(1).

Core files:

    Types.hpp              Square, Color, PieceKind, Piece, Move
    BoardConstants.hpp     geometry, neighbours, rays, setup
    RuleSet.hpp            max-capture / free-capture enum
    Zobrist.hpp/.cpp       deterministic 64-bit hash keys
    Board.hpp/.cpp         bitboard position + incremental Zobrist
    CaptureRules.hpp/.cpp  capture chain expansion
    MoveGenerator.hpp/.cpp legal move enumeration, majority rule
    GameState.hpp/.cpp     turn, ply, repetition, draw clocks
    GameEngine.hpp/.cpp    apply/undo/redo, terminal detection
    Notation.hpp/.cpp      FMJD algebraic notation
    Perft.hpp/.cpp         move-generation correctness harness

## Layer detail: ai

- Depends on core only.
- Owns no game rules. Every rule is delegated to core.
- Fully independent of Qt. Can be built and tested without a GUI.

AI files:

    PST.hpp                     piece-square tables (constexpr data)
    Evaluator.hpp/.cpp          static evaluation, negamax convention
    TimeManager.hpp/.cpp        soft/hard budget with external stop flag
    TranspositionTable.hpp/.cpp striped-lock, thread-safe hash table
    MoveOrdering.hpp/.cpp       TT / killer / history / capture ordering
    Search.hpp/.cpp             iterative deepening alpha-beta
    AIEngine.hpp/.cpp           Lazy SMP orchestration, callbacks

## Layer detail: ui

- Depends on core (for game state) and calls ai via the controller.
- Owns every visual constant (Theme), every rendered pixel
  (PieceRenderer, BoardWidget), and every widget.

UI files:

    Theme.hpp/.cpp              colors, sizes, fonts
    PieceRenderer.hpp/.cpp      chip + crown drawing
    BoardWidget.hpp/.cpp        board rendering and click handling
    Animator.hpp/.cpp           60 Hz piece-slide driver
    GameController.hpp/.cpp     MVC glue, owns GameEngine + AIEngine
    HistoryPanel.hpp/.cpp       scrollable move list
    SidePanel.hpp/.cpp          status readouts + action buttons
    SoundManager.hpp/.cpp       move/capture/king/win SFX
    Settings.hpp/.cpp           QSettings-backed preferences
    SettingsDialog.hpp/.cpp     preferences dialog
    SaveGame.hpp/.cpp           text save/load (.drgt)

## Layer detail: controller

- Just ModeConfig.hpp today. Its job is to answer "who supplies the
  next move?" without leaking that decision into the UI or the AI.
- Human vs Human: UI accepts clicks for both colours.
- Human vs AI: UI accepts clicks only for the human colour. The AI
  turn triggers an automatic search.

## Data flow: one human move

    User clicks a piece on the board
            |
            v
    BoardWidget::mousePressEvent -> GameController::handleSquareClick(sq)
                                            |
                                            v
                             core::generateLegalMoves(board, side, rules)
                                            |
                                            v
                             if sq is a legal destination -> engine_.tryApply(move)
                                            |
                            +---------------+---------------+
                            |                               |
                            v                               v
                  emit moveApplied()              emit changed()
                            |                               |
                            v                               v
                  BoardWidget animates              HistoryPanel refreshes
                            |                       SidePanel refreshes
                            v
                 GameController::maybeTriggerAI()
                            |
                            v
                 if AI turn -> AIEngine::think(board, side, rules)
                                            |
                                            v
                             search on worker threads (~5 s)
                                            |
                                            v
                  emit done (queued to UI thread via QMetaObject)
                                            |
                                            v
                        GameController::onAISearchDone(move)
                                            |
                                            v
                        engine_.tryApply(move) -> animation

Every arrow crosses a module boundary through a narrow interface. No
module reaches into another internals.

## Build layout

The build is driven by a single CMakeLists.txt at the root, plus
tests/CMakeLists.txt. Targets:

    Target            Type    Depends on
    ------------------------------------------------------------
    draughts_core     static  nothing
    draughts_ai       static  draughts_core
    draughts          exe     draughts_core, draughts_ai, Qt 6
    draughts_tests    exe     draughts_core, Catch2
    perft             exe     draughts_core
    ai_bench          exe     draughts_ai
    ai_selfplay       exe     draughts_ai
    ai_ui_sim         exe     draughts_core, draughts_ai, Qt Core

The dependency graph is a strict tree - no cycles. This lets CMake
build core and ai in parallel with Qt setup, and lets tests run
without the UI.

## Extension points

Where to add code if you want to extend the project:

    If you want to...                  Add code in...
    ------------------------------------------------------------
    Add a new rule variant             core/RuleSet.hpp + core/CaptureRules.cpp
    Change evaluation weights          ai/PST.hpp + ai/Evaluator.cpp
    Add a search heuristic             ai/Search.cpp
    Add a piece-square table           ai/PST.hpp
    Change board colours               ui/Theme.cpp
    Change piece rendering             ui/PieceRenderer.cpp
    Add a side-panel button            ui/SidePanel.cpp
    Add a preference                   ui/Settings.hpp/.cpp + SettingsDialog
    Add a keyboard shortcut            ui/SidePanel.cpp or main.cpp
    Add a new file format              ui/SaveGame.cpp
    Add an opening book                new ai/OpeningBook.hpp/.cpp
    Add endgame tablebases             new ai/EndgameTablebase.hpp/.cpp

In every case, the module boundary is preserved: the new feature plugs
into one layer and reuses everything below it.

## Non-goals

- No external board representation. There will not be a Board in the
  UI or the AI. If you want a copy of the current core::Board, it is
  exactly 40 bytes - just copy it.
- No duplicated rule logic. If a rule appears in two places, one of
  them is a bug.
- No blocking the UI thread. All search work runs on worker threads.
  Progress returns via Qt signals that land on the UI thread.
- No global mutable state. Settings::instance() is the only singleton
  and it is a pure QSettings wrapper.

## Testing strategy

Tests are organised by the module they exercise:

- core tests live in tests/test_*.cpp and cover geometry, move
  generation, promotion, flying kings, Zobrist, and perft reference
  values.
- ai correctness is verified by the diagnostic tools (ai_bench,
  ai_selfplay, ai_ui_sim).

The perft test is the strongest single guarantee: it exercises the
generator recursively to depth 6 against published FMJD counts for
both rule variants. Any regression in move generation, capture rules,
or promotion will fail perft immediately.

## Determinism

Given the same start position and the same settings, the game produces
the same result:

- Zobrist keys are seeded from a fixed constant.
- The transposition table is deterministic per call (single writer per
  bucket, thanks to striping).
- The AI search starts from threadId-varied depths, but the
  authoritative result is always thread 0s.
- Multi-threaded scheduling can produce minor timing differences but
  never different moves at the same depth.

For fully deterministic testing, use the perft CLI - it is single-
threaded and produces byte-identical output every run.

## A note on "one thing, one place"

This is the rule we broke most often during early development, and
every time it bit us. The clearest example: we initially had a "fast
path" in GameController that played moves[0] when only one legal move
existed, bypassing the search. That was duplicated rule knowledge -
the generator said "only one move", so the controller decided to skip
search. It seemed harmless until the AI started playing moves[0] on
every forced position and looked broken to the player.

The fix was to delete the fast path and let the search run. The
lesson: if the search is the ONE place that picks the AI move, then
it must always run. Special cases in higher layers are a smell.

## Version history

- v0.1  core rules engine, perft 1-6 verified, 11 tests
- v0.2  Qt UI: board, side panel, history, animations, sound
- v0.3  save/load, copy notation, settings dialog
- v0.4  AI engine: alpha-beta, TT, killers, history, PVS, LMR
- v0.5  Lazy SMP multithreading (4-8 workers)
- v0.6  bug fixes: TT thread safety, done-callback ordering
- v0.7  test coverage: 25 tests, docs, CI, consolidated CMakeLists
