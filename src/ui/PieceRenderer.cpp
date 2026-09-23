#include "PieceRenderer.hpp"
#include "Theme.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <algorithm>

namespace draughts::ui {

namespace {

void drawShadow(QPainter& p, const QRectF& disc) {
    QRadialGradient g(disc.center() + QPointF(3, 4), disc.width() * 0.75);
    g.setColorAt(0.0, QColor(0, 0, 0, 130));
    g.setColorAt(0.6, QColor(0, 0, 0,  60));
    g.setColorAt(1.0, QColor(0, 0, 0,   0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(disc.translated(2, 3));
}

void drawBody(QPainter& p, const QRectF& disc, core::Color color) {
    const QColor base = (color == core::Color::Red)
                            ? Theme::redPiece()
                            : Theme::yellowPiece();
    const QColor hi   = (color == core::Color::Red)
                            ? Theme::redPieceHighlight()
                            : Theme::yellowPieceHighlight();
    const QColor edge = (color == core::Color::Red)
                            ? Theme::redPieceEdge()
                            : Theme::yellowPieceEdge();

    QRadialGradient g(disc.center() - QPointF(disc.width() * 0.15,
                                              disc.height() * 0.20),
                      disc.width() * 0.95);
    g.setColorAt(0.00, hi);
    g.setColorAt(0.45, base);
    g.setColorAt(1.00, base.darker(150));

    p.setBrush(g);
    p.setPen(QPen(edge, std::max(1.0, disc.width() * 0.03)));
    p.drawEllipse(disc);
}

void drawHighlight(QPainter& p, const QRectF& disc) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::selectedGlow(),
                  std::max(2.0, disc.width() * 0.09)));
    p.drawEllipse(disc.adjusted(-2, -2, 2, 2));
}

void drawCrown(QPainter& p, const QRectF& disc) {
    const double W = disc.width()  * 0.62;
    const double H = disc.height() * 0.46;
    const double X = disc.center().x() - W / 2.0;
    const double Y = disc.center().y() - H / 2.0;

    QPainterPath crown;
    crown.moveTo(X,             Y + H);
    crown.lineTo(X,             Y + H * 0.55);
    crown.lineTo(X + W * 0.20,  Y + H * 0.05);
    crown.lineTo(X + W * 0.35,  Y + H * 0.55);
    crown.lineTo(X + W * 0.50,  Y + H * 0.00);
    crown.lineTo(X + W * 0.65,  Y + H * 0.55);
    crown.lineTo(X + W * 0.80,  Y + H * 0.05);
    crown.lineTo(X + W,         Y + H * 0.55);
    crown.lineTo(X + W,         Y + H);
    crown.closeSubpath();

    p.setPen(QPen(Theme::crownEdge(), 1.4));
    p.setBrush(Theme::crown());
    p.drawPath(crown);
}

} // namespace

void PieceRenderer::draw(QPainter&       p,
                         const QRectF&   cellRect,
                         core::Color     color,
                         core::PieceKind kind,
                         bool            highlighted)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const double inset = cellRect.width() * 0.10;
    const QRectF disc  = cellRect.adjusted(inset, inset, -inset, -inset);

    drawShadow(p, disc);
    drawBody(p, disc, color);
    if (kind == core::PieceKind::King) drawCrown(p, disc);
    if (highlighted)                   drawHighlight(p, disc);

    p.restore();
}

} // namespace draughts::ui
