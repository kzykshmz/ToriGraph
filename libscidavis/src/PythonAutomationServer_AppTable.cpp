/***************************************************************************
    File                 : PythonAutomationServer_AppTable.cpp
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
                           [App, Table & Matrix RPC Handlers]
 ***************************************************************************/
#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "PythonAutomationServer.h"
#include "PythonAutomationServer_Internal.h"

#ifdef _MSC_VER
#pragma warning(disable : 4267 4373)
#endif

#include "AdvancedSignalProc.h"
#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "AutoUpdater.h"
#include "BoxCurve.h"
#include "CurveMathTools.h"
#include "ExtraPlotTypes.h"
#include "Differentiation.h"
#include "FFT.h"
#include "FFTFilter.h"
#include "Folder.h"
#include "FunctionCurve.h"
#include "Graph.h"
#include "Graph3D.h"
#include "GraphDigitizer.h"
#include "GraphInspectorDock.h"
#include "GraphStatsAndSurfaceFit.h"
#include "Grid.h"
#include "ImportOPJU.h"
#include "XlsxImport.h"
#include "Workbook.h"
#include "Integration.h"
#include "Interpolation.h"
#include "Legend.h"
#include "Matrix.h"
#include "ModernRibbonAndHud.h"
#include "MultiLayer.h"
#include "MultiPeakFit.h"
#include "MyParser.h"
#include "NonLinearFit.h"
#include "Note.h"
#include "OriginGadgets.h"
#include "PiecewiseAndRobustFit.h"
#include "Plot.h"
#include "PlotCurve.h"
#include "PolynomialFit.h"
#include "QwtBarCurve.h"
#include "QwtErrorPlotCurve.h"
#include "QwtHistogram.h"
#include "QwtPieCurve.h"
#include "RecalcEngine.h"
#include "ScaleDraw.h"
#include "SmoothFilter.h"
#include "Spectrogram.h"
#include "Table.h"
#include "VectorCurve.h"
#include "globals.h"

#include "core/column/Column.h"
#include "core/datatypes/Double2DateTimeFilter.h"
#include "core/datatypes/Double2DayOfWeekFilter.h"
#include "core/datatypes/Double2MonthFilter.h"
#include "core/datatypes/Double2StringFilter.h"
#include "core/datatypes/String2DateTimeFilter.h"
#include "lib/Interval.h"
#include "matrix/future_Matrix.h"
#include "table/AsciiTableImportFilter.h"
#include "table/future_Table.h"
#include "core/Project.h"

#ifdef Q_OS_WIN
#include "EmfPaintDevice.h"
#include "OleServer.h"
#include <ole2.h>
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif
#endif

#ifdef ORIGIN_IMPORT
#include "importOPJ.h"
#endif

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

#include <gsl/gsl_cdf.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_statistics.h>

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QMetaMethod>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableView>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTextCharFormat>
#include <QTextCodec>
#include <QTextCursor>
#include <QTextDecoder>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <vector>

using namespace rpc_internal;

namespace {

Matrix *createUniqueMatrixHeadless(ApplicationWindow *app, int rows, int cols, const QString &reqName)
{
    rows = std::max(1, rows);
    cols = std::max(1, cols);
    const QString trimmedReq = reqName.trimmed();
    QString finalName;
    if (trimmedReq.isEmpty()) {
        finalName = app->generateUniqueName(QStringLiteral("Matrix"), true);
    } else {
        const QString base = sanitizeWindowNameForSciDAVis(trimmedReq, QStringLiteral("Matrix"), true);
        finalName = makeUniqueValidWindowName(app, base, nullptr);
    }
    Matrix *m = app->newMatrix(finalName, rows, cols);
    if (!m)
        throwRpcRuntime(QStringLiteral("Failed to create Matrix '%1'.").arg(finalName));
    if (!trimmedReq.isEmpty() && trimmedReq != finalName)
        m->setWindowLabel(trimmedReq);
    return m;
}

void resetProjectInPlaceHeadless(ApplicationWindow *app)
{
    if (!app)
        return;
    const bool saveTbl = app->confirmCloseTable;
    const bool saveMtx = app->confirmCloseMatrix;
    const bool saveP2D = app->confirmClosePlot2D;
    const bool saveP3D = app->confirmClosePlot3D;
    const bool saveFld = app->confirmCloseFolder;
    const bool saveNts = app->confirmCloseNotes;

    app->confirmCloseTable = false;
    app->confirmCloseMatrix = false;
    app->confirmClosePlot2D = false;
    app->confirmClosePlot3D = false;
    app->confirmCloseFolder = false;
    app->confirmCloseNotes = false;

    auto collectAllProjectWindows = [app]() -> QList<QPointer<MyWidget>> {
        QList<QPointer<MyWidget>> all;
        for (MyWidget *w : app->windowsList()) {
            if (w)
                all.append(QPointer<MyWidget>(w));
        }
        std::function<void(Folder *)> walkFolder = [&](Folder *f) {
            if (!f)
                return;
            const QList<MyWidget *> fw = f->windowsList();
            for (MyWidget *w : fw) {
                QPointer<MyWidget> wp(w);
                if (wp && !all.contains(wp))
                    all.append(wp);
            }
            const QList<Folder *> subs = f->folders();
            for (Folder *sub : subs)
                walkFolder(sub);
        };
        walkFolder(app->projectFolder());
        return all;
    };

    // Close dependent graph windows first, then tables/matrices/notes
    QList<QPointer<MyWidget>> pass1 = collectAllProjectWindows();
    for (const QPointer<MyWidget> &w : pass1) {
        if (w && (w->inherits("MultiLayer") || w->inherits("Graph3D"))) {
            w->askOnCloseEvent(false);
            w->hide();
            w->close();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QList<QPointer<MyWidget>> pass2 = collectAllProjectWindows();
    for (const QPointer<MyWidget> &w : pass2) {
        if (w) {
            w->askOnCloseEvent(false);
            w->hide();
            w->close();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // Ensure no residual windows remain in lists or subfolders
    QList<QPointer<MyWidget>> rem = collectAllProjectWindows();
    for (const QPointer<MyWidget> &w : rem) {
        if (w) {
            w->askOnCloseEvent(false);
            w->hide();
            if (w->folder())
                app->closeWindow(w);
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // Delete any subfolders under root projectFolder() so new/open project starts with a clean tree
    if (Folder *rootF = app->projectFolder()) {
        app->folders.blockSignals(true);
        app->lv.blockSignals(true);
        app->current_folder = rootF;
        const QList<Folder *> subs = rootF->folders();
        for (Folder *sub : subs) {
            if (sub)
                app->deleteFolder(sub);
        }
        if (rootF->folderListItem())
            app->folders.setCurrentItem(rootF->folderListItem());
        app->lv.blockSignals(false);
        app->folders.blockSignals(false);
        app->changeFolder(rootF, true);
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    app->projectname = QStringLiteral("untitled");
    app->logInfo.clear();
    app->savedProject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    app->confirmCloseTable = saveTbl;
    app->confirmCloseMatrix = saveMtx;
    app->confirmClosePlot2D = saveP2D;
    app->confirmClosePlot3D = saveP3D;
    app->confirmCloseFolder = saveFld;
    app->confirmCloseNotes = saveNts;
}

QString extractCurrentVersionClean()
{
    QString raw = SciDAVis::versionString().trimmed();
    QRegularExpression rx(QStringLiteral("([0-9]+\\.[0-9]+(?:\\.[0-9]+)?(?:-[A-Za-z0-9._-]+)?)"));
    QRegularExpressionMatch m = rx.match(raw);
    if (m.hasMatch())
        return m.captured(1);
    return raw.section(QLatin1Char(' '), -1);
}

QJsonObject checkUpdatesBlockingFallback(const QJsonObject &params)
{
    QString feedUrl = optionalJsonString(params, QStringLiteral("feed_url")).trimmed();
    if (feedUrl.isEmpty())
        feedUrl = optionalJsonString(params, QStringLiteral("url")).trimmed();
    if (feedUrl.isEmpty())
        feedUrl = qEnvironmentVariable("TORIGRAPH_UPDATE_FEED_URL").trimmed();
    if (feedUrl.isEmpty()) {
        feedUrl = QStringLiteral("https://api.github.com/repos/kzykshmz/ToriGraph/releases/latest");
    }

    const bool includePrerelease =
            optionalJsonBool(params, QStringLiteral("include_prerelease"), false);
    const QString currentVer = (params.contains(QStringLiteral("current_version"))
                                && !params.value(QStringLiteral("current_version")).isNull())
            ? optionalJsonString(params, QStringLiteral("current_version")).trimmed()
            : extractCurrentVersionClean();

    QByteArray rawPayload;
    QUrl qurl(feedUrl);
    if (!AutoUpdater::isAllowedUpdateUrl(qurl)) {
        throwRpcRuntime(QStringLiteral("Blocked untrusted update feed URL: %1").arg(feedUrl));
    }
    {
        QNetworkAccessManager nam;
        // Same proxy policy as the GUI updater (system/PAC proxy for remote hosts, none for loopback).
        AutoUpdater::applyNetworkProxyPolicy(nam, qurl);
        QNetworkRequest req(qurl);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("ToriGraph-Updater/%1").arg(currentVer));
        req.setRawHeader("Accept", "application/vnd.github+json");
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
        QNetworkReply *reply = nam.get(req);
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        bool blockedRedirect = false;
        const int timeoutMs =
                parseBoundedInt(params, QStringLiteral("timeout_ms"), 10000, 500, 60000);
        QObject::connect(reply, &QNetworkReply::redirected, &loop, [&](const QUrl &newUrl) {
            if (!AutoUpdater::isAllowedUpdateUrl(newUrl)) {
                blockedRedirect = true;
                reply->abort();
                loop.quit();
            }
        });
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        {
            RpcInteractiveWindow interactiveWindow;
            loop.exec();
        }

        if (!reply->isFinished()) {
            reply->abort();
            reply->deleteLater();
            throwRpcRuntime(QStringLiteral("Update check timed out fetching %1").arg(feedUrl));
        }
        if (blockedRedirect || !AutoUpdater::isAllowedUpdateUrl(reply->url())) {
            const QString badUrl = reply->url().toString();
            reply->deleteLater();
            throwRpcRuntime(QStringLiteral("Blocked untrusted update feed redirect: %1").arg(badUrl));
        }
        rawPayload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString errDetail = reply->errorString();
            const QJsonDocument errDoc = QJsonDocument::fromJson(rawPayload);
            if (errDoc.isObject()) {
                const QString apiMsg =
                        errDoc.object().value(QStringLiteral("message")).toString().trimmed();
                if (!apiMsg.isEmpty()) {
                    errDetail = (httpStatus > 0)
                            ? QStringLiteral("HTTP %1: %2").arg(httpStatus).arg(apiMsg)
                            : apiMsg;
                }
            }
            reply->deleteLater();
            throwRpcRuntime(QStringLiteral("Update check network error: %1").arg(errDetail));
        }
        reply->deleteLater();
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(rawPayload, &perr);
    if (perr.error != QJsonParseError::NoError) {
        throwRpcRuntime(
                QStringLiteral("Invalid GitHub Release JSON: %1").arg(perr.errorString()));
    }

    QJsonObject relObj;
    if (doc.isArray()) {
        const QJsonArray rels = doc.array();
        for (const QJsonValue &it : rels) {
            if (!it.isObject())
                continue;
            QJsonObject cand = it.toObject();
            if (cand.value(QStringLiteral("draft")).toBool(false))
                continue;
            if (!includePrerelease && cand.value(QStringLiteral("prerelease")).toBool(false))
                continue;
            relObj = cand;
            break;
        }
    } else if (doc.isObject()) {
        relObj = doc.object();
    }

    const QString tagName = relObj.value(QStringLiteral("tag_name")).toString().trimmed();
    QString latestVer = tagName;
    while (!latestVer.isEmpty()
           && (latestVer.front() == QLatin1Char('v') || latestVer.front() == QLatin1Char('V')))
        latestVer.remove(0, 1);

    const bool isPrerelease = relObj.value(QStringLiteral("prerelease")).toBool(false);
    const bool isDraft = relObj.value(QStringLiteral("draft")).toBool(false);
    bool updateAvailable = false;
    if (!latestVer.isEmpty() && !isDraft && (includePrerelease || !isPrerelease)) {
        updateAvailable = (AutoUpdater::compareSemVer(latestVer, currentVer) > 0);
    }

    QString assetName, assetUrl, sha256Hex;
    qint64 assetSize = 0;
    const QJsonArray assets = relObj.value(QStringLiteral("assets")).toArray();
    QJsonObject chosenAsset;
    for (const QJsonValue &av : assets) {
        if (!av.isObject())
            continue;
        const QJsonObject ao = av.toObject();
        const QString nm = ao.value(QStringLiteral("name")).toString();
        if (nm.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)
            || nm.endsWith(QStringLiteral(".msi"), Qt::CaseInsensitive)
            || nm.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
            chosenAsset = ao;
            if (nm.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
                break;
        }
    }
    if (chosenAsset.isEmpty() && !assets.isEmpty() && assets.first().isObject())
        chosenAsset = assets.first().toObject();

    if (!chosenAsset.isEmpty()) {
        assetName = chosenAsset.value(QStringLiteral("name")).toString();
        for (const char *urlKey : { "browser_download_url", "download_url", "downloadUrl",
                                    "installer_url", "installerUrl", "url" }) {
            const QString candUrl =
                    chosenAsset.value(QLatin1String(urlKey)).toString().trimmed();
            if (!candUrl.isEmpty()) {
                assetUrl = candUrl;
                break;
            }
        }
        assetSize = static_cast<qint64>(chosenAsset.value(QStringLiteral("size")).toDouble(0));
        const QString digest = chosenAsset.value(QStringLiteral("digest")).toString().trimmed();
        if (digest.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive)) {
            static const QRegularExpression kSha256HexRe(QStringLiteral("^[0-9a-fA-F]{64}$"));
            const QString candHex = digest.mid(7).trimmed().toLower();
            if (kSha256HexRe.match(candHex).hasMatch())
                sha256Hex = candHex;
        }
    }
    if (assetUrl.isEmpty()) {
        for (const char *relUrlKey : { "asset_url", "downloadUrl", "installerUrl" }) {
            const QString candUrl =
                    relObj.value(QLatin1String(relUrlKey)).toString().trimmed();
            if (!candUrl.isEmpty()) {
                assetUrl = candUrl;
                break;
            }
        }
    }
    if (!assetUrl.isEmpty() && !AutoUpdater::isAllowedUpdateUrl(QUrl(assetUrl, QUrl::StrictMode))) {
        throwRpcRuntime(QStringLiteral("Blocked untrusted download URL: %1").arg(assetUrl));
    }

    const QString rawHtmlUrl = relObj.value(QStringLiteral("html_url")).toString().trimmed();
    if (!rawHtmlUrl.isEmpty()
        && !AutoUpdater::isAllowedUpdateUrl(QUrl(rawHtmlUrl, QUrl::StrictMode))) {
        throwRpcRuntime(
                QStringLiteral("Blocked untrusted release page URL in feed: %1").arg(rawHtmlUrl));
    }

    const QString body = relObj.value(QStringLiteral("body")).toString();
    // NOTE: a bare 64-hex string in the release body is deliberately NOT accepted as a digest
    // (AutoUpdater dropped that heuristic as well); only the asset "digest" field is trusted.

    QJsonObject out;
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("feed_url")] = feedUrl;
    out[QStringLiteral("current_version")] = currentVer;
    out[QStringLiteral("latest_version")] = latestVer;
    out[QStringLiteral("tag_name")] = tagName;
    out[QStringLiteral("update_available")] = updateAvailable;
    out[QStringLiteral("is_prerelease")] = isPrerelease;
    out[QStringLiteral("title")] = relObj.value(QStringLiteral("name")).toString();
    out[QStringLiteral("body")] = body;
    out[QStringLiteral("html_url")] = rawHtmlUrl;
    out[QStringLiteral("asset_name")] = assetName;
    out[QStringLiteral("asset_url")] = assetUrl;
    out[QStringLiteral("asset_size")] = assetSize;
    out[QStringLiteral("sha256")] = sha256Hex;
    return out;
}

SciDAVis::PlotDesignation parsePlotDesignation(const QString &str,
                                               SciDAVis::PlotDesignation fallback = SciDAVis::Y)
{
    const QString s = str.trimmed().toLower();
    if (s == QStringLiteral("x"))
        return SciDAVis::X;
    if (s == QStringLiteral("y"))
        return SciDAVis::Y;
    if (s == QStringLiteral("z"))
        return SciDAVis::Z;
    if (s == QStringLiteral("xerr") || s == QStringLiteral("x_err") || s == QStringLiteral("dx"))
        return SciDAVis::xErr;
    if (s == QStringLiteral("yerr") || s == QStringLiteral("y_err") || s == QStringLiteral("dy"))
        return SciDAVis::yErr;
    if (s == QStringLiteral("label") || s == QStringLiteral("none")
        || s == QStringLiteral("disregard") || s == QStringLiteral("nodesignation"))
        return SciDAVis::noDesignation;
    return fallback;
}

QString plotDesignationToString(SciDAVis::PlotDesignation pd)
{
    switch (pd) {
    case SciDAVis::X:
        return QStringLiteral("X");
    case SciDAVis::Y:
        return QStringLiteral("Y");
    case SciDAVis::Z:
        return QStringLiteral("Z");
    case SciDAVis::xErr:
        return QStringLiteral("xErr");
    case SciDAVis::yErr:
        return QStringLiteral("yErr");
    default:
        return QStringLiteral("None");
    }
}

QString columnModeToString(SciDAVis::ColumnMode mode)
{
    switch (mode) {
    case SciDAVis::ColumnMode::Numeric:
        return QStringLiteral("numeric");
    case SciDAVis::ColumnMode::Text:
        return QStringLiteral("text");
    case SciDAVis::ColumnMode::DateTime:
        return QStringLiteral("datetime");
    case SciDAVis::ColumnMode::Month:
        return QStringLiteral("month");
    case SciDAVis::ColumnMode::Day:
        return QStringLiteral("day");
    }
    return QStringLiteral("numeric");
}

QJsonObject serializeColumnMeta(Table *t, int colIdx)
{
    Column *col = t->column(colIdx);
    QJsonObject meta;
    meta[QStringLiteral("index")] = colIdx;
    meta[QStringLiteral("name")] = t->colLabel(colIdx);
    meta[QStringLiteral("full_name")] = t->colName(colIdx);
    meta[QStringLiteral("long_name")] = t->colLongName(colIdx);
    meta[QStringLiteral("units")] = t->colUnits(colIdx);
    meta[QStringLiteral("comment")] = t->colComment(colIdx);
    meta[QStringLiteral("plot_designation")] = plotDesignationToString(t->colPlotDesignation(colIdx));
    meta[QStringLiteral("mode")] =
            col ? columnModeToString(col->columnMode()) : QStringLiteral("numeric");
    meta[QStringLiteral("formula")] = col ? col->formula(0) : QString();
    meta[QStringLiteral("formula_error")] = col ? col->formulaError() : QString();
    meta[QStringLiteral("formatted_axis_title")] =
            col ? col->formattedAxisTitle() : t->colLabel(colIdx);
    if (col) {
        if (auto *f = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
            meta[QStringLiteral("numeric_format")] = QString(QChar::fromLatin1(f->numericFormat()));
            meta[QStringLiteral("precision")] = f->numDigits();
        }
    }
    return meta;
}

//! Opens one undo macro on an aspect for the lifetime of the object, so that everything a single
//! RPC call changes is undone by one Undo step (a no-op without an undo stack).
struct ScopedRpcUndoMacro
{
    AbstractAspect *aspect = nullptr;
    ScopedRpcUndoMacro(AbstractAspect *a, const QString &label) : aspect(a)
    {
        if (aspect)
            aspect->beginMacro(label);
    }
    ~ScopedRpcUndoMacro()
    {
        if (aspect)
            aspect->endMacro();
    }
    ScopedRpcUndoMacro(const ScopedRpcUndoMacro &) = delete;
    ScopedRpcUndoMacro &operator=(const ScopedRpcUndoMacro &) = delete;
};

//! Evaluates every formula column of `tables` (app.recalculate: all worksheets) with the
//! worksheets' own dependency-ordered recalculation -- the one automatic recalculation runs
//! (Table::setAutoRecalcFormulas(true) -> recalculateFormulaColumnsOrdered /
//! evaluateCrossTableDag) -- so a circular formula gets a formula error and keeps its values
//! instead of feeding on itself. For the duration only the worksheets in `tables` count as
//! auto-recalculating (set quietly through the future::Table flag), so a worksheet in manual mode
//! that is not in `tables` is not evaluated. The previous modes are restored afterwards.
//! A single worksheet (table.recalculate, set_formula) is evaluated with
//! Table::recalculateFormulaColumnsChecked instead, which touches no other worksheet.
void recalcFormulaColumnsWithCycleCheck(const QVector<Table *> &tables)
{
    QVector<QPointer<Table>> safeTables;
    safeTables.reserve(tables.size());
    for (Table *t : tables) {
        if (t)
            safeTables.append(QPointer<Table>(t));
    }

    QVector<QPointer<Table>> quietlyEnabled;
    for (const QPointer<Table> &tp : safeTables) {
        if (Table *t = tp.data()) {
            if (!t->autoRecalcFormulas() && t->d_future_table) {
                t->d_future_table->setAutoRecalcFormulas(true);
                quietlyEnabled.append(tp);
            }
        }
    }
    for (const QPointer<Table> &tp : safeTables) {
        if (Table *t = tp.data())
            t->setAutoRecalcFormulas(true);
    }
    for (const QPointer<Table> &tp : quietlyEnabled) {
        if (Table *t = tp.data())
            t->setAutoRecalcFormulas(false);
    }
}

//! [{table, col, error}] for every formula column of `tables` that carries a formula error.
QJsonArray formulaErrorsJson(const QVector<Table *> &tables)
{
    QJsonArray out;
    for (Table *t : tables) {
        if (!t)
            continue;
        for (int c : t->formulaErrorColumns()) {
            QJsonObject e;
            e[QStringLiteral("table")] = t->name();
            e[QStringLiteral("col")] = t->colLabel(c);
            e[QStringLiteral("error")] = t->column(c) ? t->column(c)->formulaError() : QString();
            out.append(e);
        }
    }
    return out;
}

//! "datetime"/"date"/"time", "month" and "day" (the spellings table.set_column_meta accepts).
bool calendarColumnModeFromHint(const QString &modeHint, SciDAVis::ColumnMode *out)
{
    const QString m = modeHint.trimmed().toLower();
    if (m == QStringLiteral("datetime") || m == QStringLiteral("date")
        || m == QStringLiteral("time")) {
        *out = SciDAVis::ColumnMode::DateTime;
        return true;
    }
    if (m == QStringLiteral("month")) {
        *out = SciDAVis::ColumnMode::Month;
        return true;
    }
    if (m == QStringLiteral("day")) {
        *out = SciDAVis::ColumnMode::Day;
        return true;
    }
    return false;
}

bool isCalendarColumnMode(SciDAVis::ColumnMode m)
{
    return m == SciDAVis::ColumnMode::DateTime || m == SciDAVis::ColumnMode::Month
            || m == SciDAVis::ColumnMode::Day;
}

//! A number as a date/time, month or day-of-week cell of `col`, converted the way the GUI
//! column-type conversion from numbers does (the numeric path of writeBulkColumnArray): a day
//! count through the column's numeric date/time base (Double2DateTimeFilter), a month number
//! (1 = January) or a day of the week (1 = Monday). Invalid for other columns.
QDateTime calendarDateTimeFromNumber(Column *col, double value)
{
    if (!col || !std::isfinite(value))
        return QDateTime();
    Column number(QStringLiteral("number"), QVector<qreal>{ value });
    std::unique_ptr<AbstractSimpleFilter> conv;
    switch (col->columnMode()) {
    case SciDAVis::ColumnMode::DateTime:
        conv.reset(new Double2DateTimeFilter(*col->numericDateTimeBaseFilter()));
        break;
    case SciDAVis::ColumnMode::Month:
        conv.reset(new Double2MonthFilter());
        break;
    case SciDAVis::ColumnMode::Day:
        conv.reset(new Double2DayOfWeekFilter());
        break;
    default:
        return QDateTime();
    }
    if (!conv->input(0, &number))
        return QDateTime();
    return conv->dateTimeAt(0);
}

//! A cell as the worksheet shows it: numbers as doubles, every other column type (text, date/time,
//! month, day of week) as its displayed text, like export_csv.
QJsonValue columnCellToJson(Column *col, int row)
{
    if (!col || col->isInvalid(row))
        return QJsonValue::Null;
    if (col->columnMode() == SciDAVis::ColumnMode::Numeric)
        return safeJsonDouble(col->valueAt(row));
    if (col->columnMode() == SciDAVis::ColumnMode::Text)
        return col->textAt(row);
    return col->asStringColumn()->textAt(row);
}

void writeBulkColumnArray(Table *t, int colIdx, const QJsonArray &arr, const QString &modeHint)
{
    Column *col = t->column(colIdx);
    if (!col)
        throwRpcRuntime(QStringLiteral("Null column at index %1.").arg(colIdx));

    const int n = arr.size();
    if (n > t->numRows())
        t->setNumRows(n);

    // Date/time, month and day columns: strings are written as text and then converted with the
    // column-type conversion of the GUI (the same parsing as Column > Type), numbers are written as
    // numbers and converted the same way (e.g. a day count to a date) -- the target type is kept.
    SciDAVis::ColumnMode calendarMode = SciDAVis::ColumnMode::Numeric;
    if (calendarColumnModeFromHint(modeHint, &calendarMode)) {
        bool anyString = false;
        for (const QJsonValue &v : arr) {
            if (v.isString() && !v.toString().trimmed().isEmpty()) {
                anyString = true;
                break;
            }
        }
        writeBulkColumnArray(t, colIdx, arr,
                             anyString ? QStringLiteral("text") : QStringLiteral("numeric"));
        col = t->column(colIdx);
        if (col && col->columnMode() != calendarMode)
            col->setColumnMode(calendarMode);
        return;
    }

    const QString trimmedMode = modeHint.trimmed();
    bool useText = (trimmedMode.compare(QStringLiteral("text"), Qt::CaseInsensitive) == 0
                    || trimmedMode.compare(QStringLiteral("string"), Qt::CaseInsensitive) == 0);
    if (!useText && trimmedMode.isEmpty()) {
        for (const QJsonValue &v : arr) {
            if (v.isString()) {
                const QString s = v.toString().trimmed();
                if (!s.isEmpty() && s.compare(QStringLiteral("nan"), Qt::CaseInsensitive) != 0
                    && s.compare(QStringLiteral("null"), Qt::CaseInsensitive) != 0) {
                    bool okNum = false;
                    s.toDouble(&okNum);
                    if (!okNum) {
                        useText = true;
                        break;
                    }
                }
            }
        }
    }

    if (useText) {
        if (col->columnMode() != SciDAVis::ColumnMode::Text)
            col->setColumnMode(SciDAVis::ColumnMode::Text);
        QStringList texts;
        texts.reserve(n);
        for (int i = 0; i < n; ++i) {
            const QJsonValue &v = arr[i];
            if (v.isNull() || v.isUndefined())
                texts.append(QString());
            else if (v.isString())
                texts.append(v.toString());
            else if (v.isDouble()) // shortest text that reads back as the same double
                texts.append(QString::number(v.toDouble(), 'g', QLocale::FloatingPointShortest));
            else if (v.isBool())
                texts.append(v.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
            else
                texts.append(QString());
        }
        if (n > 0)
            col->replaceTexts(0, texts);
        if (n < t->numRows())
            col->setInvalid(Interval<int>(n, t->numRows() - 1), true);
    } else {
        if (col->columnMode() != SciDAVis::ColumnMode::Numeric)
            col->setColumnMode(SciDAVis::ColumnMode::Numeric);
        QVector<qreal> vals(n, 0.0);
        QVector<int> invalidRows;
        for (int i = 0; i < n; ++i) {
            const double d = jsonToDoubleOrNaN(arr[i]);
            if (std::isfinite(d)) {
                vals[i] = d;
            } else {
                vals[i] = std::numeric_limits<double>::quiet_NaN();
                invalidRows.append(i);
            }
        }
        col->clearValidity();
        if (n > 0)
            col->replaceValues(0, vals);
        // Compress contiguous invalid intervals for speed
        int k = 0;
        while (k < invalidRows.size()) {
            int startR = invalidRows[k];
            int endR = startR;
            while (k + 1 < invalidRows.size() && invalidRows[k + 1] == endR + 1) {
                ++k;
                endR = invalidRows[k];
            }
            col->setInvalid(Interval<int>(startR, endR), true);
            ++k;
        }
        if (n < t->numRows())
            col->setInvalid(Interval<int>(n, t->numRows() - 1), true);
    }
}

void validateColumnMetadataObject(const QJsonObject &meta)
{
    if (meta.contains(QStringLiteral("name")) && !meta.value(QStringLiteral("name")).isNull())
        (void)optionalJsonString(meta, QStringLiteral("name"));
    if (meta.contains(QStringLiteral("long_name"))
        && !meta.value(QStringLiteral("long_name")).isNull()) {
        (void)optionalJsonString(meta, QStringLiteral("long_name"));
    }
    if (meta.contains(QStringLiteral("units")) && !meta.value(QStringLiteral("units")).isNull())
        (void)optionalJsonString(meta, QStringLiteral("units"));
    if (meta.contains(QStringLiteral("comment"))
        && !meta.value(QStringLiteral("comment")).isNull()) {
        (void)optionalJsonString(meta, QStringLiteral("comment"));
    }
    if (meta.contains(QStringLiteral("mode")) && !meta.value(QStringLiteral("mode")).isNull()) {
        const QString mStr = optionalJsonString(meta, QStringLiteral("mode")).trimmed().toLower();
        if (!mStr.isEmpty() && mStr != QStringLiteral("text") && mStr != QStringLiteral("string")
            && mStr != QStringLiteral("numeric") && mStr != QStringLiteral("double")
            && mStr != QStringLiteral("float") && mStr != QStringLiteral("number")
            && mStr != QStringLiteral("datetime") && mStr != QStringLiteral("date")
            && mStr != QStringLiteral("time") && mStr != QStringLiteral("month")
            && mStr != QStringLiteral("day")) {
            throwInvalidParams(
                    QStringLiteral("Unknown column mode '%1' (expected numeric|text|datetime|month|day).")
                            .arg(mStr));
        }
    }
    if (meta.contains(QStringLiteral("plot_designation"))
        && !meta.value(QStringLiteral("plot_designation")).isNull()) {
        const QString pdStr =
                optionalJsonString(meta, QStringLiteral("plot_designation")).trimmed();
        if (!pdStr.isEmpty()) {
            const QString pdLower = pdStr.toLower();
            const bool validPd = (pdLower == QStringLiteral("x") || pdLower == QStringLiteral("y")
                                  || pdLower == QStringLiteral("z")
                                  || pdLower == QStringLiteral("xerr")
                                  || pdLower == QStringLiteral("x_err")
                                  || pdLower == QStringLiteral("dx")
                                  || pdLower == QStringLiteral("yerr")
                                  || pdLower == QStringLiteral("y_err")
                                  || pdLower == QStringLiteral("dy")
                                  || pdLower == QStringLiteral("label")
                                  || pdLower == QStringLiteral("none")
                                  || pdLower == QStringLiteral("disregard")
                                  || pdLower == QStringLiteral("nodesignation"));
            if (!validPd) {
                throwInvalidParams(
                        QStringLiteral("Unknown plot_designation '%1' (expected X|Y|Z|xErr|yErr|Label|None).")
                                .arg(pdStr));
            }
        }
    }
    if (meta.contains(QStringLiteral("numeric_format"))
        && !meta.value(QStringLiteral("numeric_format")).isNull()) {
        const QString fmt = optionalJsonString(meta, QStringLiteral("numeric_format")).trimmed();
        if (!fmt.isEmpty()) {
            const char fc = fmt.at(0).toLatin1();
            if (fc != 'f' && fc != 'e' && fc != 'E' && fc != 'g' && fc != 'G') {
                throwInvalidParams(
                        QStringLiteral("Unsupported numeric_format '%1' (expected f|e|E|g|G).")
                                .arg(fmt));
            }
        }
    }
    if (meta.contains(QStringLiteral("precision"))
        && !meta.value(QStringLiteral("precision")).isNull()) {
        (void)parseBoundedInt(meta, QStringLiteral("precision"), 6, 0, 16);
    }
}

void applyColumnMetadataObject(Table *t, int colIdx, const QJsonObject &meta)
{
    Column *col = t->column(colIdx);
    if (!col)
        return;

    // 1. Validate all metadata parameters upfront before opening any QUndoStack macro.
    bool hasNewName = false;
    QString newName;
    if (meta.contains(QStringLiteral("name")) && !meta.value(QStringLiteral("name")).isNull()) {
        newName = optionalJsonString(meta, QStringLiteral("name")).trimmed();
        hasNewName = !newName.isEmpty() && (newName != t->colLabel(colIdx));
    }

    const QString oldLong = t->colLongName(colIdx);
    QString newLong = oldLong;
    bool hasNewLong = false;
    if (meta.contains(QStringLiteral("long_name"))
        && !meta.value(QStringLiteral("long_name")).isNull()) {
        newLong = optionalJsonString(meta, QStringLiteral("long_name"));
        hasNewLong = (newLong != oldLong);
    }

    const QString oldUnits = t->colUnits(colIdx);
    QString newUnits = oldUnits;
    bool hasNewUnits = false;
    if (meta.contains(QStringLiteral("units")) && !meta.value(QStringLiteral("units")).isNull()) {
        newUnits = optionalJsonString(meta, QStringLiteral("units"));
        hasNewUnits = (newUnits != oldUnits);
    }

    const QString oldComment = t->colComment(colIdx);
    QString newComment = oldComment;
    bool hasNewComment = false;
    if (meta.contains(QStringLiteral("comment"))
        && !meta.value(QStringLiteral("comment")).isNull()) {
        newComment = optionalJsonString(meta, QStringLiteral("comment"));
        hasNewComment = (newComment != oldComment);
    }

    bool hasNewMode = false;
    SciDAVis::ColumnMode targetMode = col->columnMode();
    if (meta.contains(QStringLiteral("mode")) && !meta.value(QStringLiteral("mode")).isNull()) {
        const QString mStr = optionalJsonString(meta, QStringLiteral("mode")).trimmed().toLower();
        if (mStr == QStringLiteral("text") || mStr == QStringLiteral("string")) {
            targetMode = SciDAVis::ColumnMode::Text;
            hasNewMode = (col->columnMode() != targetMode);
        } else if (mStr == QStringLiteral("numeric") || mStr == QStringLiteral("double")
                   || mStr == QStringLiteral("float") || mStr == QStringLiteral("number")) {
            targetMode = SciDAVis::ColumnMode::Numeric;
            hasNewMode = (col->columnMode() != targetMode);
        } else if (mStr == QStringLiteral("datetime") || mStr == QStringLiteral("date")
                   || mStr == QStringLiteral("time")) {
            targetMode = SciDAVis::ColumnMode::DateTime;
            hasNewMode = (col->columnMode() != targetMode);
        } else if (mStr == QStringLiteral("month")) {
            targetMode = SciDAVis::ColumnMode::Month;
            hasNewMode = (col->columnMode() != targetMode);
        } else if (mStr == QStringLiteral("day")) {
            targetMode = SciDAVis::ColumnMode::Day;
            hasNewMode = (col->columnMode() != targetMode);
        } else if (!mStr.isEmpty()) {
            throwInvalidParams(
                    QStringLiteral("Unknown column mode '%1' (expected numeric|text|datetime|month|day).")
                            .arg(mStr));
        }
    }

    bool hasNewPd = false;
    bool forceLabelTextMode = false;
    SciDAVis::PlotDesignation targetPd = t->colPlotDesignation(colIdx);
    if (meta.contains(QStringLiteral("plot_designation"))
        && !meta.value(QStringLiteral("plot_designation")).isNull()) {
        const QString pdStr =
                optionalJsonString(meta, QStringLiteral("plot_designation")).trimmed();
        if (!pdStr.isEmpty()) {
            const QString pdLower = pdStr.toLower();
            const bool validPd = (pdLower == QStringLiteral("x") || pdLower == QStringLiteral("y")
                                  || pdLower == QStringLiteral("z")
                                  || pdLower == QStringLiteral("xerr")
                                  || pdLower == QStringLiteral("x_err")
                                  || pdLower == QStringLiteral("dx")
                                  || pdLower == QStringLiteral("yerr")
                                  || pdLower == QStringLiteral("y_err")
                                  || pdLower == QStringLiteral("dy")
                                  || pdLower == QStringLiteral("label")
                                  || pdLower == QStringLiteral("none")
                                  || pdLower == QStringLiteral("disregard")
                                  || pdLower == QStringLiteral("nodesignation"));
            if (!validPd) {
                throwInvalidParams(
                        QStringLiteral("Unknown plot_designation '%1' (expected X|Y|Z|xErr|yErr|Label|None).")
                                .arg(pdStr));
            }
            if (pdLower == QStringLiteral("label")
                && (hasNewMode ? targetMode : col->columnMode()) != SciDAVis::ColumnMode::Text) {
                forceLabelTextMode = true;
            }
            targetPd = parsePlotDesignation(pdStr, t->colPlotDesignation(colIdx));
            hasNewPd = (targetPd != t->colPlotDesignation(colIdx)) || forceLabelTextMode;
        }
    }

    bool hasNewNumFmt = false;
    char newNumFmtChar = 'g';
    bool hasNewPrec = false;
    int newPrecVal = 6;
    if (meta.contains(QStringLiteral("numeric_format"))
        || meta.contains(QStringLiteral("precision"))) {
        if (meta.contains(QStringLiteral("numeric_format"))
            && !meta.value(QStringLiteral("numeric_format")).isNull()) {
            const QString fmt =
                    optionalJsonString(meta, QStringLiteral("numeric_format")).trimmed();
            if (!fmt.isEmpty()) {
                const char fc = fmt.at(0).toLatin1();
                if (fc == 'f' || fc == 'e' || fc == 'E' || fc == 'g' || fc == 'G') {
                    newNumFmtChar = fc;
                    hasNewNumFmt = true;
                } else {
                    throwInvalidParams(
                            QStringLiteral("Unsupported numeric_format '%1' (expected f|e|E|g|G).")
                                    .arg(fmt));
                }
            }
        }
        if (meta.contains(QStringLiteral("precision"))
            && !meta.value(QStringLiteral("precision")).isNull()) {
            newPrecVal = parseBoundedInt(meta, QStringLiteral("precision"), 6, 0, 16);
            hasNewPrec = true;
        }
    }

    const bool anyMetaMutation = hasNewName || hasNewLong || hasNewUnits || hasNewComment
            || hasNewMode || hasNewPd || hasNewNumFmt || hasNewPrec;
    if (!anyMetaMutation)
        return;

    struct ScopedMetaUndoMacro
    {
        future::Table *ft = nullptr;
        ScopedMetaUndoMacro(future::Table *f, const QString &lbl) : ft(f)
        {
            if (ft && ft->undoStack())
                ft->beginMacro(lbl);
            else
                ft = nullptr;
        }
        ~ScopedMetaUndoMacro()
        {
            if (ft)
                ft->endMacro();
        }
    } metaMacro(t->d_future_table, QStringLiteral("Set Column Metadata"));

    if (hasNewName)
        t->setColName(colIdx, newName);

    // Column::setLongName / setUnits push ColumnSetLongNameCmd / ColumnSetUnitsCmd themselves
    // (inside the macro above); their undo()/redo() only touch the private column data, so the
    // Redo history survives an Undo. A wrapper command calling these setters from its own
    // undo()/redo() would push new commands from inside the stack and wipe the Redo entries.
    if (hasNewLong)
        t->setColLongName(colIdx, newLong);
    if (hasNewUnits)
        t->setColUnits(colIdx, newUnits);

    if (hasNewComment)
        t->setColComment(colIdx, newComment);

    if (hasNewMode && col->columnMode() != targetMode)
        col->setColumnMode(targetMode);

    if (hasNewPd) {
        if (forceLabelTextMode && col->columnMode() != SciDAVis::ColumnMode::Text)
            col->setColumnMode(SciDAVis::ColumnMode::Text);
        if (t->colPlotDesignation(colIdx) != targetPd)
            t->setColPlotDesignation(colIdx, targetPd);
    }

    if (hasNewNumFmt || hasNewPrec) {
        if (auto *f = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
            if (hasNewNumFmt && f->numericFormat() != newNumFmtChar)
                f->setNumericFormat(newNumFmtChar);
            if (hasNewPrec && f->numDigits() != newPrecVal)
                f->setNumDigits(newPrecVal);
        }
    }
}

QJsonObject computeNumericStatsJson(std::vector<double> vals)
{
    QJsonObject res;
    vals.erase(std::remove_if(vals.begin(), vals.end(),
                              [](double v) { return !std::isfinite(v); }),
               vals.end());
    const int n = static_cast<int>(vals.size());
    res[QStringLiteral("n")] = n;
    if (n == 0) {
        res[QStringLiteral("mean")] = QJsonValue::Null;
        res[QStringLiteral("sd")] = QJsonValue::Null;
        res[QStringLiteral("se")] = QJsonValue::Null;
        res[QStringLiteral("min")] = QJsonValue::Null;
        res[QStringLiteral("q1")] = QJsonValue::Null;
        res[QStringLiteral("median")] = QJsonValue::Null;
        res[QStringLiteral("q3")] = QJsonValue::Null;
        res[QStringLiteral("iqr")] = QJsonValue::Null;
        res[QStringLiteral("max")] = QJsonValue::Null;
        res[QStringLiteral("sum")] = 0.0;
        return res;
    }

    std::sort(vals.begin(), vals.end());
    const double sum = std::accumulate(vals.begin(), vals.end(), 0.0);
    const double mean = sum / double(n);
    double varSum = 0.0;
    for (double v : vals) {
        const double d = v - mean;
        varSum += d * d;
    }
    // the sample standard deviation needs n >= 2: with one value sd / se are undefined (null,
    // as for n == 0), not 0
    const double sd =
            (n > 1) ? std::sqrt(varSum / double(n - 1)) : std::numeric_limits<double>::quiet_NaN();
    const double se =
            (n > 1) ? (sd / std::sqrt(double(n))) : std::numeric_limits<double>::quiet_NaN();

    auto quantileSorted = [&](double q) -> double {
        if (vals.size() == 1)
            return vals[0];
        const double pos = q * double(vals.size() - 1);
        const size_t lo = static_cast<size_t>(std::floor(pos));
        const size_t hi = std::min(vals.size() - 1, lo + 1);
        const double frac = pos - double(lo);
        return vals[lo] * (1.0 - frac) + vals[hi] * frac;
    };

    const double q1 = quantileSorted(0.25);
    const double med = quantileSorted(0.50);
    const double q3 = quantileSorted(0.75);

    res[QStringLiteral("mean")] = safeJsonDouble(mean);
    res[QStringLiteral("sd")] = safeJsonDouble(sd);
    res[QStringLiteral("se")] = safeJsonDouble(se);
    res[QStringLiteral("min")] = safeJsonDouble(vals.front());
    res[QStringLiteral("q1")] = safeJsonDouble(q1);
    res[QStringLiteral("median")] = safeJsonDouble(med);
    res[QStringLiteral("q3")] = safeJsonDouble(q3);
    res[QStringLiteral("iqr")] = safeJsonDouble(q3 - q1);
    res[QStringLiteral("max")] = safeJsonDouble(vals.back());
    res[QStringLiteral("sum")] = safeJsonDouble(sum);
    return res;
}

// Pure-C++ partial-pivoting LU decomposition (eliminates GSL 0xC0000005 segfault on zero/singular matrices)
struct PureLuResult
{
    int n = 0;
    std::vector<double> lu;
    std::vector<int> piv;
    int signum = 1;
    double norm1 = 0.0;
    bool singular = false;
};

// Square-shape, size and finite-entry checks shared by the LU (invert) and determinant paths;
// returns the cells row-major and their 1-norm.
std::vector<double> readSquareMatrixChecked(Matrix *m, const QString &opLabel, double *norm1Out)
{
    const int n = m->numRows();
    if (n <= 0 || n != m->numCols()) {
        throwInvalidParams(
                QStringLiteral("Matrix '%1' must be square (%2x%3) for %4.")
                        .arg(m->name())
                        .arg(m->numRows())
                        .arg(m->numCols())
                        .arg(opLabel));
    }
    if (n > 4096) {
        throwInvalidParams(
                QStringLiteral("Matrix '%1' (%2x%2) exceeds maximum %3 dimension (4096).")
                        .arg(m->name())
                        .arg(n)
                        .arg(opLabel));
    }

    std::vector<double> a(size_t(n) * size_t(n), 0.0);
    double norm1 = 0.0;
    for (int c = 0; c < n; ++c) {
        double colAbsSum = 0.0;
        for (int r = 0; r < n; ++r) {
            const double v = m->cell(r, c);
            if (!std::isfinite(v) || std::abs(v) > 1e300) {
                throwInvalidParams(
                        QStringLiteral("Matrix '%1' contains non-finite or out-of-domain value at (%2, %3).")
                                .arg(m->name())
                                .arg(r)
                                .arg(c));
            }
            a[size_t(r) * size_t(n) + size_t(c)] = v;
            colAbsSum += std::abs(v);
        }
        norm1 = std::max(norm1, colAbsSum);
    }
    if (norm1Out)
        *norm1Out = norm1;
    return a;
}

PureLuResult factorizeSquareMatrixPureLu(Matrix *m, const QString &opLabel)
{
    PureLuResult res;
    res.lu = readSquareMatrixChecked(m, opLabel, &res.norm1);
    const int n = m->numRows();
    res.n = n;
    res.piv.resize(size_t(n));
    for (int i = 0; i < n; ++i)
        res.piv[size_t(i)] = i;

    if (res.norm1 == 0.0) {
        res.singular = true;
        return res;
    }

    const double pivTol = std::numeric_limits<double>::epsilon() * res.norm1;
    for (int k = 0; k < n; ++k) {
        int pivRow = k;
        double maxAbs = std::abs(res.lu[size_t(k) * size_t(n) + size_t(k)]);
        for (int i = k + 1; i < n; ++i) {
            const double cand = std::abs(res.lu[size_t(i) * size_t(n) + size_t(k)]);
            if (cand > maxAbs) {
                maxAbs = cand;
                pivRow = i;
            }
        }
        if (maxAbs <= pivTol || !std::isfinite(maxAbs)) {
            res.singular = true;
            return res;
        }
        if (pivRow != k) {
            for (int j = 0; j < n; ++j) {
                std::swap(res.lu[size_t(k) * size_t(n) + size_t(j)],
                          res.lu[size_t(pivRow) * size_t(n) + size_t(j)]);
            }
            std::swap(res.piv[size_t(k)], res.piv[size_t(pivRow)]);
            res.signum = -res.signum;
        }
        const double diag = res.lu[size_t(k) * size_t(n) + size_t(k)];
        for (int i = k + 1; i < n; ++i) {
            const double mult = res.lu[size_t(i) * size_t(n) + size_t(k)] / diag;
            res.lu[size_t(i) * size_t(n) + size_t(k)] = mult;
            for (int j = k + 1; j < n; ++j) {
                res.lu[size_t(i) * size_t(n) + size_t(j)] -=
                        mult * res.lu[size_t(k) * size_t(n) + size_t(j)];
            }
        }
    }
    return res;
}

double computeMatrixDeterminantHeadless(Matrix *m)
{
    // Same computation as the GUI's Matrix::determinant() (plain partial-pivot LU, as LAPACK);
    // the inverse keeps its own rcond test above.
    const std::vector<double> a = readSquareMatrixChecked(m, QStringLiteral("determinant"), nullptr);
    const double det = Matrix::denseDeterminant(a.data(), m->numRows());
    if (!std::isfinite(det)) {
        throwRpcRuntime(
                QStringLiteral("Matrix '%1' determinant overflowed IEEE-754 double range.")
                        .arg(m->name()));
    }
    return det;
}

void invertMatrixHeadless(ApplicationWindow *app, Matrix *m)
{
    const PureLuResult lu = factorizeSquareMatrixPureLu(m, QStringLiteral("invert"));
    const int n = lu.n;
    if (lu.singular) {
        throwRpcRuntime(
                QStringLiteral("Matrix '%1' is singular and cannot be inverted.").arg(m->name()));
    }

    std::vector<double> inv(size_t(n) * size_t(n), 0.0);
    std::vector<double> x(size_t(n), 0.0);
    double invNorm1 = 0.0;

    for (int col = 0; col < n; ++col) {
        for (int i = 0; i < n; ++i)
            x[size_t(i)] = (lu.piv[size_t(i)] == col) ? 1.0 : 0.0;
        for (int i = 0; i < n; ++i) {
            double s = x[size_t(i)];
            for (int j = 0; j < i; ++j)
                s -= lu.lu[size_t(i) * size_t(n) + size_t(j)] * x[size_t(j)];
            x[size_t(i)] = s;
        }
        for (int i = n - 1; i >= 0; --i) {
            double s = x[size_t(i)];
            for (int j = i + 1; j < n; ++j)
                s -= lu.lu[size_t(i) * size_t(n) + size_t(j)] * x[size_t(j)];
            const double diag = lu.lu[size_t(i) * size_t(n) + size_t(i)];
            if (diag == 0.0 || !std::isfinite(s / diag)) {
                throwRpcRuntime(
                        QStringLiteral("Matrix '%1' is singular and cannot be inverted.")
                                .arg(m->name()));
            }
            x[size_t(i)] = s / diag;
        }
        double colAbsSum = 0.0;
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(x[size_t(i)])) {
                throwRpcRuntime(
                        QStringLiteral("Matrix '%1' is singular or ill-conditioned and cannot be inverted.")
                                .arg(m->name()));
            }
            inv[size_t(i) * size_t(n) + size_t(col)] = x[size_t(i)];
            colAbsSum += std::abs(x[size_t(i)]);
        }
        invNorm1 = std::max(invNorm1, colAbsSum);
    }

    const double rcond = (lu.norm1 > 0.0 && invNorm1 > 0.0 && std::isfinite(invNorm1))
            ? (1.0 / (lu.norm1 * invNorm1))
            : 0.0;
    if (!std::isfinite(rcond) || rcond < 1e-13 * double(n)) {
        throwRpcRuntime(
                QStringLiteral("Matrix '%1' is singular or numerically rank-deficient (rcond=%2) and cannot be inverted.")
                        .arg(m->name())
                        .arg(rcond, 0, 'e', 2));
    }

    if (m->d_future_matrix)
        m->d_future_matrix->beginMacro(QObject::tr("%1: invert").arg(m->name()));
    for (int c = 0; c < n; ++c) {
        QVector<qreal> colVec(n);
        for (int r = 0; r < n; ++r)
            colVec[r] = inv[size_t(r) * size_t(n) + size_t(c)];
        m->d_future_matrix->setColumnCells(c, 0, n - 1, colVec);
    }
    if (m->d_future_matrix)
        m->d_future_matrix->endMacro();
    if (app)
        app->updateMatrixPlots(m);
}

QByteArray readGzipHeaderBytes(const QString &filePath, int maxOutBytes)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    const QByteArray compressed = f.read(std::max(1024, maxOutBytes * 4));
    f.close();
    if (compressed.isEmpty())
        return QByteArray();

    z_stream zs{};
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK)
        return QByteArray();

    QByteArray out(maxOutBytes, '\0');
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    zs.avail_in = static_cast<uInt>(compressed.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    const int rc = inflate(&zs, Z_SYNC_FLUSH);
    const int produced = static_cast<int>(zs.total_out);
    inflateEnd(&zs);
    if ((rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) || produced <= 0)
        return QByteArray();
    out.resize(produced);
    return out;
}

void restoreProjectSnapshotOnError(ApplicationWindow *app, const QByteArray &snap,
                                   const QString &savedProjName, bool savedWasSaved,
                                   const QStringList &savedRecentProjects,
                                   const QString &savedRootFolderName,
                                   bool reacquireProjectTracking)
{
    if (!app)
        return;

    if (reacquireProjectTracking && !savedProjName.isEmpty()
        && savedProjName != QLatin1String("untitled") && QFileInfo::exists(savedProjName)
        && (savedProjName.endsWith(QLatin1String(".sciprj"), Qt::CaseInsensitive)
            || savedProjName.endsWith(QLatin1String(".sciprj.gz"), Qt::CaseInsensitive))
        && QFileInfo(savedProjName).size() <= 32LL * 1024LL * 1024LL) {
        (void)app->loadProject(savedProjName);
    }

    resetProjectInPlaceHeadless(app);
    if (!snap.isEmpty()) {
        QTemporaryFile tmpProj(
                QDir::temp().filePath(QStringLiteral("torigraph_open_rb_XXXXXX.sciprj~")));
        if (tmpProj.open()) {
            const QString tmpPath = tmpProj.fileName();
            tmpProj.write(snap);
            tmpProj.flush();
            tmpProj.close();
            (void)app->loadProject(tmpPath);
            QFile::remove(tmpPath);
            for (QFileSystemWatcher *watcher : app->findChildren<QFileSystemWatcher *>()) {
                if (!watcher)
                    continue;
                for (const QString &watched : watcher->files()) {
                    if (watched == tmpPath || watched.contains(QLatin1String("torigraph_open_rb_")))
                        watcher->removePath(watched);
                }
            }
        }
    }

    QStringList restoredRecent = savedRecentProjects;
    for (int i = restoredRecent.size() - 1; i >= 0; --i) {
        if (restoredRecent.at(i).contains(QLatin1String("torigraph_open_rb_")))
            restoredRecent.removeAt(i);
    }
    app->recentProjects = restoredRecent;
    app->updateRecentProjectsList();

    app->projectname = savedProjName.isEmpty() ? QStringLiteral("untitled") : savedProjName;
    if (app->projectname == QLatin1String("untitled")) {
        app->setWindowTitle(ApplicationWindow::tr("ToriGraph - untitled"));
    } else {
        app->setWindowTitle(ApplicationWindow::tr("ToriGraph") + QStringLiteral(" - ")
                            + app->projectname);
    }

    if (Folder *rf = app->projectFolder()) {
        const QString targetRootName = !savedRootFolderName.isEmpty()
                ? savedRootFolderName
                : ((app->projectname == QLatin1String("untitled"))
                           ? ApplicationWindow::tr("UNTITLED")
                           : QFileInfo(app->projectname).baseName());
        rf->setName(targetRootName);
        if (rf->folderListItem()) {
            QSignalBlocker sbFolders(&app->folders);
            rf->folderListItem()->setText(0, targetRootName);
        }
    }

    if (savedWasSaved)
        app->savedProject();
    else
        app->modifiedProject();
}

struct ParsedCsvCell
{
    QString value;
    bool quoted = false;
};

bool isMissingNumericSentinel(const QString &trimmed)
{
    if (trimmed.isEmpty())
        return true;
    const QString low = trimmed.toLower();
    return low == QStringLiteral("-") || low == QStringLiteral("--") || low == QStringLiteral("---")
            || low == QStringLiteral("n/a") || low == QStringLiteral("na")
            || low == QStringLiteral("nan") || low == QStringLiteral("+nan")
            || low == QStringLiteral("-nan") || low == QStringLiteral("inf")
            || low == QStringLiteral("+inf") || low == QStringLiteral("-inf")
            || low == QStringLiteral("infinity") || low == QStringLiteral("+infinity")
            || low == QStringLiteral("-infinity") || low == QStringLiteral("null")
            || low == QStringLiteral("none")
            // a number too large for a double ("1e400"), read as the GUI import reads it
            || AsciiTableImportFilter::isOverflowNumberToken(trimmed);
}

//! Missing value of a numeric column in the text import, the same test as the GUI import
//! (AsciiTableImportFilter): the missing / Excel error tokens it shares ("", "-", "--", "N.D.",
//! "ND", "#N/A", "#DIV/0!", ..., also when quoted), "none", and unless quoted "inf" /
//! "infinity" and numbers that overflow a double ("1e400").
bool isImportMissingNumericCell(const QString &trimmed, bool wasQuoted)
{
    if (AsciiTableImportFilter::isMissingDataToken(trimmed))
        return true;
    if (!wasQuoted)
        return isMissingNumericSentinel(trimmed);
    return trimmed.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0;
}

//! Turns a numeric column into a text column (table.paste_tsv / table.set_cell of a value that
//! is not a number) with its numbers written exactly: the shortest text that reads back as the
//! same double (at most 17 digits), not the column's display format, whose 6 digits would cut
//! 1000.3333333333334 to 1000.33. Invalid cells stay invalid.
void convertNumericColumnToTextExactly(Column *col)
{
    if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric)
        return;
    const int rows = col->rowCount();
    QStringList texts;
    texts.reserve(rows);
    QVector<Interval<int>> invalid;
    for (int r = 0; r < rows; ++r) {
        if (col->isInvalid(r)) {
            texts << QString();
            if (!invalid.isEmpty() && invalid.last().end() == r - 1)
                invalid.last().setEnd(r);
            else
                invalid << Interval<int>(r, r);
        } else {
            texts << QString::number(col->valueAt(r), 'g', QLocale::FloatingPointShortest);
        }
    }
    col->setColumnMode(SciDAVis::ColumnMode::Text);
    if (rows > 0) {
        col->replaceTexts(0, texts);
        for (const Interval<int> &iv : invalid)
            col->setInvalid(iv, true);
    }
}

int countUnquotedImportChar(const QString &line, QChar ch)
{
    int n = 0;
    bool inQuotes = false;
    for (const QChar c : line) {
        if (c == QLatin1Char('"'))
            inQuotes = !inQuotes;
        else if (!inQuotes && c == ch)
            ++n;
    }
    return n;
}

//! Up to `maxLines` trimmed non-empty lines from `startPos` (CR, LF or CRLF endings), without
//! the '#' comment lines -- and the ';' / '!' ones when `skipSemicolonBangLines` -- for the
//! separator / decimal-comma detection.
QStringList collectImportSampleLines(const QString &text, int startPos, int maxLines,
                                     bool skipSemicolonBangLines)
{
    QStringList out;
    const int len = text.size();
    int p = std::max(0, startPos);
    while (p < len && out.size() < maxLines) {
        int lineEnd = p;
        while (lineEnd < len && text[lineEnd] != QLatin1Char('\r')
               && text[lineEnd] != QLatin1Char('\n')) {
            ++lineEnd;
        }
        const QString trimmed = text.mid(p, lineEnd - p).trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1Char('#'))
            && !(skipSemicolonBangLines
                 && (trimmed.startsWith(QLatin1Char(';'))
                     || trimmed.startsWith(QLatin1Char('!'))))) {
            out.append(trimmed);
        }
        p = lineEnd;
        if (p < len && text[p] == QLatin1Char('\r'))
            ++p;
        if (p < len && text[p] == QLatin1Char('\n'))
            ++p;
    }
    return out;
}

