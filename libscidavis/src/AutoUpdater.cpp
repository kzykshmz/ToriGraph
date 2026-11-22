/***************************************************************************
    File                 : AutoUpdater.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : GitHub Releases Auto-Update Engine & Dialog
 ***************************************************************************/

#include "AutoUpdater.h"
#include "ApplicationWindow.h"
#include "IconLoader.h"
#include "globals.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr const char *kDefaultLatestUrl =
        "https://api.github.com/repos/kzykshmz/ToriGraph/releases/latest";
constexpr const char *kDefaultReleasesUrl =
        "https://api.github.com/repos/kzykshmz/ToriGraph/releases";
constexpr const char *kSettingsKeyFeedUrl = "AutoUpdate/FeedUrl";
constexpr const char *kSettingsKeyIncludePre = "AutoUpdate/IncludePreReleases";
constexpr const char *kSettingsKeySkippedVer = "AutoUpdate/SkippedVersion";
constexpr int kLiveCheckTimeoutMs = 10000;
constexpr int kDownloadStallTimeoutMs = 30000;
constexpr qint64 kStreamChunkBytes = 65536;

bool isAllDigits(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return false;
    }
    return true;
}

bool isValidHex64(const QString &s)
{
    if (s.size() != 64)
        return false;
    for (int i = 0; i < 64; ++i) {
        const QChar c = s.at(i);
        const bool ok = (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
                || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
                || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
        if (!ok)
            return false;
    }
    return true;
}

QString normalizeSha256Token(QString raw)
{
    raw = raw.trimmed();
    if (raw.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
        raw = raw.mid(7).trimmed();
    else if (raw.startsWith(QStringLiteral("sha-256:"), Qt::CaseInsensitive))
        raw = raw.mid(8).trimmed();
    if (!isValidHex64(raw))
        return QString();
    return raw.toLower();
}

QString stripVersionPrefixes(QString s)
{
    s = s.trimmed();
    bool changed = true;
    while (changed) {
        changed = false;
        if (s.startsWith(QStringLiteral("ToriGraph"), Qt::CaseInsensitive)) {
            QString rest = s.mid(9).trimmed();
            if (rest.startsWith(QLatin1Char('-')) || rest.startsWith(QLatin1Char('_'))
                || rest.startsWith(QLatin1Char(':')))
                rest = rest.mid(1).trimmed();
            s = rest;
            changed = true;
        } else if (s.startsWith(QStringLiteral("SciDAVis"), Qt::CaseInsensitive)) {
            QString rest = s.mid(8).trimmed();
            if (rest.startsWith(QLatin1Char('-')) || rest.startsWith(QLatin1Char('_'))
                || rest.startsWith(QLatin1Char(':')))
                rest = rest.mid(1).trimmed();
            s = rest;
            changed = true;
        } else if ((s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V')))
                   && s.size() > 1 && s.at(1).isDigit()) {
            s = s.mid(1).trimmed();
            changed = true;
        }
    }
    return s;
}

struct ParsedSemVer
{
    QVector<qint64> core;
    QStringList preRelease;
    bool valid = false;
};

ParsedSemVer parseSemVerInternal(const QString &input)
{
    ParsedSemVer out;
    QString s = stripVersionPrefixes(input);
    if (s.isEmpty())
        return out;

    // Ignore SemVer 2.0 build metadata (+...)
    const int plusIdx = s.indexOf(QLatin1Char('+'));
    if (plusIdx >= 0)
        s = s.left(plusIdx).trimmed();

    QString coreStr = s;
    QString preStr;
    const int dashIdx = s.indexOf(QLatin1Char('-'));
    if (dashIdx >= 0) {
        coreStr = s.left(dashIdx).trimmed();
        preStr = s.mid(dashIdx + 1).trimmed();
    }

    const QStringList coreParts = coreStr.split(QLatin1Char('.'));
    for (int i = 0; i < coreParts.size(); ++i) {
        const QString part = coreParts.at(i).trimmed();
        if (part.isEmpty()) {
            out.core.push_back(0);
            continue;
        }
        int digitCount = 0;
        while (digitCount < part.size() && part.at(digitCount).isDigit())
            ++digitCount;
        if (digitCount == 0) {
            if (preStr.isEmpty())
                preStr = QStringList(coreParts.mid(i)).join(QLatin1Char('.'));
            break;
        }
        out.core.push_back(part.left(digitCount).toLongLong());
        out.valid = true;
        if (digitCount < part.size()) {
            QString suffix = part.mid(digitCount);
            if (i + 1 < coreParts.size())
                suffix += QLatin1Char('.') + QStringList(coreParts.mid(i + 1)).join(QLatin1Char('.'));
            if (preStr.isEmpty())
                preStr = suffix;
            break;
        }
    }

    while (out.core.size() < 3)
        out.core.push_back(0);

    if (!preStr.isEmpty()) {
        const QStringList rawPre = preStr.split(QLatin1Char('.'));
        for (const QString &tok : rawPre) {
            const QString trimmed = tok.trimmed();
            if (!trimmed.isEmpty())
                out.preRelease.push_back(trimmed);
        }
    }

    return out;
}

int scoreWindowsAsset(const QString &assetName)
{
    const QString lower = assetName.trimmed().toLower();
    if (lower.isEmpty())
        return -1000;

    // Exclude sidecar checksum/signature files and explicit non-Windows packages
    if (lower.endsWith(QStringLiteral(".sha256")) || lower.endsWith(QStringLiteral(".txt"))
        || lower.endsWith(QStringLiteral(".sig")) || lower.endsWith(QStringLiteral(".asc"))
        || lower.endsWith(QStringLiteral(".tar.gz")) || lower.endsWith(QStringLiteral(".tgz"))
        || lower.endsWith(QStringLiteral(".tar.xz")) || lower.endsWith(QStringLiteral(".dmg"))
        || lower.endsWith(QStringLiteral(".pkg")) || lower.endsWith(QStringLiteral(".deb"))
        || lower.endsWith(QStringLiteral(".rpm")) || lower.endsWith(QStringLiteral(".appimage")))
        return -1000;

    if (lower.contains(QStringLiteral("linux")) || lower.contains(QStringLiteral("macos"))
        || lower.contains(QStringLiteral("darwin")) || lower.contains(QStringLiteral("osx")))
        return -1000;

    int score = -1000;
    if (lower.endsWith(QStringLiteral(".zip")))
        score = 100;
    else if (lower.endsWith(QStringLiteral(".exe")))
        score = 90;
    else if (lower.endsWith(QStringLiteral(".msi")))
        score = 80;
    else
        return -1000;

    if (lower.contains(QStringLiteral("win64")) || lower.contains(QStringLiteral("x64"))
        || lower.contains(QStringLiteral("x86_64")) || lower.contains(QStringLiteral("amd64")))
        score += 40;
    if (lower.contains(QStringLiteral("windows")) || lower.contains(QStringLiteral("win")))
        score += 25;
    if (lower.contains(QStringLiteral("torigraph")))
        score += 20;
    if (lower.endsWith(QStringLiteral(".exe"))
        && (lower.contains(QStringLiteral("setup")) || lower.contains(QStringLiteral("install"))))
        score += 12;
    if (lower.contains(QStringLiteral("arm64")) || lower.contains(QStringLiteral("aarch64")))
        score -= 15;
    if (lower.contains(QStringLiteral("pdb")) || lower.contains(QStringLiteral("symbols"))
        || lower.contains(QStringLiteral("debug")) || lower.contains(QStringLiteral("source")))
        score -= 60;

    return score;
}

QString extractSha256FromRelease(const QJsonObject &chosenAsset,
                                 const QJsonObject &relObj,
                                 const QString &assetName,
                                 const QString &bodyMarkdown,
                                 const QStringList &otherAssetNames)
{
    QString sha = normalizeSha256Token(chosenAsset.value(QStringLiteral("digest")).toString());
    if (!sha.isEmpty())
        return sha;

    sha = normalizeSha256Token(chosenAsset.value(QStringLiteral("sha256")).toString());
    if (!sha.isEmpty())
        return sha;

    sha = normalizeSha256Token(relObj.value(QStringLiteral("sha256")).toString());
    if (!sha.isEmpty())
        return sha;

    sha = normalizeSha256Token(relObj.value(QStringLiteral("digest")).toString());
    if (!sha.isEmpty())
        return sha;

    if (bodyMarkdown.trimmed().isEmpty())
        return QString();

    static const QRegularExpression hex64Re(
            QStringLiteral("(?:sha-?256\\s*[:=]?\\s*)?\\b([0-9a-fA-F]{64})\\b"),
            QRegularExpression::CaseInsensitiveOption);

    const QStringList lines = bodyMarkdown.split(QLatin1Char('\n'));

    // Pass 1: Line explicitly mentioning our chosen asset filename + 64-hex digest
    if (!assetName.isEmpty()) {
        for (const QString &rawLine : lines) {
            if (rawLine.contains(assetName, Qt::CaseInsensitive)) {
                const QRegularExpressionMatch m = hex64Re.match(rawLine);
                if (m.hasMatch())
                    return m.captured(1).toLower();
            }
        }
    }

    // Pass 2: Line mentioning SHA256 / SHA-256 without naming a conflicting asset
    for (const QString &rawLine : lines) {
        if (!rawLine.contains(QStringLiteral("sha256"), Qt::CaseInsensitive)
            && !rawLine.contains(QStringLiteral("sha-256"), Qt::CaseInsensitive))
            continue;
        bool mentionsOtherAsset = false;
        for (const QString &other : otherAssetNames) {
            if (!other.isEmpty() && other.compare(assetName, Qt::CaseInsensitive) != 0
                && rawLine.contains(other, Qt::CaseInsensitive)) {
                mentionsOtherAsset = true;
                break;
            }
        }
        if (mentionsOtherAsset)
            continue;
        const QRegularExpressionMatch m = hex64Re.match(rawLine);
        if (m.hasMatch())
            return m.captured(1).toLower();
    }

    // Pass 3: Standalone 64-hex digest line (optionally wrapped in backticks or bullet)
    for (const QString &rawLine : lines) {
        QString cleaned = rawLine.trimmed();
        while (cleaned.startsWith(QLatin1Char('-')) || cleaned.startsWith(QLatin1Char('*'))
               || cleaned.startsWith(QLatin1Char('`')) || cleaned.startsWith(QLatin1Char('>')))
            cleaned = cleaned.mid(1).trimmed();
        while (cleaned.endsWith(QLatin1Char('`')))
            cleaned.chop(1);
        cleaned = normalizeSha256Token(cleaned);
        if (!cleaned.isEmpty())
            return cleaned;
    }

    return QString();
}

void configureNetworkManagerForUrl(QNetworkAccessManager &nam, const QUrl &url)
{
    const QString host = url.host().toLower();
    if (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost")
        || host == QStringLiteral("::1")) {
        nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    }
}

QNetworkRequest buildGitHubRequest(const QUrl &url, bool isJsonFeed)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ToriGraph-Updater");
    req.setRawHeader("Accept",
                     isJsonFeed ? "application/vnd.github+json"
                                : "application/octet-stream, */*");
    req.setRawHeader("Connection", "close");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

bool atomicReplaceFile(const QString &partPath, const QString &destFilePath, QString *errOut)
{
    const QFileInfo destInfo(destFilePath);
    if (!destInfo.dir().exists())
        destInfo.dir().mkpath(QStringLiteral("."));

#ifdef Q_OS_WIN
    const QString nativePart = QDir::toNativeSeparators(partPath);
    const QString nativeDest = QDir::toNativeSeparators(destFilePath);
    if (MoveFileExW(reinterpret_cast<LPCWSTR>(nativePart.utf16()),
                    reinterpret_cast<LPCWSTR>(nativeDest.utf16()),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
#endif
    if (QFile::exists(destFilePath) && !QFile::remove(destFilePath)) {
        if (errOut)
            *errOut = AutoUpdater::tr("Could not replace existing destination file: %1")
                              .arg(destFilePath);
        return false;
    }
    if (!QFile::rename(partPath, destFilePath)) {
        if (errOut)
            *errOut = AutoUpdater::tr("Could not finalize downloaded file: %1")
                              .arg(destFilePath);
        return false;
    }
    return true;
}

QString psSingleQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString extractGitHubErrorMessage(const QByteArray &payload)
{
    if (payload.trimmed().isEmpty())
        return QString();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QString();
    const QJsonObject obj = doc.object();
    QString msg = obj.value(QStringLiteral("message")).toString().trimmed();
    if (msg.isEmpty()) {
        const QJsonValue errVal = obj.value(QStringLiteral("error"));
        if (errVal.isString())
            msg = errVal.toString().trimmed();
    }
    return msg;
}

QString extractGitHubErrorMessageFromFile(const QString &filePath)
{
    QFile f(filePath);
    if (!f.exists() || f.size() <= 0 || f.size() > 65536)
        return QString();
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray bytes = f.readAll();
    f.close();
    return extractGitHubErrorMessage(bytes);
}

bool isHeadlessOrEmbeddedProcess(const ApplicationWindow *app)
{
    if (app && (app->isOleEmbeddedMode() || app->batchMode()))
        return true;
    const QStringList args = QCoreApplication::arguments();
    for (const QString &arg : args) {
        if (arg.compare(QStringLiteral("-Embedding"), Qt::CaseInsensitive) == 0
            || arg.compare(QStringLiteral("/Embedding"), Qt::CaseInsensitive) == 0
            || arg.compare(QStringLiteral("--headless"), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

// ============================================================================
// UpdateAvailableDialog
// ============================================================================

UpdateAvailableDialog::UpdateAvailableDialog(const ReleaseInfo &releaseInfo,
                                             const QString &currentVersion,
                                             ApplicationWindow *app,
                                             QWidget *parent)
    : QDialog(parent ? parent : static_cast<QWidget *>(app)),
      d_info(releaseInfo),
      d_currentVersion(currentVersion.trimmed().isEmpty() ? AutoUpdater::currentAppVersion()
                                                          : stripVersionPrefixes(currentVersion)),
      d_app(app)
{
    d_shaState = d_info.expectedSha256.isEmpty() ? ShaBadgeState::NoChecksum
                                                 : ShaBadgeState::ReadyChecksum;
    initUi();
    retranslateUi();
}

UpdateAvailableDialog::UpdateAvailableDialog(const ReleaseInfo &releaseInfo, QWidget *parent)
    : UpdateAvailableDialog(releaseInfo,
                            AutoUpdater::currentAppVersion(),
                            qobject_cast<ApplicationWindow *>(parent),
                            parent)
{
}

UpdateAvailableDialog::~UpdateAvailableDialog()
{
    cleanupActiveDownload(true);
}

void UpdateAvailableDialog::initUi()
{
    setMinimumSize(580, 480);
    resize(620, 520);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowIcon(IconLoader::duotoneIcon(QStringLiteral("update-check"), 24));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 16);
    rootLayout->setSpacing(12);

    // 1. Modern Header Card
    d_headerCard = new QFrame(this);
    d_headerCard->setObjectName(QStringLiteral("updateHeaderCard"));
    d_headerCard->setStyleSheet(
            QStringLiteral("QFrame#updateHeaderCard {"
                           "  background-color: #FFFFFF;"
                           "  border: 1px solid #CBD5E1;"
                           "  border-radius: 10px;"
                           "}"));
    auto *headerRow = new QHBoxLayout(d_headerCard);
    headerRow->setContentsMargins(16, 14, 16, 14);
    headerRow->setSpacing(14);

    d_lblIcon = new QLabel(d_headerCard);
    d_lblIcon->setPixmap(IconLoader::duotonePixmap(QStringLiteral("update-check"), 40));
    d_lblIcon->setFixedSize(44, 44);
    d_lblIcon->setAlignment(Qt::AlignCenter);
    headerRow->addWidget(d_lblIcon, 0, Qt::AlignTop);

    auto *headerTextCol = new QVBoxLayout();
    headerTextCol->setSpacing(6);

    d_lblHeadline = new QLabel(d_headerCard);
    d_lblHeadline->setStyleSheet(
            QStringLiteral("font-size: 11pt; font-weight: 700; color: #0F172A; border: none;"));
    headerTextCol->addWidget(d_lblHeadline);

    auto *pillRow = new QHBoxLayout();
    pillRow->setSpacing(8);

    d_lblVersionCompare = new QLabel(d_headerCard);
    d_lblVersionCompare->setStyleSheet(
            QStringLiteral("background-color: #EFF6FF;"
                           "color: #1D4ED8;"
                           "border: 1px solid #BFDBFE;"
                           "border-radius: 11px;"
                           "padding: 3px 10px;"
                           "font-weight: 600;"
                           "font-size: 9pt;"));
    pillRow->addWidget(d_lblVersionCompare);

    d_lblPreReleasePill = new QLabel(d_headerCard);
    d_lblPreReleasePill->setStyleSheet(
            QStringLiteral("background-color: #FEF3C7;"
                           "color: #B45309;"
                           "border: 1px solid #FDE68A;"
                           "border-radius: 11px;"
                           "padding: 3px 9px;"
                           "font-weight: 600;"
                           "font-size: 9pt;"));
    d_lblPreReleasePill->setVisible(d_info.isPreRelease);
    pillRow->addWidget(d_lblPreReleasePill);
    pillRow->addStretch(1);

    d_lblPublishedDate = new QLabel(d_headerCard);
    d_lblPublishedDate->setStyleSheet(
            QStringLiteral("color: #64748B; font-size: 9pt; border: none;"));
    pillRow->addWidget(d_lblPublishedDate);

    headerTextCol->addLayout(pillRow);
    headerRow->addLayout(headerTextCol, 1);
    rootLayout->addWidget(d_headerCard);

    // 2. Release Notes Markdown / HTML Viewer
    d_lblNotesHeader = new QLabel(this);
    d_lblNotesHeader->setStyleSheet(
            QStringLiteral("font-weight: 600; color: #334155; font-size: 9pt;"));
    rootLayout->addWidget(d_lblNotesHeader);

    d_notesBrowser = new QTextBrowser(this);
    d_notesBrowser->setOpenExternalLinks(true);
    d_notesBrowser->setStyleSheet(
            QStringLiteral("QTextBrowser {"
                           "  background-color: #FFFFFF;"
                           "  border: 1px solid #CBD5E1;"
                           "  border-radius: 8px;"
                           "  padding: 10px;"
                           "  color: #1E293B;"
                           "}"));
    rootLayout->addWidget(d_notesBrowser, 1);

    // 3. Asset & SHA-256 Verification Badge Card
    d_packageCard = new QFrame(this);
    d_packageCard->setObjectName(QStringLiteral("updatePackageCard"));
    d_packageCard->setStyleSheet(
            QStringLiteral("QFrame#updatePackageCard {"
                           "  background-color: #F8FAFC;"
                           "  border: 1px solid #E2E8F0;"
                           "  border-radius: 8px;"
                           "}"));
    auto *pkgRow = new QHBoxLayout(d_packageCard);
    pkgRow->setContentsMargins(12, 8, 12, 8);
    pkgRow->setSpacing(10);

    d_lblAssetSummary = new QLabel(d_packageCard);
    d_lblAssetSummary->setStyleSheet(
            QStringLiteral("color: #334155; font-size: 9pt; border: none;"));
    pkgRow->addWidget(d_lblAssetSummary, 1);

    d_lblShaBadge = new QLabel(d_packageCard);
    pkgRow->addWidget(d_lblShaBadge, 0, Qt::AlignRight);

    rootLayout->addWidget(d_packageCard);

    // 4. Async Progress Bar + Cancel Row
    d_progressFrame = new QFrame(this);
    auto *progCol = new QVBoxLayout(d_progressFrame);
    progCol->setContentsMargins(0, 2, 0, 2);
    progCol->setSpacing(5);

    auto *progRow = new QHBoxLayout();
    progRow->setSpacing(8);
    d_progressBar = new QProgressBar(d_progressFrame);
    d_progressBar->setRange(0, 100);
    d_progressBar->setValue(0);
    d_progressBar->setMinimumHeight(20);
    progRow->addWidget(d_progressBar, 1);

    d_btnCancelDownload = new QPushButton(d_progressFrame);
    progRow->addWidget(d_btnCancelDownload);
    progCol->addLayout(progRow);

    d_lblProgressDetail = new QLabel(d_progressFrame);
    d_lblProgressDetail->setStyleSheet(QStringLiteral("color: #475569; font-size: 9pt;"));
    progCol->addWidget(d_lblProgressDetail);

    d_progressFrame->setVisible(false);
    rootLayout->addWidget(d_progressFrame);

    // 5. Action Buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    d_btnSkipVersion = new QPushButton(this);
    btnRow->addWidget(d_btnSkipVersion);

    btnRow->addStretch(1);

    d_btnLater = new QPushButton(this);
    btnRow->addWidget(d_btnLater);

    d_btnDownloadInstall = new QPushButton(this);
    d_btnDownloadInstall->setDefault(true);
    d_btnDownloadInstall->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background-color: #2563EB;"
                           "  color: #FFFFFF;"
                           "  border: 1px solid #1D4ED8;"
                           "  border-radius: 6px;"
                           "  padding: 6px 16px;"
                           "  font-weight: 600;"
                           "}"
                           "QPushButton:hover { background-color: #1D4ED8; }"
                           "QPushButton:disabled { background-color: #5B6B80; border-color: #94A3B8; }"));
    btnRow->addWidget(d_btnDownloadInstall);

    rootLayout->addLayout(btnRow);

    d_stallTimer = new QTimer(this);
    d_stallTimer->setSingleShot(true);
    connect(d_stallTimer, &QTimer::timeout, this, &UpdateAvailableDialog::onDownloadStallTimeout);

    connect(d_btnSkipVersion, &QPushButton::clicked, this, &UpdateAvailableDialog::onSkipThisVersion);
    connect(d_btnLater, &QPushButton::clicked, this, &UpdateAvailableDialog::reject);
    connect(d_btnCancelDownload, &QPushButton::clicked, this, &UpdateAvailableDialog::onCancelDownload);
    connect(d_btnDownloadInstall, &QPushButton::clicked,
            this, &UpdateAvailableDialog::onStartDownloadAndInstall);

    updateShaBadgeStyle();
}

void UpdateAvailableDialog::updateShaBadgeStyle()
{
    if (!d_lblShaBadge)
        return;

    switch (d_shaState) {
    case ShaBadgeState::Verified:
        d_lblShaBadge->setStyleSheet(
                QStringLiteral("background-color: #DCFCE7; color: #166534;"
                               "border: 1px solid #86EFAC; border-radius: 10px;"
                               "padding: 2px 9px; font-weight: 600; font-size: 9pt;"));
        d_lblShaBadge->setText(tr("SHA-256 Verified"));
        break;
    case ShaBadgeState::Failed:
        d_lblShaBadge->setStyleSheet(
                QStringLiteral("background-color: #FEE2E2; color: #991B1B;"
                               "border: 1px solid #FCA5A5; border-radius: 10px;"
                               "padding: 2px 9px; font-weight: 600; font-size: 9pt;"));
        d_lblShaBadge->setText(tr("SHA-256 Verification Failed"));
        break;
    case ShaBadgeState::ReadyChecksum:
        d_lblShaBadge->setStyleSheet(
                QStringLiteral("background-color: #E0F2FE; color: #075985;"
                               "border: 1px solid #BAE6FD; border-radius: 10px;"
                               "padding: 2px 9px; font-weight: 600; font-size: 9pt;"));
        d_lblShaBadge->setText(
                tr("SHA-256 Checksum Ready (%1...)").arg(d_info.expectedSha256.left(12)));
        break;
    case ShaBadgeState::NoChecksum:
        d_lblShaBadge->setStyleSheet(
                QStringLiteral("background-color: #F1F5F9; color: #475569;"
                               "border: 1px solid #CBD5E1; border-radius: 10px;"
                               "padding: 2px 9px; font-size: 9pt;"));
        d_lblShaBadge->setText(tr("No SHA-256 checksum published"));
        break;
    }
}

void UpdateAvailableDialog::retranslateUi()
{
    setWindowTitle(tr("Software Update Available"));
    d_lblHeadline->setText(tr("A new version of ToriGraph is available!"));
    d_lblVersionCompare->setText(
            tr("Installed: v%1  →  Latest: v%2").arg(d_currentVersion, d_info.version));
    d_lblPreReleasePill->setText(tr("Pre-release"));

    if (!d_info.publishedAt.trimmed().isEmpty()) {
        d_lblPublishedDate->setText(
                tr("Published: %1").arg(formatPublishedDate(d_info.publishedAt)));
    } else {
        const QString relLabel = d_info.title.trimmed().isEmpty() ? d_info.tagName : d_info.title;
        d_lblPublishedDate->setText(tr("Release: %1").arg(relLabel));
    }

    d_lblNotesHeader->setText(tr("Release Notes"));
    if (d_info.bodyMarkdown.trimmed().isEmpty()) {
        d_notesBrowser->setPlainText(tr("No release notes provided for this version."));
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        d_notesBrowser->setMarkdown(d_info.bodyMarkdown);
#else
        d_notesBrowser->setPlainText(d_info.bodyMarkdown);
#endif
    }

    if (!d_info.assetName.isEmpty() && d_info.assetSize > 0) {
        d_lblAssetSummary->setText(
                tr("Package: %1 (%2)").arg(d_info.assetName, formatByteSize(d_info.assetSize)));
    } else if (!d_info.assetName.isEmpty()) {
        d_lblAssetSummary->setText(tr("Package: %1").arg(d_info.assetName));
    } else {
        d_lblAssetSummary->setText(tr("Package: GitHub Release Page"));
    }

    updateShaBadgeStyle();

    if (!d_activeReply)
        d_lblProgressDetail->setText(tr("Ready to download update package."));

    d_btnCancelDownload->setText(tr("Cancel"));
    d_btnSkipVersion->setText(tr("Skip This Version"));
    d_btnLater->setText(tr("Later"));

    if (d_info.assetUrl.isValid() && !d_info.assetUrl.isEmpty())
        d_btnDownloadInstall->setText(tr("Download && Install Update"));
    else
        d_btnDownloadInstall->setText(tr("Open Release Page..."));
}

void UpdateAvailableDialog::changeEvent(QEvent *event)
{
    if (event && (event->type() == QEvent::LanguageChange || event->type() == QEvent::LocaleChange))
        retranslateUi();
    QDialog::changeEvent(event);
}

void UpdateAvailableDialog::closeEvent(QCloseEvent *event)
{
    cleanupActiveDownload(true);
    QDialog::closeEvent(event);
}

void UpdateAvailableDialog::reject()
{
    cleanupActiveDownload(true);
    QDialog::reject();
}

void UpdateAvailableDialog::onSkipThisVersion()
{
    if (!d_info.version.trimmed().isEmpty()) {
        ApplicationWindow::getSettings().setValue(QString::fromLatin1(kSettingsKeySkippedVer),
                                                  d_info.version.trimmed());
    }
    reject();
}

void UpdateAvailableDialog::cleanupActiveDownload(bool removePartFile)
{
    if (d_stallTimer)
        d_stallTimer->stop();

    if (d_activeReply) {
        QNetworkReply *reply = d_activeReply;
        d_activeReply = nullptr;
        reply->disconnect(this);
        if (reply->isRunning())
            reply->abort();
        reply->deleteLater();
    }

    if (d_partFile) {
        if (d_partFile->isOpen())
            d_partFile->close();
        delete d_partFile;
        d_partFile = nullptr;
    }

    if (removePartFile && !d_partFilePath.isEmpty() && QFile::exists(d_partFilePath))
        QFile::remove(d_partFilePath);
}

void UpdateAvailableDialog::onCancelDownload()
{
    d_downloadCanceled = true;
    cleanupActiveDownload(true);
    d_progressBar->setRange(0, 100);
    d_progressBar->setValue(0);
    d_lblProgressDetail->setText(tr("Download canceled."));
    d_btnDownloadInstall->setEnabled(true);
    d_btnSkipVersion->setEnabled(true);
    d_btnLater->setEnabled(true);
    d_btnCancelDownload->setEnabled(false);
}

void UpdateAvailableDialog::onDownloadStallTimeout()
{
    if (!d_activeReply)
        return;
    d_downloadStalled = true;
    d_activeReply->abort();
}

void UpdateAvailableDialog::onStartDownloadAndInstall()
{
    if (!d_info.assetUrl.isValid() || d_info.assetUrl.isEmpty()) {
        if (d_info.htmlUrl.isValid())
            QDesktopServices::openUrl(d_info.htmlUrl);
        accept();
        return;
    }

    QString stagingRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (stagingRoot.isEmpty())
        stagingRoot = QDir::tempPath();
    const QString stagingDir = QDir(stagingRoot).filePath(QStringLiteral("ToriGraph_UpdateStaging"));
    QDir().mkpath(stagingDir);

    QString safeFileName = QFileInfo(d_info.assetName).fileName();
    if (safeFileName.isEmpty())
        safeFileName = QFileInfo(d_info.assetUrl.path()).fileName();
    if (safeFileName.isEmpty())
        safeFileName = QStringLiteral("ToriGraph-Update.zip");

    d_finalPackagePath = QDir(stagingDir).filePath(safeFileName);
    d_partFilePath = d_finalPackagePath + QStringLiteral(".part");

    d_downloadCanceled = false;
    d_downloadStalled = false;
    d_lastReceivedBytes = 0;
    d_lastTotalBytes = d_info.assetSize;

    // Local file:// fast path
    if (d_info.assetUrl.isLocalFile()) {
        d_btnDownloadInstall->setEnabled(false);
        d_btnSkipVersion->setEnabled(false);
        d_btnLater->setEnabled(false);

        QString errMsg;
        if (!AutoUpdater::downloadAndVerifyBlocking(
                    d_info.assetUrl, d_finalPackagePath, d_info.expectedSha256, &errMsg, 20000)) {
            if (!d_info.expectedSha256.isEmpty()) {
                d_shaState = ShaBadgeState::Failed;
                updateShaBadgeStyle();
            }
            d_btnDownloadInstall->setEnabled(true);
            d_btnSkipVersion->setEnabled(true);
            d_btnLater->setEnabled(true);
            QMessageBox::critical(this, tr("Update Download Failed"), errMsg);
            return;
        }
        if (!d_info.expectedSha256.isEmpty()) {
            d_shaState = ShaBadgeState::Verified;
            updateShaBadgeStyle();
        }
        d_progressFrame->setVisible(true);
        d_progressBar->setRange(0, 100);
        d_progressBar->setValue(100);
        d_btnCancelDownload->setEnabled(false);

        QString stageErr;
        const QString appDir = QCoreApplication::applicationDirPath();
        QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
        if (exeName.isEmpty())
            exeName = QStringLiteral("ToriGraph.exe");
        d_stagedScriptPath = AutoUpdater::writeWindowsStagingUpdaterScript(
                d_finalPackagePath, appDir, exeName, QCoreApplication::applicationPid(), true,
                &stageErr);
        if (d_stagedScriptPath.isEmpty()) {
            d_btnDownloadInstall->setEnabled(true);
            d_btnSkipVersion->setEnabled(true);
            d_btnLater->setEnabled(true);
            QMessageBox::warning(this, tr("Update Staging Error"), stageErr);
            return;
        }

        const QMessageBox::StandardButton ans = QMessageBox::question(
                this, tr("Install Update & Restart"),
                tr("The update package (v%1) has been downloaded and verified.\n\n"
                   "ToriGraph will now close to install the update and restart automatically. "
                   "Any unsaved project changes will prompt to save first.\n\n"
                   "Install update and restart now?")
                        .arg(d_info.version),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (ans == QMessageBox::Yes) {
            const bool started = QProcess::startDetached(
                    QStringLiteral("powershell.exe"),
                    QStringList() << QStringLiteral("-NoProfile")
                                  << QStringLiteral("-ExecutionPolicy")
                                  << QStringLiteral("Bypass")
                                  << QStringLiteral("-File")
                                  << QDir::toNativeSeparators(d_stagedScriptPath));
            if (!started) {
                d_btnDownloadInstall->setEnabled(true);
                d_btnSkipVersion->setEnabled(true);
                d_btnLater->setEnabled(true);
                QMessageBox::warning(
                        this, tr("Installer Launch Failed"),
                        tr("Could not launch Windows PowerShell to apply the staged update."));
                return;
            }
            accept();
            if (d_app)
                QTimer::singleShot(0, d_app.data(), &QWidget::close);
            else
                QCoreApplication::quit();
        } else {
            d_btnDownloadInstall->setEnabled(true);
            d_btnSkipVersion->setEnabled(true);
            d_btnLater->setEnabled(true);
        }
        return;
    }

    // Async HTTP / HTTPS download
    cleanupActiveDownload(true);
    d_partFile = new QFile(d_partFilePath);
    if (!d_partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete d_partFile;
        d_partFile = nullptr;
        QMessageBox::warning(
                this, tr("Update Download Failed"),
                tr("Cannot open temporary file for writing:\n%1").arg(d_partFilePath));
        return;
    }

    d_btnDownloadInstall->setEnabled(false);
    d_btnSkipVersion->setEnabled(false);
    d_btnLater->setEnabled(false);
    d_btnCancelDownload->setEnabled(true);
    d_progressFrame->setVisible(true);
    d_progressBar->setRange(0, d_info.assetSize > 0 ? 100 : 0);
    d_progressBar->setValue(0);
    d_lblProgressDetail->setText(tr("Connecting to release server..."));

    configureNetworkManagerForUrl(d_nam, d_info.assetUrl);
    const QNetworkRequest req = buildGitHubRequest(d_info.assetUrl, false);
    d_downloadClock.start();
    d_activeReply = d_nam.get(req);
    d_stallTimer->start(kDownloadStallTimeoutMs);

    connect(d_activeReply, &QNetworkReply::readyRead,
            this, &UpdateAvailableDialog::onDownloadReadyRead);
    connect(d_activeReply, &QNetworkReply::downloadProgress,
            this, &UpdateAvailableDialog::onDownloadProgress);
    connect(d_activeReply, &QNetworkReply::finished,
            this, &UpdateAvailableDialog::onDownloadFinished);
}

void UpdateAvailableDialog::onDownloadReadyRead()
{
    if (!d_activeReply || !d_partFile)
        return;
    if (d_stallTimer)
        d_stallTimer->start(kDownloadStallTimeoutMs);
    const QByteArray chunk = d_activeReply->readAll();
    if (!chunk.isEmpty())
        d_partFile->write(chunk);
}

void UpdateAvailableDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (!d_activeReply)
        return;
    if (d_stallTimer)
        d_stallTimer->start(kDownloadStallTimeoutMs);

    d_lastReceivedBytes = bytesReceived;
    const qint64 effectiveTotal = (bytesTotal > 0) ? bytesTotal : d_info.assetSize;
    d_lastTotalBytes = effectiveTotal;

    const qint64 elapsedMs = qMax<qint64>(1, d_downloadClock.elapsed());
    const qint64 bytesPerSec = (bytesReceived * 1000) / elapsedMs;
    const QString speedStr = formatByteSize(bytesPerSec);

    if (effectiveTotal > 0) {
        if (d_progressBar->maximum() != 100)
            d_progressBar->setRange(0, 100);
        const int pct = qBound(0, static_cast<int>((bytesReceived * 100) / effectiveTotal), 100);
        d_progressBar->setValue(pct);
        d_lblProgressDetail->setText(
                tr("Downloading: %1 / %2 (%3/s)")
                        .arg(formatByteSize(bytesReceived),
                             formatByteSize(effectiveTotal),
                             speedStr));
    } else {
        d_progressBar->setRange(0, 0);
        d_lblProgressDetail->setText(
                tr("Downloading: %1 (%2/s)").arg(formatByteSize(bytesReceived), speedStr));
    }
}

void UpdateAvailableDialog::onDownloadFinished()
{
    if (!d_activeReply)
        return;

    if (d_stallTimer)
        d_stallTimer->stop();

    QNetworkReply *reply = d_activeReply;
    d_activeReply = nullptr;

    if (d_partFile && d_partFile->isOpen()) {
        const QByteArray rest = reply->readAll();
        if (!rest.isEmpty())
            d_partFile->write(rest);
        d_partFile->flush();
        d_partFile->close();
    }
    delete d_partFile;
    d_partFile = nullptr;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (d_downloadCanceled) {
        QFile::remove(d_partFilePath);
        d_btnDownloadInstall->setEnabled(true);
        d_btnSkipVersion->setEnabled(true);
        d_btnLater->setEnabled(true);
        d_btnCancelDownload->setEnabled(false);
        return;
    }

    if (d_downloadStalled || netErr != QNetworkReply::NoError || httpStatus != 200) {
        const QString apiMsg = (!d_downloadStalled && httpStatus > 0 && httpStatus != 200)
                ? extractGitHubErrorMessageFromFile(d_partFilePath)
                : QString();
        QFile::remove(d_partFilePath);
        d_progressBar->setRange(0, 100);
        d_progressBar->setValue(0);
        d_btnDownloadInstall->setEnabled(true);
        d_btnSkipVersion->setEnabled(true);
        d_btnLater->setEnabled(true);
        d_btnCancelDownload->setEnabled(false);
        const QString reason = d_downloadStalled
                ? tr("Network connection timed out during download.")
                : (!apiMsg.isEmpty()
                           ? AutoUpdater::tr("GitHub API error (HTTP %1): %2")
                                     .arg(httpStatus)
                                     .arg(apiMsg)
                           : tr("Download failed (HTTP %1): %2").arg(httpStatus).arg(netErrStr));
        d_lblProgressDetail->setText(reason);
        QMessageBox::warning(this, tr("Update Download Failed"), reason);
        return;
    }

    // Verify SHA-256 checksum if published
    if (!d_info.expectedSha256.trimmed().isEmpty()) {
        if (!AutoUpdater::verifyFileSha256(d_partFilePath, d_info.expectedSha256)) {
            QFile::remove(d_partFilePath);
            d_shaState = ShaBadgeState::Failed;
            updateShaBadgeStyle();
            d_btnDownloadInstall->setEnabled(true);
            d_btnSkipVersion->setEnabled(true);
            d_btnLater->setEnabled(true);
            d_btnCancelDownload->setEnabled(false);
            d_lblProgressDetail->setText(tr("SHA-256 Verification Failed"));
            QMessageBox::critical(
                    this, tr("Security Verification Failed"),
                    tr("The downloaded file failed SHA-256 checksum verification and was deleted "
                       "for your safety."));
            return;
        }
        d_shaState = ShaBadgeState::Verified;
        updateShaBadgeStyle();
    }

    QString replaceErr;
    if (!atomicReplaceFile(d_partFilePath, d_finalPackagePath, &replaceErr)) {
        QFile::remove(d_partFilePath);
        d_btnDownloadInstall->setEnabled(true);
        d_btnSkipVersion->setEnabled(true);
        d_btnLater->setEnabled(true);
        d_btnCancelDownload->setEnabled(false);
        QMessageBox::warning(this, tr("Update Download Failed"), replaceErr);
        return;
    }

    d_progressBar->setRange(0, 100);
    d_progressBar->setValue(100);
    d_btnCancelDownload->setEnabled(false);
    d_lblProgressDetail->setText(tr("Download complete and verified."));

    QString stageErr;
    const QString appDir = QCoreApplication::applicationDirPath();
    QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    if (exeName.isEmpty())
        exeName = QStringLiteral("ToriGraph.exe");

    d_stagedScriptPath = AutoUpdater::writeWindowsStagingUpdaterScript(
            d_finalPackagePath, appDir, exeName, QCoreApplication::applicationPid(), true,
            &stageErr);
    if (d_stagedScriptPath.isEmpty()) {
        d_btnDownloadInstall->setEnabled(true);
        d_btnSkipVersion->setEnabled(true);
        d_btnLater->setEnabled(true);
        QMessageBox::warning(this, tr("Update Staging Error"), stageErr);
        return;
    }

    const QMessageBox::StandardButton ans = QMessageBox::question(
            this, tr("Install Update & Restart"),
            tr("The update package (v%1) has been downloaded and verified.\n\n"
               "ToriGraph will now close to install the update and restart automatically. "
               "Any unsaved project changes will prompt to save first.\n\n"
               "Install update and restart now?")
                    .arg(d_info.version),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (ans == QMessageBox::Yes) {
        const bool started = QProcess::startDetached(
                QStringLiteral("powershell.exe"),
                QStringList() << QStringLiteral("-NoProfile")
                              << QStringLiteral("-ExecutionPolicy")
                              << QStringLiteral("Bypass")
                              << QStringLiteral("-File")
                              << QDir::toNativeSeparators(d_stagedScriptPath));
        if (!started) {
            d_btnDownloadInstall->setEnabled(true);
            d_btnSkipVersion->setEnabled(true);
            d_btnLater->setEnabled(true);
            QMessageBox::warning(
                    this, tr("Installer Launch Failed"),
                    tr("Could not launch Windows PowerShell to apply the staged update."));
            return;
        }
        accept();
        if (d_app)
            QTimer::singleShot(0, d_app.data(), &QWidget::close);
        else
            QCoreApplication::quit();
    } else {
        d_btnDownloadInstall->setEnabled(true);
        d_btnSkipVersion->setEnabled(true);
        d_btnLater->setEnabled(true);
    }
}

QString UpdateAvailableDialog::formatByteSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024LL * 1024LL)
        return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 2);
    return QStringLiteral("%1 GB").arg(double(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString UpdateAvailableDialog::formatPublishedDate(const QString &isoText)
{
    const QDateTime dt = QDateTime::fromString(isoText.trimmed(), Qt::ISODate);
    if (dt.isValid())
        return dt.toUTC().toString(QStringLiteral("yyyy-MM-dd"));
    return isoText.trimmed().left(10);
}

// ============================================================================
// AutoUpdater
// ============================================================================

AutoUpdater::AutoUpdater(ApplicationWindow *app, QObject *parent)
    : QObject(parent), d_app(app)
{
    d_checkTimeoutTimer = new QTimer(this);
    d_checkTimeoutTimer->setSingleShot(true);
    connect(d_checkTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (d_activeCheckReply && d_activeCheckReply->isRunning()) {
            d_activeTimedOut = true;
            d_activeCheckReply->abort();
        }
    });
}

AutoUpdater::~AutoUpdater()
{
    cancelActiveCheck();
}

QString AutoUpdater::defaultLatestFeedUrlString()
{
    return QString::fromLatin1(kDefaultLatestUrl);
}

QString AutoUpdater::defaultReleasesFeedUrlString()
{
    return QString::fromLatin1(kDefaultReleasesUrl);
}

QUrl AutoUpdater::parseFlexibleUrl(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return QUrl();
    if (trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        return QUrl(trimmed);
    }
    if (QFileInfo::exists(trimmed)
        || (trimmed.size() >= 3 && trimmed.at(0).isLetter() && trimmed.at(1) == QLatin1Char(':'))) {
        return QUrl::fromLocalFile(trimmed);
    }
    return QUrl::fromUserInput(trimmed);
}

void AutoUpdater::setFeedUrl(const QUrl &url)
{
    d_feedUrlOverride = url;
}

QUrl AutoUpdater::feedUrl() const
{
    if (d_feedUrlOverride.isValid() && !d_feedUrlOverride.isEmpty())
        return d_feedUrlOverride;

    const QString envFeed = qEnvironmentVariable("TORIGRAPH_UPDATE_FEED_URL").trimmed();
    if (!envFeed.isEmpty()) {
        const QUrl envUrl = parseFlexibleUrl(envFeed);
        if (envUrl.isValid() && !envUrl.isEmpty())
            return envUrl;
    }

    const QString settingsFeed = ApplicationWindow::getSettings()
                                         .value(QString::fromLatin1(kSettingsKeyFeedUrl))
                                         .toString()
                                         .trimmed();
    if (!settingsFeed.isEmpty()) {
        const QUrl cfgUrl = parseFlexibleUrl(settingsFeed);
        if (cfgUrl.isValid() && !cfgUrl.isEmpty())
            return cfgUrl;
    }

    return QUrl(includePreReleases() ? defaultReleasesFeedUrlString()
                                     : defaultLatestFeedUrlString());
}

bool AutoUpdater::includePreReleases() const
{
    return ApplicationWindow::getSettings()
            .value(QString::fromLatin1(kSettingsKeyIncludePre), false)
            .toBool();
}

void AutoUpdater::setIncludePreReleases(bool include)
{
    ApplicationWindow::getSettings().setValue(QString::fromLatin1(kSettingsKeyIncludePre), include);
}

QString AutoUpdater::skippedVersion() const
{
    return ApplicationWindow::getSettings()
            .value(QString::fromLatin1(kSettingsKeySkippedVer))
            .toString()
            .trimmed();
}

void AutoUpdater::setSkippedVersion(const QString &version)
{
    ApplicationWindow::getSettings().setValue(QString::fromLatin1(kSettingsKeySkippedVer),
                                              stripVersionPrefixes(version));
}

QString AutoUpdater::currentAppVersion()
{
    const QString combined = ApplicationWindow::versionString() + SciDAVis::extraVersion();
    const QString stripped = stripVersionPrefixes(combined);
    const ParsedSemVer parsed = parseSemVerInternal(stripped);
    if (parsed.valid) {
        QString norm = QStringLiteral("%1.%2.%3")
                               .arg(parsed.core.at(0))
                               .arg(parsed.core.at(1))
                               .arg(parsed.core.at(2));
        if (!parsed.preRelease.isEmpty())
            norm += QLatin1Char('-') + parsed.preRelease.join(QLatin1Char('.'));
        return norm;
    }

    const int v = SciDAVis::version();
    return QStringLiteral("%1.%2.%3")
                   .arg((v >> 16) & 0xFF)
                   .arg((v >> 8) & 0xFF)
                   .arg(v & 0xFF)
            + SciDAVis::extraVersion();
}

int AutoUpdater::compareSemVer(const QString &v1, const QString &v2)
{
    const ParsedSemVer a = parseSemVerInternal(v1);
    const ParsedSemVer b = parseSemVerInternal(v2);

    if (!a.valid && !b.valid)
        return 0;
    if (a.valid && !b.valid)
        return 1;
    if (!a.valid && b.valid)
        return -1;

    const int coreCount = qMax(a.core.size(), b.core.size());
    for (int i = 0; i < coreCount; ++i) {
        const qint64 ca = (i < a.core.size()) ? a.core.at(i) : 0;
        const qint64 cb = (i < b.core.size()) ? b.core.at(i) : 0;
        if (ca < cb)
            return -1;
        if (ca > cb)
            return 1;
    }

    // Normal release > pre-release per SemVer 2.0.0 §11.4
    if (a.preRelease.isEmpty() && b.preRelease.isEmpty())
        return 0;
    if (a.preRelease.isEmpty() && !b.preRelease.isEmpty())
        return 1;
    if (!a.preRelease.isEmpty() && b.preRelease.isEmpty())
        return -1;

    static const QRegularExpression alphaNumRe(QStringLiteral("^([A-Za-z-]+)(\\d+)$"));

    const int preMin = qMin(a.preRelease.size(), b.preRelease.size());
    for (int i = 0; i < preMin; ++i) {
        const QString &pa = a.preRelease.at(i);
        const QString &pb = b.preRelease.at(i);
        if (pa == pb)
            continue;

        const bool numA = isAllDigits(pa);
        const bool numB = isAllDigits(pb);
        if (numA && numB) {
            const qint64 na = pa.toLongLong();
            const qint64 nb = pb.toLongLong();
            if (na < nb)
                return -1;
            if (na > nb)
                return 1;
            continue;
        }
        if (numA != numB) {
            // Numeric identifiers have lower precedence than non-numeric identifiers
            return numA ? -1 : 1;
        }

        // Handle combined alpha+digit identifiers such as rc2 vs rc10 cleanly
        const QRegularExpressionMatch ma = alphaNumRe.match(pa);
        const QRegularExpressionMatch mb = alphaNumRe.match(pb);
        if (ma.hasMatch() && mb.hasMatch() && ma.captured(1) == mb.captured(1)) {
            const qint64 na = ma.captured(2).toLongLong();
            const qint64 nb = mb.captured(2).toLongLong();
            if (na < nb)
                return -1;
            if (na > nb)
                return 1;
        }

        const int cmp = QString::compare(pa, pb, Qt::CaseSensitive);
        if (cmp < 0)
            return -1;
        if (cmp > 0)
            return 1;
    }

    if (a.preRelease.size() < b.preRelease.size())
        return -1;
    if (a.preRelease.size() > b.preRelease.size())
        return 1;
    return 0;
}

bool AutoUpdater::parseGitHubReleaseJson(const QByteArray &jsonBytes,
                                         ReleaseInfo *outInfo,
                                         QString *errOut,
                                         bool allowPreRelease)
{
    if (outInfo)
        *outInfo = ReleaseInfo();
    else {
        if (errOut)
            *errOut = tr("Internal error: output ReleaseInfo pointer is null.");
        return false;
    }

    if (jsonBytes.trimmed().isEmpty()) {
        if (errOut)
            *errOut = tr("Empty release feed response.");
        return false;
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        if (errOut)
            *errOut = tr("Invalid release feed JSON: %1").arg(parseErr.errorString());
        return false;
    }

    QJsonObject selectedRel;
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (!obj.contains(QStringLiteral("tag_name")) && !obj.contains(QStringLiteral("version"))) {
            const QString apiMsg = obj.value(QStringLiteral("message")).toString().trimmed();
            if (errOut) {
                *errOut = apiMsg.isEmpty()
                        ? tr("Release JSON does not contain a tag_name field.")
                        : tr("GitHub API error: %1").arg(apiMsg);
            }
            return false;
        }
        if (obj.value(QStringLiteral("draft")).toBool(false)) {
            if (errOut)
                *errOut = tr("Latest release is marked as draft.");
            return false;
        }
        if (obj.value(QStringLiteral("prerelease")).toBool(false) && !allowPreRelease) {
            if (errOut)
                *errOut = tr("Pre-release versions are excluded by current preferences.");
            return false;
        }
        selectedRel = obj;
    } else if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        QString bestVersion;
        bool found = false;
        for (const QJsonValue &val : arr) {
            if (!val.isObject())
                continue;
            const QJsonObject candidate = val.toObject();
            if (candidate.value(QStringLiteral("draft")).toBool(false))
                continue;
            if (candidate.value(QStringLiteral("prerelease")).toBool(false) && !allowPreRelease)
                continue;
            const QString rawTag = candidate.value(QStringLiteral("tag_name")).toString().trimmed().isEmpty()
                    ? candidate.value(QStringLiteral("version")).toString().trimmed()
                    : candidate.value(QStringLiteral("tag_name")).toString().trimmed();
            if (rawTag.isEmpty())
                continue;
            const QString candVer = stripVersionPrefixes(rawTag);
            if (!found || compareSemVer(candVer, bestVersion) > 0) {
                selectedRel = candidate;
                bestVersion = candVer;
                found = true;
            }
        }
        if (!found) {
            if (errOut)
                *errOut = tr("No eligible release found in GitHub releases feed.");
            return false;
        }
    } else {
        if (errOut)
            *errOut = tr("Unexpected JSON root type in release feed.");
        return false;
    }

    ReleaseInfo info;
    info.tagName = selectedRel.value(QStringLiteral("tag_name")).toString().trimmed();
    if (info.tagName.isEmpty())
        info.tagName = selectedRel.value(QStringLiteral("version")).toString().trimmed();
    info.version = stripVersionPrefixes(info.tagName);
    if (!parseSemVerInternal(info.version).valid) {
        if (errOut)
            *errOut = tr("Could not parse semantic version from tag '%1'.").arg(info.tagName);
        return false;
    }

    info.title = selectedRel.value(QStringLiteral("name")).toString().trimmed();
    if (info.title.isEmpty())
        info.title = selectedRel.value(QStringLiteral("title")).toString().trimmed();
    if (info.title.isEmpty())
        info.title = info.tagName;

    info.bodyMarkdown = selectedRel.value(QStringLiteral("body")).toString();
    if (info.bodyMarkdown.isEmpty())
        info.bodyMarkdown = selectedRel.value(QStringLiteral("release_notes")).toString();

    info.htmlUrl = QUrl(selectedRel.value(QStringLiteral("html_url")).toString().trimmed());
    info.publishedAt = selectedRel.value(QStringLiteral("published_at")).toString().trimmed();
    if (info.publishedAt.isEmpty())
        info.publishedAt = selectedRel.value(QStringLiteral("created_at")).toString().trimmed();
    info.isPreRelease = selectedRel.value(QStringLiteral("prerelease")).toBool(false);

    // Pick best Windows asset (.zip, .exe, .msi)
    QJsonObject bestAssetObj;
    int bestAssetScore = -1000;
    QStringList allAssetNames;

    const QJsonArray assets = selectedRel.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &aval : assets) {
        if (!aval.isObject())
            continue;
        const QJsonObject aobj = aval.toObject();
        const QString name = aobj.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty())
            allAssetNames.push_back(name);
        const int sc = scoreWindowsAsset(name);
        if (sc > bestAssetScore) {
            bestAssetScore = sc;
            bestAssetObj = aobj;
        }
    }

    if (bestAssetScore > 0) {
        info.assetName = bestAssetObj.value(QStringLiteral("name")).toString().trimmed();
        QString dlUrl = bestAssetObj.value(QStringLiteral("browser_download_url")).toString().trimmed();
        if (dlUrl.isEmpty())
            dlUrl = bestAssetObj.value(QStringLiteral("download_url")).toString().trimmed();
        if (dlUrl.isEmpty())
            dlUrl = bestAssetObj.value(QStringLiteral("url")).toString().trimmed();
        info.assetUrl = parseFlexibleUrl(dlUrl);
        info.assetSize = bestAssetObj.value(QStringLiteral("size")).toVariant().toLongLong();
    } else if (selectedRel.contains(QStringLiteral("asset_url"))) {
        info.assetUrl = parseFlexibleUrl(
                selectedRel.value(QStringLiteral("asset_url")).toString().trimmed());
        info.assetName = selectedRel.value(QStringLiteral("asset_name")).toString().trimmed();
        if (info.assetName.isEmpty())
            info.assetName = QFileInfo(info.assetUrl.path()).fileName();
        info.assetSize = selectedRel.value(QStringLiteral("asset_size")).toVariant().toLongLong();
    }

    info.expectedSha256 = extractSha256FromRelease(
            bestAssetObj, selectedRel, info.assetName, info.bodyMarkdown, allAssetNames);

    *outInfo = info;
    return true;
}

QString AutoUpdater::computeFileSha256(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    QByteArray buffer(static_cast<int>(kStreamChunkBytes), Qt::Uninitialized);
    while (true) {
        const qint64 n = file.read(buffer.data(), buffer.size());
        if (n < 0)
            return QString();
        if (n == 0)
            break;
        hasher.addData(buffer.constData(), static_cast<int>(n));
    }
    return QString::fromLatin1(hasher.result().toHex()).toLower();
}

bool AutoUpdater::verifyFileSha256(const QString &filePath, const QString &expectedHex)
{
    const QString actual = computeFileSha256(filePath);
    if (actual.size() != 64)
        return false;

    const QString trimmedExpected = expectedHex.trimmed();
    if (trimmedExpected.isEmpty())
        return true;

    const QString normalizedExpected = normalizeSha256Token(trimmedExpected);
    if (normalizedExpected.size() != 64)
        return false;

    const QByteArray actualBytes = actual.toLatin1();
    const QByteArray expectedBytes = normalizedExpected.toLatin1();
    if (actualBytes.size() != 64 || expectedBytes.size() != 64)
        return false;

    // Constant-length 64-hex comparison
    unsigned char diff = 0;
    for (int i = 0; i < 64; ++i)
        diff |= static_cast<unsigned char>(actualBytes.at(i) ^ expectedBytes.at(i));
    return diff == 0;
}

QString AutoUpdater::writeWindowsStagingUpdaterScript(const QString &downloadedFilePath,
                                                      const QString &targetInstallDir,
                                                      const QString &exeName,
                                                      qint64 waitForPid,
                                                      bool relaunch,
                                                      QString *errOut)
{
    const QString pkgPath = QDir::toNativeSeparators(downloadedFilePath.trimmed());
    const QString installDir = QDir::toNativeSeparators(targetInstallDir.trimmed());
    const QString binaryName = exeName.trimmed().isEmpty()
            ? QStringLiteral("ToriGraph.exe")
            : QFileInfo(exeName.trimmed()).fileName();

    if (pkgPath.isEmpty() || installDir.isEmpty()) {
        if (errOut)
            *errOut = tr("Invalid paths supplied for Windows staging updater script.");
        return QString();
    }

    QString tempBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempBase.isEmpty())
        tempBase = QDir::tempPath();
    const QString stagingDir = QDir(tempBase).filePath(QStringLiteral("ToriGraph_UpdateStaging"));
    if (!QDir().mkpath(stagingDir)) {
        if (errOut)
            *errOut = tr("Could not create staging directory: %1").arg(stagingDir);
        return QString();
    }

    const QString ps1Path = QDir(stagingDir).filePath(QStringLiteral("apply_torigraph_update.ps1"));
    const QString cmdPath = QDir(stagingDir).filePath(QStringLiteral("apply_torigraph_update.cmd"));
    const QString nativeStagingDir = QDir::toNativeSeparators(stagingDir);

    QString psScript;
    psScript += QStringLiteral("# ToriGraph Staged Windows Auto-Updater\r\n");
    psScript += QStringLiteral("Set-StrictMode -Version Latest\r\n");
    psScript += QStringLiteral("$ErrorActionPreference = 'Stop'\r\n\r\n");
    psScript += QStringLiteral("$DownloadedFile = %1\r\n").arg(psSingleQuote(pkgPath));
    psScript += QStringLiteral("$TargetInstallDir = %1\r\n").arg(psSingleQuote(installDir));
    psScript += QStringLiteral("$ExeName = %1\r\n").arg(psSingleQuote(binaryName));
    psScript += QStringLiteral("$WaitForPid = %1\r\n").arg(waitForPid);
    psScript += QStringLiteral("$Relaunch = $%1\r\n").arg(relaunch ? QStringLiteral("true")
                                                                   : QStringLiteral("false"));
    psScript += QStringLiteral("$StagingRoot = %1\r\n\r\n").arg(psSingleQuote(nativeStagingDir));

    psScript += QStringLiteral(
            "$TargetExe = [System.IO.Path]::Combine($TargetInstallDir, $ExeName)\r\n"
            "$BackupExe = \"$TargetExe.rollback.bak\"\r\n"
            "$ExtractDir = [System.IO.Path]::Combine($StagingRoot, ('extract_' + [Guid]::NewGuid().ToString('N')))\r\n\r\n"
            "# (1) Wait up to 30s for PID $WaitForPid to exit\r\n"
            "if ($WaitForPid -gt 0) {\r\n"
            "    $deadline = (Get-Date).AddSeconds(30)\r\n"
            "    while ((Get-Date) -lt $deadline) {\r\n"
            "        $proc = Get-Process -Id $WaitForPid -ErrorAction SilentlyContinue\r\n"
            "        if ($null -eq $proc) { break }\r\n"
            "        Start-Sleep -Milliseconds 250\r\n"
            "    }\r\n"
            "}\r\n\r\n"
            "# (2) Create rollback backup of ToriGraph.exe\r\n"
            "if (!(Test-Path -LiteralPath $TargetInstallDir)) {\r\n"
            "    [System.IO.Directory]::CreateDirectory($TargetInstallDir) | Out-Null\r\n"
            "}\r\n"
            "if (Test-Path -LiteralPath $TargetExe) {\r\n"
            "    Copy-Item -LiteralPath $TargetExe -Destination $BackupExe -Force\r\n"
            "}\r\n\r\n"
            "# (3) Extract .zip via Expand-Archive or run installer, (4) restore backup on error\r\n"
            "try {\r\n"
            "    $ext = [System.IO.Path]::GetExtension($DownloadedFile).ToLowerInvariant()\r\n"
            "    if ($ext -eq '.zip') {\r\n"
            "        if ($DownloadedFile -match '[\\[\\]]' -or $ExtractDir -match '[\\[\\]]') {\r\n"
            "            Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n"
            "            [System.IO.Compression.ZipFile]::ExtractToDirectory($DownloadedFile, $ExtractDir)\r\n"
            "        } else {\r\n"
            "            try {\r\n"
            "                Expand-Archive -LiteralPath $DownloadedFile -DestinationPath $ExtractDir -Force\r\n"
            "            } catch {\r\n"
            "                if (Test-Path -LiteralPath $ExtractDir) {\r\n"
            "                    Remove-Item -LiteralPath $ExtractDir -Recurse -Force -ErrorAction SilentlyContinue\r\n"
            "                }\r\n"
            "                Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n"
            "                [System.IO.Compression.ZipFile]::ExtractToDirectory($DownloadedFile, $ExtractDir)\r\n"
            "            }\r\n"
            "        }\r\n"
            "        $entries = @(Get-ChildItem -LiteralPath $ExtractDir -Force)\r\n"
            "        $copyRoot = $ExtractDir\r\n"
            "        if ($entries.Count -eq 1 -and $entries[0].PSIsContainer) {\r\n"
            "            $copyRoot = $entries[0].FullName\r\n"
            "        }\r\n"
            "        $itemsToCopy = @(Get-ChildItem -LiteralPath $copyRoot -Force)\r\n"
            "        if ($itemsToCopy.Count -eq 0) {\r\n"
            "            throw \"Extracted update archive contains no files: $DownloadedFile\"\r\n"
            "        }\r\n"
            "        Get-ChildItem -LiteralPath $copyRoot -Force | ForEach-Object {\r\n"
            "            Copy-Item -LiteralPath $_.FullName -Destination $TargetInstallDir -Recurse -Force\r\n"
            "            $destEntry = [System.IO.Path]::Combine($TargetInstallDir, $_.Name)\r\n"
            "            if (!(Test-Path -LiteralPath $destEntry)) {\r\n"
            "                throw \"Failed to copy staged item to target directory: $destEntry\"\r\n"
            "            }\r\n"
            "        }\r\n"
            "        $sourceHasExe = Test-Path -LiteralPath ([System.IO.Path]::Combine($copyRoot, $ExeName))\r\n"
            "        if (($sourceHasExe -or (Test-Path -LiteralPath $BackupExe)) -and !(Test-Path -LiteralPath $TargetExe)) {\r\n"
            "            throw \"Updated executable not found in target directory: $TargetExe\"\r\n"
            "        }\r\n"
            "    } elseif ($ext -eq '.msi') {\r\n"
            "        $p = Start-Process -FilePath 'msiexec.exe' -ArgumentList @('/i', $DownloadedFile, '/passive', '/norestart') -Wait -PassThru\r\n"
            "        if ($p.ExitCode -ne 0) { throw \"MSI exited with code $($p.ExitCode)\" }\r\n"
            "    } elseif ($ext -eq '.exe') {\r\n"
            "        $leaf = [System.IO.Path]::GetFileName($DownloadedFile).ToLowerInvariant()\r\n"
            "        if ($leaf -match 'setup|install') {\r\n"
            "            $p = Start-Process -FilePath $DownloadedFile -ArgumentList @('/SILENT', '/SUPPRESSMSGBOXES', '/NORESTART', (\"/DIR=$TargetInstallDir\")) -Wait -PassThru\r\n"
            "            if ($p.ExitCode -ne 0) { throw \"Installer exited with code $($p.ExitCode)\" }\r\n"
            "        } else {\r\n"
            "            Copy-Item -LiteralPath $DownloadedFile -Destination $TargetExe -Force\r\n"
            "            if (!(Test-Path -LiteralPath $TargetExe)) {\r\n"
            "                throw \"Updated executable not found in target directory: $TargetExe\"\r\n"
            "            }\r\n"
            "        }\r\n"
            "    } else {\r\n"
            "        throw \"Unsupported package extension: $ext\"\r\n"
            "    }\r\n"
            "    if (Test-Path -LiteralPath $BackupExe) {\r\n"
            "        if (!(Test-Path -LiteralPath $TargetExe)) {\r\n"
            "            throw \"Updated executable not found in target directory: $TargetExe\"\r\n"
            "        }\r\n"
            "        Remove-Item -LiteralPath $BackupExe -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "} catch {\r\n"
            "    if (Test-Path -LiteralPath $BackupExe) {\r\n"
            "        Copy-Item -LiteralPath $BackupExe -Destination $TargetExe -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "    throw\r\n"
            "} finally {\r\n"
            "    if (Test-Path -LiteralPath $ExtractDir) {\r\n"
            "        Remove-Item -LiteralPath $ExtractDir -Recurse -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "}\r\n\r\n"
            "# (5) Relaunch ToriGraph.exe if requested\r\n"
            "if ($Relaunch -and (Test-Path -LiteralPath $TargetExe)) {\r\n"
            "    Start-Process -FilePath $TargetExe -WorkingDirectory $TargetInstallDir\r\n"
            "}\r\n");

    QFile psFile(ps1Path);
    if (!psFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut)
            *errOut = tr("Could not write PowerShell staging script: %1").arg(ps1Path);
        return QString();
    }
    // Write UTF-8 BOM so Windows PowerShell 5.1 parses Unicode paths accurately
    psFile.write("\xEF\xBB\xBF", 3);
    psFile.write(psScript.toUtf8());
    psFile.close();

    // Companion .cmd wrapper for convenience
    QFile cmdFile(cmdPath);
    if (cmdFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString cmdText = QStringLiteral(
                "@chcp 65001 >nul\r\n"
                "@echo off\r\n"
                "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%1\"\r\n")
                                        .arg(QDir::toNativeSeparators(ps1Path));
        cmdFile.write(cmdText.toUtf8());
        cmdFile.close();
    }

    return ps1Path;
}

