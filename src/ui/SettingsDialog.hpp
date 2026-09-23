#pragma once
#include <QDialog>

class QCheckBox;
class QComboBox;
class QRadioButton;

namespace draughts::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void restartRequested();

private slots:
    void onAccepted();

private:
    QCheckBox*    soundCheck_{nullptr};
    QComboBox*    animSpeedCombo_{nullptr};
    QRadioButton* maxCaptureOn_{nullptr};
    QRadioButton* maxCaptureOff_{nullptr};
    QRadioButton* firstRed_{nullptr};
    QRadioButton* firstYellow_{nullptr};
    QRadioButton* colorRed_{nullptr};
    QRadioButton* colorYellow_{nullptr};
};

} // namespace draughts::ui
