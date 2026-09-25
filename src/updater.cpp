#include "updater.h"
#include "platform.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QSharedPointer>
#include <QSysInfo>
#include <QTimer>

namespace {

constexpr const char *GITHUB_BASE = "https://github.com";
constexpr const char *GITHUB_API = "https://api.github.com";

// ghproxy-style prefixes that prepend the full GitHub URL. The list is
// tried in order; the first one that answers the probe is cached in
// QSettings so later downloads skip the race.
const char *BUILTIN_MIRRORS[] = {
    "https://ghfast.top/",
    "https://gh-proxy.com/",
};

constexpr int MIRROR_PROBE_TIMEOUT_MS = 3000;
constexpr const char *MIRROR_CACHE_KEY = "updater/lastGoodMirror";

QNetworkRequest makeRequest(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, "mudi-updater");
    req.setTransferTimeout(15000);
    return req;
}

// Leading numeric segment: "10" -> 10, "0b3" -> 0.
int leadingNumber(const QString &s)
{
    int n = 0;
    for (int i = 0; i < s.size() && s.at(i).isDigit(); ++i) {
        n = n * 10 + s.at(i).digitValue();
    }
    return n;
}

} // namespace

ReleaseUpdater::ReleaseUpdater(const QString &apiPath, const QString &assetPrefix,
                               const QString &installName, QObject *parent)
    : QObject(parent)
    , m_apiPath(apiPath)
    , m_assetPrefix(assetPrefix)
    , m_installName(installName)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString ReleaseUpdater::platformAssetName(const QString &prefix)
{
#if defined(Q_OS_WIN)
    return prefix + "-windows-x86_64.zip"; // windows assets are x86_64-only today
#elif defined(Q_OS_MACOS)
    QString arch = (QSysInfo::buildCpuArchitecture() == "arm64") ? "aarch64" : "x86_64";
    return prefix + "-macos-" + arch;
#else
    QString arch = (QSysInfo::buildCpuArchitecture() == "arm64") ? "aarch64" : "x86_64";
    return prefix + "-linux-" + arch;
#endif
}

QString ReleaseUpdater::releasesPageUrl() const
{
    // "repos/OWNER/NAME/releases/latest" -> "https://github.com/OWNER/NAME/releases"
    QString repo = m_apiPath;
    if (repo.startsWith(QStringLiteral("repos/"))) {
        repo.remove(0, 6);
    }
    const int latest = repo.indexOf(QStringLiteral("/releases/latest"));
    if (latest > 0) {
        repo.truncate(latest);
    }
    return QString(GITHUB_BASE) + "/" + repo + "/releases";
}

QString ReleaseUpdater::projectPageUrl() const
{
    // "repos/OWNER/NAME/releases/latest" -> "https://github.com/OWNER/NAME"
    QString repo = m_apiPath;
    if (repo.startsWith(QStringLiteral("repos/"))) {
        repo.remove(0, 6);
    }
    const int latest = repo.indexOf(QStringLiteral("/releases/latest"));
    if (latest > 0) {
        repo.truncate(latest);
    }
    return QString(GITHUB_BASE) + "/" + repo;
}

int ReleaseUpdater::compareVersions(const QString &a, const QString &b)
{
    const QStringList as = a.split('.');
    const QStringList bs = b.split('.');
    const int n = qMax(as.size(), bs.size());
    for (int i = 0; i < n; ++i) {
        const int av = (i < as.size()) ? leadingNumber(as.at(i)) : 0;
        const int bv = (i < bs.size()) ? leadingNumber(bs.at(i)) : 0;
        if (av != bv) {
            return (av < bv) ? -1 : 1;
        }
    }
    return 0;
}

QString strippedVersion(const QString &tagName)
{
    QString v = tagName;
    while (v.startsWith(QLatin1Char('v')) || v.startsWith(QLatin1Char('V'))) {
        v.remove(0, 1);
    }
    return v;
}

void ReleaseUpdater::checkLatest()
{
    // remember when a check happened (shown in settings; M9 throttle
    // will build on this key)
    Platform::appSettings().setValue(
        QStringLiteral("updater/lastCheckAt/") + m_installName,
        QDateTime::currentMSecsSinceEpoch());

    auto *reply = m_nam->get(makeRequest(QUrl(QString(GITHUB_API) + "/" + m_apiPath)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        Release release;
        if (reply->error() != QNetworkReply::NoError) {
            emitCheckResult(false, release, reply->errorString());
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            emitCheckResult(false, release, QStringLiteral("invalid release metadata"));
            return;
        }

        const QJsonObject obj = doc.object();
        release.tagName = obj.value("tag_name").toString();
        release.version = strippedVersion(release.tagName);
        release.htmlUrl = obj.value("html_url").toString();

        const QString want = platformAssetName(m_assetPrefix);
        const QJsonArray assets = obj.value("assets").toArray();
        for (const QJsonValue &v : assets) {
            const QJsonObject asset = v.toObject();
            if (asset.value("name").toString() == want) {
                release.assetUrl = asset.value("browser_download_url").toString();
                const QString digest = asset.value("digest").toString(); // "sha256:<hex>"
                if (digest.startsWith(QStringLiteral("sha256:"))) {
                    release.sha256 = digest.mid(7);
                }
                break;
            }
        }

        if (release.version.isEmpty() || release.assetUrl.isEmpty()) {
            emitCheckResult(false, release,
                            QStringLiteral("no release asset for this platform (%1)").arg(want));
            return;
        }
        emitCheckResult(true, release, {});
    });
}

