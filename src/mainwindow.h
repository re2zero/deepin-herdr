#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>

class QTermWidget;
class QTimer;
class QNetworkReply;

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
};

#endif // MAINWINDOW_H
