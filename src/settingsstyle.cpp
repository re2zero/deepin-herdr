#include "settingsstyle.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>

namespace SettingsStyle {

// Accent constants shared by the stylesheet and the painters. The DTK
// palette highlight is #0081FF; semantic red matches the values already
// used by the app's status text.
static const char *kAccent = "#0081FF";

bool isDark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

static QString qrgba(int r, int g, int b, int a255)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(r).arg(g).arg(b).arg(a255);
}

// Per-theme token set, mirrored from the HTML design. Text colors use
// the alpha ladder 90/55/35% familiar from the DTK palette.
struct Tokens {
    QString winBg, card, line, ctlLine, ctlBg, ctlHover, navHover;
    QString t1, t2, t3, warn, warnBg, danger, dangerBg, ok;

    static Tokens make(bool dark)
    {
        Tokens t;
        if (!dark) {
            t.winBg   = "#F6F6F7";
            t.card    = "#FFFFFF";
            t.line    = qrgba(0, 0, 0, 15);
            t.ctlLine = qrgba(0, 0, 0, 26);
            t.ctlBg   = "#FFFFFF";
            t.ctlHover= qrgba(0, 0, 0, 13);
            t.navHover= qrgba(0, 0, 0, 15);
            t.t1 = qrgba(0, 0, 0, 230);
            t.t2 = qrgba(0, 0, 0, 140);
            t.t3 = qrgba(0, 0, 0, 90);
            t.warn   = "#B26A00";
            t.warnBg = "#FFF8E6";
            t.danger = "#F54A45";
            t.dangerBg = "#FFEEEB";
            t.ok = "#34C724";
        } else {
            t.winBg   = "#212124";
            t.card    = "#2A2A2E";
            t.line    = qrgba(255, 255, 255, 18);
            t.ctlLine = qrgba(255, 255, 255, 31);
            t.ctlBg   = "#2E2E32";
            t.ctlHover= qrgba(255, 255, 255, 20);
            t.navHover= qrgba(255, 255, 255, 20);
            t.t1 = qrgba(255, 255, 255, 230);
            t.t2 = qrgba(255, 255, 255, 140);
            t.t3 = qrgba(255, 255, 255, 90);
            t.warn   = "#FFB906";
            t.warnBg = qrgba(255, 185, 6, 36);
            t.danger = "#F54A45";
            t.dangerBg = qrgba(245, 74, 69, 41);
            t.ok = "#34C724";
        }
        return t;
    }
};

