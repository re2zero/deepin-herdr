#include <cstdio>
#include <QtGlobal>

#include <QApplication>

#include "platform.h"
#include "version.h"
#include "mainwindow.h"
#include "appcore.h"

#if HAVE_DTK
#include <DApplication>
DWIDGET_USE_NAMESPACE
#endif

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    Q_UNUSED(ctx)
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "[mudi] %s\n", qPrintable(msg));
    }
}

int main(int argc, char *argv[]) {
    qInstallMessageHandler(messageHandler);

#if HAVE_DTK
    // DTK flavor on deepin/UOS: DApplication brings the deepin translator
    // loading, product icon and about description.
    QApplication *app = nullptr;
    if (Platform::useDtk()) {
        auto *dtkApp = new DApplication(argc, argv);
        dtkApp->loadTranslator();
        dtkApp->setProductIcon(QIcon::fromTheme("mudi"));
        dtkApp->setApplicationDescription(
            QApplication::translate("main",
                "MuDi (牧笛) is a GUI frontend for the herdr terminal\n"
                "workspace manager for AI coding agents.\n"
                "herdr provides a terminal-based IDE experience with\n"
                "multi-pane workspaces, tabs, and AI agent integration."));
        app = dtkApp;
    } else {
        app = new QApplication(argc, argv);
    }
#else
    QApplication *app = new QApplication(argc, argv);
#endif

    app->setApplicationName("mudi");
    app->setApplicationDisplayName("MuDi");
    app->setOrganizationName("mudi");
    if (!Platform::useDtk()) {
        // DApplication::loadTranslator covers the DTK flavor
        Platform::loadTranslations(app);
    }
    app->setApplicationVersion(APP_VERSION);
    // import pre-rename settings before anything reads them
    Platform::migrateLegacySettings();

    AppCore *core = new AppCore(app);
    QMainWindow *window = createMainWindow(core);
    window->show();

    return app->exec();
}