QString autoDetectCsvSeparator(const QString &text, int startPos, const QString &fileHint,
                               bool *outWhitespace = nullptr)
{
    if (outWhitespace)
        *outWhitespace = false;
    if (fileHint.endsWith(QStringLiteral(".tsv"), Qt::CaseInsensitive)
        || fileHint.endsWith(QStringLiteral(".tab"), Qt::CaseInsensitive)) {
        return QStringLiteral("\t");
    }
    const bool isSpaceExt = fileHint.endsWith(QStringLiteral(".xy"), Qt::CaseInsensitive)
            || fileHint.endsWith(QStringLiteral(".xye"), Qt::CaseInsensitive)
            || fileHint.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive)
            || fileHint.endsWith(QStringLiteral(".asc"), Qt::CaseInsensitive);
    // Decided over several lines with the GUI import's rule (a delimiter that splits every
    // line into the same number of fields wins), not from the header line alone: a comma in a
    // column name ("Time, s;Force, N") must not beat the ';' / TAB of the data lines.
    const QStringList samples = collectImportSampleLines(text, startPos, 15, true);
    if (!samples.isEmpty()) {
        const QString detected = AsciiTableImportFilter::detectSeparator(samples, fileHint);
        if (detected == QLatin1String(" ")) {
            if (outWhitespace)
                *outWhitespace = true;
            return detected;
        }
        // The GUI rule falls back to "," / TAB when no line holds any delimiter: then a
        // single-column file, read as whitespace-separated for the XRD/ASCII extensions.
        bool delimiterSeen = false;
        if (detected.size() == 1) {
            for (const QString &ln : samples) {
                if (countUnquotedImportChar(ln, detected.at(0)) > 0) {
                    delimiterSeen = true;
                    break;
                }
            }
        }
        if (delimiterSeen)
            return detected;
    }
    if (isSpaceExt) {
        if (outWhitespace)
            *outWhitespace = true;
        return QStringLiteral(" ");
    }
    return QStringLiteral(",");
}

//! Decimal commas are read only with the ';' separator, when the lines look like a European
//! export ("0,5;1,5"), as the GUI import decides it (AsciiTableImportFilter).
bool detectImportDecimalComma(const QString &text, int startPos, const QString &sep,
                              bool convertNumeric)
{
    if (!convertNumeric || sep != QLatin1String(";"))
        return false;
    return AsciiTableImportFilter::looksLikeEuropeanDecimalComma(
            collectImportSampleLines(text, startPos, 24, false));
}

//! Number of an imported cell: C locale, or with `decimalComma` the German one first (the C
//! locale stays as fallback, "1.5" is still 1.5), as the GUI import reads it. Group separators
//! are never accepted ("1.234" is not 1234).
bool parseImportNumber(const QString &trimmed, bool decimalComma, double &out)
{
    static const QLocale cLoc = [] {
        QLocale l = QLocale::c();
        l.setNumberOptions(QLocale::RejectGroupSeparator);
        return l;
    }();
    bool ok = false;
    if (decimalComma) {
        static const QLocale deLoc = [] {
            QLocale l(QLocale::German);
            l.setNumberOptions(QLocale::RejectGroupSeparator);
            return l;
        }();
        out = deLoc.toDouble(trimmed, &ok);
        if (ok && std::isfinite(out))
            return true;
        // European decimal-comma Fortran exponents (e.g. "3,50D-02", "-4,125d+01"):
        // require a single comma with no dot anywhere, followed by a D/d exponent, and
        // delegate final syntax/range checks to AsciiTableImportFilter::parseNumberToken.
        const int commaPos = trimmed.indexOf(QLatin1Char(','));
        if (commaPos >= 0 && trimmed.indexOf(QLatin1Char(','), commaPos + 1) < 0
            && trimmed.indexOf(QLatin1Char('.')) < 0) {
            const int dPos = trimmed.indexOf(QLatin1Char('D'), commaPos + 1);
            const int ldPos = trimmed.indexOf(QLatin1Char('d'), commaPos + 1);
            if (dPos > commaPos || ldPos > commaPos) {
                QString normalized = trimmed;
                normalized[commaPos] = QLatin1Char('.');
                if (AsciiTableImportFilter::parseNumberToken(normalized, out))
                    return true;
            }
        }
    }
    out = cLoc.toDouble(trimmed, &ok);
    if (ok && std::isfinite(out))
        return true;
    // full-width digits ("２．５") and Fortran exponents ("1.0D+00"), as the GUI import reads them
    return AsciiTableImportFilter::parseNumberToken(trimmed, out);
}

//! A text written into an existing date/time column by table.paste_tsv or table.set_cell that
//! is a number: a day count from the column's numeric date/time base, as table.set_cell with a
//! JSON number ("45000" is the same cell as 45000). Through the input filter "2" and "45000"
//! were clock times on 1900-01-01 (its "h" / "hmmss" formats: 02:00, 04:50:00) and "45000.5" or
//! "1.5e4" empty cells. Digit strings written as a date or a clock time keep the filter's
//! reading: the column's own format, a date in the yyyyMMdd form ("20250506"), digits with a
//! leading zero ("0930", "093000") and six digits that are a valid hhmmss clock time ("130000"
//! stays 13:00:00 as before).
bool dateTimeTextAsDayCount(Column *col, const QString &trimmed, bool decimalComma, double &days)
{
    if (!col || col->columnMode() != SciDAVis::ColumnMode::DateTime || trimmed.isEmpty())
        return false;
    double v = 0.0;
    if (!parseImportNumber(trimmed, decimalComma, v) || !std::isfinite(v))
        return false;
    static const QRegularExpression leadingZeroDigits(QStringLiteral("^0[0-9]+$"));
    if (leadingZeroDigits.match(trimmed).hasMatch())
        return false;
    if (QDate::fromString(trimmed, QStringLiteral("yyyyMMdd")).isValid())
        return false;
    static const QRegularExpression sixDigits(QStringLiteral("^[0-9]{6}$"));
    if (sixDigits.match(trimmed).hasMatch()
        && QTime::fromString(trimmed, QStringLiteral("hhmmss")).isValid())
        return false;
    if (const auto *f = dynamic_cast<const String2DateTimeFilter *>(col->inputFilter())) {
        if (!f->format().isEmpty() && QDateTime::fromString(trimmed, f->format()).isValid())
            return false;
    }
    days = v;
    return true;
}

//! table.paste_tsv into an existing date/time column: numbers are day counts as in
//! table.set_cell (dateTimeTextAsDayCount), the other texts go through the column's input filter
//! as before; one command. Returns the rows (in `texts`) of cells that hold a value but cannot
//! be read: they are empty cells. The missing-value tokens of the import ("N.D.", "-", ...) are
//! not counted.
QVector<int> writeTextsToDateTimeColumn(Column *col, int firstRow, const QStringList &texts,
                                        const QVector<bool> &quoted, bool decimalComma)
{
    QVector<int> unreadable;
    const int n = texts.size();
    if (!col || n == 0)
        return unreadable;
    QStringList filterTexts = texts;
    QVector<int> dayRows;
    QVector<double> dayValues;
    for (int r = 0; r < n; ++r) {
        double days = 0.0;
        if (dateTimeTextAsDayCount(col, texts.at(r).trimmed(), decimalComma, days)) {
            filterTexts[r].clear();
            dayRows.append(r);
            dayValues.append(days);
        }
    }
    QList<QDateTime> values;
    values.reserve(n);
    {
        // the column's own input filter, as ColumnStringIO::copy uses it (a column with a time
        // zone offset keeps the offset of its first value)
        Column source(QStringLiteral("paste"), filterTexts);
        AbstractSimpleFilter *in = col->inputFilter();
        in->input(0, &source);
        const AbstractColumn *out = in->output(0);
        for (int r = 0; r < n; ++r)
            values.append(out->dateTimeAt(r));
        in->input(0, col->asStringColumn());
    }
    for (int k = 0; k < dayRows.size(); ++k)
        values[dayRows.at(k)] = calendarDateTimeFromNumber(col, dayValues.at(k));
    for (int r = 0; r < n; ++r) {
        if (values.at(r).isValid())
            continue;
        const QString trimmed = texts.at(r).trimmed();
        if (!trimmed.isEmpty() && !isImportMissingNumericCell(trimmed, quoted.value(r)))
            unreadable.append(r);
    }
    col->replaceDateTimes(firstRow, values);
    return unreadable;
}

QVector<QVector<ParsedCsvCell>> parseDelimitedRecordsRfc4180(const QString &text, int startPos,
                                                             const QString &sep, bool stripSpaces,
                                                             bool simplifySpaces, int &outMaxCols,
                                                             bool fileImport, bool decimalComma)
{
    QVector<QVector<ParsedCsvCell>> records;
    outMaxCols = 0;
    const int len = text.size();
    const int sepLen = std::max(1, sep.size());
    int pos = std::max(0, startPos);

    // Skip leading '#' comment preamble lines before the first header/data row, and for a file
    // import also leading blank lines (a blank first line must not become the header row).
    // Pasted text keeps them: a copied range whose first row is empty ("\t\r\n1\t2") is a row of
    // empty cells, and dropping it would shift the data up a row (or, with header=true, make
    // the first data row the column names).
    while (pos < len) {
        int probe = pos;
        while (probe < len && (text[probe] == QLatin1Char(' ') || text[probe] == QLatin1Char('\t')))
            ++probe;
        if (fileImport && probe < len
            && (text[probe] == QLatin1Char('\r') || text[probe] == QLatin1Char('\n'))) {
            pos = probe;
            if (text[pos] == QLatin1Char('\r'))
                ++pos;
            if (pos < len && text[pos] == QLatin1Char('\n'))
                ++pos;
            continue;
        }
        if (probe < len && text[probe] == QLatin1Char('#')) {
            while (pos < len && text[pos] != QLatin1Char('\r') && text[pos] != QLatin1Char('\n'))
                ++pos;
            if (pos < len && text[pos] == QLatin1Char('\r'))
                ++pos;
            if (pos < len && text[pos] == QLatin1Char('\n'))
                ++pos;
            continue;
        }
        break;
    }

    while (pos < len) {
        QVector<ParsedCsvCell> row;
        while (true) {
            ParsedCsvCell cell;
            const int fieldStart = pos;
            if (stripSpaces) {
                while (pos < len
                       && (text[pos] == QLatin1Char(' ')
                           || (sep != QStringLiteral("\t") && text[pos] == QLatin1Char('\t')))
                       && text.midRef(pos, sepLen) != sep) {
                    ++pos;
                }
            }
            if (pos < len && text[pos] == QLatin1Char('"')) {
                cell.quoted = true;
                ++pos;
                while (pos < len) {
                    const QChar ch = text[pos];
                    if (ch == QLatin1Char('"')) {
                        if (pos + 1 < len && text[pos + 1] == QLatin1Char('"')) {
                            cell.value.append(QLatin1Char('"'));
                            pos += 2;
                        } else {
                            ++pos;
                            break;
                        }
                    } else {
                        cell.value.append(ch);
                        ++pos;
                    }
                }
                while (pos < len && text[pos] != QLatin1Char('\r') && text[pos] != QLatin1Char('\n')
                       && text.midRef(pos, sepLen) != sep) {
                    if (text[pos] != QLatin1Char(' ') && text[pos] != QLatin1Char('\t'))
                        cell.value.append(text[pos]);
                    ++pos;
                }
            } else {
                pos = fieldStart;
                const int rawStart = pos;
                while (pos < len && text[pos] != QLatin1Char('\r') && text[pos] != QLatin1Char('\n')
                       && text.midRef(pos, sepLen) != sep) {
                    ++pos;
                }
                QString rawField = text.mid(rawStart, pos - rawStart);
                if (simplifySpaces)
                    rawField = rawField.simplified();
                else if (stripSpaces)
                    rawField = rawField.trimmed();
                cell.value = rawField;
            }
            row.append(cell);

            if (pos < len && text.midRef(pos, sepLen) == sep) {
                pos += sepLen;
                continue;
            }
            if (pos < len && text[pos] == QLatin1Char('\r')) {
                ++pos;
                if (pos < len && text[pos] == QLatin1Char('\n'))
                    ++pos;
            } else if (pos < len && text[pos] == QLatin1Char('\n')) {
                ++pos;
            }
            break;
        }
        records.append(row);
    }

    // File import: '#' lines further down (a comment between data blocks, "# units: ..." under
    // the header) are comments too, decided by the GUI import's '#'-row rule: "#1, 520" specimen
    // rows, "#N/A" cells and -- once data rows were read -- "#A1, 520" identifiers stay data.
    // (Pasted text keeps them, like a paste into the worksheet.)
    auto startsWithHash = [](const QVector<ParsedCsvCell> &rec) {
        if (rec.isEmpty() || rec[0].quoted)
            return false;
        for (const QChar ch : rec[0].value) {
            if (!ch.isSpace())
                return ch == QLatin1Char('#');
        }
        return false;
    };
    bool anyHashRow = false;
    for (int i = 0; fileImport && i < records.size() && !anyHashRow; ++i)
        anyHashRow = startsWithHash(records[i]);
    if (anyHashRow) {
        // as in the GUI: a '#'-only line of a multi-column table is a comment, of a one-column
        // table ("#1", "#2") it may be data
        bool tableHasMultipleCols = false;
        for (const QVector<ParsedCsvCell> &rec : records) {
            if (startsWithHash(rec))
                continue;
            int effCols = 0;
            for (int k = 0; k < rec.size(); ++k) {
                if (!rec[k].value.trimmed().isEmpty() || rec[k].quoted)
                    effCols = k + 1;
            }
            if (effCols >= 2) {
                tableHasMultipleCols = true;
                break;
            }
        }
        const bool whitespaceDelim = (sep == QLatin1String(" "));
        // numbers read as the import reads the cells: with decimal commas the German locale
        // first, as the GUI import does ("#A1;12,5;3,4" after the data rows is a specimen row,
        // not a comment), the C locale as fallback
        QLocale cLoc = QLocale::c();
        cLoc.setNumberOptions(QLocale::RejectGroupSeparator);
        QLocale numLoc = decimalComma ? QLocale(QLocale::German) : QLocale::c();
        numLoc.setNumberOptions(QLocale::RejectGroupSeparator);
        auto allNumeric = [&](const QVector<ParsedCsvCell> &rec) {
            int n = 0;
            for (const ParsedCsvCell &cell : rec) {
                const QString tok = cell.value.trimmed();
                if (tok.isEmpty())
                    continue;
                bool ok = false;
                if (decimalComma)
                    numLoc.toDouble(tok, &ok);
                if (!ok)
                    cLoc.toDouble(tok, &ok);
                if (!ok || cell.quoted)
                    return false;
                ++n;
            }
            return n > 0;
        };
        QVector<QVector<ParsedCsvCell>> kept;
        kept.reserve(records.size());
        for (QVector<ParsedCsvCell> &rec : records) {
            if (startsWithHash(rec)) {
                // data rows seen: past the header row, or a headerless file starting with numbers
                const bool dataSeen =
                        kept.size() >= 2 || (kept.size() == 1 && allNumeric(kept.first()));
                QStringList cells;
                QVector<bool> quoted;
                cells.reserve(rec.size());
                quoted.reserve(rec.size());
                for (const ParsedCsvCell &cell : rec) {
                    cells.append(cell.value);
                    quoted.append(cell.quoted);
                }
                if (AsciiTableImportFilter::isHashCommentRow(cells, quoted, whitespaceDelim,
                                                             tableHasMultipleCols, dataSeen,
                                                             numLoc))
                    continue;
            }
            kept.append(std::move(rec));
        }
        records = std::move(kept);
    }
    for (const QVector<ParsedCsvCell> &rec : records)
        outMaxCols = std::max(outMaxCols, rec.size());

    outMaxCols = std::max(1, outMaxCols);
    while (!records.isEmpty() && outMaxCols > 1 && records.last().size() == 1
           && !records.last()[0].quoted && records.last()[0].value.isEmpty()) {
        records.removeLast();
    }
    // Every line ending in a separator ("x,y,", "1,2,") with nothing after it, the header line
    // included: the separator terminates the line, it does not open an empty last column (as
    // in the GUI import). Blank lines do not count. Only for a file import, which makes a new
    // table: pasted text (table.paste_tsv) is a cell range, and its empty last column clears
    // the cells it is pasted over.
    if (fileImport && outMaxCols >= 2) {
        bool allEndWithSep = true;
        int nonBlank = 0;
        for (const QVector<ParsedCsvCell> &rec : records) {
            bool blank = true;
            for (const ParsedCsvCell &cell : rec) {
                if (cell.quoted || !cell.value.trimmed().isEmpty()) {
                    blank = false;
                    break;
                }
            }
            if (blank)
                continue;
            ++nonBlank;
            if (rec.size() != outMaxCols || rec.last().quoted
                || !rec.last().value.trimmed().isEmpty()) {
                allEndWithSep = false;
                break;
            }
        }
        if (allEndWithSep && nonBlank > 0) {
            for (QVector<ParsedCsvCell> &rec : records) {
                if (rec.size() == outMaxCols)
                    rec.removeLast();
            }
            --outMaxCols;
        }
    }
    return records;
}

bool looksLikeSecondRowUnits(const QVector<ParsedCsvCell> &candUnitRow,
                             const QVector<ParsedCsvCell> &firstDataRow, int maxCols,
                             bool decimalComma)
{
    int numericColsInDataRow = 0;
    int nonEmptyUnitTokens = 0;
    for (int c = 0; c < maxCols; ++c) {
        const QString uTxt = (c < candUnitRow.size()) ? candUnitRow[c].value.trimmed() : QString();
        const QString dTxt =
                (c < firstDataRow.size()) ? firstDataRow[c].value.trimmed() : QString();
        // A row holding a number in any column is data, not units (as for xlsx): also when the
        // next row fills other columns ("1,10,," above ",,1.5,20").
        double uv = 0.0;
        if (!uTxt.isEmpty() && parseImportNumber(uTxt, decimalComma, uv))
            return false;
        double dv = 0.0;
        if (parseImportNumber(dTxt, decimalComma, dv))
            ++numericColsInDataRow;
        if (!uTxt.isEmpty()) {
            if (uTxt.size() > 28)
                return false;
            ++nonEmptyUnitTokens;
        }
    }
    return (numericColsInDataRow >= 1 && nonEmptyUnitTokens >= 1);
}

