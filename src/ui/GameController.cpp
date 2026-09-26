#include "GameController.hpp"
#include "Settings.hpp"
#include "core/MoveGenerator.hpp"
#include "ai/Evaluator.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <array>
#include <random>

namespace draughts::ui {

namespace {

QString findBookFile() {
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString candidate = d.absoluteFilePath("data/opening_book.txt");
        if (QFileInfo::exists(candidate)) return candidate;
        if (!d.cdUp()) break;
    }
    return {};
}

constexpr auto kPonderSoft = std::chrono::milliseconds(600000);
constexpr auto kPonderHard = std::chrono::milliseconds(660000);

constexpr int kClockTickMs = 200;

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

    clockTimer_ = new QTimer(this);
    clockTimer_->setInterval(kClockTickMs);
    connect(clockTimer_, &QTimer::timeout, this, [this]{
        commitCurrentSlice();
        emit timeChanged(static_cast<quint64>(elapsedRedMs_),
                         static_cast<quint64>(elapsedYellowMs_));
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
    chainClicks_.clear();
    highlightSquares_.clear();
}

// ?? Clock helpers ??????????????????????????????????????????????????????????

void GameController::resetClocks() {
    elapsedRedMs_    = 0;
    elapsedYellowMs_ = 0;
    clockSide_       = engine_.sideToMove();
    clockRunning_    = false;
    clockTimer_->stop();
    emit timeChanged(0, 0);
}

void GameController::startClockFor(core::Color side) {
    clockSide_      = side;
    clockTurnStart_ = std::chrono::steady_clock::now();
    clockRunning_   = true;
    if (!clockTimer_->isActive()) clockTimer_->start();
}

void GameController::stopClock() {
    if (clockRunning_) commitCurrentSlice();
    clockRunning_ = false;
    clockTimer_->stop();
}

void GameController::commitCurrentSlice() {
    if (!clockRunning_) return;

    const auto now = std::chrono::steady_clock::now();
    const qint64 sliceMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - clockTurnStart_).count();

    if (clockSide_ == core::Color::Red) elapsedRedMs_ += sliceMs;
    else                                elapsedYellowMs_ += sliceMs;

    clockTurnStart_ = now;
}

// ?? State transitions ??????????????????????????????????????????????????????

void GameController::setMode(controller::GameMode m) {
    if (mode_ == m) return;
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    mode_ = m;
    emit aiThinkingChanged(false);
    emit changed();
    maybeTriggerAI();
}

// Reconstruct a move's landing sequence: [from, land1, land2, ..., to].
std::vector<core::Square> GameController::landingsOf(const core::Move& m) const {
    std::vector<core::Square> out;
    if (!m.isCapture()) {
        out.push_back(m.from);
        out.push_back(m.to);
        return out;
    }
    std::array<core::Square, 32> buf{};
    if (!core::expandChainLandings(engine_.board(), engine_.sideToMove(),
                                   m, buf.data(),
                                   static_cast<int>(buf.size()))) {
        return out;
    }
    // A capture chain has exactly 1 + captureCount() elements: the start
    // square plus one entry per captured piece. Stopping at move.to is
    // wrong when the first hop coincides with the final landing
    // (e.g. 24 -> 33 -> ... -> 33).
    const std::size_t expected = 1
        + static_cast<std::size_t>(m.captureCount());
    for (auto s : buf) {
        if (s == core::kInvalidSquare) break;
        if (out.size() >= expected) break;
        out.push_back(s);
    }
    return out;
}

// Compute which squares to highlight as the next hop of the current chain.
void GameController::recomputeHighlights() {
    highlightSquares_.clear();
    if (chainClicks_.empty()) return;

    const std::size_t hopIndex = chainClicks_.size();
    for (const auto& m : selectionMoves_) {
        const auto seq = landingsOf(m);
        if (seq.size() > hopIndex) {
            const core::Square s = seq[hopIndex];
            if (std::find(highlightSquares_.begin(),
                          highlightSquares_.end(), s)
                == highlightSquares_.end()) {
                highlightSquares_.push_back(s);
            }
        }
    }
}
void GameController::handleSquareClick(core::Square sq) {
    if (replayMode_) return;
    if (engine_.result() != core::GameResult::Ongoing) return;
    if (aiThinking_ || isAITurn()) return;

    const auto& b    = engine_.board();
    const auto  side = engine_.sideToMove();

    // 1) Chain in progress: sq is a possible next hop.
    if (selected_ && !chainClicks_.empty()) {
        const bool isNextHop = std::find(highlightSquares_.begin(),
                                         highlightSquares_.end(), sq)
                               != highlightSquares_.end();
        if (isNextHop) {
            chainClicks_.push_back(sq);

            std::vector<core::Move> filtered;
            for (const auto& m : selectionMoves_) {
                const auto seq = landingsOf(m);
                if (seq.size() < chainClicks_.size()) continue;
                bool match = true;
                for (std::size_t i = 0; i < chainClicks_.size(); ++i) {
                    if (seq[i] != chainClicks_[i]) { match = false; break; }
                }
                if (match) filtered.push_back(m);
            }
            selectionMoves_ = std::move(filtered);

            bool continues = false;
            for (const auto& m : selectionMoves_) {
                if (landingsOf(m).size() > chainClicks_.size()) {
                    continues = true;
                    break;
                }
            }

            if (!continues) {
                if (!selectionMoves_.empty()) {
                    const core::Move chosen = selectionMoves_.front();
                    const core::Board preBoard = engine_.board();
                    ai_.stop(std::chrono::milliseconds(1500));
                    if (engine_.tryApply(chosen)) {
                        const auto& hist = engine_.history();
                        const auto& rec  = hist[engine_.historyCursor() - 1];
                        commitCurrentSlice();
                        if (engine_.result() == core::GameResult::Ongoing) {
                            startClockFor(engine_.sideToMove());
                        } else {
                            stopClock();
                        }
                        clearSelection();
                        emit moveApplied(rec, preBoard);
                        emit changed();
                        maybeTriggerAI();
                        return;
                    }
                }
                clearSelection();
                emit changed();
                return;
            }

            recomputeHighlights();
            emit changed();
            return;
        }
    }

    // 2) Selecting a new piece.
    const auto p = b.at(sq);
    if (!p.empty() && p.color == side) {
        selected_ = sq;
        selectionMoves_.clear();

        const auto moves = core::generateLegalMoves(b, side, engine_.rules());
        for (std::size_t i = 0; i < moves.size(); ++i)
            if (moves[i].from == sq) selectionMoves_.push_back(moves[i]);

        if (selectionMoves_.empty()) {
            clearSelection();
            emit changed();
            return;
        }

        const bool allQuiet = std::all_of(selectionMoves_.begin(),
                                          selectionMoves_.end(),
                                          [](const core::Move& m){ return !m.isCapture(); });
        if (allQuiet && selectionMoves_.size() == 1) {
            const core::Move chosen = selectionMoves_.front();
            const core::Board preBoard = engine_.board();
            ai_.stop(std::chrono::milliseconds(1500));
            if (engine_.tryApply(chosen)) {
                const auto& hist = engine_.history();
                const auto& rec  = hist[engine_.historyCursor() - 1];
                commitCurrentSlice();
                if (engine_.result() == core::GameResult::Ongoing) {
                    startClockFor(engine_.sideToMove());
                } else {
                    stopClock();
                }
                clearSelection();
                emit moveApplied(rec, preBoard);
                emit changed();
                maybeTriggerAI();
                return;
            }
        }

        // Multiple quiet moves OR any capture chain: step-by-step.
        chainClicks_.clear();
        chainClicks_.push_back(sq);
        recomputeHighlights();

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
    resetClocks();
    startClockFor(engine_.sideToMove());
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
    commitCurrentSlice();
    if (engine_.result() == core::GameResult::Ongoing) {
        startClockFor(engine_.sideToMove());
    } else {
        stopClock();
    }
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
        commitCurrentSlice();
        if (engine_.result() == core::GameResult::Ongoing) {
            startClockFor(engine_.sideToMove());
        } else {
            stopClock();
        }
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
    stopClock();
    emit changed();
}

void GameController::offerDraw() {
    if (engine_.result() != core::GameResult::Ongoing) return;

    // Human vs Human: the offer is accepted immediately, since the same
    // person is operating both sides at the keyboard.
    if (mode_ != controller::GameMode::HumanVsAI) {
        ai_.stop(std::chrono::milliseconds(1500));
        aiThinking_ = false;
        emit aiThinkingChanged(false);
        engine_.agreeDraw();
        clearSelection();
        stopClock();
        emit changed();
        emit drawOffered(true, QStringLiteral("Draw agreed."));
        return;
    }

    // Human vs AI: the AI evaluates the position with the static evaluator.
    // Accept unless the AI is clearly ahead.
    const int evalForMover = ai::evaluate(engine_.board(), engine_.sideToMove());
    const int evalForAI = (engine_.sideToMove() == aiColor())
                            ? evalForMover
                            : -evalForMover;

    constexpr int kDeclineThresholdCp = 50;   // half a man

    if (evalForAI > kDeclineThresholdCp) {
        emit drawOffered(false,
            QStringLiteral("AI declines the draw - it thinks it is ahead."));
        // Ponder continues; nothing else to do.
        return;
    }

    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);
    engine_.agreeDraw();
    clearSelection();
    stopClock();
    emit changed();
    emit drawOffered(true, QStringLiteral("AI accepts the draw."));
}

void GameController::adoptEngine(core::GameEngine&& newEngine) {
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);
    engine_ = std::move(newEngine);
    clearSelection();
    resetClocks();
    if (engine_.result() == core::GameResult::Ongoing) {
        startClockFor(engine_.sideToMove());
    }
    emit changed();
    maybeTriggerAI();
}

