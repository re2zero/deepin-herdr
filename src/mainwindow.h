#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>

class QTermWidget;
class QTimer;

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
private:
    void initUI();
    void ensureServerRunning(const QString &socketPath);
    void launchClient();
    QString findHerdrBinary() const;

    QTermWidget *m_terminal;
    QTimer *m_launchTimer;
    int m_launchAttempts;
};

#endif // MAINWINDOW_H
