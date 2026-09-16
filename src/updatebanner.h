#ifndef UPDATEBANNER_H
#define UPDATEBANNER_H

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QWidget>

// Non-modal strip shown above the terminal when an update is available.
// States: info (action + skip + close), downloading (progress), done, error.
// Tracks application palette changes so it works under DTK themes and
// plain Qt styles alike.
class UpdateBanner : public QWidget {
    Q_OBJECT
public:
    explicit UpdateBanner(QWidget *parent = nullptr);

    // update semantics: action + skip-this-version + close
    void showUpdate(const QString &message, const QString &actionText);
    // lifecycle notices: optional action + close, never a skip button
    void showNotice(const QString &message, const QString &actionText = QString());
    void showProgress(int percent);
    void showDone(const QString &message);
    void showError(const QString &message);

signals:
    void actionTriggered();
    void skipTriggered();
    void dismissed();

protected:
    void changeEvent(QEvent *event) override;

private:
    void present(const QString &message, const QString &actionText, bool showSkip);

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
