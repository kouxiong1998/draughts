#include "PieceRenderer.hpp"
#include "Theme.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace draughts::ui {

namespace {

// ?? 3D chip body (unchanged) ???????????????????????????????????????????????

void draw3DChip(QPainter& p, const QRectF& disc, bool isRed) {
    const QPointF c = disc.center();
    const double  R = disc.width() * 0.5;

    QColor highlight, base, dark;
    if (isRed) {
        highlight = QColor(255, 100, 100);
        base      = QColor(215,  25,  25);
        dark      = QColor(110,   0,   0);
    } else {
        highlight = QColor(255, 250, 160);
        base      = QColor(240, 200,  20);
        dark      = QColor(140,  95,   0);
    }

    const QPointF lightOffset(-R * 0.30, -R * 0.30);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    {
        QRadialGradient shadow(c + QPointF(R * 0.08, R * 0.12), R * 1.05);
        shadow.setColorAt(0.00, QColor(0, 0, 0, 110));
        shadow.setColorAt(0.70, QColor(0, 0, 0,  35));
        shadow.setColorAt(1.00, QColor(0, 0, 0,   0));
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawEllipse(disc.translated(R * 0.04, R * 0.06));
    }

    {
        QRadialGradient g(c + lightOffset, R * 1.35);
        g.setColorAt(0.00, highlight);
        g.setColorAt(0.35, base.lighter(115));
        g.setColorAt(0.75, base);
        g.setColorAt(1.00, dark);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(disc);
    }

    p.restore();
}

void drawHighlight(QPainter& p, const QRectF& disc) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Theme::selectedGlow(),
                  std::max(2.0, disc.width() * 0.08)));
    p.drawEllipse(disc.adjusted(-2, -2, 2, 2));
}

// ?? King marker: 5-point star ?????????????????????????????????????????????

/// Build a 5-point star path with the first point at 12 o'clock.
QPainterPath starPath(const QPointF& center, double outerR, double innerR) {
    QPainterPath path;
    constexpr int    kPoints     = 5;
    constexpr double kPi         = 3.14159265358979323846;
    constexpr double kStartAngle = -kPi / 2.0;   // straight up

    for (int i = 0; i < kPoints * 2; ++i) {
        const double angle = kStartAngle + i * kPi / kPoints;
        const double r     = (i % 2 == 0) ? outerR : innerR;
        const QPointF pt(center.x() + r * std::cos(angle),
                         center.y() + r * std::sin(angle));
        if (i == 0) path.moveTo(pt);
        else        path.lineTo(pt);
    }
    path.closeSubpath();
    return path;
}

/// Kings are marked with a 5-point star in the same colour family as the
/// chip. The fill is a brighter shade so the star stays visible against
/// both the light and dark parts of the chip's 3D gradient, and a darker
/// outline defines the edges.
///
///   Red king    -> bright red star with dark-red outline
///   Yellow king -> bright yellow star with dark-amber outline
void drawStar(QPainter& p, const QRectF& disc, bool isRedPiece) {
    const QPointF c       = disc.center();
    const double  outerR  = disc.width() * 0.26;
    const double  innerR  = outerR * 0.42;

    QColor fill, edge;
    if (isRedPiece) {
        fill = QColor(255,  70,  70);   // bright red
        edge = QColor( 80,   0,   0);   // dark red outline
    } else {
        fill = QColor(255, 255, 190);   // bright yellow
        edge = QColor(120,  80,   0);   // dark amber outline
    }

    p.setPen(QPen(edge, std::max(1.0, disc.width() * 0.018)));
    p.setBrush(fill);
    p.drawPath(starPath(c, outerR, innerR));
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

    const bool isRed = (color == core::Color::Red);
    draw3DChip(p, disc, isRed);

    if (kind == core::PieceKind::King) drawStar(p, disc, isRed);
    if (highlighted)                   drawHighlight(p, disc);

    p.restore();
}

} // namespace draughts::ui