void GameController::maybeTriggerAI() {
    if (replayMode_) return;
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

    // ?? Forced-move fast path ?????????????????????????????????????????????
    // If the position has exactly one legal move AND it is a capture chain,
    // the move is forced - there is nothing to search. Play it instantly:
    // no threads spawned, no callbacks fired, no delay.
    //
    // Draughts makes captures mandatory when available, so "one legal move
    // that is a capture" means exactly one capture chain is possible.
    if (moves.size() == 1 && moves[0].isCapture()) {
        emit forcedMovePlayed();
        playMoveFromAI(moves[0]);
        return;
    }

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

    // Build the time budget from the user's think-time setting.
    //   soft    = 80%  of total  (start next ID iteration only if elapsed < soft)
    //   hard    = 110% of total  (absolute ceiling, abort immediately)
    //   minimum = min(300 ms, total/10)
    const int totalMs = Settings::instance().thinkTimeMs();
    ai::TimeBudget tb;
    tb.soft    = std::chrono::milliseconds(totalMs * 8 / 10);
    tb.hard    = std::chrono::milliseconds(totalMs * 11 / 10);
    tb.minimum = std::chrono::milliseconds(
        std::min(300, totalMs / 10));

    ai_.think(engine_.board(), engine_.sideToMove(), engine_.rules(), tb);
}

