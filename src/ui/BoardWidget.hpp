#pragma once
/// @file BoardWidget.hpp
/// @brief Renders the ONE board used by every mode, forwards mouse clicks to
///        the GameController, and animates piece slides via the Animator.

#include "core/Board.hpp"
#include "core/GameEngine.hpp"
#include "core/Types.hpp"

#include <QPoint>
#include <QRectF>
#include <QWidget>
#include <array>
#include <optional>

class QPaintEvent;
class QMouseEvent;

namespace draughts::ui {

class GameController;
class Animator;

class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(QWidget* parent = nullptr);

    void setController(GameController* controller);

    [[nodiscard]] bool isRotated() const noexcept { return rotated_; }
    void setRotated(bool r) { rotated_ = r; update(); }

    [[nodiscard]] QSize sizeHint() const override { return {640, 640}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onMoveApplied(const core::MoveRecord& rec, const core::Board& preBoard);
    void onAnimationUpdated();

private:
    GameController* controller_{nullptr};
    Animator*       animator_{nullptr};
    core::Board     board_{};
    bool            rotated_{false};

    // Cached state for the duration of one animation.
    core::Board     preBoard_{};
    core::Move      animMove_{};
    core::Color     animMover_{core::Color::Red};

    [[nodiscard]] int    computeCellSize(int w, int h) const noexcept;
    [[nodiscard]] QPoint boardOrigin   (int w, int h, int cell) const noexcept;
    [[nodiscard]] QRectF cellRect      (int row, int col, int cell,
                                        const QPoint& origin) const noexcept;
    [[nodiscard]] std::optional<core::Square>
                         squareAt      (const QPoint& pixel) const noexcept;

    void drawSurface   (QPainter& p, int cell, const QPoint& origin) const;
    void drawHighlights(QPainter& p, int cell, const QPoint& origin) const;
    void drawPieces    (QPainter& p, int cell, const QPoint& origin) const;
    void drawPiecesAnimated(QPainter& p, int cell, const QPoint& origin) const;
};

} // namespace draughts::ui

