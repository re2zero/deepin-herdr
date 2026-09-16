#include "platform.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QStyle>
#include <QStyleHints>
#include <QTranslator>
#include <QVBoxLayout>

#if HAVE_DTK
#include <DApplication>
#include <DDialog>
#include <DGuiApplicationHelper>
#include <DProgressBar>
DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE
#endif

namespace Platform {

bool useDtk()
{
    static const bool cached = []() {
        const QByteArray force = qgetenv("DEEPIN_HERDR_UI");
        if (force == "generic") {
            return false;
        }
        if (force == "dtk") {
            return true;
        }
#if !HAVE_DTK
        return false;
#else
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        return false;
#else
        // freedesktop os-release: /etc/os-release, fallback /usr/lib/os-release
        for (const QString &path : {QStringLiteral("/etc/os-release"),
                                    QStringLiteral("/usr/lib/os-release")}) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            const QList<QByteArray> lines = file.readAll().split('\n');
            for (const QByteArray &line : lines) {
                if (!line.startsWith("ID=")) {
                    continue; // also skips ID_LIKE, per spec only ID counts
                }
                QString id = QString::fromLatin1(line.mid(3)).trimmed();
                id.remove(QLatin1Char('"'));
                return id == QLatin1String("deepin") || id == QLatin1String("uos");
            }
            break; // os-release exists but has no ID — don't try the fallback
        }
        return false;
#endif
#endif
    }();
    return cached;
}

void loadTranslations(QApplication *app)
{
#if HAVE_DTK
    if (useDtk()) {
        if (auto *dtkApp = qobject_cast<DApplication *>(app)) {
            dtkApp->loadTranslator();
            return;
        }
    }
#endif
    // generic flavor: install our own .qm files
    const QString locale = QLocale::system().name();
    const QStringList dirs = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/../share/deepin-herdr/translations"),
        QStringLiteral("/usr/share/deepin-herdr/translations"),
        QStringLiteral("translations"),
    };
    for (const QString &dir : dirs) {
        for (const QString &name : {locale, locale.left(2)}) {
            auto *translator = new QTranslator(app);
            if (translator->load(QStringLiteral("deepin-herdr_") + name, dir)) {
                app->installTranslator(translator);
                return;
            }
            translator->deleteLater();
        }
    }
}

QPalette genericDarkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(53, 53, 53));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    p.setColor(QPalette::Base, QColor(42, 42, 42));
    p.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    p.setColor(QPalette::Dark, QColor(35, 35, 35));
    p.setColor(QPalette::Shadow, QColor(20, 20, 20));
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    p.setColor(QPalette::PlaceholderText, QColor(127, 127, 127));
    return p;
}

void applyUiTheme(UiTheme theme)
{
    // captured on the first call, which happens before any override
    static const QString defaultStyle = QApplication::style()
        ? QApplication::style()->objectName() : QString();
    static const QPalette defaultPalette = QApplication::palette();

#if HAVE_DTK
    if (useDtk()) {
        auto *helper = DGuiApplicationHelper::instance();
        // Defer to the event loop: theme restoration runs while windows
        // are being constructed, and dxcb reconfigures its frame on
        // palette changes — never do that mid-init.
        QTimer::singleShot(0, helper, [helper, theme]() {
            switch (theme) {
            case UiTheme::Light:
                helper->setPaletteType(DGuiApplicationHelper::LightType);
                break;
            case UiTheme::Dark:
                helper->setPaletteType(DGuiApplicationHelper::DarkType);
                break;
            case UiTheme::Auto:
                helper->setPaletteType(DGuiApplicationHelper::UnknownType);
                break;
            }
        });
        return;
    }
#endif
    // generic flavor. Note: on a deepin system the dtkgui platform theme
    // hijacks application palettes and resists overrides — this path is
    // meant for systems without the deepin stack.
    bool dark = (theme == UiTheme::Dark);
    if (theme == UiTheme::Auto) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        dark = false;
#endif
    }
    if (dark) {
        QApplication::setStyle(QStringLiteral("Fusion"));
        QApplication::setPalette(genericDarkPalette());
    } else {
        if (!defaultStyle.isEmpty()) {
            QApplication::setStyle(defaultStyle);
        }
        QApplication::setPalette(defaultPalette);
    }
}

void moveToCenter(QWidget *window)
{
    if (!window) {
        return;
    }
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    window->move(screen.center() - QRect(QPoint(0, 0), window->frameGeometry().size()).center());
}

void showErrorDialog(QWidget *parent, const QString &title, const QString &message)
{
#if HAVE_DTK
    if (useDtk()) {
        auto *dlg = new DDialog(parent);
        dlg->setTitle(title);
        dlg->setMessage(message);
        dlg->addButton(QObject::tr("OK"));
        dlg->exec();
        dlg->deleteLater();
        return;
    }
#endif
    QMessageBox::critical(parent, title, message, QMessageBox::Ok);
}