QString baseQss(bool dark)
{
    const Tokens c = Tokens::make(dark);

    QString qss = QStringLiteral(
        // ---- shell ----
        "SettingsDialog { background: %winBg; }"
        "SettingsDialog QScrollArea { background: transparent; border: none; }"
        "SettingsDialog QScrollArea > QWidget > QWidget { background: transparent; }"
        "SettingsDialog QStackedWidget { background: transparent; }"
        // page title
        "QLabel[pageTitle=\"true\"] { color: %t1; font-size: 17px; font-weight: 600; }"
        // ---- group cards ----
        "QWidget[card=\"true\"] { background: %card; border: 1px solid %line; border-radius: 12px; }"
        "QWidget[srow=\"true\"] { background: transparent; border: none; border-bottom: 1px solid %line; }"
        "QWidget[srow=\"true\"][lastRow=\"true\"] { border-bottom: none; }"
        "QWidget[srow=\"true\"] QLabel { background: transparent; }"
        // ---- status bits ----
        "QLabel[ver=\"true\"] { color: %t2; background: %ctlHover; border-radius: 4px; padding: 1px 6px; }"
        "QFrame[dot=\"ok\"] { background: %ok; border-radius: 4px; }"
        "QFrame[dot=\"bad\"] { background: %danger; border-radius: 4px; }"
        "QFrame[dot=\"dim\"] { background: %ctlLineStrong; border-radius: 4px; }"
        "QLabel[tone=\"dim\"] { color: %t2; font-size: 11.5px; }"
        "QLabel[tone=\"ok\"] { color: %ok; font-size: 12.5px; }"
        "QLabel[tone=\"err\"] { color: %danger; font-size: 12.5px; }"
        // banners
        "QFrame[banner=\"warn\"] { background: %warnBg; border-radius: 8px; }"
        "QFrame[banner=\"err\"] { background: %dangerBg; border-radius: 8px; }"
        "QFrame[banner=\"warn\"] QLabel, QFrame[banner=\"err\"] QLabel { background: transparent; border: none; }"
        "QLabel[bannerText=\"warn\"] { color: %warn; font-size: 12.5px; }"
        "QLabel[bannerText=\"err\"] { color: %danger; font-size: 12.5px; }"
        // ---- controls ----
        "QComboBox, QFontComboBox, QSpinBox, QLineEdit {"
        "  background: %ctlBg; border: 1px solid %ctlLine; border-radius: 8px;"
        "  padding: 2px 10px; min-height: 24px; color: %t1; selection-background-color: %accent; }"
        "QComboBox:hover, QFontComboBox:hover, QSpinBox:hover, QLineEdit:hover { border-color: %ctlLineStrong; }"
        "QComboBox:focus, QFontComboBox:focus, QSpinBox:focus, QLineEdit:focus { border-color: %accent; }"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QComboBox::down-arrow { image: none; width: 0; height: 0;"
        "  border-left: 4px solid transparent; border-right: 4px solid transparent;"
        "  border-top: 5px solid %t2; margin-top: 1px; }"
        "QComboBox QAbstractItemView { background: %card; color: %t1; border: 1px solid %line;"
        "  border-radius: 10px; padding: 4px; selection-background-color: %ctlHover; selection-color: %t1; outline: 0; }"
        "QSpinBox::up-button, QSpinBox::down-button { background: transparent; border: none; width: 20px; }"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: %ctlHover; border-radius: 4px; }"
        "QSpinBox::up-arrow { image: none; width: 0; height: 0;"
        "  border-left: 4px solid transparent; border-right: 4px solid transparent;"
        "  border-bottom: 5px solid %t2; }"
        "QSpinBox::down-arrow { image: none; width: 0; height: 0;"
        "  border-left: 4px solid transparent; border-right: 4px solid transparent;"
        "  border-top: 5px solid %t2; }"
        "QLineEdit { padding: 2px 10px; }"
        // buttons
        "QPushButton { background: %ctlBg; border: 1px solid %ctlLine; border-radius: 8px;"
        "  padding: 4px 16px; min-height: 22px; color: %t1; }"
        "QPushButton:hover { background: %ctlHover; }"
        "QPushButton:pressed { background: %navHover; }"
        "QPushButton:disabled { color: %t3; }"
        "QPushButton[primary=\"true\"] { background: %accent; border: none; color: white; }"
        "QPushButton[primary=\"true\"]:hover { background: %accentHover; }"
        "QPushButton[primary=\"true\"]:pressed { background: %accentActive; }"
        "QPushButton[flat=\"true\"] { background: transparent; border: none; color: %t2; }"
        "QPushButton[flat=\"true\"]:hover { background: %navHover; color: %t1; }"
        // theme swatch cards
        "QToolButton[swatch=\"true\"] { background: %card; border: 1px solid %ctlLine;"
        "  border-radius: 8px; padding: 6px 10px 4px; color: %t2; font-size: 12px; }"
        "QToolButton[swatch=\"true\"]:hover { border-color: %ctlLineStrong; }"
        "QToolButton[swatch=\"true\"]:checked { border: 2px solid %accent; color: %accent; }"
        // slider (terminal background opacity)
        "QSlider { min-height: 22px; }"
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: %sliderRest; }"
        "QSlider::sub-page:horizontal { height: 4px; border-radius: 2px; background: %accent; }"
        "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;"
        "  background: %card; border: 0.5px solid %ctlLine; }"
        // nav list
        "QListWidget { background: transparent; border: none; outline: 0; font-size: 14px; }"
        "QListWidget::item { padding: 8px 12px; margin: 2px 0; border-radius: 8px; color: %t1; }"
        "QListWidget::item:hover { background: %navHover; }"
        "QListWidget::item:selected { background: %accent; color: white; }"
        // search field in the nav pane
        "QLineEdit[search=\"true\"] { border-radius: 8px; }"
        // thin scrollbars
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %scrollThumb; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
        );

    const QString ctlLineStrong = dark ? qrgba(255, 255, 255, 64) : qrgba(0, 0, 0, 51);
    const QString sliderRest = dark ? qrgba(255, 255, 255, 56) : qrgba(0, 0, 0, 26);
    const QString scrollThumb = dark ? qrgba(255, 255, 255, 72) : qrgba(0, 0, 0, 60);

    // Longer tokens first: %accent would otherwise corrupt %accentHover,
    // %warnBg, %dangerBg, %ctlLine and friends share that trap.
    struct Pair { const char *token; QString value; };
    const QList<Pair> subs = {
        {"%accentHover", QStringLiteral("#339CFF")},
        {"%accentActive", QStringLiteral("#0068CE")},
        {"%ctlLineStrong", ctlLineStrong},
        {"%sliderRest", sliderRest},
        {"%scrollThumb", scrollThumb},
        {"%dangerBg", c.dangerBg},
        {"%warnBg", c.warnBg},
        {"%ctlLine", c.ctlLine},
        {"%accent", QLatin1String(kAccent)},
        {"%danger", c.danger},
        {"%navHover", c.navHover},
        {"%ctlHover", c.ctlHover},
        {"%winBg", c.winBg},
        {"%warn", c.warn},
        {"%t3", c.t3},
        {"%t2", c.t2},
        {"%t1", c.t1},
        {"%ok", c.ok},
        {"%card", c.card},
        {"%line", c.line},
        {"%ctlBg", c.ctlBg},
    };
    for (const Pair &pair : subs) {
        qss.replace(QLatin1String(pair.token), pair.value);
    }
    return qss;
}

