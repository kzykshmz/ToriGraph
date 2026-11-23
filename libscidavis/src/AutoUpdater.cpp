/***************************************************************************
    File                 : AutoUpdater.cpp
    Project              : ToriGraph
    --------------------------------------------------------------------
    Description          : GitHub Releases Auto-Update Engine & Dialog
 ***************************************************************************/

#include "AutoUpdater.h"
#include "ApplicationWindow.h"
#include "IconLoader.h"
#include "I18nJapanese.h"
#include "globals.h"

#include <QApplication>
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
#include <QHash>
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
#include <QTemporaryFile>
#include <QTextBrowser>
#include <QTimer>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>
#include <QVector>

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#ifdef _MSC_VER
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#endif
#endif

namespace {

constexpr const char *kDefaultLatestUrl =
        "https://api.github.com/repos/kzykshmz/ToriGraph/releases/latest";
constexpr const char *kDefaultReleasesUrl =
        "https://api.github.com/repos/kzykshmz/ToriGraph/releases";
constexpr const char *kSettingsKeyFeedUrl = "AutoUpdate/FeedUrl";
constexpr const char *kSettingsKeyIncludePre = "AutoUpdate/IncludePreReleases";
constexpr const char *kSettingsKeySkippedVer = "AutoUpdate/SkippedVersion";
constexpr const char *kSettingsKeyAllowUnverified = "AutoUpdate/AllowUnverifiedInstall";
constexpr const char *kSettingsKeyRequireSig = "AutoUpdate/RequireCodeSignature";
constexpr const char *kSettingsKeyExpectedSigner = "AutoUpdate/ExpectedSignerSubject";
constexpr const char *kDefaultReleasesPage = "https://github.com/kzykshmz/ToriGraph/releases";
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

QString normalizeCertThumbprintHex40(const QString &raw)
{
    static const QRegularExpression kThumbRe(
            QStringLiteral("^(?:[0-9A-Fa-f]{40}|[0-9A-Fa-f]{2}(?:[:\\s][0-9A-Fa-f]{2}){19})$"));
    const QString t = raw.trimmed();
    if (!kThumbRe.match(t).hasMatch())
        return QString();
    QString hex;
    hex.reserve(40);
    for (const QChar c : t) {
        if (c != QLatin1Char(':') && !c.isSpace())
            hex.append(c.toUpper());
    }
    return (hex.size() == 40) ? hex : QString();
}

bool isX500DistinguishedNameSpec(const QString &raw)
{
    static const QRegularExpression kDnPrefixRe(
            QStringLiteral("^(?:CN|O|OU|C|L|S|ST|E|EMAILADDRESS|DC|UID|SERIALNUMBER|G|SN|T|OID\\.[0-9.]+)\\s*="),
            QRegularExpression::CaseInsensitiveOption);
    return kDnPrefixRe.match(raw.trimmed()).hasMatch();
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

// A copy of ToriGraph that was unpacked from the portable .zip has no Inno Setup uninstaller
// next to the executable. Such an install should be updated from the portable .zip again
// (running a setup.exe would create a second, registered installation).
bool isPortableInstall()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    return appDir.entryList(QStringList() << QStringLiteral("unins*.exe"), QDir::Files).isEmpty();
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
    if (isPortableInstall()) {
        if (lower.contains(QStringLiteral("portable")))
            score += 30;
        else if (lower.endsWith(QStringLiteral(".exe")) || lower.endsWith(QStringLiteral(".msi")))
            score -= 20;
    }
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

    // Pass 3 (a bare 64-hex line anywhere in the release notes) was removed on purpose:
    // an unrelated hash in the changelog must never be accepted as the installer digest.

    return QString();
}

bool isLoopbackHost(const QString &rawHost)
{
    const QString host = rawHost.trimmed().toLower();
    return host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost")
            || host == QStringLiteral("::1") || host == QStringLiteral("[::1]");
}

// Campus / corporate networks often only reach GitHub through the proxy configured
// in Windows (WinINET settings, WPAD/PAC). Qt does not use it unless asked to.
class UpdaterProxyFactory : public QNetworkProxyFactory
{
public:
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override
    {
        QUrl url = query.url();
        if (url.isEmpty() || url.host().isEmpty()) {
            url = QUrl();
            url.setScheme(query.protocolTag().isEmpty() ? QStringLiteral("https")
                                                        : query.protocolTag());
            url.setHost(query.peerHostName());
            if (query.peerPort() > 0)
                url.setPort(query.peerPort());
        }
        return AutoUpdater::proxiesForUrl(url);
    }
};

void configureNetworkManagerForUrl(QNetworkAccessManager &nam, const QUrl &url)
{
    AutoUpdater::applyNetworkProxyPolicy(nam, url);
}

void registerAutoUpdaterExtraTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Open Release Page", "リリースページを開く");
        reg("Update Not Installed", "更新はインストールされていません");
        reg("ToriGraph is still open, so the update was not installed. The downloaded package "
            "is kept; run \"Check for Updates\" again when you are ready to install it.",
            "ToriGraph が閉じられなかったため、更新はインストールしていません。"
            "ダウンロードしたパッケージは残してあります。インストールするときは"
            "「更新を確認」をもう一度実行してください。");
        reg("The update package (v%1) has been downloaded and its SHA-256 checksum verified.\n\n"
            "ToriGraph will now ask you to save unsaved changes and close. The update is applied "
            "only after ToriGraph has closed; ToriGraph then restarts automatically.\n\n"
            "Install update and restart now?",
            "更新パッケージ (v%1) をダウンロードし、SHA-256 チェックサムを検証しました。\n\n"
            "このあと未保存の変更を保存するか確認してから ToriGraph を閉じます。更新は "
            "ToriGraph が閉じてから適用され、その後自動で再起動します。\n\n"
            "今すぐ更新をインストールして再起動しますか？");
        reg("Install Unverified Update?", "検証できない更新をインストールしますか？");
        reg("The update package (v%1) has been downloaded, but it could NOT be verified because "
            "this release does not publish a SHA-256 checksum.\n\nInstall it anyway? ToriGraph "
            "will ask you to save unsaved changes, close, and then apply the update.",
            "更新パッケージ (v%1) をダウンロードしましたが、このリリースには SHA-256 "
            "チェックサムが公開されていないため、内容を検証できません。\n\n"
            "それでもインストールしますか？未保存の変更を保存するか確認してから ToriGraph "
            "を閉じ、更新を適用します。");
        reg("Download complete (not verified: no SHA-256 checksum published).",
            "ダウンロード完了 (SHA-256 チェックサムが公開されていないため未検証)");
        reg("This release does not publish a SHA-256 checksum, so ToriGraph cannot verify the "
            "download. Open the release page to download and install it manually.",
            "このリリースには SHA-256 チェックサムが公開されていないため、ダウンロードした"
            "内容を検証できません。リリースページを開いて手動でダウンロード・インストール"
            "してください。");
        reg("Could not launch Windows PowerShell to apply the staged update.",
            "更新を適用する Windows PowerShell を起動できませんでした。");
        reg("Could not find Windows PowerShell (powershell.exe), so the downloaded update "
            "cannot be applied automatically. ToriGraph stays open; download and install the "
            "package from the release page instead.",
            "Windows PowerShell (powershell.exe) が見つからないため、ダウンロードした更新を"
            "自動では適用できません。ToriGraph は開いたままです。リリースページからパッケージを"
            "ダウンロードしてインストールしてください。");
        reg("The staged update script was not found:\n%1\nToriGraph stays open; run \"Check "
            "for Updates\" again.",
            "更新を適用するスクリプトが見つかりません:\n%1\nToriGraph は開いたままです。"
            "「更新を確認」をもう一度実行してください。");
        reg("Administrator Rights Needed", "管理者権限が必要です");
        reg("ToriGraph is installed in a folder you cannot write to:\n%1\n\nWindows will ask for "
            "administrator permission (User Account Control) after ToriGraph has closed. If you "
            "cannot give that permission, choose No and ask the person who installed ToriGraph "
            "to update it.\n\nContinue?",
            "ToriGraph は書き込み権限のないフォルダーにインストールされています:\n%1\n\n"
            "ToriGraph を閉じたあと、Windows が管理者の許可 (ユーザー アカウント制御) を"
            "求めます。許可できない場合は「いいえ」を選び、ToriGraph をインストールした人に"
            "更新を依頼してください。\n\n続けますか？");
        reg("The update was not installed because administrator permission was not given (or "
            "Windows PowerShell could not be started).\n\nToriGraph was not changed. Restart "
            "ToriGraph now?",
            "管理者の許可が得られなかった (または Windows PowerShell を起動できなかった) ため、"
            "更新はインストールしていません。\n\nToriGraph は変更されていません。"
            "ToriGraph を再起動しますか？");
        reg("Could not launch Windows PowerShell to apply the staged update.\n\nToriGraph was "
            "not changed. Restart ToriGraph now?",
            "更新を適用する Windows PowerShell を起動できませんでした。\n\n"
            "ToriGraph は変更されていません。ToriGraph を再起動しますか？");
        reg("The proxy server asks for a user name and password, which the updater cannot "
            "enter. Open the release page in your web browser and download the package from "
            "there, or ask your network administrator.",
            "プロキシ サーバーがユーザー名とパスワードを求めていますが、更新機能からは入力"
            "できません。Web ブラウザーでリリースページを開いてパッケージをダウンロードするか、"
            "ネットワーク管理者に相談してください。");
        reg("Manual Update Required (Unsigned Package)", "手動更新が必要です(未署名パッケージ)");
        reg("The downloaded update package (v%1) is not digitally signed with an Authenticode "
            "certificate, so automatic installation is disabled for your safety.\n\n"
            "Verified SHA-256:\n%2\n\nRelease Notes:\n%3\n\n"
            "Click \"Open Release Page\" to review and install the update manually from GitHub.",
            "ダウンロードした更新パッケージ(v%1)に Authenticode 電子署名がないため、"
            "安全のため自動インストールは行いません。\n\n"
            "検証済み SHA-256:\n%2\n\nリリースノート:\n%3\n\n"
            "「リリースページを開く」を押して GitHub の公式リリースページから手動で更新してください。");
        reg("The downloaded update package (v%1) is not digitally signed with an Authenticode "
            "certificate, so automatic installation is disabled for your safety.\n\n"
            "Computed SHA-256 (unverified):\n%2\n\nRelease Notes:\n%3\n\n"
            "Click \"Open Release Page\" to review and install the update manually from GitHub.",
            "ダウンロードした更新パッケージ(v%1)に Authenticode 電子署名がないため、"
            "安全のため自動インストールは行いません。\n\n"
            "算出 SHA-256 (未検証):\n%2\n\nリリースノート:\n%3\n\n"
            "「リリースページを開く」を押して GitHub の公式リリースページから手動で更新してください。");
        reg("The downloaded update package has an invalid or untrusted digital signature and was "
            "deleted for your safety.\n\n%1",
            "ダウンロードした更新パッケージの電子署名が不正または信頼できないため、"
            "安全のため削除しました。\n\n%1");
        reg("Digital signature verification failed.", "電子署名の検証に失敗しました。");
        reg("Authenticode signer '%1' does not match expected signer '%2'.",
            "Authenticode 署名者 '%1' が期待される署名者 '%2' と一致しません。");
        reg("This release does not publish a SHA-256 checksum, so automatic installation is "
            "blocked.",
            "このリリースには SHA-256 チェックサムが公開されていないため、自動インストールはブロックされました。");
        reg("Blocked untrusted download URL: %1",
            "信頼できないダウンロード URL をブロックしました: %1");
        reg("Blocked untrusted update feed URL.",
            "信頼できない更新フィード URL をブロックしました。");
        reg("The staged update script could not be read, so the update was not started. "
            "ToriGraph stays open; run \"Check for Updates\" again.",
            "ステージング済みの更新スクリプトを読み取れなかったため、更新を開始しませんでした。"
            "ToriGraph は開いたままです。「更新を確認」をもう一度実行してください。");
    });
}

QString updateStagingDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                + QStringLiteral("/ToriGraph");
    if (base.isEmpty())
        base = QDir::tempPath() + QStringLiteral("/ToriGraph");
    return QDir(base).filePath(QStringLiteral("UpdateStaging"));
}