void showContentDialog(QWidget *content, const QString &title, QWidget *parent,
                       const std::function<void()> &onPersist)
{
#if HAVE_DTK
    if (useDtk()) {
        auto *dlg = new DDialog(parent);
        // no DDialog heading: content pages carry their own titles
        dlg->addContent(content); // dialog takes ownership
        dlg->addButton(QObject::tr("OK"));
        dlg->setCloseButtonVisible(true);
        QObject::connect(dlg, &DDialog::closed, dlg, [dlg, onPersist] {
            onPersist();
            dlg->deleteLater();
        });
        dlg->show();
        return;
    }
#endif
    auto *dlg = new QDialog(parent);
    dlg->setWindowTitle(title);
    auto *layout = new QVBoxLayout(dlg);
    layout->addWidget(content);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    QObject::connect(dlg, &QDialog::finished, dlg, [dlg, onPersist](int) {
        onPersist();
        dlg->deleteLater();
    });
    dlg->show();
}

// ---- ProgressDialog ----

struct ProgressDialog::Impl : public QObject {
    Impl(ProgressDialog *outer, QWidget *parent)
        : QObject(outer)
        , q(outer)
        , parentWidget(parent)
    {
    }
    ProgressDialog *q;
    QWidget *parentWidget;
    virtual void setMessage(const QString &message) = 0;
    virtual void setProgress(int percent) = 0;
    virtual void show() = 0;
    virtual void close() = 0;
};

#if HAVE_DTK
struct ProgressDialog::ImplDtk final : public ProgressDialog::Impl {
    ImplDtk(ProgressDialog *outer, QWidget *parent)
        : Impl(outer, parent)
    {
        dlg = new DDialog(parentWidget);
        bar = new DProgressBar(dlg);
        bar->setRange(0, 100);
        bar->setFixedWidth(300);
        dlg->addContent(bar);
        // any close path (cancel button, window close) counts as cancelled
        QObject::connect(dlg, &DDialog::closed, this, [this] {
            emit q->cancelled();
        });
    }
    ~ImplDtk() override { if (dlg) dlg->deleteLater(); }
    void setMessage(const QString &message) override { if (dlg) dlg->setMessage(message); }
    void setProgress(int percent) override { if (bar) bar->setValue(percent); }
    void show() override { if (dlg) dlg->show(); }
    void close() override { if (dlg) dlg->close(); }
    QPointer<DDialog> dlg;
    QPointer<DProgressBar> bar;
};
#endif

struct ProgressDialog::ImplGeneric final : public ProgressDialog::Impl {
    ImplGeneric(ProgressDialog *outer, QWidget *parent)
        : Impl(outer, parent)
    {
        dlg = new QDialog(parentWidget);
        dlg->setWindowTitle(QObject::tr("Installing herdr"));
        auto *layout = new QVBoxLayout(dlg);
        label = new QLabel(dlg);
        label->setWordWrap(true);
        layout->addWidget(label);
        bar = new QProgressBar(dlg);
        bar->setRange(0, 100);
        layout->addWidget(bar);
        auto *cancel = new QPushButton(QObject::tr("Cancel"), dlg);
        layout->addWidget(cancel);
        QObject::connect(cancel, &QPushButton::clicked, dlg, &QDialog::close);
        QObject::connect(dlg, &QDialog::finished, this, [this](int) {
            emit q->cancelled();
        });
    }
    ~ImplGeneric() override { if (dlg) dlg->deleteLater(); }
    void setMessage(const QString &message) override
    {
        if (label) label->setText(message);
        if (dlg) dlg->setWindowTitle(message.section(QLatin1Char('\n'), 0, 0));
    }
    void setProgress(int percent) override { if (bar) bar->setValue(percent); }
    void show() override { if (dlg) dlg->show(); }
    void close() override { if (dlg) dlg->close(); }
    QPointer<QDialog> dlg;
    QPointer<QLabel> label;
    QPointer<QProgressBar> bar;
};

ProgressDialog::ProgressDialog(QWidget *parent)
    : QObject(parent)
{
#if HAVE_DTK
    if (useDtk()) {
        m_d = new ImplDtk(this, parent);
        return;
    }
#endif
    m_d = new ImplGeneric(this, parent);
}

ProgressDialog::~ProgressDialog() = default;

void ProgressDialog::setMessage(const QString &message) { m_d->setMessage(message); }
void ProgressDialog::setProgress(int percent) { m_d->setProgress(percent); }
void ProgressDialog::show() { m_d->show(); }
void ProgressDialog::close() { m_d->close(); }

} // namespace Platform
