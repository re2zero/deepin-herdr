#include "mainwindow.h"
#include "settingsdialog.h"
#include "updatebanner.h"
#include "version.h"

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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QShortcut>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QToolButton>
#include <QActionGroup>
#include <QSettings>
#include <QTimer>

#include <QLocalSocket>
#include <QClipboard>

#include <qtermwidget.h>
#include <DGuiApplicationHelper>

static const char *HERDR_BINARY = "herdr";
static const char *HERDR_CONFIG_DIR = "herdr";

// herdr upstream publishes single binaries per platform on GitHub releases
static const char *HERDR_API_PATH = "repos/herdrdev/herdr/releases/latest";
static const char *HERDR_ASSET_PREFIX = "herdr";
// the app itself
static const char *APP_API_PATH = "repos/re2zero/deepin-herdr/releases/latest";
static const char *APP_ASSET_PREFIX = "deepin-herdr";

namespace {
constexpr int BANNER_HERDR = 0;
constexpr int BANNER_APP = 1;
constexpr int AUTO_CHECK_DELAY_MS = 4000;
}

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_terminal(nullptr)
    , m_launchTimer(new QTimer(this))
    , m_launchAttempts(0)
    , m_themeMenu(nullptr)
    , m_cursorShape(0)
{
    resize(1200, 800);
    initUI();

    titlebar()->setSwitchThemeMenuVisible(false);

    // Get DTK titlebar menu and add theme options
    QMenu *dtkMenu = titlebar()->menu();
    if (dtkMenu) {
        // Add theme separator and options to DTK menu
        dtkMenu->addSeparator();

        m_themeMenu = new QMenu(qApp->translate("TitleBarMenu", "Theme"));
        m_lightThemeAction = m_themeMenu->addAction(QObject::tr("Light"));
        m_darkThemeAction = m_themeMenu->addAction(QObject::tr("Dark"));
        m_autoThemeAction = m_themeMenu->addAction(QObject::tr("Follow System"));
        m_themeMenu->addSeparator();

        // Add built-in terminal color schemes with real names
        QStringList schemes = QTermWidget::availableColorSchemes();
        for (const QString &scheme : schemes) {
            // Skip basic theme names to avoid duplication
            if (scheme == "Light" || scheme == "Dark" || scheme == "System") {
                continue;
            }

            // Resolve Theme1-10 to real names
            QString displayName = scheme;
            if (scheme == "Theme1") displayName = THEME_ONE_NAME;
            else if (scheme == "Theme2") displayName = THEME_TWO_NAME;
            else if (scheme == "Theme3") displayName = THEME_THREE_NAME;
            else if (scheme == "Theme4") displayName = THEME_FOUR_NAME;
            else if (scheme == "Theme5") displayName = THEME_FIVE_NAME;
            else if (scheme == "Theme6") displayName = THEME_SIX_NAME;
            else if (scheme == "Theme7") displayName = THEME_SEVEN_NAME;
            else if (scheme == "Theme8") displayName = THEME_EIGHT_NAME;
            else if (scheme == "Theme9") displayName = THEME_NINE_NAME;
            else if (scheme == "Theme10") displayName = THEME_TEN_NAME;

            QAction *action = m_themeMenu->addAction(displayName);
            action->setData(scheme); // Store original scheme name
            action->setCheckable(true);
        }

        // Set checkable for basic theme actions
        m_lightThemeAction->setCheckable(true);
        m_darkThemeAction->setCheckable(true);
        m_autoThemeAction->setCheckable(true);

        // Create action group
        QActionGroup *themeGroup = new QActionGroup(m_themeMenu);
        themeGroup->addAction(m_lightThemeAction);
        themeGroup->addAction(m_darkThemeAction);
        themeGroup->addAction(m_autoThemeAction);

        for (QAction *action : m_themeMenu->actions()) {
            if (action != m_lightThemeAction && action != m_darkThemeAction && action != m_autoThemeAction) {
                themeGroup->addAction(action);
            }
        }

        // Connect theme actions
        connect(m_lightThemeAction, &QAction::triggered, this, [this]() {
            switchThemeAction(m_lightThemeAction);
        });
        connect(m_darkThemeAction, &QAction::triggered, this, [this]() {
            switchThemeAction(m_darkThemeAction);
        });
        connect(m_autoThemeAction, &QAction::triggered, this, [this]() {
            switchThemeAction(m_autoThemeAction);
        });

        for (QAction *action : m_themeMenu->actions()) {
            if (action != m_lightThemeAction && action != m_darkThemeAction && action != m_autoThemeAction) {
                connect(action, &QAction::triggered, this, [this, action]() {
                    switchThemeAction(action);
                });
            }
        }

        // Add theme menu to DTK menu
        dtkMenu->addMenu(m_themeMenu);

        // Settings entry (same level as Theme)
        QAction *settingsAction = dtkMenu->addAction(QObject::tr("Settings"));
        connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    }

    // Load saved theme on startup
    QSettings settings("deepin-herdr", "deepin-herdr");
    QString savedTheme = settings.value("theme", "Auto").toString();
    applyThemeByKey(savedTheme);

    // Restore terminal font/size/cursor from persisted settings
    restoreTerminalSettings();

    initUpdateSystem();

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

    m_banner = new UpdateBanner(centralWidget);
    layout->addWidget(m_banner);

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

    // Initialize with default color scheme
    m_terminal->setColorScheme(QStringLiteral("Theme7"));

    connect(m_terminal, &QTermWidget::urlActivated, this,
        [](const QUrl &url, bool fromContextMenu) {
            Q_UNUSED(fromContextMenu);
            QDesktopServices::openUrl(url);
        });

    // Auto-copy on QTermWidget's native Shift+drag selection.
    // Gated by the "copy on select" setting.
    connect(m_terminal, &QTermWidget::copyAvailable,
            m_terminal, [this](bool available) {
        if (available && m_autoCopyOnSelect) {
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

void MainWindow::initUpdateSystem()
{
    m_herdrUpdater = new ReleaseUpdater(HERDR_API_PATH, HERDR_ASSET_PREFIX,
                                        HERDR_BINARY, this);
    m_appUpdater = new ReleaseUpdater(APP_API_PATH, APP_ASSET_PREFIX,
                                      "deepin-herdr", this);

    // Mirror preferences apply to both updaters
    QSettings settings("deepin-herdr", "deepin-herdr");
    const QString mode = settings.value("mirrorMode", "auto").toString();
    ReleaseUpdater::MirrorMode m = ReleaseUpdater::MirrorMode::Auto;
    if (mode == "direct") m = ReleaseUpdater::MirrorMode::DirectFirst;
    else if (mode == "mirror") m = ReleaseUpdater::MirrorMode::MirrorFirst;
    for (ReleaseUpdater *u : {m_herdrUpdater, m_appUpdater}) {
        u->setMirrorMode(m);
        u->setCustomMirrorPrefix(settings.value("customMirrorUrl").toString());
    }

    // Update-available handling for background checks. The settings dialog
    // triggers the same checks; these handlers drive the banner.
    connect(m_herdrUpdater, &ReleaseUpdater::checkFinished, this,
            [this](bool ok, const ReleaseUpdater::Release &release, const QString &) {
        if (!ok || m_herdrUpdater->isDownloading()) {
            return;
        }
        if (m_herdrVersion.isEmpty()
            || ReleaseUpdater::compareVersions(release.version, m_herdrVersion) <= 0) {
            return;
        }
        QSettings store("deepin-herdr", "deepin-herdr");
        if (store.value("skippedHerdrVersion").toString() == release.version) {
            return;
        }
        queueBanner(BANNER_HERDR,
                    QObject::tr("herdr %1 is available").arg(release.version),
                    QObject::tr("Update"), release);
    });

    connect(m_appUpdater, &ReleaseUpdater::checkFinished, this,
            [this](bool ok, const ReleaseUpdater::Release &release, const QString &) {
        if (!ok) {
            return;
        }
        if (ReleaseUpdater::compareVersions(release.version, APP_VERSION) <= 0) {
            return;
        }
        QSettings store("deepin-herdr", "deepin-herdr");
        if (store.value("skippedAppVersion").toString() == release.version) {
            return;
        }
        queueBanner(BANNER_APP,
                    QObject::tr("deepin-herdr %1 is available").arg(release.version),
                    QObject::tr("View"), release);
    });

    // Banner actions
    connect(m_banner, &UpdateBanner::actionTriggered, this, [this]() {
        if (m_currentBanner.kind == BANNER_HERDR) {
            m_banner->showProgress(0);
            m_herdrUpdater->downloadAndInstall(m_currentBanner.release);
        } else {
            QDesktopServices::openUrl(QUrl(m_appUpdater->releasesPageUrl()));
            m_banner->hide();
            showNextBanner();
        }
    });

    connect(m_herdrUpdater, &ReleaseUpdater::installProgress, this, [this](int percent) {
        if (m_banner->isVisible() && m_currentBanner.kind == BANNER_HERDR) {
            m_banner->showProgress(percent);
        }
    });

    connect(m_herdrUpdater, &ReleaseUpdater::installFinished, this,
            [this](bool ok, const QString &error) {
        if (!m_banner->isVisible() || m_currentBanner.kind != BANNER_HERDR) {
            return; // install was started elsewhere (first run / settings page)
        }
        if (ok) {
            m_banner->showDone(QObject::tr("herdr updated. Restart the herdr server to apply."));
        } else {
            m_banner->showError(QObject::tr("herdr update failed: %1").arg(error));
        }
    });

    connect(m_banner, &UpdateBanner::skipTriggered, this, [this]() {
        QSettings store("deepin-herdr", "deepin-herdr");
        const QString key = (m_currentBanner.kind == BANNER_HERDR)
            ? "skippedHerdrVersion" : "skippedAppVersion";
        store.setValue(key, m_currentBanner.release.version);
        showNextBanner();
    });

    connect(m_banner, &UpdateBanner::dismissed, this,
            &MainWindow::showNextBanner);

    // Deferred background update check
    QTimer::singleShot(AUTO_CHECK_DELAY_MS, this, &MainWindow::autoCheckUpdates);
}

void MainWindow::autoCheckUpdates()
{
    QSettings settings("deepin-herdr", "deepin-herdr");
    if (!settings.value("autoCheckUpdates", true).toBool()) {
        return;
    }
    m_herdrUpdater->checkLatest();
    m_appUpdater->checkLatest();
}

void MainWindow::queueBanner(int kind, const QString &title, const QString &actionText,
                             const ReleaseUpdater::Release &release)
{
    for (const BannerRequest &r : m_bannerQueue) {
        if (r.kind == kind && r.release.version == release.version) {
            return;
        }
    }
    if (m_banner->isVisible() && m_currentBanner.kind == kind
            && m_currentBanner.release.version == release.version) {
        return;
    }

    BannerRequest request;
    request.kind = kind;
    request.title = title;
    request.actionText = actionText;
    request.release = release;
    m_bannerQueue.append(request);

    if (!m_banner->isVisible()) {
        showNextBanner();
    }
}

void MainWindow::showNextBanner()
{
    if (m_bannerQueue.isEmpty()) {
        return;
    }
    // An in-flight install owns the banner; keep the request queued.
    if (m_bannerQueue.first().kind == BANNER_HERDR && m_herdrUpdater->isDownloading()) {
        return;
    }
    m_currentBanner = m_bannerQueue.takeFirst();
    m_banner->showUpdate(m_currentBanner.title, m_currentBanner.actionText);
}

void MainWindow::checkHerdrAndStart()
{
    if (!findHerdrBinary().isEmpty()) {
        detectHerdrVersion();
        QString socketPath = QDir::homePath() + "/.config/" + HERDR_CONFIG_DIR
            + "/herdr-client.sock";
        ensureServerRunning(socketPath);
        return;
    }

    runFirstRunInstall();
}

// First run: fetch the latest release from the API (version + digest) and
// install it atomically through ReleaseUpdater.
void MainWindow::runFirstRunInstall()
{
    QPointer<DDialog> dlg = new DDialog(this);
    dlg->setTitle(QObject::tr("Installing herdr"));
    dlg->setMessage(QObject::tr("herdr terminal workspace manager is required.\n"
                                "Fetching release information…"));
    dlg->addButton(QObject::tr("Cancel"));
    dlg->setCloseButtonVisible(true);

    QPointer<DProgressBar> progress = new DProgressBar(dlg);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setFixedWidth(300);
    dlg->addContent(progress);

    dlg->show();

    auto cancelled = QSharedPointer<bool>::create(false);
    connect(dlg, &DDialog::closed, this, [this, cancelled]() {
        *cancelled = true;
        m_herdrUpdater->cancelDownload();
        close();
    });

    // One check, one install, one outcome — these fire once per first run.
    connect(m_herdrUpdater, &ReleaseUpdater::checkFinished, this,
            [this, dlg, cancelled](bool ok, const ReleaseUpdater::Release &release, const QString &error) {
        if (!dlg || *cancelled) {
            return;
        }
        if (!ok) {
            dlg->close();
            dlg->deleteLater();
            auto *errDlg = new DDialog(this);
            errDlg->setTitle(QObject::tr("Download Failed"));
            errDlg->setMessage(QObject::tr("Failed to fetch herdr release info: %1\n"
                                         "Please install herdr manually to ~/.local/bin/herdr")
                .arg(error));
            errDlg->addButton(QObject::tr("OK"));
            errDlg->exec();
            errDlg->deleteLater();
            close();
            return;
        }
        dlg->setMessage(QObject::tr("Downloading v%1…").arg(release.version));
        m_herdrUpdater->downloadAndInstall(release);
    }, Qt::SingleShotConnection);

    connect(m_herdrUpdater, &ReleaseUpdater::installProgress, this,
            [progress](int percent) {
        if (progress) {
            progress->setValue(percent);
        }
    }, Qt::SingleShotConnection);

    connect(m_herdrUpdater, &ReleaseUpdater::installFinished, this,
            [this, dlg, cancelled](bool ok, const QString &error) {
        if (*cancelled) {
            return;
        }
        if (dlg) {
            dlg->close();
            dlg->deleteLater();
        }
        if (!ok) {
            auto *errDlg = new DDialog(this);
            errDlg->setTitle(QObject::tr("Download Failed"));
            errDlg->setMessage(QObject::tr("Failed to download herdr: %1\n"
                                         "Please install herdr manually to ~/.local/bin/herdr")
                .arg(error));
            errDlg->addButton(QObject::tr("OK"));
            errDlg->exec();
            errDlg->deleteLater();
            close();
            return;
        }
        checkHerdrAndStart();
    }, Qt::SingleShotConnection);

    m_herdrUpdater->checkLatest();
}

void MainWindow::detectHerdrVersion()
{
    const QString binary = findHerdrBinary();
    if (binary.isEmpty()) {
        return;
    }
    auto *proc = new QProcess(this);
    proc->start(binary, {"--version"});
    connect(proc, &QProcess::finished, this, [this, proc](int, QProcess::ExitStatus) {
        // output like "herdr 0.8.2"
        const QStringList parts =
            QString::fromUtf8(proc->readAllStandardOutput()).simplified().split(' ');
        proc->deleteLater();
        if (parts.size() >= 2) {
            m_herdrVersion = parts.at(1);
        }
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

void MainWindow::switchThemeAction(QAction *action)
{
    QString themeKey = "theme";
    QSettings settings("deepin-herdr", "deepin-herdr");

    if (action == m_lightThemeAction) {
        // Light theme: terminal follows light theme
        m_terminal->setColorScheme(QStringLiteral("Theme10"));
        settings.setValue(themeKey, "Light");
        for (QAction *a : m_themeMenu->actions()) {
            a->setChecked(a == m_lightThemeAction);
        }
        return;
    }

    if (action == m_darkThemeAction) {
        // Dark theme: terminal follows dark theme
        m_terminal->setColorScheme(QStringLiteral("Theme7"));
        settings.setValue(themeKey, "Dark");
        for (QAction *a : m_themeMenu->actions()) {
            a->setChecked(a == m_darkThemeAction);
        }
        return;
    }

    if (action == m_autoThemeAction) {
        // Follow system: terminal follows system theme
        DGuiApplicationHelper::ColorType themeType = DGuiApplicationHelper::instance()->themeType();
        if (themeType == DGuiApplicationHelper::DarkType) {
            m_terminal->setColorScheme(QStringLiteral("Theme7"));
        } else {
            m_terminal->setColorScheme(QStringLiteral("Theme10"));
        }
        settings.setValue(themeKey, "Auto");
        for (QAction *a : m_themeMenu->actions()) {
            a->setChecked(a == m_autoThemeAction);
        }
        return;
    }

    // Extended theme: built-in terminal color scheme
    QString scheme = action->data().toString();
    if (scheme.isEmpty()) {
        scheme = action->text();
    }
    m_terminal->setColorScheme(scheme);
    settings.setValue(themeKey, scheme);
    for (QAction *a : m_themeMenu->actions()) {
        a->setChecked(a == action);
    }
}

void MainWindow::applyThemeByKey(const QString &key)
{
    if (!m_themeMenu) {
        return;
    }
    QAction *target = nullptr;
    if (key == "Light") {
        target = m_lightThemeAction;
    } else if (key == "Dark") {
        target = m_darkThemeAction;
    } else if (key == "Auto") {
        target = m_autoThemeAction;
    } else {
        for (QAction *action : m_themeMenu->actions()) {
            if (action == m_lightThemeAction || action == m_darkThemeAction
                    || action == m_autoThemeAction) {
                continue;
            }
            if (action->data().toString() == key) {
                target = action;
                break;
            }
        }
    }
    if (target) {
        switchThemeAction(target);
    }
}

void MainWindow::restoreTerminalSettings()
{
    QSettings settings("deepin-herdr", "deepin-herdr");

    // Restore font family
    QString fontFamily = settings.value("fontFamily").toString();
    if (!fontFamily.isEmpty()) {
        QFont font = m_terminal->getTerminalFont();
        font.setFamily(fontFamily);
        m_terminal->setTerminalFont(font);
    }

    // Restore font size
    if (settings.contains("fontSize")) {
        int fontSize = settings.value("fontSize").toInt();
        QFont font = m_terminal->getTerminalFont();
        font.setPointSize(fontSize);
        m_terminal->setTerminalFont(font);
    }

    // Restore cursor shape (D-1)
    if (settings.contains("cursorShape")) {
        m_cursorShape = settings.value("cursorShape").toInt();
        if (m_cursorShape < 0 || m_cursorShape > 2) {
            m_cursorShape = 0;
        }
        m_terminal->setKeyboardCursorShape(
            static_cast<QTermWidget::KeyboardCursorShape>(m_cursorShape));
    }

    // Cursor blink
    m_terminal->setBlinkingCursor(settings.value("cursorBlink", false).toBool());

    // Scrollback buffer (-1 = infinite)
    m_terminal->setHistorySize(settings.value("scrollbackLines", 1000).toInt());

    // Copy on selection
    m_autoCopyOnSelect = settings.value("autoCopyOnSelect", true).toBool();

    // Background transparency
    const qreal opacity = settings.value("terminalOpacity", 1.0).toDouble();
    if (opacity < 1.0) {
        m_terminal->setTerminalOpacity(opacity);
        setTranslucentBackground(true);
    }

    // m_originalFont serves as the Ctrl+0 baseline — now reflects persisted size/family
    m_originalFont = m_terminal->getTerminalFont();
}

void MainWindow::openSettings()
{
    auto *dlg = new SettingsDialog(m_terminal, this);
    connect(dlg, &SettingsDialog::settingsChanged, this, &MainWindow::onSettingsChanged);
    connect(dlg, &SettingsDialog::themeSelected, this, &MainWindow::onThemeSelected);
    connect(dlg, &SettingsDialog::transparencyChanged, this, &MainWindow::onTransparencyChanged);
    connect(dlg, &SettingsDialog::autoCopyChanged, this, &MainWindow::onAutoCopyChanged);
    connect(dlg, &DDialog::closed, dlg, &QObject::deleteLater);
    dlg->show();
}

void MainWindow::onSettingsChanged(const SettingsDialog::TerminalSettings &settings)
{
    QSettings store("deepin-herdr", "deepin-herdr");
    store.setValue("fontFamily", settings.fontFamily);
    store.setValue("fontSize", settings.fontSize);
    store.setValue("cursorShape", settings.cursorShape);
    store.setValue("cursorBlink", settings.cursorBlink);
    store.setValue("scrollbackLines", settings.scrollbackLines);
    store.setValue("autoCopyOnSelect", settings.autoCopyOnSelect);

    m_cursorShape = settings.cursorShape;
    m_autoCopyOnSelect = settings.autoCopyOnSelect;

    // Update Ctrl+0 baseline to the persisted font state
    m_originalFont = m_terminal->getTerminalFont();
}

void MainWindow::onThemeSelected(const QString &key)
{
    applyThemeByKey(key);
}

void MainWindow::onTransparencyChanged(qreal opacity)
{
    m_terminal->setTerminalOpacity(opacity);
    setTranslucentBackground(opacity < 1.0);
    QSettings store("deepin-herdr", "deepin-herdr");
    store.setValue("terminalOpacity", opacity);
}

void MainWindow::onAutoCopyChanged(bool enabled)
{
    m_autoCopyOnSelect = enabled;
}
