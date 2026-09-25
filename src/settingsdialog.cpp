#include "settingsdialog.h"
#include "appcore.h"
#include "platform.h"
#include "settingsstyle.h"
#include "version.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <qtermwidget.h>

static constexpr int MIN_FONT_SIZE = 6;
static constexpr int MAX_FONT_SIZE = 72;
static constexpr int DEFAULT_FONT_SIZE = 10;
static constexpr int MIN_OPACITY_PERCENT = 30;
static constexpr int CONTROL_WIDTH = 200;
static constexpr int NAV_WIDTH = 172;

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

// One rounded group card. Rows get hairline separators from the
// stylesheet (QWidget[srow]); the last row opts out via the lastRow
// property, maintained as rows are appended. Style properties are
// strings so the QSS attribute selectors match them directly.
class GroupCard : public QWidget {
public:
    // keywords: extra (English) search terms beyond the visible labels
    explicit GroupCard(const QString &keywords = QString())
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setProperty("card", QStringLiteral("true"));
        if (!keywords.isEmpty()) {
            setProperty("kw", keywords);
        }
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
    }

    void addRow(QWidget *row)
    {
        row->setProperty("srow", QStringLiteral("true"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        if (m_last) {
            m_last->setProperty("lastRow", QStringLiteral("false"));
            restyle(m_last);
        }
        row->setProperty("lastRow", QStringLiteral("true"));
        restyle(row);
        for (const QLabel *label : row->findChildren<QLabel *>()) {
            m_texts.append(label->text());
        }
        m_last = row;
        layout()->addWidget(row);
    }

    bool matches(const QString &needle) const
    {
        if (needle.isEmpty()) {
            return true;
        }
        const QString kw = property("kw").toString();
        return m_texts.join(QLatin1Char(' ')).contains(needle, Qt::CaseInsensitive)
            || kw.contains(needle, Qt::CaseInsensitive);
    }

    static bool isCard(const QWidget *w)
    {
        return w->property("card").toString() == QLatin1String("true");
    }

private:
    // property-driven borders need a polish cycle to take effect
    static void restyle(QWidget *w)
    {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }

    QStringList m_texts;
    QWidget *m_last = nullptr;
};

// Left label (optionally with a muted second line), control pinned right.
QWidget *makeRow(const QString &label, QWidget *control, const QString &sub = QString())
{
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(16, 8, 16, 8);
    h->setSpacing(12);

    QWidget *caption = nullptr;
    if (sub.isEmpty()) {
        caption = new QLabel(label, row);
    } else {
        auto *v = new QVBoxLayout;
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(1);
        v->addWidget(new QLabel(label, row));
        auto *subLabel = new QLabel(sub, row);
        subLabel->setProperty("tone", QStringLiteral("dim"));
        v->addWidget(subLabel);
        caption = new QWidget(row);
        caption->setLayout(v);
    }
    h->addWidget(caption);
    h->addStretch();
    if (control) {
        h->addWidget(control);
    }
    return row;
}

// Full-width row (banners, buttons, wrapped status text).
QWidget *makeFullRow(QWidget *content)
{
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(16, 8, 16, 8);
    h->addWidget(content, 1);
    return row;
}

// Row with a pre-built caption widget (e.g. label + muted sub line).
QWidget *makeRow(QWidget *caption, QWidget *control)
{
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(16, 8, 16, 8);
    h->setSpacing(12);
    h->addWidget(caption);
    h->addStretch();
    if (control) {
        h->addWidget(control);
    }
    return row;
}

// Semantic banner: warn (amber) or err (red) rounded strip.
QWidget *makeBanner(const char *kind, const QString &text)
{
    auto *frame = new QFrame;
    frame->setProperty("banner", QString::fromLatin1(kind));
    auto *h = new QHBoxLayout(frame);
    h->setContentsMargins(12, 8, 12, 8);
    auto *label = new QLabel(text, frame);
    label->setWordWrap(true);
    label->setProperty("bannerText", QString::fromLatin1(kind));
    h->addWidget(label, 1);
    return frame;
}

// Two-line wrapped status text (long check results must not stretch the
// dialog).
QLabel *makeStatusLabel()
{
    auto *label = new QLabel;
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setFixedHeight(label->fontMetrics().lineSpacing() * 2 + 2);
    return label;
}

// Property-driven label color needs a polish cycle after changing it.
void setTone(QLabel *label, const char *tone)
{
    label->setProperty("tone", QString::fromLatin1(tone));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// Small pill badge (version numbers).
QLabel *makeVerBadge(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("ver", true);
    return label;
}

// Live status dot (green = healthy, red = needs attention). A QFrame
// with WA_StyledBackground — an empty QLabel would not paint its QSS
// background at all.
QFrame *makeDot()
{
    auto *dot = new QFrame;
    dot->setProperty("dot", QStringLiteral("dim"));
    dot->setAttribute(Qt::WA_StyledBackground, true);
    dot->setFixedSize(8, 8);
    return dot;
}

QIcon appIcon()
{
    const QIcon themed = QIcon::fromTheme(QStringLiteral("mudi"));
    return themed.isNull() ? QIcon(QStringLiteral(":/mudi.svg")) : themed;
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

    QSettings s = Platform::appSettings();
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

    // ---- nav pane: search, page list, restore defaults ----
    auto *nav = new QWidget(contentWidget);
    nav->setFixedWidth(NAV_WIDTH);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(4, 0, 4, 0);
    navLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(nav);
    m_searchEdit->setProperty("search", true);
    m_searchEdit->setPlaceholderText(tr("Search settings"));
    m_searchEdit->setClearButtonEnabled(true);
    navLayout->addWidget(m_searchEdit);

    m_navList = new QListWidget(nav);
    m_navList->setViewMode(QListView::ListMode);
    m_navList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setTextElideMode(Qt::ElideNone);
    navLayout->addWidget(m_navList, 1);

    m_resetButton = new QPushButton(tr("Restore defaults"), nav);
    m_resetButton->setProperty("flat", true);
    m_resetButton->setCursor(Qt::PointingHandCursor);
    navLayout->addWidget(m_resetButton);

    auto *stack = new QStackedWidget(contentWidget);
    m_stack = stack;
    stack->addWidget(createTerminalPage());
    stack->addWidget(createAppearancePage());
    stack->addWidget(createHerdrPage());
    stack->addWidget(createAboutPage());

    struct NavEntry {
        const char *title;
        SettingsStyle::IconKind icon;
    };
    const QList<NavEntry> navEntries = {
        {QT_TR_NOOP("Terminal"), SettingsStyle::IconKind::Terminal},
        {QT_TR_NOOP("Appearance"), SettingsStyle::IconKind::Appearance},
        {QT_TR_NOOP("herdr"), SettingsStyle::IconKind::Herdr},
        {QT_TR_NOOP("About & Updates"), SettingsStyle::IconKind::About},
    };
    for (const NavEntry &entry : navEntries) {
        m_navList->addItem(tr(entry.title));
    }
    m_navList->setCurrentRow(0);

    connect(m_navList, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SettingsDialog::applySearch);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);

    layout->addWidget(nav);
    layout->addWidget(stack, 1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->addWidget(contentWidget);
    setMinimumWidth(640);

    refreshStyle();

    // the initial server status runs before the stylesheet lands; re-run
    // it once the polish cycle can resolve the tone/dot properties
    QMetaObject::invokeMethod(this, [this] { updateServerArea(); }, Qt::QueuedConnection);

    // collect searchable cards after all pages exist
    for (QWidget *card : stack->findChildren<QWidget *>()) {
        if (GroupCard::isCard(card)) {
            m_cards.append(card);
        }
    }
}

bool SettingsDialog::event(QEvent *event)
{
    // theme flips (system or the app's own theme menu) restyle the dialog.
    // The rebuild must NOT run synchronously: we are inside
    // propagatePaletteChange here, and setIcon/nav relayout re-enters
    // item layout mid-palette-propagation (crashes in the text engine).
    // Queue it for the next event-loop turn instead.
    if (event->type() == QEvent::PaletteChange && !m_styleRefreshPending) {
        m_styleRefreshPending = true;
        QMetaObject::invokeMethod(this, [this] {
            m_styleRefreshPending = false;
            refreshStyle();
        }, Qt::QueuedConnection);
    }
    return QWidget::event(event);
}

void SettingsDialog::refreshStyle()
{
    const QPalette pal = palette();
    setStyleSheet(SettingsStyle::baseQss(SettingsStyle::isDark(pal)));

    // nav icons: neutral tint normally, white on the highlighted row
    const QColor tint = SettingsStyle::isDark(pal) ? QColor(255, 255, 255, 145) : QColor(0, 0, 0, 140);
    const QList<SettingsStyle::IconKind> kinds = {
        SettingsStyle::IconKind::Terminal,
        SettingsStyle::IconKind::Appearance,
        SettingsStyle::IconKind::Herdr,
        SettingsStyle::IconKind::About,
    };
    for (int i = 0; i < kinds.size() && i < m_navList->count(); ++i) {
        m_navList->item(i)->setIcon(SettingsStyle::navIcon(kinds.at(i), tint, Qt::white));
    }
}

QWidget *SettingsDialog::createTerminalPage()
{
    auto *page = new QWidget(this);
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);

    auto *title = new QLabel(tr("Terminal"), page);
    title->setProperty("pageTitle", true);
    v->addWidget(title);

    // font group
    {
        auto *card = new GroupCard(QStringLiteral("font family typeface mono monospace"));
        m_fontCombo = new QFontComboBox(page);
        m_fontCombo->setCurrentFont(QFont(m_settings.fontFamily));
        // D-3: do not restrict to monospaced fonts — allow any font
        m_fontCombo->setFontFilters(QFontComboBox::AllFonts);
        m_fontCombo->setFixedWidth(CONTROL_WIDTH + 40);
        card->addRow(makeRow(tr("Font"), m_fontCombo));

        // amber banner between the font and size rows, visible only for
        // non-monospaced selections (hairline stays consistent either way)
        m_monoBannerRow = makeFullRow(makeBanner("warn", tr(
            "This font is not monospaced; terminal alignment may be affected.")));
        m_monoBannerRow->setVisible(false);
        card->addRow(m_monoBannerRow);

        m_sizeSpinBox = new QSpinBox(page);
        m_sizeSpinBox->setRange(MIN_FONT_SIZE, MAX_FONT_SIZE);
        m_sizeSpinBox->setValue(m_settings.fontSize);
        m_sizeSpinBox->setSuffix(" pt");
        m_sizeSpinBox->setFixedWidth(CONTROL_WIDTH);
        card->addRow(makeRow(tr("Size"), m_sizeSpinBox));
        v->addWidget(card);
    }

    // cursor group
    {
        auto *card = new GroupCard(QStringLiteral("cursor shape blink ibeam block underline"));
        m_cursorCombo = new QComboBox(page);
        m_cursorCombo->addItem(tr("Block"), 0);
        m_cursorCombo->addItem(tr("Underline"), 1);
        m_cursorCombo->addItem(tr("IBeam"), 2);
        m_cursorCombo->setCurrentIndex(m_settings.cursorShape);
        m_cursorCombo->setFixedWidth(CONTROL_WIDTH);
        card->addRow(makeRow(tr("Cursor Shape"), m_cursorCombo));

        m_blinkCheck = new SwitchCheck(page);
        m_blinkCheck->setChecked(m_settings.cursorBlink);
        card->addRow(makeRow(tr("Blinking cursor"), m_blinkCheck));
        v->addWidget(card);
    }

    // buffer & clipboard group
    {
        auto *card = new GroupCard(QStringLiteral("scrollback history buffer clipboard copy selection"));
        m_scrollbackSpin = new QSpinBox(page);
        m_scrollbackSpin->setRange(-1, 999999);
        m_scrollbackSpin->setSpecialValueText(tr("Unlimited"));
        m_scrollbackSpin->setValue(m_settings.scrollbackLines);
        m_scrollbackSpin->setSuffix(" " + tr("lines"));
        m_scrollbackSpin->setFixedWidth(CONTROL_WIDTH);
        card->addRow(makeRow(tr("Scrollback"), m_scrollbackSpin, tr("Output beyond the limit is discarded")));

        m_autoCopyCheck = new SwitchCheck(page);
        m_autoCopyCheck->setChecked(m_settings.autoCopyOnSelect);
        card->addRow(makeRow(tr("Copy selection to clipboard automatically"), m_autoCopyCheck));
        v->addWidget(card);
    }

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
    connect(m_blinkCheck, &QAbstractButton::toggled, this, [this](bool on) {
        m_settings.cursorBlink = on;
        m_terminal->setBlinkingCursor(on);
    });
    connect(m_scrollbackSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int lines) {
        m_settings.scrollbackLines = lines;
        m_terminal->setHistorySize(lines);
    });
    connect(m_autoCopyCheck, &QAbstractButton::toggled, this, [this](bool on) {
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
    v->setSpacing(14);

    auto *title = new QLabel(tr("Appearance"), page);
    title->setProperty("pageTitle", true);
    v->addWidget(title);

    // app theme swatches (same keys as the theme menu)
    {
        auto *card = new GroupCard(QStringLiteral("theme light dark auto follow system appearance"));
        auto *holder = new QWidget(page);
        auto *h = new QHBoxLayout(holder);
        h->setContentsMargins(16, 10, 16, 12);
        h->setSpacing(12);
        h->addStretch();

        m_themeSwatches = new QButtonGroup(holder);
        m_themeSwatches->setExclusive(true);
        const QColor accent(QStringLiteral("#0081FF"));
        struct Swatch {
            const char *key;
            const char *label;
            SettingsStyle::SwatchKind kind;
        };
        const QList<Swatch> swatches = {
            {"Auto", QT_TR_NOOP("Follow System"), SettingsStyle::SwatchKind::Auto},
            {"Light", QT_TR_NOOP("Light"), SettingsStyle::SwatchKind::Light},
            {"Dark", QT_TR_NOOP("Dark"), SettingsStyle::SwatchKind::Dark},
        };
        for (int i = 0; i < swatches.size(); ++i) {
            const Swatch &sw = swatches.at(i);
            auto *btn = new QToolButton(holder);
            btn->setProperty("swatch", QStringLiteral("true"));
            btn->setCheckable(true);
            btn->setIcon(QIcon(SettingsStyle::themeSwatch(sw.kind, accent)));
            btn->setIconSize(QSize(60, 32));
            btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            btn->setChecked(m_themeKey == QLatin1String(sw.key));
            m_themeSwatches->addButton(btn, i);
            h->addWidget(btn);
        }
        h->addStretch();
        card->addRow(makeFullRow(holder));
        v->addWidget(card);
    }

    // terminal color scheme (keys shared with the theme menu)
    {
        auto *card = new GroupCard(QStringLiteral("color scheme terminal palette theme1 theme2"));
        auto *schemeCombo = new QComboBox(page);
        const QList<ThemeEntry> entries = themeEntries();
        int currentIndex = 0;
        for (const ThemeEntry &entry : entries) {
            schemeCombo->addItem(entry.name, entry.key);
            if (entry.key == m_themeKey) {
                currentIndex = schemeCombo->count() - 1;
            }
        }
        schemeCombo->setCurrentIndex(currentIndex);
        schemeCombo->setFixedWidth(CONTROL_WIDTH + 40);
        card->addRow(makeRow(tr("Terminal color scheme"), schemeCombo));
        v->addWidget(card);

        connect(schemeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this, schemeCombo](int index) {
            m_themeKey = schemeCombo->itemData(index).toString();
            syncSwatches(m_themeKey);
            emit themeSelected(m_themeKey);
        });
    }

    // background transparency
    {
        auto *card = new GroupCard(QStringLiteral("opacity transparency background transparent"));
        QSettings settings = Platform::appSettings();
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
        card->addRow(makeRow(tr("Opacity"), opacityRow));
        v->addWidget(card);
    }

    v->addStretch();

    connect(m_themeSwatches, &QButtonGroup::idClicked, this, [this](int id) {
        const QString key = id == 0 ? QStringLiteral("Auto")
                          : id == 1 ? QStringLiteral("Light") : QStringLiteral("Dark");
        m_themeKey = key;
        emit themeSelected(m_themeKey);
    });
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int percent) {
        m_opacityValue->setText(QStringLiteral("%1%").arg(percent));
        emit transparencyChanged(percent / 100.0);
    });

    return page;
}

