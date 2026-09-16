#include "settingsdialog.h"
#include "appcore.h"
#include "version.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFontComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <qtermwidget.h>

static constexpr int MIN_FONT_SIZE = 6;
static constexpr int MAX_FONT_SIZE = 72;
static constexpr int DEFAULT_FONT_SIZE = 10;
static constexpr int MIN_OPACITY_PERCENT = 30;
static constexpr int CONTROL_WIDTH = 200;

namespace {

struct ThemeEntry {
    QString key;   // value stored in QSettings / action data
    QString name;  // display name
};

QList<ThemeEntry> themeEntries()
{
    QList<ThemeEntry> entries;
    entries.append({QStringLiteral("Auto"), QObject::tr("Follow System")});
    entries.append({QStringLiteral("Light"), QObject::tr("Light")});
    entries.append({QStringLiteral("Dark"), QObject::tr("Dark")});

    const QStringList schemes = QTermWidget::availableColorSchemes();
    for (const QString &scheme : schemes) {
        if (scheme == "Light" || scheme == "Dark" || scheme == "System") {
            continue;
        }
        QString display = scheme;
        if (scheme == "Theme1") display = THEME_ONE_NAME;
        else if (scheme == "Theme2") display = THEME_TWO_NAME;
        else if (scheme == "Theme3") display = THEME_THREE_NAME;
        else if (scheme == "Theme4") display = THEME_FOUR_NAME;
        else if (scheme == "Theme5") display = THEME_FIVE_NAME;
        else if (scheme == "Theme6") display = THEME_SIX_NAME;
        else if (scheme == "Theme7") display = THEME_SEVEN_NAME;
        else if (scheme == "Theme8") display = THEME_EIGHT_NAME;
        else if (scheme == "Theme9") display = THEME_NINE_NAME;
        else if (scheme == "Theme10") display = THEME_TEN_NAME;
        entries.append({scheme, display});
    }
    return entries;
}

QLabel *makePageTitle(const QString &text)
{
    auto *title = new QLabel(text);
    QFont font = title->font();
    font.setPointSizeF(font.pointSizeF() + 2.5);
    font.setBold(true);
    title->setFont(font);
    title->setContentsMargins(4, 4, 4, 10);
    return title;
}

QFrame *makeSeparator()
{
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setFixedHeight(1);
    return line;
}

QWidget *makeHeightForWidth(QWidget *row);

// DTK-style setting row: label on the left, control pinned to the
// right, hairline separators drawn between rows (see RowList).
QWidget *makeRow(const QString &label, QWidget *control)
{
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(16, 10, 16, 10);
    h->setSpacing(12);
    if (!label.isEmpty()) {
        h->addWidget(new QLabel(label, row));
    }
    h->addStretch();
    if (control) {
        h->addWidget(control);
    }
    return makeHeightForWidth(row);
}

// Full-width row (checkbox lines, buttons, wrapped status text).
QWidget *makeFullRow(QWidget *content)
{
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(16, 10, 16, 10);
    h->addWidget(content, 1);
    return makeHeightForWidth(row);
}

// A plain QWidget with a layout does not propagate its children's
// heightForWidth to the parent layout unless the flag is set — without
// it, word-wrapped status text gets clipped to a single-line row.
QWidget *makeHeightForWidth(QWidget *row)
{
    QSizePolicy policy = row->sizePolicy();
    policy.setHeightForWidth(true);
    policy.setVerticalPolicy(QSizePolicy::Minimum);
    row->setSizePolicy(policy);
    return row;
}

// Appends rows with hairline separators between them.
class RowList {
public:
    explicit RowList(QVBoxLayout *layout)
        : m_layout(layout)
    {
    }

    void add(QWidget *row)
    {
        if (!m_first) {
            m_layout->addWidget(makeSeparator());
        }
        m_first = false;
        m_layout->addWidget(row);
    }

private:
    QVBoxLayout *m_layout;
    bool m_first = true;
};

QLabel *makeStatusLabel()
{
    auto *label = new QLabel;
    label->setWordWrap(true); // also enables heightForWidth so rows grow
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return label;
}

} // namespace

