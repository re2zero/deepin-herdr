#ifndef SETTINGSSTYLE_H
#define SETTINGSSTYLE_H

#include <QAbstractButton>
#include <QIcon>
#include <QString>

class QPalette;

// Design tokens and painting helpers for the settings UI. Everything is
// derived from the running palette (light/dark detected via window
// lightness) so the same stylesheet works in the DTK shell, the generic
// Qt shell, and on macOS/Windows — no DTK headers, no SVG plugin.
//
// Tokens follow docs/design/settings-redesign.html (DTK language:
// #0081FF accent, 8px control corners, 12px group cards, pill switches).
namespace SettingsStyle {

bool isDark(const QPalette &palette);

// Full stylesheet for the settings dialog (scoped to the widget it is
// set on). Rebuild and re-apply on QEvent::PaletteChange.
QString baseQss(bool dark);

enum class IconKind { Terminal, Appearance, Herdr, About, Reset };

// Line icon drawn with QPainter (32px @2x for crisp HiDPI). One pixmap
// per state: neutral for normal/hover, the given selectedColor for the
// highlighted nav item.
QIcon navIcon(IconKind kind, const QColor &color, const QColor &selectedColor);

// Thumbnail for the theme swatch cards (Follow system / Light / Dark).
enum class SwatchKind { Auto, Light, Dark };
QPixmap themeSwatch(SwatchKind kind, const QColor &accent);

// Pill switch used for every boolean row. Pure QPainter — no QSS
// internals, repaints itself on palette (theme) changes.
class SwitchCheck : public QAbstractButton {
    Q_OBJECT
public:
    explicit SwitchCheck(QWidget *parent = nullptr);

    QSize sizeHint() const override { return {40, 22}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
};

} // namespace SettingsStyle

#endif // SETTINGSSTYLE_H