void AutoUpdater::cancelActiveCheck()
{
    if (d_checkTimeoutTimer)
        d_checkTimeoutTimer->stop();
    if (d_activeCheckReply) {
        QNetworkReply *r = d_activeCheckReply;
        d_activeCheckReply = nullptr;
        r->disconnect(this);
        if (r->isRunning())
            r->abort();
        r->deleteLater();
    }
}

void AutoUpdater::checkForUpdates(bool silentIfUpToDate)
{
    // CRITICAL OLE / Batch / Headless Guard: zero network traffic & zero modal popups
    if (silentIfUpToDate && isHeadlessOrEmbeddedProcess(d_app))
        return;

    if (d_activeCheckReply)
        return;

    const QUrl targetUrl = feedUrl();
    if (!targetUrl.isValid() || targetUrl.isEmpty()) {
        if (!silentIfUpToDate) {
            handleCheckResult(false, ReleaseInfo(),
                              tr("Invalid update feed URL configured."), silentIfUpToDate);
        }
        return;
    }

    ++d_networkCheckCount;
    const bool allowPre = includePreReleases();

    if (targetUrl.isLocalFile()) {
        QFile localFeed(targetUrl.toLocalFile());
        if (!localFeed.open(QIODevice::ReadOnly)) {
            handleCheckResult(false, ReleaseInfo(),
                              tr("Could not open local release feed file: %1")
                                      .arg(targetUrl.toLocalFile()),
                              silentIfUpToDate);
            return;
        }
        const QByteArray raw = localFeed.readAll();
        localFeed.close();
        ReleaseInfo info;
        QString parseErr;
        const bool ok = parseGitHubReleaseJson(raw, &info, &parseErr, allowPre);
        handleCheckResult(ok, info, parseErr, silentIfUpToDate);
        return;
    }

    d_activeSilentMode = silentIfUpToDate;
    d_activeTimedOut = false;

    configureNetworkManagerForUrl(d_nam, targetUrl);
    const QNetworkRequest req = buildGitHubRequest(targetUrl, true);
    d_activeCheckReply = d_nam.get(req);
    d_checkTimeoutTimer->start(kLiveCheckTimeoutMs);

    connect(d_activeCheckReply, &QNetworkReply::finished, this, [this, allowPre]() {
        if (d_checkTimeoutTimer)
            d_checkTimeoutTimer->stop();
        QNetworkReply *reply = d_activeCheckReply;
        d_activeCheckReply = nullptr;
        if (!reply)
            return;

        const bool silent = d_activeSilentMode;
        const bool timedOut = d_activeTimedOut;
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        const QString netErrStr = reply->errorString();
        const QByteArray payload = reply->readAll();
        reply->deleteLater();

        if (timedOut) {
            handleCheckResult(false, ReleaseInfo(),
                              tr("Timed out while checking GitHub Releases (10s limit)."),
                              silent);
            return;
        }
        if (netErr != QNetworkReply::NoError || httpStatus != 200) {
            const QString apiMsg = (httpStatus > 0 && httpStatus != 200)
                    ? extractGitHubErrorMessage(payload)
                    : QString();
            const QString fullErr = !apiMsg.isEmpty()
                    ? tr("GitHub API error (HTTP %1): %2").arg(httpStatus).arg(apiMsg)
                    : tr("HTTP %1 error while checking updates: %2").arg(httpStatus).arg(netErrStr);
            handleCheckResult(false, ReleaseInfo(), fullErr, silent);
            return;
        }

        ReleaseInfo info;
        QString parseErr;
        const bool ok = parseGitHubReleaseJson(payload, &info, &parseErr, allowPre);
        handleCheckResult(ok, info, parseErr, silent);
    });
}

