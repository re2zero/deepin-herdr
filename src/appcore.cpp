#include "appcore.h"
#include "settingsdialog.h"
#include "updatebanner.h"
#include "agentmonitor.h"
#include "platform.h"
#include "version.h"

#include <algorithm>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QActionGroup>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QLocalSocket>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <qtermwidget.h>

#if HAVE_DTK
#include <DGuiApplicationHelper>
DGUI_USE_NAMESPACE
#endif

static const char *HERDR_BINARY = "herdr";

// herdr upstream publishes single binaries per platform on GitHub releases
static const char *HERDR_API_PATH = "repos/herdrdev/herdr/releases/latest";
static const char *HERDR_ASSET_PREFIX = "herdr";
// the app itself
static const char *APP_API_PATH = "repos/re2zero/mudi/releases/latest";
static const char *APP_ASSET_PREFIX = "mudi";

namespace {
constexpr int BANNER_HERDR = 0;
constexpr int BANNER_APP = 1;
constexpr int BANNER_CONNECTING_SERVER = 2;
constexpr int BANNER_CONNECT_SERVER = 3;
constexpr int BANNER_RETRY_SERVER = 4;
constexpr int AUTO_CHECK_DELAY_MS = 4000;
// server restore can restart every pane's agent process; 13 workspaces
// easily take over a minute, so the launch window is generous
constexpr int LAUNCH_MAX_ATTEMPTS = 120;      // x 500ms = 60s
constexpr int SERVER_WATCH_INTERVAL_MS = 2000;
const char *CLIENT_SOCKET_NAME = "herdr-client.sock";

QString serverSocketPath()
{
    // GenericConfigLocation honours XDG_CONFIG_HOME (matches herdr's own
    // ~/.config/herdr layout)
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + "/herdr/" + CLIENT_SOCKET_NAME;
}
}

AppCore::AppCore(QObject *parent)
    : QObject(parent)
    , m_launchTimer(new QTimer(this))
    , m_launchAttempts(0)
    , m_cursorShape(0)
{
}

AppCore::~AppCore() = default;

void AppCore::setTranslucencyHandler(std::function<void(bool)> handler)
{
    m_translucencyHandler = std::move(handler);
}

void AppCore::init()
{
    // Launch retry loop. The 500ms single-shot timer re-probes the server
    // socket until the (re)started server finishes restoring workspaces.
    m_launchTimer->setSingleShot(true);
    m_launchTimer->setInterval(500);
    connect(m_launchTimer, &QTimer::timeout, this, [this]() {
        ensureServerRunning(serverSocketPath());
    });

    initContent();
    initThemeMenu();
    initNotificationActivation();
    restoreTerminalSettings();
    initUpdateSystem();
    checkHerdrAndStart();
}

QAction *AppCore::settingsAction() const
{
    return m_settingsAction;
}

void AppCore::initContent()
{
    m_container = new QWidget;
    auto *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_banner = new UpdateBanner(m_container);
    layout->addWidget(m_banner);

    m_terminal = new QTermWidget(0, m_container);
    layout->addWidget(m_terminal);
    m_originalFont = m_terminal->getTerminalFont();

    // Initialize with default color scheme
    m_terminal->setColorScheme(QStringLiteral("Theme7"));

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

    connect(m_terminal, &QTermWidget::finished, this, &AppCore::closeRequested);
}

// The theme menu is shell-independent; shells attach it to the DTK
// titlebar menu or the menu bar.
void AppCore::initThemeMenu()
{
    m_themeMenu = new QMenu(Platform::useDtk()
        ? qApp->translate("TitleBarMenu", "Theme") : tr("Theme"), m_container);

    m_lightThemeAction = m_themeMenu->addAction(tr("Light"));
    m_darkThemeAction = m_themeMenu->addAction(tr("Dark"));
    m_autoThemeAction = m_themeMenu->addAction(tr("Follow System"));
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

    // Settings entry (same level as Theme)
    m_settingsAction = new QAction(tr("Settings"), m_themeMenu);
    connect(m_settingsAction, &QAction::triggered, this, &AppCore::openSettings);
}