// Removes leftover per-run GUID directories (32 hex chars) older than a week, e.g. from an
// update that failed or was cancelled. Anything else in the staging folder is left alone.
void purgeStaleStagingSubdirs(const QString &stagingBase)
{
    static const QRegularExpression guidDirRe(QStringLiteral("^[0-9a-fA-F]{32}$"));
    const QDir base(stagingBase);
    if (!base.exists())
        return;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
    const QFileInfoList subs = base.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : subs) {
        if (!guidDirRe.match(fi.fileName()).hasMatch() || fi.isSymLink())
            continue;
        if (fi.lastModified() < cutoff)
            QDir(fi.absoluteFilePath()).removeRecursively();
    }
}

QString resolveSystem32Executable(const QString &exeLeaf)
{
#ifdef Q_OS_WIN
    wchar_t sysDirBuf[MAX_PATH + 4] = {};
    const UINT n = GetSystemDirectoryW(sysDirBuf, MAX_PATH + 1);
    if (n > 0 && n <= MAX_PATH) {
        const QString sysDir = QString::fromWCharArray(sysDirBuf, static_cast<int>(n));
        const QString cand = QDir(sysDir).filePath(exeLeaf);
        if (QFileInfo(cand).isFile())
            return QDir::toNativeSeparators(cand);
        return QDir::toNativeSeparators(cand);
    }
#endif
    return exeLeaf;
}

