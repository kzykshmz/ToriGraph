/***************************************************************************
    File                 : SettingsTransfer.cpp
    Project              : ToriGraph
    Description          : Export / import / reset of all user settings and lab defaults.
 ***************************************************************************/
#include "SettingsTransfer.h"

#include "ApplicationWindow.h"
#include "I18nJapanese.h"
#include "MultiLayer.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include <mutex>

namespace {

const char *const kLabGroup = "LabDefaults";
const char *const kLabAppliedPrefix = "LabDefaults/Applied/";
//! Key under which graph_style_presets.json travels inside an exported INI file.
const char *const kStylePresetsKey = "GraphStylePresets/JsonBase64";

bool s_saveSuppressed = false;

void registerSettingsTransferTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("E&xport Settings...", "設定を書き出す(&X)...");
        reg("&Import Settings...", "設定を読み込む(&I)...");
        reg("&Reset Settings...", "設定を初期化(&R)...");
        reg("Save all ToriGraph settings to an INI file (to back up or copy to another PC)",
            "ToriGraph のすべての設定を INI ファイルに書き出します(控えや別の PC への移行用)");
        reg("Read settings from an INI file written by Export Settings",
            "「設定を書き出す」で作った INI ファイルから設定を読み込みます");
        reg("Erase all ToriGraph settings and return to the defaults (takes effect after restart)",
            "ToriGraph の設定をすべて消して既定に戻します(再起動後に反映)");
        reg("Export Settings", "設定を書き出す");
        reg("Import Settings", "設定を読み込む");
        reg("Reset Settings", "設定を初期化");
        reg("Settings File", "設定ファイル");
        reg("Export, import or reset all settings", "設定の書き出し・読み込み・初期化");
        reg("INI files (*.ini);;All files (*)", "INI ファイル (*.ini);;すべてのファイル (*)");
        reg("%1 settings were written to\n%2", "%1 件の設定を書き出しました。\n%2");
        reg("The settings could not be written:\n%1", "設定を書き出せませんでした。\n%1");
        reg("The settings could not be read:\n%1", "設定を読み込めませんでした。\n%1");
        reg("Read the settings in\n%1\n\nSettings found in the file replace your current ones. "
            "Continue?",
            "次のファイルの設定を読み込みます。\n%1\n\nファイルにある設定は今の設定を上書きします。続けますか?");
        reg("%1 settings were read. Restart ToriGraph to use them.",
            "%1 件の設定を読み込みました。ToriGraph を再起動すると反映されます。");
        reg("%1 settings were read and applied.",
            "%1 件の設定を読み込み、反映しました。");
        reg("Erase all ToriGraph settings (recent files, preferences, window layout, "
            "style presets chosen as default...) and return to the defaults?\n\n"
            "Projects and data files are not touched. This cannot be undone; use "
            "Export Settings first if you may want them back.",
            "ToriGraph の設定(最近使ったファイル、環境設定、ウィンドウの配置、既定のスタイルプリセットなど)を"
            "すべて消して既定に戻しますか?\n\nプロジェクトやデータのファイルは消えません。元に戻せないので、"
            "必要なら先に「設定を書き出す」で控えを取ってください。");
        reg("The settings were erased. Restart ToriGraph to start with the defaults.",
            "設定を消去しました。ToriGraph を再起動すると既定の状態で始まります。");
        reg("The file contains no settings.", "ファイルに設定がありません。");
        reg("The file does not exist.", "ファイルがありません。");
        reg("The file is not a valid INI file.", "INI ファイルとして読めません。");
    });
}

QString trST(const char *s)
{
    registerSettingsTransferTranslations();
    return QObject::tr(s);
}

bool isBookkeepingKey(const QString &key)
{
    return key == QLatin1String(kLabGroup) || key.startsWith(QLatin1String(kLabGroup) + QLatin1Char('/'));
}

//! Compares a stored value with one read from an INI file (the INI side is usually a
//! string, the native side may be an int or bool written back by the application).
bool sameSettingValue(const QVariant &a, const QVariant &b)
{
    if (a.type() == QVariant::StringList || b.type() == QVariant::StringList)
        return a.toStringList() == b.toStringList();
    if (a.type() == QVariant::ByteArray || b.type() == QVariant::ByteArray)
        return a.toByteArray() == b.toByteArray();
    if (a.canConvert<QString>() && b.canConvert<QString>())
        return a.toString() == b.toString();
    return a == b;
}

//! graph_style_presets.json belongs to the application's own settings only. A QSettings object
//! created elsewhere (tests, tools converting an INI file) must neither read, overwrite nor delete
//! the real per-user presets file, so it gets an empty path here.
QString stylePresetsFileFor(const QSettings &s)
{
    if (&s != &ApplicationWindow::getSettings())
        return QString();
    return MultiLayer::userStylePresetsFilePath();
}

