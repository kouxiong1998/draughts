#include "Animator.hpp"

#include <QTimer>
#include <algorithm>

namespace draughts::ui {

Animator::Animator(QObject* parent) : QObject(parent) {
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, &Animator::onTick);
}

void Animator::start(std::vector<QPointF> path, int durationMs) {
    if (running_) stop();
    if (path.size() < 2 || durationMs <= 0) return;
    path_       = std::move(path);
    durationMs_ = durationMs;
    running_    = true;
    clock_.restart();
    if (!timer_->isActive()) timer_->start();
    emit updated();
}

void Animator::stop() {
    if (timer_->isActive()) timer_->stop();
    if (running_) { running_ = false; emit finished(); }
    path_.clear();
    durationMs_ = 0;
}

double Animator::fraction() const noexcept {
    if (!running_ || durationMs_ <= 0) return 1.0;
    const double t = static_cast<double>(clock_.elapsed());
    return std::clamp(t / static_cast<double>(durationMs_), 0.0, 1.0);
}

QPointF Animator::position() const noexcept {
    if (!running_ || path_.size() < 2) return {};
    const double f     = fraction();
    const int    nSeg  = static_cast<int>(path_.size()) - 1;
    const double scaled = f * nSeg;
    int seg = static_cast<int>(scaled);
    if (seg >= nSeg) seg = nSeg - 1;
    const double local = scaled - seg;

    // Smoothstep per segment for a pleasant ease-in/out.
    const double e = local * local * (3.0 - 2.0 * local);

    const QPointF& a = path_[seg];
    const QPointF& b = path_[seg + 1];
    return { a.x() + (b.x() - a.x()) * e,
             a.y() + (b.y() - a.y()) * e };
}

void Animator::onTick() {
    if (!running_) { timer_->stop(); return; }
    if (fraction() >= 1.0) { stop(); return; }
    emit updated();
}

} // namespace draughts::ui
