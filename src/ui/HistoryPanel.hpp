#pragma once
/// @file HistoryPanel.hpp
/// @brief Scrollable list of moves in FMJD algebraic notation. Has a small
///        back-arrow at the top that returns to the main side panel.

#include <QWidget>

class QListWidget;

namespace draughts::ui {

class GameController;

class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget* parent = nullptr);

    void setController(GameController* controller);

signals:
    void backRequested();

public slots:
    void refresh();

private:
    GameController* controller_{nullptr};
    QListWidget*    list_{nullptr};
};

} // namespace draughts::ui
