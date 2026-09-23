#include "SidePanel.hpp"
#include "GameController.hpp"
#include "BoardWidget.hpp"
#include "Settings.hpp"
#include "SettingsDialog.hpp"
#include "SaveGame.hpp"
#include "controller/ModeConfig.hpp"
#include "core/BoardConstants.hpp"
#include "core/Notation.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDate>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace draughts::ui {

namespace {

QString colorName(core::Color c) {
    return c == core::Color::Red ? "Red" : "Yellow";
}

QString formatTime(quint64 ms) {
    const quint64 totalSec = ms / 1000;
    const quint64 mm = totalSec / 60;
    const quint64 ss = totalSec % 60;
    return QString("%1:%2")
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'));
}

} // namespace

SidePanel::SidePanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto boldLabel = [this]() {
        auto* l = new QLabel(this);
        QFont f = l->font();
        f.setBold(true);
        l->setFont(f);
        return l;
    };

    turnLabel_    = boldLabel();
    moveLabel_    = new QLabel(this);
    captureLabel_ = new QLabel(this);
    timeLabel_    = new QLabel(this);
    resultLabel_  = boldLabel();

    bookLabel_ = new QLabel(this);
    bookLabel_->setStyleSheet("color: #ffffff; font-size: 10px;");
    bookLabel_->setWordWrap(false);

    aiStatusLabel_ = new QLabel(this);
    aiStatusLabel_->setStyleSheet("color: #ffffff; font-size: 10px;");
    aiStatusLabel_->setWordWrap(false);

    ponderLabel_ = new QLabel(this);
    ponderLabel_->setStyleSheet("color: #ffffff; font-size: 10px;");
    ponderLabel_->setWordWrap(false);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    auto* hidePanelBtn = new QPushButton(QStringLiteral("\u25C0"), this);
    hidePanelBtn->setFixedSize(56, 56);
    hidePanelBtn->setToolTip("Hide panel");
    hidePanelBtn->setStyleSheet(
        "QPushButton {"
        "  color: #22c55e;"
        "  background-color: rgba(34, 197, 94, 40);"
        "  border: 2px solid #22c55e;"
        "  border-radius: 6px;"
        "  font-size: 28px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(34, 197, 94, 90);"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(34, 197, 94, 150);"
        "}");
    topRow->addWidget(hidePanelBtn);
    topRow->addStretch(1);
    layout->addLayout(topRow);

    layout->addWidget(turnLabel_);
    layout->addWidget(moveLabel_);
    layout->addWidget(captureLabel_);
    layout->addWidget(timeLabel_);
    layout->addWidget(resultLabel_);
    layout->addWidget(bookLabel_);
    layout->addWidget(aiStatusLabel_);
    layout->addWidget(ponderLabel_);
    layout->addSpacing(6);

    connect(hidePanelBtn, &QPushButton::clicked, this, [this]{
        emit hidePanelRequested();
    });

    auto* modeBox = new QGroupBox("Mode", this);
    auto* modeLayout = new QVBoxLayout(modeBox);
    modeLayout->setSpacing(2);
    modeHumanHuman_ = new QRadioButton("Human vs Human", modeBox);
    modeHumanAI_    = new QRadioButton("Human vs AI",    modeBox);
    auto* mg = new QButtonGroup(this);
    mg->addButton(modeHumanHuman_);
    mg->addButton(modeHumanAI_);
    modeHumanHuman_->setChecked(true);
    modeLayout->addWidget(modeHumanHuman_);
    modeLayout->addWidget(modeHumanAI_);
    layout->addWidget(modeBox);
    layout->addSpacing(6);

    auto* grid = new QGridLayout();
    grid->setSpacing(6);

    undoBtn_     = new QPushButton("Undo",           this);
    redoBtn_     = new QPushButton("Redo",           this);
    rotateBtn_   = new QPushButton(QStringLiteral("Rotate 180\u00B0"), this);
    newGameBtn_  = new QPushButton("New Game",       this);
    drawBtn_     = new QPushButton("Draw",           this);
    resignBtn_   = new QPushButton("Give Up",        this);
    settingsBtn_ = new QPushButton("Settings...",    this);
    saveBtn_     = new QPushButton("Save Game...",   this);
    loadBtn_     = new QPushButton("Load Game...",   this);
    copyBtn_     = new QPushButton("Copy Notation",  this);
    copyPgnBtn_  = new QPushButton("Copy PGN",       this);

    grid->addWidget(undoBtn_,     0, 0);
    grid->addWidget(redoBtn_,     0, 1);
    grid->addWidget(rotateBtn_,   1, 0, 1, 2);
    grid->addWidget(newGameBtn_,  2, 0, 1, 2);
    grid->addWidget(saveBtn_,     3, 0);
    grid->addWidget(loadBtn_,     3, 1);
    grid->addWidget(copyBtn_,     4, 0);
    grid->addWidget(copyPgnBtn_,  4, 1);
    grid->addWidget(drawBtn_,     5, 0);
    grid->addWidget(resignBtn_,   5, 1);
    grid->addWidget(settingsBtn_, 6, 0, 1, 2);

    layout->addLayout(grid);
    layout->addStretch(1);

    connect(undoBtn_,    &QPushButton::clicked, this, [this]{ if (controller_) controller_->undo(); });
    connect(redoBtn_,    &QPushButton::clicked, this, [this]{ if (controller_) controller_->redo(); });
    connect(resignBtn_,  &QPushButton::clicked, this, [this]{ if (controller_) controller_->resign(); });
    connect(drawBtn_,    &QPushButton::clicked, this, [this]{ if (controller_) controller_->offerDraw(); });
    connect(rotateBtn_,  &QPushButton::clicked, this, [this]{
        if (board_) { board_->setRotated(!board_->isRotated()); refresh(); }
    });

    connect(newGameBtn_, &QPushButton::clicked, this, [this]{
        bookLabel_->clear();
        aiStatusLabel_->clear();
        ponderLabel_->clear();
        if (controller_) controller_->newGame();
        if (board_) {
            const auto pc = Settings::instance().playerColor();
            board_->setRotated(pc == core::Color::Red);
        }
    });

    connect(settingsBtn_, &QPushButton::clicked, this, [this]{
        SettingsDialog dlg(this);
        connect(&dlg, &SettingsDialog::restartRequested, this, [this]{
            bookLabel_->clear();
            aiStatusLabel_->clear();
            ponderLabel_->clear();
            if (controller_) controller_->newGame();
            if (board_) {
                const auto pc = Settings::instance().playerColor();
                board_->setRotated(pc == core::Color::Red);
            }
        });
        dlg.exec();
        refresh();
    });

    connect(saveBtn_, &QPushButton::clicked, this, &SidePanel::onSaveGame);
    connect(loadBtn_, &QPushButton::clicked, this, &SidePanel::onLoadGame);
    connect(copyBtn_,    &QPushButton::clicked, this, &SidePanel::onCopyNotation);
    connect(copyPgnBtn_, &QPushButton::clicked, this, &SidePanel::onCopyPGN);

    connect(modeHumanHuman_, &QRadioButton::toggled, this, [this](bool on){
        if (on) {
            Settings::instance().setLastGameMode(0);
            if (controller_)
                controller_->setMode(controller::GameMode::HumanVsHuman);
        }
    });
    connect(modeHumanAI_,    &QRadioButton::toggled, this, [this](bool on){
        if (on) {
            Settings::instance().setLastGameMode(1);
            if (controller_)
                controller_->setMode(controller::GameMode::HumanVsAI);
        }
    });
}

