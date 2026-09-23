#pragma once
/// @file Animator.hpp
/// @brief Drives smooth piece-slide animations. Owns a ~60 Hz timer and a
///        path of (col, row) waypoints. BoardWidget reads position() each
///        repaint; the rest of the app is unaware it exists.

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <vector>

class QTimer;

namespace draughts::ui {

class Animator : public QObject {
    Q_OBJECT
public:
    explicit Animator(QObject* parent = nullptr);

    [[nodiscard]] bool running() const noexcept { return running_; }

    /// Begin a new animation. `path` must have ? 2 points, in
    /// (col, row) floating-point board coordinates. Duration is ms.
    void start(std::vector<QPointF> path, int durationMs);
    void stop();

    /// Interpolated position in the same coordinate space as the input path.
    [[nodiscard]] QPointF position() const noexcept;

    /// 0..1 progress across the whole animation.
    [[nodiscard]] double fraction() const noexcept;

signals:
    void updated();
    void finished();

private slots:
    void onTick();

private:
    QTimer*              timer_{nullptr};
    QElapsedTimer        clock_{};
    std::vector<QPointF> path_{};
    int                  durationMs_{0};
    bool                 running_{false};
};

} // namespace draughts::ui