int parseUnitsRowHintParam(const QJsonObject &params)
{
    if (!params.contains(QStringLiteral("units_row"))
        || params.value(QStringLiteral("units_row")).isNull()
        || params.value(QStringLiteral("units_row")).isUndefined()) {
        return -1; // auto
    }
    const QJsonValue v = params.value(QStringLiteral("units_row"));
    if (v.isBool())
        return v.toBool() ? 1 : 0;
    if (v.isDouble()) {
        const int iv = parseStrictJsonInt(v, QStringLiteral("units_row"));
        if (iv < -1 || iv > 2) {
            throwInvalidParams(
                    QStringLiteral("Parameter 'units_row' integer must be in [-1, 2], got %1.")
                            .arg(iv));
        }
        return iv;
    }
    if (v.isString()) {
        const QString s = v.toString().trimmed().toLower();
        if (s == QStringLiteral("auto"))
            return -1;
        if (s == QStringLiteral("none") || s == QStringLiteral("false")
            || s == QStringLiteral("0"))
            return 0;
        if (s == QStringLiteral("true") || s == QStringLiteral("1"))
            return 1;
        throwInvalidParams(
                QStringLiteral("Unsupported 'units_row' string '%1' (expected 'auto'|'none'|'true'|'false').")
                        .arg(v.toString()));
    }
    throwInvalidParams(
            QStringLiteral("Parameter 'units_row' must be a boolean, integer, or string."));
}

void populateDelimitedRecordsIntoTable(Table *t, const QVector<QVector<ParsedCsvCell>> &records,
                                       int maxCols, bool hasHeader, bool withComments,
                                       int unitsRowHint, bool convertNumeric, int startRowOffset,
                                       int startColOffset, bool assignDefaultPlotDesignations,
                                       bool decimalComma, bool keepExistingColumnTypes = false,
                                       QStringList *warnings = nullptr)
{
    QVector<ParsedCsvCell> headerRow;
    QVector<ParsedCsvCell> unitsRowCells;
    QVector<ParsedCsvCell> commentRow;
    int dataStartIdx = 0;
    if (hasHeader && !records.isEmpty()) {
        headerRow = records[0];
        dataStartIdx = 1;
        if (!withComments) {
            bool useRowAsUnits = false;
            if (unitsRowHint >= 1 && dataStartIdx < records.size()) {
                useRowAsUnits = true;
            } else if (unitsRowHint < 0 && records.size() >= 3) {
                useRowAsUnits =
                        looksLikeSecondRowUnits(records[1], records[2], maxCols, decimalComma);
            }
            if (useRowAsUnits && dataStartIdx < records.size()) {
                unitsRowCells = records[dataStartIdx];
                ++dataStartIdx;
            }
        }
    }
    if (withComments && dataStartIdx < records.size()) {
        commentRow = records[dataStartIdx];
        ++dataStartIdx;
    }

    const int dataRowCount = std::max(0, records.size() - dataStartIdx);
    const int neededCols = std::max(1, startColOffset + maxCols);
    const int neededRows = std::max(1, startRowOffset + dataRowCount);
    // every name the table had before the import (before the columns added below)
    const QStringList namesBeforeImport = t->colNames();
    const int colsBeforeImport = t->numCols();
    if (t->numCols() < neededCols) {
        const int oldCols = t->numCols();
        t->setNumCols(neededCols);
        for (int nc = oldCols; nc < neededCols; ++nc)
            applyDefaultNumericFormat6g(t, nc);
    }
    if (t->numRows() < neededRows)
        t->setNumRows(neededRows);

    // The header columns are re-pointed through temporary names below: references keep theirs,
    // except names that are gone afterwards (propagateRelabeledColumnNames at the end).
    const bool relabelHeader = hasHeader && !headerRow.isEmpty() && startRowOffset == 0;
    const QStringList oldHeaderNames =
            relabelHeader ? t->colNamesInRange(startColOffset, maxCols) : QStringList();
    Table::RenamePropagationBlocker renameBlocker(t);
    if (relabelHeader) {
        for (int c = 0; c < maxCols; ++c) {
            t->setColName(startColOffset + c,
                          QStringLiteral("__import_col_%1__").arg(startColOffset + c));
        }
    }

    for (int c = 0; c < maxCols; ++c) {
        const int dstCol = startColOffset + c;
        Column *col = t->column(dstCol);
        if (!col)
            continue;

        // table.paste_tsv into a column that existed before: the column keeps its type, as
        // table.set_cell does, so the rows that are not pasted over keep their data. Text
        // columns take the texts; date/time, month and day columns read them through their
        // input filter; numeric columns take numbers and missing values, a few values that are
        // not numbers as invalid cells (the import's 90 % rule), and become text columns (their
        // numbers kept exactly as text) when more of the pasted values are not numbers. Default
        // empty numeric columns and columns added by the paste take the type of the data.
        bool keepType = false;
        if (keepExistingColumnTypes && convertNumeric && dstCol < colsBeforeImport) {
            keepType = col->columnMode() != SciDAVis::ColumnMode::Numeric;
            const int pasteEnd = startRowOffset + dataRowCount;
            for (int r = 0; !keepType && r < col->rowCount(); ++r) {
                if (r >= startRowOffset && r < pasteEnd) {
                    r = pasteEnd - 1;
                    continue;
                }
                keepType = !col->isInvalid(r);
            }
        }
        if (keepType) {
            QStringList texts;
            texts.reserve(dataRowCount);
            for (int r = 0; r < dataRowCount; ++r) {
                const QVector<ParsedCsvCell> &rec = records[dataStartIdx + r];
                texts.append((c < rec.size()) ? rec[c].value : QString());
            }
            if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                // The pasted values decide as in the import (the 90 % rule below): a few values
                // that are not numbers ("OVER") are invalid cells of the numeric column.
                QVector<qreal> nums(dataRowCount, std::numeric_limits<double>::quiet_NaN());
                QVector<int> invalidRows;
                int validNum = 0, strayNonNum = 0;
                for (int r = 0; r < dataRowCount; ++r) {
                    const QString trimmed = texts[r].trimmed();
                    const bool wasQuoted = (c < records[dataStartIdx + r].size())
                            && records[dataStartIdx + r][c].quoted;
                    if (trimmed.isEmpty() || isImportMissingNumericCell(trimmed, wasQuoted)) {
                        invalidRows.append(r);
                    } else if (parseImportNumber(trimmed, decimalComma, nums[r])) {
                        ++validNum;
                    } else {
                        nums[r] = std::numeric_limits<double>::quiet_NaN();
                        invalidRows.append(r);
                        ++strayNonNum;
                    }
                }
                const bool staysNumeric = strayNonNum == 0
                        || (validNum >= 5
                            && qint64(validNum) * 10 >= qint64(validNum + strayNonNum) * 9);
                if (staysNumeric) {
                    if (dataRowCount > 0) {
                        col->setInvalid(
                                Interval<int>(startRowOffset, startRowOffset + dataRowCount - 1),
                                false);
                        col->replaceValues(startRowOffset, nums);
                        for (int r : invalidRows)
                            col->setInvalid(startRowOffset + r, true);
                    }
                } else {
                    // the rows that are not pasted over keep their numbers exactly
                    convertNumericColumnToTextExactly(col);
                    if (dataRowCount > 0)
                        col->replaceTexts(startRowOffset, texts);
                }
            } else if (dataRowCount > 0 && col->columnMode() == SciDAVis::ColumnMode::DateTime) {
                // date/time: numbers are day counts as in table.set_cell, other texts through
                // the column's input filter; a value that cannot be read is an empty cell and
                // a warning, not a silent loss
                QVector<bool> quoted(dataRowCount, false);
                for (int r = 0; r < dataRowCount; ++r) {
                    const QVector<ParsedCsvCell> &rec = records[dataStartIdx + r];
                    quoted[r] = c < rec.size() && rec[c].quoted;
                }
                const QVector<int> unreadable = writeTextsToDateTimeColumn(
                        col, startRowOffset, texts, quoted, decimalComma);
                if (warnings && !unreadable.isEmpty()) {
                    QStringList examples;
                    for (int k = 0; k < unreadable.size() && k < 5; ++k) {
                        const int r = unreadable.at(k);
                        examples << QStringLiteral("row %1 '%2'")
                                            .arg(startRowOffset + r)
                                            .arg(texts.at(r).trimmed());
                    }
                    warnings->append(
                            QStringLiteral("Column '%1': %2 value(s) could not be read as a "
                                           "date/time and are empty cells (%3%4).")
                                    .arg(t->colLabel(dstCol))
                                    .arg(unreadable.size())
                                    .arg(examples.join(QStringLiteral(", ")))
                                    .arg(unreadable.size() > 5 ? QStringLiteral(", ...")
                                                               : QString()));
                }
            } else if (dataRowCount > 0) {
                // text: as typed; month, day: through the column's input filter
                col->asStringColumn()->replaceTexts(startRowOffset, texts);
            }
        } else {
            bool useText = !convertNumeric;
            bool anyQuoted = false;
            bool anyFiniteNumber = false;
            QVector<qreal> parsedNums(dataRowCount, std::numeric_limits<double>::quiet_NaN());
            QVector<int> invalidRows;

            // Column type as the GUI import decides it (AsciiTableImportFilter::
            // importLabTextData): missing tokens are empty cells; a few stray non-numeric cells
            // ("OVER", a repeated header line) are invalid cells of a numeric column when at
            // least 5 cells and 90 % of the non-missing cells are numbers; otherwise text.
            if (convertNumeric) {
                int validNumCount = 0;
                int strayNonNumCount = 0;
                for (int r = 0; r < dataRowCount; ++r) {
                    const QVector<ParsedCsvCell> &rec = records[dataStartIdx + r];
                    const QString valStr = (c < rec.size()) ? rec[c].value : QString();
                    const bool wasQuoted = (c < rec.size()) ? rec[c].quoted : false;
                    if (wasQuoted)
                        anyQuoted = true;
                    const QString trimmed = valStr.trimmed();
                    if (trimmed.isEmpty()) {
                        invalidRows.append(r);
                        continue;
                    }
                    if (isImportMissingNumericCell(trimmed, wasQuoted)) {
                        invalidRows.append(r);
                        continue;
                    }
                    double numVal = 0.0;
                    if (parseImportNumber(trimmed, decimalComma, numVal)) {
                        parsedNums[r] = numVal;
                        anyFiniteNumber = true;
                        ++validNumCount;
                    } else {
                        ++strayNonNumCount;
                        const int maxRemainingNum = validNumCount + (dataRowCount - r - 1);
                        if (maxRemainingNum < 5
                            || qint64(maxRemainingNum) < qint64(strayNonNumCount) * 9) {
                            useText = true;
                            break;
                        }
                        invalidRows.append(r);
                    }
                }
                if (!useText) {
                    const bool enoughForNumeric = (strayNonNumCount == 0)
                            || (validNumCount >= 5
                                && qint64(validNumCount) * 10
                                        >= qint64(validNumCount + strayNonNumCount) * 9);
                    if (!enoughForNumeric || (anyQuoted && !anyFiniteNumber))
                        useText = true;
                }
            }

            if (useText) {
                if (col->columnMode() != SciDAVis::ColumnMode::Text)
                    col->setColumnMode(SciDAVis::ColumnMode::Text);
                if (dataRowCount > 0) {
                    QStringList texts;
                    texts.reserve(dataRowCount);
                    for (int r = 0; r < dataRowCount; ++r) {
                        const QVector<ParsedCsvCell> &rec = records[dataStartIdx + r];
                        texts.append((c < rec.size()) ? rec[c].value : QString());
                    }
                    col->replaceTexts(startRowOffset, texts);
                } else if (startRowOffset == 0) {
                    col->setInvalid(Interval<int>(0, t->numRows() - 1), true);
                }
            } else {
                if (col->columnMode() != SciDAVis::ColumnMode::Numeric)
                    col->setColumnMode(SciDAVis::ColumnMode::Numeric);
                applyDefaultNumericFormat6g(t, dstCol);
                if (dataRowCount > 0) {
                    if (startRowOffset == 0 && dataRowCount == t->numRows())
                        col->clearValidity();
                    else
                        col->setInvalid(Interval<int>(startRowOffset,
                                                      startRowOffset + dataRowCount - 1),
                                        false);
                    col->replaceValues(startRowOffset, parsedNums);
                    int k = 0;
                    while (k < invalidRows.size()) {
                        const int startR = startRowOffset + invalidRows[k];
                        int endR = startR;
                        while (k + 1 < invalidRows.size()
                               && (startRowOffset + invalidRows[k + 1]) == endR + 1) {
                            endR = startRowOffset + invalidRows[++k];
                        }
                        col->setInvalid(Interval<int>(startR, endR), true);
                        ++k;
                    }
                } else if (startRowOffset == 0) {
                    col->setInvalid(Interval<int>(0, t->numRows() - 1), true);
                }
            }
        }

        if (hasHeader && !headerRow.isEmpty() && startRowOffset == 0) {
            QString hdrName = (c < headerRow.size()) ? headerRow[c].value.trimmed() : QString();
            if (hdrName.isEmpty())
                hdrName = QString::number(dstCol + 1);

            QString splitShort, splitLong, splitUnits;
            AsciiTableImportFilter::splitHeaderNameAndUnits(hdrName, splitShort, splitLong,
                                                            splitUnits);
            QString rowUnit =
                    (c < unitsRowCells.size()) ? unitsRowCells[c].value.trimmed() : QString();
            if (rowUnit.size() >= 2
                && ((rowUnit.startsWith(QLatin1Char('(')) && rowUnit.endsWith(QLatin1Char(')')))
                    || (rowUnit.startsWith(QLatin1Char('['))
                        && rowUnit.endsWith(QLatin1Char(']'))))) {
                rowUnit = rowUnit.mid(1, rowUnit.size() - 2).trimmed();
            }

            if (!splitUnits.isEmpty()) {
                QString candShort = !splitShort.isEmpty() ? splitShort : hdrName;
                if (t->colNames().contains(candShort)) {
                    for (int sfx = 2; sfx < 1000; ++sfx) {
                        const QString alt = QStringLiteral("%1_%2").arg(candShort).arg(sfx);
                        if (!t->colNames().contains(alt)) {
                            candShort = alt;
                            break;
                        }
                    }
                }
                t->setColName(dstCol, candShort);
                t->setColLongName(dstCol, !splitLong.isEmpty() ? splitLong : candShort);
                t->setColUnits(dstCol, !rowUnit.isEmpty() ? rowUnit : splitUnits);
            } else {
                QString candName = hdrName;
                if (t->colNames().contains(candName)) {
                    for (int sfx = 2; sfx < 1000; ++sfx) {
                        const QString alt = QStringLiteral("%1_%2").arg(candName).arg(sfx);
                        if (!t->colNames().contains(alt)) {
                            candName = alt;
                            break;
                        }
                    }
                }
                t->setColName(dstCol, candName);
                if (!rowUnit.isEmpty()) {
                    t->setColUnits(dstCol, rowUnit);
                    if (t->colLongName(dstCol).trimmed().isEmpty())
                        t->setColLongName(dstCol, hdrName);
                }
            }
        }
        if (withComments && startRowOffset == 0) {
            const QString cmtStr = (c < commentRow.size()) ? commentRow[c].value : QString();
            t->setColComment(dstCol, cmtStr);
        }
        if (assignDefaultPlotDesignations)
            t->setColPlotDesignation(dstCol, (dstCol == 0) ? SciDAVis::X : SciDAVis::Y);
    }
    t->propagateRelabeledColumnNames(oldHeaderNames, namesBeforeImport, startColOffset);
    // The data were written while the header columns had their temporary names, so the
    // automatic recalculation of dependent formula columns (col("y")) could not resolve them:
    // run it again now that the names are back (inside the caller's undo macro, if any).
    if (relabelHeader)
        t->handleColumnChange(startColOffset, maxCols);
}

//! New worksheet holding the columns of `src` (a table read by AsciiTableImportFilter), named
//! `reqName` or labelled with the file path, with the columns' names, units, comments and plot
//! designations.
Table *createTableFromImportedColumns(ApplicationWindow *app, future::Table *src,
                                      const QString &reqName, const QString &absPath,
                                      const QString &tmpNamePrefix)
{
    const int nRows = std::max(1, src->rowCount());
    const int nCols = src->columnCount();
    if (nCols > 4096 || nRows > 5000000 || qint64(nCols) * qint64(nRows) > 25000000LL) {
        throwInvalidParams(QStringLiteral("Imported table dimensions (%1x%2) exceed limits.")
                                   .arg(nRows)
                                   .arg(nCols));
    }
    Table *t = createUniqueTableHeadless(app, nRows, nCols, reqName);
    if (reqName.trimmed().isEmpty() || reqName.trimmed() == t->name())
        t->setWindowLabel(absPath);
    // columns re-pointed through temporary names: references keep their names, except names
    // that are gone afterwards (propagateRelabeledColumnNames below)
    const QStringList oldNames = t->colNamesInRange(0, nCols);
    const QStringList namesBeforeImport = t->colNames();
    Table::RenamePropagationBlocker renameBlocker(t);
    for (int c = 0; c < nCols; ++c)
        t->setColName(c, tmpNamePrefix + QString::number(c) + QStringLiteral("__"));
    for (int c = 0; c < nCols; ++c) {
        Column *srcCol = src->column(c);
        Column *dstCol = t->column(c);
        if (!srcCol || !dstCol)
            continue;
        dstCol->setColumnMode(srcCol->columnMode());
        dstCol->copy(srcCol);
        t->setColName(c,
                      srcCol->name().trimmed().isEmpty() ? QString::number(c + 1) : srcCol->name());
        t->setColLongName(c, srcCol->longName());
        t->setColUnits(c, srcCol->units());
        t->setColComment(c, srcCol->comment());
        t->setColPlotDesignation(c, srcCol->plotDesignation());
        applyDefaultNumericFormat6g(t, c);
    }
    t->propagateRelabeledColumnNames(oldNames, namesBeforeImport);
    return t;
}

// ============================================================================
// Next-Gen AI-Agent Autonomy Engines:
//   1. app.batch ($prev / $steps[k] token substitution & atomic rollback)
//   2. graph.auto_layout (5-stage deterministic self-healing layout solver)
//   3. graph.to_python / generatePythonScriptForWindow (reproducible codegen)
// ============================================================================

QJsonValue extractBatchPathValue(const QJsonValue &root, const QString &rawPath, int stepIdx,
                                 const QString &fullToken)
{
    QJsonValue cur = root;
    QString rem = rawPath.trimmed();
    while (!rem.isEmpty()) {
        if (rem.startsWith(QLatin1Char('.')))
            rem.remove(0, 1);
        if (rem.isEmpty())
            break;
        if (rem.startsWith(QLatin1Char('['))) {
            const int closeBr = rem.indexOf(QLatin1Char(']'));
            if (closeBr <= 1) {
                throwInvalidParams(
                        QStringLiteral("Step %1 invalid array index syntax in token '%2'.")
                                .arg(stepIdx)
                                .arg(fullToken));
            }
            const QString inside = rem.mid(1, closeBr - 1).trimmed();
            bool okIdx = false;
            const int arrIdx = inside.toInt(&okIdx);
            if (okIdx && cur.isArray()) {
                const QJsonArray arr = cur.toArray();
                if (arrIdx < 0 || arrIdx >= arr.size()) {
                    throwInvalidParams(
                            QStringLiteral("Step %1 token '%2' index %3 out of bounds (size=%4).")
                                    .arg(stepIdx)
                                    .arg(fullToken)
                                    .arg(arrIdx)
                                    .arg(arr.size()));
                }
                cur = arr.at(arrIdx);
            } else if (cur.isObject()) {
                QString key = inside;
                if (key.size() >= 2
                    && ((key.startsWith(QLatin1Char('\'')) && key.endsWith(QLatin1Char('\'')))
                        || (key.startsWith(QLatin1Char('"')) && key.endsWith(QLatin1Char('"'))))) {
                    key = key.mid(1, key.size() - 2);
                }
                const QJsonObject obj = cur.toObject();
                if (!obj.contains(key)) {
                    throwInvalidParams(
                            QStringLiteral("Step %1 token '%2' missing key '%3'.")
                                    .arg(stepIdx)
                                    .arg(fullToken, key));
                }
                cur = obj.value(key);
            } else {
                throwInvalidParams(
                        QStringLiteral("Step %1 token '%2' cannot index non-container value.")
                                .arg(stepIdx)
                                .arg(fullToken));
            }
            rem = rem.mid(closeBr + 1);
        } else {
            int nextSep = -1;
            for (int i = 0; i < rem.size(); ++i) {
                if (rem[i] == QLatin1Char('.') || rem[i] == QLatin1Char('[')) {
                    nextSep = i;
                    break;
                }
            }
            const QString key = (nextSep >= 0) ? rem.left(nextSep) : rem;
            if (!cur.isObject()) {
                throwInvalidParams(
                        QStringLiteral("Step %1 token '%2' cannot access field '%3' on non-object.")
                                .arg(stepIdx)
                                .arg(fullToken, key));
            }
            const QJsonObject obj = cur.toObject();
            if (!obj.contains(key)) {
                throwInvalidParams(
                        QStringLiteral("Step %1 token '%2' missing field '%3'.")
                                .arg(stepIdx)
                                .arg(fullToken, key));
            }
            cur = obj.value(key);
            rem = (nextSep >= 0) ? rem.mid(nextSep) : QString();
        }
    }
    return cur;
}

QJsonValue resolveSingleBatchToken(const QString &token, const QVector<QJsonObject> &results,
                                   int curStepIdx)
{
    static const QRegularExpression rxExact(
            QStringLiteral("^\\$(?:(prev)|steps\\[(\\d+)\\])((?:\\.[A-Za-z0-9_\\-]+|\\[[^\\]]+\\])*)$"));
    const QRegularExpressionMatch m = rxExact.match(token);
    if (!m.hasMatch())
        return QJsonValue(token);
    int refIdx = -1;
    if (!m.captured(1).isEmpty()) {
        if (curStepIdx <= 0) {
            throwInvalidParams(
                    QStringLiteral("Step 0 cannot reference '$prev' in token '%1'.").arg(token));
        }
        refIdx = curStepIdx - 1;
    } else {
        refIdx = m.captured(2).toInt();
        if (refIdx < 0 || refIdx >= curStepIdx || refIdx >= results.size()) {
            throwInvalidParams(
                    QStringLiteral("Step %1 references invalid or future '$steps[%2]' in '%3'.")
                            .arg(curStepIdx)
                            .arg(refIdx)
                            .arg(token));
        }
    }
    const QString path = m.captured(3);
    if (path.isEmpty())
        return results[refIdx];
    return extractBatchPathValue(results[refIdx], path, curStepIdx, token);
}

QString batchScalarToString(const QJsonValue &v)
{
    if (v.isString())
        return v.toString();
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (std::isfinite(d) && std::floor(d) == d && std::abs(d) < 9.0e15)
            return QString::number(static_cast<qint64>(d));
        return QString::number(d, 'g', 15);
    }
    if (v.isNull() || v.isUndefined())
        return QString();
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return QString();
}

QJsonValue resolveBatchValueTokens(const QJsonValue &val, const QVector<QJsonObject> &results,
                                   int curStepIdx)
{
    if (val.isString()) {
        const QString s = val.toString();
        if (!s.contains(QLatin1Char('$')))
            return val;
        static const QRegularExpression rxFull(
                QStringLiteral("^\\$(?:prev|steps\\[\\d+\\])(?:\\.[A-Za-z0-9_\\-]+|\\[[^\\]]+\\])*$"));
        if (rxFull.match(s).hasMatch())
            return resolveSingleBatchToken(s, results, curStepIdx);

        static const QRegularExpression rxInline(
                QStringLiteral("\\$(?:prev|steps\\[\\d+\\])(?:\\.[A-Za-z0-9_\\-]+|\\[[^\\]]+\\])*"));
        QString out;
        int lastEnd = 0;
        auto it = rxInline.globalMatch(s);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            out.append(s.mid(lastEnd, m.capturedStart(0) - lastEnd));
            const QJsonValue resolved =
                    resolveSingleBatchToken(m.captured(0), results, curStepIdx);
            out.append(batchScalarToString(resolved));
            lastEnd = m.capturedEnd(0);
        }
        out.append(s.mid(lastEnd));
        return QJsonValue(out);
    }
    if (val.isArray()) {
        const QJsonArray inArr = val.toArray();
        QJsonArray outArr;
        for (const QJsonValue &item : inArr)
            outArr.append(resolveBatchValueTokens(item, results, curStepIdx));
        return outArr;
    }
    if (val.isObject()) {
        const QJsonObject inObj = val.toObject();
        QJsonObject outObj;
        for (auto it = inObj.begin(); it != inObj.end(); ++it)
            outObj.insert(it.key(), resolveBatchValueTokens(it.value(), results, curStepIdx));
        return outObj;
    }
    return val;
}

void confirmUnsavedProjectBeforeReset(PythonAutomationServer *server, ApplicationWindow *d_app,
                                      const QJsonObject &params)
{
    if (!d_app || d_app->saved)
        return;

    const bool hasExplicitForce = params.contains(QStringLiteral("force"))
            && !params.value(QStringLiteral("force")).isUndefined()
            && !params.value(QStringLiteral("force")).isNull();
    if (hasExplicitForce) {
        const bool forceVal = optionalJsonBool(params, QStringLiteral("force"), false);
        if (forceVal)
            return;
        throw RpcDispatchException(
                -32002,
                QStringLiteral("Current project '%1' has unsaved changes. Save first or pass "
                               "force=True.")
                        .arg(d_app->projectname));
    }

    const bool nonInteractive = (server ? server->callerWasNonInteractive()
                                        : ApplicationWindow::isNonInteractiveContext(d_app))
            || d_app->isOleEmbeddedMode();
    if (nonInteractive)
        return;

    // Never spin a nested QMessageBox::exec() event loop inside RPC dispatch: immediately
    // reject unsaved interactive resets unless force=True was explicitly specified.
    throw RpcDispatchException(
            -32002,
            QStringLiteral("Current project '%1' has unsaved changes. Save first or pass "
                           "force=True.")
                    .arg(d_app->projectname));
}

} // namespace

