#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
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

/// Pick the screen the cursor is on, or the primary screen as fallback.
const QScreen* currentScreen() {
    if (const QScreen* s = QGuiApplication::screenAt(QCursor::pos())) return s;
    return QGuiApplication::primaryScreen();
}

/// Center the window both horizontally and vertically inside the screen's
/// available area (which already excludes the taskbar). Called before and
/// after show() so Qt's layout system can't push it off-center.
void placeWindow(QMainWindow& window, int wantedW, int wantedH) {
    const QScreen* screen = currentScreen();
    if (!screen) {
        window.resize(wantedW, wantedH);
        return;
    }

    const QRect avail = screen->availableGeometry();

    // Clamp size so we never exceed the available rectangle.
    const int winW = std::min(wantedW, std::max(640, avail.width()));
    const int winH = std::min(wantedH, std::max(480, avail.height()));

    window.resize(winW, winH);

    // Center both axes.
    const int x = avail.x() + (avail.width()  - winW) / 2;
    const int y = avail.y() + (avail.height() - winH) / 2;

    window.move(x, y);
}

/// Re-apply the screen-center clamp after show(). Qt on Windows may grow
/// or reposition the window during show() to satisfy layout minimums; this
/// runs from a zero-delay timer once the event loop settles, using the
/// frame-vs-client offset so the whole window (title bar included) ends
/// up centered inside the available area.
void enforceScreenBounds(QMainWindow& window) {
    const QScreen* screen =
        QGuiApplication::screenAt(window.frameGeometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    const QRect avail = screen->availableGeometry();

    // Frame-vs-client offset (Windows: ~8 px left/right, ~30 px title bar).
    const QRect frame  = window.frameGeometry();
    const QRect client = window.geometry();
    const int frameDX = client.x() - frame.x();
    const int frameDY = client.y() - frame.y();

    // Fit the frame entirely inside the available rectangle.
    const int fW = std::min(frame.width(),  avail.width());
    const int fH = std::min(frame.height(), avail.height());

    // Center the frame inside the available rectangle.
    const int fX = avail.x() + (avail.width()  - fW) / 2;
    const int fY = avail.y() + (avail.height() - fH) / 2;

    // Convert frame coords back to client coords for resize/move.
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

    // ?? Board stacked tightly with a gear-icon panel toggle button below ???
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

    // ?? Right column ????????????????????????????????????????????????????????
    auto* side    = new draughts::ui::SidePanel(&window);
    auto* history = new draughts::ui::HistoryPanel(&window);
    side->setController(controller, board);
    history->setController(controller);

    auto* sound = new draughts::ui::SoundManager(&window);
    sound->setController(controller);

    auto* rightColumn = new QWidget(&window);
    auto* rightLayout = new QVBoxLayout(rightColumn);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->addWidget(side);
    rightLayout->addWidget(history, 1);

    // ?? Splitter ????????????????????????????????????????????????????????????
    const int w = window.width();
    const QList<int> kDefaultSizes{ w * 2 / 3, w / 3 };

    auto* splitter = new QSplitter(Qt::Horizontal, &window);
    splitter->addWidget(leftColumn);
    splitter->addWidget(rightColumn);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(kDefaultSizes);

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

    QObject::connect(side, &draughts::ui::SidePanel::hidePanelRequested,
                     &window, [splitter]{
        splitter->widget(1)->setVisible(false);
    });

    window.setCentralWidget(splitter);
    window.show();

    // Qt on Windows can grow or reposition the window during show() to
    // satisfy the layout's minimum sizes. Re-apply the screen-bounds
    // clamp once the event loop has processed the show() call.
    QTimer::singleShot(0, &window, [&window]{
        enforceScreenBounds(window);
    });

    return app.exec();
}
