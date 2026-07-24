#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <DGuiApplicationHelper>

class QTermWidget;
class QTimer;
class QNetworkReply;
class QMenu;
class QAction;
class QFont;
class SettingsDialog;

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

private slots:
    void handleOSC52Clipboard(char target, const QString &base64Data);
    void switchThemeAction(QAction *action);
    void openSettings();
    void onSettingsChanged(const QString &fontFamily, int fontSize, int cursorShape);

private:
    void initUI();
    void checkHerdrAndStart();
    void ensureServerRunning(const QString &socketPath);
    void launchClient();
    QString findHerdrBinary() const;
    void installHerdr();
    void restoreTerminalSettings();

    struct HerdrRelease {
        QString version;
        QString url;
        QString sha256;
    };

    HerdrRelease selectRelease() const;
    void downloadAndInstall(const HerdrRelease &release);

    QTermWidget *m_terminal;
    QTimer *m_launchTimer;
    int m_launchAttempts;
    QFont m_originalFont;
    QMenu *m_themeMenu;
    QAction *m_lightThemeAction;
    QAction *m_darkThemeAction;
    QAction *m_autoThemeAction;
    int m_cursorShape;
};

#endif // MAINWINDOW_H
