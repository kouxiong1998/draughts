#include "HistoryPanel.hpp"
#include "GameController.hpp"
#include "core/Notation.hpp"

#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>

namespace draughts::ui {

HistoryPanel::HistoryPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("History", this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    list_ = new QListWidget(this);
    list_->setAlternatingRowColors(true);
    list_->setSelectionMode(QAbstractItemView::NoSelection);
    list_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(list_, 1);
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

    // Reconstruct board state incrementally so Notation can print full
    // capture chains (it needs the board *before* each move).
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
