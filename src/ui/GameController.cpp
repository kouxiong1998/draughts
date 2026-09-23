#include "GameController.hpp"
#include "Settings.hpp"
#include "core/MoveGenerator.hpp"

#include <QMetaObject>
#include <algorithm>

namespace draughts::ui {

GameController::GameController(QObject* parent) : QObject(parent) {
    humanColor_ = Settings::instance().playerColor();

    ai_.setProgressCallback([this](const ai::SearchStats& s) {
        QMetaObject::invokeMethod(this, [this, s]{
            emit aiProgress(s.depth,
                            static_cast<quint64>(s.nodes),
                            s.score,
                            static_cast<qint64>(s.elapsed.count()));
        }, Qt::QueuedConnection);
    });
    ai_.setDoneCallback([this](const core::Move& mv, const ai::SearchStats&) {
        QMetaObject::invokeMethod(this, [this, mv]{
            onAISearchDone(mv);
        }, Qt::QueuedConnection);
    });

    newGame();
}

GameController::~GameController() {
    ai_.stop(std::chrono::milliseconds(1500));
}

void GameController::clearSelection() {
    selected_.reset();
    selectionMoves_.clear();
}

void GameController::setMode(controller::GameMode m) {
    if (mode_ == m) return;
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    mode_ = m;
    emit aiThinkingChanged(false);
    emit changed();
    maybeTriggerAI();
}

void GameController::handleSquareClick(core::Square sq) {
    if (engine_.result() != core::GameResult::Ongoing) return;
    if (aiThinking_ || isAITurn()) return;

    const auto& b    = engine_.board();
    const auto  side = engine_.sideToMove();

    if (selected_) {
        const auto it = std::find_if(
            selectionMoves_.begin(), selectionMoves_.end(),
            [sq](const core::Move& m) { return m.to == sq; });
        if (it != selectionMoves_.end()) {
            const core::Move chosen = *it;
            const core::Board preBoard = engine_.board();
            if (engine_.tryApply(chosen)) {
                const auto& hist = engine_.history();
                const auto& rec  = hist[engine_.historyCursor() - 1];
                clearSelection();
                emit moveApplied(rec, preBoard);
                emit changed();
                maybeTriggerAI();
                return;
            }
        }
    }

    const auto p = b.at(sq);
    if (!p.empty() && p.color == side) {
        selected_ = sq;
        selectionMoves_.clear();

        const auto moves = core::generateLegalMoves(b, side, engine_.rules());
        for (std::size_t i = 0; i < moves.size(); ++i)
            if (moves[i].from == sq) selectionMoves_.push_back(moves[i]);

        emit changed();
        return;
    }

    clearSelection();
    emit changed();
}

void GameController::newGame() {
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);

    humanColor_ = Settings::instance().playerColor();
    const auto& st = Settings::instance();
    engine_.newGame(st.nextRuleSet(), st.nextFirstPlayer());
    clearSelection();
    emit changed();
    maybeTriggerAI();
}

void GameController::undo() {
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);

    if (!engine_.canUndo()) return;

    // In AI mode, undo twice so it's the human's turn again.
    if (mode_ == controller::GameMode::HumanVsAI
        && engine_.sideToMove() == aiColor()) {
        engine_.undo();
    }
    engine_.undo();
    clearSelection();
    emit changed();
    maybeTriggerAI();
}

void GameController::redo() {
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);

    if (!engine_.canRedo()) return;

    const core::Board preBoard = engine_.board();
    if (engine_.redo()) {
        const auto& hist = engine_.history();
        const auto& rec  = hist[engine_.historyCursor() - 1];
        clearSelection();
        emit moveApplied(rec, preBoard);
        emit changed();
        maybeTriggerAI();
    }
}

void GameController::resign() {
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);
    engine_.resign();
    clearSelection();
    emit changed();
}

void GameController::adoptEngine(core::GameEngine&& newEngine) {
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);
    engine_ = std::move(newEngine);
    clearSelection();
    emit changed();
    maybeTriggerAI();
}

void GameController::maybeTriggerAI() {
    if (engine_.result() != core::GameResult::Ongoing) return;
    if (!isAITurn()) return;
    launchAISearch();
}

void GameController::launchAISearch() {
    if (aiThinking_) return;

    const auto moves = core::generateLegalMoves(engine_.board(),
                                                engine_.sideToMove(),
                                                engine_.rules());

    if (moves.empty()) return;


    aiThinking_ = true;
    emit aiThinkingChanged(true);

    ai_.think(engine_.board(), engine_.sideToMove(), engine_.rules(),
              ai::TimeBudget{});
}

void GameController::playMoveFromAI(const core::Move& move) {
    const core::Board preBoard = engine_.board();
    if (!engine_.tryApply(move)) return;

    const auto& hist = engine_.history();
    const auto& rec  = hist[engine_.historyCursor() - 1];
    emit moveApplied(rec, preBoard);
    emit changed();
    // No chained trigger ? in H-vs-AI the AI only plays its own colour,
    // so after one AI move it is the human's turn.
}

void GameController::onAISearchDone(const core::Move& move) {
    aiThinking_ = false;
    emit aiThinkingChanged(false);

    if (engine_.result() != core::GameResult::Ongoing) return;
    if (!isAITurn()) return;

    const core::Board preBoard = engine_.board();
    if (engine_.tryApply(move)) {
        const auto& hist = engine_.history();
        const auto& rec  = hist[engine_.historyCursor() - 1];
        emit moveApplied(rec, preBoard);
        emit changed();
    } else {
        // Fallback: play the first legal move so we never hang.
        const auto moves = core::generateLegalMoves(engine_.board(),
                                                    engine_.sideToMove(),
                                                    engine_.rules());
        if (!moves.empty()) playMoveFromAI(moves[0]);
    }
}

} // namespace draughts::ui
