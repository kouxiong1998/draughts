#include "SettingsDialog.hpp"
#include "Settings.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

namespace draughts::ui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings");
    setMinimumWidth(440);

    auto& st = Settings::instance();

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    root->addLayout(form);

    soundCheck_ = new QCheckBox("Enable sound effects", this);
    soundCheck_->setChecked(st.soundEnabled());
    form->addRow(soundCheck_);

    animSpeedCombo_ = new QComboBox(this);
    animSpeedCombo_->addItem("Fast (0.5x)",   0.5);
    animSpeedCombo_->addItem("Normal (1.0x)", 1.0);
    animSpeedCombo_->addItem("Slow (1.75x)",  1.75);
    {
        const double s = st.animationSpeed();
        int idx = 1;
        if (s < 0.75)      idx = 0;
        else if (s > 1.25) idx = 2;
        animSpeedCombo_->setCurrentIndex(idx);
    }
    form->addRow("Animation speed:", animSpeedCombo_);

    form->addRow(new QLabel("<hr>", this));

    maxCaptureOn_  = new QRadioButton("Majority (max capture ON)",  this);
    maxCaptureOff_ = new QRadioButton("Free capture (max capture OFF)", this);
    auto* captureGroup = new QButtonGroup(this);
    captureGroup->addButton(maxCaptureOn_);
    captureGroup->addButton(maxCaptureOff_);
    (st.nextRuleSet() == core::RuleSet::InternationalMaxCapture
         ? maxCaptureOn_ : maxCaptureOff_)->setChecked(true);
    form->addRow("Capture rule:", maxCaptureOn_);
    form->addRow("",              maxCaptureOff_);

    firstRed_    = new QRadioButton("Red",    this);
    firstYellow_ = new QRadioButton("Yellow", this);
    auto* firstGroup = new QButtonGroup(this);
    firstGroup->addButton(firstRed_);
    firstGroup->addButton(firstYellow_);
    (st.nextFirstPlayer() == core::Color::Red
         ? firstRed_ : firstYellow_)->setChecked(true);
    form->addRow("First move:", firstRed_);
    form->addRow("",            firstYellow_);

    form->addRow(new QLabel("<hr>", this));

    colorRed_    = new QRadioButton("Red",    this);
    colorYellow_ = new QRadioButton("Yellow", this);
    auto* colorGroup = new QButtonGroup(this);
    colorGroup->addButton(colorRed_);
    colorGroup->addButton(colorYellow_);
    (st.playerColor() == core::Color::Red
         ? colorRed_ : colorYellow_)->setChecked(true);
    form->addRow("Chip colour (I play as):", colorRed_);
    form->addRow("",                          colorYellow_);

    auto* note = new QLabel(
        "In Human vs AI, the AI plays the opposite colour automatically.\n"
        "In Human vs Human, both colours are played by humans; the chip\n"
        "colour you pick is drawn at the bottom of the board.\n"
        "Changing any of: capture rule, first move, or chip colour\n"
        "starts a new game.",
        this);
    note->setWordWrap(true);
    note->setStyleSheet("color: #888; margin-top: 6px;");
    root->addWidget(note);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted,
            this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

void SettingsDialog::onAccepted() {
    auto& st = Settings::instance();

    st.setSoundEnabled(soundCheck_->isChecked());
    st.setAnimationSpeed(animSpeedCombo_->currentData().toDouble());

    const core::RuleSet newRules = maxCaptureOn_->isChecked()
        ? core::RuleSet::InternationalMaxCapture
        : core::RuleSet::InternationalFreeCapture;
    const core::Color newFirst = firstRed_->isChecked()
        ? core::Color::Red : core::Color::Yellow;
    const core::Color newColor = colorRed_->isChecked()
        ? core::Color::Red : core::Color::Yellow;

    const bool rulesChanged = (newRules != st.nextRuleSet());
    const bool firstChanged = (newFirst != st.nextFirstPlayer());
    const bool colorChanged = (newColor != st.playerColor());

    st.setNextRuleSet(newRules);
    st.setNextFirstPlayer(newFirst);
    st.setPlayerColor(newColor);

    accept();

    if (rulesChanged || firstChanged || colorChanged) emit restartRequested();
}

} // namespace draughts::ui