void AutoUpdater::handleCheckResult(bool ok,
                                    const ReleaseInfo &info,
                                    const QString &errorMsg,
                                    bool silentIfUpToDate)
{
    const QString curVer = currentAppVersion();
    const bool hasUpdate = ok && (compareSemVer(info.version, curVer) > 0);
    emit updateCheckFinished(ok, hasUpdate, info.version, errorMsg);

    if (!ok) {
        if (!silentIfUpToDate && !isHeadlessOrEmbeddedProcess(d_app)) {
            QMessageBox::warning(
                    d_app, tr("Update Check Failed"),
                    tr("Could not check for ToriGraph updates:\n%1").arg(errorMsg));
        }
        return;
    }

    if (!hasUpdate) {
        if (!silentIfUpToDate && !isHeadlessOrEmbeddedProcess(d_app)) {
            QMessageBox::information(
                    d_app, tr("ToriGraph is Up to Date"),
                    tr("You are already running the latest version of ToriGraph (v%1).")
                            .arg(curVer));
        }
        return;
    }

    if (silentIfUpToDate) {
        const QString skipped = skippedVersion();
        if (!skipped.isEmpty() && compareSemVer(info.version, skipped) == 0)
            return;
    }

    if (!isHeadlessOrEmbeddedProcess(d_app)) {
        UpdateAvailableDialog dlg(info, curVer, d_app, d_app);
        dlg.exec();
    }
}