SettingsDialog::SettingsDialog(QTermWidget *terminal, AppCore *core)
    : QWidget(nullptr)
    , m_terminal(terminal)
    , m_core(core)
{
    QFont currentFont = m_terminal->getTerminalFont();
    m_settings.fontFamily = currentFont.family();
    m_settings.fontSize = currentFont.pointSize() > 0 ? currentFont.pointSize() : DEFAULT_FONT_SIZE;

    QSettings s("deepin-herdr", "deepin-herdr");
    m_settings.cursorShape = s.value("cursorShape", 0).toInt();
    if (m_settings.cursorShape < 0 || m_settings.cursorShape > 2) {
        m_settings.cursorShape = 0;
    }
    m_settings.cursorBlink = s.value("cursorBlink", false).toBool();
    m_settings.scrollbackLines = s.value("scrollbackLines", 1000).toInt();
    m_settings.autoCopyOnSelect = s.value("autoCopyOnSelect", true).toBool();
    m_themeKey = s.value("theme", "Auto").toString();

    auto *contentWidget = new QWidget(this);
    auto *layout = new QHBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    // DTK-style nav: borderless list with pill selection
    auto *pageList = new QListWidget(contentWidget);
    pageList->setViewMode(QListView::ListMode);
    pageList->setSelectionMode(QAbstractItemView::SingleSelection);
    pageList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pageList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pageList->setTextElideMode(Qt::ElideNone);
    pageList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; outline: 0; }"
        "QListWidget::item { padding: 6px 12px; margin: 2px 0; border-radius: 6px; }"
        "QListWidget::item:hover { background: palette(midlight); }"
        "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"));

    auto *stack = new QStackedWidget(contentWidget);
    stack->addWidget(createTerminalPage());
    stack->addWidget(createAppearancePage());
    stack->addWidget(createHerdrPage());
    stack->addWidget(createAboutPage());

    const QStringList pageTitles = {
        QObject::tr("Terminal"),
        QObject::tr("Appearance"),
        QObject::tr("herdr"),
        QObject::tr("About & Updates"),
    };
    for (const QString &title : pageTitles) {
        pageList->addItem(title);
    }
    pageList->setFixedWidth(qMax(150, pageList->sizeHintForColumn(0) + 40));
    pageList->setCurrentRow(0);

    connect(pageList, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);

    layout->addWidget(pageList);
    layout->addWidget(stack, 1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->addWidget(contentWidget);
    setMinimumWidth(640);
}

