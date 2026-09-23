#pragma once
/// @file GameController.hpp
/// @brief MVC glue. Owns the core::GameEngine and the AIEngine, translates
///        raw square clicks into engine calls, and schedules AI moves on a
///        worker thread. UI never mutates the engine directly.

#include "core/GameEngine.hpp"
#include "core/Types.hpp"
#include "controller/ModeConfig.hpp"
#include "ai/AIEngine.hpp"

#include <QObject>
#include <optional>
#include <vector>

namespace draughts::ui {

class GameController : public QObject {
    Q_OBJECT
public:
    explicit GameController(QObject* parent = nullptr);
    ~GameController() override;

    // ?? State ???????????????????????????????????????????????????????????????
    [[nodiscard]] const core::Board& board()      const noexcept { return engine_.board(); }
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
        return selected_;
    }
    [[nodiscard]] const std::vector<core::Move>& selectionMoves() const noexcept {
        return selectionMoves_;
    }

    // ?? Mode / colour ???????????????????????????????????????????????????????
    [[nodiscard]] controller::GameMode mode() const noexcept { return mode_; }
    [[nodiscard]] core::Color humanColor()   const noexcept { return humanColor_; }

    /// The colour the AI plays: the opposite of the human's colour in AI mode.
    [[nodiscard]] core::Color aiColor() const noexcept {
        return humanColor_ == core::Color::Red ? core::Color::Yellow
                                                : core::Color::Red;
    }

    [[nodiscard]] bool isAITurn()     const noexcept {
        return mode_ == controller::GameMode::HumanVsAI
            && engine_.sideToMove() == aiColor()
            && engine_.result() == core::GameResult::Ongoing;
    }
    [[nodiscard]] bool aiThinking() const noexcept { return aiThinking_; }

    /// Replace the internal engine (used by Load Game).
    void adoptEngine(core::GameEngine&& newEngine);

public slots:
    void handleSquareClick(core::Square sq);
    void newGame();
    void undo();
    void redo();
    void resign();

    void setMode(controller::GameMode m);

signals:
    void changed();
    void moveApplied(const core::MoveRecord& rec, const core::Board& preBoard);
    void aiThinkingChanged(bool thinking);
    void aiProgress(int depth, quint64 nodes, int score, qint64 elapsedMs);

private:
    core::GameEngine            engine_{};
    std::optional<core::Square> selected_{};
    std::vector<core::Move>     selectionMoves_{};

    controller::GameMode mode_{controller::GameMode::HumanVsHuman};
    core::Color          humanColor_{core::Color::Yellow};
    bool                 aiThinking_{false};

    ai::AIEngine ai_;

    void clearSelection();
    void maybeTriggerAI();
    void launchAISearch();
    void onAISearchDone(const core::Move& move);
    void playMoveFromAI(const core::Move& move);
};

} // namespace draughts::ui
