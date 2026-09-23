#pragma once
/// @file SoundManager.hpp
/// @brief Plays the four game SFX (move, capture, king, win). Connects
///        itself to GameController and reacts to moveApplied / changed
///        signals. Silently no-ops if the WAV files are missing.

#include "core/GameEngine.hpp"

#include <QObject>
#include <QSoundEffect>

namespace draughts::ui {

class GameController;

class SoundManager : public QObject {
    Q_OBJECT
public:
    explicit SoundManager(QObject* parent = nullptr);

    void setController(GameController* controller);

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    void setEnabled(bool on) noexcept { enabled_ = on; }

private slots:
    void onMoveApplied(const core::MoveRecord& rec, const core::Board& preBoard);
    void onChanged();

private:
    void loadEffect(QSoundEffect& effect, const QString& fileName);

    GameController* controller_{nullptr};
    QSoundEffect    moveSfx_;
    QSoundEffect    captureSfx_;
    QSoundEffect    kingSfx_;
    QSoundEffect    winSfx_;
    bool            enabled_{true};
    core::GameResult lastResult_{core::GameResult::Ongoing};
};

} // namespace draughts::ui
