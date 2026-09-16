#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <DGuiApplicationHelper>

#include <QVector>

#include "updater.h"
// full definition needed: slot signatures use SettingsDialog::TerminalSettings
#include "settingsdialog.h"

class QTermWidget;
class QTimer;
class QMenu;
class QAction;
class QFont;
class UpdateBanner;

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

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    ReleaseUpdater *herdrUpdater() const { return m_herdrUpdater; }
    ReleaseUpdater *appUpdater() const { return m_appUpdater; }
    QString herdrVersion() const { return m_herdrVersion; }

private slots:
    void handleOSC52Clipboard(char target, const QString &base64Data);
    void switchThemeAction(QAction *action);
    void openSettings();
    void onSettingsChanged(const SettingsDialog::TerminalSettings &settings);
    void onThemeSelected(const QString &key);
    void onTransparencyChanged(qreal opacity);
    void onAutoCopyChanged(bool enabled);

private:
    void initUI();
    void initUpdateSystem();
    void checkHerdrAndStart();
    void runFirstRunInstall();
    void ensureServerRunning(const QString &socketPath);
    void launchClient();
    QString findHerdrBinary() const;
    void detectHerdrVersion();
    void autoCheckUpdates();
    void queueBanner(int kind, const QString &title, const QString &actionText,
                     const ReleaseUpdater::Release &release);
    void showNextBanner();
    void applyThemeByKey(const QString &key);
    void restoreTerminalSettings();

    struct BannerRequest {
        int kind = 0; // 0 = herdr update, 1 = app update
        QString title;
        QString actionText;
        ReleaseUpdater::Release release;
    };

    QTermWidget *m_terminal;
    QTimer *m_launchTimer;
    int m_launchAttempts;
    QFont m_originalFont;
    QMenu *m_themeMenu;
    QAction *m_lightThemeAction;
    QAction *m_darkThemeAction;
    QAction *m_autoThemeAction;
    int m_cursorShape;
    bool m_autoCopyOnSelect = true;

    ReleaseUpdater *m_herdrUpdater = nullptr;
    ReleaseUpdater *m_appUpdater = nullptr;
    UpdateBanner *m_banner = nullptr;
    QString m_herdrVersion;
    QVector<BannerRequest> m_bannerQueue;
    BannerRequest m_currentBanner;
};

#endif // MAINWINDOW_H
