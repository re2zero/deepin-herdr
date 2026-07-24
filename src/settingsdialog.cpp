#include "settingsdialog.h"

#include <QFormLayout>
#include <QLabel>
#include <QFontComboBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFontDatabase>

#include <qtermwidget.h>

static constexpr int MIN_FONT_SIZE = 6;
static constexpr int MAX_FONT_SIZE = 72;
static constexpr int DEFAULT_FONT_SIZE = 10;

SettingsDialog::SettingsDialog(QTermWidget *terminal, int cursorShape, QWidget *parent)
    : DDialog(parent)
    , m_terminal(terminal)
    , m_cursorShape(cursorShape)
    , m_persisted(false)
{
    setTitle(QObject::tr("Settings"));

    QFont currentFont = m_terminal->getTerminalFont();
    m_fontFamily = currentFont.family();
    m_fontSize = currentFont.pointSize() > 0 ? currentFont.pointSize() : DEFAULT_FONT_SIZE;

    auto *contentWidget = new QWidget(this);
    auto *formLayout = new QFormLayout(contentWidget);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setSpacing(10);

    // Font family selector
    m_fontCombo = new QFontComboBox(contentWidget);
    m_fontCombo->setCurrentFont(QFont(m_fontFamily));
    // D-3: do not restrict to monospaced fonts — allow any font
    m_fontCombo->setFontFilters(QFontComboBox::AllFonts);
    formLayout->addRow(QObject::tr("Font"), m_fontCombo);

    // Non-monospace alignment warning (D-3)
    m_monoWarning = new QLabel(contentWidget);
    m_monoWarning->setWordWrap(true);
    m_monoWarning->setStyleSheet("color: #F39C12; font-size: 11px;");
    m_monoWarning->setVisible(false);
    formLayout->addRow(QString(), m_monoWarning);

    // Font size selector
    m_sizeSpinBox = new QSpinBox(contentWidget);
    m_sizeSpinBox->setRange(MIN_FONT_SIZE, MAX_FONT_SIZE);
    m_sizeSpinBox->setValue(m_fontSize);
    m_sizeSpinBox->setSuffix(" pt");
    formLayout->addRow(QObject::tr("Size"), m_sizeSpinBox);

    // Cursor shape selector (D-1: terminal text cursor shape)
    m_cursorCombo = new QComboBox(contentWidget);
    m_cursorCombo->addItem(QObject::tr("Block"), 0);
    m_cursorCombo->addItem(QObject::tr("Underline"), 1);
    m_cursorCombo->addItem(QObject::tr("IBeam"), 2);
    m_cursorCombo->setCurrentIndex(m_cursorShape);
    formLayout->addRow(QObject::tr("Cursor Shape"), m_cursorCombo);

    addContent(contentWidget);
    addButton(QObject::tr("OK"));
    setCloseButtonVisible(true);

    setFixedWidth(380);

    updateMonoWarning(m_fontFamily);

    // Immediate preview (D-5)
    connect(m_fontCombo, &QFontComboBox::currentFontChanged,
            this, [this](const QFont &font) {
                onFontFamilyChanged(font.family());
            });
    connect(m_sizeSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, &SettingsDialog::onFontSizeChanged);
    connect(m_cursorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onCursorShapeChanged);

    // Persist on close (D-5: close window = save current preview state)
    connect(this, &DDialog::closed, this, &SettingsDialog::onDialogClosed);
}

void SettingsDialog::onFontFamilyChanged(const QString &family)
{
    m_fontFamily = family;
    updateMonoWarning(family);
    applyFontPreview();
}

void SettingsDialog::onFontSizeChanged(int size)
{
    m_fontSize = size;
    applyFontPreview();
}

void SettingsDialog::onCursorShapeChanged(int index)
{
    m_cursorShape = m_cursorCombo->itemData(index).toInt();
    m_terminal->setKeyboardCursorShape(
        static_cast<QTermWidget::KeyboardCursorShape>(m_cursorShape));
}

void SettingsDialog::applyFontPreview()
{
    QFont font = m_terminal->getTerminalFont();
    font.setFamily(m_fontFamily);
    int size = m_fontSize;
    if (size < MIN_FONT_SIZE) size = MIN_FONT_SIZE;
    if (size > MAX_FONT_SIZE) size = MAX_FONT_SIZE;
    font.setPointSize(size);
    m_terminal->setTerminalFont(font);
}

void SettingsDialog::updateMonoWarning(const QString &family)
{
    bool monospace = QFontDatabase::isFixedPitch(family);
    m_monoWarning->setVisible(!monospace);
    if (!monospace) {
        m_monoWarning->setText(QObject::tr(
            "This font is not monospaced; terminal alignment may be affected."));
    }
}

void SettingsDialog::onDialogClosed()
{
    if (!m_persisted) {
        m_persisted = true;
        emit settingsChanged(m_fontFamily, m_fontSize, m_cursorShape);
    }
}
