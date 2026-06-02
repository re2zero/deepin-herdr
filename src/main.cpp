#include <cstdio>
#include <QtGlobal>
#include <DApplication>

#include "mainwindow.h"

DWIDGET_USE_NAMESPACE

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    Q_UNUSED(ctx)
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "[deepin-herdr] %s\n", qPrintable(msg));
    }
}

int main(int argc, char *argv[]) {
    qInstallMessageHandler(messageHandler);

    DApplication app(argc, argv);
    app.loadTranslator();

    app.setApplicationName("deepin-herdr");
    app.setApplicationDisplayName("deepin-herdr");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("deepin");
    app.setProductIcon(QIcon::fromTheme("deepin-herdr"));
    app.setApplicationDescription(
        QApplication::translate("main",
            "deepin-herdr is a DTK frontend for herdr terminal\n"
            "workspace manager.\n"
            "herdr provides a terminal-based IDE experience with\n"
            "multi-pane workspaces, tabs, and AI agent integration."));

    MainWindow window;
    window.show();

    return app.exec();
}