void AppCore::initUpdateSystem()
{
    m_herdrUpdater = new ReleaseUpdater(HERDR_API_PATH, HERDR_ASSET_PREFIX,
                                        HERDR_BINARY, this);
    m_appUpdater = new ReleaseUpdater(APP_API_PATH, APP_ASSET_PREFIX,
                                      "mudi", this);

    // Mirror preferences apply to both updaters
    QSettings settings = Platform::appSettings();
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
        QSettings store = Platform::appSettings();
        if (store.value("skippedHerdrVersion").toString() == release.version) {
            return;
        }
        queueBanner(BANNER_HERDR,
                    tr("herdr %1 is available").arg(release.version),
                    tr("Update"), release);
    });

    connect(m_appUpdater, &ReleaseUpdater::checkFinished, this,
            [this](bool ok, const ReleaseUpdater::Release &release, const QString &) {
        if (!ok) {
            return;
        }
        if (ReleaseUpdater::compareVersions(release.version, APP_VERSION) <= 0) {
            return;
        }
        QSettings store = Platform::appSettings();
        if (store.value("skippedAppVersion").toString() == release.version) {
            return;
        }
        queueBanner(BANNER_APP,
                    tr("MuDi %1 is available").arg(release.version),
                    tr("View"), release);
    });

    // Banner actions
    connect(m_banner, &UpdateBanner::actionTriggered, this, [this]() {
        if (m_currentBanner.kind == BANNER_HERDR) {
            m_banner->showProgress(0);
            m_herdrUpdater->downloadAndInstall(m_currentBanner.release);
        } else if (m_currentBanner.kind == BANNER_CONNECT_SERVER) {
            // run the client inside the fallback shell's session
            m_terminal->sendText(QStringLiteral("herdr client\n"));
            m_serverWatchTimer->stop();
            m_banner->hide();
            showNextBanner();
        } else if (m_currentBanner.kind == BANNER_RETRY_SERVER) {
            m_banner->hide();
            m_launchAttempts = 0;
            m_staleSocketRemoved = false;
            checkHerdrAndStart();
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
            m_banner->showDone(tr("herdr updated. Restart the herdr server to apply."));
        } else {
            m_banner->showError(tr("herdr update failed: %1").arg(error));
        }
    });

    connect(m_banner, &UpdateBanner::skipTriggered, this, [this]() {
        QSettings store = Platform::appSettings();
        const QString key = (m_currentBanner.kind == BANNER_HERDR)
            ? "skippedHerdrVersion" : "skippedAppVersion";
        store.setValue(key, m_currentBanner.release.version);
        showNextBanner();
    });

    connect(m_banner, &UpdateBanner::dismissed, this,
            &AppCore::showNextBanner);

    // Deferred background update check
    QTimer::singleShot(AUTO_CHECK_DELAY_MS, this, &AppCore::autoCheckUpdates);
}

void AppCore::autoCheckUpdates()
{
    QSettings settings = Platform::appSettings();
    if (!settings.value("autoCheckUpdates", true).toBool()) {
        return;
    }
    m_herdrUpdater->checkLatest();
    m_appUpdater->checkLatest();
}