class SettingsActionHub : public QObject
{
public:
    explicit SettingsActionHub(ApplicationWindow *app) : QObject(app), d_app(app)
    {
        setObjectName(QStringLiteral("SettingsTransferActionHub"));
        if (qApp)
            qApp->installEventFilter(this);
    }
    QAction *actions[SettingsTransfer::ActCount] = { nullptr, nullptr, nullptr };

    void retranslate()
    {
        static const char *const kText[SettingsTransfer::ActCount] = {
            "E&xport Settings...", "&Import Settings...", "&Reset Settings..."
        };
        static const char *const kTip[SettingsTransfer::ActCount] = {
            "Save all ToriGraph settings to an INI file (to back up or copy to another PC)",
            "Read settings from an INI file written by Export Settings",
            "Erase all ToriGraph settings and return to the defaults (takes effect after restart)"
        };
        for (int i = 0; i < SettingsTransfer::ActCount; ++i) {
            if (!actions[i])
                continue;
            actions[i]->setText(trST(kText[i]));
            actions[i]->setToolTip(trST(kTip[i]));
            actions[i]->setStatusTip(trST(kTip[i]));
        }
    }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev && ev->type() == QEvent::LanguageChange && obj == qApp)
            retranslate();
        return QObject::eventFilter(obj, ev);
    }

private:
    QPointer<ApplicationWindow> d_app;
};

SettingsActionHub *actionHub(ApplicationWindow *app)
{
    if (!app)
        return nullptr;
    if (auto *hub = app->findChild<SettingsActionHub *>(
                QStringLiteral("SettingsTransferActionHub"), Qt::FindDirectChildrenOnly))
        return hub;
    return new SettingsActionHub(app);
}

} // namespace

QString SettingsTransfer::labDefaultsFileName()
{
    return QStringLiteral("torigraph-lab-defaults.ini");
}

QString SettingsTransfer::labDefaultsPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(labDefaultsFileName());
}

bool SettingsTransfer::isPersonalKey(const QString &key)
{
    QString k = key.trimmed();
    while (k.startsWith(QLatin1Char('/')))
        k.remove(0, 1);
    if (k.isEmpty())
        return false;

    static const char *const kExactKeys[] = {
        "General/RecentProjects",
        "General/Paths/WorkingDir",
        "General/Paths/ASCII",
        "General/Paths/Images",
        "General/Paths/TemplatesDir",
        "General/Paths/HelpFile",
        "General/Paths/FitPlugins",
        "PptxExport/LastPath",
        "General/DockWindows",
        "General/ExplorerSplitter",
        "AutoUpdate/RequireCodeSignature",
        "AutoUpdate/AllowUnverifiedInstall",
        "AutoUpdate/FeedUrl",
        "AutoUpdate/ExpectedSignerSubject",
    };
    for (const char *exact : kExactKeys) {
        if (k.compare(QLatin1String(exact), Qt::CaseInsensitive) == 0)
            return true;
    }

    if (k.compare(QLatin1String("General/ProjectWindow"), Qt::CaseInsensitive) == 0
        || k.startsWith(QLatin1String("General/ProjectWindow/"), Qt::CaseInsensitive)) {
        return true;
    }
    if (k.compare(QLatin1String("Geometry"), Qt::CaseInsensitive) == 0
        || k.startsWith(QLatin1String("Geometry/"), Qt::CaseInsensitive)) {
        return true;
    }
    if (k.contains(QLatin1String("/LastTableWindow/"), Qt::CaseInsensitive)
        || k.endsWith(QLatin1String("/LastTableWindow"), Qt::CaseInsensitive)
        || k.startsWith(QLatin1String("LastTableWindow/"), Qt::CaseInsensitive)) {
        return true;
    }
    if (k.contains(QLatin1String("/LastGraphWindow/"), Qt::CaseInsensitive)
        || k.endsWith(QLatin1String("/LastGraphWindow"), Qt::CaseInsensitive)
        || k.startsWith(QLatin1String("LastGraphWindow/"), Qt::CaseInsensitive)) {
        return true;
    }
    if (k.startsWith(QLatin1String("AutoUpdate/LastCheck"), Qt::CaseInsensitive))
        return true;

    return false;
}

