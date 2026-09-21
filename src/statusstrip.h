#ifndef STATUSSTRIP_H
#define STATUSSTRIP_H

#include <QFrame>
#include <QVector>

#include "agentmonitor.h"

class QHBoxLayout;
class QLabel;

// One-line agent status strip between the update banner and the
// terminal (M6c): a colored capsule per agent — red for blocked ones —
// refreshed from the monitor's full snapshot. Clicking a capsule asks
// for that pane to be focused; the in-window arm of the shepherd loop.
class AgentStatusStrip : public QFrame {
    Q_OBJECT
public:
    explicit AgentStatusStrip(QWidget *parent = nullptr);

    void refreshAgents(const QVector<AgentMonitor::AgentSummary> &agents);

signals:
    void paneClicked(const QString &paneId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QHBoxLayout *m_layout;
};

#endif // STATUSSTRIP_H
