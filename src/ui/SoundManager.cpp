#include "SoundManager.hpp"
#include "Settings.hpp"
#include "GameController.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace draughts::ui {

namespace {
/// Locate data/assets/sounds/ by searching upward from the exe dir.
/// Works whether we run from build/Release/ or a deployed install.
QString soundsDir() {
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString candidate = d.absoluteFilePath("data/assets/sounds");
        if (QFileInfo::exists(candidate + "/move.wav")) return candidate;
        if (!d.cdUp()) break;
    }
    return {};
}
} // namespace

SoundManager::SoundManager(QObject* parent) : QObject(parent) {
    const QString dir = soundsDir();
    if (dir.isEmpty()) return;
    loadEffect(moveSfx_,    dir + "/move.wav");
    loadEffect(captureSfx_, dir + "/capture.wav");
    loadEffect(kingSfx_,    dir + "/king.wav");
    loadEffect(winSfx_,     dir + "/win.wav");
}

void SoundManager::loadEffect(QSoundEffect& effect, const QString& fileName) {
    effect.setSource(QUrl::fromLocalFile(fileName));
    effect.setVolume(0.55f);
}

void SoundManager::setController(GameController* c) {
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = c;
    if (controller_) {
        connect(controller_, &GameController::moveApplied,
                this, &SoundManager::onMoveApplied);
        connect(controller_, &GameController::changed,
                this, &SoundManager::onChanged);
    }
    lastResult_ = c ? c->result() : core::GameResult::Ongoing;
}

void SoundManager::onMoveApplied(const core::MoveRecord& rec,
                                 const core::Board& /*preBoard*/) {
    if (!Settings::instance().soundEnabled()) return;

    if (rec.move.isPromotion)          kingSfx_.play();
    else if (rec.move.isCapture())     captureSfx_.play();
    else                                moveSfx_.play();
}

void SoundManager::onChanged() {
    if (!controller_ || !Settings::instance().soundEnabled()) return;
    const auto now = controller_->result();
    if (now != core::GameResult::Ongoing && lastResult_ == core::GameResult::Ongoing) {
        winSfx_.play();
    }
    lastResult_ = now;
}

} // namespace draughts::ui

