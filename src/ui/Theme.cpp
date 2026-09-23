#include "Theme.hpp"

namespace draughts::ui {

QColor Theme::boardBackground()     { return QColor( 24,  24,  24); }
QColor Theme::boardShadow()         { return QColor(  0,   0,   0, 120); }
QColor Theme::darkSquare()          { return QColor( 20,  20,  20); }   // black
QColor Theme::lightSquare()         { return QColor(240, 240, 240); }   // white
QColor Theme::gridLine()            { return QColor(  0,   0,   0, 60); }

QColor Theme::redPiece()            { return QColor(255,   0,   0); }
QColor Theme::redPieceHighlight()   { return QColor(255, 110, 110); }
QColor Theme::redPieceEdge()        { return QColor(140,   0,   0); }

QColor Theme::yellowPiece()         { return QColor(255, 255,   0); }
QColor Theme::yellowPieceHighlight(){ return QColor(255, 255, 180); }
QColor Theme::yellowPieceEdge()     { return QColor(158, 138,   0); }

QColor Theme::crown()               { return QColor(255, 215,   0); }
QColor Theme::crownEdge()           { return QColor(120,  80,   0); }
QColor Theme::selectedGlow()        { return QColor( 80, 200, 255, 200); }
QColor Theme::legalMoveDot()        { return QColor( 40, 220,  90, 210); }
QColor Theme::captureHighlight()    { return QColor(255,  80,  80, 220); }

QFont Theme::statusFont() {
    QFont f;
    f.setPointSize(11);
    return f;
}

} // namespace draughts::ui