QWidget *SettingsDialog::createTerminalPage()
{
    auto *page = new QWidget(this);
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(makePageTitle(QObject::tr("Terminal")));

    RowList rows(v);

    // Font family selector
    m_fontCombo = new QFontComboBox(page);
    m_fontCombo->setCurrentFont(QFont(m_settings.fontFamily));
    // D-3: do not restrict to monospaced fonts — allow any font
    m_fontCombo->setFontFilters(QFontComboBox::AllFonts);
    rows.add(makeRow(QObject::tr("Font"), m_fontCombo));

    // Non-monospace alignment warning (D-3)
    m_monoWarning = new QLabel(page);
    m_monoWarning->setWordWrap(true);
    m_monoWarning->setStyleSheet("color: #F39C12; font-size: 11px;");
    m_monoWarningRow = makeFullRow(m_monoWarning);
    m_monoWarningRow->setVisible(false);
    rows.add(m_monoWarningRow);

    // Font size selector
    m_sizeSpinBox = new QSpinBox(page);
    m_sizeSpinBox->setRange(MIN_FONT_SIZE, MAX_FONT_SIZE);
    m_sizeSpinBox->setValue(m_settings.fontSize);
    m_sizeSpinBox->setSuffix(" pt");
    m_sizeSpinBox->setFixedWidth(CONTROL_WIDTH);
    rows.add(makeRow(QObject::tr("Size"), m_sizeSpinBox));

    // Cursor shape (D-1)
    m_cursorCombo = new QComboBox(page);
    m_cursorCombo->addItem(QObject::tr("Block"), 0);
    m_cursorCombo->addItem(QObject::tr("Underline"), 1);
    m_cursorCombo->addItem(QObject::tr("IBeam"), 2);
    m_cursorCombo->setCurrentIndex(m_settings.cursorShape);
    m_cursorCombo->setFixedWidth(CONTROL_WIDTH);
    rows.add(makeRow(QObject::tr("Cursor Shape"), m_cursorCombo));

    // Cursor blink
    m_blinkCheck = new QCheckBox(QObject::tr("Blinking cursor"), page);
    m_blinkCheck->setChecked(m_settings.cursorBlink);
    rows.add(makeFullRow(m_blinkCheck));

    // Scrollback buffer
    m_scrollbackSpin = new QSpinBox(page);
    m_scrollbackSpin->setRange(-1, 999999);
    m_scrollbackSpin->setSpecialValueText(QObject::tr("Unlimited"));
    m_scrollbackSpin->setValue(m_settings.scrollbackLines);
    m_scrollbackSpin->setSuffix(" " + QObject::tr("lines"));
    m_scrollbackSpin->setFixedWidth(CONTROL_WIDTH);
    rows.add(makeRow(QObject::tr("Scrollback"), m_scrollbackSpin));

    // Copy on selection
    m_autoCopyCheck = new QCheckBox(QObject::tr("Copy selection to clipboard automatically"), page);
    m_autoCopyCheck->setChecked(m_settings.autoCopyOnSelect);
    rows.add(makeFullRow(m_autoCopyCheck));

    v->addStretch();

    // Immediate preview (D-5)
    connect(m_fontCombo, &QFontComboBox::currentFontChanged,
            this, [this](const QFont &font) {
                m_settings.fontFamily = font.family();
                applyFontPreview();
                updateMonoWarning(font.family());
            });
    connect(m_sizeSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int size) {
                m_settings.fontSize = size;
                applyFontPreview();
            });
    connect(m_cursorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                m_settings.cursorShape = m_cursorCombo->itemData(index).toInt();
                m_terminal->setKeyboardCursorShape(
                    static_cast<QTermWidget::KeyboardCursorShape>(m_settings.cursorShape));
            });
    connect(m_blinkCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.cursorBlink = on;
        m_terminal->setBlinkingCursor(on);
    });
    connect(m_scrollbackSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int lines) {
        m_settings.scrollbackLines = lines;
        m_terminal->setHistorySize(lines);
    });
    connect(m_autoCopyCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.autoCopyOnSelect = on;
        emit autoCopyChanged(on);
    });

    updateMonoWarning(m_settings.fontFamily);
    return page;
}

QWidget *SettingsDialog::createAppearancePage()
{
    auto *page = new QWidget(this);
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(makePageTitle(QObject::tr("Appearance")));

    RowList rows(v);

    // Theme selector — same keys as the theme menu
    m_themeCombo = new QComboBox(page);
    const QList<ThemeEntry> entries = themeEntries();
    int currentIndex = 0;
    for (const ThemeEntry &entry : entries) {
        m_themeCombo->addItem(entry.name, entry.key);
        if (entry.key == m_themeKey) {
            currentIndex = m_themeCombo->count() - 1;
        }
    }
    m_themeCombo->setCurrentIndex(currentIndex);
    m_themeCombo->setFixedWidth(CONTROL_WIDTH);
    rows.add(makeRow(QObject::tr("Theme"), m_themeCombo));

    // Background transparency
    QSettings settings("deepin-herdr", "deepin-herdr");
    m_opacitySlider = new QSlider(Qt::Horizontal, page);
    m_opacitySlider->setRange(MIN_OPACITY_PERCENT, 100);
    const int opacityPercent = qBound(MIN_OPACITY_PERCENT,
                                      static_cast<int>(settings.value("terminalOpacity", 100.0).toDouble() * 100),
                                      100);
    m_opacitySlider->setValue(opacityPercent);
    m_opacitySlider->setFixedWidth(240);
    m_opacityValue = new QLabel(QStringLiteral("%1%").arg(opacityPercent), page);
    auto *opacityRow = new QWidget(page);
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(8);
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityValue);
    rows.add(makeRow(QObject::tr("Opacity"), opacityRow));

    connect(m_themeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_themeKey = m_themeCombo->itemData(index).toString();
        emit themeSelected(m_themeKey);
    });
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int percent) {
        m_opacityValue->setText(QStringLiteral("%1%").arg(percent));
        emit transparencyChanged(percent / 100.0);
    });

    v->addStretch();
    return page;
}

