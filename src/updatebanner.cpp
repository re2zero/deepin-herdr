#include "updatebanner.h"

#include <QEvent>
#include <QHBoxLayout>

UpdateBanner::UpdateBanner(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    m_icon = new QLabel(this);
    m_icon->setText(QStringLiteral("⬆"));
    layout->addWidget(m_icon);

    m_message = new QLabel(this);
    layout->addWidget(m_message);

    layout->addStretch();

    m_status = new QLabel(this);
    m_status->setVisible(false);
    layout->addWidget(m_status);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setFixedWidth(200);
    m_progress->setFixedHeight(30);
    m_progress->setTextVisible(false); // percentage lives in the front label
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    m_actionButton = new QPushButton(this);
    connect(m_actionButton, &QPushButton::clicked, this, [this]() {
        if (m_actionDismisses) {
            hide();
            emit dismissed();
        } else {
            emit actionTriggered();
        }
    });
    layout->addWidget(m_actionButton);

    m_skipButton = new QPushButton(this);
    m_skipButton->setText(tr("Skip this version"));
    connect(m_skipButton, &QPushButton::clicked, this, [this]() {
        hide();
        emit skipTriggered();
    });
    layout->addWidget(m_skipButton);

    m_closeButton = new QPushButton(this);
    m_closeButton->setText(tr("Close"));
    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        hide();
        emit dismissed();
    });
    layout->addWidget(m_closeButton);

    // track light/dark theme switches; DTK and plain Qt both rotate the
    // application palette, which surfaces as a palette change event
    auto repolish = [this]() {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, pal.color(QPalette::Base));
        setPalette(pal);
        setAutoFillBackground(true);
    };
    repolish();

    setFixedHeight(44);
    hide();
}

void UpdateBanner::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange
            || event->type() == QEvent::PaletteChange) {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, pal.color(QPalette::Base));
        setPalette(pal);
        setAutoFillBackground(true);
    }
    QWidget::changeEvent(event);
}

void UpdateBanner::showUpdate(const QString &message, const QString &actionText)
{
    m_actionDismisses = false;
    m_message->setText(message);
    m_actionButton->setText(actionText);
    m_actionButton->setVisible(true);
    m_skipButton->setVisible(true);
    m_progress->setVisible(false);
    m_status->setVisible(false);
    m_closeButton->setVisible(true);
    show();
    raise();
}

void UpdateBanner::showProgress(int percent)
{
    m_actionButton->setVisible(false);
    m_skipButton->setVisible(false);
    m_closeButton->setVisible(false);
    m_progress->setVisible(true);
    m_progress->setValue(percent);
    m_status->setVisible(true);
    m_status->setText(QStringLiteral("%1%").arg(percent));
    show();
    raise();
}

void UpdateBanner::showDone(const QString &message)
{
    m_actionDismisses = true;
    m_message->setText(message);
    m_actionButton->setText(tr("Got it"));
    m_actionButton->setVisible(true);
    m_skipButton->setVisible(false);
    m_progress->setVisible(false);
    m_status->setVisible(false);
    m_closeButton->setVisible(true);
    show();
    raise();
}

void UpdateBanner::showError(const QString &message)
{
    m_actionDismisses = true;
    m_message->setText(message);
    m_actionButton->setVisible(false);
    m_skipButton->setVisible(false);
    m_progress->setVisible(false);
    m_status->setVisible(false);
    m_closeButton->setVisible(true);
    show();
    raise();
}