QWidget *SettingsDialog::createHerdrPage()
{
    auto *page = new QWidget(this);
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("herdr"), page);
    title->setProperty("pageTitle", true);
    v->addWidget(title);

    // server lifecycle (M7): live view refreshed by AppCore's `herdr
    // status` probe; red when the server lags the installed binary
    {
        auto *card = new GroupCard(QStringLiteral("server status restart protocol running"));
        auto *statusRow = new QWidget(page);
        auto *h = new QHBoxLayout(statusRow);
        h->setContentsMargins(16, 8, 16, 8);
        h->setSpacing(10);
        m_serverDot = makeDot();
        m_serverDot->setParent(statusRow);
        h->addWidget(m_serverDot);
        m_serverStatusLabel = makeStatusLabel();
        m_serverStatusLabel->setObjectName(QStringLiteral("serverStatusLabel"));
        h->addWidget(m_serverStatusLabel, 1);
        m_serverVerLabel = makeVerBadge(QString());
        m_serverVerLabel->hide();
        h->addWidget(m_serverVerLabel);
        m_serverRestartButton = new QPushButton(tr("Restart server"), page);
        h->addWidget(m_serverRestartButton);
        card->addRow(statusRow);

        // full-width red banner shown only when the server lags the
        // installed binary (replaces the bare red text of v0.3.0)
        m_serverBannerRow = makeFullRow(makeBanner("err", QString()));
        // the banner wrapper owns the text label; keep a pointer to it
        m_serverBanner = m_serverBannerRow->findChild<QLabel *>();
        m_serverBannerRow->setVisible(false);
        card->addRow(m_serverBannerRow);
        v->addWidget(card);
    }

    // version & updates
    {
        auto *card = new GroupCard(QStringLiteral("version update check release mirror download"));
        m_herdrVersionLabel = new QLabel(page);
        const QString installed = m_core->herdrVersion();
        m_herdrVersionLabel->setText(installed.isEmpty()
            ? tr("Unknown") : QStringLiteral("v%1").arg(installed));
        m_herdrVersionLabel->setProperty("ver", true);
        auto *versionHolder = new QWidget(page);
        auto *vh = new QHBoxLayout(versionHolder);
        vh->setContentsMargins(0, 0, 0, 0);
        vh->addWidget(m_herdrVersionLabel);
        card->addRow(makeRow(tr("Current version"), versionHolder));

        m_herdrStatus = makeStatusLabel();
        m_herdrStatus->setObjectName(QStringLiteral("herdrStatusLabel"));
        auto *statusHolder = new QWidget(page);
        auto *sh = new QHBoxLayout(statusHolder);
        sh->setContentsMargins(0, 0, 0, 0);
        sh->setSpacing(10);
        sh->addWidget(m_herdrStatus, 1);
        m_herdrCheckButton = new QPushButton(tr("Check for updates"), page);
        sh->addWidget(m_herdrCheckButton);

        // "Software updates" caption with the muted last-check line under it
        auto *caption = new QWidget(page);
        auto *cv = new QVBoxLayout(caption);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(1);
        cv->addWidget(new QLabel(tr("Software updates"), caption));
        m_lastCheckLabel = new QLabel(caption);
        m_lastCheckLabel->setProperty("tone", QStringLiteral("dim"));
        refreshLastCheckLabel();
        cv->addWidget(m_lastCheckLabel);
        card->addRow(makeRow(caption, statusHolder));

        m_herdrUpdateButton = new QPushButton(page);
        m_herdrUpdateButton->setProperty("primary", true);
        m_herdrUpdateButton->setVisible(false);
        {
            auto *updateHolder = new QWidget(page);
            auto *uh = new QHBoxLayout(updateHolder);
            uh->setContentsMargins(16, 0, 16, 8);
            uh->addWidget(m_herdrUpdateButton);
            uh->addStretch();
            card->addRow(makeFullRow(updateHolder));
        }

        // Download source (mirror) settings
        QSettings settings = Platform::appSettings();
        m_mirrorCombo = new QComboBox(page);
        m_mirrorCombo->addItem(tr("Auto (recommended)"), "auto");
        m_mirrorCombo->addItem(tr("GitHub direct"), "direct");
        m_mirrorCombo->addItem(tr("Mirror first"), "mirror");
        const QString mode = settings.value("mirrorMode", "auto").toString();
        const int modeIndex = m_mirrorCombo->findData(mode);
        m_mirrorCombo->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
        m_mirrorCombo->setFixedWidth(CONTROL_WIDTH);
        card->addRow(makeRow(tr("Download source"), m_mirrorCombo));

        m_customMirrorEdit = new QLineEdit(page);
        m_customMirrorEdit->setPlaceholderText(QStringLiteral("https://your-mirror.example/"));
        m_customMirrorEdit->setText(settings.value("customMirrorUrl").toString());
        m_customMirrorEdit->setFixedWidth(CONTROL_WIDTH + 80);
        card->addRow(makeRow(tr("Custom mirror"), m_customMirrorEdit));
        v->addWidget(card);
    }

    // notifications & tray presence
    {
        auto *card = new GroupCard(QStringLiteral("notification notify attention idle dnd do not disturb tray"));
        QSettings settings = Platform::appSettings();

        m_agentNotifyCheck = new SwitchCheck(page);
        m_agentNotifyCheck->setChecked(settings.value("agentNotify", true).toBool());
        card->addRow(makeRow(tr("Notify when an agent needs attention"), m_agentNotifyCheck));

        m_agentNotifyIdleCheck = new SwitchCheck(page);
        m_agentNotifyIdleCheck->setChecked(settings.value("agentNotifyIdle", false).toBool());
        m_agentNotifyIdleCheck->setEnabled(m_agentNotifyCheck->isChecked());
        card->addRow(makeRow(tr("Also notify when an agent goes idle"), m_agentNotifyIdleCheck));

        m_dndCombo = new QComboBox(page);
        m_dndCombo->addItem(tr("Off"), QString());
        m_dndCombo->addItem(tr("Night (23:00 – 08:00)"), QStringLiteral("23:00-08:00"));
        const QString dnd = settings.value("dndWindow").toString();
        const int dndIndex = m_dndCombo->findData(dnd);
        m_dndCombo->setCurrentIndex(dndIndex >= 0 ? dndIndex : 0);
        m_dndCombo->setFixedWidth(CONTROL_WIDTH);
        m_dndCombo->setEnabled(m_agentNotifyCheck->isChecked());
        card->addRow(makeRow(tr("Do not disturb"),
                             m_dndCombo,
                             tr("During this window only the tray badge stays active")));

        m_closeToTrayCheck = new SwitchCheck(page);
        m_closeToTrayCheck->setChecked(settings.value("closeToTray", false).toBool());
        card->addRow(makeRow(tr("Keep running in the tray when the window is closed"), m_closeToTrayCheck));
        v->addWidget(card);
    }

    v->addStretch();

    auto updateServerAreaFn = [this]() { updateServerArea(); };
    updateServerArea();
    connect(m_core, &AppCore::serverStatusChanged, this, updateServerAreaFn);
    connect(m_serverRestartButton, &QPushButton::clicked, this, [this]() {
        m_core->requestServerRestart();
    });

    ReleaseUpdater *updater = m_core->herdrUpdater();
    connect(m_herdrCheckButton, &QPushButton::clicked, this, [this, updater]() {
        m_herdrStatus->setText(tr("Checking…"));
        setTone(m_herdrStatus, "dim");
        m_herdrUpdateButton->setVisible(false);
        updater->checkLatest();
    });
    connect(updater, &ReleaseUpdater::checkFinished, this,
            [this](bool ok, const ReleaseUpdater::Release &release, const QString &error) {
                refreshLastCheckLabel();
                if (!ok) {
                    m_herdrStatus->setText(tr("Check failed: %1").arg(error));
                    setTone(m_herdrStatus, "err");
                    return;
                }
                m_herdrLatest = release;
                const QString installed = m_core->herdrVersion();
                const bool hasUpdate = installed.isEmpty()
                    || ReleaseUpdater::compareVersions(release.version, installed) > 0;
                if (!hasUpdate) {
                    m_herdrStatus->setText(tr("herdr is up to date (v%1).").arg(release.version));
                    setTone(m_herdrStatus, "ok");
                    return;
                }
                m_herdrStatus->setText(tr("Update available: v%1 (current %2).")
                                           .arg(release.version, installed.isEmpty() ? tr("Unknown") : installed));
                setTone(m_herdrStatus, "dim");
                m_herdrUpdateButton->setText(tr("Update to v%1").arg(release.version));
                m_herdrUpdateButton->setVisible(true);
            });
    connect(m_herdrUpdateButton, &QPushButton::clicked, this, [this, updater]() {
        m_herdrUpdateButton->setEnabled(false);
        m_herdrStatus->setText(tr("Downloading…"));
        setTone(m_herdrStatus, "dim");
        updater->downloadAndInstall(m_herdrLatest);
    });
    connect(updater, &ReleaseUpdater::installProgress, this, [this](int percent) {
        m_herdrStatus->setText(tr("Downloading… %1%").arg(percent));
    });
    connect(updater, &ReleaseUpdater::installFinished, this,
            [this](bool ok, const QString &error) {
                m_herdrUpdateButton->setEnabled(true);
                m_herdrUpdateButton->setVisible(false);
                if (ok) {
                    m_herdrStatus->setText(tr("herdr updated. Restart the herdr server to apply."));
                    setTone(m_herdrStatus, "ok");
                } else {
                    m_herdrStatus->setText(tr("Update failed: %1").arg(error));
                    setTone(m_herdrStatus, "err");
                }
            });

    connect(m_agentNotifyCheck, &QAbstractButton::toggled, this, [this](bool on) {
        m_agentNotifyIdleCheck->setEnabled(on);
        m_dndCombo->setEnabled(on);
    });

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
    v->setSpacing(14);

    auto *title = new QLabel(tr("About & Updates"), page);
    title->setProperty("pageTitle", true);
    v->addWidget(title);

    // app identity & update
    {
        auto *card = new GroupCard(QStringLiteral("version app update check release about"));

        auto *identity = new QWidget(page);
        auto *ih = new QHBoxLayout(identity);
        ih->setContentsMargins(16, 12, 16, 12);
        ih->setSpacing(14);
        auto *icon = new QLabel(identity);
        icon->setPixmap(appIcon().pixmap(44, 44));
        icon->setFixedSize(44, 44);
        ih->addWidget(icon);
        auto *nameCol = new QVBoxLayout;
        nameCol->setContentsMargins(0, 0, 0, 0);
        nameCol->setSpacing(2);
        auto *nameRow = new QWidget(identity);
        auto *nrh = new QHBoxLayout(nameRow);
        nrh->setContentsMargins(0, 0, 0, 0);
        nrh->setSpacing(8);
        nrh->addWidget(new QLabel(tr("MuDi (牧笛)"), nameRow));
        nrh->addWidget(makeVerBadge(QStringLiteral("v" APP_VERSION)));
        nrh->addStretch();
        nameCol->addWidget(nameRow);
        auto *tagline = new QLabel(tr("Desktop cockpit for herdr"), identity);
        tagline->setProperty("tone", QStringLiteral("dim"));
        nameCol->addWidget(tagline);
        ih->addLayout(nameCol, 1);
        card->addRow(identity);

        m_appStatus = makeStatusLabel();
        m_appStatus->setObjectName(QStringLiteral("appStatusLabel"));
        auto *checkBtn = new QPushButton(tr("Check for updates"), page);
        m_releasesButton = new QPushButton(tr("Open releases page"), page);
        auto *buttons = new QWidget(page);
        auto *row = new QHBoxLayout(buttons);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);
        row->addWidget(m_appStatus, 1);
        row->addWidget(checkBtn);
        row->addWidget(m_releasesButton);
        card->addRow(makeFullRow(buttons));

        m_autoCheckCheck = new SwitchCheck(page);
        QSettings aboutSettings = Platform::appSettings();
        m_autoCheckCheck->setChecked(aboutSettings.value("autoCheckUpdates", true).toBool());
        card->addRow(makeRow(tr("Check for updates on startup"), m_autoCheckCheck));
        v->addWidget(card);

        ReleaseUpdater *appUpdater = m_core->appUpdater();
        connect(checkBtn, &QPushButton::clicked, this, [this, appUpdater]() {
            m_appStatus->setText(tr("Checking…"));
            setTone(m_appStatus, "dim");
            appUpdater->checkLatest();
        });
        connect(appUpdater, &ReleaseUpdater::checkFinished, this,
                [this](bool ok, const ReleaseUpdater::Release &release, const QString &error) {
                    if (!ok) {
                        m_appStatus->setText(tr("Check failed: %1").arg(error));
                        setTone(m_appStatus, "err");
                        return;
                    }
                    m_appLatest = release;
                    if (ReleaseUpdater::compareVersions(release.version, APP_VERSION) > 0) {
                        m_appStatus->setText(tr("App update available: v%1 (current %2).")
                                                 .arg(release.version, APP_VERSION));
                        setTone(m_appStatus, "dim");
                    } else {
                        m_appStatus->setText(tr("App is up to date (v%1).").arg(APP_VERSION));
                        setTone(m_appStatus, "ok");
                    }
                });
    }

    // logs, project links, license
    {
        auto *card = new GroupCard(QStringLiteral("log logs diagnostics github feedback license source"));

        m_openLogButton = new QPushButton(tr("Open log folder"), page);
        card->addRow(makeRow(tr("Run logs"),
                             m_openLogButton,
                             QDir::toNativeSeparators(Platform::logFilePath())));

        m_githubButton = new QPushButton(tr("GitHub ↗"), page);
        card->addRow(makeRow(tr("Project page / feedback"), m_githubButton));

        auto *license = new QLabel(QStringLiteral("GPL-3.0 · terminalwidget, see 3rdparty/"));
        license->setProperty("tone", QStringLiteral("dim"));
        card->addRow(makeRow(tr("License"), license));
        v->addWidget(card);
    }

    v->addStretch();

    ReleaseUpdater *updater = m_core->appUpdater();
    connect(m_releasesButton, &QPushButton::clicked, this, [this, updater]() {
        const QUrl url(!m_appLatest.htmlUrl.isEmpty()
            ? m_appLatest.htmlUrl
            : QUrl(updater->releasesPageUrl()));
        QDesktopServices::openUrl(url);
    });
    connect(m_githubButton, &QPushButton::clicked, this, [updater]() {
        QDesktopServices::openUrl(QUrl(updater->projectPageUrl()));
    });
    connect(m_openLogButton, &QPushButton::clicked, this, []() {
        const QFileInfo info(Platform::logFilePath());
        const QUrl dir = QUrl::fromLocalFile(info.absolutePath());
        QDesktopServices::openUrl(dir);
    });

    return page;
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
    QSettings store = Platform::appSettings();
    store.setValue("mirrorMode", m_mirrorCombo->currentData().toString());
    store.setValue("customMirrorUrl", m_customMirrorEdit->text().trimmed());
    store.setValue("autoCheckUpdates", m_autoCheckCheck->isChecked());
    store.setValue("agentNotify", m_agentNotifyCheck->isChecked());
    store.setValue("agentNotifyIdle", m_agentNotifyIdleCheck->isChecked());
    store.setValue("dndWindow", m_dndCombo->currentData().toString());
    store.setValue("closeToTray", m_closeToTrayCheck->isChecked());
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
    m_monoBannerRow->setVisible(!monospace);
}