QWidget *SettingsDialog::createHerdrPage()
{
    auto *page = new QWidget(this);
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(makePageTitle(QStringLiteral("herdr")));

    RowList rows(v);

    // Installed version
    m_herdrVersionLabel = new QLabel(page);
    const QString installed = m_core->herdrVersion();
    m_herdrVersionLabel->setText(installed.isEmpty()
        ? QObject::tr("Unknown") : QStringLiteral("v%1").arg(installed));
    rows.add(makeRow(QObject::tr("Current version"), m_herdrVersionLabel));

    // Update check
    m_herdrStatus = makeStatusLabel();
    m_herdrStatus->setObjectName(QStringLiteral("herdrStatusLabel"));
    m_herdrStatus->installEventFilter(this);
    m_herdrCheckButton = new QPushButton(QObject::tr("Check for updates"), page);
    m_herdrUpdateButton = new QPushButton(page);
    m_herdrUpdateButton->setVisible(false);
    auto *checkButtons = new QWidget(page);
    auto *checkLayout = new QHBoxLayout(checkButtons);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setSpacing(8);
    checkLayout->addWidget(m_herdrCheckButton);
    checkLayout->addWidget(m_herdrUpdateButton);
    checkLayout->addStretch();
    rows.add(makeFullRow(checkButtons));
    rows.add(makeFullRow(m_herdrStatus));

    ReleaseUpdater *updater = m_core->herdrUpdater();
    connect(m_herdrCheckButton, &QPushButton::clicked, this, [this, updater]() {
        m_herdrStatus->setText(QObject::tr("Checking…"));
        m_herdrUpdateButton->setVisible(false);
        updater->checkLatest();
    });
    connect(updater, &ReleaseUpdater::checkFinished, this,
            [this](bool ok, const ReleaseUpdater::Release &release, const QString &error) {
                if (!ok) {
                    m_herdrStatus->setText(QObject::tr("Check failed: %1").arg(error));
                    return;
                }
                m_herdrLatest = release;
                const QString installed = m_core->herdrVersion();
                const bool hasUpdate = installed.isEmpty()
                    || ReleaseUpdater::compareVersions(release.version, installed) > 0;
                if (!hasUpdate) {
                    m_herdrStatus->setText(QObject::tr("herdr is up to date (v%1).").arg(release.version));
                    return;
                }
                m_herdrStatus->setText(QObject::tr("Update available: v%1 (current %2).")
                                           .arg(release.version, installed.isEmpty() ? QObject::tr("Unknown") : installed));
                m_herdrUpdateButton->setText(QObject::tr("Update to v%1").arg(release.version));
                m_herdrUpdateButton->setVisible(true);
            });
    connect(m_herdrUpdateButton, &QPushButton::clicked, this, [this, updater]() {
        m_herdrUpdateButton->setEnabled(false);
        m_herdrStatus->setText(QObject::tr("Downloading…"));
        updater->downloadAndInstall(m_herdrLatest);
    });
    connect(updater, &ReleaseUpdater::installProgress, this, [this](int percent) {
        m_herdrStatus->setText(QObject::tr("Downloading… %1%").arg(percent));
    });
    connect(updater, &ReleaseUpdater::installFinished, this,
            [this](bool ok, const QString &error) {
                m_herdrUpdateButton->setEnabled(true);
                m_herdrUpdateButton->setVisible(false);
                if (ok) {
                    m_herdrStatus->setText(QObject::tr("herdr updated. Restart the herdr server to apply."));
                } else {
                    m_herdrStatus->setText(QObject::tr("Update failed: %1").arg(error));
                }
            });

    // Download source (mirror) settings
    QSettings settings("deepin-herdr", "deepin-herdr");
    m_mirrorCombo = new QComboBox(page);
    m_mirrorCombo->addItem(QObject::tr("Auto (recommended)"), "auto");
    m_mirrorCombo->addItem(QObject::tr("GitHub direct"), "direct");
    m_mirrorCombo->addItem(QObject::tr("Mirror first"), "mirror");
    const QString mode = settings.value("mirrorMode", "auto").toString();
    const int modeIndex = m_mirrorCombo->findData(mode);
    m_mirrorCombo->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
    m_mirrorCombo->setFixedWidth(CONTROL_WIDTH);
    rows.add(makeRow(QObject::tr("Download source"), m_mirrorCombo));

    m_customMirrorEdit = new QLineEdit(page);
    m_customMirrorEdit->setPlaceholderText(QStringLiteral("https://your-mirror.example/"));
    m_customMirrorEdit->setText(settings.value("customMirrorUrl").toString());
    m_customMirrorEdit->setFixedWidth(CONTROL_WIDTH + 80);
    rows.add(makeRow(QObject::tr("Custom mirror"), m_customMirrorEdit));

    // Agent state notifications
    m_agentNotifyCheck = new QCheckBox(QObject::tr("Notify when an agent needs attention"), page);
    m_agentNotifyCheck->setChecked(settings.value("agentNotify", true).toBool());
    rows.add(makeFullRow(m_agentNotifyCheck));

    m_agentNotifyIdleCheck = new QCheckBox(QObject::tr("Also notify when an agent goes idle"), page);
    m_agentNotifyIdleCheck->setChecked(settings.value("agentNotifyIdle", false).toBool());
    m_agentNotifyIdleCheck->setEnabled(m_agentNotifyCheck->isChecked());
    connect(m_agentNotifyCheck, &QCheckBox::toggled, m_agentNotifyIdleCheck, &QWidget::setEnabled);
    rows.add(makeFullRow(m_agentNotifyIdleCheck));

    v->addStretch();

    auto applyMirror = [this]() {
        const QString mode = m_mirrorCombo->currentData().toString();
        ReleaseUpdater::MirrorMode m = ReleaseUpdater::MirrorMode::Auto;
        if (mode == "direct") m = ReleaseUpdater::MirrorMode::DirectFirst;
        else if (mode == "mirror") m = ReleaseUpdater::MirrorMode::MirrorFirst;
        for (ReleaseUpdater *u : {m_core->herdrUpdater(), m_core->appUpdater()}) {
            u->setMirrorMode(m);
            u->setCustomMirrorPrefix(m_customMirrorEdit->text().trimmed());
        }
    };
    connect(m_mirrorCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, applyMirror);
    connect(m_customMirrorEdit, &QLineEdit::textChanged, this, applyMirror);

    return page;
}

