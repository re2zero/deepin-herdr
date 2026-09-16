#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QWidget>

#include "updater.h"

class QTermWidget;
class QFontComboBox;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QStackedWidget;
class QListWidget;
class QSlider;
class QLineEdit;
class QPushButton;
class AppCore;

// Settings UI as a plain content widget (nav list + pages). It is hosted
// in a DDialog on deepin/UOS and in a plain QDialog elsewhere — see
// Platform::showContentDialog. Live preview applies immediately;
// persist() writes everything (call on dialog close).
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

private:
    QWidget *createTerminalPage();
    QWidget *createAppearancePage();
    QWidget *createHerdrPage();
    QWidget *createAboutPage();

    void applyFontPreview();
    void updateMonoWarning(const QString &family);

    QTermWidget *m_terminal;
    AppCore *m_core;

    // terminal page
    QFontComboBox *m_fontCombo;
    QSpinBox *m_sizeSpinBox;
    QComboBox *m_cursorCombo;
    QCheckBox *m_blinkCheck;
    QSpinBox *m_scrollbackSpin;
    QCheckBox *m_autoCopyCheck;
    QLabel *m_monoWarning;
    QWidget *m_monoWarningRow = nullptr;

    // appearance page
    QComboBox *m_themeCombo;
    QSlider *m_opacitySlider;
    QLabel *m_opacityValue;

    // herdr page
    QLabel *m_herdrVersionLabel;
    QLabel *m_herdrStatus;
    QComboBox *m_mirrorCombo;
    QLineEdit *m_customMirrorEdit;
    QPushButton *m_herdrCheckButton;
    QPushButton *m_herdrUpdateButton;
    QCheckBox *m_agentNotifyCheck;
    QCheckBox *m_agentNotifyIdleCheck;

    // about page
    QLabel *m_appStatus;
    QCheckBox *m_autoCheckCheck;
    QPushButton *m_releasesButton;

    TerminalSettings m_settings;
    ReleaseUpdater::Release m_herdrLatest;
    ReleaseUpdater::Release m_appLatest;
    QString m_themeKey;
};

#endif // SETTINGSDIALOG_H