void ReleaseUpdater::emitCheckResult(bool ok, const Release &release, const QString &error)
{
    emit checkFinished(ok, release, error);
}

QStringList ReleaseUpdater::candidateUrls(const QString &directUrl) const
{
    QStringList prefixes;
    if (!m_customMirror.trimmed().isEmpty()) {
        prefixes << m_customMirror.trimmed();
    }
    for (const char *m : BUILTIN_MIRRORS) {
        prefixes << QString::fromLatin1(m);
    }

    QStringList urls;
    urls << directUrl;
    for (const QString &p : prefixes) {
        QString prefix = p;
        if (!prefix.endsWith(QLatin1Char('/'))) {
            prefix += QLatin1Char('/');
        }
        urls << prefix + directUrl;
    }
    return urls;
}

void ReleaseUpdater::downloadAndInstall(const Release &release)
{
    if (isDownloading()) {
        return;
    }
#ifdef Q_OS_WIN
    // The windows asset is a zip; its install flow lands with the M4 port.
    Q_UNUSED(release);
    emit installFinished(false, tr("Automatic install is not supported on this platform yet; "
                                   "please download and install manually."));
    return;
#endif
    if (release.sha256.isEmpty()) {
        // Fail closed: without the API-provided digest we cannot verify
        // what a mirror served us.
        emit installFinished(false, tr("Release metadata has no SHA-256 digest; "
                                       "refusing to install."));
        return;
    }

    m_currentRelease = release;
    m_lastError.clear();
    resolveOrder(release.assetUrl);
}

// Decide the mirror order, then run the chain. In Auto mode a cached
// winner short-circuits the probe; otherwise race with 1-byte GETs.
void ReleaseUpdater::resolveOrder(const QString &directUrl)
{
    const QStringList all = candidateUrls(directUrl);

    if (m_mirrorMode == MirrorMode::DirectFirst) {
        startDownloadChain(all);
        return;
    }
    if (m_mirrorMode == MirrorMode::MirrorFirst) {
        QStringList mirrorsFirst;
        for (int i = 1; i < all.size(); ++i) {
            mirrorsFirst << all.at(i);
        }
        mirrorsFirst << directUrl;
        startDownloadChain(mirrorsFirst);
        return;
    }

    QSettings settings = Platform::appSettings();
    const QString cached = settings.value(MIRROR_CACHE_KEY).toString();
    if (!cached.isEmpty()) {
        QStringList ordered;
        const QString directPrefix = QString(GITHUB_BASE) + "/";
        if (cached == directPrefix) {
            ordered << directUrl;
            for (int i = 1; i < all.size(); ++i) {
                ordered << all.at(i);
            }
        } else {
            // cached is a mirror prefix; put its URL first
            ordered << cached + directUrl;
            ordered << directUrl;
            for (int i = 1; i < all.size(); ++i) {
                if (all.at(i) != cached + directUrl) {
                    ordered << all.at(i);
                }
            }
        }
        startDownloadChain(ordered);
        return;
    }

    // Probe every candidate with a 1-byte range request; first HTTP
    // success wins. Abort the rest and cache the winner. Shared pointers
    // keep the race state alive across abort-triggered finished signals.
    struct Probe {
        QNetworkReply *reply = nullptr;
        QTimer *timer = nullptr;
        QString prefix;
    };
    auto probes = QSharedPointer<QList<Probe>>::create();
    auto done = QSharedPointer<bool>::create(false);
    auto cleanup = [probes]() {
        for (const Probe &p : *probes) {
            if (p.reply) {
                p.reply->abort();
            }
            if (p.timer) {
                p.timer->stop();
            }
        }
    };

    const QString directPrefix = QString(GITHUB_BASE) + "/";
    QStringList prefixes;
    prefixes << directPrefix;
    for (int i = 1; i < all.size(); ++i) {
        // mirror candidate URL = all[i]; recover its prefix by trimming directUrl
        const QString u = all.at(i);
        prefixes << u.left(u.size() - directUrl.size());
    }

    for (const QString &prefix : prefixes) {
        QNetworkRequest req = makeRequest(QUrl(prefix + directUrl));
        req.setRawHeader("Range", "bytes=0-0");
        Probe probe;
        probe.prefix = prefix;
        probe.reply = m_nam->get(req);
        probe.timer = new QTimer(this);
        probe.timer->setSingleShot(true);
        connect(probe.timer, &QTimer::timeout, this, [probe]() {
            if (probe.reply) {
                probe.reply->abort();
            }
        });
        probe.timer->start(MIRROR_PROBE_TIMEOUT_MS);

        connect(probe.reply, &QNetworkReply::finished, this,
                [this, probes, done, cleanup, probe, directUrl, all]() {
                    if (*done) {
                        return;
                    }
                    QNetworkReply *r = probe.reply;
                    const QVariant code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                    const bool httpOk =
                        (r->error() == QNetworkReply::NoError)
                        && code.isValid() && (code.toInt() == 200 || code.toInt() == 206);
                    if (httpOk) {
                        *done = true;
                        QSettings settings = Platform::appSettings();
                        settings.setValue(MIRROR_CACHE_KEY, probe.prefix);

                        QStringList ordered;
                        ordered << (probe.prefix + directUrl);
                        for (const QString &u : all) {
                            if (u != probe.prefix + directUrl) {
                                ordered << u;
                            }
                        }
                        cleanup();
                        startDownloadChain(ordered);
                    } else {
                        for (int i = 0; i < probes->size(); ++i) {
                            if (probes->at(i).reply == r) {
                                probes->removeAt(i);
                                break;
                            }
                        }
                        if (probes->isEmpty()) {
                            *done = true;
                            // Nothing reachable now; direct first and let
                            // the chain report the real error.
                            startDownloadChain(all);
                        }
                    }
                });
        probes->append(probe);
    }
}

