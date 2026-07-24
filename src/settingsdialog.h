#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <DDialog>

DWIDGET_USE_NAMESPACE

class QTermWidget;
class QFontComboBox;
class QSpinBox;
class QComboBox;
class QLabel;

class SettingsDialog : public DDialog {
    Q_OBJECT
public:
    SettingsDialog(QTermWidget *terminal, int cursorShape, QWidget *parent = nullptr);

signals:
    void settingsChanged(const QString &fontFamily, int fontSize, int cursorShape);

private slots:
    void onFontFamilyChanged(const QString &family);
    void onFontSizeChanged(int size);
    void onCursorShapeChanged(int index);

private:
    void applyFontPreview();
    void updateMonoWarning(const QString &family);
    void onDialogClosed();

    QTermWidget *m_terminal;
    QFontComboBox *m_fontCombo;
    QSpinBox *m_sizeSpinBox;
    QComboBox *m_cursorCombo;
    QLabel *m_monoWarning;

    QString m_fontFamily;
    int m_fontSize;
    int m_cursorShape;
    bool m_persisted;
};

#endif // SETTINGSDIALOG_H
