#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/BoardWidget.hpp"
#include "ui/GameController.hpp"
#include "ui/HistoryPanel.hpp"
#include "ui/Settings.hpp"
#include "ui/SidePanel.hpp"
#include "ui/SoundManager.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("DraughtsApp");
    QCoreApplication::setApplicationName("Draughts");

    QMainWindow window;
    window.setWindowTitle("10x10 International Draughts");
    window.resize(1180, 760);

    auto* controller = new draughts::ui::GameController(&window);

    auto* board = new draughts::ui::BoardWidget(&window);
    board->setController(controller);

    // Default orientation: player's colour at the bottom.
    {
        const auto pc = draughts::ui::Settings::instance().playerColor();
        board->setRotated(pc == draughts::core::Color::Red);
    }

    auto* side = new draughts::ui::SidePanel(&window);
    auto* history = new draughts::ui::HistoryPanel(&window);
    side->setController(controller, board);
    history->setController(controller);

    auto* sound = new draughts::ui::SoundManager(&window);
    sound->setController(controller);

    auto* rightColumn = new QWidget(&window);
    auto* rl = new QVBoxLayout(rightColumn);
    rl->setContentsMargins(12, 12, 12, 12);
    rl->addWidget(side);
    rl->addWidget(history, 1);

    auto* splitter = new QSplitter(Qt::Horizontal, &window);
    splitter->addWidget(board);
    splitter->addWidget(rightColumn);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 800, 380 });

    window.setCentralWidget(splitter);
    window.show();
    return app.exec();
}
