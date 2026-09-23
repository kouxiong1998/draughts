#pragma once
#include "core/RuleSet.hpp"
#include "core/Types.hpp"

#include <QObject>

namespace draughts::ui {

class Settings : public QObject {
    Q_OBJECT
public:
    static Settings& instance();

    [[nodiscard]] bool soundEnabled() const noexcept { return soundEnabled_; }
    void setSoundEnabled(bool on);

    [[nodiscard]] double animationSpeed() const noexcept { return animSpeed_; }
    void setAnimationSpeed(double s);

    [[nodiscard]] core::RuleSet nextRuleSet() const noexcept { return nextRules_; }
    void setNextRuleSet(core::RuleSet r);

    [[nodiscard]] core::Color nextFirstPlayer() const noexcept { return nextFirst_; }
    void setNextFirstPlayer(core::Color c);

    [[nodiscard]] core::Color playerColor() const noexcept { return playerColor_; }
    void setPlayerColor(core::Color c);

    /// Which mode the last session used. 0 = Human vs Human, 1 = Human vs AI.
    /// Persisted so the app resumes in the same mode on next launch.
    [[nodiscard]] int lastGameMode() const noexcept { return lastGameMode_; }
    void setLastGameMode(int m);

    /// Per-move think time in milliseconds. One of the six presets:
    ///   Instant (300 ms) / Fast (2000) / Normal (5000) / Slow (30000)
    ///   Deep (120000) / Analysis (600000)
    [[nodiscard]] int thinkTimeMs() const noexcept { return thinkTimeMs_; }
    void setThinkTimeMs(int ms);

    void load();
    void save();

signals:
    void changed();

private:
    Settings();
    bool          soundEnabled_{true};
    double        animSpeed_{1.0};
    core::RuleSet nextRules_{core::RuleSet::InternationalMaxCapture};
    core::Color   nextFirst_{core::Color::Red};
    core::Color   playerColor_{core::Color::Yellow};
    int           lastGameMode_{0};
    int           thinkTimeMs_{5000};   // Normal = 5 s default
    bool          loading_{false};
};

} // namespace draughts::ui
