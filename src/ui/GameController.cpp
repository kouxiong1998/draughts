#include "GameController.hpp"
#include "Settings.hpp"
#include "core/MoveGenerator.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <random>

namespace draughts::ui {

namespace {

/// Locate data/opening_book.txt by searching upward from the exe dir.
QString findBookFile() {
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString candidate = d.absoluteFilePath("data/opening_book.txt");
        if (QFileInfo::exists(candidate)) return candidate;
        if (!d.cdUp()) break;
    }
    return {};
}

/// Ponder budget. The ponder search runs during the human's turn and is
/// cancelled the moment the human moves. The budget is effectively
/// unbounded: 10 minutes is far longer than any human will think, and
/// the search stops early on its own when it reaches max depth.
constexpr auto kPonderSoft = std::chrono::milliseconds(600000);
constexpr auto kPonderHard = std::chrono::milliseconds(660000);

} // namespace

GameController::GameController(QObject* parent) : QObject(parent) {
    humanColor_ = Settings::instance().playerColor();

    loadOpeningBook();
    ai_.setOpeningBook(book_.empty() ? nullptr : &book_);

    ai_.setProgressCallback([this](const ai::SearchStats& s) {
        QMetaObject::invokeMethod(this, [this, s]{
            if (isAITurn()) {
                emit aiProgress(s.depth,
                                static_cast<quint64>(s.nodes),
                                s.score,
                                static_cast<qint64>(s.elapsed.count()));
            } else {
                // Silent ponder search ? show it distinctly so the user
                // can see the AI thinking on their turn.
                emit ponderProgress(s.depth,
                                    static_cast<quint64>(s.nodes),
                                    s.score,
                                    static_cast<qint64>(s.elapsed.count()));
            }
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

void GameController::loadOpeningBook() {
    const QString path = findBookFile();
    if (path.isEmpty()) return;
    (void)book_.loadFromFile(path.toStdString());
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

            // Cancel any in-flight ponder search before mutating state.
            ai_.stop(std::chrono::milliseconds(1500));

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

    if (!book_.empty()) {
        std::mt19937_64 rng(std::random_device{}());
        const auto bookMove = book_.pickMove(engine_.board(),
                                             engine_.sideToMove(),
                                             engine_.rules(),
                                             rng);
        if (bookMove) {
            emit openingBookPlayed();
            playMoveFromAI(*bookMove);
            return;
        }
    }

    aiThinking_ = true;
    emit aiThinkingChanged(true);

    ai_.think(engine_.board(), engine_.sideToMove(), engine_.rules(),
              ai::TimeBudget{});
}

void GameController::startPonder() {
    // Ponder only in AI mode, only when it's the human's turn, only when
    // the game is ongoing. Silently warms the TT while the human thinks.
    if (mode_ != controller::GameMode::HumanVsAI) return;
    if (engine_.result() != core::GameResult::Ongoing) return;
    if (isAITurn()) return;

    ai::TimeBudget pb;
    pb.soft    = kPonderSoft;
    pb.hard    = kPonderHard;
    pb.minimum = std::chrono::milliseconds(100);

    ai_.think(engine_.board(), engine_.sideToMove(), engine_.rules(), pb,
              /*silent=*/true);
}

void GameController::playMoveFromAI(const core::Move& move) {
    const core::Board preBoard = engine_.board();
    if (!engine_.tryApply(move)) return;

    const auto& hist = engine_.history();
    const auto& rec  = hist[engine_.historyCursor() - 1];
    emit moveApplied(rec, preBoard);
    emit changed();

    // After the AI moves, it's the human's turn ? start pondering.
    startPonder();
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
        startPonder();
    } else {
        const auto moves = core::generateLegalMoves(engine_.board(),
                                                    engine_.sideToMove(),
                                                    engine_.rules());
        if (!moves.empty()) playMoveFromAI(moves[0]);
    }
}

} // namespace draughts::ui
