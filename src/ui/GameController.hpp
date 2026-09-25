#pragma once
/// @file GameController.hpp
/// @brief MVC glue. Owns the core::GameEngine, an AIEngine, an opening book,
///        and a move timer. Translates raw square clicks into engine calls,
///        schedules AI moves on a worker thread, and starts silent ponder
///        searches during the human's turn.

#include "core/GameEngine.hpp"
#include "core/Types.hpp"
#include "controller/ModeConfig.hpp"
#include "ai/AIEngine.hpp"
#include "ai/OpeningBook.hpp"

#include <QObject>
#include <QTimer>

#include <chrono>
#include <optional>
#include <vector>

namespace draughts::ui {

class GameController : public QObject {
    Q_OBJECT
public:
    explicit GameController(QObject* parent = nullptr);
    ~GameController() override;

    [[nodiscard]] const core::Board& board()      const noexcept {
        if (!replayMode_) return engine_.board();
        rebuildReplayBoard();
        return replayBoard_;
    }
    [[nodiscard]] core::Color        sideToMove() const noexcept { return engine_.sideToMove(); }
    [[nodiscard]] core::GameResult   result()     const noexcept { return engine_.result(); }
    [[nodiscard]] core::RuleSet      rules()      const noexcept { return engine_.rules(); }
    [[nodiscard]] int                ply()        const noexcept { return engine_.ply(); }

    [[nodiscard]] const core::GameEngine& engine() const noexcept { return engine_; }
    [[nodiscard]]       core::GameEngine& engine()       noexcept { return engine_; }

    [[nodiscard]] bool canUndo() const noexcept { return engine_.canUndo(); }
    [[nodiscard]] bool canRedo() const noexcept { return engine_.canRedo(); }

    [[nodiscard]] const std::vector<core::MoveRecord>& history() const noexcept {
        return engine_.history();
    }
    [[nodiscard]] std::size_t historyCursor() const noexcept {
        return engine_.historyCursor();
    }

    [[nodiscard]] std::optional<core::Square> selectedSquare() const noexcept {
        if (replayMode_) return std::nullopt;
        return selected_;
    }
    [[nodiscard]] const std::vector<core::Move>& selectionMoves() const noexcept {
        static const std::vector<core::Move> empty;
        if (replayMode_) return empty;
        return selectionMoves_;
    }

    /// Squares to highlight as the NEXT possible clicks in the current chain.
    /// During the first click this is the set of first-hop landings across all
    /// candidate chains. After each click it advances to the next hop.
    [[nodiscard]] const std::vector<core::Square>& highlightSquares() const noexcept {
        return highlightSquares_;
    }

    [[nodiscard]] controller::GameMode mode() const noexcept { return mode_; }
    [[nodiscard]] core::Color humanColor()   const noexcept { return humanColor_; }

    [[nodiscard]] core::Color aiColor() const noexcept {
        return humanColor_ == core::Color::Red ? core::Color::Yellow
                                                : core::Color::Red;
    }

    [[nodiscard]] bool isAITurn() const noexcept {
        return mode_ == controller::GameMode::HumanVsAI
            && engine_.sideToMove() == aiColor()
            && engine_.result() == core::GameResult::Ongoing;
    }
    [[nodiscard]] bool aiThinking() const noexcept { return aiThinking_; }

    // ?? Replay ?????????????????????????????????????????????????????????????
    [[nodiscard]] bool isReplaying() const noexcept { return replayMode_; }
    [[nodiscard]] int  replayPly()   const noexcept { return replayPly_; }
    [[nodiscard]] int  totalPlies()  const noexcept {
        return static_cast<int>(engine_.historyCursor());
    }

    void adoptEngine(core::GameEngine&& newEngine);

public slots:
    void handleSquareClick(core::Square sq);
    void newGame();
    void undo();
    void redo();
    void resign();
    void offerDraw();
    void setMode(controller::GameMode m);

    void enterReplay();
    void exitReplay();
    void setReplayPly(int ply);
    void replayFirst();
    void replayPrev();
    void replayNext();
    void replayLast();

signals:
    void changed();
    void moveApplied(const core::MoveRecord& rec, const core::Board& preBoard);
    void aiThinkingChanged(bool thinking);
    void aiProgress(int depth, quint64 nodes, int score, qint64 elapsedMs);
    void openingBookPlayed();
    void forcedMovePlayed();
    void ponderProgress(int depth, quint64 nodes, int score, qint64 elapsedMs);
    void timeChanged(quint64 redMs, quint64 yellowMs);
    void drawOffered(bool accepted, const QString& reason);
    void replayModeChanged(bool active);

private:
    core::GameEngine            engine_{};
    std::optional<core::Square> selected_{};
    std::vector<core::Move>     selectionMoves_{};
    std::vector<core::Square>   chainClicks_{};      // clicks so far, starting with the piece
    std::vector<core::Square>   highlightSquares_{}; // next-hop squares to draw dots on

    controller::GameMode mode_{controller::GameMode::HumanVsHuman};
    core::Color          humanColor_{core::Color::Yellow};
    bool                 aiThinking_{false};

    ai::OpeningBook book_;
    ai::AIEngine    ai_;

    // Replay
    bool           replayMode_{false};
    int            replayPly_{0};
    mutable core::Board replayBoard_{};
    void rebuildReplayBoard() const;

    // Move timer
    QTimer*        clockTimer_{nullptr};
    qint64         elapsedRedMs_{0};
    qint64         elapsedYellowMs_{0};
    core::Color    clockSide_{core::Color::Red};
    std::chrono::steady_clock::time_point clockTurnStart_{};
    bool           clockRunning_{false};

    void clearSelection();
    void recomputeHighlights();
    [[nodiscard]] std::vector<core::Square> landingsOf(const core::Move& m) const;
    void loadOpeningBook();
    void maybeTriggerAI();
    void launchAISearch();
    void startPonder();
    void onAISearchDone(const core::Move& move);
    void playMoveFromAI(const core::Move& move);

    void resetClocks();
    void startClockFor(core::Color side);
    void stopClock();
    void commitCurrentSlice();
};

} // namespace draughts::ui
