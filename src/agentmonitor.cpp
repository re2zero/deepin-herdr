#include "agentmonitor.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>

namespace {

// statuses worth interrupting the user for (herdr: agent needs attention)
bool isAttentionStatus(const QString &status, bool includeIdle)
{
    if (status == QLatin1String("blocked") || status == QLatin1String("done")) {
        return true;
    }
    return includeIdle && status == QLatin1String("idle");
}

struct AgentInfo {
    QString agent;
    QString title;
    QString cwd;
};

} // namespace

AgentMonitor::AgentMonitor(const QString &herdrBinary, QObject *parent)
    : QObject(parent)
    , m_binary(herdrBinary)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(POLL_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &AgentMonitor::pollNow);
}

void AgentMonitor::start()
{
    if (!m_timer->isActive()) {
        m_timer->start();
        QTimer::singleShot(0, this, &AgentMonitor::pollNow);
    }
}

void AgentMonitor::stop()
{
    m_timer->stop();
}

bool AgentMonitor::isRunning() const
{
    return m_timer->isActive();
}

void AgentMonitor::pollNow()
{
    if (m_poll) {
        return; // previous poll still in flight
    }
    if (m_binary.isEmpty()) {
        return;
    }

    m_poll = new QProcess(this);
    connect(m_poll, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        handlePollFinished();
        m_poll->deleteLater();
        m_poll = nullptr;
    });
    connect(m_poll, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        // server down / binary gone: drop the in-flight poll, retry next tick
        m_poll->deleteLater();
        m_poll = nullptr;
    });
    m_poll->start(m_binary, {"agent", "list"});
}

void AgentMonitor::handlePollFinished()
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(m_poll->readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return; // older herdr or transient garbage; keep previous state
    }

    const QJsonArray agents = doc.object().value("result").toObject()
                                  .value("agents").toArray();

    QHash<QString, QString> fresh;
    QHash<QString, AgentInfo> infos;
    for (const QJsonValue &v : agents) {
        const QJsonObject a = v.toObject();
        const QString paneId = a.value("pane_id").toString();
        const QString status = a.value("agent_status").toString();
        if (paneId.isEmpty() || status.isEmpty()) {
            continue;
        }
        fresh.insert(paneId, status);
        infos.insert(paneId, {
            a.value("agent").toString(),
            a.value("terminal_title_stripped").toString(),
            a.value("cwd").toString(),
        });
    }

    QSettings settings("deepin-herdr", "deepin-herdr");
    const bool enabled = settings.value("agentNotify", true).toBool();
    if (!enabled || !m_primed) {
        // disabled or first successful poll: just prime silently, never signal
        m_statusByPane = fresh;
        m_primed = true;
        return;
    }

    const QHash<QString, QString> previous = m_statusByPane;
    m_statusByPane = fresh;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = fresh.constBegin(); it != fresh.constEnd(); ++it) {
        const QString &paneId = it.key();
        const QString &status = it.value();
        // panes we see for the first time never signal
        if (!previous.contains(paneId)) {
            continue;
        }
        if (previous.value(paneId) == status) {
            continue;
        }
        if (!isAttentionStatus(status, settings.value("agentNotifyIdle", false).toBool())) {
            continue;
        }
        // rate limit per pane+status: real progressions (blocked -> done)
        // still notify, only flapping of the same state is suppressed
        const QString cooldownKey = paneId + QLatin1Char('/') + status;
        if (m_lastNotifiedAt.contains(cooldownKey)
                && now - m_lastNotifiedAt.value(cooldownKey) < PER_PANE_COOLDOWN_MS) {
            continue;
        }
        const AgentInfo info = infos.value(paneId);
        m_lastNotifiedAt.insert(cooldownKey, now);
        emit agentAttention(paneId, info.agent, info.title, info.cwd, status);
    }
}
