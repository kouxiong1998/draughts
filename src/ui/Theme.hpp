#pragma once
/// @file Theme.hpp
/// @brief THE single source of truth for every visual constant (colors,
///        sizes, fonts). Board, pieces, panels, and dialogs all read from
///        here. Nothing anywhere else hard-codes a color or a size.

#include <QColor>
#include <QFont>

namespace draughts::ui {

struct Theme {
    // Geometry
    static constexpr int kBoardSize         = 10;
    static constexpr int kBorderMarginPx    = 14;
    static constexpr int kMinimumWidgetSide = 400;

    // Board surface
    [[nodiscard]] static QColor boardBackground();
    [[nodiscard]] static QColor boardShadow();
    [[nodiscard]] static QColor darkSquare();
    [[nodiscard]] static QColor lightSquare();
    [[nodiscard]] static QColor gridLine();

    // Red pieces (255, 0, 0)
    [[nodiscard]] static QColor redPiece();
    [[nodiscard]] static QColor redPieceHighlight();
    [[nodiscard]] static QColor redPieceEdge();

    // Yellow pieces (255, 255, 0)
    [[nodiscard]] static QColor yellowPiece();
    [[nodiscard]] static QColor yellowPieceHighlight();
    [[nodiscard]] static QColor yellowPieceEdge();

    // Decorations
    [[nodiscard]] static QColor crown();
    [[nodiscard]] static QColor crownEdge();
    [[nodiscard]] static QColor selectedGlow();
    [[nodiscard]] static QColor legalMoveDot();
    [[nodiscard]] static QColor captureHighlight();

    // Fonts
    [[nodiscard]] static QFont statusFont();
};

} // namespace draughts::ui