QWidget *SettingsDialog::createAboutPage()
{
    auto *page = new QWidget(this);
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(makePageTitle(QObject::tr("About & Updates")));

    RowList rows(v);

    QLabel *appVersion = new QLabel(QStringLiteral("v" APP_VERSION), page);
    rows.add(makeRow(QObject::tr("App version"), appVersion));

    m_appStatus = makeStatusLabel();
    m_appStatus->setObjectName(QStringLiteral("appStatusLabel"));
    m_appStatus->installEventFilter(this);
    auto *checkBtn = new QPushButton(QObject::tr("Check for updates"), page);
    m_releasesButton = new QPushButton(QObject::tr("Open releases page"), page);
    auto *buttons = new QWidget(page);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    row->addWidget(checkBtn);
    row->addWidget(m_releasesButton);
    row->addStretch();
    rows.add(makeFullRow(buttons));
    rows.add(makeFullRow(m_appStatus));

    ReleaseUpdater *updater = m_core->appUpdater();
    connect(checkBtn, &QPushButton::clicked, this, [this, updater]() {
        m_appStatus->setText(QObject::tr("Checking…"));
        updater->checkLatest();
    });
    connect(updater, &ReleaseUpdater::checkFinished, this,
            [this](bool ok, const ReleaseUpdater::Release &release, const QString &error) {
                if (!ok) {
                    m_appStatus->setText(QObject::tr("Check failed: %1").arg(error));
                    return;
                }
                m_appLatest = release;
                if (ReleaseUpdater::compareVersions(release.version, APP_VERSION) > 0) {
                    m_appStatus->setText(QObject::tr("App update available: v%1 (current %2).")
                                             .arg(release.version, APP_VERSION));
                } else {
                    m_appStatus->setText(QObject::tr("App is up to date (v%1).").arg(APP_VERSION));
                }
            });
    connect(m_releasesButton, &QPushButton::clicked, this, [this, updater]() {
        const QUrl url(!m_appLatest.htmlUrl.isEmpty()
            ? m_appLatest.htmlUrl
            : QUrl(updater->releasesPageUrl()));
        QDesktopServices::openUrl(url);
    });

    m_autoCheckCheck = new QCheckBox(QObject::tr("Check for updates on startup"), page);
    QSettings aboutSettings("deepin-herdr", "deepin-herdr");
    m_autoCheckCheck->setChecked(aboutSettings.value("autoCheckUpdates", true).toBool());
    rows.add(makeFullRow(m_autoCheckCheck));

    v->addStretch();
    return page;
}

