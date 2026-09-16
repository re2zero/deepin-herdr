#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <DDialog>

#include "updater.h"

DWIDGET_USE_NAMESPACE

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
class MainWindow;

// Paginated settings: Terminal / Appearance / herdr / About & Updates.
// Live preview applies immediately; everything persists when the dialog
// closes (matching the original persist-on-close behavior).
class SettingsDialog : public DDialog {
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

    SettingsDialog(QTermWidget *terminal, MainWindow *window);

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
    MainWindow *m_window;

    // terminal page
    QFontComboBox *m_fontCombo;
    QSpinBox *m_sizeSpinBox;
    QComboBox *m_cursorCombo;
    QCheckBox *m_blinkCheck;
    QSpinBox *m_scrollbackSpin;
    QCheckBox *m_autoCopyCheck;
    QLabel *m_monoWarning;

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

    // about page
    QLabel *m_appStatus;
    QCheckBox *m_autoCheckCheck;
    QPushButton *m_releasesButton;

    TerminalSettings m_settings;
    ReleaseUpdater::Release m_herdrLatest;
    ReleaseUpdater::Release m_appLatest;
    QString m_themeKey;
    bool m_persisted = false;
};

#endif // SETTINGSDIALOG_H
