#pragma once
#include <QWidget>

class QLabel;
class QPushButton;
class QRadioButton;

namespace draughts::ui {

class GameController;
class BoardWidget;

class SidePanel : public QWidget {
    Q_OBJECT
public:
    explicit SidePanel(QWidget* parent = nullptr);

    void setController(GameController* controller, BoardWidget* board);

public slots:
    void refresh();

private slots:
    void onSaveGame();
    void onLoadGame();
    void onCopyNotation();
    void onAIThinkingChanged(bool thinking);
    void onAIProgress(int depth, quint64 nodes, int score, qint64 ms);

private:
    GameController* controller_{nullptr};
    BoardWidget*    board_{nullptr};

    QLabel* turnLabel_{nullptr};
    QLabel* moveLabel_{nullptr};
    QLabel* captureLabel_{nullptr};
    QLabel* resultLabel_{nullptr};
    QLabel* aiStatusLabel_{nullptr};

    QRadioButton* modeHumanHuman_{nullptr};
    QRadioButton* modeHumanAI_{nullptr};

    QPushButton* undoBtn_{nullptr};
    QPushButton* redoBtn_{nullptr};
    QPushButton* rotateBtn_{nullptr};
    QPushButton* newGameBtn_{nullptr};
    QPushButton* resignBtn_{nullptr};
    QPushButton* settingsBtn_{nullptr};
    QPushButton* saveBtn_{nullptr};
    QPushButton* loadBtn_{nullptr};
    QPushButton* copyBtn_{nullptr};
};

} // namespace draughts::ui
