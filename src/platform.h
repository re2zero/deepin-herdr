#ifndef PLATFORM_H
#define PLATFORM_H

#include <QApplication>
#include <functional>

class QWidget;

// Runtime/build-time UI flavor selection.
//
// DTK is only used when the binary was built with DTK support (CMake
// HAVE_DTK) AND the running OS is deepin/UOS (/etc/os-release ID).
// Everything else — other distros, Windows, macOS — uses the generic Qt
// implementations. DEEPIN_HERDR_UI=dtk|generic overrides detection for
// testing.
namespace Platform {

bool useDtk();

// Application UI theme (menus, dialogs) — distinct from the terminal
// color scheme. Light/Dark force the palette, Auto follows the system.
enum class UiTheme { Light, Dark, Auto };
void applyUiTheme(UiTheme theme);

// QApplication translation bootstrap for the current flavor.
void loadTranslations(QApplication *app);

// Center a (not yet shown) window on its screen.
void moveToCenter(QWidget *window);

// Modal message box in the current flavor. Blocks until dismissed.
void showErrorDialog(QWidget *parent, const QString &title, const QString &message);

// Hosts a settings-style content widget in a dialog with an OK button.
// onPersist runs when the dialog closes (either way), then everything
// is deleted. The content widget must be created without a parent or
// with this dialog-intended parent; ownership transfers to the dialog.
void showContentDialog(QWidget *content, const QString &title, QWidget *parent,
                       const std::function<void()> &onPersist);

// Non-modal progress dialog facade used by the first-run installer.
// Backed by DDialog on DTK, plain QDialog elsewhere.
class ProgressDialog : public QObject {
    Q_OBJECT
public:
    explicit ProgressDialog(QWidget *parent);
    ~ProgressDialog() override;

    void setMessage(const QString &message);
    void setProgress(int percent); // 0..100
    void show();
    void close();

signals:
    // fires once when the dialog goes away (cancel button or close)
    void cancelled();

private:
    struct Impl; // QObject, parented to this
#if HAVE_DTK
    struct ImplDtk;
#endif
    struct ImplGeneric;
    Impl *m_d = nullptr;
};

} // namespace Platform

#endif // PLATFORM_H