void ReleaseUpdater::startDownloadChain(const QStringList &urls)
{
    m_pendingUrls = urls;
    tryNextUrl();
}

void ReleaseUpdater::tryNextUrl()
{
    if (m_pendingUrls.isEmpty()) {
        const QString err = m_lastError.isEmpty()
            ? QStringLiteral("download failed") : m_lastError;
        emit installFinished(false, err);
        return;
    }

    const QUrl url = QUrl(m_pendingUrls.takeFirst());
    const QString installDir = QDir::homePath() + "/.local/bin";
    QDir().mkpath(installDir);

    // QSaveFile writes into the target directory and renames on commit,
    // so a partial download can never replace a working binary and a
    // cross-filesystem temp dir can never yield an empty file.
    m_saveFile = new QSaveFile(installDir + "/" + m_installName, this);
    if (!m_saveFile->open(QIODevice::WriteOnly)) {
        m_lastError = tr("Cannot write to %1").arg(installDir);
        m_saveFile->deleteLater();
        m_saveFile = nullptr;
        tryNextUrl();
        return;
    }
    m_hash = new QCryptographicHash(QCryptographicHash::Sha256);

    QNetworkRequest req = makeRequest(url);
    req.setTransferTimeout(0); // large binary; no idle cutoff mid-stream
    m_downloadReply = m_nam->get(req);

    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0) {
                    emit installProgress(static_cast<int>(received * 100 / total));
                }
            });

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_downloadReply || !m_saveFile) {
            return;
        }
        const QByteArray data = m_downloadReply->readAll();
        m_hash->addData(data);
        m_saveFile->write(data);
    });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this, url]() {
        QNetworkReply *reply = m_downloadReply;
        m_downloadReply = nullptr;
        reply->deleteLater();

        if (!m_saveFile) { // cancelled
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            m_lastError = QStringLiteral("%1: %2").arg(url.host(), reply->errorString());
            finishAttempt(false, m_lastError);
            return;
        }

        m_saveFile->flush();
        const QString actual = QString::fromLatin1(m_hash->result().toHex());
        if (!m_currentRelease.sha256.isEmpty() && actual != m_currentRelease.sha256) {
            m_lastError = tr("SHA-256 mismatch from %1 (expected %2, got %3)")
                              .arg(url.host(), m_currentRelease.sha256.left(12) + "…", actual.left(12) + "…");
            finishAttempt(false, m_lastError);
            return;
        }

        const QString installDir = QDir::homePath() + "/.local/bin";
        if (!m_saveFile->commit()) {
            m_lastError = tr("Cannot write to %1").arg(installDir);
            finishAttempt(false, m_lastError);
            return;
        }
        QFile installed(installDir + "/" + m_installName);
        installed.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                 | QFile::ReadGroup | QFile::ExeGroup
                                 | QFile::ReadOther | QFile::ExeOther);
        finishAttempt(true, {});
    });
}

void ReleaseUpdater::finishAttempt(bool ok, const QString &error)
{
    if (m_saveFile) {
        if (!ok) {
            m_saveFile->cancelWriting();
        }
        m_saveFile->deleteLater();
        m_saveFile = nullptr;
    }
    delete m_hash;
    m_hash = nullptr;

    if (!ok) {
        // A checksum failure means this source is bad but another may be
        // fine; every candidate has to pass the digest either way.
        tryNextUrl();
        return;
    }
    emit installFinished(true, {});
}

void ReleaseUpdater::cancelDownload()
{
    if (m_downloadReply) {
        m_downloadReply->abort(); // finished handler cleans up
    }
    if (m_saveFile) {
        m_saveFile->cancelWriting();
        m_saveFile->deleteLater();
        m_saveFile = nullptr;
    }
    delete m_hash;
    m_hash = nullptr;
    m_pendingUrls.clear();
}
