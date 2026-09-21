#ifndef TRAY_H
#define TRAY_H

#include <QSystemTrayIcon>
#include <QVector>

#include "agentmonitor.h"

class QMenu;

// System tray presence: the watch keeps running with the window closed.
// Tooltip = agent summary ("MuDi — 2 running, 1 blocked"), menu = show
// main window / jump to a blocked agent (AppCore focus route) / quit.
// Clicking the tray icon restores the window.
class TrayIcon : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = nullptr);

    void refreshAgents(const QVector<AgentMonitor::AgentSummary> &agents);

signals:
    void showWindowRequested();
    void focusPaneRequested(const QString &paneId);
    void quitRequested();

private:
    void rebuildMenu();

    QVector<AgentMonitor::AgentSummary> m_agents;
    QMenu *m_menu;
};

#endif // TRAY_H
