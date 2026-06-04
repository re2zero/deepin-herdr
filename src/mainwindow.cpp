#include "mainwindow.h"

#include <DWidgetUtil>
#include <DTitlebar>
#include <DDialog>
#include <DProgressBar>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QLabel>
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QShortcut>
#include <QDesktopServices>
#include <QUrl>

#include <QLocalSocket>
#include <QClipboard>

#include <qtermwidget.h>
#include <DGuiApplicationHelper>

static const char *HERDR_BINARY = "herdr";
static const char *HERDR_CONFIG_DIR = "herdr";

struct HerdrMirror {
    const char *name;
    const char *baseUrl;
    const char *downloadPath;
};

static const HerdrMirror HERDR_MIRRORS[] = {
    {"GitHub", "https://github.com", "/ogulcancelik/herdr/releases/download/v{ver}/herdr-linux-{arch}"},
};

static const char *HERDR_LATEST_VERSION = "0.6.6";

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

    connect(m_launchTimer, &QTimer::timeout, this, [this]() {
        QString socketPath = QDir::homePath() + "/.config/" + HERDR_CONFIG_DIR
            + "/herdr-client.sock";
        ensureServerRunning(socketPath);
    });

    checkHerdrAndStart();
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
    m_originalFont = m_terminal->getTerminalFont();

    setCentralWidget(centralWidget);
    Dtk::Widget::moveToCenter(this);

    auto bindKey = [this](const QKeySequence &key, auto slot) {
        auto *s = new QShortcut(key, m_terminal);
        connect(s, &QShortcut::activated, m_terminal, slot);
    };

    bindKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C),
        &QTermWidget::copyClipboard);
    bindKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V),
        &QTermWidget::pasteClipboard);
    bindKey(QKeySequence(Qt::CTRL | Qt::Key_Equal),
        &QTermWidget::zoomIn);
    bindKey(QKeySequence(Qt::CTRL | Qt::Key_Minus),
        &QTermWidget::zoomOut);
    bindKey(QKeySequence(Qt::CTRL | Qt::Key_0), [this]() {
        m_terminal->setTerminalFont(m_originalFont);
    });
    bindKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A),
        &QTermWidget::setSelectionAll);
    bindKey(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F),
        &QTermWidget::toggleShowSearchBar);

    applyTerminalColorScheme(DGuiApplicationHelper::instance()->themeType());
    connect(DGuiApplicationHelper::instance(),
        &DGuiApplicationHelper::themeTypeChanged, this,
        &MainWindow::applyTerminalColorScheme);

    connect(m_terminal, &QTermWidget::urlActivated, this,
        [](const QUrl &url, bool fromContextMenu) {
            Q_UNUSED(fromContextMenu);
            QDesktopServices::openUrl(url);
        });

    // Auto-copy on QTermWidget's native Shift+drag selection.
    connect(m_terminal, &QTermWidget::copyAvailable,
            m_terminal, [this](bool available) {
        if (available) {
            m_terminal->copyClipboard();
        }
    });

    // Runtime-detected OSC 52 clipboard support.
    // Newer libterminalwidget6 versions have osc52ClipboardRequest
    // as a real signal (parsed from Vt102Emulation). Older versions
    // do not — check at runtime via meta-object to avoid linker error.
    if (QTermWidget::staticMetaObject.indexOfSignal(
            "osc52ClipboardRequest(char,QString)") >= 0) {
        connect(m_terminal, SIGNAL(osc52ClipboardRequest(char,QString)),
                this, SLOT(handleOSC52Clipboard(char,QString)));
    }
}

void MainWindow::checkHerdrAndStart()
{
    if (!findHerdrBinary().isEmpty()) {
        QString socketPath = QDir::homePath() + "/.config/" + HERDR_CONFIG_DIR
            + "/herdr-client.sock";
        ensureServerRunning(socketPath);
        return;
    }

    installHerdr();
}

MainWindow::HerdrRelease MainWindow::selectRelease() const
{
    QString arch = QSysInfo::buildCpuArchitecture();
    QString archSuffix = (arch == "arm64") ? "aarch64" : "x86_64";

    HerdrRelease release;
    release.version = HERDR_LATEST_VERSION;
    release.url = QString(HERDR_MIRRORS[0].baseUrl) + QString(HERDR_MIRRORS[0].downloadPath)
        .arg(HERDR_LATEST_VERSION, archSuffix);
    return release;
}