QString resolveWindowsExecutable(const QString &exeLeaf)
{
#ifdef Q_OS_WIN
    wchar_t winDirBuf[MAX_PATH + 4] = {};
    const UINT n = GetWindowsDirectoryW(winDirBuf, MAX_PATH + 1);
    if (n > 0 && n <= MAX_PATH) {
        const QString winDir = QString::fromWCharArray(winDirBuf, static_cast<int>(n));
        const QString cand = QDir(winDir).filePath(exeLeaf);
        if (QFileInfo(cand).isFile())
            return QDir::toNativeSeparators(cand);
        return QDir::toNativeSeparators(cand);
    }
#endif
    return exeLeaf;
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

bool isSignablePackageFile(const QString &filePath)
{
    QString lower = filePath.trimmed().toLower();
    if (lower.endsWith(QStringLiteral(".part")))
        lower.chop(5);
    return lower.endsWith(QStringLiteral(".exe")) || lower.endsWith(QStringLiteral(".msi"));
}

QString psSingleQuote(const QString &value)
{
    QString escaped;
    escaped.reserve(value.size() + 8);
    for (int i = 0; i < value.size(); ++i) {
        const QChar ch = value.at(i);
        const ushort u = ch.unicode();
        // PowerShell tokenizes ASCII ' plus Unicode U+2018..U+201B as single-quote delimiters.
        if (u == 0x0027 || u == 0x2018 || u == 0x2019 || u == 0x201A || u == 0x201B) {
            escaped.append(ch);
            escaped.append(ch);
        } else {
            escaped.append(ch);
        }
    }
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
    registerAutoUpdaterExtraTranslations();
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
    // Release notes come from the network: never let a link open arbitrary schemes/hosts
    // (file:, smb:, custom protocol handlers, look-alike domains). Only https links to
    // GitHub hosts are handed to the system browser.
    d_notesBrowser->setOpenLinks(false);
    d_notesBrowser->setOpenExternalLinks(false);
    connect(d_notesBrowser, &QTextBrowser::anchorClicked, this, [this](const QUrl &link) {
        if (link.scheme().isEmpty() && !link.fragment().isEmpty()) {
            d_notesBrowser->scrollToAnchor(link.fragment());
            return;
        }
        if (link.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
            && AutoUpdater::isAllowedUpdateUrl(link)) {
            QDesktopServices::openUrl(link);
        } else {
            qWarning("Blocked release-notes link: %s", qPrintable(link.toString()));
        }
    });
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

    if (d_info.assetUrl.isValid() && !d_info.assetUrl.isEmpty() && canInstallFromDialog())
        d_btnDownloadInstall->setText(tr("Download && Install Update"));
    else
        d_btnDownloadInstall->setText(tr("Open Release Page..."));
    // Without a published SHA-256 the package cannot be verified: by default the dialog only
    // offers the release page (AutoUpdate/AllowUnverifiedInstall=true re-enables installing).
    d_btnDownloadInstall->setToolTip(
            d_info.expectedSha256.trimmed().isEmpty()
                    ? tr("This release does not publish a SHA-256 checksum, so ToriGraph cannot "
                         "verify the download. Open the release page to download and install it "
                         "manually.")
                    : QString());
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
    if (!d_info.assetUrl.isValid() || d_info.assetUrl.isEmpty() || !canInstallFromDialog()) {
        QDesktopServices::openUrl(AutoUpdater::releasesPageUrl(d_info.htmlUrl));
        accept();
        return;
    }

    if (!AutoUpdater::isAllowedUpdateUrl(d_info.assetUrl)) {
        AutoUpdater::showFailureWithReleasePage(
                this, tr("Update Download Failed"),
                tr("Blocked untrusted download URL: %1").arg(d_info.assetUrl.toString()),
                d_info.htmlUrl);
        return;
    }

    const QString stagingBase = updateStagingDirectory();
    purgeStaleStagingSubdirs(stagingBase);
    const QString stagingDir =
            QDir(stagingBase).filePath(QUuid::createUuid().toString(QUuid::Id128));
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
    d_downloadBlockedRedirect = false;
    d_lastReceivedBytes = 0;
    d_lastTotalBytes = d_info.assetSize;

    // Async HTTP(loopback) / HTTPS(GitHub) download
    cleanupActiveDownload(true);
    d_partFile = new QFile(d_partFilePath);
    if (!d_partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete d_partFile;
        d_partFile = nullptr;
        AutoUpdater::showFailureWithReleasePage(
                this, tr("Update Download Failed"),
                tr("Cannot open temporary file for writing:\n%1").arg(d_partFilePath),
                d_info.htmlUrl);
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

    connect(d_activeReply, &QNetworkReply::redirected, this, [this](const QUrl &newUrl) {
        if (!AutoUpdater::isAllowedUpdateUrl(newUrl)) {
            d_downloadBlockedRedirect = true;
            if (d_activeReply)
                d_activeReply->abort();
        }
    });
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

    const QUrl finalReplyUrl = reply->url();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrStr = AutoUpdater::describeNetworkError(netErr, reply->errorString());
    reply->deleteLater();

    if (d_downloadCanceled) {
        QFile::remove(d_partFilePath);
        d_btnDownloadInstall->setEnabled(true);
        d_btnSkipVersion->setEnabled(true);
        d_btnLater->setEnabled(true);
        d_btnCancelDownload->setEnabled(false);
        return;
    }

    if (d_downloadBlockedRedirect || !AutoUpdater::isAllowedUpdateUrl(finalReplyUrl)) {
        QFile::remove(d_partFilePath);
        d_progressBar->setRange(0, 100);
        d_progressBar->setValue(0);
        restoreButtonsAfterFailure();
        const QString reason = tr("Blocked untrusted download URL: %1").arg(finalReplyUrl.toString());
        d_lblProgressDetail->setText(reason);
        AutoUpdater::showFailureWithReleasePage(this, tr("Update Download Failed"), reason,
                                                d_info.htmlUrl);
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
        AutoUpdater::showFailureWithReleasePage(this, tr("Update Download Failed"), reason,
                                                d_info.htmlUrl);
        return;
    }

    // Verify SHA-256 checksum if published (or reject if unverified installs are disallowed)
    const bool verified = !d_info.expectedSha256.trimmed().isEmpty();
    if (verified) {
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
    } else if (!AutoUpdater::allowUnverifiedInstall()) {
        QFile::remove(d_partFilePath);
        restoreButtonsAfterFailure();
        const QString reason = tr("This release does not publish a SHA-256 checksum, so automatic "
                                  "installation is blocked.");
        d_lblProgressDetail->setText(reason);
        AutoUpdater::showFailureWithReleasePage(this, tr("Security Verification Failed"), reason,
                                                d_info.htmlUrl);
        return;
    }

    // Verify Authenticode signature for PE/MSI packages (.zip archives are verified on extracted
    // executables inside the staged PowerShell script). Always reject tampered/invalid signatures.
    if (isSignablePackageFile(d_finalPackagePath)) {
        QString sigErr;
        const AutoUpdater::CodeSignatureStatus sigStatus =
                AutoUpdater::verifyFileCodeSignature(d_partFilePath, nullptr, &sigErr);
        if (sigStatus == AutoUpdater::CodeSignatureStatus::InvalidSignature) {
            QFile::remove(d_partFilePath);
            d_shaState = ShaBadgeState::Failed;
            updateShaBadgeStyle();
            restoreButtonsAfterFailure();
            d_lblProgressDetail->setText(tr("Digital signature verification failed."));
            QMessageBox::critical(
                    this, tr("Security Verification Failed"),
                    tr("The downloaded update package has an invalid or untrusted digital "
                       "signature and was deleted for your safety.\n\n%1")
                            .arg(sigErr));
            return;
        }
    }

    QString replaceErr;
    if (!atomicReplaceFile(d_partFilePath, d_finalPackagePath, &replaceErr)) {
        QFile::remove(d_partFilePath);
        d_btnDownloadInstall->setEnabled(true);
        d_btnSkipVersion->setEnabled(true);
        d_btnLater->setEnabled(true);
        d_btnCancelDownload->setEnabled(false);
        AutoUpdater::showFailureWithReleasePage(this, tr("Update Download Failed"), replaceErr,
                                                d_info.htmlUrl);
        return;
    }

    d_progressBar->setRange(0, 100);
    d_progressBar->setValue(100);
    d_btnCancelDownload->setEnabled(false);
    d_lblProgressDetail->setText(
            verified ? tr("Download complete and verified.")
                     : tr("Download complete (not verified: no SHA-256 checksum published)."));
    offerInstallAfterDownload(verified);
}

bool UpdateAvailableDialog::canInstallFromDialog() const
{
    return !d_info.expectedSha256.trimmed().isEmpty() || AutoUpdater::allowUnverifiedInstall();
}

void UpdateAvailableDialog::restoreButtonsAfterFailure()
{
    d_btnDownloadInstall->setEnabled(true);
    d_btnSkipVersion->setEnabled(true);
    d_btnLater->setEnabled(true);
    d_btnCancelDownload->setEnabled(false);
}

void UpdateAvailableDialog::offerInstallAfterDownload(bool verified)
{
    const bool interactiveGui = !isHeadlessOrEmbeddedProcess(d_app)
            && !ApplicationWindow::isNonInteractiveContext(d_app);
    const bool shaVerified = verified && !d_info.expectedSha256.trimmed().isEmpty()
            && AutoUpdater::verifyFileSha256(d_finalPackagePath, d_info.expectedSha256);

    if (!shaVerified && !AutoUpdater::allowUnverifiedInstall()) {
        QFile::remove(d_finalPackagePath);
        restoreButtonsAfterFailure();
        const QString reason = tr("This release does not publish a SHA-256 checksum, so automatic "
                                  "installation is blocked.");
        if (interactiveGui) {
            AutoUpdater::showFailureWithReleasePage(this, tr("Security Verification Failed"),
                                                    reason, d_info.htmlUrl);
        } else {
            qWarning("%s", qPrintable(reason));
        }
        return;
    }

    const QString actualSha = AutoUpdater::computeFileSha256(d_finalPackagePath);

    if (isSignablePackageFile(d_finalPackagePath)) {
        QString sigErr;
        const AutoUpdater::CodeSignatureStatus sigStatus =
                AutoUpdater::verifyFileCodeSignature(d_finalPackagePath, nullptr, &sigErr);
        if (sigStatus == AutoUpdater::CodeSignatureStatus::InvalidSignature) {
            QFile::remove(d_finalPackagePath);
            restoreButtonsAfterFailure();
            if (interactiveGui) {
                QMessageBox::critical(
                        this, tr("Security Verification Failed"),
                        tr("The downloaded update package has an invalid or untrusted digital "
                           "signature and was deleted for your safety.\n\n%1")
                                .arg(sigErr));
            } else {
                qWarning("Rejected update package with invalid signature: %s", qPrintable(sigErr));
            }
            return;
        }
        if (sigStatus == AutoUpdater::CodeSignatureStatus::Unsigned
            && AutoUpdater::requireCodeSignature()) {
            restoreButtonsAfterFailure();
            if (!interactiveGui) {
                QFile::remove(d_finalPackagePath);
                qWarning("Rejected unsigned update package in non-interactive mode: %s",
                         qPrintable(d_finalPackagePath));
                return;
            }
            QString notesPreview = d_info.bodyMarkdown.trimmed();
            if (notesPreview.isEmpty())
                notesPreview = tr("No release notes provided for this version.");
            else if (notesPreview.size() > 600)
                notesPreview = notesPreview.left(600) + QStringLiteral("...");

            const QUrl httpsReleasePage =
                    (d_info.htmlUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive)
                             == 0
                     && AutoUpdater::isAllowedUpdateUrl(d_info.htmlUrl))
                    ? d_info.htmlUrl
                    : QUrl(QString::fromLatin1(kDefaultReleasesPage));
            const QString displaySha = shaVerified
                    ? d_info.expectedSha256.trimmed().toLower()
                    : (actualSha.isEmpty() ? QStringLiteral("-") : actualSha);
            const QString msg = shaVerified
                    ? tr("The downloaded update package (v%1) is not digitally signed with an "
                         "Authenticode certificate, so automatic installation is disabled for your "
                         "safety.\n\nVerified SHA-256:\n%2\n\nRelease Notes:\n%3\n\n"
                         "Click \"Open Release Page\" to review and install the update manually "
                         "from GitHub.")
                              .arg(d_info.version, displaySha, notesPreview)
                    : tr("The downloaded update package (v%1) is not digitally signed with an "
                         "Authenticode certificate, so automatic installation is disabled for your "
                         "safety.\n\nComputed SHA-256 (unverified):\n%2\n\nRelease Notes:\n%3\n\n"
                         "Click \"Open Release Page\" to review and install the update manually "
                         "from GitHub.")
                              .arg(d_info.version, displaySha, notesPreview);
            if (AutoUpdater::showFailureWithReleasePage(
                        this, tr("Manual Update Required (Unsigned Package)"), msg,
                        httpsReleasePage)) {
                accept();
            }
            return;
        }
    }

    QString stageErr;
    const QString appDir = QCoreApplication::applicationDirPath();
    QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    if (exeName.isEmpty())
        exeName = QStringLiteral("ToriGraph.exe");

    const QString stageSha = shaVerified
            ? d_info.expectedSha256.trimmed()
            : (AutoUpdater::allowUnverifiedInstall() ? actualSha : QString());
    d_stagedScriptPath = AutoUpdater::writeWindowsStagingUpdaterScript(
            d_finalPackagePath, appDir, exeName, QCoreApplication::applicationPid(), true,
            &stageErr, 60, stageSha, AutoUpdater::requireCodeSignature(),
            AutoUpdater::expectedSignerSubject());
    if (d_stagedScriptPath.isEmpty()) {
        restoreButtonsAfterFailure();
        AutoUpdater::showFailureWithReleasePage(this, tr("Update Staging Error"), stageErr,
                                                d_info.htmlUrl);
        return;
    }

    QMessageBox::StandardButton ans = QMessageBox::No;
    if (verified) {
        ans = QMessageBox::question(
                this, tr("Install Update & Restart"),
                tr("The update package (v%1) has been downloaded and its SHA-256 checksum "
                   "verified.\n\nToriGraph will now ask you to save unsaved changes and close. "
                   "The update is applied only after ToriGraph has closed; ToriGraph then "
                   "restarts automatically.\n\nInstall update and restart now?")
                        .arg(d_info.version),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    } else {
        // Unverified packages are never applied by default: "No" is the default button.
        ans = QMessageBox::warning(
                this, tr("Install Unverified Update?"),
                tr("The update package (v%1) has been downloaded, but it could NOT be verified "
                   "because this release does not publish a SHA-256 checksum.\n\nInstall it "
                   "anyway? ToriGraph will ask you to save unsaved changes, close, and then "
                   "apply the update.")
                        .arg(d_info.version),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    }
    if (ans != QMessageBox::Yes) {
        restoreButtonsAfterFailure();
        return;
    }

    // The script must not start before ToriGraph has actually closed (save confirmation
    // first); closeAppAndLaunchUpdater() does that once this dialog has returned.
    const QPointer<ApplicationWindow> app = d_app;
    const QString script = d_stagedScriptPath;
    const QUrl page = AutoUpdater::releasesPageUrl(d_info.htmlUrl);
    accept();
    QObject *ctx = app ? static_cast<QObject *>(app.data())
                       : static_cast<QObject *>(QCoreApplication::instance());
    QTimer::singleShot(0, ctx, [app, script, page]() {
        AutoUpdater::closeAppAndLaunchUpdater(app.data(), script, page);
    });
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
    registerAutoUpdaterExtraTranslations();
    static std::once_flag s_purgeOnce;
    std::call_once(s_purgeOnce, []() { purgeStaleStagingSubdirs(updateStagingDirectory()); });
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

bool AutoUpdater::isAllowedUpdateUrl(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || url.isLocalFile())
        return false;
    if (!url.userInfo().isEmpty())
        return false;
    const QString scheme = url.scheme().trimmed().toLower();
    const QString host = url.host().trimmed().toLower();
    if (scheme.isEmpty() || host.isEmpty() || scheme == QStringLiteral("file"))
        return false;
    if (scheme == QStringLiteral("https")) {
        return host == QStringLiteral("api.github.com")
                || host == QStringLiteral("github.com")
                || host == QStringLiteral("objects.githubusercontent.com")
                || host == QStringLiteral("release-assets.githubusercontent.com")
                || host == QStringLiteral("codeload.github.com");
    }
    if (scheme == QStringLiteral("http")) {
        return host == QStringLiteral("127.0.0.1")
                || host == QStringLiteral("::1")
                || host == QStringLiteral("[::1]")
                || host == QStringLiteral("localhost");
    }
    return false;
}

QUrl AutoUpdater::parseFlexibleUrl(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return QUrl();
    if (trimmed.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("\\\\"))
        || (trimmed.size() >= 2 && trimmed.at(0).isLetter() && trimmed.at(1) == QLatin1Char(':'))
        || QFileInfo::exists(trimmed)) {
        return QUrl();
    }
    QUrl candidate;
    if (trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        candidate = QUrl(trimmed, QUrl::StrictMode);
    } else {
        return QUrl();
    }
    if (!isAllowedUpdateUrl(candidate))
        return QUrl();
    return candidate;
}

void AutoUpdater::setFeedUrl(const QUrl &url)
{
    d_feedUrlOverride = url;
}

QUrl AutoUpdater::feedUrl() const
{
    // Plaintext http:// feeds (loopback test servers) are only honoured for headless /
    // non-interactive runs (tests, automation). In the interactive GUI a user could be
    // tricked into an http feed through an environment variable or a stale setting.
    const bool interactiveGui = !isHeadlessOrEmbeddedProcess(d_app)
            && !ApplicationWindow::isNonInteractiveContext(d_app);
    const auto httpBlockedHere = [interactiveGui](const QUrl &u) {
        return interactiveGui
                && u.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0;
    };

    // The programmatic override (tests / embedding code, never user-controlled) keeps its
    // loopback http allowance.
    if (!d_feedUrlOverride.isEmpty()) {
        return isAllowedUpdateUrl(d_feedUrlOverride) ? d_feedUrlOverride : QUrl();
    }

    const QString envFeed = qEnvironmentVariable("TORIGRAPH_UPDATE_FEED_URL").trimmed();
    if (!envFeed.isEmpty()) {
        const QUrl envUrl = parseFlexibleUrl(envFeed);
        if (isAllowedUpdateUrl(envUrl) && !httpBlockedHere(envUrl))
            return envUrl;
        return QUrl();
    }

    const QString settingsFeed = ApplicationWindow::getSettings()
                                         .value(QString::fromLatin1(kSettingsKeyFeedUrl))
                                         .toString()
                                         .trimmed();
    if (!settingsFeed.isEmpty()) {
        const QUrl cfgUrl = parseFlexibleUrl(settingsFeed);
        if (isAllowedUpdateUrl(cfgUrl) && !httpBlockedHere(cfgUrl))
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

    const QString rawHtmlUrl = selectedRel.value(QStringLiteral("html_url")).toString().trimmed();
    if (!rawHtmlUrl.isEmpty()) {
        const QUrl candHtml = parseFlexibleUrl(rawHtmlUrl);
        if (!isAllowedUpdateUrl(candHtml)) {
            if (errOut)
                *errOut = tr("Blocked untrusted release page URL in feed: %1").arg(rawHtmlUrl);
            return false;
        }
        info.htmlUrl = candHtml;
    }
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
        if (!dlUrl.isEmpty()) {
            const QUrl candAssetUrl = parseFlexibleUrl(dlUrl);
            if (!isAllowedUpdateUrl(candAssetUrl)) {
                if (errOut)
                    *errOut = tr("Blocked untrusted download URL: %1").arg(dlUrl);
                return false;
            }
            info.assetUrl = candAssetUrl;
        }
        info.assetSize = bestAssetObj.value(QStringLiteral("size")).toVariant().toLongLong();
    } else if (selectedRel.contains(QStringLiteral("asset_url"))) {
        const QString rawAssetUrl =
                selectedRel.value(QStringLiteral("asset_url")).toString().trimmed();
        if (!rawAssetUrl.isEmpty()) {
            const QUrl candAssetUrl = parseFlexibleUrl(rawAssetUrl);
            if (!isAllowedUpdateUrl(candAssetUrl)) {
                if (errOut)
                    *errOut = tr("Blocked untrusted download URL: %1").arg(rawAssetUrl);
                return false;
            }
            info.assetUrl = candAssetUrl;
        }
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
        return false; // never report "verified" when no digest was published

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

bool AutoUpdater::requireCodeSignature()
{
    return ApplicationWindow::getSettings()
            .value(QString::fromLatin1(kSettingsKeyRequireSig), false)
            .toBool();
}

QString AutoUpdater::expectedSignerSubject()
{
    return ApplicationWindow::getSettings()
            .value(QString::fromLatin1(kSettingsKeyExpectedSigner))
            .toString()
            .trimmed();
}

AutoUpdater::CodeSignatureStatus AutoUpdater::verifyFileCodeSignature(
        const QString &filePath, QString *signerSubjectOut, QString *errOut)
{
    if (signerSubjectOut)
        signerSubjectOut->clear();
    if (errOut)
        errOut->clear();

    const QFileInfo fi(filePath);
    if (filePath.trimmed().isEmpty() || !fi.exists() || !fi.isFile()) {
        if (errOut)
            *errOut = tr("Update package file does not exist: %1").arg(filePath);
        return CodeSignatureStatus::InvalidSignature;
    }

#ifdef Q_OS_WIN
    const QString expectedSubj = expectedSignerSubject();
    QString canonPath = fi.canonicalFilePath();
    if (canonPath.isEmpty())
        canonPath = fi.absoluteFilePath();
    const QString normalizedCanonPath = QDir::cleanPath(canonPath).toLower();
    const qint64 fileSize = fi.size();
    const qint64 lastModMsecs = fi.lastModified().toMSecsSinceEpoch();

    const std::wstring nativePath = QDir::toNativeSeparators(fi.absoluteFilePath()).toStdWString();
    quint64 winWriteTicks100ns = 0;
    {
        WIN32_FILE_ATTRIBUTE_DATA fad = {};
        if (GetFileAttributesExW(nativePath.c_str(), GetFileExInfoStandard, &fad)) {
            winWriteTicks100ns = (static_cast<quint64>(fad.ftLastWriteTime.dwHighDateTime) << 32)
                    | static_cast<quint64>(fad.ftLastWriteTime.dwLowDateTime);
        }
    }

    struct CachedSigVerification
    {
        CodeSignatureStatus status = CodeSignatureStatus::InvalidSignature;
        QString signerSubject;
        QString errText;
    };
    static std::mutex s_sigCacheMutex;
    static QHash<QString, CachedSigVerification> s_sigCache;

    const QString cacheKey = normalizedCanonPath + QChar(0) + QString::number(fileSize) + QChar(0)
            + QString::number(lastModMsecs) + QChar(0) + QString::number(winWriteTicks100ns)
            + QChar(0) + expectedSubj;
    {
        std::lock_guard<std::mutex> lock(s_sigCacheMutex);
        const auto it = s_sigCache.constFind(cacheKey);
        if (it != s_sigCache.constEnd()) {
            if (signerSubjectOut)
                *signerSubjectOut = it->signerSubject;
            if (errOut)
                *errOut = it->errText;
            return it->status;
        }
    }

    auto storeAndReturn = [&](CodeSignatureStatus st, const QString &subj,
                              const QString &err) -> CodeSignatureStatus {
        {
            std::lock_guard<std::mutex> lock(s_sigCacheMutex);
            if (s_sigCache.size() >= 64)
                s_sigCache.clear();
            s_sigCache.insert(cacheKey, CachedSigVerification { st, subj, err });
        }
        if (signerSubjectOut)
            *signerSubjectOut = subj;
        if (errOut)
            *errOut = err;
        return st;
    };

    auto runVerifyAttempt = [&](DWORD revokeChecks, DWORD provFlags, QString *outSimpleSubj,
                                QString *outFullDnRev, QString *outFullDnFwd,
                                QString *outThumbprint, DWORD *outLastErr) -> LONG {
        if (outSimpleSubj)
            outSimpleSubj->clear();
        if (outFullDnRev)
            outFullDnRev->clear();
        if (outFullDnFwd)
            outFullDnFwd->clear();
        if (outThumbprint)
            outThumbprint->clear();
        WINTRUST_FILE_INFO fileInfo = {};
        fileInfo.cbStruct = sizeof(fileInfo);
        fileInfo.pcwszFilePath = nativePath.c_str();
        fileInfo.hFile = nullptr;
        fileInfo.pgKnownSubject = nullptr;

        WINTRUST_DATA wtd = {};
        wtd.cbStruct = sizeof(wtd);
        wtd.pPolicyCallbackData = nullptr;
        wtd.pSIPClientData = nullptr;
        wtd.dwUIChoice = WTD_UI_NONE;
        wtd.fdwRevocationChecks = revokeChecks;
        wtd.dwUnionChoice = WTD_CHOICE_FILE;
        wtd.pFile = &fileInfo;
        wtd.dwStateAction = WTD_STATEACTION_VERIFY;
        wtd.hWVTStateData = nullptr;
        wtd.pwszURLReference = nullptr;
        wtd.dwProvFlags = provFlags;
        wtd.dwUIContext = 0;

        GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        const LONG st = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &wtd);
        if (outLastErr)
            *outLastErr = GetLastError();

        if (st == ERROR_SUCCESS && wtd.hWVTStateData) {
            if (CRYPT_PROVIDER_DATA *provData = WTHelperProvDataFromStateData(wtd.hWVTStateData)) {
                if (CRYPT_PROVIDER_SGNR *provSigner =
                            WTHelperGetProvSignerFromChain(provData, 0, FALSE, 0)) {
                    if (provSigner->pasCertChain && provSigner->csCertChain > 0
                        && provSigner->pasCertChain[0].pCert) {
                        PCCERT_CONTEXT pCert = provSigner->pasCertChain[0].pCert;
                        if (outSimpleSubj) {
                            const DWORD reqCch = CertGetNameStringW(
                                    pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
                            if (reqCch > 1) {
                                QVector<wchar_t> subjBuf(static_cast<int>(reqCch), L'\0');
                                const DWORD cch = CertGetNameStringW(
                                        pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr,
                                        subjBuf.data(), reqCch);
                                if (cch > 1)
                                    *outSimpleSubj = QString::fromWCharArray(subjBuf.constData()).trimmed();
                            }
                        }

                        auto extractDn = [&](DWORD strType) -> QString {
                            const DWORD reqCch = CertNameToStrW(
                                    X509_ASN_ENCODING, &pCert->pCertInfo->Subject, strType, nullptr, 0);
                            if (reqCch <= 1)
                                return QString();
                            QVector<wchar_t> dnBuf(static_cast<int>(reqCch), L'\0');
                            const DWORD cch = CertNameToStrW(
                                    X509_ASN_ENCODING, &pCert->pCertInfo->Subject, strType,
                                    dnBuf.data(), reqCch);
                            return (cch > 1) ? QString::fromWCharArray(dnBuf.constData()).trimmed()
                                             : QString();
                        };
                        if (outFullDnRev)
                            *outFullDnRev = extractDn(CERT_X500_NAME_STR | CERT_NAME_STR_REVERSE_FLAG);
                        if (outFullDnFwd)
                            *outFullDnFwd = extractDn(CERT_X500_NAME_STR);

                        if (outThumbprint) {
                            BYTE sha1Hash[64] = {};
                            DWORD cbSha1 = sizeof(sha1Hash);
                            if (CertGetCertificateContextProperty(pCert, CERT_HASH_PROP_ID,
                                                                  sha1Hash, &cbSha1)
                                && cbSha1 == 20) {
                                *outThumbprint = QString::fromLatin1(
                                                         QByteArray(reinterpret_cast<const char *>(sha1Hash), 20)
                                                                 .toHex())
                                                         .toUpper();
                            }
                        }
                    }
                }
            }
        }

        wtd.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &wtd);
        return st;
    };

    QString simpleSubject;
    QString fullSubjectDnRev;
    QString fullSubjectDnFwd;
    QString certThumbprint;
    DWORD winLastErr = 0;

    // GUI-safe verification: the GUI thread passes WTD_CACHE_ONLY_URL_RETRIEVAL to avoid blocking
    // the UI when CRL/OCSP servers are unreachable, rejecting cached CRYPT_E_REVOKED immediately
    // and falling back to WTD_REVOKE_NONE only on CRYPT_E_REVOCATION_OFFLINE /
    // CERT_E_REVOCATION_FAILURE. The separate PowerShell staging script runs
    // Get-AuthenticodeSignature plus an explicit X509Chain (RevocationMode::Online, EntireChain,
    // 15s timeout) check that rejects Revoked certificates before touching $TargetInstallDir.
    LONG status = runVerifyAttempt(WTD_REVOKE_WHOLECHAIN,
                                   WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL,
                                   &simpleSubject, &fullSubjectDnRev, &fullSubjectDnFwd,
                                   &certThumbprint, &winLastErr);
    HRESULT hr = static_cast<HRESULT>(status);
    if (hr == CRYPT_E_REVOCATION_OFFLINE || hr == CERT_E_REVOCATION_FAILURE) {
        status = runVerifyAttempt(WTD_REVOKE_NONE,
                                  WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL,
                                  &simpleSubject, &fullSubjectDnRev, &fullSubjectDnFwd,
                                  &certThumbprint, &winLastErr);
        hr = static_cast<HRESULT>(status);
    }

    if (status == ERROR_SUCCESS) {
        const QString resolvedDisp = !simpleSubject.isEmpty()
                ? simpleSubject
                : (!fullSubjectDnRev.isEmpty() ? fullSubjectDnRev : fullSubjectDnFwd);
        if (!expectedSubj.isEmpty()) {
            const QString expectedThumb = normalizeCertThumbprintHex40(expectedSubj);
            bool matched = false;
            if (!expectedThumb.isEmpty()) {
                matched = !certThumbprint.isEmpty()
                        && certThumbprint.compare(expectedThumb, Qt::CaseInsensitive) == 0;
            } else if (isX500DistinguishedNameSpec(expectedSubj)) {
                matched = (!fullSubjectDnRev.isEmpty()
                           && fullSubjectDnRev.compare(expectedSubj, Qt::CaseInsensitive) == 0)
                        || (!fullSubjectDnFwd.isEmpty()
                            && fullSubjectDnFwd.compare(expectedSubj, Qt::CaseInsensitive) == 0);
            } else {
                matched = !simpleSubject.isEmpty()
                        && simpleSubject.compare(expectedSubj, Qt::CaseInsensitive) == 0;
            }
            if (!matched) {
                const QString actualDisp = !fullSubjectDnRev.isEmpty()
                        ? fullSubjectDnRev
                        : (!fullSubjectDnFwd.isEmpty() ? fullSubjectDnFwd : simpleSubject);
                return storeAndReturn(
                        CodeSignatureStatus::InvalidSignature, QString(),
                        tr("Authenticode signer '%1' does not match expected signer '%2'.")
                                .arg(actualDisp, expectedSubj));
            }
        }
        return storeAndReturn(CodeSignatureStatus::Valid, resolvedDisp, QString());
    }

    if (hr == TRUST_E_NOSIGNATURE || hr == TRUST_E_SUBJECT_FORM_UNKNOWN
        || hr == TRUST_E_PROVIDER_UNKNOWN) {
        if (winLastErr == static_cast<DWORD>(TRUST_E_BAD_DIGEST)
            || winLastErr == static_cast<DWORD>(TRUST_E_EXPLICIT_DISTRUST)
            || winLastErr == static_cast<DWORD>(CRYPT_E_SECURITY_SETTINGS)) {
            return storeAndReturn(
                    CodeSignatureStatus::InvalidSignature, QString(),
                    tr("Authenticode signature verification failed (0x%1).")
                            .arg(static_cast<quint32>(winLastErr), 8, 16, QLatin1Char('0')));
        }
        return storeAndReturn(
                CodeSignatureStatus::Unsigned, QString(),
                tr("The update package does not carry an Authenticode digital signature."));
    }

    return storeAndReturn(
            CodeSignatureStatus::InvalidSignature, QString(),
            tr("Authenticode signature is invalid or untrusted (0x%1).")
                    .arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0')));
#else
    if (errOut)
        *errOut = tr("Authenticode signature verification is only available on Windows.");
    return CodeSignatureStatus::Unsigned;
#endif
}

QString AutoUpdater::writeWindowsStagingUpdaterScript(const QString &downloadedFilePath,
                                                      const QString &targetInstallDir,
                                                      const QString &exeName,
                                                      qint64 waitForPid,
                                                      bool relaunch,
                                                      QString *errOut,
                                                      int waitTimeoutSec,
                                                      const QString &expectedSha256,
                                                      bool requireSignature,
                                                      const QString &expectedSigner)
{
    const QString pkgPath = QDir::toNativeSeparators(downloadedFilePath.trimmed());
    const QString installDir = QDir::toNativeSeparators(targetInstallDir.trimmed());
    const QString binaryName = exeName.trimmed().isEmpty()
            ? QStringLiteral("ToriGraph.exe")
            : QFileInfo(exeName.trimmed()).fileName();
    const QString normalizedSha = expectedSha256.trimmed().isEmpty()
            ? QString()
            : normalizeSha256Token(expectedSha256);
    if (!expectedSha256.trimmed().isEmpty() && normalizedSha.isEmpty()) {
        if (errOut)
            *errOut = tr("Invalid expected SHA-256 digest supplied for staging script.");
        return QString();
    }

    if (pkgPath.isEmpty() || installDir.isEmpty()) {
        if (errOut)
            *errOut = tr("Invalid paths supplied for Windows staging updater script.");
        return QString();
    }

    // Every staged script lives in its own GUID sub-directory of the staging folder so that
    // a predictable path cannot be pre-created/replaced by another process, and so that the
    // whole directory can be removed once the update has been applied.
    const QString stagingBase = updateStagingDirectory();
    purgeStaleStagingSubdirs(stagingBase);
    static const QRegularExpression guidDirRe(QStringLiteral("^[0-9a-fA-F]{32}$"));
    const QFileInfo pkgParentFi(QFileInfo(downloadedFilePath.trimmed()).absolutePath());
    const bool alreadyInGuidSubdir =
            guidDirRe.match(pkgParentFi.fileName()).hasMatch()
            && QDir::cleanPath(pkgParentFi.absolutePath())
                       .compare(QDir::cleanPath(stagingBase), Qt::CaseInsensitive)
                    == 0;
    const QString stagingDir = alreadyInGuidSubdir
            ? pkgParentFi.absoluteFilePath()
            : QDir(stagingBase).filePath(QUuid::createUuid().toString(QUuid::Id128));
    if (!QDir().mkpath(stagingDir)) {
        if (errOut)
            *errOut = tr("Could not create staging directory: %1").arg(stagingDir);
        return QString();
    }

    const QString ps1Path = QDir(stagingDir).filePath(QStringLiteral("apply_torigraph_update.ps1"));
    const QString nativeStagingDir = QDir::toNativeSeparators(stagingDir);
    const QString nativeStagingBase = QDir::toNativeSeparators(stagingBase);
    const QString msiExecAbs = resolveSystem32Executable(QStringLiteral("msiexec.exe"));
    const QString explorerExeAbs = resolveWindowsExecutable(QStringLiteral("explorer.exe"));

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
    psScript += QStringLiteral("$StagingRoot = %1\r\n").arg(psSingleQuote(nativeStagingDir));
    psScript += QStringLiteral("$StagingBase = %1\r\n").arg(psSingleQuote(nativeStagingBase));
    psScript += QStringLiteral("$ExpectedSha256 = %1\r\n").arg(psSingleQuote(normalizedSha));
    psScript += QStringLiteral("$RequireSignature = $%1\r\n")
                        .arg(requireSignature ? QStringLiteral("true") : QStringLiteral("false"));
    psScript += QStringLiteral("$ExpectedSignerSubject = %1\r\n")
                        .arg(psSingleQuote(expectedSigner.trimmed()));
    psScript += QStringLiteral("$MsiExecPath = %1\r\n").arg(psSingleQuote(msiExecAbs));
    psScript += QStringLiteral("$ExplorerExePath = %1\r\n").arg(psSingleQuote(explorerExeAbs));
    psScript += QStringLiteral("$WaitTimeoutSec = %1\r\n\r\n").arg(qMax(1, waitTimeoutSec));

    psScript += QStringLiteral(
            "$OrigDownloadedFile = $DownloadedFile\r\n"
            "$TargetExe = [System.IO.Path]::Combine($TargetInstallDir, $ExeName)\r\n"
            "$BackupExe = \"$TargetExe.rollback.bak\"\r\n"
            "$LogFile = [System.IO.Path]::Combine($StagingRoot, 'apply_torigraph_update.log')\r\n"
            "$SecureStageDir = ''\r\n"
            "$ExtractDir = ''\r\n"
            "$BackupDir = ''\r\n\r\n"
            "function Invoke-RelaunchTargetExe {\r\n"
            "    if ($Relaunch -and (Test-Path -LiteralPath $TargetExe -PathType Leaf)) {\r\n"
            "        $elevated = $false\r\n"
            "        try { $elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) } catch { }\r\n"
            "        if ($elevated) {\r\n"
            "            # Started elevated (install folder needed admin rights): let Explorer start\r\n"
            "            # ToriGraph so it runs with the user's normal rights again.\r\n"
            "            Start-Process -FilePath $ExplorerExePath -ArgumentList ('\"' + $TargetExe + '\"')\r\n"
            "        } else {\r\n"
            "            Start-Process -FilePath $TargetExe -WorkingDirectory $TargetInstallDir\r\n"
            "        }\r\n"
            "    }\r\n"
            "}\r\n\r\n"
            "function Test-AuthenticodeFile([string]$FilePath, [bool]$MustBeSigned, [bool]$CheckSubject) {\r\n"
            "    $isPe = $false\r\n"
            "    try {\r\n"
            "        $fs = [System.IO.File]::OpenRead($FilePath)\r\n"
            "        try {\r\n"
            "            if ($fs.Length -ge 2) {\r\n"
            "                $b0 = $fs.ReadByte()\r\n"
            "                $b1 = $fs.ReadByte()\r\n"
            "                if ($b0 -eq 0x4D -and $b1 -eq 0x5A) { $isPe = $true }\r\n"
            "            }\r\n"
            "        } finally {\r\n"
            "            $fs.Dispose()\r\n"
            "        }\r\n"
            "    } catch { }\r\n"
            "    $sig = Get-AuthenticodeSignature -LiteralPath $FilePath\r\n"
            "    if ($null -eq $sig) {\r\n"
            "        if ($OrigDownloadedFile -and (Test-Path -LiteralPath $OrigDownloadedFile -PathType Leaf)) {\r\n"
            "            Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "        }\r\n"
            "        throw \"Could not inspect Authenticode signature of $FilePath\"\r\n"
            "    }\r\n"
            "    if ($sig.Status -eq 'Valid') {\r\n"
            "        if ($null -ne $sig.SignerCertificate) {\r\n"
            "            $chain = $null\r\n"
            "            try {\r\n"
            "                $chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain\r\n"
            "                $chain.ChainPolicy.RevocationMode = [System.Security.Cryptography.X509Certificates.X509RevocationMode]::Online\r\n"
            "                $chain.ChainPolicy.RevocationFlag = [System.Security.Cryptography.X509Certificates.X509RevocationFlag]::EntireChain\r\n"
            "                $chain.ChainPolicy.UrlRetrievalTimeout = [TimeSpan]::FromSeconds(15)\r\n"
            "                [void]$chain.Build($sig.SignerCertificate)\r\n"
            "                foreach ($st in $chain.ChainStatus) {\r\n"
            "                    if (($st.Status -band [System.Security.Cryptography.X509Certificates.X509ChainStatusFlags]::Revoked) -ne 0) {\r\n"
            "                        if ($OrigDownloadedFile -and (Test-Path -LiteralPath $OrigDownloadedFile -PathType Leaf)) {\r\n"
            "                            Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "                        }\r\n"
            "                        throw \"Authenticode certificate is revoked ($($st.StatusInformation.Trim())) on $FilePath\"\r\n"
            "                    }\r\n"
            "                }\r\n"
            "            } finally {\r\n"
            "                if ($null -ne $chain) { $chain.Dispose() }\r\n"
            "            }\r\n"
            "        }\r\n"
            "        if ($CheckSubject -and $ExpectedSignerSubject.Length -gt 0) {\r\n"
            "            $cert = $sig.SignerCertificate\r\n"
            "            $simpleName = ''\r\n"
            "            if ($null -ne $cert) {\r\n"
            "                try { $simpleName = ([string]$cert.GetNameInfo([System.Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false)).Trim() } catch { $simpleName = '' }\r\n"
            "            }\r\n"
            "            $subj = if ($null -ne $cert -and $null -ne $cert.Subject) { ([string]$cert.Subject).Trim() } else { '' }\r\n"
            "            $subjFwd = ''\r\n"
            "            if ($null -ne $cert -and $null -ne $cert.SubjectName) {\r\n"
            "                try { $subjFwd = ([string]$cert.SubjectName.Format($false)).Trim() } catch { $subjFwd = '' }\r\n"
            "            }\r\n"
            "            $thumb = if ($null -ne $cert -and $null -ne $cert.Thumbprint) { ([string]$cert.Thumbprint).Trim() } else { '' }\r\n"
            "            $exp = $ExpectedSignerSubject.Trim()\r\n"
            "            $expHex = ($exp -replace '[:\\s]', '')\r\n"
            "            $signerMatched = $false\r\n"
            "            if ($exp -match '^(?:[0-9A-Fa-f]{40}|[0-9A-Fa-f]{2}(?:[:\\s][0-9A-Fa-f]{2}){19})$') {\r\n"
            "                $signerMatched = ($thumb.Length -eq 40) -and ($thumb -ieq $expHex)\r\n"
            "            } elseif ($exp -match '^(?:CN|O|OU|C|L|S|ST|E|EMAILADDRESS|DC|UID|SERIALNUMBER|G|SN|T|OID\\.[0-9.]+)\\s*=') {\r\n"
            "                $signerMatched = (($subj.Length -gt 0) -and ($subj -ieq $exp)) -or (($subjFwd.Length -gt 0) -and ($subjFwd -ieq $exp))\r\n"
            "            } else {\r\n"
            "                $signerMatched = ($simpleName.Length -gt 0) -and ($simpleName -ieq $exp)\r\n"
            "            }\r\n"
            "            if (-not $signerMatched) {\r\n"
            "                if ($OrigDownloadedFile -and (Test-Path -LiteralPath $OrigDownloadedFile -PathType Leaf)) {\r\n"
            "                    Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "                }\r\n"
            "                throw \"Authenticode signer '$subj' does not match expected '$ExpectedSignerSubject' on $FilePath\"\r\n"
            "            }\r\n"
            "        }\r\n"
            "        return\r\n"
            "    }\r\n"
            "    if (($isPe -or ($null -ne $sig.SignerCertificate)) -and ($sig.Status -ne 'NotSigned')) {\r\n"
            "        if ($OrigDownloadedFile -and (Test-Path -LiteralPath $OrigDownloadedFile -PathType Leaf)) {\r\n"
            "            Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "        }\r\n"
            "        throw \"Invalid or untrusted Authenticode signature ($($sig.Status)) on $FilePath\"\r\n"
            "    }\r\n"
            "    if ($MustBeSigned) {\r\n"
            "        if ($OrigDownloadedFile -and (Test-Path -LiteralPath $OrigDownloadedFile -PathType Leaf)) {\r\n"
            "            Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "        }\r\n"
            "        throw \"Required Authenticode signature is missing ($($sig.Status)) on $FilePath\"\r\n"
            "    }\r\n"
            "}\r\n\r\n"
            "# (1) Wait for ToriGraph (PID $WaitForPid and any process running $TargetExe) to exit.\r\n"
            "#     If it is still running after $WaitTimeoutSec s, abort WITHOUT touching the install.\r\n"
            "function Get-BlockingProcess {\r\n"
            "    $found = @()\r\n"
            "    if ($WaitForPid -gt 0) {\r\n"
            "        $p = Get-Process -Id $WaitForPid -ErrorAction SilentlyContinue\r\n"
            "        if ($null -ne $p) { $found += $p }\r\n"
            "    }\r\n"
            "    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($ExeName)\r\n"
            "    $targetFull = [System.IO.Path]::GetFullPath($TargetExe)\r\n"
            "    foreach ($q in @(Get-Process -Name $baseName -ErrorAction SilentlyContinue)) {\r\n"
            "        $qPath = $null\r\n"
            "        try { $qPath = $q.Path } catch { $qPath = $null }\r\n"
            "        if ($qPath -and ([System.IO.Path]::GetFullPath($qPath) -ieq $targetFull)) { $found += $q }\r\n"
            "    }\r\n"
            "    return @($found | Sort-Object -Property Id -Unique)\r\n"
            "}\r\n"
            "$deadline = (Get-Date).AddSeconds($WaitTimeoutSec)\r\n"
            "$blocking = @(Get-BlockingProcess)\r\n"
            "while ($blocking.Count -gt 0 -and (Get-Date) -lt $deadline) {\r\n"
            "    Start-Sleep -Milliseconds 250\r\n"
            "    $blocking = @(Get-BlockingProcess)\r\n"
            "}\r\n"
            "if ($blocking.Count -gt 0) {\r\n"
            "    $ids = ($blocking | ForEach-Object { $_.Id }) -join ', '\r\n"
            "    $msg = \"ToriGraph is still running (PID $ids). The update was NOT applied and $TargetInstallDir was not changed. Close ToriGraph and update again, or install $DownloadedFile manually.\"\r\n"
            "    try { Set-Content -LiteralPath $LogFile -Value $msg -Encoding UTF8 } catch { }\r\n"
            "    if (-not ([Environment]::GetCommandLineArgs() -contains '-NonInteractive')) {\r\n"
            "        try { Add-Type -AssemblyName System.Windows.Forms; [void][System.Windows.Forms.MessageBox]::Show($msg, 'ToriGraph Update') } catch { }\r\n"
            "    }\r\n"
            "    [Console]::Error.WriteLine($msg)\r\n"
            "    exit 3\r\n"
            "}\r\n\r\n"
            "# (1b) Copy package into protected staging directory & re-verify SHA-256 (eliminate TOCTOU)\r\n"
            "if (!(Test-Path -LiteralPath $TargetInstallDir)) {\r\n"
            "    [System.IO.Directory]::CreateDirectory($TargetInstallDir) | Out-Null\r\n"
            "}\r\n"
            "$SecureStageDir = [System.IO.Path]::Combine($TargetInstallDir, ('.update_staging_' + [Guid]::NewGuid().ToString('N')))\r\n"
            "[System.IO.Directory]::CreateDirectory($SecureStageDir) | Out-Null\r\n"
            "$ExtractDir = [System.IO.Path]::Combine($SecureStageDir, 'extract')\r\n"
            "$BackupDir = [System.IO.Path]::Combine($SecureStageDir, 'rollback')\r\n"
            "$StagedPkg = [System.IO.Path]::Combine($SecureStageDir, [System.IO.Path]::GetFileName($DownloadedFile))\r\n"
            "Copy-Item -LiteralPath $DownloadedFile -Destination $StagedPkg -Force\r\n"
            "$DownloadedFile = $StagedPkg\r\n"
            "if ($ExpectedSha256.Length -gt 0) {\r\n"
            "    $actualHash = ''\r\n"
            "    try { $actualHash = (Get-FileHash -LiteralPath $DownloadedFile -Algorithm SHA256).Hash } catch { $actualHash = '' }\r\n"
            "    if ((-not $actualHash) -or ($actualHash.ToLowerInvariant() -ne $ExpectedSha256.ToLowerInvariant())) {\r\n"
            "        $shaMsg = \"SHA-256 verification failed before install (expected $ExpectedSha256, got $actualHash). Update aborted.\"\r\n"
            "        if (Test-Path -LiteralPath $SecureStageDir) { Remove-Item -LiteralPath $SecureStageDir -Recurse -Force -ErrorAction SilentlyContinue }\r\n"
            "        if ($OrigDownloadedFile -and (Test-Path -LiteralPath $OrigDownloadedFile -PathType Leaf)) {\r\n"
            "            Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "        }\r\n"
            "        try { Set-Content -LiteralPath $LogFile -Value $shaMsg -Encoding UTF8 } catch { }\r\n"
            "        if (-not ([Environment]::GetCommandLineArgs() -contains '-NonInteractive')) {\r\n"
            "            try { Add-Type -AssemblyName System.Windows.Forms; [void][System.Windows.Forms.MessageBox]::Show($shaMsg, 'ToriGraph Security Error') } catch { }\r\n"
            "        }\r\n"
            "        [Console]::Error.WriteLine($shaMsg)\r\n"
            "        if ($Relaunch -and (Test-Path -LiteralPath $TargetExe)) {\r\n"
            "            try { Invoke-RelaunchTargetExe } catch { }\r\n"
            "        }\r\n"
            "        exit 4\r\n"
            "    }\r\n"
            "}\r\n\r\n"
            "# (2) Create rollback backup of ToriGraph.exe & track overwritten/new files\r\n"
            "if (Test-Path -LiteralPath $TargetExe) {\r\n"
            "    Copy-Item -LiteralPath $TargetExe -Destination $BackupExe -Force\r\n"
            "}\r\n"
            "$BackedUpRelPaths = New-Object System.Collections.Generic.List[string]\r\n"
            "$CreatedFiles = New-Object System.Collections.Generic.List[string]\r\n"
            "$CreatedDirs = New-Object System.Collections.Generic.List[string]\r\n\r\n"
            "# (3) Extract .zip via Expand-Archive or run installer, (4) restore full backup on error\r\n"
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
            "        $allNodes = @(Get-ChildItem -LiteralPath $copyRoot -Recurse -Force)\r\n"
            "        $candExe = [System.IO.Path]::Combine($copyRoot, $ExeName)\r\n"
            "        if (Test-Path -LiteralPath $candExe -PathType Leaf) {\r\n"
            "            Test-AuthenticodeFile -FilePath $candExe -MustBeSigned $RequireSignature -CheckSubject $true\r\n"
            "        } elseif ($RequireSignature) {\r\n"
            "            if ($OrigDownloadedFile -and (Test-Path -LiteralPath $OrigDownloadedFile -PathType Leaf)) {\r\n"
            "                Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "            }\r\n"
            "            throw \"Required signed executable '$ExeName' not found inside update archive.\"\r\n"
            "        }\r\n"
            "        foreach ($exeNode in $allNodes) {\r\n"
            "            if ((-not $exeNode.PSIsContainer) -and ([System.IO.Path]::GetExtension($exeNode.Name).ToLowerInvariant() -eq '.exe') -and ($exeNode.FullName -ine $candExe)) {\r\n"
            "                Test-AuthenticodeFile -FilePath $exeNode.FullName -MustBeSigned $false -CheckSubject $false\r\n"
            "            }\r\n"
            "        }\r\n"
            "        foreach ($node in $allNodes) {\r\n"
            "            $rel = $node.FullName.Substring($copyRoot.Length).TrimStart('\\', '/')\r\n"
            "            if (-not $rel) { continue }\r\n"
            "            $destPath = [System.IO.Path]::Combine($TargetInstallDir, $rel)\r\n"
            "            if ($node.PSIsContainer) {\r\n"
            "                if (!(Test-Path -LiteralPath $destPath)) {\r\n"
            "                    $CreatedDirs.Add($destPath)\r\n"
            "                }\r\n"
            "            } else {\r\n"
            "                if (Test-Path -LiteralPath $destPath -PathType Leaf) {\r\n"
            "                    $bakPath = [System.IO.Path]::Combine($BackupDir, $rel)\r\n"
            "                    $bakParent = [System.IO.Path]::GetDirectoryName($bakPath)\r\n"
            "                    if ($bakParent -and !(Test-Path -LiteralPath $bakParent)) {\r\n"
            "                        [System.IO.Directory]::CreateDirectory($bakParent) | Out-Null\r\n"
            "                    }\r\n"
            "                    Copy-Item -LiteralPath $destPath -Destination $bakPath -Force\r\n"
            "                    $BackedUpRelPaths.Add($rel)\r\n"
            "                } else {\r\n"
            "                    $CreatedFiles.Add($destPath)\r\n"
            "                }\r\n"
            "            }\r\n"
            "        }\r\n"
            "        foreach ($node in $allNodes) {\r\n"
            "            $rel = $node.FullName.Substring($copyRoot.Length).TrimStart('\\', '/')\r\n"
            "            if (-not $rel) { continue }\r\n"
            "            $destPath = [System.IO.Path]::Combine($TargetInstallDir, $rel)\r\n"
            "            if ($node.PSIsContainer) {\r\n"
            "                if (!(Test-Path -LiteralPath $destPath)) {\r\n"
            "                    [System.IO.Directory]::CreateDirectory($destPath) | Out-Null\r\n"
            "                }\r\n"
            "            } else {\r\n"
            "                $destParent = [System.IO.Path]::GetDirectoryName($destPath)\r\n"
            "                if ($destParent -and !(Test-Path -LiteralPath $destParent)) {\r\n"
            "                    [System.IO.Directory]::CreateDirectory($destParent) | Out-Null\r\n"
            "                }\r\n"
            "                Copy-Item -LiteralPath $node.FullName -Destination $destPath -Force\r\n"
            "                if (!(Test-Path -LiteralPath $destPath -PathType Leaf)) {\r\n"
            "                    throw \"Failed to copy staged item to target directory: $destPath\"\r\n"
            "                }\r\n"
            "            }\r\n"
            "        }\r\n"
            "        $sourceHasExe = Test-Path -LiteralPath ([System.IO.Path]::Combine($copyRoot, $ExeName))\r\n"
            "        if (($sourceHasExe -or (Test-Path -LiteralPath $BackupExe)) -and !(Test-Path -LiteralPath $TargetExe)) {\r\n"
            "            throw \"Updated executable not found in target directory: $TargetExe\"\r\n"
            "        }\r\n"
            "    } elseif ($ext -eq '.msi') {\r\n"
            "        Test-AuthenticodeFile -FilePath $DownloadedFile -MustBeSigned $RequireSignature -CheckSubject $true\r\n"
            "        # Windows PowerShell 5.1 joins -ArgumentList with spaces without quoting, so\r\n"
            "        # paths that contain spaces are quoted here.\r\n"
            "        $p = Start-Process -FilePath $MsiExecPath -ArgumentList @('/i', ('\"' + $DownloadedFile + '\"'), '/passive', '/norestart') -Wait -PassThru\r\n"
            "        if ($p.ExitCode -ne 0) { throw \"MSI exited with code $($p.ExitCode)\" }\r\n"
            "    } elseif ($ext -eq '.exe') {\r\n"
            "        Test-AuthenticodeFile -FilePath $DownloadedFile -MustBeSigned $RequireSignature -CheckSubject $true\r\n"
            "        $leaf = [System.IO.Path]::GetFileName($DownloadedFile).ToLowerInvariant()\r\n"
            "        if ($leaf -match 'setup|install') {\r\n"
            "            # Quote /DIR (5.1 does not); drop a trailing backslash so it cannot escape the quote.\r\n"
            "            $dirArg = '/DIR=\"' + $TargetInstallDir.TrimEnd('\\') + '\"'\r\n"
            "            $p = Start-Process -FilePath $DownloadedFile -ArgumentList @('/SILENT', '/SUPPRESSMSGBOXES', '/NORESTART', $dirArg) -Wait -PassThru\r\n"
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
            "    $errMsg = $_.Exception.Message\r\n"
            "    $rollbackOk = $true\r\n"
            "    $rollbackErrors = New-Object System.Collections.Generic.List[string]\r\n"
            "    foreach ($cf in $CreatedFiles) {\r\n"
            "        if (Test-Path -LiteralPath $cf -PathType Leaf) {\r\n"
            "            try {\r\n"
            "                Remove-Item -LiteralPath $cf -Force -ErrorAction Stop\r\n"
            "            } catch {\r\n"
            "                $rollbackOk = $false\r\n"
            "                $rollbackErrors.Add(\"remove ${cf}: $($_.Exception.Message)\")\r\n"
            "            }\r\n"
            "        }\r\n"
            "    }\r\n"
            "    foreach ($rel in $BackedUpRelPaths) {\r\n"
            "        $bakPath = [System.IO.Path]::Combine($BackupDir, $rel)\r\n"
            "        $destPath = [System.IO.Path]::Combine($TargetInstallDir, $rel)\r\n"
            "        if (Test-Path -LiteralPath $bakPath -PathType Leaf) {\r\n"
            "            try {\r\n"
            "                $destParent = [System.IO.Path]::GetDirectoryName($destPath)\r\n"
            "                if ($destParent -and !(Test-Path -LiteralPath $destParent)) {\r\n"
            "                    [System.IO.Directory]::CreateDirectory($destParent) | Out-Null\r\n"
            "                }\r\n"
            "                Copy-Item -LiteralPath $bakPath -Destination $destPath -Force -ErrorAction Stop\r\n"
            "            } catch {\r\n"
            "                $rollbackOk = $false\r\n"
            "                $rollbackErrors.Add(\"restore ${destPath}: $($_.Exception.Message)\")\r\n"
            "            }\r\n"
            "        } else {\r\n"
            "            $rollbackOk = $false\r\n"
            "            $rollbackErrors.Add(\"missing backup for ${destPath}\")\r\n"
            "        }\r\n"
            "    }\r\n"
            "    for ($idx = $CreatedDirs.Count - 1; $idx -ge 0; $idx--) {\r\n"
            "        $cd = $CreatedDirs[$idx]\r\n"
            "        if (Test-Path -LiteralPath $cd -PathType Container) {\r\n"
            "            try { Remove-Item -LiteralPath $cd -Force -ErrorAction Stop } catch { }\r\n"
            "        }\r\n"
            "    }\r\n"
            "    if (Test-Path -LiteralPath $BackupExe) {\r\n"
            "        try {\r\n"
            "            Copy-Item -LiteralPath $BackupExe -Destination $TargetExe -Force -ErrorAction Stop\r\n"
            "            Remove-Item -LiteralPath $BackupExe -Force -ErrorAction SilentlyContinue\r\n"
            "        } catch {\r\n"
            "            $rollbackOk = $false\r\n"
            "            $rollbackErrors.Add(\"restore ${TargetExe}: $($_.Exception.Message)\")\r\n"
            "        }\r\n"
            "    }\r\n"
            "    if ($rollbackOk) {\r\n"
            "        $rbReport = \"ToriGraph update failed and previous installation files were rolled back: $errMsg\"\r\n"
            "    } else {\r\n"
            "        $rbDetail = ($rollbackErrors -join '; ')\r\n"
            "        $rbReport = \"ToriGraph update failed AND rollback encountered errors ($rbDetail). Original error: $errMsg\"\r\n"
            "    }\r\n"
            "    try { Set-Content -LiteralPath $LogFile -Value $rbReport -Encoding UTF8 } catch { }\r\n"
            "    if (-not ([Environment]::GetCommandLineArgs() -contains '-NonInteractive')) {\r\n"
            "        try { Add-Type -AssemblyName System.Windows.Forms; [void][System.Windows.Forms.MessageBox]::Show($rbReport, 'ToriGraph Update Failed') } catch { }\r\n"
            "    }\r\n"
            "    [Console]::Error.WriteLine($rbReport)\r\n"
            "    if ($rollbackOk -and $Relaunch -and (Test-Path -LiteralPath $TargetExe -PathType Leaf)) {\r\n"
            "        try { Invoke-RelaunchTargetExe } catch { }\r\n"
            "    }\r\n"
            "    throw\r\n"
            "} finally {\r\n"
            "    if ($ExtractDir -and (Test-Path -LiteralPath $ExtractDir)) {\r\n"
            "        Remove-Item -LiteralPath $ExtractDir -Recurse -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "    if ($BackupDir -and (Test-Path -LiteralPath $BackupDir)) {\r\n"
            "        Remove-Item -LiteralPath $BackupDir -Recurse -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "    if ($SecureStageDir -and (Test-Path -LiteralPath $SecureStageDir)) {\r\n"
            "        Remove-Item -LiteralPath $SecureStageDir -Recurse -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "}\r\n\r\n"
            "# (4b) Success: delete the downloaded package (only when it sits in ToriGraph's own\r\n"
            "#      staging folder) so old installers do not pile up\r\n"
            "$StagingBaseFull = ''\r\n"
            "$StagingRootFull = ''\r\n"
            "try {\r\n"
            "    $StagingBaseFull = [System.IO.Path]::GetFullPath($StagingBase).TrimEnd('\\')\r\n"
            "    $StagingRootFull = [System.IO.Path]::GetFullPath($StagingRoot).TrimEnd('\\')\r\n"
            "    $pkgDir = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($OrigDownloadedFile))\r\n"
            "    if ($pkgDir -and (($pkgDir.TrimEnd('\\') -ieq $StagingBaseFull) -or ($pkgDir.TrimEnd('\\') -ieq $StagingRootFull))) {\r\n"
            "        Remove-Item -LiteralPath $OrigDownloadedFile -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "} catch { }\r\n\r\n"
            "# (5) Relaunch ToriGraph.exe if requested\r\n"
            "Invoke-RelaunchTargetExe\r\n\r\n"
            "# (6) Remove this script's GUID staging directory (never anything but a direct child of\r\n"
            "#     the staging base)\r\n"
            "try {\r\n"
            "    if ($StagingRootFull -and $StagingBaseFull -and ([System.IO.Path]::GetDirectoryName($StagingRootFull) -ieq $StagingBaseFull)) {\r\n"
            "        Set-Location -LiteralPath $env:TEMP\r\n"
            "        Remove-Item -LiteralPath $StagingRootFull -Recurse -Force -ErrorAction SilentlyContinue\r\n"
            "    }\r\n"
            "} catch { }\r\n");

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

    // (The former companion apply_torigraph_update.cmd wrapper was never executed by the
    // application and only widened the attack surface of the staging folder; it is gone.)
    return ps1Path;
}

