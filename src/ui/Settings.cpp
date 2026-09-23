#include "Settings.hpp"
#include <QSettings>

namespace draughts::ui {

Settings& Settings::instance() {
    static Settings s;
    return s;
}

Settings::Settings() : QObject(nullptr) { load(); }

void Settings::load() {
    loading_ = true;
    QSettings s;
    soundEnabled_ = s.value("sound/enabled", true).toBool();
    animSpeed_    = s.value("anim/speed", 1.0).toDouble();

    const int mc = s.value("rules/maxCapture", 1).toInt();
    nextRules_   = mc ? core::RuleSet::InternationalMaxCapture
                      : core::RuleSet::InternationalFreeCapture;

    const int fp = s.value("rules/firstPlayer", 0).toInt();
    nextFirst_   = fp ? core::Color::Yellow : core::Color::Red;

    const int pc = s.value("rules/playerColor", 1).toInt();
    playerColor_ = pc ? core::Color::Yellow : core::Color::Red;

    loading_ = false;
}

void Settings::save() {
    QSettings s;
    s.setValue("sound/enabled",   soundEnabled_);
    s.setValue("anim/speed",      animSpeed_);
    s.setValue("rules/maxCapture",
               nextRules_ == core::RuleSet::InternationalMaxCapture ? 1 : 0);
    s.setValue("rules/firstPlayer",
               nextFirst_ == core::Color::Yellow ? 1 : 0);
    s.setValue("rules/playerColor",
               playerColor_ == core::Color::Yellow ? 1 : 0);
    s.sync();
}

void Settings::setSoundEnabled(bool on) {
    if (soundEnabled_ == on) return;
    soundEnabled_ = on; save();
    if (!loading_) emit changed();
}

void Settings::setAnimationSpeed(double v) {
    if (animSpeed_ == v) return;
    animSpeed_ = v; save();
    if (!loading_) emit changed();
}

void Settings::setNextRuleSet(core::RuleSet r) {
    if (nextRules_ == r) return;
    nextRules_ = r; save();
    if (!loading_) emit changed();
}

void Settings::setNextFirstPlayer(core::Color c) {
    if (nextFirst_ == c) return;
    nextFirst_ = c; save();
    if (!loading_) emit changed();
}

void Settings::setPlayerColor(core::Color c) {
    if (playerColor_ == c) return;
    playerColor_ = c; save();
    if (!loading_) emit changed();
}

} // namespace draughts::ui