namespace rpc_internal {

QJsonObject serializeTableInfo(Table *t)
{
    QJsonObject info;
    info[QStringLiteral("name")] = t->name();
    info[QStringLiteral("label")] = t->windowLabel();
    info[QStringLiteral("type")] = QStringLiteral("Table");
    info[QStringLiteral("rows")] = t->numRows();
    info[QStringLiteral("cols")] = t->numCols();
    info[QStringLiteral("auto_recalc")] = t->autoRecalcFormulas();
    QJsonArray colsArr;
    for (int c = 0; c < t->numCols(); ++c)
        colsArr.append(serializeColumnMeta(t, c));
    info[QStringLiteral("columns")] = colsArr;
    return info;
}

QJsonObject serializeMatrixInfo(Matrix *m)
{
    QJsonObject info;
    info[QStringLiteral("name")] = m->name();
    info[QStringLiteral("label")] = m->windowLabel();
    info[QStringLiteral("type")] = QStringLiteral("Matrix");
    info[QStringLiteral("rows")] = m->numRows();
    info[QStringLiteral("cols")] = m->numCols();
    info[QStringLiteral("x_min")] = safeJsonDouble(m->xStart());
    info[QStringLiteral("x_max")] = safeJsonDouble(m->xEnd());
    info[QStringLiteral("y_min")] = safeJsonDouble(m->yStart());
    info[QStringLiteral("y_max")] = safeJsonDouble(m->yEnd());
    double zMin = 0.0, zMax = 0.0;
    m->range(&zMin, &zMax);
    info[QStringLiteral("z_min")] = safeJsonDouble(zMin);
    info[QStringLiteral("z_max")] = safeJsonDouble(zMax);
    info[QStringLiteral("formula")] = m->formula();
    return info;
}

bool dispatchAppTableMatrixMethod(PythonAutomationServer *server, ApplicationWindow *d_app,
                                  bool &focusHudFallback, QString &workspaceModeFallback,
                                  QString &lastUndoDomain, QString &lastUndoGraph,
                                  const QString &method, const QJsonObject &params,
                                  QJsonObject &outResult)
{
    // ------------------------------------------------------------------------
    // 1. App / Project / GUI / Action Escape Hatch
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("rpc.auth") || method == QStringLiteral("auth")) {
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("authenticated")] = true;
        return true;
    }

    if (method == QStringLiteral("ping")) {
        outResult[QStringLiteral("pong")] = true;
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("endpoint")] = server->endpointString();
        outResult[QStringLiteral("pid")] = QCoreApplication::applicationPid();
        return true;
    }

    if (method == QStringLiteral("app.supported_methods")) {
        const QStringList mList = PythonAutomationServer::supportedMethods();
        outResult[QStringLiteral("count")] = mList.size();
        outResult[QStringLiteral("methods")] = toJsonArray(mList);
        return true;
    }

    if (method == QStringLiteral("app.describe_api")) {
        const QString reqMethod = optionalJsonString(params, QStringLiteral("method")).trimmed();
        const QString reqDomain =
                optionalJsonString(params, QStringLiteral("domain")).trimmed().toLower();
        QStringList allDomains;
        for (int i = 0; i < kApiCatalogCount; ++i) {
            const QString dom = QString::fromUtf8(kApiCatalogSpecs[i].domain);
            if (!allDomains.contains(dom))
                allDomains.append(dom);
        }
        if (!reqMethod.isEmpty()) {
            for (int i = 0; i < kApiCatalogCount; ++i) {
                if (reqMethod.compare(QString::fromUtf8(kApiCatalogSpecs[i].method),
                                      Qt::CaseInsensitive)
                    == 0) {
                    const QJsonObject schema = serializeCatalogRow(kApiCatalogSpecs[i]);
                    outResult = schema;
                    outResult[QStringLiteral("ok")] = true;
                    outResult[QStringLiteral("schema")] = schema;
                    outResult[QStringLiteral("count")] = 1;
                    QJsonArray singleArr;
                    singleArr.append(schema);
                    outResult[QStringLiteral("methods")] = singleArr;
                    return true;
                }
            }
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Unknown RPC method '%1'.").arg(reqMethod),
                    PythonAutomationServer::supportedMethods());
        }

        if (!reqDomain.isEmpty() && !allDomains.contains(reqDomain)) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Unknown RPC domain '%1'.").arg(reqDomain), allDomains);
        }

        QJsonArray methodsArr;
        for (int i = 0; i < kApiCatalogCount; ++i) {
            const QString dom = QString::fromUtf8(kApiCatalogSpecs[i].domain);
            if (!reqDomain.isEmpty() && dom != reqDomain)
                continue;
            methodsArr.append(serializeCatalogRow(kApiCatalogSpecs[i]));
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("count")] = methodsArr.size();
        outResult[QStringLiteral("domains")] = toJsonArray(allDomains);
        if (!reqDomain.isEmpty())
            outResult[QStringLiteral("domain")] = reqDomain;
        outResult[QStringLiteral("methods")] = methodsArr;
        return true;
    }

    if (!d_app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is not initialized."));

    if (method == QStringLiteral("app.info")) {
        outResult[QStringLiteral("name")] = QStringLiteral("ToriGraph");
        outResult[QStringLiteral("version")] = extractCurrentVersionClean();
        outResult[QStringLiteral("version_full")] = SciDAVis::versionString();
        outResult[QStringLiteral("pid")] = QCoreApplication::applicationPid();
        outResult[QStringLiteral("language")] = d_app->appLanguage;
        outResult[QStringLiteral("endpoint")] = server->endpointString();
        outResult[QStringLiteral("tcp_port")] = int(server->tcpPort());
        outResult[QStringLiteral("local_pipe")] = server->localPipeName();
        outResult[QStringLiteral("project_name")] = d_app->projectname;
        outResult[QStringLiteral("window_count")] = d_app->windowsList().size();
        outResult[QStringLiteral("method_count")] =
                PythonAutomationServer::supportedMethods().size();
        return true;
    }

    if (method == QStringLiteral("app.new_project")) {
        confirmUnsavedProjectBeforeReset(server, d_app, params);
        resetProjectInPlaceHeadless(d_app);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("window_count")] = d_app->windowsList().size();
        return true;
    }

    if (method == QStringLiteral("app.batch")) {
        const QJsonValue cmdsVal = params.contains(QStringLiteral("commands"))
                ? params.value(QStringLiteral("commands"))
                : params.value(QStringLiteral("steps"));
        if (!cmdsVal.isArray()) {
            throwInvalidParams(
                    QStringLiteral("app.batch requires a 'commands' (or 'steps') array of {\"method\", \"params\"} dicts."));
        }
        const QJsonArray cmdsArr = cmdsVal.toArray();
        const int totalSteps = cmdsArr.size();
        if (totalSteps > 5000) {
            throwInvalidParams(
                    QStringLiteral("app.batch supports at most 5,000 commands per batch (got %1).")
                            .arg(totalSteps));
        }
        const bool stopOnError = optionalJsonBool(params, QStringLiteral("stop_on_error"), true);
        const bool atomic = optionalJsonBool(params, QStringLiteral("atomic"), true);
        const bool deferReplot = optionalJsonBool(params, QStringLiteral("defer_replot"), true);

        BatchExecutionContext &bctx = rpcBatchContext();
        if (bctx.recursionDepth >= 2) {
            throwInvalidParams(
                    QStringLiteral("Nested app.batch recursion depth exceeded (maximum depth 2)."));
        }
        if (totalSteps == 0) {
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("completed")] = 0;
            outResult[QStringLiteral("total")] = 0;
            outResult[QStringLiteral("rolled_back")] = false;
            outResult[QStringLiteral("undo_history_cleared")] = false;
            outResult[QStringLiteral("elapsed_ms")] = 0.0;
            outResult[QStringLiteral("results")] = QJsonArray();
            outResult[QStringLiteral("failed_index")] = QJsonValue::Null;
            outResult[QStringLiteral("error")] = QJsonValue::Null;
            return true;
        }

        static bool s_atomicSnapshotHeld = false;
        const bool ownsAtomicRollback = atomic && !s_atomicSnapshotHeld;
        bool graphOnlyFastSnapshot = ownsAtomicRollback && (bctx.recursionDepth == 0);
        for (int i = 0; i < totalSteps; ++i) {
            if (!cmdsArr[i].isObject()) {
                throwInvalidParams(
                        QStringLiteral("app.batch step %1 must be a JSON object.").arg(i));
            }
            const QJsonObject stepObj = cmdsArr[i].toObject();
            const QString sm = optionalJsonString(stepObj, QStringLiteral("method")).trimmed();
            if (sm.isEmpty()) {
                throwInvalidParams(
                        QStringLiteral("app.batch step %1 missing non-empty 'method' string.")
                                .arg(i));
            }
            if (stepObj.contains(QStringLiteral("params"))
                && !stepObj.value(QStringLiteral("params")).isNull()
                && !stepObj.value(QStringLiteral("params")).isObject()) {
                throwInvalidParams(
                        QStringLiteral("app.batch step %1 'params' must be a JSON object.").arg(i));
            }
            if (!sm.startsWith(QStringLiteral("graph."))
                || sm == QStringLiteral("graph.merge_graphs")
                || sm == QStringLiteral("graph.batch_clone")
                || sm == QStringLiteral("graph.drop_columns")
                // Masks live in the source table columns, which the MultiLayer XML snapshot
                // does not cover; these need the full workspace snapshot to roll back.
                || sm == QStringLiteral("graph.mask_region")
                || sm == QStringLiteral("graph.clear_masks")
                // Graph3D and density2d matrix counts are outside MultiLayer XML snapshots.
                || sm == QStringLiteral("graph.set_3d_options")
                || sm == QStringLiteral("graph.rebuild_density2d")) {
                graphOnlyFastSnapshot = false;
            }
        }

        // Capture pre-batch atomic snapshot (two-tier: fast MultiLayer XML vs full .sciprj bytes)
        struct PreBatchGadgetSnap
        {
            enum Kind { KindRoi, KindVerticalCursor, KindDeltaCursor, KindImageProfile };
            QString mlName;
            int layerIndex = 0;
            Kind kind = KindRoi;
            RoiGadgetTool::GadgetMode roiMode = RoiGadgetTool::RegionStats;
            RoiGadgetTool::QuickFitModel quickFitModel = RoiGadgetTool::FitLinear;
            RoiGadgetTool::IntegrateBaseline integrateBaseline = RoiGadgetTool::BaseYZero;
            bool detectNegativePeaks = false;
            int savGolWindow = 5;
            QRectF roiRect;
            QString targetCurve;
            bool syncAllLayers = true;
            DeltaCursorTool::SnapMode deltaSnapMode = DeltaCursorTool::SnapInterpolateCurve;
            bool horizCrosshair = true;
            int activeCursor = 1;
            int specIndex = 0;
            double x1 = 0.0;
            double y1 = 0.0;
            double x2 = 0.0;
            double y2 = 0.0;
        };
        auto collectAllMultiLayers = [&]() -> QList<MultiLayer *> {
            QList<MultiLayer *> result;
            QSet<MultiLayer *> seen;
            auto addFromList = [&](const QList<MyWidget *> &wins) {
                for (MyWidget *w : wins) {
                    if (w && w->inherits("MultiLayer")) {
                        auto *ml = static_cast<MultiLayer *>(w);
                        if (!seen.contains(ml)) {
                            seen.insert(ml);
                            result.append(ml);
                        }
                    }
                }
            };
            if (Folder *rootF = d_app->projectFolder()) {
                QList<Folder *> stack;
                stack.append(rootF);
                while (!stack.isEmpty()) {
                    Folder *f = stack.takeLast();
                    if (!f)
                        continue;
                    addFromList(f->windowsList());
                    for (Folder *sub : f->folders()) {
                        if (sub)
                            stack.append(sub);
                    }
                }
            }
            addFromList(d_app->windowsList());
            return result;
        };
        QVector<PreBatchGadgetSnap> preBatchGadgets;
        auto restorePreBatchGadgets = [&]() {
            const QList<MultiLayer *> curMls = collectAllMultiLayers();
            QHash<QString, MultiLayer *> mlByName;
            for (MultiLayer *ml : curMls) {
                if (!ml)
                    continue;
                mlByName.insert(ml->name(), ml);
                for (Graph *g : ml->layersList()) {
                    if (g && g->activeTool())
                        RoiGadgetTool::clearGadget(g);
                }
            }
            for (const PreBatchGadgetSnap &snap : preBatchGadgets) {
                MultiLayer *ml = mlByName.value(snap.mlName, nullptr);
                if (!ml)
                    continue;
                const QList<Graph *> layers = ml->layersList();
                if (snap.layerIndex < 0 || snap.layerIndex >= layers.size())
                    continue;
                Graph *g = layers.at(snap.layerIndex);
                if (!g)
                    continue;
                switch (snap.kind) {
                case PreBatchGadgetSnap::KindRoi:
                    if (auto *roi = RoiGadgetTool::attachGadget(d_app, g, snap.roiMode)) {
                        roi->setQuickFitModel(snap.quickFitModel);
                        roi->setIntegrateBaseline(snap.integrateBaseline);
                        roi->setDetectNegativePeaks(snap.detectNegativePeaks);
                        roi->setSavGolWindow(snap.savGolWindow);
                        if (!snap.targetCurve.isEmpty())
                            roi->setTargetCurve(snap.targetCurve);
                        if (!snap.roiRect.isNull())
                            roi->setRoiRect(snap.roiRect);
                        roi->recalculate();
                    }
                    break;
                case PreBatchGadgetSnap::KindVerticalCursor:
                    if (auto *vc = VerticalCursorTool::attachCursor(d_app, g, snap.syncAllLayers))
                        vc->setCursorX(snap.x1);
                    break;
                case PreBatchGadgetSnap::KindDeltaCursor:
                    if (auto *dc = DeltaCursorTool::attach(d_app, g, snap.deltaSnapMode)) {
                        dc->setHorizontalCrosshairEnabled(snap.horizCrosshair);
                        dc->setActiveCursor(snap.activeCursor);
                        if (!snap.targetCurve.isEmpty())
                            dc->setTargetCurve(snap.targetCurve);
                        if (snap.deltaSnapMode == DeltaCursorTool::FreeScreenCrosshair)
                            dc->setCursorPoints2D(snap.x1, snap.y1, snap.x2, snap.y2);
                        else
                            dc->setCursorPositions(snap.x1, snap.x2);
                    }
                    break;
                case PreBatchGadgetSnap::KindImageProfile:
                    if (auto *ip = ImageProfileGadgetTool::attach(g, d_app)) {
                        if (snap.specIndex >= 0)
                            ip->setTargetSpectrogramIndex(snap.specIndex);
                        ip->setCrosshairPosition(snap.x1, snap.y1);
                    }
                    break;
                }
            }
        };

        QHash<MultiLayer *, QString> fastMlSnapshots;
        QHash<MultiLayer *, int> fastMlUndoIdx;
        QByteArray fullWorkspaceSnapshot;
        bool needFullWorkspaceRestore = false;
        bool hadUntouchedSingleTable = false;
        QString untouchedTblName;
        int untouchedTblRows = 30;
        int untouchedTblCols = 2;
        const QString savedProjName = d_app->projectname;
        const QStringList savedRecent = d_app->recentProjects;
        // loadProject() of the staged snapshot renames the root folder after the temporary file
        const QString savedRootFolderName =
                d_app->projectFolder() ? d_app->projectFolder()->name() : QString();
        const QString savedWindowTitle = d_app->windowTitle();
        QString savedActiveWinName;
        if (QWidget *actW = d_app->d_workspace.activeSubWindow()) {
            if (auto *mw = qobject_cast<MyWidget *>(actW))
                savedActiveWinName = mw->name();
        }
        if (ownsAtomicRollback) {
            for (MultiLayer *ml : collectAllMultiLayers()) {
                if (!ml)
                    continue;
                const QList<Graph *> layers = ml->layersList();
                for (int li = 0; li < layers.size(); ++li) {
                    Graph *g = layers.at(li);
                    PlotToolInterface *tool = g ? g->activeTool() : nullptr;
                    if (!tool)
                        continue;
                    PreBatchGadgetSnap snap;
                    snap.mlName = ml->name();
                    snap.layerIndex = li;
                    if (auto *roi = dynamic_cast<RoiGadgetTool *>(tool)) {
                        snap.kind = PreBatchGadgetSnap::KindRoi;
                        snap.roiMode = roi->gadgetMode();
                        snap.quickFitModel = roi->quickFitModel();
                        snap.integrateBaseline = roi->integrateBaseline();
                        snap.detectNegativePeaks = roi->detectNegativePeaks();
                        snap.savGolWindow = roi->savGolWindow();
                        snap.roiRect = roi->roiRect();
                        snap.targetCurve = roi->targetCurveTitle();
                        preBatchGadgets.append(snap);
                    } else if (auto *vc = dynamic_cast<VerticalCursorTool *>(tool)) {
                        snap.kind = PreBatchGadgetSnap::KindVerticalCursor;
                        snap.syncAllLayers = vc->syncAllLayers();
                        snap.x1 = vc->cursorX();
                        preBatchGadgets.append(snap);
                    } else if (auto *dc = dynamic_cast<DeltaCursorTool *>(tool)) {
                        snap.kind = PreBatchGadgetSnap::KindDeltaCursor;
                        snap.deltaSnapMode = dc->snapMode();
                        snap.horizCrosshair = dc->horizontalCrosshairEnabled();
                        snap.activeCursor = dc->activeCursor();
                        snap.targetCurve = dc->targetCurveTitle();
                        const auto rd = dc->currentReadout();
                        snap.x1 = rd.x1;
                        snap.y1 = rd.y1;
                        snap.x2 = rd.x2;
                        snap.y2 = rd.y2;
                        preBatchGadgets.append(snap);
                    } else if (auto *ip = dynamic_cast<ImageProfileGadgetTool *>(tool)) {
                        snap.kind = PreBatchGadgetSnap::KindImageProfile;
                        snap.specIndex = ip->targetSpectrogramIndex();
                        const QPointF pos = ip->crosshairPosition();
                        snap.x1 = pos.x();
                        snap.y1 = pos.y();
                        preBatchGadgets.append(snap);
                    }
                }
            }
            if (graphOnlyFastSnapshot) {
                for (MyWidget *w : d_app->windowsList()) {
                    if (w && w->inherits("MultiLayer")) {
                        auto *ml = static_cast<MultiLayer *>(w);
                        fastMlSnapshots.insert(ml, ml->saveToString(QString()));
                        fastMlUndoIdx.insert(ml, ml->undoStack() ? ml->undoStack()->index() : -1);
                    }
                }
            } else {
                const QList<MyWidget *> curWins = d_app->windowsList();
                const bool hasSubfolders =
                        d_app->projectFolder() && !d_app->projectFolder()->folders().isEmpty();
                if (curWins.isEmpty() && !hasSubfolders) {
                    // Empty workspace fast path: 0-byte snapshot, resetProjectInPlaceHeadless rolls back in <1ms
                } else if (!hasSubfolders && curWins.size() == 1 && curWins.first()
                           && curWins.first()->inherits("Table")) {
                    auto *t0 = static_cast<Table *>(curWins.first());
                    WorkbookManager *wbm = WorkbookManager::existing(d_app);
                    const bool inWorkbook = (wbm != nullptr) && !wbm->bookOf(t0).isEmpty();
                    bool untouched = (!inWorkbook && t0->numRows() <= 100 && t0->numCols() <= 16
                                      && (!t0->d_future_table || !t0->d_future_table->undoStack()
                                          || t0->d_future_table->undoStack()->index() == 0));
                    for (int c = 0; untouched && c < t0->numCols(); ++c) {
                        Column *col = t0->column(c);
                        const SciDAVis::PlotDesignation expectedPd =
                                (c == 0) ? SciDAVis::X : SciDAVis::Y;
                        if (!col || t0->colLabel(c) != QString::number(c + 1)
                            || col->columnMode() != SciDAVis::ColumnMode::Numeric
                            || col->plotDesignation() != expectedPd
                            || !col->comment().trimmed().isEmpty()
                            || !col->formula(0).trimmed().isEmpty()
                            || !col->longName().trimmed().isEmpty()
                            || !col->units().trimmed().isEmpty()) {
                            untouched = false;
                            break;
                        }
                        for (int r = 0; r < t0->numRows(); ++r) {
                            if (!col->isInvalid(r) || col->isMasked(r)) {
                                untouched = false;
                                break;
                            }
                        }
                    }
                    if (untouched) {
                        hadUntouchedSingleTable = true;
                        untouchedTblName = t0->name();
                        untouchedTblRows = t0->numRows();
                        untouchedTblCols = t0->numCols();
                    } else {
                        sanitizeAllProjectFunctionCurves(d_app);
                        fullWorkspaceSnapshot = d_app->saveProjectToByteArray();
                        needFullWorkspaceRestore = true;
                    }
                } else {
                    sanitizeAllProjectFunctionCurves(d_app);
                    fullWorkspaceSnapshot = d_app->saveProjectToByteArray();
                    needFullWorkspaceRestore = true;
                }
            }
        }

        QElapsedTimer timer;
        timer.start();

        struct ScopedBatchActivation
        {
            BatchExecutionContext &ctx;
            bool &atomicSnapshotHeld;
            bool prevActive;
            bool prevDefer;
            bool prevAtomicSnapshotHeld;
            ScopedBatchActivation(BatchExecutionContext &c, bool defer, bool &atomicHeld,
                                  bool ownsAtomic)
                : ctx(c)
                , atomicSnapshotHeld(atomicHeld)
                , prevActive(c.active)
                , prevDefer(c.deferReplot)
                , prevAtomicSnapshotHeld(atomicHeld)
            {
                ctx.active = true;
                ctx.deferReplot = defer;
                if (ownsAtomic)
                    atomicSnapshotHeld = true;
                ++ctx.recursionDepth;
            }
            ~ScopedBatchActivation()
            {
                --ctx.recursionDepth;
                ctx.active = prevActive;
                ctx.deferReplot = prevDefer;
                atomicSnapshotHeld = prevAtomicSnapshotHeld;
                if (ctx.recursionDepth <= 0) {
                    ctx.recursionDepth = 0;
                    for (const auto &pr : ctx.preBatchGraphSnaps) {
                        if (MultiLayer *ml = pr.first.data())
                            ml->endSuppressUndo();
                    }
                    ctx.preBatchGraphSnaps.clear();
                    ctx.dirtyGraphs.clear();
                    ctx.dirtyMultiLayers.clear();
                    ctx.snapshottedMultiLayers.clear();
                }
            }
        } batchScope(bctx, deferReplot, s_atomicSnapshotHeld, ownsAtomicRollback);

        QVector<QJsonObject> stepResultsVec;
        stepResultsVec.reserve(totalSteps);
        QJsonArray resultsJsonArr;
        int completedCount = 0;
        int failedIndex = -1;
        QJsonObject firstErrorObj;

        for (int i = 0; i < totalSteps; ++i) {
            const QJsonObject stepObj = cmdsArr[i].toObject();
            const QString stepMethod = stepObj.value(QStringLiteral("method")).toString().trimmed();
            const QJsonObject rawStepParams =
                    stepObj.value(QStringLiteral("params")).toObject();
            try {
                const QJsonObject resolvedParams =
                        resolveBatchValueTokens(rawStepParams, stepResultsVec, i).toObject();
                validateParamsAgainstCatalogSchema(stepMethod, resolvedParams);
                QJsonObject stepRes;
                if (!dispatchCoreAndGraphMethod(server, d_app, focusHudFallback,
                                                workspaceModeFallback, lastUndoDomain,
                                                lastUndoGraph, stepMethod, resolvedParams, stepRes)
                    && !dispatchAnalysisAndScienceMethod(d_app, stepMethod, resolvedParams,
                                                         stepRes)) {
                    QJsonObject errData;
                    errData[QStringLiteral("available")] =
                            toJsonArray(PythonAutomationServer::supportedMethods());
                    throw RpcDispatchException(
                            -32601, QStringLiteral("Method not found: %1").arg(stepMethod),
                            errData);
                }
                if (stepMethod == QStringLiteral("app.batch")
                    && !stepRes.value(QStringLiteral("ok")).toBool(true)) {
                    const QJsonObject innerErr = stepRes.value(QStringLiteral("error")).toObject();
                    QJsonObject errEntry = innerErr;
                    if (!errEntry.contains(QStringLiteral("code")))
                        errEntry[QStringLiteral("code")] = -32603;
                    if (errEntry.value(QStringLiteral("message")).toString().trimmed().isEmpty()) {
                        errEntry[QStringLiteral("message")] =
                                QStringLiteral("Nested app.batch failed.");
                    }
                    if (!innerErr.isEmpty()
                        && (innerErr.contains(QStringLiteral("step_index"))
                            || innerErr.contains(QStringLiteral("step_method")))) {
                        errEntry[QStringLiteral("nested_error")] = innerErr;
                    }
                    errEntry[QStringLiteral("step_index")] = i;
                    errEntry[QStringLiteral("step_method")] = stepMethod;

                    if (failedIndex < 0) {
                        failedIndex = i;
                        firstErrorObj = errEntry;
                    }
                    stepResultsVec.append(stepRes);
                    resultsJsonArr.append(stepRes);
                    if (stopOnError || atomic)
                        break;
                    continue;
                }
                if (!stepRes.contains(QStringLiteral("ok")))
                    stepRes[QStringLiteral("ok")] = true;
                stepResultsVec.append(stepRes);
                resultsJsonArr.append(stepRes);
                ++completedCount;
            } catch (const RpcDispatchException &ex) {
                QJsonObject errEntry;
                errEntry[QStringLiteral("code")] = ex.code;
                errEntry[QStringLiteral("message")] = QString::fromUtf8(ex.what());
                errEntry[QStringLiteral("step_index")] = i;
                errEntry[QStringLiteral("step_method")] = stepMethod;
                if (!ex.data.isUndefined() && !ex.data.isNull())
                    errEntry[QStringLiteral("data")] = ex.data;

                if (failedIndex < 0) {
                    failedIndex = i;
                    firstErrorObj = errEntry;
                }
                QJsonObject failStepRes;
                failStepRes[QStringLiteral("ok")] = false;
                failStepRes[QStringLiteral("error")] = errEntry;
                stepResultsVec.append(failStepRes);
                resultsJsonArr.append(failStepRes);

                if (stopOnError || atomic)
                    break;
            } catch (const std::exception &ex) {
                QJsonObject errEntry;
                errEntry[QStringLiteral("code")] = -32603;
                errEntry[QStringLiteral("message")] = QString::fromUtf8(ex.what());
                errEntry[QStringLiteral("step_index")] = i;
                errEntry[QStringLiteral("step_method")] = stepMethod;
                if (failedIndex < 0) {
                    failedIndex = i;
                    firstErrorObj = errEntry;
                }
                QJsonObject failStepRes;
                failStepRes[QStringLiteral("ok")] = false;
                failStepRes[QStringLiteral("error")] = errEntry;
                stepResultsVec.append(failStepRes);
                resultsJsonArr.append(failStepRes);
                if (stopOnError || atomic)
                    break;
            }
        }

        bool rolledBack = false;
        bool undoHistoryCleared = false;
        QString rollbackError;
        if (failedIndex >= 0 && ownsAtomicRollback) {
            if (bctx.recursionDepth == 1) {
                const auto openSnaps = bctx.preBatchGraphSnaps;
                bctx.preBatchGraphSnaps.clear();
                bctx.active = false;
                bctx.deferReplot = false;
                bctx.dirtyGraphs.clear();
                bctx.dirtyMultiLayers.clear();
                bctx.snapshottedMultiLayers.clear();
                for (const auto &pr : openSnaps) {
                    if (MultiLayer *ml = pr.first.data())
                        ml->endSuppressUndo();
                }
            }

            if (graphOnlyFastSnapshot) {
                for (auto it = fastMlSnapshots.begin(); it != fastMlSnapshots.end(); ++it) {
                    MultiLayer *ml = it.key();
                    if (!ml || !d_app->windowsList().contains(ml))
                        continue;
                    if (ml->undoStack() && fastMlUndoIdx.contains(ml)) {
                        const int wantIdx = fastMlUndoIdx.value(ml);
                        if (wantIdx >= 0 && ml->undoStack()->index() > wantIdx)
                            ml->undoStack()->setIndex(wantIdx);
                    }
                    ml->restoreSnapshotInPlace(d_app, it.value());
                }
                restorePreBatchGadgets();
                rolledBack = true;
            } else {
                bool restoreOk = true;
                // set where the workspace is actually cleared (and rebuilt); a rollback that
                // fails before that (snapshot not staged) leaves the windows and their Undo
                // history untouched
                bool workspaceReset = false;
                if (hadUntouchedSingleTable) {
                    resetProjectInPlaceHeadless(d_app);
                    workspaceReset = true;
                    createUniqueTableHeadless(d_app, untouchedTblRows, untouchedTblCols,
                                              untouchedTblName);
                } else if (needFullWorkspaceRestore) {
                    // Stage the snapshot on disk first and only clear the workspace once it is
                    // known to be readable back; otherwise the batch's partial changes are left in
                    // place (rolled_back:false) instead of silently losing the whole project.
                    QTemporaryFile tmpProj(
                            QDir::temp().filePath(QStringLiteral("torigraph_batch_rb_XXXXXX.sciprj")));
                    bool staged = false;
                    QString tmpPath;
                    if (fullWorkspaceSnapshot.isEmpty()) {
                        rollbackError = QStringLiteral(
                                "atomic rollback failed: the pre-batch workspace snapshot is empty; "
                                "the workspace was left in its post-failure state.");
                    } else if (!tmpProj.open()) {
                        rollbackError =
                                QStringLiteral("atomic rollback failed: cannot create temporary "
                                               "snapshot file (%1); the workspace was left in its "
                                               "post-failure state.")
                                        .arg(tmpProj.errorString());
                    } else {
                        tmpPath = tmpProj.fileName();
                        staged = tmpProj.write(fullWorkspaceSnapshot) == fullWorkspaceSnapshot.size()
                                && tmpProj.flush();
                        tmpProj.close();
                        if (!staged) {
                            rollbackError =
                                    QStringLiteral("atomic rollback failed: cannot write temporary "
                                                   "snapshot file '%1'; the workspace was left in "
                                                   "its post-failure state.")
                                            .arg(QDir::toNativeSeparators(tmpPath));
                        }
                    }
                    if (staged) {
                        resetProjectInPlaceHeadless(d_app);
                        workspaceReset = true;
                        if (!d_app->loadProject(tmpPath)) {
                            rollbackError =
                                    QStringLiteral("atomic rollback failed: the pre-batch snapshot "
                                                   "could not be reloaded; the workspace may be "
                                                   "incomplete.");
                        }
                        QFile::remove(tmpPath);
                    }
                    restoreOk = rollbackError.isEmpty();
                } else {
                    // Empty pre-batch workspace: clearing it is the full rollback.
                    resetProjectInPlaceHeadless(d_app);
                    workspaceReset = true;
                }
                d_app->recentProjects = savedRecent;
                d_app->projectname = savedProjName;
                d_app->updateRecentProjectsList();
                if (Folder *rootF = d_app->projectFolder()) {
                    if (!savedRootFolderName.isEmpty() && rootF->name() != savedRootFolderName) {
                        rootF->setName(savedRootFolderName);
                        if (rootF->folderListItem())
                            rootF->folderListItem()->setText(0, savedRootFolderName);
                    }
                }
                d_app->setWindowTitle(savedWindowTitle);
                // The workspace was rebuilt from the snapshot: the Undo history of the windows
                // that existed before the batch cannot be carried over.
                undoHistoryCleared = workspaceReset;
                if (workspaceReset && bctx.recursionDepth > 1) {
                    const auto openSnaps = bctx.preBatchGraphSnaps;
                    bctx.preBatchGraphSnaps.clear();
                    bctx.dirtyGraphs.clear();
                    bctx.dirtyMultiLayers.clear();
                    bctx.snapshottedMultiLayers.clear();
                    for (const auto &pr : openSnaps) {
                        if (MultiLayer *ml = pr.first.data())
                            ml->endSuppressUndo();
                    }
                }
                if (!savedActiveWinName.isEmpty()) {
                    if (MyWidget *aw = findWindowByName(d_app, savedActiveWinName))
                        d_app->activateSubWindow(aw);
                }
                if (restoreOk)
                    restorePreBatchGadgets();
                rolledBack = restoreOk;
            }
        } else if (bctx.recursionDepth == 1) {
            // Flush coalesced font enforcement, layout arrangement, replots, and final Undo snapshots once at commit
            const QVector<QPointer<Graph>> graphsToFlush = bctx.dirtyGraphs;
            const QVector<QPointer<MultiLayer>> mlsToFlush = bctx.dirtyMultiLayers;
            const auto snapsToCommit = bctx.preBatchGraphSnaps;
            bctx.preBatchGraphSnaps.clear();
            bctx.active = false;
            bctx.deferReplot = false;
            bctx.dirtyGraphs.clear();
            bctx.dirtyMultiLayers.clear();
            bctx.snapshottedMultiLayers.clear();
            for (const QPointer<Graph> &gp : graphsToFlush) {
                if (Graph *g = gp.data()) {
                    enforceArialGraphFonts(g);
                    g->replot();
                }
            }
            for (const QPointer<MultiLayer> &mlp : mlsToFlush) {
                if (MultiLayer *ml = mlp.data()) {
                    if (ml->pageViewMode() || ml->pageUnit() != MultiLayer::UnitPx)
                        ml->fitLayersToPage();
                }
            }
            for (const auto &pr : snapsToCommit) {
                if (MultiLayer *ml = pr.first.data()) {
                    ml->endSuppressUndo();
                    if (d_app->windowsList().contains(ml))
                        ml->pushUndoSnapshot(QStringLiteral("Batch Operations"), pr.second);
                }
            }
        }

        const double elapsedMs = double(timer.nsecsElapsed()) / 1.0e6;
        outResult[QStringLiteral("ok")] = (failedIndex < 0);
        outResult[QStringLiteral("completed")] = completedCount;
        outResult[QStringLiteral("total")] = totalSteps;
        outResult[QStringLiteral("rolled_back")] = rolledBack;
        outResult[QStringLiteral("undo_history_cleared")] = undoHistoryCleared;
        outResult[QStringLiteral("elapsed_ms")] = safeJsonDouble(elapsedMs);
        outResult[QStringLiteral("results")] = resultsJsonArr;
        outResult[QStringLiteral("failed_index")] =
                (failedIndex >= 0) ? QJsonValue(failedIndex) : QJsonValue::Null;
        if (!rollbackError.isEmpty()) {
            firstErrorObj[QStringLiteral("rollback_error")] = rollbackError;
            outResult[QStringLiteral("rollback_error")] = rollbackError;
        }
        outResult[QStringLiteral("error")] =
                (failedIndex >= 0) ? QJsonValue(firstErrorObj) : QJsonValue::Null;
        return true;
    }

    if (method == QStringLiteral("app.open_project")) {
        const QString path = optionalJsonString(params, QStringLiteral("path")).trimmed();
        const QFileInfo pfi(path);
        if (path.isEmpty() || !pfi.exists() || !pfi.isFile())
            throwInvalidParams(QStringLiteral("Project file does not exist: %1").arg(path));
        const QString absPath = pfi.absoluteFilePath();
        const bool append = optionalJsonBool(params, QStringLiteral("append"), false);

        auto hasAnyExt = [&absPath](std::initializer_list<const char *> exts) -> bool {
            for (const char *ext : exts) {
                if (absPath.endsWith(QLatin1String(ext), Qt::CaseInsensitive))
                    return true;
            }
            return false;
        };

        if (hasAnyExt({ ".csv", ".tsv", ".tab", ".txt", ".dat", ".asc", ".ras", ".xy", ".xye",
                        ".uxd", ".chi", ".mpt", ".dta" })) {
            throwInvalidParams(QStringLiteral(
                    "'%1' is a tabular/text data file, not a project; use app.import_csv() instead.")
                                       .arg(path));
        }
        if (hasAnyExt({ ".xlsx", ".xlsm", ".xls", ".ods" })) {
            throwInvalidParams(QStringLiteral(
                    "'%1' is a spreadsheet file, not a project; use app.import_xlsx() instead.")
                                       .arg(path));
        }

        // Pre-flight readability & non-empty check before touching the active workspace.
        QFile hdrFile(absPath);
        if (!hdrFile.open(QIODevice::ReadOnly)) {
            throwRpcRuntime(QStringLiteral("Failed to open project file '%1': %2")
                                    .arg(path, hdrFile.errorString()));
        }
        const QByteArray rawHead = hdrFile.read(256);
        hdrFile.close();
        if (rawHead.isEmpty())
            throwRpcRuntime(QStringLiteral("Project file is empty: %1").arg(path));

        auto captureRollbackSnapshot = [d_app](QByteArray *outSnap, QString *outName,
                                              bool *outSaved, QStringList *outRecent,
                                              QString *outRootFolder) {
            *outName = d_app->projectname;
            *outSaved = d_app->saved;
            *outRecent = d_app->recentProjects;
            *outRootFolder = d_app->projectFolder() ? d_app->projectFolder()->name() : QString();
            if (!d_app->windowsList().isEmpty()) {
                sanitizeAllProjectFunctionCurves(d_app);
                *outSnap = d_app->saveProjectToByteArray();
            } else {
                outSnap->clear();
            }
        };

        const bool isOpjuExt = hasAnyExt({ ".opju" });
        if (isOpjuExt || ImportOPJU::isSupportedOpjuFile(absPath)) {
            if (!ImportOPJU::isSupportedOpjuFile(absPath)) {
                throwRpcRuntime(
                        QStringLiteral("Invalid or corrupt Origin '.opju' file: %1").arg(path));
            }
            QByteArray rollbackSnap;
            QString savedProjName;
            bool savedWasSaved = true;
            QStringList savedRecent;
            QString savedRootFolder;
            if (!append) {
                confirmUnsavedProjectBeforeReset(server, d_app, params);
                captureRollbackSnapshot(&rollbackSnap, &savedProjName, &savedWasSaved, &savedRecent,
                                        &savedRootFolder);
                resetProjectInPlaceHeadless(d_app);
            }
            const int winBefore = d_app->windowsList().size();
            QString opjuErr;
            ImportOPJU::ImportStats stats;
            if (!ImportOPJU::importFile(d_app, absPath, &opjuErr, &stats)) {
                if (!append)
                    restoreProjectSnapshotOnError(d_app, rollbackSnap, savedProjName, savedWasSaved,
                                                  savedRecent, savedRootFolder, false);
                throwRpcRuntime(opjuErr.isEmpty()
                                        ? QStringLiteral("Failed to import OPJU file: %1").arg(path)
                                        : opjuErr);
            }
            if (!append) {
                d_app->projectname = absPath;
                d_app->savedProject();
            } else if (d_app->projectname.isEmpty()
                       || d_app->projectname == QStringLiteral("untitled")) {
                d_app->projectname = absPath;
            }
            const int winAfter = d_app->windowsList().size();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("appended")] = append;
            outResult[QStringLiteral("path")] = absPath;
            outResult[QStringLiteral("imported_windows")] = std::max(0, winAfter - winBefore);
            outResult[QStringLiteral("tables")] = stats.tablesImported;
            outResult[QStringLiteral("matrices")] = stats.matricesImported;
            outResult[QStringLiteral("graphs")] = stats.graphsImported;
            outResult[QStringLiteral("notes")] = stats.notesImported;
            outResult[QStringLiteral("folders")] = stats.foldersImported;
            outResult[QStringLiteral("window_count")] = winAfter;
            return true;
        }

        const bool isLegacyOpjExt = hasAnyExt({ ".opj", ".ogm", ".ogw", ".ogg", ".org" });
        const bool isSciExt = hasAnyExt(
                { ".sciprj", ".sciprj.gz", ".sciprj~", ".sciprj.gz~", ".qti", ".qti.gz", ".qti~",
                  ".qti.gz~" });
#ifndef ORIGIN_IMPORT
        if (isLegacyOpjExt) {
            throwRpcRuntime(QStringLiteral(
                    "Origin '.opj' binary import is not enabled in this build; convert to '.opju' in Origin or export CSV/XLSX."));
        }
