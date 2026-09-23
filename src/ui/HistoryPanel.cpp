#include "HistoryPanel.hpp"
#include "GameController.hpp"
#include "core/Notation.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace draughts::ui {

HistoryPanel::HistoryPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // ?? Top row: back-arrow (green) on the left, "History" title next ?????
    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);

    auto* backBtn = new QPushButton(QStringLiteral("\u25C0"), this);
    backBtn->setFixedSize(56, 56);
    backBtn->setToolTip("Back to the side panel");
    backBtn->setStyleSheet(
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

    auto* title = new QLabel("History", this);
    {
        QFont f = title->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 2);
        title->setFont(f);
    }

    topRow->addWidget(backBtn);
    topRow->addSpacing(8);
    topRow->addWidget(title);
    topRow->addStretch(1);
    layout->addLayout(topRow);

    list_ = new QListWidget(this);
    list_->setAlternatingRowColors(true);
    list_->setSelectionMode(QAbstractItemView::NoSelection);
    list_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(list_, 1);

    connect(backBtn, &QPushButton::clicked, this, [this]{
        emit backRequested();
    });
}

void HistoryPanel::setController(GameController* c) {
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = c;
    if (controller_)
        connect(controller_, &GameController::changed,
                this, &HistoryPanel::refresh);
    refresh();
}

void HistoryPanel::refresh() {
    list_->clear();
    if (!controller_) return;

    const auto& history = controller_->history();
    const auto  cursor  = controller_->historyCursor();

    core::Board board; board.resetStandard();
    core::Color side = core::Color::Red;

    for (std::size_t i = 0; i < history.size(); ++i) {
        const auto& rec = history[i];

        const QString prefix = QString::fromStdString(
            core::formatMoveNumber(static_cast<int>(i), rec.mover));
        const QString text   = QString::fromStdString(
            core::formatMove(board, side, rec.move));

        auto* item = new QListWidgetItem(
            QString("%1  %2  %3").arg(i + 1, 3).arg(prefix, -6).arg(text));
        if (i >= cursor) item->setForeground(QColor(140, 140, 140));
        list_->addItem(item);

        // Advance the reconstruction board.
        auto next = board;
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

    if (list_->count() > 0)
        list_->scrollToBottom();
}

} // namespace draughts::ui