// ---- icons ----

static QPixmap drawIcon(IconKind kind, const QColor &color, int logical)
{
    QPixmap pm(logical * 2, logical * 2); // 2x for HiDPI crispness
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(2.0, 2.0); // paint in logical coordinates
    p.setPen(QPen(color, 1.6));
    p.setBrush(Qt::NoBrush);

    const QRectF r(1, 1, logical - 2, logical - 2);

    switch (kind) {
    case IconKind::Terminal:
        p.drawRoundedRect(r, 2.5, 2.5);
        {
            QPainterPath chevron;
            chevron.moveTo(5.0, 7.5);
            chevron.lineTo(7.5, 9.5);
            chevron.lineTo(5.0, 11.5);
            p.drawPath(chevron);
            QPen pen = p.pen();
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.drawLine(QPointF(9.5, 11.5), QPointF(13.5, 11.5));
        }
        break;
    case IconKind::Appearance: {
        // crescent moon: full circle minus an offset circle
        QPainterPath full, bite;
        full.addEllipse(QPointF(9.5, 9.5), 6.5, 6.5);
        bite.addEllipse(QPointF(12.5, 7.0), 5.6, 5.6);
        p.fillPath(full.subtracted(bite), color);
        break;
    }
    case IconKind::Herdr: {
        const QPointF c(9.5, 9.5);
        p.drawEllipse(c, 2.4, 2.4);
        p.drawEllipse(c, 6.6, 6.6);
        p.drawLine(QPointF(9.5, 2.9), QPointF(9.5, 7.1));
        p.drawLine(QPointF(9.5, 11.9), QPointF(9.5, 16.1));
        p.drawLine(QPointF(2.9, 9.5), QPointF(7.1, 9.5));
        p.drawLine(QPointF(11.9, 9.5), QPointF(16.1, 9.5));
        break;
    }
    case IconKind::About:
        p.drawEllipse(QPointF(9.5, 9.5), 6.6, 6.6);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(9.5, 6.4), 0.9, 0.9);
        p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(9.5, 9.0), QPointF(9.5, 12.8));
        break;
    case IconKind::Reset: {
        // circular arrow (restore defaults)
        QPainterPath arc;
        arc.moveTo(14.8, 6.2);
        arc.arcTo(QRectF(3.2, 3.2, 12.6, 12.6), 40, 300);
        p.drawPath(arc);
        QPainterPath head;
        head.moveTo(14.9, 2.9);
        head.lineTo(15.2, 6.6);
        head.lineTo(11.6, 6.9);
        head.closeSubpath();
        p.fillPath(head, color);
        break;
    }
    }
    return pm;
}