void MainWindow::installHerdr()
{
    HerdrRelease release = selectRelease();

    QPointer<DDialog> dlg = new DDialog(this);
    dlg->setTitle(QObject::tr("Installing herdr"));
    dlg->setMessage(QObject::tr("herdr terminal workspace manager is required.\n"
                                "Downloading v%1...").arg(release.version));
    dlg->addButton(QObject::tr("Cancel"));
    dlg->setCloseButtonVisible(true);

    auto *progress = new DProgressBar(dlg);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setFixedWidth(300);
    dlg->addContent(progress);

    dlg->show();

    auto *nam = new QNetworkAccessManager(this);
    QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(release.url)));

    connect(reply, &QNetworkReply::downloadProgress, this,
        [progress](qint64 received, qint64 total) {
            if (total > 0) {
                progress->setValue(static_cast<int>(received * 100 / total));
            }
        });

    connect(reply, &QNetworkReply::readyRead, this,
        [this, reply, release, dlg]() {
            if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
                return;
            }
        });

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, release, dlg]() {
            reply->deleteLater();
            if (dlg) {
                dlg->close();
                dlg->deleteLater();
            }

            if (reply->error() != QNetworkReply::NoError) {
                auto *errDlg = new DDialog(this);
                errDlg->setTitle(QObject::tr("Download Failed"));
                errDlg->setMessage(QObject::tr("Failed to download herdr: %1\n"
                                             "Please install herdr manually to ~/.local/bin/herdr")
                    .arg(reply->errorString()));
                errDlg->addButton(QObject::tr("OK"));
                errDlg->exec();
                errDlg->deleteLater();
                close();
                return;
            }

            QString installDir = QDir::homePath() + "/.local/bin";
            QDir().mkpath(installDir);
            QString installPath = installDir + "/" + HERDR_BINARY;

            QFile::remove(installPath);
            if (!QFile::rename(reply->property("tempFile").toString(), installPath)) {
                QByteArray data = reply->readAll();
                QFile out(installPath);
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(data);
                    out.close();
                }
            }
            QFile::setPermissions(installPath,
                QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                | QFile::ReadGroup | QFile::ExeGroup
                | QFile::ReadOther | QFile::ExeOther);

            checkHerdrAndStart();
        });

    QString tempPath = QDir::tempPath() + "/herdr-download-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    reply->setProperty("tempFile", tempPath);

    auto *tempFile = new QFile(tempPath);
    if (tempFile->open(QIODevice::WriteOnly)) {
        connect(reply, &QNetworkReply::readyRead, this,
            [reply, tempFile]() {
                tempFile->write(reply->readAll());
            });
        connect(reply, &QNetworkReply::finished, this,
            [tempFile]() {
                tempFile->close();
                tempFile->deleteLater();
            });
    } else {
        delete tempFile;
    }

    connect(dlg, &DDialog::closed, this, [reply]() {
        reply->abort();
    });
}

void MainWindow::ensureServerRunning(const QString &socketPath)
{
    if (QFileInfo::exists(socketPath)) {
        QLocalSocket probe;
        probe.connectToServer(socketPath);
        if (probe.waitForConnected(500)) {
            launchClient();
            return;
        }
        QFile::remove(socketPath);
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

void MainWindow::applyTerminalColorScheme(DGuiApplicationHelper::ColorType themeType)
{
    if (!m_terminal) {
        return;
    }
    QString scheme = (themeType == DGuiApplicationHelper::DarkType)
        ? QStringLiteral("Theme7")
        : QStringLiteral("Theme10");
    m_terminal->setColorScheme(scheme);
}

void MainWindow::handleOSC52Clipboard(char target, const QString &base64Data)
{
    const int MAX_BASE64_SIZE = 64 * 1024 * 1024;
    if (base64Data.size() > MAX_BASE64_SIZE) {
        qWarning().nospace() << "OSC52: Rejected - data too large ("
                             << base64Data.size() << " bytes, max=" << MAX_BASE64_SIZE << ")";
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    QString text;

    if (!base64Data.isEmpty()) {
        QByteArray decoded = QByteArray::fromBase64(base64Data.toLatin1());
        if (decoded.isEmpty()) {
            qWarning() << "OSC52: Base64 decoding failed";
            return;
        }
        text = QString::fromUtf8(decoded);
        if (text.isEmpty() && !decoded.isEmpty()) {
            qWarning() << "OSC52: Invalid UTF-8 encoding in clipboard data";
            return;
        }
    }

    switch (target) {
    case 'p':
        clipboard->setText(text, QClipboard::Selection);
        break;
    case 's':
        clipboard->setText(text, QClipboard::Selection);
        break;
    case '0':
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
        break;
    case 'c':
    default:
        clipboard->setText(text, QClipboard::Clipboard);
        break;
    }
}