QJsonObject AutoUpdater::checkFeedBlocking(const QUrl &overrideUrl, int timeoutMs)
{
    const QString curVer = currentAppVersion();
    const QUrl effectiveUrl = (overrideUrl.isValid() && !overrideUrl.isEmpty())
            ? overrideUrl
            : feedUrl();

    auto makeResult = [&](bool ok, const ReleaseInfo &info, const QString &err) -> QJsonObject {
        const bool updateAvail = ok && (compareSemVer(info.version, curVer) > 0);
        QJsonObject obj;
        obj.insert(QStringLiteral("ok"), ok);
        obj.insert(QStringLiteral("current_version"), curVer);
        obj.insert(QStringLiteral("latest_version"), info.version);
        obj.insert(QStringLiteral("update_available"), updateAvail);
        obj.insert(QStringLiteral("tag_name"), info.tagName);
        obj.insert(QStringLiteral("title"), info.title);
        obj.insert(QStringLiteral("asset_name"), info.assetName);
        obj.insert(QStringLiteral("asset_url"), info.assetUrl.toString());
        obj.insert(QStringLiteral("asset_size"), info.assetSize);
        obj.insert(QStringLiteral("sha256"), info.expectedSha256);
        obj.insert(QStringLiteral("release_notes"), info.bodyMarkdown);
        obj.insert(QStringLiteral("published_at"), info.publishedAt);
        obj.insert(QStringLiteral("html_url"), info.htmlUrl.toString());
        obj.insert(QStringLiteral("is_prerelease"), info.isPreRelease);
        obj.insert(QStringLiteral("feed_url"), effectiveUrl.toString());
        obj.insert(QStringLiteral("error"), err);
        return obj;
    };

    if (!effectiveUrl.isValid() || effectiveUrl.isEmpty())
        return makeResult(false, ReleaseInfo(), tr("Invalid update feed URL."));

    const bool allowPre = includePreReleases();

    if (effectiveUrl.isLocalFile()) {
        QFile f(effectiveUrl.toLocalFile());
        if (!f.open(QIODevice::ReadOnly)) {
            return makeResult(false, ReleaseInfo(),
                              tr("Could not read local feed file: %1")
                                      .arg(effectiveUrl.toLocalFile()));
        }
        const QByteArray bytes = f.readAll();
        f.close();
        ReleaseInfo info;
        QString parseErr;
        if (!parseGitHubReleaseJson(bytes, &info, &parseErr, allowPre))
            return makeResult(false, ReleaseInfo(), parseErr);
        return makeResult(true, info, QString());
    }

    QNetworkAccessManager localNam;
    configureNetworkManagerForUrl(localNam, effectiveUrl);
    const QNetworkRequest req = buildGitHubRequest(effectiveUrl, true);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;

    QNetworkReply *reply = localNam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        if (reply->isRunning())
            reply->abort();
        loop.quit();
    });

    timer.start(qMax(100, timeoutMs));
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    timer.stop();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrStr = reply->errorString();
    const QByteArray payload = reply->readAll();
    delete reply;

    if (timedOut) {
        return makeResult(false, ReleaseInfo(),
                          tr("Timed out after %1 ms fetching update feed.").arg(timeoutMs));
    }
    if (netErr != QNetworkReply::NoError || httpStatus != 200) {
        const QString apiMsg = (httpStatus > 0 && httpStatus != 200)
                ? extractGitHubErrorMessage(payload)
                : QString();
        const QString fullErr = !apiMsg.isEmpty()
                ? tr("GitHub API error (HTTP %1): %2").arg(httpStatus).arg(apiMsg)
                : tr("HTTP %1 error fetching update feed: %2").arg(httpStatus).arg(netErrStr);
        return makeResult(false, ReleaseInfo(), fullErr);
    }

    ReleaseInfo info;
    QString parseErr;
    if (!parseGitHubReleaseJson(payload, &info, &parseErr, allowPre))
        return makeResult(false, ReleaseInfo(), parseErr);

    return makeResult(true, info, QString());
}