void SidePanel::setController(GameController* c, BoardWidget* b) {
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = c;
    board_      = b;
    if (controller_) {
        // Restore the last-used mode from persistent settings.
        {
            const int saved = Settings::instance().lastGameMode();
            if (saved == 1) {
                modeHumanAI_->setChecked(true);
            } else {
                modeHumanHuman_->setChecked(true);
            }
            if (controller_)
                controller_->setMode(saved == 1
                    ? controller::GameMode::HumanVsAI
                    : controller::GameMode::HumanVsHuman);
        }

        connect(controller_, &GameController::changed,
                this, &SidePanel::refresh);
        connect(controller_, &GameController::aiThinkingChanged,
                this, &SidePanel::onAIThinkingChanged);
        connect(controller_, &GameController::aiProgress,
                this, &SidePanel::onAIProgress);
        connect(controller_, &GameController::ponderProgress,
                this, &SidePanel::onPonderProgress);
        connect(controller_, &GameController::openingBookPlayed,
                this, &SidePanel::onOpeningBookPlayed);
        connect(controller_, &GameController::timeChanged,
                this, &SidePanel::onTimeChanged);
        connect(controller_, &GameController::drawOffered,
                this, &SidePanel::onDrawOffered);
    }
    refresh();
}

void SidePanel::refresh() {
    if (!controller_) return;

    const auto side = controller_->sideToMove();
    const auto res  = controller_->result();

    if (res == core::GameResult::Ongoing) {
        turnLabel_->setText(QString("Turn: <span style='color:%1'>%2</span>")
            .arg(side == core::Color::Red ? "#ff4040" : "#e0d000")
            .arg(colorName(side)));
        turnLabel_->setTextFormat(Qt::RichText);
    } else {
        turnLabel_->clear();
    }

    moveLabel_->setText(QString("Move: %1").arg(controller_->ply() / 2 + 1));

    const auto& b = controller_->board();
    const int redLost    = 20 - b.count(core::Color::Red,    core::PieceKind::Man)
                              - b.count(core::Color::Red,    core::PieceKind::King);
    const int yellowLost = 20 - b.count(core::Color::Yellow, core::PieceKind::Man)
                              - b.count(core::Color::Yellow, core::PieceKind::King);
    captureLabel_->setText(QString("Captured:  Red %1  /  Yellow %2")
                            .arg(redLost).arg(yellowLost));

    switch (res) {
        case core::GameResult::Ongoing:
            resultLabel_->setText(""); break;
        case core::GameResult::RedWins:
            resultLabel_->setText("Red wins"); break;
        case core::GameResult::YellowWins:
            resultLabel_->setText("Yellow wins"); break;
        case core::GameResult::Draw:
            resultLabel_->setText("Draw"); break;
    }

    undoBtn_->setEnabled(controller_->canUndo() && !controller_->aiThinking());
    redoBtn_->setEnabled(controller_->canRedo() && !controller_->aiThinking());

    const bool lock = controller_->aiThinking();
    modeHumanHuman_->setEnabled(!lock);
    modeHumanAI_->setEnabled(!lock);
}