#endif
        if (!isSciExt && !isLegacyOpjExt) {
            throwInvalidParams(QStringLiteral(
                    "Unsupported project file extension for '%1'. Supported formats: .sciprj, .sciprj.gz, .opju, .qti.")
                                       .arg(path));
        }

        const bool hasGzipMagic = rawHead.size() >= 2
                && static_cast<unsigned char>(rawHead[0]) == 0x1F
                && static_cast<unsigned char>(rawHead[1]) == 0x8B;
        const bool hasCpyaMagic = rawHead.startsWith("CPYA");
        QByteArray headerSample = rawHead;
        if (hasGzipMagic) {
            headerSample = readGzipHeaderBytes(absPath, 256);
            if (headerSample.isEmpty()) {
                throwRpcRuntime(
                        QStringLiteral("Invalid or corrupt compressed project file '%1'.").arg(path));
            }
        }
        if (headerSample.startsWith("\xEF\xBB\xBF"))
            headerSample.remove(0, 3);
        const QByteArray trimmedHead = headerSample.trimmed();
        const bool hasSciHeader = trimmedHead.startsWith("SciDAVis")
                || trimmedHead.startsWith("ToriGraph") || trimmedHead.startsWith("QtiPlot")
                || trimmedHead.startsWith("<?xml") || trimmedHead.startsWith("<scidavis");
        if (isLegacyOpjExt ? !hasCpyaMagic : !hasSciHeader) {
            throwRpcRuntime(
                    QStringLiteral("Invalid or corrupt project header in '%1'.").arg(path));
        }

        if (append) {
            Folder *curF = d_app->currentFolder() ? d_app->currentFolder() : d_app->projectFolder();
            const int subBefore = curF ? curF->folders().size() : 0;
            const int winBefore = d_app->windowsList().size();
            d_app->setProperty("torigraph_last_append_ok", QVariant());
            d_app->appendProject(absPath);
            const QVariant appendOkProp = d_app->property("torigraph_last_append_ok");
            const int subAfter = curF ? curF->folders().size() : 0;
            const int winAfter = d_app->windowsList().size();
            if ((appendOkProp.isValid() && !appendOkProp.toBool())
                || (subAfter <= subBefore && winAfter <= winBefore)) {
                if (appendOkProp.isValid() && !appendOkProp.toBool() && curF
                    && curF->folders().size() > subBefore) {
                    const bool saveConfirmFld = d_app->confirmCloseFolder;
                    d_app->confirmCloseFolder = false;
                    d_app->deleteFolder(curF->folders().last());
                    d_app->confirmCloseFolder = saveConfirmFld;
                }
                throwRpcRuntime(QStringLiteral("Failed to append project: %1").arg(path));
            }
            if (d_app->projectname.isEmpty() || d_app->projectname == QStringLiteral("untitled"))
                d_app->projectname = absPath;
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("appended")] = true;
            outResult[QStringLiteral("path")] = absPath;
            if (curF && !curF->folders().isEmpty() && subAfter > subBefore)
                outResult[QStringLiteral("appended_folder")] = curF->folders().last()->name();
            outResult[QStringLiteral("window_count")] = winAfter;
            return true;
        }

        confirmUnsavedProjectBeforeReset(server, d_app, params);
        QByteArray rollbackSnap;
        QString savedProjName;
        bool savedWasSaved = true;
        QStringList savedRecent;
        QString savedRootFolder;
        captureRollbackSnapshot(&rollbackSnap, &savedProjName, &savedWasSaved, &savedRecent,
                                &savedRootFolder);
        resetProjectInPlaceHeadless(d_app);
        const bool ok = d_app->loadProject(absPath);
        if (!ok) {
            restoreProjectSnapshotOnError(d_app, rollbackSnap, savedProjName, savedWasSaved,
                                          savedRecent, savedRootFolder, true);
            throwRpcRuntime(QStringLiteral("Failed to load project: %1").arg(path));
        }
        d_app->projectname = absPath;
        d_app->savedProject();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("path")] = d_app->projectname;
        outResult[QStringLiteral("window_count")] = d_app->windowsList().size();
        return true;
    }

    if (method == QStringLiteral("app.save_project")) {
        const bool snapshot = optionalJsonBool(params, QStringLiteral("snapshot"), false);
        QString path = optionalJsonString(params, QStringLiteral("path")).trimmed();
        if (path.isEmpty() && !snapshot)
            path = d_app->projectname;
        if (path.isEmpty() || path == QStringLiteral("untitled"))
            throwInvalidParams(QStringLiteral("A valid output 'path' (.sciprj) is required."));
        if (QFileInfo(path).exists() && QFileInfo(path).isDir())
            throwInvalidParams(
                    QStringLiteral("Output project 'path' is an existing directory: %1").arg(path));
        if (!path.endsWith(QStringLiteral(".sciprj"), Qt::CaseInsensitive)
            && !path.endsWith(QStringLiteral(".sciprj.gz"), Qt::CaseInsensitive)) {
            path += QStringLiteral(".sciprj");
        }
        const QFileInfo fi(path);
        if (fi.exists() && fi.isDir())
            throwInvalidParams(
                    QStringLiteral("Output project 'path' is an existing directory: %1").arg(path));
        if (!fi.dir().exists())
            QDir().mkpath(fi.dir().absolutePath());
        const QString absPath = fi.absoluteFilePath();
        const bool compress = absPath.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive)
                || optionalJsonBool(params, QStringLiteral("compress"), false);
        const QString finalPath = (compress && !absPath.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive))
                ? (absPath + QStringLiteral(".gz"))
                : absPath;
        if (QFileInfo(finalPath).exists() && QFileInfo(finalPath).isDir())
            throwInvalidParams(
                    QStringLiteral("Output project 'path' is an existing directory: %1").arg(finalPath));

        if (!snapshot)
            sanitizeAllProjectFunctionCurves(d_app);

        // Same safe path as File > Save: write "<name>.new", commit it, then swap it in and
        // keep the previous version as "<name>~" (".gz" goes through a scratch file). The
        // existing project file is never opened for writing, so a failed save leaves it intact.
        if (!d_app->writeProjectFile(d_app->projectFolder(), finalPath)
            || !QFileInfo::exists(finalPath) || QFileInfo(finalPath).size() <= 0) {
            throwRpcRuntime(
                    QStringLiteral("Failed to write project file '%1'. If a previous version "
                                   "existed it has not been changed.")
                            .arg(finalPath));
        }
        if (!snapshot) {
            d_app->projectname = finalPath;
            // Keep the GUI undo history (only its clean state moves), as File > Save does.
            d_app->markProjectSaved();
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("path")] = finalPath;
        outResult[QStringLiteral("snapshot")] = snapshot;
        outResult[QStringLiteral("compressed")] =
                finalPath.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive);
        outResult[QStringLiteral("bytes")] = QFileInfo(finalPath).size();
        return true;
    }

    if (method == QStringLiteral("app.import_csv")) {
        const QString path = optionalJsonString(params, QStringLiteral("path")).trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path) || !QFileInfo(path).isFile())
            throwInvalidParams(QStringLiteral("CSV/ASCII file does not exist: %1").arg(path));

        const QString absPath = QFileInfo(path).absoluteFilePath();
        QFile inFile(absPath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            throwRpcRuntime(QStringLiteral("Failed to open CSV/ASCII file '%1': %2")
                                    .arg(absPath, inFile.errorString()));
        }
        const QByteArray rawBytes = inFile.readAll();
        inFile.close();

        const QString encodingParam = params.contains(QStringLiteral("encoding"))
                ? optionalJsonString(params, QStringLiteral("encoding"), QStringLiteral("auto")).trimmed()
                : QStringLiteral("auto");
        {
            const QString encLow = encodingParam.toLower();
            static const QSet<QString> kValidEncodings = {
                QStringLiteral(""),           QStringLiteral("auto"),
                QStringLiteral("utf-8"),      QStringLiteral("utf8"),
                QStringLiteral("utf-16"),     QStringLiteral("utf16"),
                QStringLiteral("utf-16le"),   QStringLiteral("utf-16be"),
                QStringLiteral("shift_jis"),  QStringLiteral("shift-jis"),
                QStringLiteral("sjis"),       QStringLiteral("cp932"),
                QStringLiteral("ms932"),      QStringLiteral("euc-jp"),
                QStringLiteral("euc_jp"),     QStringLiteral("iso-2022-jp"),
                QStringLiteral("latin1"),     QStringLiteral("iso-8859-1"),
                QStringLiteral("cp1252"),     QStringLiteral("windows-1252"),
                QStringLiteral("ascii"),      QStringLiteral("gbk"),
                QStringLiteral("gb18030"),    QStringLiteral("big5")
            };
            if (!kValidEncodings.contains(encLow)
                && QTextCodec::codecForName(encodingParam.toLatin1()) == nullptr) {
                throwInvalidParams(
                        QStringLiteral("Unsupported text encoding '%1' (expected 'auto'|'utf-8'|'utf-16'|'shift_jis'|'cp932'|'euc-jp'|'latin1'|'iso-8859-1').")
                                .arg(encodingParam));
            }
        }
        const QString encodingHint =
                (encodingParam.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0)
                ? QString()
                : encodingParam;
        const int unitsRowHint = parseUnitsRowHintParam(params);
        const QString reqImportName = optionalJsonString(params, QStringLiteral("name"));

        // Reject Sun Raster magic and non-UTF-16 NUL-byte binary payloads with -32602 (0 tables created)
        if (rawBytes.size() >= 4) {
            const unsigned char b0 = static_cast<unsigned char>(rawBytes[0]);
            const unsigned char b1 = static_cast<unsigned char>(rawBytes[1]);
            const unsigned char b2 = static_cast<unsigned char>(rawBytes[2]);
            const unsigned char b3 = static_cast<unsigned char>(rawBytes[3]);
            if ((b0 == 0x59 && b1 == 0xA6 && b2 == 0x6A && b3 == 0x95)
                || (b0 == 0x95 && b1 == 0x6A && b2 == 0xA6 && b3 == 0x59)) {
                throwInvalidParams(
                        QStringLiteral("Binary Sun Raster image '%1' cannot be imported as ASCII/CSV.")
                                .arg(absPath));
            }
        }
        if (rawBytes.contains('\0')) {
            const bool hasUtf16Bom = rawBytes.size() >= 2
                    && ((static_cast<unsigned char>(rawBytes[0]) == 0xFF
                         && static_cast<unsigned char>(rawBytes[1]) == 0xFE)
                        || (static_cast<unsigned char>(rawBytes[0]) == 0xFE
                            && static_cast<unsigned char>(rawBytes[1]) == 0xFF));
            const bool explicitUtf16 =
                    encodingHint.startsWith(QStringLiteral("utf-16"), Qt::CaseInsensitive)
                    || encodingHint.startsWith(QStringLiteral("utf16"), Qt::CaseInsensitive);
            bool bomlessUtf16 = false;
            if (!hasUtf16Bom && !explicitUtf16 && encodingHint.isEmpty() && rawBytes.size() >= 4
                && (rawBytes.size() % 2 == 0)) {
                const auto *uptr = reinterpret_cast<const unsigned char *>(rawBytes.constData());
                const int samplePairs = std::min(rawBytes.size() / 2, 256);
                int evenZeros = 0, oddZeros = 0;
                for (int p = 0; p < samplePairs; ++p) {
                    if (uptr[2 * p] == 0)
                        ++evenZeros;
                    if (uptr[2 * p + 1] == 0)
                        ++oddZeros;
                }
                if ((oddZeros > samplePairs / 2 && evenZeros == 0)
                    || (evenZeros > samplePairs / 2 && oddZeros == 0)) {
                    bomlessUtf16 = true;
                }
            }
            if (!hasUtf16Bom && !explicitUtf16 && !bomlessUtf16) {
                throwInvalidParams(
                        QStringLiteral("Binary file '%1' contains NUL bytes and cannot be imported as ASCII/CSV.")
                                .arg(absPath));
            }
        }

        // Rigaku .ras diffraction scan files
        const bool isRasFile = absPath.endsWith(QStringLiteral(".ras"), Qt::CaseInsensitive)
                || rawBytes.contains("*RAS_DATA_START") || rawBytes.contains("*RAS_INT_START");
        if (isRasFile) {
            std::unique_ptr<future::Table> rasFt(AsciiTableImportFilter::importLabTextData(
                    rawBytes, QStringLiteral("RasImportTmp"), absPath,
                    optionalJsonString(params, QStringLiteral("separator")), encodingHint,
                    unitsRowHint));
            if (!rasFt || rasFt->columnCount() <= 0)
                throwInvalidParams(
                        QStringLiteral("Failed to parse Rigaku .ras data in '%1'.").arg(absPath));
            const int nRows = std::max(1, rasFt->rowCount());
            const int nCols = rasFt->columnCount();
            Table *t = createUniqueTableHeadless(d_app, nRows, nCols, reqImportName);
            if (reqImportName.trimmed().isEmpty() || reqImportName.trimmed() == t->name())
                t->setWindowLabel(absPath);
            // columns re-pointed through temporary names: references keep their names, except
            // names that are gone afterwards (propagateRelabeledColumnNames below)
            const QStringList oldNames = t->colNamesInRange(0, nCols);
            const QStringList namesBeforeImport = t->colNames();
            Table::RenamePropagationBlocker renameBlocker(t);
            for (int c = 0; c < nCols; ++c)
                t->setColName(c, QStringLiteral("__ras_col_%1__").arg(c));
            for (int c = 0; c < nCols; ++c) {
                Column *srcCol = rasFt->column(c);
                Column *dstCol = t->column(c);
                if (!srcCol || !dstCol)
                    continue;
                dstCol->setColumnMode(srcCol->columnMode());
                dstCol->copy(srcCol);
                t->setColName(c, srcCol->name().trimmed().isEmpty() ? QString::number(c + 1)
                                                                    : srcCol->name());
                t->setColLongName(c, srcCol->longName());
                t->setColUnits(c, srcCol->units());
                t->setColComment(c, srcCol->comment());
                t->setColPlotDesignation(c, (c == 0) ? SciDAVis::X : SciDAVis::Y);
                applyDefaultNumericFormat6g(t, c);
            }
            t->propagateRelabeledColumnNames(oldNames, namesBeforeImport);
            outResult = serializeTableInfo(t);
            outResult[QStringLiteral("ok")] = true;
            return true;
        }

        // VASP DOSCAR / EIGENVAL scientific files
        const QString baseUpper = QFileInfo(absPath).fileName().toUpper();
        if (baseUpper.startsWith(QStringLiteral("DOSCAR"))
            || baseUpper.startsWith(QStringLiteral("EIGENVAL"))) {
            double fermiEV = std::numeric_limits<double>::quiet_NaN();
            std::unique_ptr<future::Table> vaspFt(AsciiTableImportFilter::importVaspTextData(
                    rawBytes, QStringLiteral("VaspImportTmp"), absPath, &fermiEV));
            if (vaspFt && vaspFt->columnCount() > 0 && vaspFt->rowCount() > 0) {
                const int nRows = std::max(1, vaspFt->rowCount());
                const int nCols = vaspFt->columnCount();
                if (nCols > 4096 || nRows > 5000000
                    || qint64(nCols) * qint64(nRows) > 25000000LL) {
                    throwInvalidParams(
                            QStringLiteral("Imported VASP table dimensions (%1x%2) exceed limits.")
                                    .arg(nRows)
                                    .arg(nCols));
                }
                Table *t = createUniqueTableHeadless(d_app, nRows, nCols, reqImportName);
                if (reqImportName.trimmed().isEmpty() || reqImportName.trimmed() == t->name())
                    t->setWindowLabel(absPath);
                // columns re-pointed through temporary names: references keep their names,
                // except names that are gone afterwards (propagateRelabeledColumnNames below)
                const QStringList oldNames = t->colNamesInRange(0, nCols);
                const QStringList namesBeforeImport = t->colNames();
                Table::RenamePropagationBlocker renameBlocker(t);
                for (int c = 0; c < nCols; ++c)
                    t->setColName(c, QStringLiteral("__vasp_col_%1__").arg(c));
                for (int c = 0; c < nCols; ++c) {
                    Column *srcCol = vaspFt->column(c);
                    Column *dstCol = t->column(c);
                    if (!srcCol || !dstCol)
                        continue;
                    dstCol->setColumnMode(srcCol->columnMode());
                    dstCol->copy(srcCol);
                    t->setColName(c, srcCol->name().trimmed().isEmpty() ? QString::number(c + 1)
                                                                        : srcCol->name());
                    t->setColLongName(c, srcCol->longName());
                    t->setColUnits(c, srcCol->units());
                    t->setColComment(c, srcCol->comment());
                    t->setColPlotDesignation(c, srcCol->plotDesignation());
                    applyDefaultNumericFormat6g(t, c);
                }
                t->propagateRelabeledColumnNames(oldNames, namesBeforeImport);
                outResult = serializeTableInfo(t);
                if (std::isfinite(fermiEV))
                    outResult[QStringLiteral("fermi_energy_ev")] = fermiEV;
                outResult[QStringLiteral("ok")] = true;
                return true;
            }
        }

        QString text = AsciiTableImportFilter::decodeLabTextAuto(rawBytes, encodingHint);
        const int ignoredLines = parseBoundedInt(params, QStringLiteral("skip_lines"), 0, 0, 1000000);

        // Synchrotron SPEC scan files (#S / #L): one column group per scan, read by the GUI
        // import's SPEC parser (the generic path would take the '#' directives for comments).
        if (ignoredLines == 0) {
            const bool specFirstRowNames = !(params.contains(QStringLiteral("header"))
                                             && !params.value(QStringLiteral("header")).isNull())
                    || optionalJsonBool(params, QStringLiteral("header"), true);
            std::unique_ptr<future::Table> specFt(AsciiTableImportFilter::importSpecTextData(
                    text, QStringLiteral("SpecImportTmp"),
                    optionalJsonBool(params, QStringLiteral("numeric"), true), specFirstRowNames));
            if (specFt && specFt->columnCount() > 0) {
                Table *t = createTableFromImportedColumns(d_app, specFt.get(), reqImportName,
                                                          absPath, QStringLiteral("__spec_col_"));
                outResult = serializeTableInfo(t);
                outResult[QStringLiteral("ok")] = true;
                return true;
            }
        }

        // Metadata lines above the header / numeric block (tensile tester and DSC exports with
        // "key,value" lines, "; Instrument ..." headers, spectrometer .txt): read as the GUI
        // import (File > Open, drag & drop) reads such a file, with its preamble detection.
        // Only for the default layout options; explicit separator / header / skip_lines /
        // numeric / comments settings keep the import below.
        {
            const QString sepParam = optionalJsonString(params, QStringLiteral("separator"));
            const bool defaultLayout = ignoredLines == 0
                    && (sepParam.isEmpty()
                        || sepParam.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0)
                    && !(params.contains(QStringLiteral("header"))
                         && !params.value(QStringLiteral("header")).isNull())
                    && optionalJsonBool(params, QStringLiteral("numeric"), true)
                    && !optionalJsonBool(params, QStringLiteral("with_comments"), false)
                    && !optionalJsonBool(params, QStringLiteral("comments"), false);
            if (defaultLayout
                && AsciiTableImportFilter::hasDataPreamble(rawBytes, absPath, encodingHint)) {
                std::unique_ptr<future::Table> preFt(AsciiTableImportFilter::importLabTextData(
                        rawBytes, QStringLiteral("PreambleImportTmp"), absPath, QString(),
                        encodingHint, unitsRowHint));
                if (preFt && preFt->columnCount() > 0 && preFt->rowCount() > 0) {
                    Table *t = createTableFromImportedColumns(d_app, preFt.get(), reqImportName,
                                                              absPath, QStringLiteral("__pre_col_"));
                    outResult = serializeTableInfo(t);
                    outResult[QStringLiteral("ok")] = true;
                    return true;
                }
            }
        }

        int pos = 0;
        {
            const int len0 = text.size();
            for (int skipped = 0; skipped < ignoredLines && pos < len0; ++skipped) {
                while (pos < len0 && text[pos] != QLatin1Char('\r') && text[pos] != QLatin1Char('\n'))
                    ++pos;
                if (pos < len0 && text[pos] == QLatin1Char('\r')) {
                    ++pos;
                    if (pos < len0 && text[pos] == QLatin1Char('\n'))
                        ++pos;
                } else if (pos < len0 && text[pos] == QLatin1Char('\n')) {
                    ++pos;
                }
            }
        }

        bool autoWhitespaceSep = false;
        QString sep = optionalJsonString(params, QStringLiteral("separator"));
        if (sep.isEmpty() || sep.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0) {
            sep = autoDetectCsvSeparator(text, pos, absPath, &autoWhitespaceSep);
        } else if (sep.compare(QStringLiteral("space"), Qt::CaseInsensitive) == 0
                   || sep.compare(QStringLiteral("whitespace"), Qt::CaseInsensitive) == 0
                   || sep == QStringLiteral(" ")) {
            sep = QStringLiteral(" ");
            autoWhitespaceSep = true;
        }

        if (autoWhitespaceSep && sep == QStringLiteral(" ")) {
            // Collapse runs of spaces/tabs on every line so multi-space .xy/.dat aligns cleanly
            // (CR-only, LF and CRLF line ends). ';' / '!' lines are comments of such files and are
            // dropped; '#' lines are kept for the '#'-row rule of parseDelimitedRecordsRfc4180
            // ("#1 520" specimen rows stay data).
            const QString tail = text.mid(pos);
            const QStringList rawLines =
                    tail.split(QRegularExpression(QStringLiteral("\\r\\n|\\r|\\n")));
            QStringList normLines;
            normLines.reserve(rawLines.size());
            for (const QString &ln : rawLines) {
                const QString tr = ln.trimmed();
                if (tr.isEmpty() || tr.startsWith(QLatin1Char(';'))
                    || tr.startsWith(QLatin1Char('!')))
                    continue;
                normLines.append(tr.simplified());
            }
            text = normLines.join(QLatin1Char('\n'));
            pos = 0;
        }

        const bool stripSpaces = optionalJsonBool(params, QStringLiteral("strip_spaces"), true);
        const bool simplifySpaces = optionalJsonBool(params, QStringLiteral("simplify_spaces"), false);
        const bool convertNumeric = optionalJsonBool(params, QStringLiteral("numeric"), true);
        // ';'-separated with decimal commas ("0,5;1,5"), auto-detected or with separator=";"
        const bool decimalComma = detectImportDecimalComma(text, pos, sep, convertNumeric);
        const bool withComments = params.contains(QStringLiteral("with_comments"))
                ? optionalJsonBool(params, QStringLiteral("with_comments"), false)
                : optionalJsonBool(params, QStringLiteral("comments"), false);

        int maxCols = 1;
        const QVector<QVector<ParsedCsvCell>> records = parseDelimitedRecordsRfc4180(
                text, pos, sep, stripSpaces, simplifySpaces, maxCols, true, decimalComma);

        bool hasHeader = true;
        if (params.contains(QStringLiteral("header")) && !params.value(QStringLiteral("header")).isNull()) {
            hasHeader = optionalJsonBool(params, QStringLiteral("header"), true);
        } else if (!records.isEmpty()) {
            // Auto-detect headerless numeric files (.xy, .xye, .dat, etc.): if every non-empty cell in row 0 is numeric, row 0 is data!
            int numTokens = 0;
            bool allRow0Numeric = true;
            for (const ParsedCsvCell &cell : records[0]) {
                const QString tok = cell.value.trimmed();
                // missing values ("NaN 1") are data cells like the empty ones
                if (tok.isEmpty() || (!cell.quoted && isImportMissingNumericCell(tok, false)))
                    continue;
                double dv = 0.0;
                if (!cell.quoted && parseImportNumber(tok, decimalComma, dv)) {
                    ++numTokens;
                } else {
                    allRow0Numeric = false;
                    break;
                }
            }
            if (allRow0Numeric && numTokens >= 1)
                hasHeader = false;
        }

        if (maxCols > 4096 || records.size() > 5000000
            || qint64(maxCols) * qint64(std::max(1, records.size())) > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("Imported table dimensions (%1x%2) exceed limits (max 5,000,000 rows, 4,096 cols, 25,000,000 cells).")
                            .arg(records.size())
                            .arg(maxCols));
        }

        Table *t = createUniqueTableHeadless(d_app, 1, maxCols, reqImportName);
        if (reqImportName.trimmed().isEmpty() || reqImportName.trimmed() == t->name())
            t->setWindowLabel(absPath);

        populateDelimitedRecordsIntoTable(t, records, maxCols, hasHeader, withComments,
                                          unitsRowHint, convertNumeric, 0, 0, true, decimalComma);

        if (!hasHeader
            && (absPath.endsWith(QStringLiteral(".xy"), Qt::CaseInsensitive)
                || absPath.endsWith(QStringLiteral(".xye"), Qt::CaseInsensitive))) {
            if (t->numCols() >= 1) {
                t->setColName(0, QStringLiteral("TwoTheta"));
                t->setColLongName(0, QStringLiteral("2Theta"));
                t->setColUnits(0, QStringLiteral("deg"));
                t->setColPlotDesignation(0, SciDAVis::X);
            }
            if (t->numCols() >= 2) {
                t->setColName(1, QStringLiteral("Intensity"));
                t->setColLongName(1, QStringLiteral("Intensity"));
                t->setColUnits(1, QStringLiteral("counts"));
                t->setColPlotDesignation(1, SciDAVis::Y);
            }
            if (t->numCols() >= 3) {
                t->setColName(2, QStringLiteral("Error"));
                t->setColPlotDesignation(2, SciDAVis::yErr);
            }
        }

        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("app.import_xlsx")) {
        const QString path = optionalJsonString(params, QStringLiteral("path")).trimmed();
        const QFileInfo xfi(path);
        if (path.isEmpty() || !xfi.exists() || !xfi.isFile())
            throwInvalidParams(QStringLiteral("Excel file does not exist: %1").arg(path));
        const QString sheet = optionalJsonString(params, QStringLiteral("sheet")).trimmed();
        const bool asWorkbook = optionalJsonBool(params, QStringLiteral("workbook"), true);
        QString xlsxErr;
        XlsxImport::ImportResult res;
        if (!XlsxImport::importIntoProject(d_app, xfi.absoluteFilePath(), sheet, &xlsxErr, &res,
                                           asWorkbook))
            throwRpcRuntime(xlsxErr.isEmpty()
                                    ? QStringLiteral("Failed to import Excel file '%1'.").arg(path)
                                    : xlsxErr);
        QJsonArray tablesArr;
        for (const QString &t : res.tables)
            tablesArr.append(t);
        QJsonArray emptyArr;
        for (const QString &e : res.emptySheets)
            emptyArr.append(e);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("path")] = xfi.absoluteFilePath();
        outResult[QStringLiteral("tables")] = tablesArr;
        outResult[QStringLiteral("empty_sheets")] = emptyArr;
        outResult[QStringLiteral("summary")] = res.summary;
        outResult[QStringLiteral("workbook")] = res.workbook;
        return true;
    }
    if (method == QStringLiteral("app.import_opj")) {
        const QString path = optionalJsonString(params, QStringLiteral("path")).trimmed();
        const QFileInfo opjFi(path);
        if (path.isEmpty() || !opjFi.exists() || !opjFi.isFile())
            throwInvalidParams(QStringLiteral("OPJ file does not exist: %1").arg(path));
        const QString absPath = opjFi.absoluteFilePath();
        const bool isOpjuExt = absPath.endsWith(QStringLiteral(".opju"), Qt::CaseInsensitive);
        if (isOpjuExt || ImportOPJU::isSupportedOpjuFile(absPath)) {
            if (!ImportOPJU::isSupportedOpjuFile(absPath)) {
                throwRpcRuntime(
                        QStringLiteral("Invalid or corrupt Origin OPJU file '%1'.").arg(path));
            }
            const int beforeCount = d_app->windowsList().size();
            QString opjuErr;
            ImportOPJU::ImportStats stats;
            if (!ImportOPJU::importFile(d_app, absPath, &opjuErr, &stats)) {
                throwRpcRuntime(
                        opjuErr.isEmpty()
                                ? QStringLiteral("Failed to import OPJU file '%1'.").arg(path)
                                : opjuErr);
            }
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("error_code")] = 0;
            outResult[QStringLiteral("imported_windows")] =
                    std::max(0, d_app->windowsList().size() - beforeCount);
            outResult[QStringLiteral("tables")] = stats.tablesImported;
            outResult[QStringLiteral("matrices")] = stats.matricesImported;
            outResult[QStringLiteral("graphs")] = stats.graphsImported;
            outResult[QStringLiteral("notes")] = stats.notesImported;
            outResult[QStringLiteral("folders")] = stats.foldersImported;
            outResult[QStringLiteral("window_count")] = d_app->windowsList().size();
            return true;
        }
#ifdef ORIGIN_IMPORT
        QFile opjCheck(absPath);
        if (!opjCheck.open(QIODevice::ReadOnly)) {
            throwRpcRuntime(QStringLiteral("Failed to open OPJ file '%1': %2")
                                    .arg(path, opjCheck.errorString()));
        }
        const QByteArray opjMagic = opjCheck.read(16);
        opjCheck.close();
        if (opjMagic.size() < 4 || !opjMagic.startsWith("CPYA")) {
            throwRpcRuntime(
                    QStringLiteral("Invalid or corrupt Origin OPJ file '%1' (missing CPYA header).")
                            .arg(path));
        }
        const int beforeCount = d_app->windowsList().size();
        const QString codec = optionalJsonString(params, QStringLiteral("codec")).trimmed();
        ImportOPJ importer(d_app, absPath, codec);
        if (importer.error() != 0) {
            throwRpcRuntime(
                    QStringLiteral("Failed to import OPJ file '%1' (error code %2).")
                            .arg(path)
                            .arg(importer.error()));
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("error_code")] = 0;
        outResult[QStringLiteral("imported_windows")] =
                std::max(0, d_app->windowsList().size() - beforeCount);
        outResult[QStringLiteral("window_count")] = d_app->windowsList().size();
        return true;
#else
        throwRpcRuntime(QStringLiteral("ORIGIN_IMPORT is not enabled in this build."));