QList<QNetworkProxy> AutoUpdater::proxiesForUrl(const QUrl &url)
{
    QList<QNetworkProxy> proxies;
    if (url.isLocalFile() || isLoopbackHost(url.host())) {
        proxies << QNetworkProxy(QNetworkProxy::NoProxy);
        return proxies;
    }
    // systemProxyForQuery() can block for seconds on WPAD/PAC networks, and the factory is
    // asked again for every request and redirect (API -> asset -> CDN) on the GUI thread.
    // Remember the answer per scheme/host/port for a few minutes so the stall happens at
    // most once per host.
    struct CachedProxy
    {
        QList<QNetworkProxy> proxies;
        qint64 stampMs = 0;
    };
    struct ProxyCache
    {
        std::mutex mutex;
        QHash<QString, CachedProxy> map;
    };
    static const std::shared_ptr<ProxyCache> s_cache = std::make_shared<ProxyCache>();
    const std::shared_ptr<ProxyCache> cache = s_cache;
    constexpr qint64 kProxyCacheTtlMs = 5 * 60 * 1000;
    const QString key = QStringLiteral("%1://%2:%3")
                                .arg(url.scheme().toLower(), url.host().toLower())
                                .arg(url.port(-1));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    {
        std::lock_guard<std::mutex> lock(cache->mutex);
        const auto it = cache->map.constFind(key);
        if (it != cache->map.constEnd() && now - it->stampMs >= 0
            && now - it->stampMs < kProxyCacheTtlMs)
            return it->proxies;
    }
    const auto lookupAndCache = [cache, key, url]() {
        QList<QNetworkProxy> found =
                QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));
        if (found.isEmpty())
            found << QNetworkProxy(QNetworkProxy::NoProxy);
        std::lock_guard<std::mutex> lock(cache->mutex);
        cache->map.insert(key, CachedProxy { found, QDateTime::currentMSecsSinceEpoch() });
        return found;
    };
    if (QCoreApplication::instance()
        && QThread::currentThread() == QCoreApplication::instance()->thread()) {
        // GUI thread: WPAD/PAC resolution can hang for many seconds. Resolve on a helper
        // thread and wait only briefly; on timeout go direct for this request while the
        // lookup finishes in the background and fills the cache for the next one.
        auto prom = std::make_shared<std::promise<QList<QNetworkProxy>>>();
        std::future<QList<QNetworkProxy>> fut = prom->get_future();
        std::thread([prom, lookupAndCache]() { prom->set_value(lookupAndCache()); }).detach();
        if (fut.wait_for(std::chrono::milliseconds(1500)) == std::future_status::ready)
            return fut.get();
        proxies << QNetworkProxy(QNetworkProxy::NoProxy);
        return proxies;
    }
    return lookupAndCache();
}

