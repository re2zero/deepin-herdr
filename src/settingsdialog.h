#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QWidget>

#include "settingsstyle.h"
#include "updater.h"

class QTermWidget;
class QFontComboBox;
class QSpinBox;
class QComboBox;
class QFrame;
class QLabel;
class QStackedWidget;
class QListWidget;
class QSlider;
class QLineEdit;
class QPushButton;
class QButtonGroup;
class AppCore;
using SettingsStyle::SwitchCheck;

// Settings UI as a plain content widget (nav list + pages). It is hosted
// in a DDialog on deepin/UOS and in a plain QDialog elsewhere — see
// Platform::showContentDialog. Live preview applies immediately;
// persist() writes everything (call on dialog close).
//
// Visual language (see docs/design/settings-redesign.html): group cards
// on a neutral window floor, pill switches, semantic status banners.
// All colors derive from the running palette, restyled on
// QEvent::PaletteChange — same code path for DTK/generic shells and for
// macOS/Windows.
class SettingsDialog : public QWidget {
    Q_OBJECT
public:
    struct TerminalSettings {
        QString fontFamily;
        int fontSize = 10;
        int cursorShape = 0;        // QTermWidget::KeyboardCursorShape
        bool cursorBlink = false;
        int scrollbackLines = 1000; // -1 = infinite
        bool autoCopyOnSelect = true;
    };

    SettingsDialog(QTermWidget *terminal, AppCore *core);

    // write all current values (idempotent)
    void persist();

signals:
    void settingsChanged(const SettingsDialog::TerminalSettings &settings);
    void themeSelected(const QString &key);
    void transparencyChanged(qreal opacity); // 0.3 ~ 1.0
    void autoCopyChanged(bool enabled);

protected:
    bool event(QEvent *event) override;

private:
    QWidget *createTerminalPage();
    QWidget *createAppearancePage();
    QWidget *createHerdrPage();
    QWidget *createAboutPage();

    void applyFontPreview();
    void updateMonoWarning(const QString &family);
    void refreshStyle();
    void syncSwatches(const QString &key);
    void restoreDefaults();
    void applySearch(const QString &text);
    void updateServerArea();
    void refreshLastCheckLabel();

    QTermWidget *m_terminal;
    AppCore *m_core;

    // nav pane
    QListWidget *m_navList;
    QLineEdit *m_searchEdit;
    QPushButton *m_resetButton;
    QStackedWidget *m_stack;
    QList<QWidget *> m_cards; // searchable group cards across all pages

    // terminal page
    QFontComboBox *m_fontCombo;
    QSpinBox *m_sizeSpinBox;
    QComboBox *m_cursorCombo;
    SwitchCheck *m_blinkCheck;
    QSpinBox *m_scrollbackSpin;
    SwitchCheck *m_autoCopyCheck;
    QWidget *m_monoBannerRow = nullptr; // amber banner, shown for non-mono fonts

    // appearance page
    QButtonGroup *m_themeSwatches;
    QSlider *m_opacitySlider;
    QLabel *m_opacityValue;

    // herdr page
    QFrame *m_serverDot;
    QLabel *m_serverStatusLabel;
    QLabel *m_serverVerLabel;
    QWidget *m_serverBannerRow = nullptr; // red banner row, problem state only
    QLabel *m_serverBanner;
    QPushButton *m_serverRestartButton;
    QLabel *m_herdrVersionLabel;
    QLabel *m_lastCheckLabel;
    QLabel *m_herdrStatus;
    QComboBox *m_mirrorCombo;
    QLineEdit *m_customMirrorEdit;
    QPushButton *m_herdrCheckButton;
    QPushButton *m_herdrUpdateButton;
    SwitchCheck *m_agentNotifyCheck;
    SwitchCheck *m_agentNotifyIdleCheck;
    QComboBox *m_dndCombo;
    SwitchCheck *m_closeToTrayCheck;

    // about page
    QLabel *m_appStatus;
    SwitchCheck *m_autoCheckCheck;
    QPushButton *m_releasesButton;
    QPushButton *m_githubButton;
    QPushButton *m_openLogButton;

    TerminalSettings m_settings;
    ReleaseUpdater::Release m_herdrLatest;
    ReleaseUpdater::Release m_appLatest;
    QString m_themeKey;
    bool m_styleRefreshPending = false; // queued style rebuild in flight
};

#endif // SETTINGSDIALOG_H