int SettingsTransfer::exportToIni(QSettings &src, const QString &path, QString *error)
{
    if (path.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("empty path");
        return -1;
    }
    src.sync();
    const QFileInfo fi(path);
    if (!fi.dir().exists())
        QDir().mkpath(fi.dir().absolutePath());
    const QString tmpPath = fi.absoluteFilePath() + QStringLiteral(".tmp");
    QFile::remove(tmpPath);
    int n = 0;
    {
        QSettings out(tmpPath, QSettings::IniFormat);
        out.clear();
        for (const QString &key : src.allKeys()) {
            if (isBookkeepingKey(key) || isPersonalKey(key))
                continue;
            out.setValue(key, src.value(key));
            ++n;
        }
        // user graph style presets live in a separate JSON file; carry them along (C5-5)
        {
            const QString presetsFile = stylePresetsFileFor(src);
            QFile pf(presetsFile);
            if (!presetsFile.isEmpty() && pf.exists() && pf.open(QIODevice::ReadOnly)) {
                const QByteArray json = pf.readAll();
                pf.close();
                if (!json.isEmpty()) {
                    out.setValue(QLatin1String(kStylePresetsKey), QString::fromLatin1(json.toBase64()));
                    ++n;
                }
            }
        }
        out.sync();
        if (out.status() != QSettings::NoError) {
            QFile::remove(tmpPath);
            if (error)
                *error = QStringLiteral("cannot write '%1'").arg(path);
            return -1;
        }
    }
    // Replace the target only once the new file is complete.
    if (QFile::exists(fi.absoluteFilePath()) && !QFile::remove(fi.absoluteFilePath())) {
        QFile::remove(tmpPath);
        if (error)
            *error = QStringLiteral("cannot replace '%1'").arg(path);
        return -1;
    }
    if (!QFile::rename(tmpPath, fi.absoluteFilePath())) {
        QFile::remove(tmpPath);
        if (error)
            *error = QStringLiteral("cannot write '%1'").arg(path);
        return -1;
    }
    return n;
}

int SettingsTransfer::importFromIni(QSettings &dst, const QString &path, QString *error)
{
    if (!QFileInfo(path).isFile()) {
        if (error)
            *error = trST("The file does not exist.");
        return -1;
    }
    QSettings in(path, QSettings::IniFormat);
    if (in.status() != QSettings::NoError) {
        if (error)
            *error = trST("The file is not a valid INI file.");
        return -1;
    }
    const QStringList keys = in.allKeys();
    int n = 0;
    for (const QString &key : keys) {
        if (isBookkeepingKey(key) || isPersonalKey(key))
            continue;
        if (key == QLatin1String(kStylePresetsKey)) {
            const QByteArray json = QByteArray::fromBase64(in.value(key).toString().toLatin1());
            QJsonParseError parseErr;
            const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
            const QString presetPath = stylePresetsFileFor(dst);
            if (!json.isEmpty() && parseErr.error == QJsonParseError::NoError
                && (doc.isObject() || doc.isArray()) && !presetPath.isEmpty()) {
                QDir().mkpath(QFileInfo(presetPath).absolutePath());
                QSaveFile sf(presetPath);
                if (sf.open(QIODevice::WriteOnly) && sf.write(json) == json.size() && sf.commit())
                    ++n;
            }
            continue;
        }
        dst.setValue(key, in.value(key));
        ++n;
    }
    if (n == 0) {
        if (error)
            *error = trST("The file contains no settings.");
        return -1;
    }
    dst.sync();
    return n;
}

void SettingsTransfer::resetAll(QSettings &s)
{
    s.clear();
    s.sync();
    const QString presetPath = stylePresetsFileFor(s);
    if (!presetPath.isEmpty() && QFile::exists(presetPath))
        QFile::remove(presetPath);
}

int SettingsTransfer::applyLabDefaults(QSettings &user, const QString &labIniPath)
{
    if (labIniPath.isEmpty() || !QFileInfo(labIniPath).isFile())
        return 0;
    QSettings lab(labIniPath, QSettings::IniFormat);
    if (lab.status() != QSettings::NoError)
        return 0;
    int written = 0;
    for (const QString &key : lab.allKeys()) {
        if (isBookkeepingKey(key) || isPersonalKey(key))
            continue;
        const QVariant labValue = lab.value(key);
        const QString appliedKey = QLatin1String(kLabAppliedPrefix) + key;
        bool write = false;
        if (!user.contains(key)) {
            write = true;
        } else if (user.contains(appliedKey)
                   && sameSettingValue(user.value(key), user.value(appliedKey))
                   && !sameSettingValue(user.value(key), labValue)) {
            // still the value seeded from an earlier lab file: follow the revised file
            write = true;
        }
        if (write) {
            user.setValue(key, labValue);
            user.setValue(appliedKey, labValue);
            ++written;
        }
    }
    if (written > 0)
        user.sync();
    return written;
}

