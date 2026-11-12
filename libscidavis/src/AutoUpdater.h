/***************************************************************************
    File                 : AutoUpdater.h
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : GitHub Releases Auto-Update Engine & Dialog
 ***************************************************************************/

#ifndef AUTO_UPDATER_H
#define AUTO_UPDATER_H

#include <QDialog>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class ApplicationWindow;
class QCloseEvent;
class QEvent;
class QFile;
class QFrame;
class QLabel;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QTextBrowser;
class QTimer;

struct ReleaseInfo
{
    QString tagName;
    QString version;
    QString title;
    QString bodyMarkdown;
    QUrl htmlUrl;
    QUrl assetUrl;
    QString assetName;
    qint64 assetSize = 0;
    QString expectedSha256;
    QString publishedAt;
    bool isPreRelease = false;
};

class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateAvailableDialog(const ReleaseInfo &releaseInfo,
                                   const QString &currentVersion = QString(),
                                   ApplicationWindow *app = nullptr,
                                   QWidget *parent = nullptr);
    UpdateAvailableDialog(const ReleaseInfo &releaseInfo, QWidget *parent);
    ~UpdateAvailableDialog() override;

    const ReleaseInfo &releaseInfo() const { return d_info; }
    QString stagedScriptPath() const { return d_stagedScriptPath; }

    void retranslateUi();

public slots:
    void reject() override;

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSkipThisVersion();
    void onStartDownloadAndInstall();
    void onCancelDownload();
    void onDownloadReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadStallTimeout();

private:
    void initUi();
    void updateShaBadgeStyle();
    void cleanupActiveDownload(bool removePartFile);
    static QString formatByteSize(qint64 bytes);
    static QString formatPublishedDate(const QString &isoText);

    enum class ShaBadgeState {
        ReadyChecksum,
        NoChecksum,
        Verified,
        Failed
    };

    ReleaseInfo d_info;
    QString d_currentVersion;
    QPointer<ApplicationWindow> d_app;

    QFrame *d_headerCard = nullptr;
    QLabel *d_lblIcon = nullptr;
    QLabel *d_lblHeadline = nullptr;
    QLabel *d_lblVersionCompare = nullptr;
    QLabel *d_lblPreReleasePill = nullptr;
    QLabel *d_lblPublishedDate = nullptr;

    QLabel *d_lblNotesHeader = nullptr;
    QTextBrowser *d_notesBrowser = nullptr;

    QFrame *d_packageCard = nullptr;
    QLabel *d_lblAssetSummary = nullptr;
    QLabel *d_lblShaBadge = nullptr;

    QFrame *d_progressFrame = nullptr;
    QProgressBar *d_progressBar = nullptr;
    QLabel *d_lblProgressDetail = nullptr;
    QPushButton *d_btnCancelDownload = nullptr;

    QPushButton *d_btnSkipVersion = nullptr;
    QPushButton *d_btnLater = nullptr;
    QPushButton *d_btnDownloadInstall = nullptr;

    ShaBadgeState d_shaState = ShaBadgeState::NoChecksum;
    QNetworkAccessManager d_nam;
    QNetworkReply *d_activeReply = nullptr;
    QTimer *d_stallTimer = nullptr;
    QFile *d_partFile = nullptr;
    QString d_partFilePath;
    QString d_finalPackagePath;
    QString d_stagedScriptPath;
    QElapsedTimer d_downloadClock;
    qint64 d_lastReceivedBytes = 0;
    qint64 d_lastTotalBytes = 0;
    bool d_downloadCanceled = false;
    bool d_downloadStalled = false;
};

class AutoUpdater : public QObject
{
    Q_OBJECT

public:
    using ReleaseInfo = ::ReleaseInfo;
    using UpdateDialog = ::UpdateAvailableDialog;

    explicit AutoUpdater(ApplicationWindow *app, QObject *parent = nullptr);
    ~AutoUpdater() override;

    // Feed configuration & settings
    void setFeedUrl(const QUrl &url);
    QUrl feedUrl() const;

    bool includePreReleases() const;
    void setIncludePreReleases(bool include);

    QString skippedVersion() const;
    void setSkippedVersion(const QString &version);

    static QString defaultLatestFeedUrlString();
    static QString defaultReleasesFeedUrlString();

    // Deterministic Static Core APIs (for C++ & Python testing)
    static QString currentAppVersion();
    static int compareSemVer(const QString &v1, const QString &v2);
    static bool parseGitHubReleaseJson(const QByteArray &jsonBytes,
                                       ReleaseInfo *outInfo,
                                       QString *errOut = nullptr,
                                       bool allowPreRelease = false);
    static QString computeFileSha256(const QString &filePath);
    static bool verifyFileSha256(const QString &filePath, const QString &expectedHex);
    static QString writeWindowsStagingUpdaterScript(const QString &downloadedFilePath,
                                                    const QString &targetInstallDir,
                                                    const QString &exeName,
                                                    qint64 waitForPid,
                                                    bool relaunch = true,
                                                    QString *errOut = nullptr);

    // Live Check, Download & UI Methods
    void checkForUpdates(bool silentIfUpToDate = false);
    QJsonObject checkFeedBlocking(const QUrl &overrideUrl = QUrl(), int timeoutMs = 8000);
    static bool downloadAndVerifyBlocking(const QUrl &assetUrl,
                                          const QString &destFilePath,
                                          const QString &expectedSha256,
                                          QString *errOut = nullptr,
                                          int timeoutMs = 20000);

    bool isCheckInProgress() const { return d_activeCheckReply != nullptr; }
    int networkCheckCount() const { return d_networkCheckCount; }

signals:
    void updateCheckFinished(bool ok,
                             bool updateAvailable,
                             const QString &latestVersion,
                             const QString &errorMessage);

private:
    void handleCheckResult(bool ok,
                           const ReleaseInfo &info,
                           const QString &errorMsg,
                           bool silentIfUpToDate);
    void cancelActiveCheck();
    static QUrl parseFlexibleUrl(const QString &raw);

    QPointer<ApplicationWindow> d_app;
    QUrl d_feedUrlOverride;
    QNetworkAccessManager d_nam;
    QNetworkReply *d_activeCheckReply = nullptr;
    QTimer *d_checkTimeoutTimer = nullptr;
    bool d_activeSilentMode = false;
    bool d_activeTimedOut = false;
    int d_networkCheckCount = 0;
};

#endif // AUTO_UPDATER_H
