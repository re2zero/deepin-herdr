#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;
class QCryptographicHash;

// Release checker/installer for a GitHub-repo project that ships one
// binary asset per platform (herdr releases, and the app's own releases).
//
// Download order is a mirror fallback chain. "Auto" races the candidates
// with a 1-byte probe and puts the first responsive one ahead; the winner
// is cached so later downloads skip the race. Every candidate must pass
// the release's sha256 digest before it is installed (fail closed).
class ReleaseUpdater : public QObject {
    Q_OBJECT
public:
    enum class MirrorMode { Auto, DirectFirst, MirrorFirst };

    struct Release {
        QString version;   // "0.9.0" (leading 'v' stripped)
        QString tagName;   // "v0.9.0"
        QString assetUrl;  // direct GitHub download URL
        QString sha256;    // hex digest from the API, empty if unknown
        QString htmlUrl;   // release page
    };

    // apiPath: "repos/OWNER/NAME/releases/latest"
    // assetPrefix: asset base name, e.g. "herdr" -> "herdr-linux-x86_64"
    // installName: target file name under ~/.local/bin
    ReleaseUpdater(const QString &apiPath, const QString &assetPrefix,
                   const QString &installName, QObject *parent = nullptr);

    void setMirrorMode(MirrorMode mode) { m_mirrorMode = mode; }
    MirrorMode mirrorMode() const { return m_mirrorMode; }
    void setCustomMirrorPrefix(const QString &prefix) { m_customMirror = prefix; }

    void checkLatest();
    void downloadAndInstall(const Release &release);
    void cancelDownload();
    bool isDownloading() const { return m_saveFile != nullptr; }

    // "linux-x86_64", "macos-aarch64", … — the platform key used by the
    // static updates.json index and derived asset names
    static QString platformKey();
    static QString platformAssetName(const QString &prefix);

    // Human-landing page for this project, e.g. https://github.com/OWNER/NAME/releases
    QString releasesPageUrl() const;

    // Repo landing page, e.g. https://github.com/OWNER/NAME (feedback link)
    QString projectPageUrl() const;

    // Returns -1, 0 or 1. Numeric per segment ("0.9" < "0.10.1").
    static int compareVersions(const QString &a, const QString &b);

signals:
    void checkFinished(bool ok, const ReleaseUpdater::Release &release, const QString &errorString);
    void installProgress(int percent);
    void installFinished(bool ok, const QString &errorString);

private:
    // M9 check pipeline: static updates.json index first, GitHub API as
    // the fallback; the API call carries If-None-Match so a 304 answer
    // (served from the cached release) does not burn rate-limit quota.
    void fetchStaticIndex();
    void fetchApiLatest();
    Release cachedRelease() const;

    void startDownloadChain(const QStringList &urls);
    void tryNextUrl();
    void finishAttempt(bool ok, const QString &error);
    void resolveOrder(const QString &directUrl);
    void emitCheckResult(bool ok, const Release &release, const QString &error);

    QStringList candidateUrls(const QString &directUrl) const;

    QString m_apiPath;
    QString m_assetPrefix;
    QString m_installName;
    MirrorMode m_mirrorMode = MirrorMode::Auto;
    QString m_customMirror;

    QNetworkAccessManager *m_nam = nullptr;

    // download chain state
    Release m_currentRelease;
    QStringList m_pendingUrls;
    QString m_lastError;
    QSaveFile *m_saveFile = nullptr;
    QCryptographicHash *m_hash = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
};

#endif // UPDATER_H
