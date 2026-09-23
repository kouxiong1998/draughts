#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

#include "ui/BoardWidget.hpp"
#include "ui/GameController.hpp"
#include "ui/HistoryPanel.hpp"
#include "ui/Settings.hpp"
#include "ui/SidePanel.hpp"
#include "ui/SoundManager.hpp"

namespace {

const QScreen* currentScreen() {
    if (const QScreen* s = QGuiApplication::screenAt(QCursor::pos())) return s;
    return QGuiApplication::primaryScreen();
}

void placeWindow(QMainWindow& window, int wantedW, int wantedH) {
    const QScreen* screen = currentScreen();
    if (!screen) {
        window.resize(wantedW, wantedH);
        return;
    }
    const QRect avail = screen->availableGeometry();
    const int winW = std::min(wantedW, std::max(640, avail.width()));
    const int winH = std::min(wantedH, std::max(480, avail.height()));
    window.resize(winW, winH);
    const int x = avail.x() + (avail.width()  - winW) / 2;
    const int y = avail.y() + (avail.height() - winH) / 2;
    window.move(x, y);
}

void enforceScreenBounds(QMainWindow& window) {
    const QScreen* screen =
        QGuiApplication::screenAt(window.frameGeometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    const QRect frame  = window.frameGeometry();
    const QRect client = window.geometry();
    const int frameDX = client.x() - frame.x();
    const int frameDY = client.y() - frame.y();
    const int fW = std::min(frame.width(),  avail.width());
    const int fH = std::min(frame.height(), avail.height());
    const int fX = avail.x() + (avail.width()  - fW) / 2;
    const int fY = avail.y() + (avail.height() - fH) / 2;
    const int cw = fW - 2 * frameDX;
    const int ch = fH - frameDY - frameDX;
    const int cx = fX + frameDX;
    const int cy = fY + frameDY;
    window.resize(std::max(320, cw), std::max(240, ch));
    window.move(cx, cy);
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("DraughtsApp");
    QCoreApplication::setApplicationName("Draughts");

    QMainWindow window;
    window.setWindowTitle("10x10 International Draughts");
    placeWindow(window, 1100, 720);

    auto* controller = new draughts::ui::GameController(&window);

    // ?? Left column: board + gear (panel toggle) below ?????????????????????
    auto* board = new draughts::ui::BoardWidget(&window);
    board->setController(controller);
    {
        const auto pc = draughts::ui::Settings::instance().playerColor();
        board->setRotated(pc == draughts::core::Color::Red);
    }

    auto* toggleBtn = new QPushButton(QStringLiteral("\u2699"), &window);
    toggleBtn->setFixedSize(36, 36);
    toggleBtn->setToolTip("Show / hide the side panel");
    {
        QFont f = toggleBtn->font();
        f.setPointSize(16);
        toggleBtn->setFont(f);
    }

    auto* leftColumn = new QWidget(&window);
    auto* leftLayout = new QVBoxLayout(leftColumn);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->setSpacing(4);
    leftLayout->addWidget(board, 1);
    leftLayout->addWidget(toggleBtn, 0, Qt::AlignHCenter);

    // ?? Right column: SidePanel and HistoryPanel in a QStackedWidget ???????
    auto* side    = new draughts::ui::SidePanel(&window);
    auto* history = new draughts::ui::HistoryPanel(&window);
    side->setController(controller, board);
    history->setController(controller);

    auto* sound = new draughts::ui::SoundManager(&window);
    sound->setController(controller);

    auto* stack = new QStackedWidget(&window);
    stack->addWidget(side);      // index 0
    stack->addWidget(history);   // index 1
    stack->setCurrentIndex(0);

    auto* rightColumn = new QWidget(&window);
    auto* rightLayout = new QVBoxLayout(rightColumn);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->addWidget(stack, 1);

    // ?? Splitter ???????????????????????????????????????????????????????????
    const int w = window.width();
    const QList<int> kDefaultSizes{ w * 2 / 3, w / 3 };

    auto* splitter = new QSplitter(Qt::Horizontal, &window);
    splitter->addWidget(leftColumn);
    splitter->addWidget(rightColumn);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(kDefaultSizes);

    // Gear button: show / hide the whole right column.
    QObject::connect(toggleBtn, &QPushButton::clicked,
                     &window, [splitter, kDefaultSizes]{
        auto* panel = splitter->widget(1);
        if (panel->isVisible()) {
            panel->setVisible(false);
        } else {
            panel->setVisible(true);
            splitter->setSizes(kDefaultSizes);
        }
    });

    // ? in the side panel: hide the whole right column.
    QObject::connect(side, &draughts::ui::SidePanel::hidePanelRequested,
                     &window, [splitter]{
        splitter->widget(1)->setVisible(false);
    });

    // History button in the side panel: swap the stacked view to History.
    QObject::connect(side, &draughts::ui::SidePanel::historyRequested,
                     &window, [stack, splitter]{
        auto* panel = splitter->widget(1);
        if (!panel->isVisible()) panel->setVisible(true);
        stack->setCurrentIndex(1);
    });

    // ? in the history panel: swap back to the side panel.
    QObject::connect(history, &draughts::ui::HistoryPanel::backRequested,
                     &window, [stack]{
        stack->setCurrentIndex(0);
    });

    window.setCentralWidget(splitter);
    window.show();

    QTimer::singleShot(0, &window, [&window]{
        enforceScreenBounds(window);
    });

    return app.exec();
}
