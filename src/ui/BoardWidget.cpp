#include "BoardWidget.hpp"
#include "Theme.hpp"
#include "PieceRenderer.hpp"
#include "GameController.hpp"
#include "Animator.hpp"
#include "Settings.hpp"
#include "core/BoardConstants.hpp"
#include "core/MoveGenerator.hpp"

#include <QMouseEvent>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>

namespace draughts::ui {

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(Theme::kMinimumWidgetSide, Theme::kMinimumWidgetSide);
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    setAutoFillBackground(false);
    setMouseTracking(true);
    board_.resetStandard();

    animator_ = new Animator(this);
    connect(animator_, &Animator::updated,  this, &BoardWidget::onAnimationUpdated);
    connect(animator_, &Animator::finished, this, &BoardWidget::onAnimationUpdated);
}

void BoardWidget::setController(GameController* c) {
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = c;
    if (controller_) {
        connect(controller_, &GameController::changed,     this, [this]{ update(); });
        connect(controller_, &GameController::moveApplied, this, &BoardWidget::onMoveApplied);
    }
    update();
}

int BoardWidget::computeCellSize(int w, int h) const noexcept {
    // Fill the widget exactly: no outer border margin.
    return std::max(20, std::min(w, h) / Theme::kBoardSize);
}

QPoint BoardWidget::boardOrigin(int w, int h, int cell) const noexcept {
    const int boardPx = cell * Theme::kBoardSize;
    return { (w - boardPx) / 2, (h - boardPx) / 2 };
}

QRectF BoardWidget::cellRect(int row, int col, int cell,
                             const QPoint& origin) const noexcept
{
    return { static_cast<double>(origin.x() + col * cell),
             static_cast<double>(origin.y() + row * cell),
             static_cast<double>(cell),
             static_cast<double>(cell) };
}

std::optional<core::Square> BoardWidget::squareAt(const QPoint& pixel) const noexcept {
    const int    cell   = computeCellSize(width(), height());
    const QPoint origin = boardOrigin   (width(), height(), cell);
    const int    boardPx = cell * Theme::kBoardSize;

    const int x = pixel.x() - origin.x();
    const int y = pixel.y() - origin.y();
    if (x < 0 || y < 0 || x >= boardPx || y >= boardPx) return std::nullopt;

    int col = x / cell;
    int row = y / cell;
    if (rotated_) { row = Theme::kBoardSize - 1 - row;
                    col = Theme::kBoardSize - 1 - col; }

    if (!core::bc::isPlayable(row, col)) return std::nullopt;
    return core::bc::indexFromRowCol(row, col);
}

void BoardWidget::drawSurface(QPainter& p, int cell,
                              const QPoint& origin) const
{
    const int boardPx = cell * Theme::kBoardSize;

    // Draw the grid without any outer shadow or backdrop; the widget's
    // own rectangle is now exactly the size of the board.

    for (int row = 0; row < Theme::kBoardSize; ++row) {
        for (int col = 0; col < Theme::kBoardSize; ++col) {
            const bool playableSq = ((row + col) & 1) == 1;
            p.setBrush(playableSq ? Theme::darkSquare() : Theme::lightSquare());
            p.drawRect(origin.x() + col * cell,
                       origin.y() + row * cell,
                       cell, cell);
        }
    }

    p.setPen(QPen(Theme::gridLine(), 1));
    for (int i = 0; i <= Theme::kBoardSize; ++i) {
        p.drawLine(origin.x() + i * cell, origin.y(),
                   origin.x() + i * cell, origin.y() + boardPx);
        p.drawLine(origin.x(),            origin.y() + i * cell,
                   origin.x() + boardPx,  origin.y() + i * cell);
    }
}

void BoardWidget::drawHighlights(QPainter& p, int cell,
                                 const QPoint& origin) const
{
    if (!controller_) return;
    const auto& squares = controller_->highlightSquares();

    for (core::Square s : squares) {
        int row = core::bc::rowOf(s);
        int col = core::bc::colOf(s);
        if (rotated_) { row = Theme::kBoardSize - 1 - row;
                        col = Theme::kBoardSize - 1 - col; }

        const QRectF r = cellRect(row, col, cell, origin);
        const double d = cell * 0.34;
        const QRectF dot(r.center().x() - d/2.0,
                         r.center().y() - d/2.0, d, d);

        p.setPen(Qt::NoPen);
        p.setBrush(Theme::legalMoveDot());
        p.drawEllipse(dot);
    }
}

void BoardWidget::drawPieces(QPainter& p, int cell,
                             const QPoint& origin) const
{
    const auto& b   = controller_ ? controller_->board() : board_;
    const auto  sel = controller_ ? controller_->selectedSquare()
                                  : std::optional<core::Square>{};

    for (int sq = 0; sq < core::kNumPlayableSquares; ++sq) {
        const auto s = static_cast<core::Square>(sq);
        const core::Piece piece = b.at(s);
        if (piece.empty()) continue;

        int row = core::bc::rowOf(s);
        int col = core::bc::colOf(s);
        if (rotated_) { row = Theme::kBoardSize - 1 - row;
                        col = Theme::kBoardSize - 1 - col; }

        const bool highlighted = sel && (*sel == s);
        PieceRenderer::draw(p, cellRect(row, col, cell, origin),
                            piece.color, piece.kind, highlighted);
    }
}

