#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <DGuiApplicationHelper>

class QTermWidget;
class QTimer;
class QNetworkReply;
class QFont;

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void applyTerminalColorScheme(DGuiApplicationHelper::ColorType themeType);
    void handleOSC52Clipboard(char target, const QString &base64Data);

private:
    void initUI();
    void checkHerdrAndStart();
    void ensureServerRunning(const QString &socketPath);
    void launchClient();
    QString findHerdrBinary() const;
    void installHerdr();

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
};

#endif // MAINWINDOW_H
