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
#include <QFileDialog>
#include <QGridLayout>
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

    layout->addWidget(turnLabel_);
    layout->addWidget(moveLabel_);
    layout->addWidget(captureLabel_);
    layout->addWidget(resultLabel_);
    layout->addWidget(bookLabel_);
    layout->addWidget(aiStatusLabel_);
    layout->addWidget(ponderLabel_);
    layout->addSpacing(6);

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
    rotateBtn_   = new QPushButton("Rotate 180?",    this);
    newGameBtn_  = new QPushButton("New Game",       this);
    resignBtn_   = new QPushButton("Give Up",        this);
    settingsBtn_ = new QPushButton("Settings...",    this);
    saveBtn_     = new QPushButton("Save Game...",   this);
    loadBtn_     = new QPushButton("Load Game...",   this);
    copyBtn_     = new QPushButton("Copy Notation",  this);

    grid->addWidget(undoBtn_,     0, 0);
    grid->addWidget(redoBtn_,     0, 1);
    grid->addWidget(rotateBtn_,   1, 0, 1, 2);
    grid->addWidget(newGameBtn_,  2, 0, 1, 2);
    grid->addWidget(saveBtn_,     3, 0);
    grid->addWidget(loadBtn_,     3, 1);
    grid->addWidget(copyBtn_,     4, 0, 1, 2);
    grid->addWidget(resignBtn_,   5, 0, 1, 2);
    grid->addWidget(settingsBtn_, 6, 0, 1, 2);

    layout->addLayout(grid);
    layout->addStretch(1);

    connect(undoBtn_,    &QPushButton::clicked, this, [this]{ if (controller_) controller_->undo(); });
    connect(redoBtn_,    &QPushButton::clicked, this, [this]{ if (controller_) controller_->redo(); });
    connect(resignBtn_,  &QPushButton::clicked, this, [this]{ if (controller_) controller_->resign(); });
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
    connect(copyBtn_, &QPushButton::clicked, this, &SidePanel::onCopyNotation);

    connect(modeHumanHuman_, &QRadioButton::toggled, this, [this](bool on){
        if (on && controller_)
            controller_->setMode(controller::GameMode::HumanVsHuman);
    });
    connect(modeHumanAI_,    &QRadioButton::toggled, this, [this](bool on){
        if (on && controller_)
            controller_->setMode(controller::GameMode::HumanVsAI);
    });
}

void SidePanel::setController(GameController* c, BoardWidget* b) {
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = c;
    board_      = b;
    if (controller_) {
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
    }
    refresh();
}

void SidePanel::refresh() {
    if (!controller_) return;

    const auto side = controller_->sideToMove();
    const auto res  = controller_->result();

    // Hide turn indicator once the game is decided.
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

void SidePanel::onOpeningBookPlayed() {
    bookLabel_->setText(QStringLiteral("Opening book"));
}

void SidePanel::onPonderProgress(int depth, quint64 nodes, int score, qint64 ms) {
    ponderLabel_->setText(
        QString("Pondering...  depth %1 | %2 kn | score %3 | %4 ms")
            .arg(depth).arg(nodes / 1000).arg(score).arg(ms));
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

} // namespace draughts::ui