void GameController::startPonder() {
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

    commitCurrentSlice();
    if (engine_.result() == core::GameResult::Ongoing) {
        startClockFor(engine_.sideToMove());
    } else {
        stopClock();
    }

    emit moveApplied(rec, preBoard);
    emit changed();
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

        commitCurrentSlice();
        if (engine_.result() == core::GameResult::Ongoing) {
            startClockFor(engine_.sideToMove());
        } else {
            stopClock();
        }

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

// ?? Replay ?????????????????????????????????????????????????????????????????

void GameController::rebuildReplayBoard() const {
    core::Board b; b.resetStandard();
    const int n = std::min(replayPly_,
                           static_cast<int>(engine_.historyCursor()));
    const auto& hist = engine_.history();

    for (int i = 0; i < n; ++i) {
        const auto& rec = hist[static_cast<std::size_t>(i)];
        core::Bitboard cap = rec.move.captured;
        while (cap) {
            const auto s = static_cast<core::Square>(std::countr_zero(cap));
            cap &= cap - 1;
            b.removePiece(s);
        }
        const auto p = b.at(rec.move.from);
        b.removePiece(rec.move.from);
        b.setPiece(rec.move.to, p.color, p.kind);
        if (p.kind == core::PieceKind::Man && rec.move.isPromotion)
            b.promote(rec.move.to);
    }
    replayBoard_ = b;
}

void GameController::enterReplay() {
    if (replayMode_) return;
    ai_.stop(std::chrono::milliseconds(1500));
    aiThinking_ = false;
    emit aiThinkingChanged(false);
    replayMode_ = true;
    replayPly_  = totalPlies();
    clearSelection();
    stopClock();
    emit replayModeChanged(true);
    emit changed();
}

void GameController::exitReplay() {
    if (!replayMode_) return;
    replayMode_ = false;
    replayPly_  = 0;
    clearSelection();
    if (engine_.result() == core::GameResult::Ongoing) {
        startClockFor(engine_.sideToMove());
    }
    emit replayModeChanged(false);
    emit changed();
    maybeTriggerAI();
}

void GameController::setReplayPly(int ply) {
    if (!replayMode_) return;
    const int maxPly = totalPlies();
    replayPly_ = std::clamp(ply, 0, maxPly);
    emit changed();
}

void GameController::replayFirst() {
    if (!replayMode_) return;
    replayPly_ = 0;
    emit changed();
}

void GameController::replayPrev() {
    if (!replayMode_) return;
    if (replayPly_ > 0) --replayPly_;
    emit changed();
}

void GameController::replayNext() {
    if (!replayMode_) return;
    if (replayPly_ < totalPlies()) ++replayPly_;
    emit changed();
}

void GameController::replayLast() {
    if (!replayMode_) return;
    replayPly_ = totalPlies();
    emit changed();
}

} // namespace draughts::ui
