#ifndef APPCORE_H
#define APPCORE_H

#include <QFont>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <functional>

#include "updater.h"
// full definition needed: slot signatures use SettingsDialog::TerminalSettings
#include "settingsdialog.h"

// Theme name mapping
static constexpr const char *THEME_ONE_NAME   = "Elementary";
static constexpr const char *THEME_TWO_NAME   = "Empathy";
static constexpr const char *THEME_THREE_NAME = "Tomorrow night blue";
static constexpr const char *THEME_FOUR_NAME  = "Bim";
static constexpr const char *THEME_FIVE_NAME  = "Freya";
static constexpr const char *THEME_SIX_NAME   = "Hybrid";
static constexpr const char *THEME_SEVEN_NAME = "Ocean dark";
static constexpr const char *THEME_EIGHT_NAME = "Deepin";
static constexpr const char *THEME_NINE_NAME  = "Ura";
static constexpr const char *THEME_TEN_NAME   = "One light";

class AgentMonitor;
class QMenu;
class QAction;
class QTermWidget;
class UpdateBanner;

// Application logic core, window-shell independent: owns the terminal
// container (banner + terminal), the theme menu actions, updaters and
// the agent monitor. Window shells (DTK / generic Qt) host the
// container and menu and register the translucency hook before init().
class AppCore : public QObject {
    Q_OBJECT
public:
    explicit AppCore(QObject *parent = nullptr);
    ~AppCore() override;

    // builds the UI content and applies persisted settings
    void init();

    QWidget *container() const { return m_container; }
    QMenu *themeMenu() const { return m_themeMenu; }
    QAction *settingsAction() const;

    ReleaseUpdater *herdrUpdater() const { return m_herdrUpdater; }
    ReleaseUpdater *appUpdater() const { return m_appUpdater; }
    QString herdrVersion() const { return m_herdrVersion; }

    // shell hook: toggling terminal opacity below 1 requests a
    // translucent window (DTK: setTranslucentBackground, generic:
    // WA_TranslucentBackground); must be registered before init()
    void setTranslucencyHandler(std::function<void(bool)> handler);

signals:
    void closeRequested();

private slots:
    void handleOSC52Clipboard(char target, const QString &base64Data);
    void switchThemeAction(QAction *action);
    void openSettings();
    void onSettingsChanged(const SettingsDialog::TerminalSettings &settings);
    void onThemeSelected(const QString &key);
    void onTransparencyChanged(qreal opacity);
    void onAutoCopyChanged(bool enabled);
    void onAgentAttention(const QString &paneId, const QString &agent,
                          const QString &title, const QString &cwd, const QString &status);
    void onNotificationAction(uint id, const QString &action);
    void onNotificationClosed(uint id, uint reason);

private:
    void initContent();
    void initThemeMenu();
    void initUpdateSystem();
    void initNotificationActivation();
    void focusPane(const QString &paneId);
    void checkHerdrAndStart();
    void runFirstRunInstall();
    void ensureServerRunning(const QString &socketPath);
    void startFallbackShell();
    void startServerWatch();
    void launchClient();
    QString findHerdrBinary() const;
    void detectHerdrVersion();
    void startAgentMonitor();
    void autoCheckUpdates();
    void queueBanner(int kind, const QString &title, const QString &actionText,
                     const ReleaseUpdater::Release &release);
    void replaceServerBanner(int kind, const QString &title,
                             const QString &actionText = QString());
    void showNextBanner();
    void applyThemeByKey(const QString &key);
    void restoreTerminalSettings();

    struct BannerRequest {
        int kind = 0; // 0 = herdr update, 1 = app update
        QString title;
        QString actionText;
        ReleaseUpdater::Release release;
    };

    QWidget *m_container = nullptr;
    QTermWidget *m_terminal = nullptr;
    QTimer *m_launchTimer;
    QTimer *m_serverWatchTimer = nullptr;
    int m_launchAttempts;
    bool m_staleSocketRemoved = false;
    bool m_fallbackShellStarted = false;
    QFont m_originalFont;
    QMenu *m_themeMenu = nullptr;
    QAction *m_lightThemeAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QAction *m_autoThemeAction = nullptr;
    QAction *m_settingsAction = nullptr;
    int m_cursorShape;
    bool m_autoCopyOnSelect = true;
    std::function<void(bool)> m_translucencyHandler;

    ReleaseUpdater *m_herdrUpdater = nullptr;
    ReleaseUpdater *m_appUpdater = nullptr;
    UpdateBanner *m_banner = nullptr;
    AgentMonitor *m_agentMonitor = nullptr;
    QString m_herdrVersion;
    // in-flight desktop notifications: notification id -> pane to focus
    QHash<uint, QString> m_notificationPanes;
    QVector<BannerRequest> m_bannerQueue;
    BannerRequest m_currentBanner;
};

#endif // APPCORE_H