void AppCore::queueBanner(int kind, const QString &title, const QString &actionText,
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

// Server lifecycle notices replace each other in place instead of
// queueing behind update banners.
void AppCore::replaceServerBanner(int kind, const QString &title, const QString &actionText)
{
    m_bannerQueue.erase(std::remove_if(m_bannerQueue.begin(), m_bannerQueue.end(),
                            [](const BannerRequest &r) {
                                return r.kind == BANNER_CONNECTING_SERVER
                                    || r.kind == BANNER_CONNECT_SERVER
                                    || r.kind == BANNER_RETRY_SERVER;
                            }),
                        m_bannerQueue.end());
    m_currentBanner.kind = kind;
    m_currentBanner.title = title;
    m_currentBanner.actionText = actionText;
    m_currentBanner.release = {};
    m_banner->showNotice(title, actionText);
}

void AppCore::showNextBanner()
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

void AppCore::checkHerdrAndStart()
{
    if (!findHerdrBinary().isEmpty()) {
        detectHerdrVersion();
        startAgentMonitor();
        ensureServerRunning(serverSocketPath());
        return;
    }

    runFirstRunInstall();
}

// First run: fetch the latest release from the API (version + digest) and
// install it atomically through ReleaseUpdater.
void AppCore::runFirstRunInstall()
{
    QWidget *parentWindow = m_container->window();
    auto *progress = new Platform::ProgressDialog(parentWindow);
    progress->setMessage(tr("herdr terminal workspace manager is required.\n"
                            "Fetching release information…"));
    progress->show();

    auto cancelled = QSharedPointer<bool>::create(false);
    connect(progress, &Platform::ProgressDialog::cancelled, this, [this, cancelled]() {
        *cancelled = true;
        m_herdrUpdater->cancelDownload();
        emit closeRequested();
    });

    // One check, one install, one outcome — these fire once per first run.
    connect(m_herdrUpdater, &ReleaseUpdater::checkFinished, this,
            [this, progress, cancelled](bool ok, const ReleaseUpdater::Release &release, const QString &error) {
        if (*cancelled) {
            return;
        }
        if (!ok) {
            progress->close();
            progress->deleteLater();
            Platform::showErrorDialog(m_container->window(), tr("Download Failed"),
                tr("Failed to fetch herdr release info: %1\n"
                   "Please install herdr manually to ~/.local/bin/herdr").arg(error));
            emit closeRequested();
            return;
        }
        progress->setMessage(tr("Downloading v%1…").arg(release.version));
        m_herdrUpdater->downloadAndInstall(release);
    }, Qt::SingleShotConnection);

    connect(m_herdrUpdater, &ReleaseUpdater::installProgress, this,
            [progress](int percent) {
        progress->setProgress(percent);
    }, Qt::SingleShotConnection);

    connect(m_herdrUpdater, &ReleaseUpdater::installFinished, this,
            [this, progress, cancelled](bool ok, const QString &error) {
        if (*cancelled) {
            return;
        }
        progress->close();
        progress->deleteLater();
        if (!ok) {
            Platform::showErrorDialog(m_container->window(), tr("Download Failed"),
                tr("Failed to download herdr: %1\n"
                   "Please install herdr manually to ~/.local/bin/herdr").arg(error));
            emit closeRequested();
            return;
        }
        checkHerdrAndStart();
    }, Qt::SingleShotConnection);

    m_herdrUpdater->checkLatest();
}

void AppCore::detectHerdrVersion()
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

// Desktop notifications for herdr agent state transitions (blocked /
// done / optionally idle). Cheap JSON poll through the herdr CLI.
void AppCore::startAgentMonitor()
{
    if (!m_agentMonitor) {
        m_agentMonitor = new AgentMonitor(findHerdrBinary(), this);
        connect(m_agentMonitor, &AgentMonitor::agentAttention,
                this, &AppCore::onAgentAttention);
    }
    m_agentMonitor->start();
}

void AppCore::onAgentAttention(const QString &paneId, const QString &agent,
                               const QString &title, const QString &cwd, const QString &status)
{
    // the user is already looking at the app
    if (m_container->window()->isActiveWindow()) {
        return;
    }

    QString stateText;
    if (status == QLatin1String("blocked")) {
        stateText = tr("waiting for your input");
    } else if (status == QLatin1String("done")) {
        stateText = tr("task finished");
    } else {
        stateText = tr("idle");
    }

    QString body = title;
    if (!cwd.isEmpty()) {
        body += body.isEmpty() ? cwd : QStringLiteral("\n") + cwd;
    }

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    QDBusInterface notifications("org.freedesktop.Notifications",
                                 "/org/freedesktop/Notifications",
                                 "org.freedesktop.Notifications",
                                 QDBusConnection::sessionBus());
    if (!notifications.isValid()) {
        return;
    }
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("mudi"));
    // "default" fires on notification body click where the daemon
    // supports it; the labelled button covers the rest
    const QStringList actions = {
        QStringLiteral("default"), tr("View"),
    };
    QDBusPendingReply<uint> reply = notifications.asyncCallWithArgumentList("Notify", {
        QVariant(QStringLiteral("MuDi")),
        QVariant::fromValue(static_cast<uint>(0)),
        QVariant(QStringLiteral("mudi")),
        QVariant(tr("%1 is %2").arg(agent, stateText)),
        QVariant(body),
        QVariant(actions),
        hints,
        QVariant::fromValue(-1),
    });
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, paneId]() {
        watcher->deleteLater();
        QDBusPendingReply<uint> reply = *watcher;
        if (reply.isError() || reply.value() == 0) {
            return; // daemon gone or refused: nothing to route back
        }
        m_notificationPanes.insert(reply.value(), paneId);
    });
#endif
}

