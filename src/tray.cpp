#include "tray.h"

#include <QMenu>

TrayIcon::TrayIcon(QObject *parent)
    : QSystemTrayIcon(parent)
    , m_menu(new QMenu)
{
    // hicolor scalable icon is installed by the packages; the embedded
    // copy covers running from a build tree
    QIcon icon = QIcon::fromTheme(QStringLiteral("mudi"));
    if (icon.isNull()) {
        icon = QIcon(QStringLiteral(":/mudi.svg"));
    }
    setIcon(icon);
    setContextMenu(m_menu);
    rebuildMenu();

    connect(this, &QSystemTrayIcon::activated, this, [this](ActivationReason reason) {
        if (reason == Trigger || reason == DoubleClick) {
            emit showWindowRequested();
        }
    });
}

// Menus cannot be updated in place; rebuild on every poll (cheap, every
// 4s, and the tray is hidden most of the time anyway).
void TrayIcon::refreshAgents(const QVector<AgentMonitor::AgentSummary> &agents)
{
    m_agents = agents;
    rebuildMenu();

    int running = 0, blocked = 0, done = 0, idle = 0;
    for (const AgentMonitor::AgentSummary &a : m_agents) {
        if (a.status == QLatin1String("running")) ++running;
        else if (a.status == QLatin1String("blocked")) ++blocked;
        else if (a.status == QLatin1String("done")) ++done;
        else if (a.status == QLatin1String("idle")) ++idle;
    }
    QStringList parts;
    if (running > 0) parts << tr("%n running", nullptr, running);
    if (blocked > 0) parts << tr("%n blocked", nullptr, blocked);
    if (done > 0) parts << tr("%n finished", nullptr, done);
    if (idle > 0) parts << tr("%n idle", nullptr, idle);
    setToolTip(parts.isEmpty()
        ? tr("MuDi — no agents")
        : tr("MuDi — %1").arg(parts.join(QStringLiteral(", "))));
}

void TrayIcon::rebuildMenu()
{
    m_menu->clear();

    QAction *show = m_menu->addAction(tr("Show MuDi"));
    connect(show, &QAction::triggered, this, &TrayIcon::showWindowRequested);
    m_menu->addSeparator();

    // one direct entry per blocked agent: the "needs you now" shortcut
    for (const AgentMonitor::AgentSummary &a : m_agents) {
        if (a.status != QLatin1String("blocked")) {
            continue;
        }
        QAction *jump = m_menu->addAction(a.label());
        connect(jump, &QAction::triggered, this, [this, paneId = a.paneId]() {
            emit focusPaneRequested(paneId);
        });
    }
    if (!m_agents.isEmpty()) {
        m_menu->addSeparator();
    }

    QAction *quit = m_menu->addAction(tr("Quit"));
    // routed through AppCore's confirmation dialog — quitting must be a
    // conscious choice when a herdr server is running in the background
    connect(quit, &QAction::triggered, this, &TrayIcon::quitRequested);
}