void SettingsDialog::restoreDefaults()
{
    m_fontCombo->setCurrentFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_sizeSpinBox->setValue(DEFAULT_FONT_SIZE);
    m_cursorCombo->setCurrentIndex(0);
    m_blinkCheck->setChecked(false);
    m_scrollbackSpin->setValue(1000);
    m_autoCopyCheck->setChecked(true);

    m_themeKey = QStringLiteral("Auto");
    syncSwatches(m_themeKey);
    emit themeSelected(m_themeKey);

    m_opacitySlider->setValue(100);

    m_mirrorCombo->setCurrentIndex(0);
    m_customMirrorEdit->clear();
    m_agentNotifyCheck->setChecked(true);
    m_agentNotifyIdleCheck->setChecked(false);
    m_dndCombo->setCurrentIndex(0);
    m_closeToTrayCheck->setChecked(false);
    m_autoCheckCheck->setChecked(true);
}

void SettingsDialog::applySearch(const QString &text)
{
    const QString needle = text.trimmed();
    for (QWidget *card : m_cards) {
        card->setVisible(static_cast<GroupCard *>(card)->matches(needle));
    }
    if (needle.isEmpty()) {
        return;
    }
    // jump to the first page that still has something to show
    for (int i = 0; i < m_stack->count(); ++i) {
        QWidget *page = m_stack->widget(i);
        for (QWidget *card : page->findChildren<QWidget *>()) {
            if (GroupCard::isCard(card) && !card->isHidden()) {
                if (m_stack->currentIndex() != i) {
                    m_navList->blockSignals(true);
                    m_navList->setCurrentRow(i);
                    m_navList->blockSignals(false);
                    m_stack->setCurrentIndex(i);
                }
                return;
            }
        }
    }
}