#endif
    }

    if (method == QStringLiteral("app.list_windows")) {
        const QString filterType = optionalJsonString(params, QStringLiteral("type")).trimmed();
        QJsonArray arr;
        for (MyWidget *w : d_app->windowsList()) {
            if (!w)
                continue;
            const QString wt = windowTypeString(w);
            if (!filterType.isEmpty() && wt.compare(filterType, Qt::CaseInsensitive) != 0)
                continue;
            QJsonObject item;
            item[QStringLiteral("name")] = w->name();
            item[QStringLiteral("label")] = w->windowLabel();
            item[QStringLiteral("type")] = wt;
            item[QStringLiteral("hidden")] = d_app->hidden(w);
            arr.append(item);
        }
        outResult[QStringLiteral("count")] = arr.size();
        outResult[QStringLiteral("windows")] = arr;
        return true;
    }

    if (method == QStringLiteral("app.window_info")) {
        const QString name = !optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("name")).trimmed()
                : optionalJsonString(params, QStringLiteral("window")).trimmed();
        if (name.isEmpty()) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Parameter 'name' must be a non-empty window name."),
                    collectWindowNames(d_app));
        }
        MyWidget *w = findWindowByName(d_app, name);
        if (!w) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Window not found: '%1'.").arg(name), collectWindowNames(d_app));
        }
        if (w->inherits("Table"))
            outResult = serializeTableInfo(static_cast<Table *>(w));
        else if (w->inherits("Matrix"))
            outResult = serializeMatrixInfo(static_cast<Matrix *>(w));
        else if (w->inherits("MultiLayer"))
            outResult = serializeGraphInfo(static_cast<MultiLayer *>(w));
        else if (w->inherits("Graph3D"))
            outResult = serializeGraph3DInfo(static_cast<Graph3D *>(w));
        else {
            outResult[QStringLiteral("name")] = w->name();
            outResult[QStringLiteral("label")] = w->windowLabel();
            outResult[QStringLiteral("type")] = windowTypeString(w);
        }
        return true;
    }

    if (method == QStringLiteral("app.activate_window")) {
        const QString name = !optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("name")).trimmed()
                : optionalJsonString(params, QStringLiteral("window")).trimmed();
        if (name.isEmpty()) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Parameter 'name' must be a non-empty window name."),
                    collectWindowNames(d_app));
        }
        MyWidget *w = findWindowByName(d_app, name);
        if (!w) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Window not found: '%1'.").arg(name), collectWindowNames(d_app));
        }
        d_app->activateSubWindow(w);
        if (w->inherits("MultiLayer")) {
            lastUndoDomain = QStringLiteral("graph");
            lastUndoGraph = w->name();
        } else if (w->inherits("Table") || w->inherits("Matrix")) {
            lastUndoDomain = QStringLiteral("project");
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("name")] = w->name();
        outResult[QStringLiteral("type")] = windowTypeString(w);
        return true;
    }

    if (method == QStringLiteral("app.rename_window")) {
        const QString oldName = !optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("name")).trimmed()
                : (!optionalJsonString(params, QStringLiteral("old_name")).trimmed().isEmpty()
                           ? optionalJsonString(params, QStringLiteral("old_name")).trimmed()
                           : optionalJsonString(params, QStringLiteral("window")).trimmed());
        const QString newName = optionalJsonString(params, QStringLiteral("new_name")).trimmed();
        if (oldName.isEmpty()) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Parameter 'name' (or 'old_name') must be a non-empty window name."),
                    collectWindowNames(d_app));
        }
        if (newName.isEmpty())
            throwInvalidParams(QStringLiteral("new_name must not be empty."));
        MyWidget *w = findWindowByName(d_app, oldName);
        if (!w) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Window not found: '%1'.").arg(oldName),
                    collectWindowNames(d_app));
        }
        const QString prevWindowName = w->name();
        QString checkStr = newName;
        checkStr.replace(QLatin1Char('-'), QLatin1Char('_'));
        if (checkStr.isEmpty() || checkStr.contains(QRegExp(QStringLiteral("\\W")))) {
            throwInvalidParams(
                    QStringLiteral("Invalid window name '%1': only letters, digits, '-' and '_' are allowed.")
                            .arg(newName));
        }
        QString canonicalNew = checkStr;
        canonicalNew.replace(QLatin1Char('_'), QLatin1Char('-'));
        if (w->name() != canonicalNew) {
            if (d_app->alreadyUsedName(canonicalNew)) {
                throwInvalidParams(
                        QStringLiteral("Window name '%1' is already in use.").arg(canonicalNew));
            }
            // a workbook's name, unless the window is a worksheet of that workbook (the same
            // rule as workbook.rename / workbook.create the other way round)
            if (d_app->workbookHasName(canonicalNew, w)) {
                throwInvalidParams(
                        QStringLiteral("Window name '%1' is already in use by a workbook.")
                                .arg(canonicalNew));
            }
            if (w->inherits("Table")
                && static_cast<Table *>(w)->colNames().contains(canonicalNew)) {
                throwInvalidParams(
                        QStringLiteral("Table window name '%1' must differ from its column names.")
                                .arg(canonicalNew));
            }
            if (!d_app->renameWindow(w, canonicalNew))
                throwRpcRuntime(QStringLiteral("Failed to rename window '%1' to '%2'.")
                                        .arg(oldName, canonicalNew));
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("old_name")] = prevWindowName;
        outResult[QStringLiteral("name")] = w->name();
        outResult[QStringLiteral("new_name")] = w->name();
        return true;
    }

    if (method == QStringLiteral("app.close_window")) {
        const QString name = !optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("name")).trimmed()
                : optionalJsonString(params, QStringLiteral("window")).trimmed();
        if (name.isEmpty()) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Parameter 'name' must be a non-empty window name."),
                    collectWindowNames(d_app));
        }
        MyWidget *w = findWindowByName(d_app, name);
        if (!w) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Window not found: '%1'.").arg(name), collectWindowNames(d_app));
        }
        const QString closedName = w->name();
        QPointer<MyWidget> wg(w);
        w->askOnCloseEvent(false);
        w->hide();
        w->close();
        if (wg && wg->folder())
            d_app->closeWindow(wg.data());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        // A worksheet or matrix removed with an Undo step (the project's stack holds it) keeps
        // its window, as when it is closed from the menu: the Undo shows that window again and
        // brings back the curves it held. Deleting it left the Undo a table without a window.
        bool keptForUndo = false;
        if (auto *t = qobject_cast<Table *>(wg.data()))
            keptForUndo = t->d_future_table && !t->d_future_table->parentAspect()
                    && t->d_future_table->property("_tg_owned_by_undo_cmd").toBool();
        else if (auto *m = qobject_cast<Matrix *>(wg.data()))
            keptForUndo = m->d_future_matrix && !m->d_future_matrix->parentAspect()
                    && m->d_future_matrix->property("_tg_owned_by_undo_cmd").toBool();
        if (wg && !keptForUndo) {
            wg->hide();
            wg->setParent(nullptr);
            delete wg.data();
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("closed")] = closedName;
        return true;
    }

    if (method == QStringLiteral("app.clone_window")) {
        const QString name = !optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("name")).trimmed()
                : optionalJsonString(params, QStringLiteral("window")).trimmed();
        if (name.isEmpty()) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Parameter 'name' must be a non-empty window name."),
                    collectWindowNames(d_app));
        }
        MyWidget *w = findWindowByName(d_app, name);
        if (!w) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Window not found: '%1'.").arg(name), collectWindowNames(d_app));
        }
        MyWidget *cloned = d_app->clone(w);
        if (!cloned)
            throwRpcRuntime(QStringLiteral("Failed to clone window '%1'.").arg(w->name()));
        applyRequestedWindowName(d_app, cloned, params, QStringLiteral("new_name"));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("name")] = cloned->name();
        outResult[QStringLiteral("type")] = windowTypeString(cloned);
        return true;
    }

    if (method == QStringLiteral("app.set_language")) {
        QString lang = optionalJsonString(params, QStringLiteral("language")).trimmed().toLower();
        if (lang.isEmpty())
            lang = optionalJsonString(params, QStringLiteral("lang")).trimmed().toLower();
        if (lang != QStringLiteral("ja") && lang != QStringLiteral("en"))
            throwInvalidParams(
                    QStringLiteral("Unsupported language '%1' (expected 'ja' or 'en').").arg(lang));
        d_app->switchToLanguage(lang);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("language")] = d_app->appLanguage;
        return true;
    }

    if (method == QStringLiteral("app.get_language")) {
        outResult[QStringLiteral("language")] = d_app->appLanguage;
        return true;
    }

    if (method == QStringLiteral("app.set_recalc_mode")) {
        const QString m = optionalJsonString(params, QStringLiteral("mode")).trimmed().toLower();
        RecalcManager *rm = RecalcManager::instance(d_app);
        bool autoMode = false;
        if (m == QStringLiteral("auto")) {
            rm->setRecalcMode(RecalcManager::Auto);
            d_app->setRecalcModeAuto();
            autoMode = true;
        } else if (m == QStringLiteral("manual")) {
            rm->setRecalcMode(RecalcManager::Manual);
            d_app->setRecalcModeManual();
        } else if (m == QStringLiteral("disabled") || m == QStringLiteral("off")) {
            rm->setRecalcMode(RecalcManager::Disabled);
            d_app->setRecalcModeDisabled();
        } else {
            throwInvalidParams(
                    QStringLiteral("Invalid recalc mode '%1' (expected 'auto'|'manual'|'disabled').")
                            .arg(m));
        }
        for (MyWidget *w : d_app->windowsList()) {
            if (w && w->inherits("Table"))
                static_cast<Table *>(w)->setAutoRecalcFormulas(autoMode);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("mode")] = m;
        return true;
    }

    if (method == QStringLiteral("app.list_sample_datasets")) {
        const QString reqCat = optionalJsonString(params, QStringLiteral("category")).trimmed();
        const QVector<CurveMathTools::SampleDatasetInfo> allSets =
                CurveMathTools::listSampleDatasets();
        QJsonArray arr;
        for (const auto &ds : allSets) {
            if (!reqCat.isEmpty() && ds.category.compare(reqCat, Qt::CaseInsensitive) != 0
                && !ds.category.contains(reqCat, Qt::CaseInsensitive)) {
                continue;
            }
            QJsonObject dObj;
            dObj[QStringLiteral("id")] = ds.id;
            dObj[QStringLiteral("name")] = ds.id;
            dObj[QStringLiteral("title")] = ds.title;
            dObj[QStringLiteral("category")] = ds.category;
            dObj[QStringLiteral("description")] = ds.description;
            arr.append(dObj);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("count")] = arr.size();
        outResult[QStringLiteral("datasets")] = arr;
        return true;
    }

    if (method == QStringLiteral("app.load_sample_dataset")) {
        QString dsId = !optionalJsonString(params, QStringLiteral("id")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("id")).trimmed()
                : (!optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                           ? optionalJsonString(params, QStringLiteral("name")).trimmed()
                           : optionalJsonString(params, QStringLiteral("dataset")).trimmed());
        const QVector<CurveMathTools::SampleDatasetInfo> allSets =
                CurveMathTools::listSampleDatasets();
        if (dsId.isEmpty() && !allSets.isEmpty())
            dsId = allSets.first().id;
        const bool autoPlot = params.contains(QStringLiteral("auto_plot"))
                ? optionalJsonBool(params, QStringLiteral("auto_plot"), true)
                : optionalJsonBool(params, QStringLiteral("plot"), true);
        Table *outTbl = nullptr;
        Matrix *outMat = nullptr;
        MultiLayer *ml =
                CurveMathTools::loadSampleDataset(d_app, dsId, autoPlot, &outTbl, &outMat);
        if (!ml && !outTbl && !outMat) {
            QStringList availIds;
            for (const auto &ds : allSets)
                availIds << ds.id;
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Unknown sample dataset '%1'.").arg(dsId), availIds);
        }
        const QString rawPreset = optionalJsonString(params, QStringLiteral("preset")).trimmed();
        if (ml) {
            MultiLayer::GraphStylePresetSpec spec;
            if (resolveStylePresetForRpc(rawPreset, &spec, true)) {
                applyResolvedStylePreset(ml, spec);
            } else {
                for (Graph *lg : ml->layersList())
                    enforceArialGraphFonts(lg);
            }
            for (Graph *lg : ml->layersList())
                replotGraphOrDefer(lg);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("id")] = dsId;
        if (outTbl)
            outResult[QStringLiteral("table")] = outTbl->name();
        if (outMat)
            outResult[QStringLiteral("matrix")] = outMat->name();
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("app.recalculate")) {
        RecalcManager *rm = RecalcManager::instance(d_app);
        auto collectLiveTables = [d_app]() -> QVector<Table *> {
            QVector<Table *> out;
            for (MyWidget *w : d_app->windowsList()) {
                if (w && w->inherits("Table"))
                    out.append(static_cast<Table *>(w));
            }
            return out;
        };
        const QVector<Table *> initialTables = collectLiveTables();
        // Formula columns are evaluated in dependency order through the worksheets' own
        // recalculation, so circular formulas get a formula error and keep their values (they
        // used to grow on every call). Passes repeat only while something still changes.
        auto formulaState = [](const QVector<Table *> &tables) {
            QStringList st;
            for (Table *t : tables) {
                if (!t)
                    continue;
                for (int c = 0; c < t->numCols(); ++c) {
                    Column *col = t->column(c);
                    if (!col || col->formula(0).trimmed().isEmpty())
                        continue;
                    for (int r = 0; r < t->numRows(); ++r) {
                        if (col->isInvalid(r))
                            st << QStringLiteral("\x01");
                        else if (col->columnMode() == SciDAVis::ColumnMode::Numeric)
                            st << QString::number(col->valueAt(r), 'g', 17);
                        else
                            st << col->asStringColumn()->textAt(r);
                    }
                }
            }
            return st;
        };
        const int maxPasses = std::max(2, initialTables.size() + 2);
        for (int pass = 0; pass < maxPasses; ++pass) {
            const QVector<Table *> liveBefore = collectLiveTables();
            const QStringList before = formulaState(liveBefore);
            recalcFormulaColumnsWithCycleCheck(liveBefore);
            const QVector<Table *> liveAfterPass = collectLiveTables();
            if (formulaState(liveAfterPass) == before)
                break;
        }
        QVector<QPointer<Table>> tablesToUpdate;
        for (Table *t : collectLiveTables())
            tablesToUpdate.append(QPointer<Table>(t));
        for (const QPointer<Table> &tp : tablesToUpdate) {
            for (int c = 0;; ++c) {
                Table *t = tp.data();
                if (!t || c >= t->numCols())
                    break;
                Column *col = t->column(c);
                if (col && !col->formula(0).trimmed().isEmpty()) {
                    const QString colFullName = fullColumnName(t, c);
                    if (Table *liveT = tp.data())
                        d_app->updateCurves(liveT, colFullName);
                }
            }
        }
        const int updatedLinks = rm->recalculateAll();
        const QVector<Table *> liveAfter = collectLiveTables();
        const QJsonArray fErrs = formulaErrorsJson(liveAfter);
        outResult[QStringLiteral("ok")] = fErrs.isEmpty();
        outResult[QStringLiteral("updated_links")] = updatedLinks;
        outResult[QStringLiteral("tables_checked")] = liveAfter.size();
        outResult[QStringLiteral("formula_errors")] = fErrs;
        return true;
    }

    if (method == QStringLiteral("app.undo") || method == QStringLiteral("app.redo")) {
        const bool isUndo = (method == QStringLiteral("app.undo"));

        struct ScopedTableReplayGuard
        {
            ScopedTableReplayGuard() { Table::beginUndoRedoReplay(); }
            ~ScopedTableReplayGuard() { Table::endUndoRedoReplay(); }
        };

        auto refreshAfterProjectUndoRedo = [&]() {
            for (MyWidget *w : d_app->windowsList()) {
                if (!w)
                    continue;
                if (w->inherits("Table")) {
                    auto *t = static_cast<Table *>(w);
                    for (int c = 0; c < t->numCols(); ++c)
                        d_app->updateCurves(t, fullColumnName(t, c));
                } else if (w->inherits("Matrix")) {
                    d_app->updateMatrixPlots(static_cast<Matrix *>(w));
                }
            }
            d_app->refreshAllGraphsAutoAxisTitles();
        };

        const QString graphParam = optionalJsonString(params, QStringLiteral("graph")).trimmed();
        const QString tableParam = optionalJsonString(params, QStringLiteral("table")).trimmed();
        const QString matrixParam = optionalJsonString(params, QStringLiteral("matrix")).trimmed();
        const QString windowParam = optionalJsonString(params, QStringLiteral("window")).trimmed();

        if (!graphParam.isEmpty()) {
            MultiLayer *ml = resolveMultiLayer(d_app, params, QStringLiteral("graph"), true);
            QUndoStack *us = ml->undoStack();
            if (us) {
                ScopedTableReplayGuard replayGuard;
                if (isUndo && us->canUndo())
                    us->undo();
                else if (!isUndo && us->canRedo())
                    us->redo();
            }
            d_app->updateUndoRedoActionsForActiveWindow();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("graph")] = ml->name();
            outResult[QStringLiteral("window")] = ml->name();
            outResult[QStringLiteral("can_undo")] = us ? us->canUndo() : false;
            outResult[QStringLiteral("can_redo")] = us ? us->canRedo() : false;
            return true;
        }

        if (!tableParam.isEmpty() || !matrixParam.isEmpty()) {
            QUndoStack *us = nullptr;
            QString winName;
            if (!tableParam.isEmpty()) {
                Table *t = resolveTable(d_app, params, QStringLiteral("table"), true);
                winName = t->name();
                us = t->d_future_table ? t->d_future_table->undoStack() : nullptr;
            } else {
                Matrix *m = resolveMatrix(d_app, params, QStringLiteral("matrix"), true);
                winName = m->name();
                us = m->d_future_matrix ? m->d_future_matrix->undoStack() : nullptr;
            }
            if (us) {
                {
                    ScopedTableReplayGuard replayGuard;
                    if (isUndo && us->canUndo())
                        us->undo();
                    else if (!isUndo && us->canRedo())
                        us->redo();
                }
                refreshAfterProjectUndoRedo();
            }
            d_app->updateUndoRedoActionsForActiveWindow();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("window")] = winName;
            outResult[QStringLiteral("can_undo")] = us ? us->canUndo() : false;
            outResult[QStringLiteral("can_redo")] = us ? us->canRedo() : false;
            return true;
        }

        if (!windowParam.isEmpty()) {
            MyWidget *w = findWindowByName(d_app, windowParam);
            if (!w) {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("Window not found: '%1'.").arg(windowParam),
                        collectWindowNames(d_app));
            }
            QUndoStack *us = nullptr;
            bool isProj = false;
            if (w->inherits("MultiLayer")) {
                us = static_cast<MultiLayer *>(w)->undoStack();
            } else if (w->inherits("Table")) {
                auto *t = static_cast<Table *>(w);
                us = t->d_future_table ? t->d_future_table->undoStack() : nullptr;
                isProj = true;
            } else if (w->inherits("Matrix")) {
                auto *m = static_cast<Matrix *>(w);
                us = m->d_future_matrix ? m->d_future_matrix->undoStack() : nullptr;
                isProj = true;
            }
            if (us) {
                {
                    ScopedTableReplayGuard replayGuard;
                    if (isUndo && us->canUndo())
                        us->undo();
                    else if (!isUndo && us->canRedo())
                        us->redo();
                }
                if (isProj)
                    refreshAfterProjectUndoRedo();
            }
            d_app->updateUndoRedoActionsForActiveWindow();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("window")] = w->name();
            outResult[QStringLiteral("can_undo")] = us ? us->canUndo() : false;
            outResult[QStringLiteral("can_redo")] = us ? us->canRedo() : false;
            return true;
        }

        QUndoStack *projStack = d_app->project() ? d_app->project()->undoStack() : nullptr;

        QWidget *activeSub = d_app->d_workspace.activeSubWindow();
        MultiLayer *activeMl = (activeSub && activeSub->inherits("MultiLayer"))
                ? static_cast<MultiLayer *>(activeSub)
                : nullptr;
        MultiLayer *fallbackMl = nullptr;
        if (!lastUndoGraph.isEmpty()) {
            if (MyWidget *mw = findWindowByName(d_app, lastUndoGraph)) {
                if (mw->inherits("MultiLayer"))
                    fallbackMl = static_cast<MultiLayer *>(mw);
            }
        }
        if (!fallbackMl)
            fallbackMl = resolveMultiLayer(d_app, QJsonObject(), QStringLiteral("graph"), false);

        const int projIdxBefore = projStack ? projStack->index() : -1;
        const int mlIdxBefore =
                (fallbackMl && fallbackMl->undoStack()) ? fallbackMl->undoStack()->index() : -1;

        // No target given: go back (forward) through the Undo steps the RPCs made, in the order
        // they were made across the project's stack and the graphs' stacks (logged by
        // PythonAutomationServer::dispatchMethod): two graphs edited in turn are undone back
        // through both, one step at a time, and redo walks forward again. Not the MDI active
        // window's stack: graph RPCs do not activate their graph, so the active window is often
        // the worksheet whose data entry would be undone instead (the curves then lost their
        // data). After app.activate_window, or when no logged step can be undone / redone, the
        // stack the last RPC changed decides, then the active window and the most recently
        // modified stack below.
        // the graph whose stack this is (null for the project's stack or a stack no window holds)
        auto graphOfStack = [&](QUndoStack *us) -> MultiLayer * {
            for (MyWidget *w : d_app->windowsList()) {
                auto *ml = qobject_cast<MultiLayer *>(w);
                if (ml && ml->undoStack() == us)
                    return ml;
            }
            return nullptr;
        };
        // the stack of the latest logged step that can be undone (redone), if any
        auto loggedStack = [&](bool undo, MultiLayer **mlOut) -> QUndoStack * {
            if (!server)
                return nullptr;
            const QList<QPointer<QUndoStack>> &log = undo ? server->rpcUndoLog() : server->rpcRedoLog();
            for (int i = log.size() - 1; i >= 0; --i) {
                QUndoStack *us = log.at(i).data();
                if (!us || !(undo ? us->canUndo() : us->canRedo()))
                    continue;
                // the project's stack (shared by the tables and matrices) or a graph's
                MultiLayer *ml = graphOfStack(us);
                if (!ml && us != projStack)
                    continue;
                if (mlOut)
                    *mlOut = ml;
                return us;
            }
            return nullptr;
        };
        {
            QUndoStack *recorded = nullptr;
            MultiLayer *recordedMl = nullptr;
            if (lastUndoDomain == QStringLiteral("graph") && !lastUndoGraph.isEmpty()) {
                MyWidget *mw = findWindowByName(d_app, lastUndoGraph);
                if (mw && mw->inherits("MultiLayer")) {
                    recordedMl = static_cast<MultiLayer *>(mw);
                    recorded = recordedMl->undoStack();
                }
            } else if (lastUndoDomain == QStringLiteral("project")) {
                recorded = projStack;
            }
            if (!(server && server->rpcUndoWindowChosen())) {
                MultiLayer *loggedMl = nullptr;
                if (QUndoStack *logged = loggedStack(isUndo, &loggedMl)) {
                    recorded = logged;
                    recordedMl = loggedMl;
                }
            }
            if (recorded && (isUndo ? recorded->canUndo() : recorded->canRedo())) {
                {
                    ScopedTableReplayGuard replayGuard;
                    if (isUndo)
                        recorded->undo();
                    else
                        recorded->redo();
                }
                if (!recordedMl)
                    refreshAfterProjectUndoRedo();
                d_app->updateUndoRedoActionsForActiveWindow();
                QUndoStack *other = recordedMl ? projStack
                                               : (fallbackMl ? fallbackMl->undoStack() : nullptr);
                outResult[QStringLiteral("ok")] = true;
                if (recordedMl) {
                    outResult[QStringLiteral("graph")] = recordedMl->name();
                    outResult[QStringLiteral("window")] = recordedMl->name();
                }
                outResult[QStringLiteral("can_undo")] = recorded->canUndo()
                        || (other && other->canUndo()) || loggedStack(true, nullptr) != nullptr;
                outResult[QStringLiteral("can_redo")] = recorded->canRedo()
                        || (other && other->canRedo()) || loggedStack(false, nullptr) != nullptr;
                return true;
            }
        }

        if (activeSub) {
            ScopedTableReplayGuard replayGuard;
            if (isUndo)
                d_app->undo();
            else
                d_app->redo();
        }

        const int projIdxAfter = projStack ? projStack->index() : -1;
        const int mlIdxAfter =
                (fallbackMl && fallbackMl->undoStack()) ? fallbackMl->undoStack()->index() : -1;

        // In headless mode QMdiArea::activeSubWindow() is null; route to whichever undo stack
        // (project Table/Matrix stack vs MultiLayer graph stack) was modified most recently.
        if (!activeSub && projIdxAfter == projIdxBefore && mlIdxAfter == mlIdxBefore) {
            auto tryStack = [&](QUndoStack *us, bool isProj) -> bool {
                if (!us)
                    return false;
                if (isUndo && us->canUndo()) {
                    {
                        ScopedTableReplayGuard replayGuard;
                        us->undo();
                    }
                    if (isProj)
                        refreshAfterProjectUndoRedo();
                    return true;
                }
                if (!isUndo && us->canRedo()) {
                    {
                        ScopedTableReplayGuard replayGuard;
                        us->redo();
                    }
                    if (isProj)
                        refreshAfterProjectUndoRedo();
                    return true;
                }
                return false;
            };

            QUndoStack *mlStack = fallbackMl ? fallbackMl->undoStack() : nullptr;
            if (lastUndoDomain == QStringLiteral("project")) {
                if (!tryStack(projStack, true))
                    tryStack(mlStack, false);
            } else if (lastUndoDomain == QStringLiteral("graph")) {
                if (!tryStack(mlStack, false))
                    tryStack(projStack, true);
            } else {
                if (!tryStack(mlStack, false))
                    tryStack(projStack, true);
            }
        } else if (projStack && projStack->index() != projIdxBefore) {
            refreshAfterProjectUndoRedo();
        }

        d_app->updateUndoRedoActionsForActiveWindow();

        bool canUndo = false;
        bool canRedo = false;
        if (activeMl && activeMl->undoStack()) {
            canUndo = activeMl->undoStack()->canUndo();
            canRedo = activeMl->undoStack()->canRedo();
        } else {
            if (projStack) {
                canUndo = canUndo || projStack->canUndo();
                canRedo = canRedo || projStack->canRedo();
            }
            if (!activeSub && fallbackMl && fallbackMl->undoStack()) {
                canUndo = canUndo || fallbackMl->undoStack()->canUndo();
                canRedo = canRedo || fallbackMl->undoStack()->canRedo();
            }
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("can_undo")] = canUndo;
        outResult[QStringLiteral("can_redo")] = canRedo;
        return true;
    }

    if (method == QStringLiteral("app.set_focus_hud")) {
        const bool currentState = d_app->modernRibbonBar()
                ? d_app->modernRibbonBar()->isFocusHudMode()
                : focusHudFallback;
        const bool enabled = params.contains(QStringLiteral("enabled"))
                ? optionalJsonBool(params, QStringLiteral("enabled"), !currentState)
                : !currentState;
        focusHudFallback = enabled;
        if (ModernRibbonBar *rib = d_app->modernRibbonBar())
            rib->setFocusHudMode(enabled);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("enabled")] = d_app->modernRibbonBar()
                ? d_app->modernRibbonBar()->isFocusHudMode()
                : focusHudFallback;
        return true;
    }

    if (method == QStringLiteral("app.get_focus_hud")) {
        const bool cur = d_app->modernRibbonBar()
                ? d_app->modernRibbonBar()->isFocusHudMode()
                : focusHudFallback;
        focusHudFallback = cur;
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("enabled")] = cur;
        return true;
    }

    if (method == QStringLiteral("app.set_workspace_mode")) {
        const QString mode = optionalJsonString(params, QStringLiteral("mode")).trimmed().toLower();
        if (mode == QStringLiteral("tabs") || mode == QStringLiteral("tabbed")) {
            if (d_app->modernRibbonBar())
                d_app->modernRibbonBar()->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
            else
                d_app->setTabbedWorkspaceView(true);
            workspaceModeFallback = QStringLiteral("tabs");
        } else if (mode == QStringLiteral("mdi") || mode == QStringLiteral("free")) {
            if (d_app->modernRibbonBar())
                d_app->modernRibbonBar()->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
            else
                d_app->setTabbedWorkspaceView(false);
            workspaceModeFallback = QStringLiteral("mdi");
        } else if (mode == QStringLiteral("split")) {
            if (d_app->modernRibbonBar()) {
                d_app->modernRibbonBar()->setWorkspaceLayoutMode(
                        ModernRibbonBar::LayoutSplitTableGraph);
            } else {
                d_app->setTabbedWorkspaceView(false);
                d_app->d_workspace.tileSubWindows();
            }
            workspaceModeFallback = QStringLiteral("split");
        } else {
            throwInvalidParams(
                    QStringLiteral("Invalid workspace mode '%1' (expected 'tabs'|'split'|'mdi').")
                            .arg(mode));
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("mode")] = workspaceModeFallback;
        return true;
    }

    if (method == QStringLiteral("app.grab_gui_diagnostic")) {
        const QString path = optionalJsonString(params, QStringLiteral("path")).trimmed();
        const int reqW = parseBoundedInt(params, QStringLiteral("width"), 0, 0, 16384);
        const int reqH = parseBoundedInt(params, QStringLiteral("height"), 0, 0, 16384);
        if ((reqW > 0 && reqW < 64) || (reqH > 0 && reqH < 64)) {
            throwInvalidParams(
                    QStringLiteral("Diagnostic capture dimensions must be 0 (current size) or in [64, 16384]."));
        }
        if (reqW >= 64 && reqH >= 64) {
            if (d_app->isMaximized() || d_app->isFullScreen())
                d_app->showNormal();
            d_app->resize(reqW, reqH);
        }
        if (!QCoreApplication::instance()->property("torigraph_noninteractive").toBool()
            && !d_app->isVisible())
            d_app->show();

        ModernRibbonBar *rib = d_app->modernRibbonBar();
        if (rib && params.contains(QStringLiteral("focus_hud"))
            && !params.value(QStringLiteral("focus_hud")).isNull()) {
            const bool fh = optionalJsonBool(params, QStringLiteral("focus_hud"), false);
            rib->setFocusHudMode(fh);
            focusHudFallback = fh;
        }
        if (params.contains(QStringLiteral("workspace_mode"))
            && !params.value(QStringLiteral("workspace_mode")).isNull()) {
            const QString wm =
                    optionalJsonString(params, QStringLiteral("workspace_mode")).trimmed().toLower();
            if (wm != QStringLiteral("tabs") && wm != QStringLiteral("tabbed")
                && wm != QStringLiteral("split") && wm != QStringLiteral("mdi")
                && wm != QStringLiteral("free") && !wm.isEmpty()) {
                throwInvalidParams(
                        QStringLiteral("Invalid workspace_mode '%1' (expected 'tabs'|'split'|'mdi').")
                                .arg(wm));
            }
            if (rib) {
                if (wm == QStringLiteral("tabs") || wm == QStringLiteral("tabbed"))
                    rib->setWorkspaceLayoutMode(ModernRibbonBar::LayoutTabbed);
                else if (wm == QStringLiteral("split"))
                    rib->setWorkspaceLayoutMode(ModernRibbonBar::LayoutSplitTableGraph);
                else if (wm == QStringLiteral("mdi") || wm == QStringLiteral("free"))
                    rib->setWorkspaceLayoutMode(ModernRibbonBar::LayoutFreeMdi);
            }
        }

        const bool hasShowExplorer = params.contains(QStringLiteral("show_explorer"))
                && !params.value(QStringLiteral("show_explorer")).isNull();
        const bool wantExplorer =
                hasShowExplorer && optionalJsonBool(params, QStringLiteral("show_explorer"), false);
        const bool hasExpandInspector = params.contains(QStringLiteral("expand_inspector"))
                && !params.value(QStringLiteral("expand_inspector")).isNull();
        const bool wantExpandInspector =
                hasExpandInspector && optionalJsonBool(params, QStringLiteral("expand_inspector"), false);

        GraphInspectorDock *insp = d_app->findChild<GraphInspectorDock *>();
        struct ScopedHeadlessInspectorGuard {
            ApplicationWindow *app = nullptr;
            bool wasHeadless = false;
            ScopedHeadlessInspectorGuard(ApplicationWindow *a, bool reveal)
                : app(a), wasHeadless(a ? a->isHeadlessMode() : false)
            {
                if (app && reveal && wasHeadless)
                    app->setHeadlessMode(false);
            }
            ~ScopedHeadlessInspectorGuard()
            {
                if (app && wasHeadless)
                    app->setHeadlessMode(true);
            }
        } headlessInspectorGuard(d_app, wantExplorer || wantExpandInspector);

        bool dockedLeftOrRight = false;
        if (params.contains(QStringLiteral("dock_explorer"))
            && !params.value(QStringLiteral("dock_explorer")).isNull()) {
            const QString area =
                    optionalJsonString(params, QStringLiteral("dock_explorer")).trimmed().toLower();
            if (!area.isEmpty() && area != QStringLiteral("left") && area != QStringLiteral("right")
                && area != QStringLiteral("bottom")) {
                throwInvalidParams(
                        QStringLiteral("Invalid dock_explorer '%1' (expected 'left'|'right'|'bottom').")
                                .arg(area));
            }
            d_app->explorerWindow.setFloating(false);
            if (area == QStringLiteral("right")) {
                d_app->addDockWidget(Qt::RightDockWidgetArea, &d_app->explorerWindow);
                dockedLeftOrRight = true;
            } else if (area == QStringLiteral("bottom")) {
                d_app->addDockWidget(Qt::BottomDockWidgetArea, &d_app->explorerWindow);
            } else {
                d_app->addDockWidget(Qt::LeftDockWidgetArea, &d_app->explorerWindow);
                dockedLeftOrRight = true;
            }
            if (!hasShowExplorer || wantExplorer) {
                d_app->explorerWindow.show();
            }
        }
        if (hasShowExplorer) {
            d_app->explorerWindow.setVisible(wantExplorer);
        }

        if (params.contains(QStringLiteral("focus_table_drawer"))
            && !params.value(QStringLiteral("focus_table_drawer")).isNull()) {
            const bool ftd = optionalJsonBool(params, QStringLiteral("focus_table_drawer"), false);
            if (auto *hud = d_app->findChild<FocusCanvasHudOverlay *>())
                hud->setTableDrawerVisible(ftd);
        }

        if (insp && hasExpandInspector) {
            if (wantExpandInspector) {
                if (insp->isHidden() || insp->isCollapsedRail())
                    d_app->toggleGraphInspectorDock();
                insp->setUserPinned(true);
                insp->show();
                insp->setCollapsedRail(false);
            } else {
                insp->setUserPinned(false);
                insp->setCollapsedRail(true);
            }
        }
        if (insp && params.contains(QStringLiteral("inspector_tab"))
            && !params.value(QStringLiteral("inspector_tab")).isNull()) {
            const int itab = parseBoundedInt(params, QStringLiteral("inspector_tab"), 0, 0, 3);
            insp->switchToInspectorPage(
                    static_cast<GraphInspectorDock::InspectorPageIndex>(itab));
        }

        if (d_app->explorerWindow.isVisibleTo(d_app) && !d_app->explorerWindow.isFloating()) {
            if (params.contains(QStringLiteral("explorer_width"))
                && !params.value(QStringLiteral("explorer_width")).isNull()) {
                const int explicitW = parseBoundedInt(
                        params, QStringLiteral("explorer_width"), 240, 60, 4096);
                d_app->resizeDocks({ &d_app->explorerWindow },
                                   { std::clamp(explicitW, 140, std::max(180, d_app->width() - 200)) },
                                   Qt::Horizontal);
            } else if (dockedLeftOrRight) {
                const int expW = qBound(220, int(d_app->width() * 0.22), 265);
                d_app->resizeDocks({ &d_app->explorerWindow }, { expW }, Qt::Horizontal);
            }
        }

        int targetTab = rib ? rib->currentTabIndex() : 0;
        if (rib && params.contains(QStringLiteral("ribbon_tab"))
            && !params.value(QStringLiteral("ribbon_tab")).isNull()) {
            targetTab = parseBoundedInt(params, QStringLiteral("ribbon_tab"), 0, 0,
                                        std::max(0, rib->tabCount() - 1));
        }
        if (rib)
            rib->setCurrentTabIndex(targetTab);

        auto pumpLayoutEvents = [d_app]() {
            d_app->ensurePolished();
            if (d_app->layout())
                d_app->layout()->activate();
            for (int pass = 0; pass < 4; ++pass) {
                QCoreApplication::sendPostedEvents();
                QApplication::processEvents();
            }
        };
        pumpLayoutEvents();

        int clippedCaptions = 0;
        QJsonArray clippedDetails;
        if (rib && rib->isVisibleTo(d_app)) {
            const bool checkAllTabs =
                    optionalJsonBool(params, QStringLiteral("check_all_tabs"), true);
            const int tabStart = checkAllTabs ? 0 : targetTab;
            const int tabEnd = checkAllTabs ? (rib->tabCount() - 1) : targetTab;
            for (int ti = tabStart; ti <= tabEnd; ++ti) {
                rib->setCurrentTabIndex(ti);
                pumpLayoutEvents();
                const QList<QLabel *> labels = rib->findChildren<QLabel *>();
                for (QLabel *lbl : labels) {
                    if (!lbl || !lbl->isVisibleTo(d_app)
                        || lbl->property("class").toString()
                                != QStringLiteral("RibbonGroupCaption"))
                        continue;
                    const QString txt = lbl->text().trimmed();
                    const int fmH = lbl->fontMetrics().height();
                    const int txtW = lbl->fontMetrics().horizontalAdvance(txt);
                    bool isClipped = txt.isEmpty() || (lbl->height() + 1 < fmH)
                            || (lbl->width() + 4 < txtW);
                    int botInVp = lbl->height();
                    int vpH = lbl->parentWidget() ? lbl->parentWidget()->height() : lbl->height();
                    if (lbl->parentWidget()
                        && lbl->geometry().bottom() > lbl->parentWidget()->height() + 1) {
                        isClipped = true;
                    }
                    for (QWidget *anc = lbl->parentWidget(); anc; anc = anc->parentWidget()) {
                        if (auto *sa = qobject_cast<QScrollArea *>(anc)) {
                            if (sa->viewport()) {
                                botInVp = lbl->mapTo(sa->viewport(), QPoint(0, lbl->height())).y();
                                vpH = sa->viewport()->height();
                                if (botInVp > vpH + 1)
                                    isClipped = true;
                            }
                            break;
                        }
                    }
                    if (isClipped) {
                        ++clippedCaptions;
                        clippedDetails.append(
                                QStringLiteral("tab%1:%2(h=%3,fmH=%4,w=%5,txtW=%6,botVp=%7,vpH=%8)")
                                        .arg(ti)
                                        .arg(txt)
                                        .arg(lbl->height())
                                        .arg(fmH)
                                        .arg(lbl->width())
                                        .arg(txtW)
                                        .arg(botInVp)
                                        .arg(vpH));
                    }
                }
            }
            if (rib->currentTabIndex() != targetTab) {
                rib->setCurrentTabIndex(targetTab);
                pumpLayoutEvents();
            }
        }
        if (insp && hasExpandInspector && wantExpandInspector
            && (insp->isHidden() || insp->isCollapsedRail())) {
            d_app->toggleGraphInspectorDock();
            insp->setUserPinned(true);
            insp->show();
            insp->setCollapsedRail(false);
            pumpLayoutEvents();
        }

        auto findInspectorPropScroll = [insp, d_app]() -> QScrollArea * {
            if (!insp)
                return nullptr;
            for (QScrollArea *sa : insp->findChildren<QScrollArea *>()) {
                if (sa && sa->isVisibleTo(d_app) && sa->widget() == insp->propertyStack())
                    return sa;
            }
            for (QScrollArea *sa : insp->findChildren<QScrollArea *>()) {
                if (sa && sa->isVisibleTo(d_app))
                    return sa;
            }
            return nullptr;
        };
        if (insp && params.contains(QStringLiteral("scroll_inspector"))
            && !params.value(QStringLiteral("scroll_inspector")).isNull()) {
            if (QScrollArea *propScroll = findInspectorPropScroll()) {
                if (QScrollBar *vsb = propScroll->verticalScrollBar()) {
                    const QJsonValue sv = params.value(QStringLiteral("scroll_inspector"));
                    if (sv.isBool())
                        vsb->setValue(sv.toBool() ? vsb->maximum() : 0);
                    else if (sv.isDouble())
                        vsb->setValue(std::clamp(qRound(sv.toDouble()), vsb->minimum(),
                                                 vsb->maximum()));
                    pumpLayoutEvents();
                }
            }
        }

        // Force full layout & render pass first so headless geometries are finalized before telemetry.
        pumpLayoutEvents();
        if (reqW >= 64 || reqH >= 64 || hasShowExplorer || hasExpandInspector
            || params.contains(QStringLiteral("dock_explorer"))
            || params.contains(QStringLiteral("workspace_mode"))) {
            // Allow coalesced viewport auto-fit debounce timers (20 ms) to settle before grab().
            QEventLoop settleLoop;
            QTimer::singleShot(45, &settleLoop, &QEventLoop::quit);
            settleLoop.exec(QEventLoop::ExcludeUserInputEvents);
            pumpLayoutEvents();
        }
        bool savedOk = true;
        QPixmap pm = d_app->grab();
        pumpLayoutEvents();
        if (!path.isEmpty()) {
            QFileInfo fi(path);
            if (fi.dir().exists() || QDir().mkpath(fi.dir().absolutePath()))
                savedOk = pm.save(path, "PNG");
            else
                savedOk = false;
        }

        int ribbonBottomY = 0;
        if (rib && rib->isVisibleTo(d_app)) {
            if (auto *tb = qobject_cast<QToolBar *>(rib->parentWidget())) {
                if (tb->isVisibleTo(d_app))
                    ribbonBottomY = tb->mapTo(d_app, QPoint(0, tb->height())).y();
                else
                    ribbonBottomY = rib->mapTo(d_app, QPoint(0, rib->height())).y();
            } else {
                ribbonBottomY = rib->mapTo(d_app, QPoint(0, rib->height())).y();
            }
        }
        const bool expVisible = d_app->explorerWindow.isVisibleTo(d_app);
        const QPoint explorerPos =
                expVisible ? d_app->explorerWindow.mapTo(d_app, QPoint(0, 0)) : QPoint(0, 99999);
        const int explorerTopY = expVisible ? explorerPos.y() : 99999;

        const bool inspVisible = (insp && insp->isVisibleTo(d_app));
        const QPoint inspectorPos =
                inspVisible ? insp->mapTo(d_app, QPoint(0, 0)) : QPoint(0, 99999);
        const int inspectorTopY = inspVisible ? inspectorPos.y() : 99999;

        int explorerSearchW = 0;
        for (QLineEdit *le : d_app->explorerWindow.findChildren<QLineEdit *>()) {
            if (le && le->isVisibleTo(d_app))
                explorerSearchW = std::max(explorerSearchW, le->width());
        }

        bool inspectorClippedBottom = false;
        int inspectorScrollMax = 0;
        int inspectorScrollVal = 0;
        if (inspVisible && insp && !insp->isCollapsedRail()) {
            if (QScrollArea *propScroll = findInspectorPropScroll()) {
                if (QScrollBar *vsb = propScroll->verticalScrollBar()) {
                    inspectorScrollMax = vsb->maximum();
                    inspectorScrollVal = vsb->value();
                    if (inspectorScrollMax > inspectorScrollVal)
                        inspectorClippedBottom = true;
                }
            }
            const int inspBottomY = insp->mapTo(d_app, QPoint(0, insp->height())).y();
            if (inspBottomY > d_app->height() + 1)
                inspectorClippedBottom = true;
        }

        int tableClippedCols = 0;
        Table *diagTable = qobject_cast<Table *>(d_app->d_workspace.activeSubWindow());
        if (!diagTable || !diagTable->isVisibleTo(d_app)) {
            diagTable = nullptr;
            for (MyWidget *w : d_app->windowsList()) {
                if (auto *t = qobject_cast<Table *>(w)) {
                    if (t->isVisibleTo(d_app)) {
                        diagTable = t;
                        break;
                    }
                }
            }
        }
        if (diagTable) {
            if (QTableView *tv = diagTable->findChild<QTableView *>()) {
                if (tv->isVisibleTo(d_app) && tv->viewport()) {
                    const int vpW = tv->viewport()->width();
                    for (int c = 0; c < diagTable->numCols(); ++c) {
                        if (tv->isColumnHidden(c))
                            continue;
                        const int cw = tv->columnWidth(c);
                        if (cw <= 0)
                            continue;
                        const int cx = tv->columnViewportPosition(c);
                        if (cx < 0 || cx + cw > vpW + 1)
                            ++tableClippedCols;
                    }
                }
            }
        }

        bool diagOk = true;
        QStringList diagErrors;
        if (wantExplorer
            && (!expVisible || explorerTopY == 99999 || explorerTopY < 0
                || explorerPos.x() < 0)) {
            diagOk = false;
            diagErrors << QStringLiteral(
                    "Explorer requested visible (show_explorer=true) but reported invalid or offscreen geometry.");
        }
        if (wantExpandInspector
            && (!inspVisible || !insp || insp->isCollapsedRail() || inspectorTopY == 99999
                || inspectorTopY < 0 || inspectorPos.x() < 0 || insp->width() <= 64)) {
            diagOk = false;
            diagErrors << QStringLiteral(
                    "Expanded Inspector requested (expand_inspector=true) but reported collapsed, invalid, or offscreen geometry.");
        }

        outResult[QStringLiteral("ok")] = savedOk && diagOk;
        if (!diagErrors.isEmpty())
            outResult[QStringLiteral("diagnostic_error")] = diagErrors.join(QLatin1Char(' '));
        outResult[QStringLiteral("path")] = path;
        outResult[QStringLiteral("width")] = pm.width();
        outResult[QStringLiteral("height")] = pm.height();
        outResult[QStringLiteral("ribbon_bottom_y")] = ribbonBottomY;
        outResult[QStringLiteral("explorer_top_y")] = explorerTopY;
        outResult[QStringLiteral("inspector_top_y")] = inspectorTopY;
        outResult[QStringLiteral("explorer_search_width")] = explorerSearchW;
        outResult[QStringLiteral("inspector_clipped_bottom")] = inspectorClippedBottom;
        outResult[QStringLiteral("inspector_scroll_max")] = inspectorScrollMax;
        outResult[QStringLiteral("inspector_scroll_value")] = inspectorScrollVal;
        outResult[QStringLiteral("table_clipped_cols")] = tableClippedCols;
        outResult[QStringLiteral("clipped_ribbon_captions")] = clippedCaptions;
        outResult[QStringLiteral("clipped_details")] = clippedDetails;
        return true;
    }

    if (method == QStringLiteral("app.check_updates")) {
        outResult = checkUpdatesBlockingFallback(params);
        return true;
    }

    if (method == QStringLiteral("app.list_actions")) {
        const QString query = optionalJsonString(params, QStringLiteral("filter")).trimmed();
        const QList<QAction *> actions = d_app->findChildren<QAction *>();
        QJsonArray arr;
        for (QAction *act : actions) {
            if (!act || act->isSeparator())
                continue;
            const QString objName = act->objectName();
            const QString cleanTxt = act->text().remove(QLatin1Char('&')).trimmed();
            if (objName.isEmpty() && cleanTxt.isEmpty())
                continue;
            if (!query.isEmpty() && !objName.contains(query, Qt::CaseInsensitive)
                && !cleanTxt.contains(query, Qt::CaseInsensitive))
                continue;
            QJsonObject aObj;
            aObj[QStringLiteral("name")] = objName;
            aObj[QStringLiteral("text")] = cleanTxt;
            aObj[QStringLiteral("shortcut")] = act->shortcut().toString(QKeySequence::PortableText);
            aObj[QStringLiteral("enabled")] = act->isEnabled();
            aObj[QStringLiteral("checkable")] = act->isCheckable();
            aObj[QStringLiteral("checked")] = act->isChecked();
            arr.append(aObj);
        }
        outResult[QStringLiteral("count")] = arr.size();
        outResult[QStringLiteral("actions")] = arr;
        return true;
    }

    if (method == QStringLiteral("app.trigger_action")) {
        const QString name = optionalJsonString(params, QStringLiteral("name")).trimmed();
        if (name.isEmpty())
            throwInvalidParams(QStringLiteral("Action or slot 'name' is required."));

        auto isBlockedInteractiveDialogTarget = [](const QString &ident,
                                                   const QString &menuText = QString()) -> bool {
            const QString id = ident.trimmed();
            if (id == QStringLiteral("openPythonConsole")
                || id == QStringLiteral("actionPythonConsole")
                || id == QStringLiteral("actionOpenPythonConsole")) {
                return false;
            }
            if (id.endsWith(QStringLiteral("Dialog")))
                return true;
            static const QSet<QString> kBlockedInteractiveTargets = {
                QStringLiteral("actionOpen"),
                QStringLiteral("open"),
                QStringLiteral("actionAppendProject"),
                QStringLiteral("appendProject"),
                QStringLiteral("actionSaveProjectAs"),
                QStringLiteral("saveProjectAs"),
                QStringLiteral("saveAsProject"),
                QStringLiteral("saveAsTemplate"),
                QStringLiteral("openTemplate"),
                QStringLiteral("saveNoteAs"),
                QStringLiteral("actionLoad"),
                QStringLiteral("importASCII"),
                QStringLiteral("actionExportGraph"),
                QStringLiteral("exportGraph"),
                QStringLiteral("actionExportAllGraphs"),
                QStringLiteral("exportAllGraphs"),
                QStringLiteral("actionExportPDF"),
                QStringLiteral("exportPDF"),
                QStringLiteral("actionPrint"),
                QStringLiteral("print"),
                QStringLiteral("actionPrintAllPlots"),
                QStringLiteral("printAllPlots"),
                QStringLiteral("actionPrintWindow"),
                QStringLiteral("printWindow"),
                QStringLiteral("actionLoadImage"),
                QStringLiteral("loadImage"),
                QStringLiteral("actionImportImage"),
                QStringLiteral("importImage"),
                QStringLiteral("actionAddImage"),
                QStringLiteral("addImage"),
                QStringLiteral("actionAbout"),
                QStringLiteral("about"),
                QStringLiteral("actionChooseHelpFolder"),
                QStringLiteral("chooseHelpFolder"),
                QStringLiteral("actionRunPythonScript"),
                QStringLiteral("actionCommandPalette"),
                QStringLiteral("openCommandPalette"),
                QStringLiteral("actionRename"),
                QStringLiteral("rename"),
                QStringLiteral("startRenameFolder"),
                QStringLiteral("folderProperties"),
                QStringLiteral("windowProperties"),
                QStringLiteral("projectProperties"),
                QStringLiteral("actionShowMoreWindows"),
                QStringLiteral("showMoreWindows"),
                QStringLiteral("actionResizeActiveWindow"),
                QStringLiteral("resizeActiveWindow"),
                QStringLiteral("actionResizeWindow"),
                QStringLiteral("resizeWindow"),
                QStringLiteral("actionShowPlotWizard"),
                QStringLiteral("showPlotWizard"),
                QStringLiteral("actionAddErrorBars"),
                QStringLiteral("addErrorBars"),
                QStringLiteral("actionAddFunctionCurve"),
                QStringLiteral("addFunctionCurve"),
                QStringLiteral("actionNewFunctionPlot"),
                QStringLiteral("newFunctionPlot"),
                QStringLiteral("actionNewSurfacePlot"),
                QStringLiteral("newSurfacePlot"),
                QStringLiteral("actionEditSurfacePlot"),
                QStringLiteral("editSurfacePlot"),
                QStringLiteral("actionAdd3DData"),
                QStringLiteral("add3DData"),
                QStringLiteral("change3DData"),
                QStringLiteral("change3DMatrix"),
                QStringLiteral("add3DMatrixPlot"),
                QStringLiteral("setPlot3DOptions"),
                QStringLiteral("actionEditCurveRange"),
                QStringLiteral("editCurveRange"),
                QStringLiteral("actionEditFunction"),
                QStringLiteral("editFunction"),
                QStringLiteral("actionColorPaletteEditor"),
                QStringLiteral("actionInterpolate"),
                QStringLiteral("actionFFT"),
                QStringLiteral("actionSmoothSavGol"),
                QStringLiteral("actionSmoothFFT"),
                QStringLiteral("actionSmoothAverage"),
                QStringLiteral("actionLowPassFilter"),
                QStringLiteral("actionHighPassFilter"),
                QStringLiteral("actionBandPassFilter"),
                QStringLiteral("actionBandBlockFilter"),
                QStringLiteral("actionMultiPeakGauss"),
                QStringLiteral("actionMultiPeakLorentz"),
                QStringLiteral("actionMultiPeakGaussAmp"),
                QStringLiteral("actionMultiPeakPseudoVoigt"),
                QStringLiteral("actionMultiPeakPearsonVII"),
                QStringLiteral("actionFind"),
                QStringLiteral("checkForUpdatesManual"),
                QStringLiteral("actionCheckUpdates"),
                QStringLiteral("searchForUpdates"),
                QStringLiteral("actionMergeGraphs"),
                QStringLiteral("mergeOpenGraphsDialog"),
                QStringLiteral("actionImportASCII"),
                QStringLiteral("actionShowLayerDialog"),
                QStringLiteral("showLayerDialog"),
                QStringLiteral("showScaleDialog"),
                QStringLiteral("showAxisDialog"),
                QStringLiteral("showPlotDialog"),
                QStringLiteral("showCurvesDialog"),
                QStringLiteral("showExportASCIIDialog"),
                QStringLiteral("exportLayer"),
                QStringLiteral("actionTensileYield"),
                QStringLiteral("showTensileYieldDialog"),
            };
            if (kBlockedInteractiveTargets.contains(id))
                return true;
            QString strippedMenu = menuText.trimmed();
            static const QRegularExpression rxMnemonic(
                    QStringLiteral("\\s*\\([A-Za-z0-9]\\)\\s*$"));
            strippedMenu.remove(rxMnemonic);
            strippedMenu = strippedMenu.trimmed();
            if (menuText.endsWith(QStringLiteral("..."))
                || menuText.endsWith(QStringLiteral("\u2026"))
                || strippedMenu.endsWith(QStringLiteral("..."))
                || strippedMenu.endsWith(QStringLiteral("\u2026"))) {
                return true;
            }
            static const QSet<QString> kBlockedJapaneseModalLabels = {
                QStringLiteral("\u30a6\u30a3\u30f3\u30c9\u30a6\u3092\u5370\u5237"),
                QStringLiteral("\u5370\u5237"),
                QStringLiteral("\u3059\u3079\u3066\u306e\u30d7\u30ed\u30c3\u30c8\u3092\u5370\u5237"),
                QStringLiteral("\u30d7\u30ed\u30c3\u30c8\u30a6\u30a3\u30b6\u30fc\u30c9"),
                QStringLiteral("\u30ec\u30a4\u30e4\u914d\u7f6e\u30fb\u30b5\u30a4\u30ba"),
                QStringLiteral("\u30a8\u30af\u30b9\u30dd\u30fc\u30c8"),
                QStringLiteral("\u3059\u3079\u3066\u306e\u30b0\u30e9\u30d5\u3092\u30a8\u30af\u30b9\u30dd\u30fc\u30c8"),
                QStringLiteral("PDF\u3078\u30a8\u30af\u30b9\u30dd\u30fc\u30c8"),
                QStringLiteral("\u30a4\u30f3\u30dd\u30fc\u30c8"),
                QStringLiteral("ASCII\u8aad\u307f\u8fbc\u307f"),
                QStringLiteral("\u30b0\u30e9\u30d5\u30a6\u30a3\u30f3\u30c9\u30a6\u3092\u7d50\u5408"),
                QStringLiteral("\u30b5\u30a4\u30ba"),
                QStringLiteral("\u30d7\u30ed\u30d1\u30c6\u30a3"),
                QStringLiteral("\u540d\u524d\u3092\u5909\u66f4"),
                QStringLiteral("\u691c\u7d22")
            };
            if (!strippedMenu.isEmpty() && kBlockedJapaneseModalLabels.contains(strippedMenu))
                return true;
            return false;
        };

        // Allow-list (not deny-list): RPC may only drive view/navigation/toggle commands and a few
        // harmless creation commands. Destructive or project-replacing commands (newProject,
        // deleteSelectedItems, clearTable, closeActiveWindow, ...) have dedicated RPC methods
        // with explicit `force` semantics and must never be reachable by name from here.
        auto isAllowedTriggerTarget = [](const QString &ident) -> bool {
            const QString id = ident.trimmed();
            if (id.isEmpty())
                return false;
            static const QSet<QString> kAllowedTriggerTargets = {
                QStringLiteral("actionNextWindow"),
                QStringLiteral("actionPrevWindow"),
                QStringLiteral("hideActiveWindow"),
                QStringLiteral("actionHideActiveWindow"),
                QStringLiteral("actionMinimizeWindow"),
                QStringLiteral("actionMaximizeWindow"),
                QStringLiteral("actionShowExplorer"),
                QStringLiteral("actionShowLog"),
                QStringLiteral("actionShowHistory"),
                QStringLiteral("actionTabbedWorkspaceView"),
                QStringLiteral("actionTogglePageView"),
                QStringLiteral("actionToggleMiniToolbar"),
                QStringLiteral("actionRecalcAuto"),
                QStringLiteral("actionRecalcManual"),
                QStringLiteral("actionRecalcDisabled"),
                QStringLiteral("actionRecalculateNow"),
                QStringLiteral("actionUnzoom"),
                QStringLiteral("actionShowAllCurves"),
                QStringLiteral("actionBoxPlot"),
                QStringLiteral("cascade"),
                QStringLiteral("hideAllFolderWindows"),
                QStringLiteral("addFolder"),
                QStringLiteral("actionToggleFocusHudMode"),
                QStringLiteral("toggleFocusHudMode"),
                QStringLiteral("actionToggleGraphInspector"),
                QStringLiteral("toggleGraphInspectorDock"),
                QStringLiteral("toggleTabbedWorkspaceView"),
                QStringLiteral("togglePageViewMode"),
                QStringLiteral("actionWorkspaceLayoutMdi"),
                QStringLiteral("actionWorkspaceLayoutSplit"),
                QStringLiteral("actionWorkspaceLayoutTabbed"),
                QStringLiteral("actionOpenPythonConsole"),
                QStringLiteral("actionPythonConsole"),
                QStringLiteral("openPythonConsole"),
                QStringLiteral("zoomIn"),
                QStringLiteral("zoomOut"),
                QStringLiteral("setRecalcModeAuto"),
                QStringLiteral("setRecalcModeManual"),
                QStringLiteral("setRecalcModeDisabled"),
            };
            if (kAllowedTriggerTargets.contains(id))
                return true;
            // Plot-from-selection commands are non-destructive (they only add a new graph).
            return id.startsWith(QStringLiteral("actionPlot"));
        };

        const bool nonInteractive = d_app->isNonInteractive()
                || ApplicationWindow::isNonInteractiveContext(d_app);

        const QList<QAction *> actions = d_app->findChildren<QAction *>();
        for (QAction *act : actions) {
            if (!act)
                continue;
            const QString cleanTxt = act->text().remove(QLatin1Char('&')).trimmed();
            if (act->objectName() == name || cleanTxt == name) {
                if (!isAllowedTriggerTarget(act->objectName())) {
                    throwInvalidParams(
                            QStringLiteral("Action '%1' is not in the RPC allow-list for app.trigger_action; use the dedicated JSON-RPC method instead.")
                                    .arg(act->objectName().isEmpty() ? cleanTxt : act->objectName()));
                }
                if (nonInteractive
                    && isBlockedInteractiveDialogTarget(act->objectName(), cleanTxt)) {
                    throwInvalidParams(
                            QStringLiteral("Interactive modal dialog action '%1' cannot be triggered headlessly via RPC; use the dedicated JSON-RPC method instead.")
                                    .arg(act->objectName().isEmpty() ? cleanTxt : act->objectName()));
                }
                if (params.contains(QStringLiteral("checked"))
                    && !params.value(QStringLiteral("checked")).isNull() && act->isCheckable()) {
                    const bool want = optionalJsonBool(params, QStringLiteral("checked"), false);
                    if (act->isChecked() != want)
                        act->trigger();
                } else {
                    if (params.contains(QStringLiteral("checked"))
                        && !params.value(QStringLiteral("checked")).isNull()) {
                        (void)optionalJsonBool(params, QStringLiteral("checked"), false);
                    }
                    act->trigger();
                }
                outResult[QStringLiteral("ok")] = true;
                outResult[QStringLiteral("triggered")] = QStringLiteral("QAction");
                outResult[QStringLiteral("name")] =
                        act->objectName().isEmpty() ? cleanTxt : act->objectName();
                if (act->isCheckable())
                    outResult[QStringLiteral("checked")] = act->isChecked();
                return true;
            }
        }

        const QMetaObject *mo = d_app->metaObject();
        for (int i = 0; i < mo->methodCount(); ++i) {
            const QMetaMethod mm = mo->method(i);
            if (QString::fromLatin1(mm.name()) == name && mm.parameterCount() == 0
                && (mm.methodType() == QMetaMethod::Slot
                    || mm.methodType() == QMetaMethod::Method)) {
                if (!isAllowedTriggerTarget(name)) {
                    throwInvalidParams(
                            QStringLiteral("Slot '%1' is not in the RPC allow-list for app.trigger_action; use the dedicated JSON-RPC method instead.")
                                    .arg(name));
                }
                if (nonInteractive && isBlockedInteractiveDialogTarget(name)) {
                    throwInvalidParams(
                            QStringLiteral("Interactive modal dialog slot '%1' cannot be invoked headlessly via RPC; use the dedicated JSON-RPC method instead.")
                                    .arg(name));
                }
                const bool ok = mm.invoke(d_app, Qt::DirectConnection);
                outResult[QStringLiteral("ok")] = ok;
                outResult[QStringLiteral("triggered")] = QStringLiteral("slot");
                outResult[QStringLiteral("name")] = name;
                return true;
            }
        }
        throwInvalidParams(QStringLiteral("No QAction or 0-arg slot named '%1' found.").arg(name));
    }

    // ------------------------------------------------------------------------
    // 1b. Workbooks (workbook.*)
    // ------------------------------------------------------------------------
    if (method.startsWith(QLatin1String("workbook."))) {
        QString wbErr;
        if (!WorkbookManager::instance(d_app)->handleRpc(method, params, outResult, &wbErr))
            throwInvalidParams(wbErr.isEmpty() ? QStringLiteral("%1 failed.").arg(method) : wbErr);
        return true;
    }

    // ------------------------------------------------------------------------
    // 2. Worksheet (table.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("table.create")) {
        const int rows = parseBoundedInt(params, QStringLiteral("rows"), 30, 1, 5000000);
        const int cols = parseBoundedInt(params, QStringLiteral("cols"), 2, 1, 4096);
        if (qint64(rows) * qint64(cols) > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("table.create total cells (%1x%2) exceeds limit (25,000,000).")
                            .arg(rows)
                            .arg(cols));
        }
        const QString reqName = optionalJsonString(params, QStringLiteral("name"));
        if (params.contains(QStringLiteral("columns"))
            && !params.value(QStringLiteral("columns")).isNull()) {
            if (!params.value(QStringLiteral("columns")).isArray())
                throwInvalidParams(QStringLiteral("Parameter 'columns' must be a list."));
            const QJsonArray cArr = params.value(QStringLiteral("columns")).toArray();
            for (int c = 0; c < cArr.size(); ++c) {
                const QJsonValue &cv = cArr[c];
                if (!cv.isString() && !cv.isObject()) {
                    throwInvalidParams(
                            QStringLiteral("Each entry in 'columns' must be a column name string or metadata object."));
                }
                if (cv.isObject() && c < cols)
                    validateColumnMetadataObject(cv.toObject());
            }
        }
        if (params.contains(QStringLiteral("col_names"))
            && !params.value(QStringLiteral("col_names")).isNull()) {
            if (!params.value(QStringLiteral("col_names")).isArray())
                throwInvalidParams(QStringLiteral("Parameter 'col_names' must be a list of strings."));
            for (const QJsonValue &nv : params.value(QStringLiteral("col_names")).toArray()) {
                if (!nv.isString())
                    throwInvalidParams(QStringLiteral("Each entry in 'col_names' must be a string."));
            }
        }
        if (params.contains(QStringLiteral("designations"))
            && !params.value(QStringLiteral("designations")).isNull()) {
            const QJsonValue dv = params.value(QStringLiteral("designations"));
            if (!dv.isArray() && !dv.isObject()) {
                throwInvalidParams(
                        QStringLiteral("Parameter 'designations' must be a list or object mapping column -> designation."));
            }
            if (dv.isArray()) {
                const QJsonArray dArr = dv.toArray();
                for (int c = 0; c < dArr.size(); ++c) {
                    const QJsonValue &item = dArr[c];
                    if (!item.isString())
                        throwInvalidParams(QStringLiteral("Each designation in 'designations' must be a string."));
                    if (c < cols) {
                        QJsonObject mo;
                        mo[QStringLiteral("plot_designation")] = item;
                        validateColumnMetadataObject(mo);
                    }
                }
            } else {
                const QJsonObject dObj = dv.toObject();
                for (auto it = dObj.begin(); it != dObj.end(); ++it) {
                    if (!it.value().isString())
                        throwInvalidParams(QStringLiteral("Each designation in 'designations' must be a string."));
                    QJsonObject mo;
                    mo[QStringLiteral("plot_designation")] = it.value();
                    validateColumnMetadataObject(mo);
                }
            }
        }
        Table *t = createUniqueTableHeadless(d_app, rows, cols, reqName);
        struct ScopedNewTableGuard
        {
            ApplicationWindow *app = nullptr;
            QPointer<Table> tbl;
            bool committed = false;
            ScopedNewTableGuard(ApplicationWindow *a, Table *table) : app(a), tbl(table) {}
            ~ScopedNewTableGuard()
            {
                if (!committed && tbl) {
                    tbl->askOnCloseEvent(false);
                    tbl->hide();
                    tbl->close();
                    if (tbl && tbl->folder())
                        app->closeWindow(tbl.data());
                    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                }
            }
        } tGuard(d_app, t);
        if (params.contains(QStringLiteral("columns"))
            && params.value(QStringLiteral("columns")).isArray()) {
            const QJsonArray cArr = params.value(QStringLiteral("columns")).toArray();
            for (int c = 0; c < std::min(cols, cArr.size()); ++c) {
                if (cArr[c].isString())
                    t->setColName(c, sanitizeUtf16Surrogates(cArr[c].toString()));
                else if (cArr[c].isObject())
                    applyColumnMetadataObject(t, c, cArr[c].toObject());
            }
        }
        if (params.contains(QStringLiteral("col_names"))
            && params.value(QStringLiteral("col_names")).isArray()) {
            const QJsonArray nArr = params.value(QStringLiteral("col_names")).toArray();
            for (int c = 0; c < std::min(cols, nArr.size()); ++c) {
                const QString nm = sanitizeUtf16Surrogates(nArr[c].toString()).trimmed();
                if (!nm.isEmpty())
                    t->setColName(c, nm);
            }
        }
        if (params.contains(QStringLiteral("designations"))
            && !params.value(QStringLiteral("designations")).isNull()) {
            const QJsonValue dv = params.value(QStringLiteral("designations"));
            if (dv.isArray()) {
                const QJsonArray dArr = dv.toArray();
                for (int c = 0; c < std::min(cols, dArr.size()); ++c) {
                    QJsonObject mo;
                    mo[QStringLiteral("plot_designation")] = dArr[c];
                    applyColumnMetadataObject(t, c, mo);
                }
            } else if (dv.isObject()) {
                const QJsonObject dObj = dv.toObject();
                for (auto it = dObj.begin(); it != dObj.end(); ++it) {
                    const int c = resolveColumnIndex(t, QJsonValue(it.key()), true);
                    QJsonObject mo;
                    mo[QStringLiteral("plot_designation")] = it.value();
                    applyColumnMetadataObject(t, c, mo);
                }
            }
        }
        tGuard.committed = true;
        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.get_info")) {
        Table *t = resolveTable(d_app, params);
        outResult = serializeTableInfo(t);
        return true;
    }

    if (method == QStringLiteral("table.resize")) {
        Table *t = resolveTable(d_app, params);
        const int nextRows = (params.contains(QStringLiteral("rows"))
                              && !params.value(QStringLiteral("rows")).isNull())
                ? parseBoundedInt(params, QStringLiteral("rows"), t->numRows(), 1, 5000000)
                : t->numRows();
        const int nextCols = (params.contains(QStringLiteral("cols"))
                              && !params.value(QStringLiteral("cols")).isNull())
                ? parseBoundedInt(params, QStringLiteral("cols"), t->numCols(), 1, 4096)
                : t->numCols();
        if (qint64(nextRows) * qint64(nextCols) > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("table.resize total cells (%1x%2) exceeds limit (25,000,000).")
                            .arg(nextRows)
                            .arg(nextCols));
        }
        {
            // rows and columns change in one Undo step
            ScopedRpcUndoMacro resizeMacro(t->d_future_table,
                                           QObject::tr("%1: resize").arg(t->name()));
            if (nextRows != t->numRows())
                t->setNumRows(nextRows);
            if (nextCols != t->numCols())
                t->setNumCols(nextCols);
        }
        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.add_column")) {
        Table *t = resolveTable(d_app, params);
        if (t->numCols() >= 4096
            || qint64(t->numRows()) * qint64(t->numCols() + 1) > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("table.add_column exceeds column/cell limit (max 4,096 cols, 25,000,000 cells)."));
        }
        (void)optionalJsonString(params, QStringLiteral("name"));
        (void)optionalJsonString(params, QStringLiteral("long_name"));
        (void)optionalJsonString(params, QStringLiteral("units"));
        (void)optionalJsonString(params, QStringLiteral("comment"));
        const QString modeStr = optionalJsonString(params, QStringLiteral("mode")).trimmed();
        const QString pdRaw =
                optionalJsonString(params, QStringLiteral("plot_designation")).trimmed();
        const SciDAVis::PlotDesignation pd = parsePlotDesignation(pdRaw, SciDAVis::Y);
        const QJsonValue initData = params.contains(QStringLiteral("data"))
                ? params.value(QStringLiteral("data"))
                : params.value(QStringLiteral("values"));
        if (!initData.isUndefined() && !initData.isNull()) {
            if (!initData.isArray())
                throwInvalidParams(QStringLiteral("'data' (or 'values') must be a JSON array."));
            const QJsonArray initArr = initData.toArray();
            if (initArr.size() > 5000000
                || qint64(std::max(t->numRows(), initArr.size())) * qint64(t->numCols() + 1)
                        > 25000000LL) {
                throwInvalidParams(
                        QStringLiteral("Column array size exceeds 5,000,000 rows / 25,000,000 cells."));
            }
        }
        validateColumnMetadataObject(params);
        struct ScopedFutureTableMacro
        {
            future::Table *ft = nullptr;
            ScopedFutureTableMacro(future::Table *f, const QString &lbl) : ft(f)
            {
                if (ft)
                    ft->beginMacro(lbl);
            }
            ~ScopedFutureTableMacro()
            {
                if (ft)
                    ft->endMacro();
            }
        } addColMacro(t->d_future_table, QStringLiteral("Add Column"));

        t->addCol(pd);
        const int newIdx = t->numCols() - 1;
        applyColumnMetadataObject(t, newIdx, params);
        if (initData.isArray()) {
            writeBulkColumnArray(t, newIdx, initData.toArray(), modeStr);
        }
        outResult = serializeColumnMeta(t, newIdx);
        outResult[QStringLiteral("table")] = t->name();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.remove_column")) {
        Table *t = resolveTable(d_app, params);
        if (t->numCols() <= 1)
            throwInvalidParams(QStringLiteral("Cannot remove the last remaining column."));
        const int idx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        const QString full = fullColumnName(t, idx);
        t->removeCol(QStringList() << full);
        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.set_column_data")) {
        Table *t = resolveTable(d_app, params);
        const int idx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        const QJsonValue rawData = params.contains(QStringLiteral("data"))
                ? params.value(QStringLiteral("data"))
                : params.value(QStringLiteral("values"));
        if (!rawData.isArray())
            throwInvalidParams(QStringLiteral("'data' (or 'values') must be a JSON array."));
        const QJsonArray arr = rawData.toArray();
        if (arr.size() > 5000000
            || qint64(std::max(t->numRows(), arr.size())) * qint64(t->numCols()) > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("Column data length (%1) exceeds limits (max 5,000,000 rows, 25,000,000 cells).")
                            .arg(arr.size()));
        }
        validateColumnMetadataObject(params);
        struct ScopedSetColDataMacro
        {
            future::Table *ft = nullptr;
            ScopedSetColDataMacro(future::Table *f, const QString &lbl) : ft(f)
            {
                if (ft && ft->undoStack())
                    ft->beginMacro(lbl);
                else
                    ft = nullptr;
            }
            ~ScopedSetColDataMacro()
            {
                if (ft)
                    ft->endMacro();
            }
        } setColMacro(t->d_future_table, QStringLiteral("Set Column Data"));
        writeBulkColumnArray(t, idx, arr, optionalJsonString(params, QStringLiteral("mode")));
        applyColumnMetadataObject(t, idx, params);
        d_app->updateCurves(t, fullColumnName(t, idx));
        outResult = serializeColumnMeta(t, idx);
        outResult[QStringLiteral("rows")] = t->numRows();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.get_column_data")) {
        Table *t = resolveTable(d_app, params);
        const int idx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        Column *col = t->column(idx);
        const int n = t->numRows();
        const int startRow = parseBoundedInt(params, QStringLiteral("start_row"), 0, 0,
                                             std::max(0, n - 1));
        const int endRow = parseBoundedInt(params, QStringLiteral("end_row"),
                                           std::max(0, n - 1), 0, std::max(0, n - 1));
        const int r0 = (n > 0) ? std::min(startRow, endRow) : 0;
        const int r1 = (n > 0) ? std::max(startRow, endRow) : -1;
        const bool compact = optionalJsonBool(params, QStringLiteral("compact"), false);
        const bool includeMaskArray =
                optionalJsonBool(params, QStringLiteral("include_masked"), !compact);
        const bool isNumericCol = col && col->columnMode() == SciDAVis::ColumnMode::Numeric;
        QJsonArray dataArr;
        QJsonArray maskArr;
        QJsonArray maskedRowsArr;
        for (int r = r0; r <= r1; ++r) {
            const bool isM = col ? col->isMasked(r) : false;
            if (isM)
                maskedRowsArr.append(r);
            if (includeMaskArray)
                maskArr.append(isM);
            if (!col || col->isInvalid(r)) {
                dataArr.append(QJsonValue::Null);
            } else if (isNumericCol) {
                dataArr.append(safeJsonDouble(col->valueAt(r)));
            } else {
                // text as stored; date/time, month and day columns as displayed (like export_csv)
                dataArr.append(columnCellToJson(col, r));
            }
        }
        outResult = serializeColumnMeta(t, idx);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = t->name();
        outResult[QStringLiteral("rows")] = n;
        outResult[QStringLiteral("data")] = dataArr;
        if (!compact)
            outResult[QStringLiteral("values")] = dataArr;
        if (includeMaskArray)
            outResult[QStringLiteral("masked")] = maskArr;
        outResult[QStringLiteral("masked_rows")] = maskedRowsArr;
        return true;
    }

    if (method == QStringLiteral("table.set_data_dict")) {
        const QJsonObject dataDict = params.value(QStringLiteral("data")).toObject();
        const QJsonArray orderedCols = params.value(QStringLiteral("column_order")).toArray();
        QStringList keys;
        if (!orderedCols.isEmpty()) {
            for (const QJsonValue &v : orderedCols) {
                const QString k = v.toString();
                if (dataDict.contains(k) && !keys.contains(k))
                    keys.append(k);
            }
        }
        for (auto it = dataDict.begin(); it != dataDict.end(); ++it) {
            if (!keys.contains(it.key()))
                keys.append(it.key());
        }
        if (keys.isEmpty())
            throwInvalidParams(QStringLiteral("'data' must be a non-empty object of column arrays."));

        int maxRows = 1;
        for (const QString &k : keys) {
            if (dataDict.value(k).isArray())
                maxRows = std::max(maxRows, dataDict.value(k).toArray().size());
        }
        if (keys.size() > 4096 || maxRows > 5000000
            || qint64(keys.size()) * qint64(maxRows) > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("table.set_data_dict dimensions (%1x%2) exceed limits (max 5,000,000 rows, 4,096 cols, 25,000,000 cells).")
                            .arg(maxRows)
                            .arg(keys.size()));
        }

        const QJsonObject metaMap = params.value(QStringLiteral("meta")).toObject();
        const QJsonObject unitsMap = params.value(QStringLiteral("units")).toObject();
        const QJsonObject longNamesMap = params.value(QStringLiteral("long_names")).toObject();
        const QJsonObject commentsMap = params.value(QStringLiteral("comments")).toObject();
        const QJsonObject desigMap = params.value(QStringLiteral("designations")).toObject();
        for (const QString &colKey : keys) {
            if (desigMap.contains(colKey)) {
                QJsonObject dm;
                dm[QStringLiteral("plot_designation")] = desigMap.value(colKey);
                validateColumnMetadataObject(dm);
            }
            if (metaMap.contains(colKey) && metaMap.value(colKey).isObject())
                validateColumnMetadataObject(metaMap.value(colKey).toObject());
        }

        Table *t = resolveTable(d_app, params, QStringLiteral("table"), false);
        const bool tableWasCreated = (t == nullptr);
        int preexistingCols = 0;
        // Column that receives each key. On an existing table a key that names one of its columns
        // writes to that column, and the other keys become new columns at the end; only when no
        // key names an existing column is the dict written over the columns from the left
        // (renaming them), as for a freshly created table.
        QVector<int> targetCol(keys.size(), -1);
        int appendedCols = 0;
        if (!tableWasCreated) {
            bool anyNameMatch = false;
            for (int k = 0; k < keys.size(); ++k) {
                for (int c = 0; c < t->numCols(); ++c) {
                    if (t->colLabel(c) == keys[k]) {
                        targetCol[k] = c;
                        anyNameMatch = true;
                        break;
                    }
                }
            }
            if (anyNameMatch) {
                for (int k = 0; k < keys.size(); ++k) {
                    if (targetCol[k] < 0)
                        targetCol[k] = t->numCols() + appendedCols++;
                }
            } else {
                for (int k = 0; k < keys.size(); ++k)
                    targetCol[k] = k;
                appendedCols = std::max(0, int(keys.size()) - t->numCols());
            }
        } else {
            for (int k = 0; k < keys.size(); ++k)
                targetCol[k] = k;
        }

        if (!t) {
            t = createUniqueTableHeadless(d_app, maxRows, keys.size(),
                                          params.value(QStringLiteral("name")).toString());
        } else {
            // The table only grows here, so check the final rows x cols footprint against the
            // shared 4,096-column / 25,000,000-cell caps before resizing anything.
            const qint64 finalRows = std::max<qint64>(t->numRows(), maxRows);
            const qint64 finalCols = qint64(t->numCols()) + appendedCols;
            const bool grows = finalRows > t->numRows() || finalCols > t->numCols();
            if (grows && (finalCols > 4096 || finalRows * finalCols > 25000000LL)) {
                throwInvalidParams(
                        QStringLiteral("table.set_data_dict would resize table '%1' to %2x%3, exceeding the 4,096-column / 25,000,000-cell limit.")
                                .arg(t->name())
                                .arg(finalRows)
                                .arg(finalCols));
            }
            preexistingCols = t->numCols();
        }

        // resizing, renaming and writing every column are one Undo step
        ScopedRpcUndoMacro dictMacro(t->d_future_table,
                                     QObject::tr("%1: set data").arg(t->name()));
        if (!tableWasCreated) {
            if (appendedCols > 0)
                t->setNumCols(preexistingCols + appendedCols);
            if (t->numRows() < maxRows)
                t->setNumRows(maxRows);
        }

        for (int k = 0; k < keys.size(); ++k) {
            const QString colKey = keys[k];
            const int c = targetCol[k];
            if (t->colLabel(c) != colKey)
                t->setColName(c, colKey);
            // Default X,Y,Y,... roles only for a freshly created table or newly appended columns;
            // an existing table keeps the roles the user (or table.create) already assigned.
            if (tableWasCreated || c >= preexistingCols)
                t->setColPlotDesignation(c, (c == 0) ? SciDAVis::X : SciDAVis::Y);
            QString modeHint;
            if (metaMap.contains(colKey) && metaMap.value(colKey).isObject()) {
                modeHint = metaMap.value(colKey).toObject().value(QStringLiteral("mode")).toString();
            }
            if (modeHint.trimmed().isEmpty()) {
                QString pdHint;
                if (metaMap.contains(colKey) && metaMap.value(colKey).isObject()) {
                    pdHint = metaMap.value(colKey)
                                     .toObject()
                                     .value(QStringLiteral("plot_designation"))
                                     .toString();
                }
                if (pdHint.trimmed().isEmpty() && desigMap.contains(colKey))
                    pdHint = desigMap.value(colKey).toString();
                if (pdHint.trimmed().compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0)
                    modeHint = QStringLiteral("text");
            }
            if (dataDict.value(colKey).isArray()) {
                writeBulkColumnArray(t, c, dataDict.value(colKey).toArray(), modeHint);
            }
            if (unitsMap.contains(colKey))
                t->setColUnits(c, unitsMap.value(colKey).toString());
            if (longNamesMap.contains(colKey))
                t->setColLongName(c, longNamesMap.value(colKey).toString());
            if (commentsMap.contains(colKey))
                t->setColComment(c, commentsMap.value(colKey).toString());
            if (desigMap.contains(colKey)) {
                QJsonObject dm;
                dm[QStringLiteral("plot_designation")] = desigMap.value(colKey);
                applyColumnMetadataObject(t, c, dm);
            }
            if (metaMap.contains(colKey) && metaMap.value(colKey).isObject()) {
                applyColumnMetadataObject(t, c, metaMap.value(colKey).toObject());
            }
            d_app->updateCurves(t, fullColumnName(t, c));
        }
        d_app->refreshAllGraphsAutoAxisTitles();
        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.paste_tsv")) {
        QString rawText;
        if (params.contains(QStringLiteral("text")) && !params.value(QStringLiteral("text")).isNull()) {
            rawText = optionalJsonString(params, QStringLiteral("text"));
        } else if (params.contains(QStringLiteral("tsv"))
                   && !params.value(QStringLiteral("tsv")).isNull()) {
            rawText = optionalJsonString(params, QStringLiteral("tsv"));
        } else if (QClipboard *cb = QApplication::clipboard()) {
            rawText = cb->text(QClipboard::Clipboard);
        }
        if (rawText.trimmed().isEmpty())
            throwInvalidParams(
                    QStringLiteral("table.paste_tsv requires non-empty 'text'/'tsv' (or clipboard text)."));

        const int startRow = parseBoundedInt(params, QStringLiteral("start_row"), 0, 0, 4999999);
        const int startCol = parseBoundedInt(params, QStringLiteral("start_col"), 0, 0, 4095);
        const bool hasHeader = params.contains(QStringLiteral("header"))
                ? optionalJsonBool(params, QStringLiteral("header"), true)
                : (startRow == 0);
        const bool convertNumeric = params.contains(QStringLiteral("numeric"))
                ? optionalJsonBool(params, QStringLiteral("numeric"), true)
                : true;
        const int unitsRowHint = parseUnitsRowHintParam(params);

        if (startRow == 0 && startCol == 0
            && (rawText.contains(QStringLiteral("*RAS_DATA_START"))
                || rawText.contains(QStringLiteral("*RAS_INT_START")))) {
            const QString encParam = optionalJsonString(params, QStringLiteral("encoding")).trimmed();
            const QString encHint =
                    (encParam.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0)
                    ? QString()
                    : encParam;
            std::unique_ptr<future::Table> rasFt(AsciiTableImportFilter::importLabTextData(
                    rawText.toUtf8(), QStringLiteral("RasPasteTmp"), QStringLiteral("paste.ras"),
                    optionalJsonString(params, QStringLiteral("separator")), encHint, unitsRowHint));
            if (!rasFt || rasFt->columnCount() <= 0) {
                throwInvalidParams(
                        QStringLiteral("Failed to parse Rigaku .ras data in table.paste_tsv."));
            }
            const int nRows = std::max(1, rasFt->rowCount());
            const int nCols = rasFt->columnCount();
            if (nRows > 5000000 || nCols > 4096 || qint64(nRows) * qint64(nCols) > 25000000LL) {
                throwInvalidParams(
                        QStringLiteral("Rigaku .ras data (%1x%2) exceeds worksheet cell limit (25,000,000).")
                                .arg(nRows)
                                .arg(nCols));
            }
            Table *t = nullptr;
            if ((params.contains(QStringLiteral("table"))
                 && !optionalJsonString(params, QStringLiteral("table")).trimmed().isEmpty())
                || (params.contains(QStringLiteral("window"))
                    && !optionalJsonString(params, QStringLiteral("window")).trimmed().isEmpty())) {
                t = resolveTable(d_app, params, QStringLiteral("table"), true);
            } else if (!optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                       || optionalJsonBool(params, QStringLiteral("create_table"), false)) {
                t = createUniqueTableHeadless(d_app, nRows, nCols,
                                              optionalJsonString(params, QStringLiteral("name")));
            } else {
                t = resolveTable(d_app, params, QStringLiteral("table"), false);
                if (!t) {
                    t = createUniqueTableHeadless(d_app, nRows, nCols,
                                                  optionalJsonString(params, QStringLiteral("name")));
                }
            }
            // growing, relabelling and filling the columns are one Undo step
            ScopedRpcUndoMacro rasPasteMacro(t->d_future_table,
                                             QObject::tr("%1: paste").arg(t->name()));
            // every name the table had before the paste (before the columns added below)
            const QStringList namesBeforeImport = t->colNames();
            if (t->numCols() < nCols)
                t->setNumCols(nCols);
            if (t->numRows() < nRows)
                t->setNumRows(nRows);
            // columns re-pointed through temporary names: references keep their names, except
            // names that are gone afterwards (propagateRelabeledColumnNames below)
            const QStringList oldNames = t->colNamesInRange(0, nCols);
            Table::RenamePropagationBlocker renameBlocker(t);
            for (int c = 0; c < nCols; ++c)
                t->setColName(c, QStringLiteral("__ras_paste_col_%1__").arg(c));
            for (int c = 0; c < nCols; ++c) {
                Column *srcCol = rasFt->column(c);
                Column *dstCol = t->column(c);
                if (!srcCol || !dstCol)
                    continue;
                dstCol->setColumnMode(srcCol->columnMode());
                dstCol->copy(srcCol);
                t->setColName(c, srcCol->name().trimmed().isEmpty() ? QString::number(c + 1)
                                                                    : srcCol->name());
                t->setColLongName(c, srcCol->longName());
                t->setColUnits(c, srcCol->units());
                t->setColComment(c, srcCol->comment());
                t->setColPlotDesignation(c, (c == 0) ? SciDAVis::X : SciDAVis::Y);
                applyDefaultNumericFormat6g(t, c);
                d_app->updateCurves(t, fullColumnName(t, c));
            }
            t->propagateRelabeledColumnNames(oldNames, namesBeforeImport);
            d_app->refreshAllGraphsAutoAxisTitles();
            outResult = serializeTableInfo(t);
            outResult[QStringLiteral("ok")] = true;
            return true;
        }

        QString sep = optionalJsonString(params, QStringLiteral("separator"));
        if (sep.isEmpty() || sep.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0) {
            sep = rawText.contains(QLatin1Char('\t'))
                    ? QStringLiteral("\t")
                    : autoDetectCsvSeparator(rawText, 0, QString());
            // one line without tab, comma or ';': a single cell ("2025-05-06 07:08", "Al 7075")
            // is not split at its blanks (an explicit separator=" " still splits it)
            const QString oneLine = rawText.trimmed();
            if (sep == QLatin1String(" ") && !oneLine.contains(QLatin1Char('\n'))
                && !oneLine.contains(QLatin1Char('\r')))
                sep = QStringLiteral("\t");
        }

        int maxCols = 1;
        const QVector<QVector<ParsedCsvCell>> records =
                parseDelimitedRecordsRfc4180(rawText, 0, sep, true, false, maxCols, false, false);
        const qint64 dataRowSpan = std::max<qint64>(1, records.size());
        const qint64 reqEndRows = qint64(startRow) + dataRowSpan;
        const qint64 reqEndCols = qint64(startCol) + std::max<qint64>(1, maxCols);
        if (reqEndRows > 5000000LL || reqEndCols > 4096LL
            || reqEndRows * reqEndCols > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("table.paste_tsv target bounds (%1 rows x %2 cols) exceed limits (max 5,000,000 rows, 4,096 cols, 25,000,000 cells).")
                            .arg(reqEndRows)
                            .arg(reqEndCols));
        }

        Table *t = nullptr;
        if ((params.contains(QStringLiteral("table"))
             && !optionalJsonString(params, QStringLiteral("table")).trimmed().isEmpty())
            || (params.contains(QStringLiteral("window"))
                && !optionalJsonString(params, QStringLiteral("window")).trimmed().isEmpty())) {
            t = resolveTable(d_app, params, QStringLiteral("table"), true);
        } else if (!optionalJsonString(params, QStringLiteral("name")).trimmed().isEmpty()
                   || optionalJsonBool(params, QStringLiteral("create_table"), false)) {
            t = createUniqueTableHeadless(d_app, 1, int(reqEndCols),
                                          optionalJsonString(params, QStringLiteral("name")));
        } else {
            t = resolveTable(d_app, params, QStringLiteral("table"), false);
            if (!t) {
                t = createUniqueTableHeadless(d_app, 1, int(reqEndCols),
                                              optionalJsonString(params, QStringLiteral("name")));
            }
        }
        const qint64 finalRows = std::max<qint64>(t ? t->numRows() : 1, reqEndRows);
        const qint64 finalCols = std::max<qint64>(t ? t->numCols() : 1, reqEndCols);
        if (finalRows > 5000000LL || finalCols > 4096LL || finalRows * finalCols > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("table.paste_tsv resulting worksheet (%1x%2 = %3 cells) exceeds 25,000,000 total cell limit.")
                            .arg(finalRows)
                            .arg(finalCols)
                            .arg(finalRows * finalCols));
        }

        const bool assignDesigs = (startRow == 0 && startCol == 0);
        QStringList pasteWarnings;
        {
            // growing the table, the temporary header names and the data are one Undo step
            ScopedRpcUndoMacro pasteMacro(t->d_future_table,
                                          QObject::tr("%1: paste").arg(t->name()));
            populateDelimitedRecordsIntoTable(
                    t, records, maxCols, hasHeader, false, unitsRowHint, convertNumeric, startRow,
                    startCol, assignDesigs,
                    detectImportDecimalComma(rawText, 0, sep, convertNumeric),
                    /*keepExistingColumnTypes=*/true, &pasteWarnings);
        }
        for (int c = startCol; c < std::min(t->numCols(), startCol + maxCols); ++c)
            d_app->updateCurves(t, fullColumnName(t, c));
        d_app->refreshAllGraphsAutoAxisTitles();

        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        if (!pasteWarnings.isEmpty())
            outResult[QStringLiteral("warnings")] = QJsonArray::fromStringList(pasteWarnings);
        return true;
    }

    if (method == QStringLiteral("table.set_cell")) {
        Table *t = resolveTable(d_app, params);
        if (!params.contains(QStringLiteral("row")) || params.value(QStringLiteral("row")).isNull())
            throwInvalidParams(QStringLiteral("Parameter 'row' is required."));
        const int row = parseBoundedInt(params, QStringLiteral("row"), 0, 0, 4999999);
        const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        if (qint64(std::max(t->numRows(), row + 1)) * qint64(t->numCols()) > 25000000LL) {
            throwInvalidParams(
                    QStringLiteral("Row %1 exceeds 25,000,000 total worksheet cells.").arg(row));
        }
        // growing the table, converting the column type and writing the cell are one Undo step
        ScopedRpcUndoMacro setCellMacro(t->d_future_table,
                                        QObject::tr("%1: change cell value(s)").arg(t->name()));
        if (row >= t->numRows())
            t->setNumRows(row + 1);
        Column *col = t->column(colIdx);
        const QJsonValue val = params.value(QStringLiteral("value"));
        if (val.isNull() || val.isUndefined()) {
            t->clearCell(row, colIdx);
        } else if (col && isCalendarColumnMode(col->columnMode()) && val.isDouble()) {
            // a number in a date/time, month or day column is a day count / month / weekday
            // number, converted like set_column_data(mode=...) with numbers -- not text that the
            // input filter would read as a time ("3" -> 03:00:00)
            const QDateTime dt = calendarDateTimeFromNumber(col, val.toDouble());
            if (dt.isValid())
                col->setDateTimeAt(row, dt);
            else
                t->clearCell(row, colIdx);
        } else if (double days = 0.0; col && val.isString()
                   && dateTimeTextAsDayCount(col, val.toString().trimmed(), false, days)) {
            // a number written as text in a date/time column is the same day count as the
            // number itself (and as table.paste_tsv of that text)
            const QDateTime dt = calendarDateTimeFromNumber(col, days);
            if (dt.isValid())
                col->setDateTimeAt(row, dt);
            else
                t->clearCell(row, colIdx);
        } else if (col && isCalendarColumnMode(col->columnMode())) {
            // date/time, month and day columns keep their type: the value goes through the
            // column's input filter, as when it is typed into the cell
            t->setText(row, colIdx,
                       val.isString() ? val.toString() : QString::number(val.toDouble(), 'g', 15));
        } else if (col && col->columnMode() == SciDAVis::ColumnMode::Text) {
            col->setTextAt(row,
                           val.isString() ? val.toString()
                                          : QString::number(val.toDouble(), 'g',
                                                            QLocale::FloatingPointShortest));
        } else if (val.isString()) {
            const QString sTrim = val.toString().trimmed();
            // the missing / Excel error tokens of the text import and table.paste_tsv ("N.D.",
            // "ND", "#N/A", "-", "NaN", ...) are a missing value of the numeric column
            if (isImportMissingNumericCell(sTrim, false)) {
                t->clearCell(row, colIdx);
            } else {
                bool ok = false;
                const double d = sTrim.toDouble(&ok);
                if (ok && std::isfinite(d)) {
                    t->setCell(row, colIdx, d);
                } else {
                    // a numeric column becomes text, its other rows keep their numbers exactly
                    convertNumericColumnToTextExactly(col);
                    t->setText(row, colIdx, val.toString());
                }
            }
        } else {
            const double d = jsonToDoubleOrNaN(val);
            if (std::isfinite(d))
                t->setCell(row, colIdx, d);
            else
                t->clearCell(row, colIdx);
        }
        d_app->updateCurves(t, fullColumnName(t, colIdx));
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.get_cell")) {
        Table *t = resolveTable(d_app, params);
        if (!params.contains(QStringLiteral("row")) || params.value(QStringLiteral("row")).isNull())
            throwInvalidParams(QStringLiteral("Parameter 'row' is required."));
        const int row = parseStrictJsonInt(params.value(QStringLiteral("row")),
                                           QStringLiteral("row"));
        const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        if (row < 0 || row >= t->numRows())
            throwInvalidParams(QStringLiteral("Row index out of range: %1").arg(row));
        Column *col = t->column(colIdx);
        outResult[QStringLiteral("row")] = row;
        outResult[QStringLiteral("col")] = colIdx;
        outResult[QStringLiteral("masked")] = col ? col->isMasked(row) : false;
        outResult[QStringLiteral("invalid")] = col ? col->isInvalid(row) : true;
        outResult[QStringLiteral("value")] = columnCellToJson(col, row);
        outResult[QStringLiteral("text")] = t->text(row, colIdx);
        return true;
    }

    if (method == QStringLiteral("table.set_column_meta")
        || method == QStringLiteral("table.set_col_metadata")) {
        Table *t = resolveTable(d_app, params);
        const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        applyColumnMetadataObject(t, colIdx, params);
        d_app->refreshAllGraphsAutoAxisTitles();
        outResult = serializeColumnMeta(t, colIdx);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.get_column_meta")) {
        Table *t = resolveTable(d_app, params);
        const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        outResult = serializeColumnMeta(t, colIdx);
        return true;
    }

    if (method == QStringLiteral("table.set_formula")) {
        Table *t = resolveTable(d_app, params);
        const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        const QString formula = optionalJsonString(params, QStringLiteral("formula"));
        const bool doRecalc = optionalJsonBool(params, QStringLiteral("recalculate"), true);
        const bool hasFormula = !formula.trimmed().isEmpty();
        // Table::setCommand evaluates the column through the worksheets' dependency graph (a
        // circular formula gets a formula error and is not evaluated), but only while automatic
        // recalculation is on. In manual mode, recalculate=true evaluates this column alone the
        // same way; the other formula columns (and worksheets) stay as they are, as in manual
        // mode. Setting and evaluating the formula are one Undo step.
        const bool wasAuto = t->autoRecalcFormulas();
        const bool forceEval = doRecalc && hasFormula && !wasAuto && t->d_future_table;
        {
            ScopedRpcUndoMacro formulaMacro(
                    forceEval ? t->d_future_table : nullptr,
                    QObject::tr("%1: apply formula to column").arg(t->name()));
            t->setCommand(colIdx, formula);
            if (forceEval)
                t->recalculateFormulaColumnsChecked(QVector<int>{ colIdx });
        }
        const bool evaluated = hasFormula && (wasAuto || forceEval);
        Column *fcol = t->column(colIdx);
        const QString fErr = (evaluated && fcol) ? fcol->formulaError() : QString();
        if (evaluated)
            d_app->updateCurves(t, fullColumnName(t, colIdx));
        outResult[QStringLiteral("ok")] = fErr.isEmpty();
        outResult[QStringLiteral("col")] = t->colLabel(colIdx);
        outResult[QStringLiteral("formula")] = formula;
        outResult[QStringLiteral("formula_error")] = fErr;
        return true;
    }

    if (method == QStringLiteral("table.recalculate")) {
        Table *t = resolveTable(d_app, params);
        int onlyCol = -1;
        if (params.contains(QStringLiteral("col")) && !params.value(QStringLiteral("col")).isNull())
            onlyCol = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        // the formula columns of this worksheet (or only `col`), in dependency order; circular
        // ones are marked. Other worksheets are not evaluated, whatever their mode.
        QVector<int> evalCols;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (col && !col->formulaIntervals().isEmpty() && (onlyCol < 0 || c == onlyCol))
                evalCols.append(c);
        }
        t->recalculateFormulaColumnsChecked(evalCols);
        bool allOk = true;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col || col->formula(0).trimmed().isEmpty())
                continue;
            if (onlyCol < 0 || c == onlyCol)
                d_app->updateCurves(t, fullColumnName(t, c));
            if ((onlyCol < 0 || c == onlyCol) && !col->formulaError().isEmpty())
                allOk = false;
        }
        outResult[QStringLiteral("ok")] = allOk;
        outResult[QStringLiteral("table")] = t->name();
        if (onlyCol >= 0) {
            outResult[QStringLiteral("formula_error")] =
                    t->column(onlyCol) ? t->column(onlyCol)->formulaError() : QString();
        }
        outResult[QStringLiteral("formula_errors")] = formulaErrorsJson(QVector<Table *>{ t });
        return true;
    }

    if (method == QStringLiteral("table.mask_rows")
        || method == QStringLiteral("table.unmask_rows")) {
        const bool maskFlag = (method == QStringLiteral("table.mask_rows"));
        Table *t = resolveTable(d_app, params);
        QList<int> targetCols;
        if (params.contains(QStringLiteral("col")) && !params.value(QStringLiteral("col")).isNull()) {
            targetCols << resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        } else {
            for (int c = 0; c < t->numCols(); ++c)
                targetCols << c;
        }
        QVector<int> rowsToChange;
        if (params.value(QStringLiteral("rows")).isArray()) {
            for (const QJsonValue &rv : params.value(QStringLiteral("rows")).toArray()) {
                const int r = rv.toInt(-1);
                if (r >= 0 && r < t->numRows())
                    rowsToChange.append(r);
            }
        } else if (params.contains(QStringLiteral("start_row"))
                   || params.contains(QStringLiteral("end_row"))) {
            const int r0 = std::max(0, params.value(QStringLiteral("start_row")).toInt(0));
            const int r1 = std::min(t->numRows() - 1,
                                    params.value(QStringLiteral("end_row")).toInt(t->numRows() - 1));
            for (int r = r0; r <= r1; ++r)
                rowsToChange.append(r);
        } else if (!maskFlag
                   && (!params.contains(QStringLiteral("rows"))
                       || params.value(QStringLiteral("rows")).isNull())) {
            for (int r = 0; r < t->numRows(); ++r)
                rowsToChange.append(r);
        }
        bool macroStarted = false;
        int actualChanged = 0;
        QSet<int> dirtyCols;
        // The runs of consecutive rows that change are set with one command per column
        // (Column::setMaskedIntervals): one setMasked(Interval) per run copied all the masks of
        // the column for each command, so scattered rows took time and memory growing with the
        // square of the runs (over 4 minutes and 6.5 GB for 25000 runs). A row listed twice
        // counts once.
        QVector<int> sortedRows = rowsToChange;
        std::sort(sortedRows.begin(), sortedRows.end());
        sortedRows.erase(std::unique(sortedRows.begin(), sortedRows.end()), sortedRows.end());
        for (int c : targetCols) {
            if (Column *col = t->column(c)) {
                // whether each listed row is masked now, in one pass over the sorted masks
                // (isMasked() per row scans all the masks)
                QList<Interval<int>> masks = col->maskedIntervals();
                std::sort(masks.begin(), masks.end(),
                          [](const Interval<int> &a, const Interval<int> &b) {
                              return a.start() < b.start();
                          });
                QList<Interval<int>> runs;
                int m = 0;
                int maskedUpTo = -1; // largest end of the masks starting at or before the row
                for (int r : sortedRows) {
                    while (m < masks.size() && masks.at(m).start() <= r)
                        maskedUpTo = std::max(maskedUpTo, masks.at(m++).end());
                    if ((maskedUpTo >= r) == maskFlag)
                        continue;
                    if (!runs.isEmpty() && runs.last().end() + 1 == r)
                        runs.last().setEnd(r);
                    else
                        runs.append(Interval<int>(r, r));
                    ++actualChanged;
                }
                if (runs.isEmpty())
                    continue;
                if (!macroStarted && t->d_future_table) {
                    t->d_future_table->beginMacro(QStringLiteral("%1: mask").arg(t->name()));
                    macroStarted = true;
                }
                col->setMaskedIntervals(runs, maskFlag);
                dirtyCols.insert(c);
            }
        }
        if (macroStarted && t->d_future_table)
            t->d_future_table->endMacro();
        for (int c : dirtyCols)
            d_app->updateCurves(t, fullColumnName(t, c));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("modified_rows")] = rowsToChange.size();
        outResult[QStringLiteral("changed_cells")] = actualChanged;
        return true;
    }

    if (method == QStringLiteral("table.clear_masks")) {
        Table *t = resolveTable(d_app, params);
        if (params.contains(QStringLiteral("col")) && !params.value(QStringLiteral("col")).isNull()) {
            const int c = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
            if (Column *col = t->column(c)) {
                bool anyMasked = false;
                for (int r = 0; r < t->numRows(); ++r) {
                    if (col->isMasked(r)) {
                        anyMasked = true;
                        break;
                    }
                }
                if (anyMasked) {
                    if (t->d_future_table)
                        t->d_future_table->beginMacro(
                                QStringLiteral("%1: clear masks").arg(t->name()));
                    col->clearMasks();
                    if (t->d_future_table)
                        t->d_future_table->endMacro();
                }
            }
            d_app->updateCurves(t, fullColumnName(t, c));
        } else {
            bool macroStarted = false;
            QVector<int> clearedCols;
            for (int c = 0; c < t->numCols(); ++c) {
                if (Column *col = t->column(c)) {
                    bool anyMasked = false;
                    for (int r = 0; r < t->numRows(); ++r) {
                        if (col->isMasked(r)) {
                            anyMasked = true;
                            break;
                        }
                    }
                    if (anyMasked) {
                        if (!macroStarted && t->d_future_table) {
                            t->d_future_table->beginMacro(
                                    QStringLiteral("%1: clear masks").arg(t->name()));
                            macroStarted = true;
                        }
                        col->clearMasks();
                        clearedCols.append(c);
                    }
                }
            }
            if (macroStarted && t->d_future_table)
                t->d_future_table->endMacro();
            for (int c : clearedCols)
                d_app->updateCurves(t, fullColumnName(t, c));
        }
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.get_masked_rows")) {
        Table *t = resolveTable(d_app, params);
        int c = 0;
        if (params.contains(QStringLiteral("col")) && !params.value(QStringLiteral("col")).isNull())
            c = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        else
            c = (t->numCols() >= 2) ? 1 : 0;
        Column *col = t->column(c);
        QJsonArray masked;
        for (int r = 0; r < t->numRows(); ++r) {
            if (col && col->isMasked(r))
                masked.append(r);
        }
        outResult[QStringLiteral("col")] = t->colLabel(c);
        outResult[QStringLiteral("count")] = masked.size();
        outResult[QStringLiteral("rows")] = masked;
        outResult[QStringLiteral("masked_rows")] = masked;
        return true;
    }

    if (method == QStringLiteral("table.sort")) {
        Table *t = resolveTable(d_app, params);
        QJsonValue leadSpec(0);
        if (params.contains(QStringLiteral("by")) && !params.value(QStringLiteral("by")).isNull())
            leadSpec = params.value(QStringLiteral("by"));
        else if (params.contains(QStringLiteral("by_col"))
                 && !params.value(QStringLiteral("by_col")).isNull())
            leadSpec = params.value(QStringLiteral("by_col"));
        else if (params.contains(QStringLiteral("col"))
                 && !params.value(QStringLiteral("col")).isNull())
            leadSpec = params.value(QStringLiteral("col"));
        const int leadIdx = resolveColumnIndex(t, leadSpec, true);
        const bool ascending = params.contains(QStringLiteral("ascending"))
                ? params.value(QStringLiteral("ascending")).toBool(true)
                : true;
        const bool onlyCol = params.contains(QStringLiteral("only_column"))
                ? params.value(QStringLiteral("only_column")).toBool(false)
                : params.value(QStringLiteral("separately")).toBool(false);
        QList<Column *> colsToSort;
        if (onlyCol) {
            colsToSort << t->column(leadIdx);
        } else {
            for (int c = 0; c < t->numCols(); ++c)
                colsToSort << t->column(c);
        }
        if (t->d_future_table)
            t->d_future_table->sortColumns(t->column(leadIdx), colsToSort, ascending);
        for (int c = 0; c < t->numCols(); ++c)
            d_app->updateCurves(t, fullColumnName(t, c));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = t->name();
        return true;
    }

    if (method == QStringLiteral("table.column_stats")) {
        Table *t = resolveTable(d_app, params);
        const bool ignoreMasks = params.contains(QStringLiteral("ignore_masked"))
                ? params.value(QStringLiteral("ignore_masked")).toBool(true)
                : true;
        if (params.contains(QStringLiteral("col")) && !params.value(QStringLiteral("col")).isNull()) {
            const int c = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
            Column *col = t->column(c);
            std::vector<double> vals;
            if (col && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                for (int r = 0; r < t->numRows(); ++r) {
                    if (col->isInvalid(r) || (ignoreMasks && col->isMasked(r)))
                        continue;
                    vals.push_back(col->valueAt(r));
                }
            }
            outResult = computeNumericStatsJson(std::move(vals));
            outResult[QStringLiteral("col")] = t->colLabel(c);
            return true;
        }
        QJsonObject perCol;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric)
                continue;
            std::vector<double> vals;
            for (int r = 0; r < t->numRows(); ++r) {
                if (col->isInvalid(r) || (ignoreMasks && col->isMasked(r)))
                    continue;
                vals.push_back(col->valueAt(r));
            }
            perCol[t->colLabel(c)] = computeNumericStatsJson(std::move(vals));
        }
        outResult[QStringLiteral("table")] = t->name();
        outResult[QStringLiteral("columns")] = perCol;
        return true;
    }

    if (method == QStringLiteral("table.row_stats")) {
        Table *t = resolveTable(d_app, params);
        QVector<int> targetRows;
        if (params.value(QStringLiteral("rows")).isArray()) {
            for (const QJsonValue &rv : params.value(QStringLiteral("rows")).toArray()) {
                const int r = parseStrictJsonInt(rv, QStringLiteral("rows"));
                if (r >= 0 && r < t->numRows())
                    targetRows.append(r);
            }
        } else if (params.contains(QStringLiteral("start_row"))
                   || params.contains(QStringLiteral("end_row"))) {
            const int r0 = parseBoundedInt(params, QStringLiteral("start_row"), 0, 0,
                                           std::max(0, t->numRows() - 1));
            const int r1 = parseBoundedInt(params, QStringLiteral("end_row"),
                                           std::max(0, t->numRows() - 1), 0,
                                           std::max(0, t->numRows() - 1));
            for (int r = std::min(r0, r1); r <= std::max(r0, r1); ++r)
                targetRows.append(r);
        } else {
            for (int r = 0; r < t->numRows(); ++r)
                targetRows.append(r);
        }
        QJsonArray rowStatsArr;
        for (int r : targetRows) {
            std::vector<double> vals;
            for (int c = 0; c < t->numCols(); ++c) {
                Column *col = t->column(c);
                if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric
                    || col->isInvalid(r) || col->isMasked(r))
                    continue;
                vals.push_back(col->valueAt(r));
            }
            QJsonObject st = computeNumericStatsJson(std::move(vals));
            st[QStringLiteral("row")] = r;
            rowStatsArr.append(st);
        }
        outResult[QStringLiteral("table")] = t->name();
        outResult[QStringLiteral("rows")] = rowStatsArr;
        return true;
    }

    if (method == QStringLiteral("table.export_csv")) {
        Table *t = resolveTable(d_app, params);
        const QString path = params.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty())
            throwInvalidParams(QStringLiteral("Output 'path' is required."));
        const QFileInfo fi(path);
        const QString absPath = fi.absoluteFilePath();
        if (fi.exists() && fi.isDir()) {
            throwInvalidParams(
                    QStringLiteral("Export 'path' is an existing directory: %1").arg(absPath));
        }
        if (!fi.dir().exists() && !QDir().mkpath(fi.dir().absolutePath())) {
            throwRpcRuntime(QStringLiteral("Failed to create export directory '%1'.")
                                    .arg(fi.dir().absolutePath()));
        }
        QString sep = params.value(QStringLiteral("separator")).toString();
        if (sep.isEmpty()) {
            sep = absPath.endsWith(QStringLiteral(".tsv"), Qt::CaseInsensitive)
                    ? QStringLiteral("\t")
                    : QStringLiteral(",");
        }
        const bool withLabels = params.contains(QStringLiteral("header"))
                ? params.value(QStringLiteral("header")).toBool(true)
                : (params.contains(QStringLiteral("with_labels"))
                           ? params.value(QStringLiteral("with_labels")).toBool(true)
                           : true);
        const bool withComments = params.value(QStringLiteral("with_comments")).toBool(false);

        // Same quoting rule as Table::exportASCII (also quotes "Al,Zn" with separator ", ").
        auto quoteField = [&sep](const QString &field) -> QString {
            return Table::quoteCsvField(field, sep);
        };

        QFile outFile(absPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throwRpcRuntime(QStringLiteral("Failed to open '%1' for CSV export: %2")
                                    .arg(absPath, outFile.errorString()));
        }

        const int rows = t->numRows();
        const int cols = t->numCols();
        QLocale cLoc = QLocale::c();
        cLoc.setNumberOptions(QLocale::OmitGroupSeparator);

        auto writeUtf8Line = [&outFile, &absPath](const QString &line) {
            const QByteArray utf8 = line.toUtf8() + '\n';
            if (outFile.write(utf8) != utf8.size()) {
                throwRpcRuntime(QStringLiteral("Failed while writing CSV data to '%1': %2")
                                        .arg(absPath, outFile.errorString()));
            }
        };

        if (withLabels && cols > 0) {
            QStringList hdrFields;
            hdrFields.reserve(cols);
            for (int c = 0; c < cols; ++c)
                hdrFields << quoteField(t->colLabel(c));
            writeUtf8Line(hdrFields.join(sep));
        }

        if (withComments && cols > 0) {
            QStringList cmtFields;
            cmtFields.reserve(cols);
            for (int c = 0; c < cols; ++c)
                cmtFields << quoteField(t->colComment(c));
            writeUtf8Line(cmtFields.join(sep));
        }

        for (int r = 0; r < rows; ++r) {
            QStringList rowFields;
            rowFields.reserve(cols);
            for (int c = 0; c < cols; ++c) {
                Column *col = t->column(c);
                if (!col || col->isInvalid(r)) {
                    rowFields << QString();
                } else if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    const double v = col->valueAt(r);
                    // shortest text that reads back as exactly the same double, as the GUI
                    // export (Table::exportASCII); 15 digits lost 0.1+0.2, 1/3, ...
                    rowFields << (std::isfinite(v)
                                          ? cLoc.toString(v, 'g', QLocale::FloatingPointShortest)
                                          : QString());
                } else if (col->columnMode() == SciDAVis::ColumnMode::Text) {
                    rowFields << quoteField(col->textAt(r));
                } else {
                    rowFields << quoteField(col->asStringColumn()->textAt(r));
                }
            }
            writeUtf8Line(rowFields.join(sep));
        }

        if (!outFile.flush()) {
            throwRpcRuntime(QStringLiteral("Failed to flush exported CSV file '%1': %2")
                                    .arg(absPath, outFile.errorString()));
        }
        outFile.close();

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("path")] = absPath;
        outResult[QStringLiteral("rows")] = rows;
        outResult[QStringLiteral("cols")] = cols;
        outResult[QStringLiteral("bytes")] = QFileInfo(absPath).size();
        return true;
    }

    if (method == QStringLiteral("table.to_matrix")) {
        Table *t = resolveTable(d_app, params);
        const int rows = t->numRows();
        const int cols = t->numCols();
        if (rows > 4096 || cols > 4096 || qint64(rows) * qint64(cols) > 16777216LL) {
            throwInvalidParams(
                    QStringLiteral("table.to_matrix dimensions (%1x%2) exceed matrix limits (max 4,096x4,096).")
                            .arg(rows)
                            .arg(cols));
        }
        Matrix *m = createUniqueMatrixHeadless(
                d_app, rows, cols, optionalJsonString(params, QStringLiteral("name")));
        const double qNaN = std::numeric_limits<double>::quiet_NaN();
        for (int c = 0; c < cols; ++c) {
            Column *col = t->column(c);
            QVector<qreal> colVec(rows, qNaN);
            for (int r = 0; r < rows; ++r) {
                if (col && !col->isInvalid(r)
                    && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    const double v = col->valueAt(r);
                    colVec[r] = std::isfinite(v) ? v : qNaN;
                }
            }
            m->d_future_matrix->setColumnCells(c, 0, rows - 1, colVec);
        }
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    // ------------------------------------------------------------------------
    // 3. Matrix (matrix.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("matrix.create")) {
        const int rows = parseBoundedInt(params, QStringLiteral("rows"), 32, 1, 4096);
        const int cols = parseBoundedInt(params, QStringLiteral("cols"), 32, 1, 4096);
        if (qint64(rows) * qint64(cols) > 16777216LL) {
            throwInvalidParams(
                    QStringLiteral("matrix.create total cells (%1x%2) exceeds limit (16,777,216).")
                            .arg(rows)
                            .arg(cols));
        }
        double xMin = 1.0, xMax = 10.0, yMin = 1.0, yMax = 10.0;
        const bool hasCoords = params.contains(QStringLiteral("x_min"))
                || params.contains(QStringLiteral("x_max"))
                || params.contains(QStringLiteral("y_min"))
                || params.contains(QStringLiteral("y_max"));
        if (hasCoords) {
            xMin = optionalJsonFiniteDouble(params, QStringLiteral("x_min"), 1.0);
            xMax = optionalJsonFiniteDouble(params, QStringLiteral("x_max"), 10.0);
            yMin = optionalJsonFiniteDouble(params, QStringLiteral("y_min"), 1.0);
            yMax = optionalJsonFiniteDouble(params, QStringLiteral("y_max"), 10.0);
            validateFiniteCoordRange(xMin, xMax, QStringLiteral("Matrix X coordinates"));
            validateFiniteCoordRange(yMin, yMax, QStringLiteral("Matrix Y coordinates"));
        }
        Matrix *m = createUniqueMatrixHeadless(
                d_app, rows, cols, optionalJsonString(params, QStringLiteral("name")));
        if (hasCoords)
            m->setCoordinates(xMin, xMax, yMin, yMax);
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.get_info")) {
        Matrix *m = resolveMatrix(d_app, params);
        outResult = serializeMatrixInfo(m);
        return true;
    }

    if (method == QStringLiteral("matrix.set_coordinates")) {
        Matrix *m = resolveMatrix(d_app, params);
        const double xMin = optionalJsonFiniteDouble(params, QStringLiteral("x_min"), m->xStart());
        const double xMax = optionalJsonFiniteDouble(params, QStringLiteral("x_max"), m->xEnd());
        const double yMin = optionalJsonFiniteDouble(params, QStringLiteral("y_min"), m->yStart());
        const double yMax = optionalJsonFiniteDouble(params, QStringLiteral("y_max"), m->yEnd());
        validateFiniteCoordRange(xMin, xMax, QStringLiteral("Matrix X coordinates"));
        validateFiniteCoordRange(yMin, yMax, QStringLiteral("Matrix Y coordinates"));
        m->setCoordinates(xMin, xMax, yMin, yMax);
        d_app->updateMatrixPlots(m);
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.set_data")) {
        Matrix *m = resolveMatrix(d_app, params);
        if (!params.value(QStringLiteral("data")).isArray())
            throwInvalidParams(QStringLiteral("'data' must be a 2D JSON array of rows."));
        const QJsonArray rowsArr = params.value(QStringLiteral("data")).toArray();
        const int nRows = rowsArr.size();
        int nCols = 0;
        for (const QJsonValue &rv : rowsArr) {
            if (!rv.isArray())
                throwInvalidParams(QStringLiteral("Each row in matrix 'data' must be a JSON array."));
            nCols = std::max(nCols, rv.toArray().size());
        }
        if (nRows <= 0 || nCols <= 0)
            throwInvalidParams(QStringLiteral("Matrix 'data' must be non-empty 2D array."));
        if (nRows > 4096 || nCols > 4096 || qint64(nRows) * qint64(nCols) > 16777216LL) {
            throwInvalidParams(
                    QStringLiteral("Matrix 'data' (%1x%2) exceeds limits (max 4,096x4,096).")
                            .arg(nRows)
                            .arg(nCols));
        }

        const double qNaN = std::numeric_limits<double>::quiet_NaN();
        QVector<QVector<qreal>> colBuffers(nCols, QVector<qreal>(nRows, qNaN));
        for (int r = 0; r < nRows; ++r) {
            const QJsonArray rArr = rowsArr[r].toArray();
            const int rowLen = rArr.size();
            for (int c = 0; c < rowLen; ++c) {
                const double d = jsonToDoubleOrNaN(rArr[c]);
                // Keep every finite value, as table.set_column does (no magnitude cut-off).
                colBuffers[c][r] = std::isfinite(d) ? d : qNaN;
            }
        }

        m->d_future_matrix->beginMacro(
                QObject::tr("%1: set cell values").arg(m->name()));
        if (m->numRows() != nRows || m->numCols() != nCols)
            m->setDimensions(nRows, nCols);
        for (int c = 0; c < nCols; ++c)
            m->d_future_matrix->setColumnCells(c, 0, nRows - 1, colBuffers[c]);
        m->d_future_matrix->endMacro();
        d_app->updateMatrixPlots(m);
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.get_data")) {
        Matrix *m = resolveMatrix(d_app, params);
        const int rows = m->numRows();
        const int cols = m->numCols();
        QJsonArray rowsArr;
        for (int r = 0; r < rows; ++r) {
            QJsonArray cArr;
            for (int c = 0; c < cols; ++c)
                cArr.append(safeJsonDouble(m->cell(r, c)));
            rowsArr.append(cArr);
        }
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("data")] = rowsArr;
        return true;
    }

    if (method == QStringLiteral("matrix.set_formula")) {
        Matrix *m = resolveMatrix(d_app, params);
        const QString formula = optionalJsonString(params, QStringLiteral("formula")).trimmed();
        if (formula.isEmpty())
            throwInvalidParams(QStringLiteral("Parameter 'formula' must be a non-empty expression."));
        validateMuParserExprOrThrow(
                formula,
                { QStringLiteral("i"), QStringLiteral("j"), QStringLiteral("x"),
                  QStringLiteral("y"), QStringLiteral("row"), QStringLiteral("col") },
                QStringLiteral("matrix.set_formula"));
        struct ScopedMatrixFormulaMacro
        {
            future::Matrix *fm = nullptr;
            ScopedMatrixFormulaMacro(future::Matrix *f, const QString &lbl) : fm(f)
            {
                if (fm)
                    fm->beginMacro(lbl);
            }
            ~ScopedMatrixFormulaMacro()
            {
                if (fm)
                    fm->endMacro();
            }
        } formulaMacro(m->d_future_matrix, QStringLiteral("Set Matrix Formula"));
        m->setFormula(formula);
        if (m->d_future_matrix)
            m->d_future_matrix->selectAll();
        const bool ok = m->recalculate();
        d_app->updateMatrixPlots(m);
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = ok;
        return true;
    }

    if (method == QStringLiteral("matrix.transpose")) {
        Matrix *m = resolveMatrix(d_app, params);
        m->transpose();
        d_app->updateMatrixPlots(m);
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.invert")) {
        Matrix *m = resolveMatrix(d_app, params);
        invertMatrixHeadless(d_app, m);
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.determinant")) {
        Matrix *m = resolveMatrix(d_app, params);
        const double det = computeMatrixDeterminantHeadless(m);
        outResult[QStringLiteral("matrix")] = m->name();
        outResult[QStringLiteral("determinant")] = safeJsonDouble(det);
        return true;
    }

    if (method == QStringLiteral("matrix.flip")) {
        Matrix *m = resolveMatrix(d_app, params);
        QString mode = params.value(QStringLiteral("mode")).toString().trimmed().toLower();
        if (mode.isEmpty())
            mode = params.value(QStringLiteral("direction")).toString().trimmed().toLower();
        const bool flipH = (mode == QStringLiteral("h") || mode == QStringLiteral("horizontal"));
        const bool flipV = (mode == QStringLiteral("v") || mode == QStringLiteral("vertical"));
        const bool rotCw = (mode == QStringLiteral("rot90_cw") || mode == QStringLiteral("cw"));
        const bool rotCcw = (mode == QStringLiteral("rot90_ccw") || mode == QStringLiteral("ccw"));
        if (!flipH && !flipV && !rotCw && !rotCcw) {
            throwInvalidParams(
                    QStringLiteral("Unknown flip mode '%1' (expected 'h'|'v'|'rot90_cw'|'rot90_ccw').")
                            .arg(mode));
        }
        {
            // a rotation is a transpose plus a mirror: one Undo step for the whole call
            ScopedRpcUndoMacro flipMacro(m->d_future_matrix,
                                         QObject::tr("%1: flip/rotate").arg(m->name()));
            if (rotCw || rotCcw)
                m->transpose();
            if (flipH || rotCw)
                m->d_future_matrix->mirrorHorizontally();
            else
                m->d_future_matrix->mirrorVertically();
        }
        d_app->updateMatrixPlots(m);
        outResult = serializeMatrixInfo(m);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.fft2d")) {
        Matrix *m = resolveMatrix(d_app, params);
        const bool inv = params.value(QStringLiteral("inverse")).toBool(false);
        const bool shift = params.contains(QStringLiteral("shift_center"))
                ? params.value(QStringLiteral("shift_center")).toBool(true)
                : (params.contains(QStringLiteral("shift"))
                           ? params.value(QStringLiteral("shift")).toBool(true)
                           : true);
        Matrix *outM = AdvancedSignalProc::fft2D(d_app, m, inv, shift);
        if (!outM)
            throwRpcRuntime(QStringLiteral("2D FFT failed on matrix '%1'.").arg(m->name()));
        outResult = serializeMatrixInfo(outM);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.filter2d")) {
        Matrix *m = resolveMatrix(d_app, params);
        const QString fStr =
                optionalJsonString(params, QStringLiteral("method"), QStringLiteral("gaussian"))
                        .trimmed()
                        .toLower();
        AdvancedSignalProc::MatrixFilter2DMethod fm = AdvancedSignalProc::FilterGaussian2D;
        if (fStr.isEmpty() || fStr == QStringLiteral("gaussian")
            || fStr == QStringLiteral("gauss"))
            fm = AdvancedSignalProc::FilterGaussian2D;
        else if (fStr == QStringLiteral("median"))
            fm = AdvancedSignalProc::FilterMedian2D;
        else if (fStr == QStringLiteral("laplacian") || fStr == QStringLiteral("sharpen"))
            fm = AdvancedSignalProc::FilterLaplacianSharpen2D;
        else if (fStr == QStringLiteral("sobel") || fStr == QStringLiteral("gradient"))
            fm = AdvancedSignalProc::FilterSobelGradient2D;
        else if (fStr == QStringLiteral("planar_tilt") || fStr == QStringLiteral("tilt")
                 || fStr == QStringLiteral("detrend"))
            fm = AdvancedSignalProc::FilterSubtractPlanarTilt2D;
        else
            throwInvalidParams(
                    QStringLiteral("Unknown 2D filter '%1' (expected 'gaussian'|'median'|'laplacian'|'sobel'|'planar_tilt').")
                            .arg(fStr));

        const double sigma = params.contains(QStringLiteral("sigma"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("sigma"), 1.5, 1000.0)
                : optionalJsonFiniteDouble(params, QStringLiteral("param"), 1.5, 1000.0);
        if (sigma <= 0.0)
            throwInvalidParams(QStringLiteral("matrix.filter2d 'sigma' must be > 0."));
        const int radius = parseBoundedInt(params, QStringLiteral("radius"), 1, 1, 64);
        const bool inPlace = optionalJsonBool(params, QStringLiteral("in_place"), false);
        const bool plotHeatmap = optionalJsonBool(params, QStringLiteral("plot_heatmap"), false);
        Matrix *outM = AdvancedSignalProc::filterMatrix2D(d_app, m, fm, sigma, radius, inPlace,
                                                          plotHeatmap);
        if (!outM)
            throwRpcRuntime(QStringLiteral("2D filter failed on matrix '%1'.").arg(m->name()));
        outResult = serializeMatrixInfo(outM);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("matrix.analyze_2d_peaks")) {
        Matrix *m = resolveMatrix(d_app, params);
        const int maxPeaks = parseBoundedInt(params, QStringLiteral("max_peaks"), 10, 1, 10000);
        double zMin = 0.0, zMax = 0.0;
        m->range(&zMin, &zMax);
        const double zSpan = std::max(1e-12, zMax - zMin);

        double prom = 0.10;
        if (params.contains(QStringLiteral("min_prominence"))
            && !params.value(QStringLiteral("min_prominence")).isNull()) {
            const double mp = params.value(QStringLiteral("min_prominence")).toDouble(-1.0);
            if (!std::isfinite(mp) || mp < 0.0)
                throwInvalidParams(
                        QStringLiteral("matrix.analyze_2d_peaks 'min_prominence' must be finite and >= 0."));
            prom = (mp > 1.0) ? (mp / zSpan) : mp;
        } else if (params.contains(QStringLiteral("prominence"))
                   && !params.value(QStringLiteral("prominence")).isNull()) {
            const double pVal = params.value(QStringLiteral("prominence")).toDouble(-1.0);
            if (!std::isfinite(pVal) || pVal < 0.0)
                throwInvalidParams(
                        QStringLiteral("matrix.analyze_2d_peaks 'prominence' must be finite and >= 0."));
            prom = pVal;
        }
        if (prom <= 0.0)
            prom = 1e-9;

        Graph *overlayG = nullptr;
        if (params.contains(QStringLiteral("graph"))
            && !params.value(QStringLiteral("graph")).isNull()) {
            MultiLayer *ml = resolveMultiLayer(d_app, params, QStringLiteral("graph"), false);
            if (ml)
                overlayG = resolveGraphLayer(ml, params, false);
        }
        Table *pkTbl = GraphStatsAndSurfaceFit::analyze2DPeaks(d_app, m, maxPeaks, prom, overlayG);
        if (!pkTbl)
            throwRpcRuntime(QStringLiteral("2D peak analysis failed on matrix '%1'.").arg(m->name()));

        QJsonArray peaksArr;
        for (int r = 0; r < pkTbl->numRows(); ++r) {
            Column *xCol = (pkTbl->numCols() > 1) ? pkTbl->column(1) : pkTbl->column(0);
            if (!xCol || xCol->isInvalid(r))
                continue;
            const double xVal = (pkTbl->numCols() > 1) ? pkTbl->cell(r, 1) : 0.0;
            const double yVal = (pkTbl->numCols() > 2) ? pkTbl->cell(r, 2) : 0.0;
            if (!std::isfinite(xVal) || !std::isfinite(yVal))
                continue;

            QJsonObject pObj;
            for (int c = 0; c < pkTbl->numCols(); ++c) {
                const QString lbl = pkTbl->colLabel(c);
                pObj[lbl] = safeJsonDouble(pkTbl->cell(r, c));
            }
            // Deterministic column indices from GraphStatsAndSurfaceFit::analyze2DPeaks:
            // 0=Peak, 1=X_Center, 2=Y_Center, 3=Amplitude, 4=Sigma_X, 5=Sigma_Y, 6=FWHM_X, 7=FWHM_Y, 8=Volume
            pObj[QStringLiteral("index")] =
                    (pkTbl->numCols() > 0) ? static_cast<int>(std::llround(pkTbl->cell(r, 0))) : (r + 1);
            if (pkTbl->numCols() > 1)
                pObj[QStringLiteral("x")] = safeJsonDouble(pkTbl->cell(r, 1));
            if (pkTbl->numCols() > 2)
                pObj[QStringLiteral("y")] = safeJsonDouble(pkTbl->cell(r, 2));
            if (pkTbl->numCols() > 3)
                pObj[QStringLiteral("amplitude")] = safeJsonDouble(pkTbl->cell(r, 3));
            if (pkTbl->numCols() > 4)
                pObj[QStringLiteral("sigma_x")] = safeJsonDouble(pkTbl->cell(r, 4));
            if (pkTbl->numCols() > 5)
                pObj[QStringLiteral("sigma_y")] = safeJsonDouble(pkTbl->cell(r, 5));
            if (pkTbl->numCols() > 6)
                pObj[QStringLiteral("fwhm_x")] = safeJsonDouble(pkTbl->cell(r, 6));
            if (pkTbl->numCols() > 7)
                pObj[QStringLiteral("fwhm_y")] = safeJsonDouble(pkTbl->cell(r, 7));
            if (pkTbl->numCols() > 8)
                pObj[QStringLiteral("volume")] = safeJsonDouble(pkTbl->cell(r, 8));
            peaksArr.append(pObj);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("matrix")] = m->name();
        outResult[QStringLiteral("table")] = pkTbl->name();
        outResult[QStringLiteral("peak_count")] = peaksArr.size();
        outResult[QStringLiteral("peaks")] = peaksArr;
        return true;
    }

    if (method == QStringLiteral("matrix.to_table")) {
        Matrix *m = resolveMatrix(d_app, params);
        const int rows = m->numRows();
        const int cols = m->numCols();
        Table *t = createUniqueTableHeadless(
                d_app, rows, cols, params.value(QStringLiteral("name")).toString());
        const double qNaN = std::numeric_limits<double>::quiet_NaN();
        for (int c = 0; c < cols; ++c) {
            Column *col = t->column(c);
            if (!col)
                continue;
            QVector<qreal> colVec(rows, qNaN);
            QVector<int> invalidRows;
            for (int r = 0; r < rows; ++r) {
                const double v = m->cell(r, c);
                if (std::isfinite(v)) {
                    colVec[r] = v;
                } else {
                    colVec[r] = qNaN;
                    invalidRows.append(r);
                }
            }
            col->setColumnMode(SciDAVis::ColumnMode::Numeric);
            applyDefaultNumericFormat6g(t, c);
            col->clearValidity();
            col->replaceValues(0, colVec);
            for (int ir : invalidRows)
                col->setInvalid(ir, true);
            t->setColPlotDesignation(c, SciDAVis::Y);
        }
        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    return false;
}

} // namespace rpc_internal