QString AutoUpdater::resolvePowerShellExecutable()
{
#ifdef Q_OS_WIN
    wchar_t sysDirBuf[MAX_PATH + 4] = {};
    const UINT n = GetSystemDirectoryW(sysDirBuf, MAX_PATH + 1);
    if (n > 0 && n <= MAX_PATH) {
        const QString sysDir = QString::fromWCharArray(sysDirBuf, static_cast<int>(n));
        const QString candidate =
                QDir(sysDir).filePath(QStringLiteral("WindowsPowerShell/v1.0/powershell.exe"));
        if (QFileInfo(candidate).isFile())
            return QDir::toNativeSeparators(candidate);
    }
    return QString();
#else
    return QString();
#endif
}

bool AutoUpdater::isDirectoryWritable(const QString &dirPath)
{
    if (dirPath.trimmed().isEmpty() || !QFileInfo(dirPath).isDir())
        return false;
    // QFileInfo::isWritable() ignores Windows ACLs (Program Files reports writable), so
    // actually try to create a file there.
    QTemporaryFile probe(QDir(dirPath).filePath(QStringLiteral(".torigraph_write_probe_XXXXXX")));
    probe.setAutoRemove(true);
    return probe.open();
}

QString AutoUpdater::describeNetworkError(int netErr, const QString &errorString)
{
    registerAutoUpdaterExtraTranslations();
    QString text = errorString;
    if (netErr == QNetworkReply::ProxyAuthenticationRequiredError) {
        text += QStringLiteral("\n")
                + tr("The proxy server asks for a user name and password, which the updater "
                     "cannot enter. Open the release page in your web browser and download "
                     "the package from there, or ask your network administrator.");
    }
    return text;
}