// Match rules for the notification daemon's activation signals. They go
// on the same session-bus connection the Notify calls went out on: some
// daemons address these signals to the sender's unique name, which a
// private second connection would never receive.
void AppCore::initNotificationActivation()
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString service = QStringLiteral("org.freedesktop.Notifications");
    const QString path = QStringLiteral("/org/freedesktop/Notifications");
    if (!bus.connect(service, path, service, QStringLiteral("ActionInvoked"),
                     this, SLOT(onNotificationAction(uint,QString)))) {
        qWarning() << "notification activation: ActionInvoked match rule failed";
    }
    if (!bus.connect(service, path, service, QStringLiteral("NotificationClosed"),
                     this, SLOT(onNotificationClosed(uint,uint)))) {
        qWarning() << "notification activation: NotificationClosed match rule failed";
    }
#endif
}

void AppCore::onNotificationAction(uint id, const QString &action)
{
    if (action != QLatin1String("default") && action != QLatin1String("view")) {
        return;
    }
    const QString paneId = m_notificationPanes.value(id);
    if (paneId.isEmpty()) {
        return;
    }
    focusPane(paneId);
}

void AppCore::onNotificationClosed(uint id, uint reason)
{
    Q_UNUSED(reason);
    m_notificationPanes.remove(id);
}

// Bring the window forward and point herdr at the pane the agent runs
// in. The focus command is best-effort: a stale pane id or an older
// herdr without `agent focus` still leaves the window raised.
void AppCore::focusPane(const QString &paneId)
{
    QWidget *win = m_container->window();
    if (win->isMinimized()) {
        win->showNormal();
    }
    win->show();
    win->raise();
    win->activateWindow();

    const QString binary = findHerdrBinary();
    if (binary.isEmpty()) {
        return;
    }
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [proc, paneId](int code, QProcess::ExitStatus st) {
        if (st != QProcess::NormalExit || code != 0) {
            qWarning() << "herdr agent focus" << paneId << "failed, code" << code;
        }
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError) {
        proc->deleteLater();
    });
    proc->start(binary, {"agent", "focus", paneId});
}

void AppCore::ensureServerRunning(const QString &socketPath)
{
    if (QFileInfo::exists(socketPath)) {
        QLocalSocket probe;
        probe.connectToServer(socketPath);
        if (probe.waitForConnected(500)) {
            m_launchTimer->stop();
            if (m_fallbackShellStarted) {
                // the server came up while the user was in the fallback shell
                m_serverWatchTimer->stop();
                replaceServerBanner(BANNER_CONNECT_SERVER, tr("herdr server is ready."),
                                    tr("Connect"));
            } else {
                // The probe can succeed synchronously from init(), before
                // the window is even shown; spawning then hands the client
                // a 0x0 pty grid and it exits on the spot. Defer to the
                // event loop, which only starts after show().
                QTimer::singleShot(0, this, &AppCore::launchClient);
            }
            return;
        }
        // a stale socket from a killed server blocks clients; remove once
        if (!m_staleSocketRemoved) {
            QFile::remove(socketPath);
            m_staleSocketRemoved = true;
        }
    }

    if (m_launchAttempts >= LAUNCH_MAX_ATTEMPTS) {
        m_launchTimer->stop();
        if (!m_fallbackShellStarted) {
            startFallbackShell();
        }
        return;
    }

    if (m_launchAttempts == 0) {
        QString binary = findHerdrBinary();
        if (binary.isEmpty()) {
            return;
        }
        QProcess::startDetached(binary, {"server"});
        replaceServerBanner(BANNER_CONNECTING_SERVER, tr("Connecting to herdr server…"));
    }

    m_launchAttempts++;
    m_launchTimer->start();
}

// The server never came up within the launch window: give the user a
// working shell instead of a dead screen, keep probing in the
// background, and offer a one-click connect once the server appears.
void AppCore::startFallbackShell()
{
    m_fallbackShellStarted = true;
    m_terminal->startTerminalTeletype();
    replaceServerBanner(BANNER_RETRY_SERVER,
                        tr("herdr server did not come up — a plain shell is available."),
                        tr("Retry"));
    startServerWatch();
}