void SidePanel::onAIThinkingChanged(bool thinking) {
    if (thinking) {
        aiStatusLabel_->setText(QStringLiteral("AI thinking..."));
    }
    refresh();
}

void SidePanel::onAIProgress(int depth, quint64 nodes, int score, qint64 ms) {
    aiStatusLabel_->setText(
        QString("depth %1 | %2 kn | score %3 | %4 ms")
            .arg(depth).arg(nodes / 1000).arg(score).arg(ms));
}

void SidePanel::onPonderProgress(int depth, quint64 nodes, int score, qint64 ms) {
    ponderLabel_->setText(
        QString("Pondering...  depth %1 | %2 kn | score %3 | %4 ms")
            .arg(depth).arg(nodes / 1000).arg(score).arg(ms));
}

void SidePanel::onOpeningBookPlayed() {
    bookLabel_->setText(QStringLiteral("Opening book"));
}

void SidePanel::onTimeChanged(quint64 redMs, quint64 yellowMs) {
    timeLabel_->setText(QString("Time:  Red %1  /  Yellow %2")
                        .arg(formatTime(redMs))
                        .arg(formatTime(yellowMs)));
}

void SidePanel::onDrawOffered(bool accepted, const QString& reason) {
    if (accepted) {
        aiStatusLabel_->setText(reason);
    } else {
        QMessageBox::information(this, "Draw declined", reason);
    }
}

void SidePanel::onSaveGame() {
    if (!controller_) return;
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Game", "game.drgt", "Draughts games (*.drgt);;All files (*)");
    if (path.isEmpty()) return;

    QString err;
    if (!SaveGame::save(controller_->engine(),
                        Settings::instance().playerColor(),
                        path, &err)) {
        QMessageBox::warning(this, "Save failed", err);
    }
}

void SidePanel::onLoadGame() {
    if (!controller_) return;
    const QString path = QFileDialog::getOpenFileName(
        this, "Load Game", {}, "Draughts games (*.drgt);;All files (*)");
    if (path.isEmpty()) return;

    core::GameEngine loaded;
    core::Color      savedPlayerColor = Settings::instance().playerColor();
    QString          err;
    if (!SaveGame::load(loaded, savedPlayerColor, path, &err)) {
        QMessageBox::warning(this, "Load failed", err);
        return;
    }

    bookLabel_->clear();
    aiStatusLabel_->clear();
    ponderLabel_->clear();
    controller_->adoptEngine(std::move(loaded));
    Settings::instance().setPlayerColor(savedPlayerColor);
    if (board_) {
        const auto pc = Settings::instance().playerColor();
        board_->setRotated(pc == core::Color::Red);
    }
    refresh();
}

void SidePanel::onCopyNotation() {
    if (!controller_) return;

    QString text;
    core::Board board; board.resetStandard();
    core::Color side = core::Color::Red;

    const auto& hist   = controller_->history();
    const auto  cursor = controller_->historyCursor();

    for (std::size_t i = 0; i < cursor; ++i) {
        const auto& rec = hist[i];
        const auto num  = core::formatMoveNumber(static_cast<int>(i), rec.mover);
        const auto mv   = core::formatMove(board, side, rec.move);
        text += QString("%1 %2\n").arg(QString::fromStdString(num), -6)
                                  .arg(QString::fromStdString(mv));

        core::Board next = board;
        core::Bitboard cap = rec.move.captured;
        while (cap) {
            const auto s = static_cast<core::Square>(std::countr_zero(cap));
            cap &= cap - 1;
            next.removePiece(s);
        }
        const auto p = next.at(rec.move.from);
        next.removePiece(rec.move.from);
        next.setPiece(rec.move.to, p.color, p.kind);
        if (p.kind == core::PieceKind::Man && rec.move.isPromotion)
            next.promote(rec.move.to);
        board = next;
        side  = core::opposite(side);
    }

    QApplication::clipboard()->setText(text);
}