void BoardWidget::drawPiecesAnimated(QPainter& p, int cell,
                                     const QPoint& origin) const
{
    const double frac = animator_->fraction();

    // Captured pieces stay visible until 60% then fade out over the tail.
    const double capOpacity = frac < 0.60
        ? 1.0
        : std::max(0.0, 1.0 - (frac - 0.60) / 0.40);

    for (int sq = 0; sq < core::kNumPlayableSquares; ++sq) {
        const auto s = static_cast<core::Square>(sq);
        if (s == animMove_.from) continue;      // drawn at interpolated pos below

        const core::Piece piece = preBoard_.at(s);
        if (piece.empty()) continue;

        int row = core::bc::rowOf(s);
        int col = core::bc::colOf(s);
        if (rotated_) { row = Theme::kBoardSize - 1 - row;
                        col = Theme::kBoardSize - 1 - col; }

        const bool captured = (animMove_.captured & core::Board::bit(s)) != 0;
        if (captured) {
            if (capOpacity <= 0.001) continue;
            p.save();
            p.setOpacity(capOpacity);
            PieceRenderer::draw(p, cellRect(row, col, cell, origin),
                                piece.color, piece.kind, false);
            p.restore();
        } else {
            PieceRenderer::draw(p, cellRect(row, col, cell, origin),
                                piece.color, piece.kind, false);
        }
    }

    // The mover at its interpolated position.
    const QPointF pos = animator_->position();
    double c = pos.x();
    double r = pos.y();
    if (rotated_) { r = Theme::kBoardSize - 1 - r;
                    c = Theme::kBoardSize - 1 - c; }

    const QRectF rPix(origin.x() + c * cell,
                      origin.y() + r * cell,
                      cell, cell);

    const core::Piece mover = preBoard_.at(animMove_.from);
    if (!mover.empty()) {
        PieceRenderer::draw(p, rPix, mover.color, mover.kind, false);
    }
}

void BoardWidget::drawSquareLabels(QPainter& p, int cell,
                                   const QPoint& origin) const
{
    QFont font = p.font();
    font.setPixelSize(std::max(8, static_cast<int>(cell * 0.30)));
    font.setBold(true);
    p.setFont(font);

    for (int sq = 0; sq < core::kNumPlayableSquares; ++sq) {
        const auto s   = static_cast<core::Square>(sq);
        const int  row = core::bc::rowOf(s);
        const int  col = core::bc::colOf(s);

        // Numbering: 1 at bottom-left, 5 at bottom-right, then 6-10 on the
        // next row up, and so on, up to 50 at the top-right.
        // Playable columns in a row start at 0 for odd rows, 1 for even rows.
        const int firstPlayableCol = (row % 2 == 0) ? 1 : 0;
        const int withinRow        = (col - firstPlayableCol) / 2;
        const int label            = (core::kBoardSize - 1 - row) * 5
                                     + withinRow + 1;

        // If the view is rotated, flip the draw position (label stays
        // attached to its logical square).
        int dRow = row;
        int dCol = col;
        if (rotated_) {
            dRow = core::kBoardSize - 1 - dRow;
            dCol = core::kBoardSize - 1 - dCol;
        }

        const QRectF  r      = cellRect(dRow, dCol, cell, origin);
        const QPointF center = r.center();
        const double  radius = cell * 0.20;

        // Dark circle background so the label is readable over both the
        // black square fill and the coloured chips.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 165));
        p.drawEllipse(center, radius, radius);

        p.setPen(QColor(255, 255, 255, 220));
        p.drawText(r, Qt::AlignCenter, QString::number(label));
    }
}

void BoardWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int    cell   = computeCellSize(width(), height());
    const QPoint origin = boardOrigin   (width(), height(), cell);

    drawSurface(p, cell, origin);

    if (animator_ && animator_->running()) {
        drawPiecesAnimated(p, cell, origin);
    } else {
        drawHighlights(p, cell, origin);
        drawPieces    (p, cell, origin);
    }

    // Square labels are always drawn last, on top of everything.
    drawSquareLabels(p, cell, origin);
}

void BoardWidget::mousePressEvent(QMouseEvent* e) {
    if (!controller_) return;
    if (e->button() != Qt::LeftButton) return;
    const auto sq = squareAt(e->pos());
    if (sq) controller_->handleSquareClick(*sq);
}

void BoardWidget::onMoveApplied(const core::MoveRecord& rec,
                                const core::Board& preBoard)
{
    if (!animator_) return;

    animMove_  = rec.move;
    animMover_ = rec.mover;
    preBoard_  = preBoard;

    std::vector<QPointF> path;
    auto push = [&](core::Square s) {
        double r = core::bc::rowOf(s);
        double c = core::bc::colOf(s);
        path.push_back({ c, r });
    };

    push(animMove_.from);

    if (animMove_.isCapture()) {
        std::array<core::Square, 32> landings{};
        if (core::expandChainLandings(preBoard, animMover_, animMove_,
                                      landings.data(),
                                      static_cast<int>(landings.size()))) {
            for (std::size_t i = 1; i < landings.size(); ++i) {
                if (landings[i] == core::kInvalidSquare) break;
                push(landings[i]);
                if (landings[i] == animMove_.to) break;
            }
        }
    }
    if (path.size() < 2) push(animMove_.to);

    const int segs = static_cast<int>(path.size()) - 1;
    const int durBase = 220 + 90 * std::max(0, segs - 1);
    const double speedMul = Settings::instance().animationSpeed();
    const int dur = static_cast<int>(durBase * speedMul);

    animator_->start(std::move(path), dur);
}

void BoardWidget::onAnimationUpdated() {
    update();
}

} // namespace draughts::ui

