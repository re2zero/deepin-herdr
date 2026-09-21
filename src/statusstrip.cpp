#include "statusstrip.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>

namespace {

// capsule dot color per herdr agent status
QString statusColor(const QString &status)
{
    if (status == QLatin1String("running")) return QStringLiteral("#2F86FF");
    if (status == QLatin1String("blocked")) return QStringLiteral("#F54A45");
    if (status == QLatin1String("done"))    return QStringLiteral("#10B981");
    return QStringLiteral("#909399"); // idle or unknown
}

} // namespace

AgentStatusStrip::AgentStatusStrip(QWidget *parent)
    : QFrame(parent)
{
    setFixedHeight(28);
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 3, 8, 3);
    m_layout->setSpacing(6);
    hide(); // appears with the first poll that finds agents
}

void AgentStatusStrip::refreshAgents(const QVector<AgentMonitor::AgentSummary> &agents)
{
    while (QLayoutItem *item = m_layout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    setVisible(!agents.isEmpty());

    for (const AgentMonitor::AgentSummary &a : agents) {
        QString name = a.agent.isEmpty() ? a.paneId : a.agent;
        if (name.size() > 28) {
            name = name.left(27) + QStringLiteral("…");
        }
        const bool blocked = a.status == QLatin1String("blocked");
        auto *capsule = new QLabel(this);
        capsule->setTextFormat(Qt::RichText);
        capsule->setText(QStringLiteral("<span style='color:%1'>●</span> %2")
                             .arg(statusColor(a.status), name.toHtmlEscaped()));
        capsule->setToolTip(QStringLiteral("%1 — %2").arg(a.paneId, a.status));
        // blocked gets a solid alert capsule, everything else a quiet one
        capsule->setStyleSheet(blocked
            ? QStringLiteral("QLabel { background: #F54A45; color: white;"
                             " border-radius: 10px; padding: 2px 9px; font-size: 11px; }")
            : QStringLiteral("QLabel { border-radius: 10px; padding: 2px 9px;"
                             " font-size: 11px;"
                             " background: palette(window); border: 1px solid palette(mid); }"));
        capsule->setProperty("paneId", a.paneId);
        capsule->setCursor(Qt::PointingHandCursor);
        // keep capsules at content width: the strip's leftover space
        // goes to the trailing stretch, not to the capsules
        capsule->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        capsule->installEventFilter(this);
        m_layout->addWidget(capsule);
    }
    m_layout->addStretch();
}

bool AgentStatusStrip::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto *capsule = qobject_cast<QLabel *>(watched)) {
            const QString paneId = capsule->property("paneId").toString();
            if (!paneId.isEmpty()) {
                emit paneClicked(paneId);
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}