void AppCore::startServerWatch()
{
    if (!m_serverWatchTimer) {
        m_serverWatchTimer = new QTimer(this);
        m_serverWatchTimer->setInterval(SERVER_WATCH_INTERVAL_MS);
        connect(m_serverWatchTimer, &QTimer::timeout, this, [this]() {
            QLocalSocket probe;
            probe.connectToServer(serverSocketPath());
            if (probe.waitForConnected(500)) {
                m_serverWatchTimer->stop();
                queueBanner(BANNER_CONNECT_SERVER, tr("herdr server is ready."),
                            tr("Connect"), {});
            }
        });
    }
    m_serverWatchTimer->start();
}

void AppCore::launchClient()
{
    m_launchTimer->stop();
    m_banner->hide();
    showNextBanner();

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
}

QString AppCore::findHerdrBinary() const
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

void AppCore::handleOSC52Clipboard(char target, const QString &base64Data)
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

void AppCore::switchThemeAction(QAction *action)
{
    QString themeKey = "theme";
    QSettings settings = Platform::appSettings();

    if (action == m_lightThemeAction) {
        // Light theme: UI palette and terminal both go light
        Platform::applyUiTheme(Platform::UiTheme::Light);
        m_terminal->setColorScheme(QStringLiteral("Theme10"));
        settings.setValue(themeKey, "Light");
        for (QAction *a : m_themeMenu->actions()) {
            a->setChecked(a == m_lightThemeAction);
        }
        return;
    }

    if (action == m_darkThemeAction) {
        // Dark theme: UI palette and terminal both go dark
        Platform::applyUiTheme(Platform::UiTheme::Dark);
        m_terminal->setColorScheme(QStringLiteral("Theme7"));
        settings.setValue(themeKey, "Dark");
        for (QAction *a : m_themeMenu->actions()) {
            a->setChecked(a == m_darkThemeAction);
        }
        return;
    }

    if (action == m_autoThemeAction) {
        // Follow system: terminal follows the system theme. Without DTK
        // there is no system theme signal; light is the neutral default.
        bool dark = false;
#if HAVE_DTK
        if (Platform::useDtk()) {
            dark = (DGuiApplicationHelper::instance()->themeType()
                    == DGuiApplicationHelper::DarkType);
        }
#endif
        m_terminal->setColorScheme(dark ? QStringLiteral("Theme7") : QStringLiteral("Theme10"));
        Platform::applyUiTheme(Platform::UiTheme::Auto);
        settings.setValue(themeKey, "Auto");
        for (QAction *a : m_themeMenu->actions()) {
            a->setChecked(a == m_autoThemeAction);
        }
        return;
    }

    // Extended theme: built-in terminal color scheme (terminal only —
    // the UI palette keeps the Light/Dark/Auto choice)
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

void AppCore::applyThemeByKey(const QString &key)
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

void AppCore::restoreTerminalSettings()
{
    QSettings settings = Platform::appSettings();

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
        if (m_translucencyHandler) {
            m_translucencyHandler(true);
        }
    }

    // m_originalFont serves as the Ctrl+0 baseline — now reflects persisted size/family
    m_originalFont = m_terminal->getTerminalFont();

    // Apply persisted theme
    applyThemeByKey(settings.value("theme", "Auto").toString());
}

void AppCore::openSettings()
{
    auto *content = new SettingsDialog(m_terminal, this);
    connect(content, &SettingsDialog::settingsChanged, this, &AppCore::onSettingsChanged);
    connect(content, &SettingsDialog::themeSelected, this, &AppCore::onThemeSelected);
    connect(content, &SettingsDialog::transparencyChanged, this, &AppCore::onTransparencyChanged);
    connect(content, &SettingsDialog::autoCopyChanged, this, &AppCore::onAutoCopyChanged);
    Platform::showContentDialog(content, tr("Settings"), m_container->window(), [content] {
        content->persist();
    });
}

void AppCore::onSettingsChanged(const SettingsDialog::TerminalSettings &settings)
{
    QSettings store = Platform::appSettings();
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

void AppCore::onThemeSelected(const QString &key)
{
    applyThemeByKey(key);
}

void AppCore::onTransparencyChanged(qreal opacity)
{
    m_terminal->setTerminalOpacity(opacity);
    if (m_translucencyHandler) {
        m_translucencyHandler(opacity < 1.0);
    }
    QSettings store = Platform::appSettings();
    store.setValue("terminalOpacity", opacity);
}

void AppCore::onAutoCopyChanged(bool enabled)
{
    m_autoCopyOnSelect = enabled;
}
