#ifndef UPDATEBANNER_H
#define UPDATEBANNER_H

#include <DWidget>

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

DWIDGET_USE_NAMESPACE

// Non-modal strip shown above the terminal when an update is available.
// States: info (action + skip + close), downloading (progress), done, error.
class UpdateBanner : public DWidget {
    Q_OBJECT
public:
    explicit UpdateBanner(QWidget *parent = nullptr);

    void showUpdate(const QString &message, const QString &actionText);
    void showProgress(int percent);
    void showDone(const QString &message);
    void showError(const QString &message);

signals:
    void actionTriggered();
    void skipTriggered();
    void dismissed();

private:
    QLabel *m_icon;
    QLabel *m_message;
    QLabel *m_status;
    QPushButton *m_actionButton;
    QPushButton *m_skipButton;
    QProgressBar *m_progress;
    QPushButton *m_closeButton;
    // when true, the action button acts as an acknowledgement (dismiss)
    bool m_actionDismisses = false;
};

#endif // UPDATEBANNER_H