bool AutoUpdater::downloadAndVerifyBlocking(const QUrl &assetUrl,
                                            const QString &destFilePath,
                                            const QString &expectedSha256,
                                            QString *errOut,
                                            int timeoutMs)
{
    if (!assetUrl.isValid() || assetUrl.isEmpty() || destFilePath.trimmed().isEmpty()) {
        if (errOut)
            *errOut = tr("Invalid asset URL or destination file path.");
        return false;
    }

    const QFileInfo destInfo(destFilePath);
    if (!destInfo.dir().exists() && !destInfo.dir().mkpath(QStringLiteral("."))) {
        if (errOut)
            *errOut = tr("Could not create destination directory: %1").arg(destInfo.dir().path());
        return false;
    }

    const QString partPath = destFilePath + QStringLiteral(".part");
    if (QFile::exists(partPath))
        QFile::remove(partPath);

    QFile partFile(partPath);
    if (!partFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errOut)
            *errOut = tr("Could not open temporary file for writing: %1").arg(partPath);
        return false;
    }

    if (assetUrl.isLocalFile()) {
        QFile src(assetUrl.toLocalFile());
        if (!src.open(QIODevice::ReadOnly)) {
            partFile.close();
            QFile::remove(partPath);
            if (errOut)
                *errOut = tr("Could not open source asset file: %1").arg(assetUrl.toLocalFile());
            return false;
        }
        QByteArray buf(static_cast<int>(kStreamChunkBytes), Qt::Uninitialized);
        while (true) {
            const qint64 n = src.read(buf.data(), buf.size());
            if (n < 0) {
                src.close();
                partFile.close();
                QFile::remove(partPath);
                if (errOut)
                    *errOut = tr("Read error while copying local asset file.");
                return false;
            }
            if (n == 0)
                break;
            if (partFile.write(buf.constData(), n) != n) {
                src.close();
                partFile.close();
                QFile::remove(partPath);
                if (errOut)
                    *errOut = tr("Write error while staging downloaded asset.");
                return false;
            }
        }
        src.close();
        partFile.flush();
        partFile.close();
    } else {
        QNetworkAccessManager localNam;
        configureNetworkManagerForUrl(localNam, assetUrl);
        const QNetworkRequest req = buildGitHubRequest(assetUrl, false);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        bool timedOut = false;
        bool writeFailed = false;

        QNetworkReply *reply = localNam.get(req);
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
            const QByteArray chunk = reply->readAll();
            if (!chunk.isEmpty()) {
                if (partFile.write(chunk) != chunk.size()) {
                    writeFailed = true;
                    reply->abort();
                }
            }
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            if (reply->isRunning())
                reply->abort();
            loop.quit();
        });

        timer.start(qMax(100, timeoutMs));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        timer.stop();

        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty() && !writeFailed) {
            if (partFile.write(tail) != tail.size())
                writeFailed = true;
        }
        partFile.flush();
        partFile.close();

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        const QString netErrStr = reply->errorString();
        delete reply;

        if (writeFailed) {
            QFile::remove(partPath);
            if (errOut)
                *errOut = tr("Disk write failure while downloading update asset.");
            return false;
        }
        if (timedOut) {
            QFile::remove(partPath);
            if (errOut)
                *errOut = tr("Asset download timed out after %1 ms.").arg(timeoutMs);
            return false;
        }
        if (netErr != QNetworkReply::NoError || httpStatus != 200) {
            const QString apiMsg = (httpStatus > 0 && httpStatus != 200)
                    ? extractGitHubErrorMessageFromFile(partPath)
                    : QString();
            QFile::remove(partPath);
            if (errOut) {
                *errOut = !apiMsg.isEmpty()
                        ? tr("GitHub API error (HTTP %1): %2").arg(httpStatus).arg(apiMsg)
                        : tr("Asset download failed (HTTP %1): %2").arg(httpStatus).arg(netErrStr);
            }
            return false;
        }
    }

    if (!expectedSha256.trimmed().isEmpty()) {
        if (!verifyFileSha256(partPath, expectedSha256)) {
            QFile::remove(partPath);
            if (errOut)
                *errOut = tr("SHA-256 checksum verification failed for downloaded update package.");
            return false;
        }
    }

    if (!atomicReplaceFile(partPath, destFilePath, errOut)) {
        QFile::remove(partPath);
        return false;
    }

    return true;
}
