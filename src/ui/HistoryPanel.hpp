#pragma once
/// @file HistoryPanel.hpp
/// @brief Scrollable list of moves in FMJD algebraic notation. Reads from
///        GameController::history() and formats every entry through
///        core::Notation ? never re-implements notation.

#include <QWidget>

class QListWidget;

namespace draughts::ui {

class GameController;

class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget* parent = nullptr);

    void setController(GameController* controller);

public slots:
    void refresh();

private:
    GameController* controller_{nullptr};
    QListWidget*    list_{nullptr};
};

} // namespace draughts::ui