void AutoUpdater::applyNetworkProxyPolicy(QNetworkAccessManager &nam, const QUrl &url)
{
    Q_UNUSED(url);
    // The factory decides per request (loopback -> direct, otherwise the system proxy).
    nam.setProxyFactory(new UpdaterProxyFactory());
}

QUrl AutoUpdater::releasesPageUrl(const QUrl &preferred)
{
    if (isAllowedUpdateUrl(preferred))
        return preferred;
    return QUrl(QString::fromLatin1(kDefaultReleasesPage));
}

bool AutoUpdater::allowUnverifiedInstall()
{
    return ApplicationWindow::getSettings()
            .value(QString::fromLatin1(kSettingsKeyAllowUnverified), false)
            .toBool();
}

bool AutoUpdater::showFailureWithReleasePage(QWidget *parent, const QString &title,
                                             const QString &text, const QUrl &releasePage)
{
    registerAutoUpdaterExtraTranslations();
    if (isHeadlessOrEmbeddedProcess(qobject_cast<ApplicationWindow *>(parent))) {
        qWarning("%s: %s", qPrintable(title), qPrintable(text));
        return false;
    }
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
    QPushButton *openBtn = box.addButton(tr("Open Release Page"), QMessageBox::ActionRole);
    box.setDefaultButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == openBtn) {
        QDesktopServices::openUrl(releasesPageUrl(releasePage));
        return true;
    }
    return false;
}

