#include "mainwindow.h"
#include "platform.h"

#include <QGuiApplication>
#include <QMenuBar>

#if HAVE_DTK
#include <DMainWindow>
#include <DTitlebar>
#include <DWidgetUtil>
DWIDGET_USE_NAMESPACE
#endif

namespace {

// Registers shared wiring that both shells need before init().
void setupShell(QMainWindow *window, AppCore *core)
{
    window->setWindowIcon(QIcon::fromTheme(QStringLiteral("mudi")));
    window->resize(1200, 800);

    window->connect(core, &AppCore::closeRequested, window, &QMainWindow::close);
    core->setTranslucencyHandler([window](bool on) {
        window->setAttribute(Qt::WA_TranslucentBackground, on);
    });

    core->init();

    window->setCentralWidget(core->container());
    window->menuBar()->addMenu(core->themeMenu());
    window->menuBar()->addAction(core->settingsAction());
    Platform::moveToCenter(window);
}

#if HAVE_DTK
class DtkMainWindow final : public DMainWindow {
public:
    explicit DtkMainWindow(AppCore *core)
    {
        setWindowIcon(QIcon::fromTheme(QStringLiteral("mudi")));
        resize(1200, 800);
        connect(core, &AppCore::closeRequested, this, &DMainWindow::close);
        core->setTranslucencyHandler([this](bool on) {
            setTranslucentBackground(on);
        });
        core->init();

        titlebar()->setSwitchThemeMenuVisible(false);
        if (QMenu *dtkMenu = titlebar()->menu()) {
            dtkMenu->addMenu(core->themeMenu());
            dtkMenu->addAction(core->settingsAction());
        }
        setCentralWidget(core->container());
        Dtk::Widget::moveToCenter(this);
    }
};
#endif

class GenericMainWindow final : public QMainWindow {
public:
    explicit GenericMainWindow(AppCore *core)
    {
        setupShell(this, core);
    }
};

} // namespace

QMainWindow *createMainWindow(AppCore *core)
{
#if HAVE_DTK
    if (Platform::useDtk()) {
        return new DtkMainWindow(core);
    }
#endif
    return new GenericMainWindow(core);
}
