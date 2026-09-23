#pragma once
/// @file PieceRenderer.hpp
/// @brief Renders one draughts piece (man or king) into a given rectangle.
///        Used by BoardWidget and by any preview UI (settings dialog, etc.).

#include "core/Types.hpp"
#include <QRectF>

class QPainter;

namespace draughts::ui {

class PieceRenderer {
public:
    /// Draw a piece centered in `cellRect`. `highlighted` adds a soft glow
    /// ring for selection/hover feedback.
    static void draw(QPainter&       painter,
                     const QRectF&   cellRect,
                     core::Color     color,
                     core::PieceKind kind,
                     bool            highlighted = false);
};

} // namespace draughts::ui