QIcon navIcon(IconKind kind, const QColor &color, const QColor &selectedColor)
{
    QIcon icon;
    icon.addPixmap(drawIcon(kind, color, 18), QIcon::Normal);
    icon.addPixmap(drawIcon(kind, selectedColor, 18), QIcon::Selected);
    return icon;
}

QPixmap themeSwatch(SwatchKind kind, const QColor &accent)
{
    QPixmap pm(120, 64); // 2x of 60x32
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(0.5, 0.5, pm.width() - 1, pm.height() - 1, 10, 10);
    p.setClipPath(clip);
    p.setPen(Qt::NoPen);

    const QColor lightBg(246, 246, 247);
    const QColor lightCard(255, 255, 255);
    const QColor darkBg(33, 33, 36);
    const QColor darkCard(46, 46, 50);

    if (kind == SwatchKind::Light) {
        p.fillRect(pm.rect(), lightBg);
        p.setBrush(lightCard);
        p.drawRoundedRect(16, 12, pm.width() - 32, pm.height() - 24, 4, 4);
    } else if (kind == SwatchKind::Dark) {
        p.fillRect(pm.rect(), darkBg);
        p.setBrush(darkCard);
        p.drawRoundedRect(16, 12, pm.width() - 32, pm.height() - 24, 4, 4);
    } else { // Auto: diagonal split light/dark
        p.fillRect(pm.rect(), lightBg);
        QPainterPath darkSide;
        darkSide.moveTo(pm.width(), 0);
        darkSide.lineTo(pm.width(), pm.height());
        darkSide.lineTo(0, pm.height());
        darkSide.closeSubpath();
        p.fillPath(darkSide, darkBg);
        QPen split(QColor(128, 128, 128, 160), 1.0);
        p.setPen(split);
        p.drawLine(0, pm.height(), pm.width(), 0);
    }
    p.end();
    pm.setDevicePixelRatio(2.0);
    return pm;
}

// ---- SwitchCheck ----

SwitchCheck::SwitchCheck(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::ClickFocus);
}

void SwitchCheck::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool dark = isDark(palette());
    const qreal trackH = 20.0;
    const qreal trackW = 38.0;
    const qreal x = (width() - trackW) / 2.0;
    const qreal y = (height() - trackH) / 2.0;
    const QRectF track(x, y, trackW, trackH);

    QColor off = dark ? QColor(255, 255, 255, 51) : QColor(0, 0, 0, 36);
    if (!isEnabled()) {
        off.setAlpha(off.alpha() / 2);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? QColor(kAccent) : off);
    p.drawRoundedRect(track, trackH / 2, trackH / 2);

    // knob
    const qreal knob = 16.0;
    const qreal kx = isChecked() ? track.right() - knob - 2.0 : track.left() + 2.0;
    const QRectF knobRect(kx, y + (trackH - knob) / 2.0, knob, knob);
    QColor ball = dark ? QColor(230, 230, 230) : Qt::white;
    if (!isEnabled()) {
        ball.setAlpha(140);
    }
    p.setBrush(ball);
    p.drawEllipse(knobRect);
}

void SwitchCheck::changeEvent(QEvent *event)
{
    // theme switch flips the track color — repaint
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::EnabledChange) {
        update();
    }
    QAbstractButton::changeEvent(event);
}

} // namespace SettingsStyle
