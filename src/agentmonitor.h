#ifndef AGENTMONITOR_H
#define AGENTMONITOR_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class QProcess;
class QTimer;

// Watches herdr agent states by polling `herdr agent list` (JSON over a
// local server round-trip, cheap) and emits one signal per attention
// transition: an agent entering "blocked", "done" or (optionally) "idle"
// from any other state.
//
// Anti-spam rules: the first successful poll only primes the state map
// (no signals), newly seen panes never signal, and a pane is rate
// limited per status. The enable/idle switches are re-read from
// QSettings on every poll so the settings dialog applies immediately.
class AgentMonitor : public QObject {
    Q_OBJECT
public:
    explicit AgentMonitor(const QString &herdrBinary, QObject *parent = nullptr);

    // one row of `herdr agent list`, consumed by tray tooltip/menu
    struct AgentSummary {
        QString paneId;
        QString agent;
        QString title;
        QString status;
        QString cwd;

        // "project (agent)" — the project directory name is what tells
        // same-named agents apart; falls back to agent, then paneId
        QString label() const;
    };

    static QString displayName(const QString &agent, const QString &cwd);

    void start();
    void stop();
    bool isRunning() const;

    static constexpr int POLL_INTERVAL_MS = 4000;
    static constexpr qint64 PER_PANE_COOLDOWN_MS = 60000;

signals:
    // status: "blocked", "done" or "idle"
    void agentAttention(const QString &paneId, const QString &agent,
                        const QString &title, const QString &cwd, const QString &status);

    // full snapshot after every successful poll, sorted by pane id
    void agentsChanged(const QVector<AgentMonitor::AgentSummary> &agents);

private slots:
    void pollNow();

private:
    void handlePollFinished();
    void absorbStates(const QHash<QString, QString> &statusByPane);

    QString m_binary;
    QTimer *m_timer;
    QProcess *m_poll = nullptr;
    QHash<QString, QString> m_statusByPane;      // paneId -> agent_status
    QHash<QString, qint64> m_lastNotifiedAt;     // paneId+status -> ms epoch
    bool m_primed = false;
};

#endif // AGENTMONITOR_H