void SettingsDialog::updateServerArea()
{
    const AppCore::ServerStatus st = m_core->serverStatus();
    QString text;
    bool problem = false;
    bool running = false;
    if (!st.known) {
        text = tr("Server status unknown");
    } else if (!st.running) {
        text = tr("Server not running");
    } else {
        running = true;
        text = tr("Server running");
        if (!st.protocolCompatible || st.restartNeeded || st.binaryStale) {
            problem = true;
        }
    }
    m_serverStatusLabel->setText(text);

    m_serverVerLabel->setVisible(running && !st.version.isEmpty());
    m_serverVerLabel->setText(st.version.isEmpty() ? QString() : QStringLiteral("v%1").arg(st.version));

    // dot: green when healthy, red on problems, gray otherwise
    const QString dotState = problem ? QStringLiteral("bad")
                           : running ? QStringLiteral("ok") : QStringLiteral("dim");
    m_serverDot->setProperty("dot", dotState);
    m_serverDot->style()->unpolish(m_serverDot);
    m_serverDot->style()->polish(m_serverDot);
    setTone(m_serverStatusLabel, problem ? "err" : (running ? "" : "dim"));

    if (problem) {
        m_serverBanner->setText(!st.protocolCompatible
            ? tr("The running server is protocol-incompatible with the installed herdr. Restart it to apply (all panes will be restarted).")
            : tr("A herdr update has been installed. Restart the server to apply (all panes will be restarted)."));
    }
    m_serverBannerRow->setVisible(problem);
}

void SettingsDialog::syncSwatches(const QString &key)
{
    int id = 0;
    if (key == QLatin1String("Light")) id = 1;
    else if (key == QLatin1String("Dark")) id = 2;
    for (int i = 0; i < m_themeSwatches->buttons().size(); ++i) {
        m_themeSwatches->button(i)->setChecked(i == id);
    }
}

void SettingsDialog::refreshLastCheckLabel()
{
    QSettings s = Platform::appSettings();
    const qint64 msecs = s.value("updater/lastCheckAt/herdr", 0).toLongLong();
    if (msecs <= 0) {
        m_lastCheckLabel->setText(tr("Never checked"));
        return;
    }
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(msecs);
    m_lastCheckLabel->setText(tr("Last checked: %1").arg(
        when.date() == QDate::currentDate()
            ? when.time().toString(QStringLiteral("HH:mm"))
            : when.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
}