bool SettingsTransfer::saveSuppressed()
{
    return s_saveSuppressed;
}

void SettingsTransfer::setSaveSuppressed(bool on)
{
    s_saveSuppressed = on;
}

QAction *SettingsTransfer::action(ApplicationWindow *app, ActionId id)
{
    SettingsActionHub *hub = actionHub(app);
    if (!hub || id < 0 || id >= ActCount)
        return nullptr;
    if (!hub->actions[id]) {
        static const char *const kObj[ActCount] = { "actionSettingsExport", "actionSettingsImport",
                                                    "actionSettingsReset" };
        auto *a = new QAction(app);
        a->setObjectName(QString::fromLatin1(kObj[id]));
        QPointer<ApplicationWindow> appPtr(app);
        QObject::connect(a, &QAction::triggered, app, [appPtr, id]() {
            if (!appPtr)
                return;
            if (id == ActExport)
                SettingsTransfer::exportInteractive(appPtr);
            else if (id == ActImport)
                SettingsTransfer::importInteractive(appPtr);
            else
                SettingsTransfer::resetInteractive(appPtr);
        });
        hub->actions[id] = a;
        hub->retranslate();
    }
    return hub->actions[id];
}

void SettingsTransfer::exportInteractive(ApplicationWindow *app)
{
    if (!app)
        return;
    app->saveSettings(); // write the current in-memory state first (no-op after import/reset)
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    const QString suggested = QDir(dir).filePath(
            QStringLiteral("ToriGraph-settings-%1.ini")
                    .arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))));
    const QString path = QFileDialog::getSaveFileName(app, trST("Export Settings"), suggested,
                                                      trST("INI files (*.ini);;All files (*)"));
    if (path.isEmpty())
        return;
    QString err;
    const int n = exportToIni(ApplicationWindow::getSettings(), path, &err);
    if (n < 0) {
        QMessageBox::warning(app, trST("Export Settings"),
                             trST("The settings could not be written:\n%1").arg(err));
        return;
    }
    QMessageBox::information(app, trST("Export Settings"),
                             trST("%1 settings were written to\n%2")
                                     .arg(n)
                                     .arg(QDir::toNativeSeparators(path)));
}

void SettingsTransfer::importInteractive(ApplicationWindow *app)
{
    if (!app)
        return;
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    const QString path = QFileDialog::getOpenFileName(app, trST("Import Settings"), dir,
                                                      trST("INI files (*.ini);;All files (*)"));
    if (path.isEmpty())
        return;
    if (QMessageBox::question(app, trST("Import Settings"),
                              trST("Read the settings in\n%1\n\nSettings found in the file "
                                   "replace your current ones. Continue?")
                                      .arg(QDir::toNativeSeparators(path)),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;
    QString err;
    const int n = importFromIni(ApplicationWindow::getSettings(), path, &err);
    if (n < 0) {
        QMessageBox::warning(app, trST("Import Settings"),
                             trST("The settings could not be read:\n%1").arg(err));
        return;
    }
    setSaveSuppressed(false);
    QPointer<ApplicationWindow> callerGuard(app);
    bool appliedToCaller = false;
    for (QWidget *tlw : QApplication::topLevelWidgets()) {
        if (auto *aw = qobject_cast<ApplicationWindow *>(tlw)) {
            const bool isCaller = (aw == callerGuard.data());
            aw->readSettings(/*restoreWindowLayout=*/false);
            aw->applyUserSettings(/*resetScriptingLang=*/isCaller);
            if (isCaller)
                appliedToCaller = true;
        }
    }
    if (!appliedToCaller && callerGuard) {
        callerGuard->readSettings(/*restoreWindowLayout=*/false);
        callerGuard->applyUserSettings(/*resetScriptingLang=*/true);
    }
    if (callerGuard)
        callerGuard->saveSettings();
    QMessageBox::information(callerGuard.data(), trST("Import Settings"),
                             trST("%1 settings were read and applied.").arg(n));
}

void SettingsTransfer::resetInteractive(ApplicationWindow *app)
{
    if (!app)
        return;
    if (QMessageBox::warning(
                app, trST("Reset Settings"),
                trST("Erase all ToriGraph settings (recent files, preferences, window layout, "
                     "style presets chosen as default...) and return to the defaults?\n\n"
                     "Projects and data files are not touched. This cannot be undone; use "
                     "Export Settings first if you may want them back."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;
    resetAll(ApplicationWindow::getSettings());
    setSaveSuppressed(true);
    QMessageBox::information(
            app, trST("Reset Settings"),
            trST("The settings were erased. Restart ToriGraph to start with the defaults."));
}