void SidePanel::onCopyPGN() {
    if (!controller_) return;

    const auto& hist   = controller_->history();
    const auto  cursor = controller_->historyCursor();

    // ?? Result tag ?????????????????????????????????????????????????????????
    QString resultTag = QStringLiteral("*");
    switch (controller_->result()) {
        case core::GameResult::RedWins:    resultTag = QStringLiteral("1-0");     break;
        case core::GameResult::YellowWins: resultTag = QStringLiteral("0-1");     break;
        case core::GameResult::Draw:       resultTag = QStringLiteral("1/2-1/2"); break;
        case core::GameResult::Ongoing:    resultTag = QStringLiteral("*");       break;
    }

    // ?? Player names ???????????????????????????????????????????????????????
    const QString redName = (controller_->mode() == controller::GameMode::HumanVsAI)
        ? (controller_->humanColor() == core::Color::Red
             ? QStringLiteral("Human") : QStringLiteral("AI"))
        : QStringLiteral("Human 1");

    const QString yellowName = (controller_->mode() == controller::GameMode::HumanVsAI)
        ? (controller_->humanColor() == core::Color::Yellow
             ? QStringLiteral("Human") : QStringLiteral("AI"))
        : QStringLiteral("Human 2");

    // ?? Today's date in PGN format: YYYY.MM.DD ?????????????????????????????
    const QDate today = QDate::currentDate();
    const QString dateStr = QString("%1.%2.%3")
        .arg(today.year(),   4, 10, QChar('0'))
        .arg(today.month(),  2, 10, QChar('0'))
        .arg(today.day(),    2, 10, QChar('0'));

    // ?? Header ?????????????????????????????????????????????????????????????
    QString pgn;
    pgn += QStringLiteral("[Event \"Draughts 10x10 game\"]\n");
    pgn += QStringLiteral("[Site  \"Local\"]\n");
    pgn += QString("[Date  \"%1\"]\n").arg(dateStr);
    pgn += QStringLiteral("[Round \"1\"]\n");
    pgn += QString("[White \"%1\"]\n").arg(redName);
    pgn += QString("[Black \"%1\"]\n").arg(yellowName);
    pgn += QString("[Result \"%1\"]\n").arg(resultTag);
    pgn += QStringLiteral("[Variant \"International\"]\n");
    pgn += QStringLiteral("\n");

    // ?? Moves, wrapped at ~80 columns ??????????????????????????????????????
    core::Board board; board.resetStandard();
    core::Color side = core::Color::Red;

    QString moveLine;
    int lineLen = 0;

    auto flushIfLong = [&]() {
        if (lineLen >= 78) {
            pgn += moveLine + "\n";
            moveLine.clear();
            lineLen = 0;
        }
    };

    for (std::size_t i = 0; i < cursor; ++i) {
        const auto& rec = hist[i];
        const bool  isRed = (rec.mover == core::Color::Red);

        // Move number prefix on Red's turn (or first move of the game).
        QString token;
        if (isRed) {
            const int moveNo = static_cast<int>(i) / 2 + 1;
            token += QString("%1. ").arg(moveNo);
        }

        token += QString::fromStdString(core::formatMove(board, side, rec.move));
        token += ' ';

        if (lineLen > 0 && lineLen + token.length() > 79) {
            pgn += moveLine + "\n";
            moveLine.clear();
            lineLen = 0;
        }
        moveLine += token;
        lineLen  += token.length();
        (void)flushIfLong;

        // Advance the reconstruction board.
        core::Board next = board;
        core::Bitboard cap = rec.move.captured;
        while (cap) {
            const auto s = static_cast<core::Square>(std::countr_zero(cap));
            cap &= cap - 1;
            next.removePiece(s);
        }
        const auto p = next.at(rec.move.from);
        next.removePiece(rec.move.from);
        next.setPiece(rec.move.to, p.color, p.kind);
        if (p.kind == core::PieceKind::Man && rec.move.isPromotion)
            next.promote(rec.move.to);
        board = next;
        side  = core::opposite(side);
    }

    if (!moveLine.isEmpty()) {
        pgn += moveLine;
    }

    pgn += QString(" %1\n").arg(resultTag);

    QApplication::clipboard()->setText(pgn);
}

} // namespace draughts::ui