bool AutoUpdater::closeAppAndLaunchUpdater(ApplicationWindow *app, const QString &scriptPath,
                                           const QUrl &releasePage)
{
    registerAutoUpdaterExtraTranslations();

    // 0) Check everything that can fail while ToriGraph is still open. Once close() has
    //    succeeded the main window (WA_DeleteOnClose) is gone and main.cpp's
    //    lastWindowClosed() -> quit() has already ended the event loop, so no Qt dialog can
    //    be shown and the window cannot be brought back.
    const QString psExe = resolvePowerShellExecutable();
    if (psExe.isEmpty() || scriptPath.trimmed().isEmpty() || !QFileInfo::exists(scriptPath)) {
        showFailureWithReleasePage(
                app, tr("Installer Launch Failed"),
                psExe.isEmpty()
                        ? tr("Could not find Windows PowerShell (powershell.exe), so the "
                             "downloaded update cannot be applied automatically. ToriGraph "
                             "stays open; download and install the package from the release "
                             "page instead.")
                        : tr("The staged update script was not found:\n%1\nToriGraph stays "
                             "open; run \"Check for Updates\" again.")
                                  .arg(QDir::toNativeSeparators(scriptPath)),
                releasePage);
        return false;
    }

    // Enforce SHA-256 & Authenticode policy before closing ToriGraph or elevating (UAC).
    // The exact bytes of the script that were inspected here are pinned (SHA-256) and, when
    // elevation is needed, the elevated PowerShell re-reads the file, compares it with the
    // pinned digest and runs those very bytes from memory. The script itself lives in a
    // user-writable folder, so it must never be executed elevated straight from disk.
    QByteArray scriptRawBytes;
    {
        QFile sf(scriptPath);
        QString scriptText;
        if (sf.open(QIODevice::ReadOnly)) {
            QByteArray raw = sf.readAll();
            scriptRawBytes = raw;
            if (raw.startsWith("\xEF\xBB\xBF"))
                raw.remove(0, 3);
            scriptText = QString::fromUtf8(raw);
            sf.close();
        }
        if (scriptRawBytes.isEmpty()) {
            showFailureWithReleasePage(
                    app, tr("Installer Launch Failed"),
                    tr("The staged update script could not be read, so the update was not "
                       "started. ToriGraph stays open; run \"Check for Updates\" again."),
                    releasePage);
            return false;
        }
        static const QRegularExpression shaRe(
                QStringLiteral("^\\$ExpectedSha256\\s*=\\s*'([^']*)'"),
                QRegularExpression::MultilineOption);
        const QRegularExpressionMatch shaMatch = shaRe.match(scriptText);
        const QString scriptSha =
                shaMatch.hasMatch() ? normalizeSha256Token(shaMatch.captured(1)) : QString();
        if (!allowUnverifiedInstall() && scriptSha.isEmpty()) {
            showFailureWithReleasePage(
                    app, tr("Security Verification Failed"),
                    tr("This release does not publish a SHA-256 checksum, so automatic "
                       "installation is blocked."),
                    releasePage);
            return false;
        }

        static const QRegularExpression pkgRe(
                QStringLiteral("^\\$DownloadedFile\\s*=\\s*'(.*)'\\s*$"),
                QRegularExpression::MultilineOption);
        const QRegularExpressionMatch pkgMatch = pkgRe.match(scriptText);
        if (pkgMatch.hasMatch()) {
            QString stagedPkg = pkgMatch.captured(1);
            stagedPkg.replace(QStringLiteral("''"), QStringLiteral("'"));
            for (ushort u : { ushort(0x2018), ushort(0x2019), ushort(0x201A), ushort(0x201B) }) {
                const QString one = QString(QChar(u));
                stagedPkg.replace(one + one, one);
            }
            if (!stagedPkg.isEmpty() && QFileInfo::exists(stagedPkg)
                && isSignablePackageFile(stagedPkg)) {
                QString sigErr;
                const CodeSignatureStatus sigStatus =
                        verifyFileCodeSignature(stagedPkg, nullptr, &sigErr);
                if (sigStatus == CodeSignatureStatus::InvalidSignature
                    || (sigStatus == CodeSignatureStatus::Unsigned && requireCodeSignature())) {
                    if (sigStatus == CodeSignatureStatus::InvalidSignature)
                        QFile::remove(stagedPkg);
                    showFailureWithReleasePage(app, tr("Security Verification Failed"),
                                               sigErr.isEmpty()
                                                       ? tr("Digital signature verification failed.")
                                                       : sigErr,
                                               releasePage);
                    return false;
                }
            }
        }
    }
    const QString installDir = QCoreApplication::applicationDirPath();
    const bool elevate = !isDirectoryWritable(installDir);
    if (elevate && !isHeadlessOrEmbeddedProcess(app)) {
        const QMessageBox::StandardButton ans = QMessageBox::question(
                app, tr("Administrator Rights Needed"),
                tr("ToriGraph is installed in a folder you cannot write to:\n%1\n\nWindows "
                   "will ask for administrator permission (User Account Control) after "
                   "ToriGraph has closed. If you cannot give that permission, choose No and "
                   "ask the person who installed ToriGraph to update it.\n\nContinue?")
                        .arg(QDir::toNativeSeparators(installDir)),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (ans != QMessageBox::Yes)
            return false;
    }

    QApplication *guiApp = qobject_cast<QApplication *>(QCoreApplication::instance());
    const bool quitOnLast = guiApp ? guiApp->quitOnLastWindowClosed() : false;
    if (guiApp)
        guiApp->setQuitOnLastWindowClosed(false);

    // 1) Close first: this runs the normal "save changes?" confirmation. Cancel keeps
    //    ToriGraph open and nothing is installed.
    if (app && !app->close()) {
        if (guiApp)
            guiApp->setQuitOnLastWindowClosed(quitOnLast);
        QMessageBox::information(
                app, tr("Update Not Installed"),
                tr("ToriGraph is still open, so the update was not installed. The downloaded "
                   "package is kept; run \"Check for Updates\" again when you are ready to "
                   "install it."));
        return false;
    }

    // 2) Only now start the script; it waits for this process to exit and aborts if it
    //    does not. When the install folder is not writable the script runs elevated.
    const QString nativeScript = QDir::toNativeSeparators(scriptPath);
    bool started = false;
#ifdef Q_OS_WIN
    if (elevate) {
        const QString pinnedHash =
                QString::fromLatin1(QCryptographicHash::hash(scriptRawBytes,
                                                             QCryptographicHash::Sha256)
                                            .toHex())
                        .toLower();
        // Bootstrap executed elevated: verify the pinned digest, then run the verified bytes
        // from memory (no second read of the user-writable file -> no TOCTOU window).
        const QString bootstrap = QStringLiteral(
                "$p = %1; $h = %2; "
                "$b = [System.IO.File]::ReadAllBytes($p); "
                "$sha = [System.Security.Cryptography.SHA256]::Create(); "
                "$a = [System.BitConverter]::ToString($sha.ComputeHash($b)).Replace('-', '').ToLowerInvariant(); "
                "if ($a -ne $h) { "
                "try { Add-Type -AssemblyName System.Windows.Forms; "
                "[void][System.Windows.Forms.MessageBox]::Show('The staged update script changed after it was verified. The update was NOT applied.', 'ToriGraph Security Error') } catch { }; "
                "[Console]::Error.WriteLine('Staged update script digest mismatch.'); exit 5 }; "
                "$t = [System.Text.Encoding]::UTF8.GetString($b).TrimStart([char]0xFEFF); "
                "& ([scriptblock]::Create($t))")
                                          .arg(psSingleQuote(nativeScript), psSingleQuote(pinnedHash));
        const QByteArray bootUtf16(reinterpret_cast<const char *>(bootstrap.utf16()),
                                   static_cast<int>(bootstrap.size()) * 2);
        const QString params =
                QStringLiteral("-NoProfile -ExecutionPolicy Bypass -EncodedCommand %1")
                        .arg(QString::fromLatin1(bootUtf16.toBase64()));
        const std::wstring file = QDir::toNativeSeparators(psExe).toStdWString();
        const std::wstring args = params.toStdWString();
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOASYNC;
        sei.lpVerb = L"runas";
        sei.lpFile = file.c_str();
        sei.lpParameters = args.c_str();
        sei.nShow = SW_HIDE;
        started = ShellExecuteExW(&sei) != FALSE;
    } else
#endif
    {
        started = QProcess::startDetached(
                psExe,
                QStringList() << QStringLiteral("-NoProfile") << QStringLiteral("-ExecutionPolicy")
                              << QStringLiteral("Bypass") << QStringLiteral("-File")
                              << nativeScript);
    }
    if (!started) {
        // Too late for a Qt dialog (see step 0). A native message box still works, so the
        // failure is never silent; ToriGraph is restarted unchanged on request.
        const QString title = tr("Installer Launch Failed");
        const QString text = elevate
                ? tr("The update was not installed because administrator permission was not "
                     "given (or Windows PowerShell could not be started).\n\nToriGraph was "
                     "not changed. Restart ToriGraph now?")
                : tr("Could not launch Windows PowerShell to apply the staged update.\n\n"
                     "ToriGraph was not changed. Restart ToriGraph now?");
        qWarning("%s: %s", qPrintable(title), qPrintable(text));
        bool restart = false;
#ifdef Q_OS_WIN
        if (!isHeadlessOrEmbeddedProcess(nullptr))
            restart = MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(text.utf16()),
                                  reinterpret_cast<LPCWSTR>(title.utf16()),
                                  MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST)
                    == IDYES;
#endif
        if (restart)
            QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
        if (guiApp)
            guiApp->setQuitOnLastWindowClosed(quitOnLast);
        QCoreApplication::quit();
        return false;
    }
    QCoreApplication::quit();
    return true;
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
    if (!isAllowedUpdateUrl(targetUrl)) {
        if (!silentIfUpToDate) {
            handleCheckResult(false, ReleaseInfo(),
                              tr("Blocked untrusted update feed URL."), silentIfUpToDate);
        }
        return;
    }

    ++d_networkCheckCount;
    const bool allowPre = includePreReleases();

    d_activeSilentMode = silentIfUpToDate;
    d_activeTimedOut = false;

    configureNetworkManagerForUrl(d_nam, targetUrl);
    const QNetworkRequest req = buildGitHubRequest(targetUrl, true);
    d_activeCheckReply = d_nam.get(req);
    d_checkTimeoutTimer->start(kLiveCheckTimeoutMs);

    auto blockedRedirect = std::make_shared<bool>(false);
    QNetworkReply *activeReply = d_activeCheckReply;
    connect(d_activeCheckReply, &QNetworkReply::redirected, this,
            [this, activeReply, blockedRedirect](const QUrl &newUrl) {
        if (!AutoUpdater::isAllowedUpdateUrl(newUrl) && d_activeCheckReply == activeReply) {
            *blockedRedirect = true;
            d_activeCheckReply->abort();
        }
    });

    connect(d_activeCheckReply, &QNetworkReply::finished, this,
            [this, allowPre, blockedRedirect]() {
        if (d_checkTimeoutTimer)
            d_checkTimeoutTimer->stop();
        QNetworkReply *reply = d_activeCheckReply;
        d_activeCheckReply = nullptr;
        if (!reply)
            return;

        const bool silent = d_activeSilentMode;
        const bool timedOut = d_activeTimedOut;
        const QUrl finalUrl = reply->url();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        const QString netErrStr = AutoUpdater::describeNetworkError(netErr, reply->errorString());
        const QByteArray payload = reply->readAll();
        reply->deleteLater();

        if (*blockedRedirect || !AutoUpdater::isAllowedUpdateUrl(finalUrl)) {
            handleCheckResult(false, ReleaseInfo(),
                              tr("Blocked untrusted update feed URL."), silent);
            return;
        }
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
            showFailureWithReleasePage(
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
    const QUrl effectiveUrl = !overrideUrl.isEmpty()
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

    if (!isAllowedUpdateUrl(effectiveUrl))
        return makeResult(false, ReleaseInfo(), tr("Blocked untrusted update feed URL."));

    const bool allowPre = includePreReleases();

    QNetworkAccessManager localNam;
    configureNetworkManagerForUrl(localNam, effectiveUrl);
    const QNetworkRequest req = buildGitHubRequest(effectiveUrl, true);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;
    bool blockedRedirect = false;

    QNetworkReply *reply = localNam.get(req);
    QObject::connect(reply, &QNetworkReply::redirected, &loop, [&](const QUrl &newUrl) {
        if (!AutoUpdater::isAllowedUpdateUrl(newUrl)) {
            blockedRedirect = true;
            if (reply->isRunning())
                reply->abort();
            loop.quit();
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

    const QUrl finalUrl = reply->url();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrStr = AutoUpdater::describeNetworkError(netErr, reply->errorString());
    const QByteArray payload = reply->readAll();
    delete reply;

    if (blockedRedirect || !isAllowedUpdateUrl(finalUrl)) {
        return makeResult(false, ReleaseInfo(), tr("Blocked untrusted update feed URL."));
    }
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

    if (!isAllowedUpdateUrl(assetUrl)) {
        if (errOut)
            *errOut = tr("Blocked untrusted download URL: %1").arg(assetUrl.toString());
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

    {
        QNetworkAccessManager localNam;
        configureNetworkManagerForUrl(localNam, assetUrl);
        const QNetworkRequest req = buildGitHubRequest(assetUrl, false);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        bool timedOut = false;
        bool writeFailed = false;
        bool blockedRedirect = false;

        QNetworkReply *reply = localNam.get(req);
        QObject::connect(reply, &QNetworkReply::redirected, &loop, [&](const QUrl &newUrl) {
            if (!AutoUpdater::isAllowedUpdateUrl(newUrl)) {
                blockedRedirect = true;
                if (reply->isRunning())
                    reply->abort();
                loop.quit();
            }
        });
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
        if (!tail.isEmpty() && !writeFailed && !blockedRedirect) {
            if (partFile.write(tail) != tail.size())
                writeFailed = true;
        }
        partFile.flush();
        partFile.close();

        const QUrl finalUrl = reply->url();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        const QString netErrStr = AutoUpdater::describeNetworkError(netErr, reply->errorString());
        delete reply;

        if (blockedRedirect || !isAllowedUpdateUrl(finalUrl)) {
            QFile::remove(partPath);
            if (errOut)
                *errOut = tr("Blocked untrusted download URL: %1").arg(finalUrl.toString());
            return false;
        }
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
    } else if (!allowUnverifiedInstall()) {
        QFile::remove(partPath);
        if (errOut) {
            *errOut = tr("SHA-256 checksum verification failed: expected SHA-256 checksum is "
                         "missing and unverified installs are disabled.");
        }
        return false;
    }

    if (isSignablePackageFile(destFilePath)) {
        QString sigErr;
        const CodeSignatureStatus sigStatus = verifyFileCodeSignature(partPath, nullptr, &sigErr);
        if (sigStatus == CodeSignatureStatus::InvalidSignature
            || (sigStatus == CodeSignatureStatus::Unsigned && requireCodeSignature())) {
            QFile::remove(partPath);
            if (errOut)
                *errOut = sigErr;
            return false;
        }
    }

    if (!atomicReplaceFile(partPath, destFilePath, errOut)) {
        QFile::remove(partPath);
        return false;
    }

    return true;
}
