#include "mainwindow.h"

#include <DWidgetUtil>
#include <DTitlebar>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <qtermwidget.h>

static const char *HERDR_BINARY = "herdr";
static const char *HERDR_CONFIG_DIR = "herdr";

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_terminal(nullptr)
    , m_launchTimer(new QTimer(this))
    , m_launchAttempts(0)
{
    resize(1200, 800);
    initUI();

    titlebar()->setIcon(QIcon::fromTheme("deepin-herdr"));
    titlebar()->setSwitchThemeMenuVisible(true);

    m_launchTimer->setSingleShot(true);
    m_launchTimer->setInterval(500);

    QString socketPath = QDir::homePath() + "/.config/" + HERDR_CONFIG_DIR
        + "/herdr-client.sock";

    connect(m_launchTimer, &QTimer::timeout, this, [this, socketPath]() {
        ensureServerRunning(socketPath);
    });
    ensureServerRunning(socketPath);
}

MainWindow::~MainWindow() = default;

void MainWindow::initUI()
{
    auto *centralWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_terminal = new QTermWidget(0, centralWidget);
    layout->addWidget(m_terminal);

    setCentralWidget(centralWidget);
    Dtk::Widget::moveToCenter(this);
}

void MainWindow::ensureServerRunning(const QString &socketPath)
{
    if (QFileInfo::exists(socketPath)) {
        launchClient();
        return;
    }

    if (m_launchAttempts >= 30) {
        return;
    }

    if (m_launchAttempts == 0) {
        QString binary = findHerdrBinary();
        if (binary.isEmpty()) {
            return;
        }
        QProcess::startDetached(binary, {"server"});
    }

    m_launchAttempts++;
    m_launchTimer->start();
}

void MainWindow::launchClient()
{
    m_launchTimer->stop();

    qunsetenv("HERDR_ENV");
    qunsetenv("HERDR_PANE_ID");
    qunsetenv("HERDR_SOCKET_PATH");

    QString binary = findHerdrBinary();
    if (binary.isEmpty()) {
        return;
    }

    m_terminal->setShellProgram(binary);
    m_terminal->setArgs({"client"});
    m_terminal->startShellProgram();

    connect(m_terminal, &QTermWidget::finished, this, &QWidget::close);
}

QString MainWindow::findHerdrBinary() const
{
    QString binary = QStandardPaths::findExecutable(HERDR_BINARY);
    if (!binary.isEmpty()) {
        return binary;
    }
    QString fallback = QDir::homePath() + "/.local/bin/" + HERDR_BINARY;
    if (QFileInfo::exists(fallback)) {
        return fallback;
    }
    return {};
}