// Word-wrapped status labels reflow after setText(); the host dialog was
// sized for the empty state, so grow it back to fit (converges: a
// no-op adjustSize produces no further resize events).
bool SettingsDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize && watched == m_herdrStatus
            || event->type() == QEvent::Resize && watched == m_appStatus) {
        QWidget *host = window();
        if (host && host != this) {
            host->adjustSize();
        }
        adjustSize();
    }
    return QWidget::eventFilter(watched, event);
}

void SettingsDialog::persist()
{
    m_settings.fontFamily = m_fontCombo->currentFont().family();
    m_settings.fontSize = m_sizeSpinBox->value();
    m_settings.cursorShape = m_cursorCombo->currentData().toInt();
    m_settings.cursorBlink = m_blinkCheck->isChecked();
    m_settings.scrollbackLines = m_scrollbackSpin->value();
    m_settings.autoCopyOnSelect = m_autoCopyCheck->isChecked();
    emit settingsChanged(m_settings);

    // updater/notification keys are owned by the dialog
    QSettings store("deepin-herdr", "deepin-herdr");
    store.setValue("mirrorMode", m_mirrorCombo->currentData().toString());
    store.setValue("customMirrorUrl", m_customMirrorEdit->text().trimmed());
    store.setValue("autoCheckUpdates", m_autoCheckCheck->isChecked());
    store.setValue("agentNotify", m_agentNotifyCheck->isChecked());
    store.setValue("agentNotifyIdle", m_agentNotifyIdleCheck->isChecked());
}

void SettingsDialog::applyFontPreview()
{
    QFont font = m_terminal->getTerminalFont();
    font.setFamily(m_settings.fontFamily);
    int size = m_settings.fontSize;
    if (size < MIN_FONT_SIZE) size = MIN_FONT_SIZE;
    if (size > MAX_FONT_SIZE) size = MAX_FONT_SIZE;
    font.setPointSize(size);
    m_terminal->setTerminalFont(font);
}

void SettingsDialog::updateMonoWarning(const QString &family)
{
    bool monospace = QFontDatabase::isFixedPitch(family);
    m_monoWarning->setVisible(!monospace);
    m_monoWarningRow->setVisible(!monospace);
    if (!monospace) {
        m_monoWarning->setText(QObject::tr(
            "This font is not monospaced; terminal alignment may be affected."));
    }
}
