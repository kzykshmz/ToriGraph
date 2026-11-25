/***************************************************************************
    File                 : PythonAutomationServer_Graph.cpp
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
                           [Graph Styling, Auto-Layout, Export & 3D]
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
#include "core/datatypes/Double2StringFilter.h"
#include "lib/Interval.h"
#include "matrix/future_Matrix.h"
#include "table/AsciiTableImportFilter.h"
#include "table/future_Table.h"

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

// Accepts an integer or a fractional point size (12 or 12.9) within [minVal, maxVal].
double parseBoundedFontPt(const QJsonObject &params, const QString &key, double defaultVal,
                          double minVal, double maxVal)
{
    if (!params.contains(key) || params.value(key).isUndefined() || params.value(key).isNull())
        return defaultVal;
    const QJsonValue v = params.value(key);
    if (!v.isDouble() || v.isBool())
        throwInvalidParams(QStringLiteral("Parameter '%1' must be a number (pt).").arg(key));
    const double d = v.toDouble();
    if (!std::isfinite(d) || d < minVal || d > maxVal) {
        throwInvalidParams(QStringLiteral("Parameter '%1' must be in [%2, %3], got %4.")
                                   .arg(key)
                                   .arg(minVal)
                                   .arg(maxVal)
                                   .arg(d));
    }
    return d;
}

Graph3D *resolveGraph3D(ApplicationWindow *app, const QJsonObject &params,
                        const QString &key = QStringLiteral("graph"), bool required = true)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = optionalJsonString(params, key).trimmed();
    if (name.isEmpty())
        name = optionalJsonString(params, QStringLiteral("window")).trimmed();
    if (!name.isEmpty()) {
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("Graph3D"))
                return static_cast<Graph3D *>(mw);
        }
        if (required) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Graph3D window not found: '%1'.").arg(name),
                    collectWindowNames(app, "Graph3D"));
        }
        return nullptr;
    }
    if (QWidget *act = app->d_workspace.activeSubWindow()) {
        if (act->inherits("Graph3D"))
            return static_cast<Graph3D *>(act);
    }
    const QList<MyWidget *> windows = app->windowsList();
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows[i] && windows[i]->inherits("Graph3D"))
            return static_cast<Graph3D *>(windows[i]);
    }
    if (required) {
        throwInvalidParamsWithAvailable(QStringLiteral("No Graph3D window available."),
                                        collectWindowNames(app, "Graph3D"));
    }
    return nullptr;
}

Graph::LegendAnchor parseLegendAnchor(const QString &posStr)
{
    QString s = posStr.trimmed().toLower();
    s.replace(QLatin1Char('-'), QLatin1Char('_'));
    s.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (s == QStringLiteral("auto") || s == QStringLiteral("avoid_data")
        || s == QStringLiteral("auto_avoid_data")) {
        return Graph::LegendAutoAvoidData;
    }
    if (s == QStringLiteral("top_right"))
        return Graph::LegendTopRight;
    if (s == QStringLiteral("top_left"))
        return Graph::LegendTopLeft;
    if (s == QStringLiteral("bottom_right"))
        return Graph::LegendBottomRight;
    if (s == QStringLiteral("bottom_left"))
        return Graph::LegendBottomLeft;
    if (s == QStringLiteral("top_center"))
        return Graph::LegendTopCenter;
    if (s == QStringLiteral("bottom_center"))
        return Graph::LegendBottomCenter;
    if (s == QStringLiteral("outside_right"))
        return Graph::LegendOutsideRight;
    throwInvalidParams(
            QStringLiteral(
                    "Unknown legend position '%1' (expected 'auto'|'avoid_data'|'top_right'|'top_left'|'bottom_right'|'bottom_left'|'top_center'|'bottom_center'|'outside_right').")
                    .arg(posStr));
}

Graph::SeriesStyleScheme parseSeriesStyleScheme(const QString &schemeStr,
                                                QString *outCanonical = nullptr)
{
    QString s = schemeStr.trimmed().toLower();
    s.replace(QLatin1Char('-'), QLatin1Char('_'));
    s.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (s == QStringLiteral("bw_hollow_filled") || s == QStringLiteral("bw")
        || s == QStringLiteral("bw_2to3")) {
        if (outCanonical)
            *outCanonical = QStringLiteral("bw_hollow_filled");
        return Graph::SchemeBW2to3Group;
    }
    if (s == QStringLiteral("blue_red_hollow")) {
        if (outCanonical)
            *outCanonical = QStringLiteral("blue_red_hollow");
        return Graph::SchemeBlueFilledRedHollow;
    }
    if (s == QStringLiteral("muted_categorical")) {
        if (outCanonical)
            *outCanonical = QStringLiteral("muted_categorical");
        return Graph::SchemeMutedCategorical;
    }
    if (s == QStringLiteral("cool_to_warm") || s == QStringLiteral("step_colors")) {
        if (outCanonical)
            *outCanonical = QStringLiteral("cool_to_warm");
        return Graph::SchemeCoolToWarmStep;
    }
    if (s == QStringLiteral("viridis") || s == QStringLiteral("viridis_seq")) {
        if (outCanonical)
            *outCanonical = QStringLiteral("viridis");
        return Graph::SchemeViridisSequential;
    }
    if (s == QStringLiteral("origin") || s == QStringLiteral("origin_classic")) {
        if (outCanonical)
            *outCanonical = QStringLiteral("origin_classic");
        return Graph::SchemeOriginClassic;
    }
    static const QStringList kSchemes = {
        QStringLiteral("bw_hollow_filled"), QStringLiteral("blue_red_hollow"),
        QStringLiteral("muted_categorical"), QStringLiteral("cool_to_warm"),
        QStringLiteral("viridis"), QStringLiteral("origin_classic"),
        QStringLiteral("okabe_ito")
    };
    throwInvalidParamsWithAvailable(
            QStringLiteral("Unknown series scheme '%1'.").arg(schemeStr), kSchemes);
}

// Silently validate that a FunctionCurve's muParser formula(s) compile and evaluate to finite
// numbers without emitting Script::error (which would trigger modal QMessageBox on .sciprj reload).
bool validateFunctionCurveSilent(ApplicationWindow *app, FunctionCurve *fc)
{
    if (!app || !fc)
        return false;
    const QStringList forms = fc->formulas();
    const QString varName = fc->variable().trimmed();
    if (varName.isEmpty())
        return false;
    const double r0 = fc->startRange();
    const double r1 = fc->endRange();
    if (!std::isfinite(r0) || !std::isfinite(r1))
        return false;

    ScriptingEnv *env = ScriptingLangManager::newEnv("muParser", app);
    if (!env)
        return false;
    env->incref();
    struct EnvRefGuard
    {
        ScriptingEnv *e;
        ~EnvRefGuard()
        {
            if (e)
                e->decref();
        }
    } envGuard{ env };

    const double pts[3] = { r0, 0.5 * (r0 + r1), r1 };
    const QByteArray varUtf8 = varName.toUtf8();

    if (fc->functionType() == FunctionCurve::Normal) {
        if (forms.isEmpty() || forms[0].trimmed().isEmpty())
            return false;
        std::unique_ptr<Script> sc(
                env->newScript(forms[0], nullptr, QStringLiteral("rpc_fc_validate")));
        if (!sc)
            return false;
        sc->setEmitErrors(false);
        int finiteCount = 0;
        for (double xv : pts) {
            sc->setDouble(xv, varUtf8.constData());
            const QVariant rv = sc->eval();
            if (rv.type() != QVariant::Double)
                return false;
            if (std::isfinite(rv.toDouble()))
                ++finiteCount;
        }
        return finiteCount > 0;
    }

    if (fc->functionType() == FunctionCurve::Parametric
        || fc->functionType() == FunctionCurve::Polar) {
        if (forms.size() < 2 || forms[0].trimmed().isEmpty() || forms[1].trimmed().isEmpty())
            return false;
        std::unique_ptr<Script> scX(
                env->newScript(forms[0], nullptr, QStringLiteral("rpc_fc_validate_x")));
        std::unique_ptr<Script> scY(
                env->newScript(forms[1], nullptr, QStringLiteral("rpc_fc_validate_y")));
        if (!scX || !scY)
            return false;
        scX->setEmitErrors(false);
        scY->setEmitErrors(false);
        int finiteCount = 0;
        for (double tv : pts) {
            scX->setDouble(tv, varUtf8.constData());
            scY->setDouble(tv, varUtf8.constData());
            const QVariant rx = scX->eval();
            const QVariant ry = scY->eval();
            if (rx.type() != QVariant::Double || ry.type() != QVariant::Double)
                return false;
            if (std::isfinite(rx.toDouble()) && std::isfinite(ry.toDouble()))
                ++finiteCount;
        }
        return finiteCount > 0;
    }

    return false;
}

bool containsJapaneseChars(const QString &s)
{
    for (int i = 0; i < s.size(); ++i) {
        const ushort u = s.at(i).unicode();
        if ((u >= 0x3000 && u <= 0x30FF) || (u >= 0x3400 && u <= 0x9FFF)
            || (u >= 0xF900 && u <= 0xFAFF) || (u >= 0xFF66 && u <= 0xFF9F)) {
            return true;
        }
    }
    return false;
}

QString translateJapaneseFitLabelToEnglish(QString s)
{
    if (!containsJapaneseChars(s))
        return s;
    s.replace(QStringLiteral("\u7dda\u5f62\u30d5\u30a3\u30c3\u30c8"), QStringLiteral("Linear Fit"));
    s.replace(QStringLiteral("\u591a\u9805\u5f0f\u30d5\u30a3\u30c3\u30c8"),
              QStringLiteral("Polynomial Fit"));
    s.replace(QStringLiteral("\u975e\u7dda\u5f62\u30d5\u30a3\u30c3\u30c8"),
              QStringLiteral("Nonlinear Fit"));
    s.replace(QStringLiteral("\u30ac\u30a6\u30b9\u30d5\u30a3\u30c3\u30c8"),
              QStringLiteral("Gaussian Fit"));
    s.replace(QStringLiteral("\u30ed\u30fc\u30ec\u30f3\u30c4\u30d5\u30a3\u30c3\u30c8"),
              QStringLiteral("Lorentzian Fit"));
    s.replace(QStringLiteral("\u30de\u30eb\u30c1\u30d4\u30fc\u30af"), QStringLiteral("Multi-Peak "));
    s.replace(QStringLiteral("\u5408\u8a08\u30d5\u30a3\u30c3\u30c8"),
              QStringLiteral("Fit Envelope"));
    s.replace(QStringLiteral("\u5305\u7d61\u7dda"), QStringLiteral("Fit Envelope"));
    s.replace(QStringLiteral("\u30d9\u30fc\u30b9\u30e9\u30a4\u30f3"), QStringLiteral("Baseline "));
    s.replace(QStringLiteral("\u30d4\u30fc\u30af"), QStringLiteral("Peak "));
    s.replace(QStringLiteral("\u30d5\u30a3\u30c3\u30c8"), QStringLiteral("Fit"));
    return s.simplified();
}

double niceCeilAxisStep(double rawStep)
{
    if (!std::isfinite(rawStep) || rawStep <= 0.0)
        return 1.0;
    const double exp10 = std::floor(std::log10(rawStep));
    const double base = std::pow(10.0, exp10);
    const double frac = rawStep / base;
    if (frac <= 1.0)
        return 1.0 * base;
    if (frac <= 2.0)
        return 2.0 * base;
    if (frac <= 2.5)
        return 2.5 * base;
    if (frac <= 5.0)
        return 5.0 * base;
    return 10.0 * base;
}

bool isPinnedPeakOrBracketCallout(const QString &rawText)
{
    const QString trimmed = rawText.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('\n')))
        return false;
    static const QRegularExpression rxHtmlTags(QStringLiteral("<[^>]+>"));
    QString plain = trimmed;
    plain.remove(rxHtmlTags);
    plain = plain.trimmed();
    if (plain.isEmpty() || plain.size() > 16)
        return false;
    static const QRegularExpression rxMiller(QStringLiteral(
            "^(?:[A-Za-z\\x{0391}-\\x{03C9}]{1,3}\\s*[-_]?)?[\\(\\{\\[][0-9\\-\\x{2212}\\x{0304}\\x{0305}hkliHKLIuvtwUVTW\\s,]{2,10}[\\)\\}\\]]$"));
    static const QRegularExpression rxSig(QStringLiteral("^(?:\\*{1,4}|ns|NS)$"));
    return rxMiller.match(plain).hasMatch() || rxSig.match(plain).hasMatch();
}

int findSafeHtmlSplitPos(const QString &txt)
{
    const int n = txt.size();
    if (n < 12)
        return -1;
    const int mid = n / 2;
    QVector<bool> insideTag(n, false);
    bool inTag = false;
    for (int i = 0; i < n; ++i) {
        if (txt[i] == QLatin1Char('<'))
            inTag = true;
        insideTag[i] = inTag;
        if (txt[i] == QLatin1Char('>'))
            inTag = false;
    }
    static const char *const kBreakTokens[] = { " = ", ", ", "; ", " - " };
    for (const char *tok : kBreakTokens) {
        const QString qtok = QString::fromLatin1(tok);
        int bestPos = -1;
        int bestDist = n;
        int pos = txt.indexOf(qtok);
        while (pos >= 0) {
            const int splitAfter = pos + qtok.size();
            if (pos >= 4 && splitAfter <= n - 4 && !insideTag[pos]) {
                const int d = std::abs(splitAfter - mid);
                if (d < bestDist) {
                    bestDist = d;
                    bestPos = splitAfter;
                }
            }
            pos = txt.indexOf(qtok, pos + 1);
        }
        if (bestPos > 0)
            return bestPos;
    }
    int bestSpace = -1;
    int bestSpaceDist = n;
    for (int i = 4; i < n - 4; ++i) {
        if (!insideTag[i] && txt[i].isSpace()) {
            const int d = std::abs(i - mid);
            if (d < bestSpaceDist) {
                bestSpaceDist = d;
                bestSpace = i;
            }
        }
    }
    return bestSpace;
}

bool fitWideCalloutToCanvas(Graph *g, Plot *plt, Legend *tm, int maxAllowedWidthPx)
{
    if (!g || !plt || !tm || maxAllowedWidthPx < 36)
        return false;
    if (tm->rect().width() <= maxAllowedWidthPx)
        return false;

    const int axisPt = g->axisTitleFont(QwtPlot::yLeft).pointSize();
    const double minPt = (axisPt >= 20) ? 14.0 : 7.5;
    QFont f = tm->font();
    const double origPt = (f.pointSizeF() > 0.0) ? f.pointSizeF()
                                                 : double(std::max(8, f.pointSize()));
    double curPt = origPt;
    bool changed = false;

    while (tm->rect().width() > maxAllowedWidthPx && curPt - 0.5 >= minPt - 1e-6) {
        curPt = std::max(minPt, curPt - 0.5);
        f.setPointSizeF(curPt);
        f.setBold(false);
        f.setWeight(QFont::Normal);
        f.setHintingPreference(QFont::PreferNoHinting);
        tm->setFont(f);
        g->replot();
        plt->updateLayout();
        changed = true;
    }

    if (tm->rect().width() > maxAllowedWidthPx && !tm->text().contains(QLatin1Char('\n'))) {
        const QString raw = tm->text().trimmed();
        const int sp = findSafeHtmlSplitPos(raw);
        if (sp > 0 && sp < raw.size()) {
            const QString wrapped =
                    raw.left(sp).trimmed() + QLatin1Char('\n') + raw.mid(sp).trimmed();
            // Restore reasonable readable font size before re-checking wrapped width
            curPt = std::max(minPt, std::min(origPt, (axisPt >= 20) ? 18.0 : 9.5));
            f.setPointSizeF(curPt);
            tm->setFont(f);
            tm->setText(wrapped);
            g->replot();
            plt->updateLayout();
            changed = true;
            while (tm->rect().width() > maxAllowedWidthPx && curPt - 0.5 >= minPt - 1e-6) {
                curPt = std::max(minPt, curPt - 0.5);
                f.setPointSizeF(curPt);
                tm->setFont(f);
                g->replot();
                plt->updateLayout();
            }
        }
    }
    return changed;
}

QJsonObject runAutoLayoutSolver(ApplicationWindow *app, MultiLayer *ml, const QJsonObject &params)
{
    const double padFraction = std::clamp(
            optionalJsonFiniteDouble(params, QStringLiteral("pad_fraction"), 0.05, 1.0), 0.0, 0.25);
    const bool allowPageResize = optionalJsonBool(params, QStringLiteral("allow_page_resize"), true);
    const bool allowTickRotation =
            optionalJsonBool(params, QStringLiteral("allow_tick_rotation"), true);
    const bool allowOutsideLegend =
            optionalJsonBool(params, QStringLiteral("allow_outside_legend"), true);
    const int maxPasses = parseBoundedInt(params, QStringLiteral("max_passes"), 3, 1, 5);
    const bool dryRun = optionalJsonBool(params, QStringLiteral("dry_run"), false);

    for (Graph *lg : ml->layersList()) {
        if (lg) {
            enforceArialGraphFonts(lg);
            lg->replot();
        }
    }

    const QJsonObject beforeSnap = inspectGraphDetailed(ml, true, true);
    const QJsonArray beforeWarnings = beforeSnap.value(QStringLiteral("warnings")).toArray();
    const QJsonArray beforeCodes = beforeSnap.value(QStringLiteral("warning_codes")).toArray();

    const QString dryBeforeXml = dryRun ? ml->saveToString(QString()) : QString();
    std::unique_ptr<ScopedGraphUndoGuard> undoGuard;
    if (!dryRun) {
        undoGuard = std::make_unique<ScopedGraphUndoGuard>(
                ml, QObject::tr("Auto-Heal Graph Layout"), true);
    }

    QStringList actionsTaken;
    int passesExecuted = 0;

    auto countCalloutCurveHits = [](Plot *plt, Graph *g, const QRect &rectPx) -> int {
        if (!plt || !g || !rectPx.isValid())
            return 0;
        const QRect padRect = rectPx.adjusted(-4, -4, 4, 4);
        int hits = 0;
        for (int ci = 0; ci < g->curves(); ++ci) {
            QwtPlotCurve *c = g->curve(ci);
            if (!c || !g->isCurveVisible(ci) || g->curveType(ci) == Graph::ErrorBars
                || dynamic_cast<QwtErrorPlotCurve *>(c) != nullptr) {
                continue;
            }
            const int xAx = (c->xAxis() == QwtPlot::xTop) ? QwtPlot::xTop : QwtPlot::xBottom;
            const int yAx = (c->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight : QwtPlot::yLeft;
            const QwtScaleMap xMap = plt->canvasMap(xAx);
            const QwtScaleMap yMap = plt->canvasMap(yAx);
            auto *dc = dynamic_cast<DataCurve *>(c);
            const bool checkMasks = (dc != nullptr) && dc->hasAnyMaskedPoints();
            Column *maskXCol = nullptr;
            Column *maskYCol = nullptr;
            if (checkMasks)
                dc->resolveColumns(maskXCol, maskYCol);
            const QList<DataCurve *> errList = dc ? dc->errorBarsList() : QList<DataCurve *>();
            const bool hasErr = !errList.isEmpty();
            const bool hasLineSegs = (c->style() != QwtPlotCurve::NoCurve);
            const int n = static_cast<int>(c->dataSize());
            bool hasPrev = false;
            QPoint prevPt;
            for (int k = 0; k < n; ++k) {
                if (checkMasks && dc->isPointMaskedFast(k, maskXCol, maskYCol)) {
                    hasPrev = false;
                    continue;
                }
                const double xv = c->x(k);
                const double yv = c->y(k);
                if (!std::isfinite(xv) || !std::isfinite(yv)) {
                    hasPrev = false;
                    continue;
                }
                const QPoint curPt(xMap.transform(xv), yMap.transform(yv));
                if (padRect.contains(curPt))
                    ++hits;
                if (hasErr) {
                    for (DataCurve *ebBase : errList) {
                        auto *ec = dynamic_cast<QwtErrorPlotCurve *>(ebBase);
                        if (!ec || k >= ec->dataSize())
                            continue;
                        const double ev = std::abs(ec->errorValue(k));
                        if (!std::isfinite(ev) || ev <= 0.0)
                            continue;
                        const int halfCap = std::max(3, ec->capLength() / 2);
                        if (ec->xErrors()) {
                            const int pxLo = xMap.transform(ec->minusSide() ? (xv - ev) : xv);
                            const int pxHi = xMap.transform(ec->plusSide() ? (xv + ev) : xv);
                            const QRect ebBox(std::min(pxLo, pxHi), curPt.y() - halfCap,
                                              std::max(1, std::abs(pxHi - pxLo)), 2 * halfCap + 1);
                            if (padRect.intersects(ebBox))
                                ++hits;
                        } else {
                            const int pyLo = yMap.transform(ec->minusSide() ? (yv - ev) : yv);
                            const int pyHi = yMap.transform(ec->plusSide() ? (yv + ev) : yv);
                            const QRect ebBox(curPt.x() - halfCap, std::min(pyLo, pyHi),
                                              2 * halfCap + 1, std::max(1, std::abs(pyHi - pyLo)));
                            if (padRect.intersects(ebBox))
                                ++hits;
                        }
                    }
                }
                if (hasPrev && hasLineSegs
                    && std::max(prevPt.x(), curPt.x()) >= padRect.left()
                    && std::min(prevPt.x(), curPt.x()) <= padRect.right()
                    && std::max(prevPt.y(), curPt.y()) >= padRect.top()
                    && std::min(prevPt.y(), curPt.y()) <= padRect.bottom()) {
                    for (int s = 1; s <= 7; ++s) {
                        const double t = double(s) / 8.0;
                        const QPoint midPt(
                                qRound((1.0 - t) * double(prevPt.x()) + t * double(curPt.x())),
                                qRound((1.0 - t) * double(prevPt.y()) + t * double(curPt.y())));
                        if (padRect.contains(midPt)) {
                            ++hits;
                            break;
                        }
                    }
                }
                prevPt = curPt;
                hasPrev = true;
            }
        }
        const QwtScaleMap refXMap = plt->canvasMap(QwtPlot::xBottom);
        const QwtScaleMap refYMap = plt->canvasMap(QwtPlot::yLeft);
        for (const Graph::ReferenceLineSpec &rl : g->referenceLines()) {
            if (!rl.statMode.isEmpty() || !std::isfinite(rl.value))
                continue;
            if (rl.orientation == 1) {
                const int rx = refXMap.transform(rl.value);
                if (rx >= padRect.left() && rx <= padRect.right())
                    hits += 3;
            } else {
                const int ry = refYMap.transform(rl.value);
                if (ry >= padRect.top() && ry <= padRect.bottom())
                    hits += 3;
            }
        }
        return hits;
    };

    for (int pass = 0; pass < maxPasses; ++pass) {
        const QJsonObject curSnap = inspectGraphDetailed(ml, true, true);
        const QJsonArray curWarnings = curSnap.value(QStringLiteral("warnings")).toArray();
        if (curWarnings.isEmpty())
            break;
        ++passesExecuted;

        const QList<Graph *> layers = ml->layersList();
        QVector<int> primaryIndices;
        for (int li = 0; li < layers.size(); ++li) {
            Graph *lg = layers[li];
            if (lg && !lg->linkedParentLayer() && !ml->isInsetLayer(lg))
                primaryIndices.append(li);
        }
        const int nPrimary = primaryIndices.size();

        auto hasWarnCode = [&](const QString &code, int targetLayer = -1) -> bool {
            for (const QJsonValue &wv : curWarnings) {
                const QJsonObject wo = wv.toObject();
                if (wo.value(QStringLiteral("code")).toString() == code
                    && (targetLayer < 0 || wo.value(QStringLiteral("layer")).toInt() == targetLayer)) {
                    return true;
                }
            }
            return false;
        };

        // Stage 1: DISABLED_AXIS_HAS_TITLE_OR_CURVE
        if (hasWarnCode(QStringLiteral("DISABLED_AXIS_HAS_TITLE_OR_CURVE"))) {
            static const int kAxes[4] = { QwtPlot::xBottom, QwtPlot::yLeft, QwtPlot::xTop,
                                          QwtPlot::yRight };
            static const char *const kAxNames[4] = { "x", "y", "top", "right" };
            for (int li = 0; li < layers.size(); ++li) {
                Graph *g = layers[li];
                Plot *plt = g ? g->plotWidget() : nullptr;
                if (!g || !plt || invokeGraphIsPolarPlot(g) || g->isTernaryPlot() || g->isPiePlot())
                    continue;
                for (int ai = 0; ai < 4; ++ai) {
                    const int ax = kAxes[ai];
                    if (plt->axisEnabled(ax))
                        continue;
                    int boundCurves = 0;
                    for (int ci = 0; ci < g->curves(); ++ci) {
                        QwtPlotCurve *c = g->curve(ci);
                        if (!c || !g->isCurveVisible(ci) || g->curveType(ci) == Graph::ErrorBars)
                            continue;
                        if (c->xAxis() == ax || c->yAxis() == ax)
                            ++boundCurves;
                    }
                    if (boundCurves > 0) {
                        g->enableAxis(ax, true);
                        g->setTickLabelsOnAxis(ax, true);
                        actionsTaken.append(
                                QStringLiteral("Enabled axis '%1' on layer %2 for %3 bound curve(s)")
                                        .arg(QLatin1String(kAxNames[ai]))
                                        .arg(li)
                                        .arg(boundCurves));
                    } else if (!g->linkedParentLayer() && !g->axisTitle(ax).trimmed().isEmpty()) {
                        g->setAxisTitleAuto(ax, false);
                        g->setAxisTitle(ax, QString());
                        actionsTaken.append(
                                QStringLiteral("Cleared orphan title on disabled axis '%1' of layer %2")
                                        .arg(QLatin1String(kAxNames[ai]))
                                        .arg(li));
                    }
                }
                g->replot();
            }
        }

        // Stage 2: CURVE_CLIPPED_BY_AXIS (expand axis envelope on non-inset layers)
        for (int li = 0; li < layers.size(); ++li) {
            Graph *g = layers[li];
            Plot *plt = g ? g->plotWidget() : nullptr;
            if (!g || !plt || ml->isInsetLayer(g) || invokeGraphIsPolarPlot(g)
                || g->isTernaryPlot() || g->isPiePlot()
                || !hasWarnCode(QStringLiteral("CURVE_CLIPPED_BY_AXIS"), li)) {
                continue;
            }
            static const int kAxes[4] = { QwtPlot::xBottom, QwtPlot::yLeft, QwtPlot::xTop,
                                          QwtPlot::yRight };
            static const char *const kAxNames[4] = { "x", "y", "top", "right" };
            for (int ai = 0; ai < 4; ++ai) {
                const int ax = kAxes[ai];
                const bool isX = (ax == QwtPlot::xBottom || ax == QwtPlot::xTop);
                const bool isLog =
                        (dynamic_cast<const QwtLog10ScaleEngine *>(plt->axisScaleEngine(ax))
                         != nullptr);
                double dMin = std::numeric_limits<double>::infinity();
                double dMax = -std::numeric_limits<double>::infinity();
                int countPts = 0;

                for (int ci = 0; ci < g->curves(); ++ci) {
                    QwtPlotCurve *c = g->curve(ci);
                    if (!c || !g->isCurveVisible(ci) || g->curveType(ci) == Graph::ErrorBars
                        || dynamic_cast<QwtErrorPlotCurve *>(c) != nullptr) {
                        continue;
                    }
                    const int cAx = isX ? ((c->xAxis() == QwtPlot::xTop) ? QwtPlot::xTop
                                                                         : QwtPlot::xBottom)
                                        : ((c->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight
                                                                           : QwtPlot::yLeft);
                    if (cAx != ax)
                        continue;
                    auto *dc = dynamic_cast<DataCurve *>(c);
                    const bool checkMasks = (dc != nullptr) && dc->hasAnyMaskedPoints();
                    Column *maskXCol = nullptr;
                    Column *maskYCol = nullptr;
                    if (checkMasks)
                        dc->resolveColumns(maskXCol, maskYCol);
                    QVector<QwtErrorPlotCurve *> errCurves;
                    if (dc) {
                        for (DataCurve *eb : dc->errorBarsList()) {
                            if (auto *ec = dynamic_cast<QwtErrorPlotCurve *>(eb)) {
                                if (ec->xErrors() == isX)
                                    errCurves.append(ec);
                            }
                        }
                    }
                    const int n = static_cast<int>(c->dataSize());
                    for (int k = 0; k < n; ++k) {
                        if (checkMasks && dc->isPointMaskedFast(k, maskXCol, maskYCol))
                            continue;
                        const double v = isX ? c->x(k) : c->y(k);
                        if (!std::isfinite(v) || std::abs(v) > 1e300)
                            continue;
                        if (isLog && v <= 0.0)
                            continue;
                        double loV = v;
                        double hiV = v;
                        for (QwtErrorPlotCurve *ec : errCurves) {
                            if (k < ec->dataSize()) {
                                const double ev = std::abs(ec->errorValue(k));
                                if (std::isfinite(ev)) {
                                    if (!isLog || v - ev > 0.0)
                                        loV = std::min(loV, v - ev);
                                    hiV = std::max(hiV, v + ev);
                                }
                            }
                        }
                        dMin = std::min(dMin, loV);
                        dMax = std::max(dMax, hiV);
                        ++countPts;
                    }
                }
                if (countPts <= 0 || !std::isfinite(dMax - dMin))
                    continue;

                for (const auto &rl : g->referenceLines()) {
                    if (!rl.visible)
                        continue;
                    const bool rlIsX = (rl.orientation == 1);
                    if (rlIsX == isX && rl.statMode.isEmpty() && std::isfinite(rl.value)
                        && (!isLog || rl.value > 0.0)) {
                        dMin = std::min(dMin, rl.value);
                        dMax = std::max(dMax, rl.value);
                    }
                }

                const QwtScaleDiv *sdiv = plt->axisScaleDiv(ax);
                const double rawLo = sdiv ? sdiv->lowerBound() : dMin;
                const double rawHi = sdiv ? sdiv->upperBound() : dMax;
                const bool inv = (rawLo > rawHi);
                const double curLo = std::min(rawLo, rawHi);
                const double curHi = std::max(rawLo, rawHi);
                if (dMin >= curLo && dMax <= curHi)
                    continue;

                double newLo = curLo;
                double newHi = curHi;
                if (isLog) {
                    const double logSpan =
                            std::max(1.0, std::log10(std::max(dMax, dMin * 10.0)) - std::log10(dMin));
                    const double factor = std::pow(10.0, padFraction * logSpan);
                    newLo = std::max(1e-300, std::min(curLo > 0.0 ? curLo : dMin, dMin / factor));
                    newHi = std::max(curHi, dMax * factor);
                } else {
                    const double span = (dMax > dMin)
                            ? (dMax - dMin)
                            : std::max(1.0, std::abs(dMax) * 0.1);
                    if (curLo == 0.0 && dMin >= 0.0 && dMin <= 0.08 * span)
                        newLo = 0.0;
                    else
                        newLo = std::min(curLo, dMin - padFraction * span);
                    newHi = std::max(curHi, dMax + padFraction * span);
                }
                if (std::isfinite(newHi - newLo) && newHi > newLo) {
                    const int maj = std::max(4, plt->axisMaxMajor(ax));
                    const int minT = isLog ? 8 : std::max(1, plt->axisMaxMinor(ax));
                    g->setScale(ax, newLo, newHi, 0.0, maj, minT, isLog ? 1 : 0, inv);
                    actionsTaken.append(
                            QStringLiteral("Expanded layer %1 axis '%2' scale to [%3, %4]")
                                    .arg(li)
                                    .arg(QLatin1String(kAxNames[ai]))
                                    .arg(newLo, 0, 'g', 5)
                                    .arg(newHi, 0, 'g', 5));
                }
            }
            g->syncChildLinkedLayers();
            g->replot();
        }

        // Stage 3: PANEL_CANVAS_SIZE_MISMATCH & cramped multi-panel page/title normalization
        if (nPrimary >= 2) {
            int rows = std::max(1, ml->getRows());
            int cols = std::max(1, ml->getCols());
            if (rows * cols < nPrimary) {
                cols = std::max(2, int(std::ceil(std::sqrt(double(nPrimary)))));
                rows = (nPrimary + cols - 1) / cols;
            }
            bool didStage3ResizeOrSync = hasWarnCode(QStringLiteral("PANEL_CANVAS_SIZE_MISMATCH"));
            if (allowPageResize
                && (ml->pageViewMode() || ml->pageUnit() != MultiLayer::UnitPx)) {
                const MultiLayer::PageUnit u = (ml->pageUnit() != MultiLayer::UnitPx)
                        ? ml->pageUnit()
                        : MultiLayer::UnitMm;
                const double toMm = (u == MultiLayer::UnitInch)
                        ? 25.4
                        : ((u == MultiLayer::UnitCm) ? 10.0 : 1.0);
                const double curWMm = ml->pageWidthPhysical() * toMm;
                const double curHMm = ml->pageHeightPhysical() * toMm;
                const double minWMm = (cols == 2) ? 175.0 : std::min(210.0, double(cols) * 80.0);
                const double minHMm = double(rows) * 80.0;
                if (curWMm < minWMm - 1.0 || curHMm < minHMm - 1.0) {
                    const double nextWMm = std::max(curWMm, minWMm);
                    const double nextHMm = std::max(curHMm, minHMm);
                    ml->setPageSizePhysical(nextWMm / toMm, nextHMm / toMm, u, ml->pageDpi());
                    didStage3ResizeOrSync = true;
                    actionsTaken.append(
                            QStringLiteral("Resized multi-panel page to %1 x %2 mm for %3x%4 grid")
                                    .arg(nextWMm, 0, 'f', 0)
                                    .arg(nextHMm, 0, 'f', 0)
                                    .arg(rows)
                                    .arg(cols));
                }
            }
            if (didStage3ResizeOrSync) {
                // Harmonize axis font sizes across primary panels so Qwt border widths match
                Graph *refG = layers[primaryIndices.first()];
                for (int pi : primaryIndices) {
                    Graph *pg = layers[pi];
                    if (!pg || pg == refG)
                        continue;
                    for (int ax = 0; ax < 4; ++ax) {
                        pg->setAxisFont(ax, refG->axisFont(ax));
                        pg->setAxisTitleFont(ax, refG->axisTitleFont(ax));
                    }
                }
                ml->setRows(rows);
                ml->setCols(cols);
                if (undoGuard)
                    undoGuard->checkpointBeforeSubPush();
                ml->arrangeLayers(false, false);
                if (ml->pageViewMode() || ml->pageUnit() != MultiLayer::UnitPx)
                    ml->fitLayersToPage();
                if (undoGuard)
                    undoGuard->checkpointAfterSubPush();
                actionsTaken.append(
                        QStringLiteral("Synchronized %1x%2 multi-panel canvas sizes")
                                .arg(rows)
                                .arg(cols));
            }
            // Step down axisTitleFont (>= 8 pt floor, harmonized across primary panels) if wider than panelSpanPx - 12
            static const QRegularExpression htmlTagStripRx(QStringLiteral("<[^>]*>"));
            bool shrunkAnyTitle = false;
            for (int ax = 0; ax < 4; ++ax) {
                const bool horizAx = (ax == QwtPlot::xBottom || ax == QwtPlot::xTop);
                int minTargetPt = 999;
                bool axisNeedsShrink = false;
                for (int pi : primaryIndices) {
                    Graph *pg = layers[pi];
                    Plot *pplt = pg ? pg->plotWidget() : nullptr;
                    if (!pg || !pplt || !pplt->axisEnabled(ax))
                        continue;
                    QString plainTitle = pg->axisTitle(ax);
                    plainTitle.remove(htmlTagStripRx);
                    plainTitle = plainTitle.simplified();
                    if (plainTitle.isEmpty())
                        continue;
                    pplt->updateLayout();
                    const QRect cr = pplt->plotLayout() ? pplt->plotLayout()->canvasRect()
                                                        : QRect(0, 0, pg->width(), pg->height());
                    const int spanPx = std::max(40, (horizAx ? cr.width() : cr.height()) - 12);
                    QFont tf = pg->axisTitleFont(ax);
                    tf.setFamily(QStringLiteral("Arial"));
                    const int startPt = std::max(8, tf.pointSize());
                    int pt = startPt;
                    while (pt > 8 && QFontMetrics(tf).horizontalAdvance(plainTitle) > spanPx) {
                        --pt;
                        tf.setPointSize(pt);
                    }
                    if (pt < startPt) {
                        axisNeedsShrink = true;
                        minTargetPt = std::min(minTargetPt, pt);
                    }
                }
                if (axisNeedsShrink && minTargetPt >= 8 && minTargetPt < 999) {
                    for (int pi : primaryIndices) {
                        Graph *pg = layers[pi];
                        if (!pg)
                            continue;
                        QFont tf = pg->axisTitleFont(ax);
                        tf.setFamily(QStringLiteral("Arial"));
                        tf.setBold(false);
                        tf.setPointSize(minTargetPt);
                        pg->setAxisTitleFont(ax, tf);
                        pg->replot();
                    }
                    shrunkAnyTitle = true;
                    const char *axLbl = (ax == QwtPlot::xBottom) ? "x"
                            : (ax == QwtPlot::xTop)              ? "top"
                            : (ax == QwtPlot::yRight)            ? "right"
                                                                 : "y";
                    actionsTaken.append(
                            QStringLiteral("Reduced multi-panel axis '%1' title font to %2 pt")
                                    .arg(QLatin1String(axLbl))
                                    .arg(minTargetPt));
                }
            }
            if (shrunkAnyTitle && (ml->pageViewMode() || ml->pageUnit() != MultiLayer::UnitPx))
                ml->fitLayersToPage();
        }

        // Stage 4: TICK_LABEL_COLLISION (4-rung escalation ladder)
        {
            const QJsonObject midSnap = inspectGraphDetailed(ml, true, true);
            const QJsonArray midWarnings = midSnap.value(QStringLiteral("warnings")).toArray();
            for (const QJsonValue &wv : midWarnings) {
                const QJsonObject wo = wv.toObject();
                if (wo.value(QStringLiteral("code")).toString()
                    != QStringLiteral("TICK_LABEL_COLLISION")) {
                    continue;
                }
                const int li = wo.value(QStringLiteral("layer")).toInt(-1);
                const QString axisKey =
                        wo.value(QStringLiteral("details")).toObject().value(QStringLiteral("axis")).toString();
                if (li < 0 || li >= layers.size() || !layers[li])
                    continue;
                Graph *g = layers[li];
                Plot *plt = g->plotWidget();
                if (!plt)
                    continue;
                const int ax = parseAxisId(axisKey);
                const bool horiz = (ax == QwtPlot::xBottom || ax == QwtPlot::xTop);
                const QStringList catLabels = invokeGraphCategoryLabels(g, ax);
                const QList<Graph::AxisType> aTypes = g->axesType();
                const bool isCatAxis = !catLabels.isEmpty()
                        || (ax >= 0 && ax < aTypes.size() && aTypes[ax] == Graph::AxisType::Txt);

                // Rung 4a: Category axis -> rotate -35 deg first instead of dropping category labels
                if (isCatAxis && horiz && allowTickRotation && g->labelsRotation(ax) == 0
                    && catLabels.size() <= 16) {
                    g->setAxisLabelRotation(ax, -35);
                    g->replot();
                    actionsTaken.append(
                            QStringLiteral("Rotated category tick labels -35 deg on layer %1 axis '%2'")
                                    .arg(li)
                                    .arg(axisKey));
                    continue;
                }

                const QwtScaleDiv *sdiv = plt->axisScaleDiv(ax);
                auto *sd = dynamic_cast<ScaleDraw *>(plt->axisScaleDraw(ax));
                const bool isLog =
                        (dynamic_cast<const QwtLog10ScaleEngine *>(plt->axisScaleEngine(ax))
                         != nullptr);
                if (sdiv && !isLog && !isCatAxis) {
                    const double rawLo = sdiv->lowerBound();
                    const double rawHi = sdiv->upperBound();
                    const bool inv = (rawLo > rawHi);
                    const double lo = std::min(rawLo, rawHi);
                    const double hi = std::max(rawLo, rawHi);
                    const double span = hi - lo;
                    if (std::isfinite(span) && span > 0.0) {
                        plt->updateLayout();
                        const QRect cr = plt->plotLayout() ? plt->plotLayout()->canvasRect()
                                                           : QRect(0, 0, g->width(), g->height());
                        const double axisLenPx =
                                std::max(60.0, double(horiz ? cr.width() : cr.height()));
                        const QFontMetrics fm(g->axisFont(ax));
                        int maxLblPx = 24;
                        int maxChars = 0;
                        for (double tv : sdiv->ticks(QwtScaleDiv::MajorTick)) {
                            const QString txt = sd ? sd->label(tv).text().trimmed()
                                                   : QString::number(tv, 'g', 6);
                            maxChars = std::max(maxChars, txt.size());
                            maxLblPx = std::max(maxLblPx,
                                                horiz ? fm.horizontalAdvance(txt) : fm.height());
                        }
                        if (maxChars >= 7
                            && (std::abs(hi) >= 1e4 || std::abs(lo) >= 1e4
                                || (hi != 0.0 && std::abs(hi) <= 1e-3))
                            && plt->axisLabelFormat(ax) == 0) {
                            const QStringList axFormulas = g->getAxesFormulas();
                            const QString curF = (ax >= 0 && ax < axFormulas.size())
                                    ? axFormulas[ax]
                                    : QString();
                            g->setLabelsNumericFormat(ax, 2 /*'e'*/, 1, curF);
                            actionsTaken.append(
                                    QStringLiteral("Switched layer %1 axis '%2' to compact scientific format")
                                            .arg(li)
                                            .arg(axisKey));
                        }
                        const int maxIntervals =
                                std::max(2, int(std::floor(axisLenPx / double(maxLblPx + 12))));
                        const double curStep = g->axisStep(ax);
                        const double minStep = span / double(maxIntervals);
                        const double nextStep = niceCeilAxisStep(
                                (curStep > 0.0 && minStep <= curStep) ? (curStep * 1.6) : minStep);
                        if (std::isfinite(nextStep) && nextStep > 0.0 && nextStep < span * 0.95) {
                            g->setScale(ax, lo, hi, nextStep, std::max(2, maxIntervals), 1, 0, inv);
                            g->replot();
                            actionsTaken.append(
                                    QStringLiteral("Coarsened layer %1 axis '%2' major step to %3")
                                            .arg(li)
                                            .arg(axisKey)
                                            .arg(nextStep, 0, 'g', 5));
                        }
                    }
                }

                // Re-check if still colliding on this axis -> Rung 4c (rotate) / Rung 4d (font -1pt)
                const QJsonObject postStepSnap = inspectGraphDetailed(ml, true, true);
                bool stillCollides = false;
                for (const QJsonValue &w2 : postStepSnap.value(QStringLiteral("warnings")).toArray()) {
                    const QJsonObject wo2 = w2.toObject();
                    if (wo2.value(QStringLiteral("code")).toString()
                                == QStringLiteral("TICK_LABEL_COLLISION")
                        && wo2.value(QStringLiteral("layer")).toInt(-1) == li
                        && wo2.value(QStringLiteral("details"))
                                   .toObject()
                                   .value(QStringLiteral("axis"))
                                   .toString()
                                == axisKey) {
                        stillCollides = true;
                        break;
                    }
                }
                if (stillCollides && horiz && allowTickRotation && std::abs(g->labelsRotation(ax)) < 35) {
                    g->setAxisLabelRotation(ax, -35);
                    g->replot();
                    actionsTaken.append(
                            QStringLiteral("Rotated tick labels -35 deg on layer %1 axis '%2'")
                                    .arg(li)
                                    .arg(axisKey));
                } else if (stillCollides && g->axisFont(ax).pointSize() > 7) {
                    QFont nf = g->axisFont(ax);
                    nf.setFamily(QStringLiteral("Arial"));
                    nf.setBold(false);
                    nf.setPointSize(std::max(7, nf.pointSize() - 1));
                    g->setAxisFont(ax, nf);
                    g->replot();
                    actionsTaken.append(
                            QStringLiteral("Reduced tick font to %1 pt on layer %2 axis '%3'")
                                    .arg(nf.pointSize())
                                    .arg(li)
                                    .arg(axisKey));
                }
                if ((ml->pageViewMode() || ml->pageUnit() != MultiLayer::UnitPx)
                    && nPrimary >= 2) {
                    ml->fitLayersToPage();
                }
            }
        }

        // Stage 5: LEGEND_OVERLAPS_DATA + CALLOUT_OVERLAPS_LEGEND / CALLOUT_OVERLAPS_DATA
        {
            const QJsonObject legSnap = inspectGraphDetailed(ml, true, true);
            const QJsonArray legWarnings = legSnap.value(QStringLiteral("warnings")).toArray();
            for (int li = 0; li < layers.size(); ++li) {
                Graph *g = layers[li];
                Plot *plt = g ? g->plotWidget() : nullptr;
                if (!g || !plt || invokeGraphIsPolarPlot(g) || g->isTernaryPlot() || g->isPiePlot())
                    continue;

                bool legOverlaps = false;
                bool calloutOverlaps = false;
                for (const QJsonValue &wv : legWarnings) {
                    const QJsonObject wo = wv.toObject();
                    if (wo.value(QStringLiteral("layer")).toInt(-1) != li)
                        continue;
                    const QString c = wo.value(QStringLiteral("code")).toString();
                    if (c == QStringLiteral("LEGEND_OVERLAPS_DATA"))
                        legOverlaps = true;
                    else if (c == QStringLiteral("CALLOUT_OVERLAPS_LEGEND")
                             || c == QStringLiteral("CALLOUT_OVERLAPS_DATA")
                             || c == QStringLiteral("CALLOUT_CLIPPED_BY_CANVAS"))
                        calloutOverlaps = true;
                }

                struct ScopedCalloutHideGuard
                {
                    QVector<QPair<Legend *, bool>> saved;
                    explicit ScopedCalloutHideGuard(Graph *gr)
                    {
                        if (!gr)
                            return;
                        Legend *mainLeg = gr->legend();
                        for (int tid : gr->textMarkerKeys()) {
                            Legend *tm = gr->textMarker(tid);
                            if (tm && tm != mainLeg && tm->isVisible()
                                && !isPinnedPeakOrBracketCallout(tm->text())) {
                                saved.append(qMakePair(tm, true));
                                tm->setVisible(false);
                            }
                        }
                    }
                    void restore()
                    {
                        for (const auto &p : saved) {
                            if (p.first)
                                p.first->setVisible(p.second);
                        }
                        saved.clear();
                    }
                    ~ScopedCalloutHideGuard() { restore(); }
                };

                auto expandLinearYHeadroom = [&](double frac) -> bool {
                    bool hasSpec = false;
                    for (QwtPlotItem *it : plt->itemList()) {
                        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                            hasSpec = true;
                            break;
                        }
                    }
                    if (hasSpec)
                        return false;
                    bool added = false;
                    const bool yLog = (dynamic_cast<const QwtLog10ScaleEngine *>(
                                               plt->axisScaleEngine(QwtPlot::yLeft))
                                       != nullptr);
                    const QwtScaleDiv *ysd = plt->axisScaleDiv(QwtPlot::yLeft);
                    if (!yLog && ysd && ysd->lowerBound() < ysd->upperBound()) {
                        const double yLo = ysd->lowerBound();
                        const double yHi = ysd->upperBound();
                        const double ySpan = yHi - yLo;
                        if (std::isfinite(ySpan) && ySpan > 0.0) {
                            g->setScale(QwtPlot::yLeft, yLo, yHi + frac * ySpan, 0.0,
                                        std::max(4, plt->axisMaxMajor(QwtPlot::yLeft)), 1, 0,
                                        false);
                            added = true;
                        }
                    }
                    if (plt->axisEnabled(QwtPlot::yRight)) {
                        bool hasRightCurves = false;
                        for (int ci = 0; ci < g->curves(); ++ci) {
                            QwtPlotCurve *c = g->curve(ci);
                            if (c && g->isCurveVisible(ci)
                                && g->curveType(ci) != Graph::ErrorBars
                                && c->yAxis() == QwtPlot::yRight) {
                                hasRightCurves = true;
                                break;
                            }
                        }
                        const bool yrLog = (dynamic_cast<const QwtLog10ScaleEngine *>(
                                                    plt->axisScaleEngine(QwtPlot::yRight))
                                            != nullptr);
                        const QwtScaleDiv *yrsd = plt->axisScaleDiv(QwtPlot::yRight);
                        if (hasRightCurves && !yrLog && yrsd
                            && yrsd->lowerBound() < yrsd->upperBound()) {
                            const double yrLo = yrsd->lowerBound();
                            const double yrHi = yrsd->upperBound();
                            const double yrSpan = yrHi - yrLo;
                            if (std::isfinite(yrSpan) && yrSpan > 0.0) {
                                g->setScale(QwtPlot::yRight, yrLo, yrHi + frac * yrSpan, 0.0,
                                            std::max(4, plt->axisMaxMajor(QwtPlot::yRight)), 1, 0,
                                            false);
                                added = true;
                            }
                        }
                    }
                    if (added)
                        g->syncChildLinkedLayers();
                    return added;
                };

                if (legOverlaps && g->isLegendVisible() && g->legend()) {
                    Legend *leg = g->legend();
                    double score = 0.0;
                    {
                        // Temporarily hide movable callouts so legend claims top headroom first;
                        // callouts pack cleanly around legRect immediately afterward!
                        ScopedCalloutHideGuard hideGuard(g);
                        // Rung 5a: Auto-place across internal candidate anchors
                        const QString anchor1 =
                                g->placeLegendAnchor(Graph::LegendAutoAvoidData, &score, 12);
                        g->replot();

                        const QString kWarningsKey = QStringLiteral("warnings");
                        const QString kLayerKey = QStringLiteral("layer");
                        const QString kCodeKey = QStringLiteral("code");
                        const QString kLegOverlapCode = QStringLiteral("LEGEND_OVERLAPS_DATA");
                        auto checkStillLegOverlap = [&]() -> bool {
                            const QJsonObject chk = inspectGraphDetailed(ml, false, true);
                            for (const QJsonValue &cw : chk.value(kWarningsKey).toArray()) {
                                const QJsonObject co = cw.toObject();
                                if (co.value(kLayerKey).toInt(-1) == li
                                    && co.value(kCodeKey).toString() == kLegOverlapCode) {
                                    return true;
                                }
                            }
                            return false;
                        };

                        if (!checkStillLegOverlap()) {
                            actionsTaken.append(
                                    QStringLiteral("Relocated legend on layer %1 to '%2'")
                                            .arg(li)
                                            .arg(anchor1));
                        } else {
                            // Rung 5b: Compact 2-column legend if >= 3 curves
                            const QRect cr = plt->plotLayout()
                                    ? plt->plotLayout()->canvasRect()
                                    : QRect(0, 0, g->width(), g->height());
                            if (g->curves() >= 3 && leg->numColumns() == 1
                                && leg->rect().width() * 17 < cr.width() * 10) {
                                leg->setNumColumns(2);
                                g->placeLegendAnchor(Graph::LegendAutoAvoidData, &score, 10);
                                g->replot();
                                actionsTaken.append(
                                        QStringLiteral("Switched legend on layer %1 to 2 columns")
                                                .arg(li));
                            }
                            // Rung 5c: +28% Y-axis top headroom on linear non-inverted non-spectrogram layer
                            if (checkStillLegOverlap()) {
                                if (expandLinearYHeadroom(0.28)) {
                                    g->placeLegendAnchor(Graph::LegendAutoAvoidData, &score, 10);
                                    g->replot();
                                    actionsTaken.append(
                                            QStringLiteral("Added +28% Y-axis headroom on layer %1 for legend")
                                                    .arg(li));
                                }
                            }
                            // Rung 5d: Single-panel outside_right fallback (NEVER on nPrimary >= 2, and guard Dual-Y narrow canvas!)
                            if (checkStillLegOverlap() && allowOutsideLegend && nPrimary == 1) {
                                const bool hasDualY = plt->axisEnabled(QwtPlot::yRight);
                                bool allowOutsideHere = true;
                                if (hasDualY) {
                                    if (allowPageResize
                                        && (ml->pageViewMode()
                                            || ml->pageUnit() != MultiLayer::UnitPx)) {
                                        const MultiLayer::PageUnit u =
                                                (ml->pageUnit() != MultiLayer::UnitPx)
                                                ? ml->pageUnit()
                                                : MultiLayer::UnitMm;
                                        const double toMm = (u == MultiLayer::UnitInch)
                                                ? 25.4
                                                : ((u == MultiLayer::UnitCm) ? 10.0 : 1.0);
                                        const double curWMm = ml->pageWidthPhysical() * toMm;
                                        const double curHMm = ml->pageHeightPhysical() * toMm;
                                        if (curWMm < 130.0) {
                                            ml->setPageSizePhysical((curWMm + 35.0) / toMm,
                                                                    curHMm / toMm, u,
                                                                    ml->pageDpi());
                                            ml->fitLayersToPage();
                                        }
                                    } else {
                                        plt->updateLayout();
                                        const int curCw = plt->canvas() ? plt->canvas()->width()
                                                                        : g->width();
                                        if (curCw - leg->rect().width() - 20 < 145)
                                            allowOutsideHere = false;
                                    }
                                }
                                if (allowOutsideHere) {
                                    g->placeLegendAnchor(Graph::LegendOutsideRight, &score, 12);
                                    if (ml->pageViewMode() || ml->pageUnit() != MultiLayer::UnitPx)
                                        ml->fitLayersToPage();
                                    g->replot();
                                    plt->updateLayout();
                                    const int postCw =
                                            plt->canvas() ? plt->canvas()->width() : g->width();
                                    if (hasDualY && postCw < 140) {
                                        g->placeLegendAnchor(Graph::LegendAutoAvoidData, &score,
                                                             10);
                                        if (ml->pageViewMode()
                                            || ml->pageUnit() != MultiLayer::UnitPx)
                                            ml->fitLayersToPage();
                                        g->replot();
                                    } else {
                                        actionsTaken.append(
                                                QStringLiteral("Moved legend outside_right on single-panel layer %1")
                                                        .arg(li));
                                    }
                                }
                            }
                        }
                        hideGuard.restore();
                    }
                    g->replot();
                }

                // Re-check callout overlaps / clipping / tick-margin violations after legend placement
                plt->updateLayout();
                const int majTickLen = std::max(4, plt->majorTickLength());
                const int spinePad = std::max(14, majTickLen + 6);
                const QRect chkCanvasRect = plt->canvas()
                        ? QRect(0, 0, plt->canvas()->width(), plt->canvas()->height())
                        : QRect(0, 0, g->width(), g->height());
                static const QRegularExpression kOutsideBadgeCheckRx(
                        QStringLiteral("^\\s*\\([A-Za-z0-9]{1,2}\\)\\s*$"));
                Legend *legAfter = g->legend();
                const bool legVisAfter = g->isLegendVisible() && legAfter
                        && !legAfter->text().trimmed().isEmpty();
                const QRect legRectAfter = legVisAfter ? legAfter->rect() : QRect();
                if (!calloutOverlaps) {
                    QVector<QRect> seenCallouts;
                    for (int tid : g->textMarkerKeys()) {
                        Legend *tm = g->textMarker(tid);
                        if (!tm || tm == legAfter || !tm->isVisible()
                            || tm->text().trimmed().isEmpty()) {
                            continue;
                        }
                        const QRect r = tm->rect();
                        const bool isOutsideBadge =
                                kOutsideBadgeCheckRx.match(tm->text().trimmed()).hasMatch()
                                && (r.left() < 0 || r.top() < 0);
                        if (isOutsideBadge)
                            continue;
                        const bool isPinned = isPinnedPeakOrBracketCallout(tm->text());
                        const int reqPadX = isPinned ? 3 : ((tm->frameStyle() != 0) ? spinePad : 4);
                        const int reqPadY = isPinned ? 4 : ((tm->frameStyle() != 0) ? spinePad : 4);
                        const QRect safeInset(
                                reqPadX, reqPadY,
                                std::max(1, chkCanvasRect.width() - 2 * reqPadX),
                                std::max(1, chkCanvasRect.height() - 2 * reqPadY));
                        if (!safeInset.contains(r)
                            || (legVisAfter && r.adjusted(-2, -2, 2, 2).intersects(legRectAfter))
                            || countCalloutCurveHits(plt, g, r) > 0) {
                            calloutOverlaps = true;
                            break;
                        }
                        for (const QRect &prevR : seenCallouts) {
                            if (r.adjusted(-2, -2, 2, 2).intersects(prevR)) {
                                calloutOverlaps = true;
                                break;
                            }
                        }
                        if (calloutOverlaps)
                            break;
                        seenCallouts.append(r);
                    }
                }

                if (calloutOverlaps) {
                    plt->updateLayout();
                    QRect canvasRect = plt->canvas()
                            ? QRect(0, 0, plt->canvas()->width(), plt->canvas()->height())
                            : QRect(0, 0, g->width(), g->height());
                    Legend *leg = g->legend();
                    bool legVis =
                            g->isLegendVisible() && leg && !leg->text().trimmed().isEmpty();
                    QRect legRect = legVis ? leg->rect() : QRect();
                    QVector<QRect> placedCalloutRects;
                    bool addedCalloutHeadroom = false;

                    for (int tid : g->textMarkerKeys()) {
                        Legend *tm = g->textMarker(tid);
                        if (!tm || tm == leg || !tm->isVisible() || tm->text().trimmed().isEmpty())
                            continue;
                        QRect curRect = tm->rect();
                        const bool isOutsideBadge =
                                kOutsideBadgeCheckRx.match(tm->text().trimmed()).hasMatch()
                                && (curRect.left() < 0 || curRect.top() < 0);
                        if (isOutsideBadge)
                            continue;

                        const bool isPinnedX = isPinnedPeakOrBracketCallout(tm->text());
                        if (!isPinnedX) {
                            const int maxAvailW = std::max(36, canvasRect.width() - 2 * spinePad);
                            if (curRect.width() > maxAvailW
                                && fitWideCalloutToCanvas(g, plt, tm, maxAvailW)) {
                                curRect = tm->rect();
                                actionsTaken.append(
                                        QStringLiteral("Auto-fitted wide text callout '%1' on layer %2")
                                                .arg(tm->text().simplified().left(24))
                                                .arg(li));
                            }
                        }

                        const int w = std::max(12, curRect.width());
                        const int h = std::max(10, curRect.height());
                        int tightW = w;
                        if (isPinnedX && tm->frameStyle() == Legend::None) {
                            static const QRegularExpression rxStripHtml(QStringLiteral("<[^>]+>"));
                            QString plainPin = tm->text();
                            plainPin.remove(rxStripHtml);
                            plainPin = plainPin.trimmed();
                            const int advW = QFontMetrics(tm->font()).horizontalAdvance(plainPin);
                            if (advW > 0)
                                tightW = std::clamp(advW + 7, 12, w);
                        }
                        const int padX = isPinnedX
                                ? 4
                                : ((canvasRect.width() - w >= 2 * spinePad)
                                           ? spinePad
                                           : std::max(2, (canvasRect.width() - w) / 2));
                        const int padTop = (canvasRect.height() - h >= 2 * spinePad)
                                ? spinePad
                                : std::max(2, (canvasRect.height() - h) / 2);
                        const int padBot = isPinnedX ? 4 : padTop;
                        const QRect safeMarginRect(
                                padX, padTop,
                                std::max(1, canvasRect.width() - 2 * padX),
                                std::max(1, canvasRect.height() - padTop - padBot));

                        const QRect curHitRect(curRect.x(), curRect.y(), tightW, h);
                        int sibHits = 0;
                        for (const QRect &otherR : placedCalloutRects) {
                            if (curHitRect.adjusted(-5, -4, 5, 4).intersects(otherR))
                                sibHits += 100;
                        }
                        const int clipPenalty = safeMarginRect.contains(curRect) ? 0 : 500;
                        int curHits = countCalloutCurveHits(plt, g, curHitRect)
                                + ((legVis && curHitRect.adjusted(-2, -2, 2, 2).intersects(legRect))
                                           ? 100
                                           : 0)
                                + sibHits + clipPenalty;
                        if (curHits <= 0) {
                            placedCalloutRects.append(curHitRect);
                            continue;
                        }

                        auto searchCalloutPocket = [&](QPoint &outOrigin, int &outHits) {
                            const int minX = padX;
                            const int maxX = std::max(minX, canvasRect.width() - w - padX);
                            const int minY = padTop;
                            const int maxY = std::max(minY, canvasRect.height() - h - padBot);

                            const int anchorX = std::clamp(curRect.x(), minX, maxX);
                            const int anchorY = std::clamp(curRect.y(), minY, maxY);
                            outOrigin = QPoint(anchorX, anchorY);
                            const QRect clampedInit(outOrigin, QSize(tightW, h));
                            int initHits = countCalloutCurveHits(plt, g, clampedInit);
                            if (legVis && clampedInit.adjusted(-4, -4, 4, 4).intersects(legRect))
                                initHits += 100;
                            for (const QRect &otherR : placedCalloutRects) {
                                if (clampedInit.adjusted(-3, -3, 3, 3).intersects(otherR))
                                    initHits += 100;
                            }
                            outHits = initHits;
                            int bestCost = initHits * 10000
                                    + std::abs(outOrigin.x() - anchorX) * (isPinnedX ? 50 : 1)
                                    + std::abs(outOrigin.y() - curRect.y());
                            if (initHits == 0 && outOrigin == curRect.topLeft())
                                return;

                            QVector<QPoint> candidates;
                            if (isPinnedX) {
                                // Crystallographic (hkl) peak labels & significance stars:
                                // keep X anchored over the peak (within <= 6 px of clamped anchorX) and lift vertically!
                                static const int kPinDx[5] = { 0, -3, 3, -6, 6 };
                                for (int dy = 0; dy >= -canvasRect.height(); dy -= 6) {
                                    for (int dx : kPinDx)
                                        candidates.append(QPoint(anchorX + dx, curRect.y() + dy));
                                }
                                for (int dy = 6; dy <= 18; dy += 6) {
                                    for (int dx : kPinDx)
                                        candidates.append(QPoint(anchorX + dx, curRect.y() + dy));
                                }
                            } else {
                                static const int kOffsets[12][2] = {
                                    { 0, -28 },  { 0, 28 },   { 28, -24 }, { -28, -24 },
                                    { 28, 24 },  { -28, 24 }, { 0, -52 },  { 0, 52 },
                                    { 56, 0 },   { -56, 0 },  { 56, -44 }, { -56, -44 }
                                };
                                for (const auto &off : kOffsets)
                                    candidates.append(curRect.topLeft() + QPoint(off[0], off[1]));
                                for (int gy = 0; gy < 5; ++gy) {
                                    for (int gx = 0; gx < 5; ++gx) {
                                        const int cx = minX
                                                + gx * std::max(4, (maxX - minX) / 4);
                                        const int cy = minY
                                                + gy * std::max(4, (maxY - minY) / 4);
                                        candidates.append(QPoint(cx, cy));
                                    }
                                }
                            }

                            for (const QPoint &pt : candidates) {
                                const int clampedX = std::clamp(pt.x(), minX, maxX);
                                const int clampedY = std::clamp(pt.y(), minY, maxY);
                                if (isPinnedX && std::abs(clampedX - anchorX) > 8)
                                    continue;
                                const QRect candRect(clampedX, clampedY, tightW, h);
                                if (legVis && candRect.adjusted(-4, -4, 4, 4).intersects(legRect))
                                    continue;
                                bool hitsSibling = false;
                                for (const QRect &otherR : placedCalloutRects) {
                                    if (candRect.adjusted(-5, -4, 5, 4).intersects(otherR)) {
                                        hitsSibling = true;
                                        break;
                                    }
                                }
                                if (hitsSibling)
                                    continue;
                                const int hits = countCalloutCurveHits(plt, g, candRect);
                                const int dxAbs = std::abs(candRect.x() - anchorX);
                                const int dyAbs = std::abs(candRect.y() - curRect.y());
                                const int dist = isPinnedX ? (dxAbs * 50 + dyAbs) : (dxAbs + dyAbs);
                                const int cost = hits * 10000 + dist;
                                if (cost < bestCost) {
                                    bestCost = cost;
                                    outHits = hits;
                                    outOrigin = candRect.topLeft();
                                    if (hits == 0 && !isPinnedX && dist <= 80)
                                        break;
                                    if (hits == 0 && isPinnedX && dxAbs == 0)
                                        break;
                                }
                            }
                        };

                        QPoint bestOrigin = curRect.topLeft();
                        int bestHits = curHits;
                        searchCalloutPocket(bestOrigin, bestHits);

                        // Dual-Y / dense canvas / tall XRD peak headroom escalation
                        if (bestHits > 0 && !addedCalloutHeadroom) {
                            if (expandLinearYHeadroom(isPinnedX ? 0.26 : 0.22)) {
                                addedCalloutHeadroom = true;
                                if (legVis && g->property("_outsideRightMargin").toInt() <= 0) {
                                    ScopedCalloutHideGuard hg(g);
                                    double sc = 0.0;
                                    g->placeLegendAnchor(Graph::LegendAutoAvoidData, &sc, 10);
                                }
                                g->replot();
                                plt->updateLayout();
                                canvasRect = plt->canvas()
                                        ? QRect(0, 0, plt->canvas()->width(),
                                                plt->canvas()->height())
                                        : QRect(0, 0, g->width(), g->height());
                                legVis = g->isLegendVisible() && leg
                                        && !leg->text().trimmed().isEmpty();
                                legRect = legVis ? leg->rect() : QRect();
                                curRect = tm->rect();
                                placedCalloutRects.clear();
                                for (int prevTid : g->textMarkerKeys()) {
                                    if (prevTid == tid)
                                        break;
                                    Legend *prevTm = g->textMarker(prevTid);
                                    if (prevTm && prevTm != leg && prevTm->isVisible()
                                        && !prevTm->text().trimmed().isEmpty()) {
                                        QRect pr = prevTm->rect();
                                        if (isPinnedPeakOrBracketCallout(prevTm->text())
                                            && prevTm->frameStyle() == Legend::None) {
                                            static const QRegularExpression rxStripHtmlPrev(
                                                    QStringLiteral("<[^>]+>"));
                                            QString pPlain = prevTm->text();
                                            pPlain.remove(rxStripHtmlPrev);
                                            const int pAdv = QFontMetrics(prevTm->font())
                                                                     .horizontalAdvance(pPlain.trimmed());
                                            if (pAdv > 0)
                                                pr.setWidth(std::clamp(pAdv + 12, 12, pr.width()));
                                        }
                                        placedCalloutRects.append(pr);
                                    }
                                }
                                searchCalloutPocket(bestOrigin, bestHits);
                                actionsTaken.append(
                                        QStringLiteral("Added Y-axis headroom on layer %1 for callout clearance")
                                                .arg(li));
                            }
                        }

                        if (bestOrigin != curRect.topLeft()) {
                            const int xAxM = (tm->xAxis() == QwtPlot::xTop) ? QwtPlot::xTop
                                                                            : QwtPlot::xBottom;
                            const int yAxM = (tm->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight
                                                                              : QwtPlot::yLeft;
                            const double newX = plt->invTransform(xAxM, bestOrigin.x());
                            const double newY = plt->invTransform(yAxM, bestOrigin.y());
                            if (std::isfinite(newX) && std::isfinite(newY)) {
                                tm->setOriginCoord(newX, newY);
                                g->replot();
                                plt->updateLayout();
                                actionsTaken.append(
                                        QStringLiteral("Relocated overlapping text callout '%1' on layer %2")
                                                .arg(tm->text().simplified().left(24))
                                                .arg(li));
                            }
                        }
                        QRect finalR = tm->rect();
                        if (isPinnedX && tm->frameStyle() == Legend::None)
                            finalR.setWidth(std::clamp(tightW, 12, std::max(12, finalR.width())));
                        placedCalloutRects.append(finalR);
                    }
                }
            }
        }
    }

    for (Graph *lg : ml->layersList()) {
        if (lg) {
            enforceArialGraphFonts(lg);
            lg->replot();
        }
    }

    const QJsonObject afterSnap = inspectGraphDetailed(ml, true, true);
    const QJsonArray afterWarnings = afterSnap.value(QStringLiteral("warnings")).toArray();
    const QJsonArray afterCodes = afterSnap.value(QStringLiteral("warning_codes")).toArray();

    if (dryRun) {
        ml->restoreSnapshotInPlace(app, dryBeforeXml);
    } else if (undoGuard && !actionsTaken.isEmpty()) {
        undoGuard->commit();
    }

    QJsonObject res;
    res[QStringLiteral("ok")] = true;
    res[QStringLiteral("graph")] = ml->name();
    res[QStringLiteral("dry_run")] = dryRun;
    res[QStringLiteral("passes")] = passesExecuted;
    res[QStringLiteral("healthy")] = afterWarnings.isEmpty();
    res[QStringLiteral("before_warning_count")] = beforeWarnings.size();
    res[QStringLiteral("after_warning_count")] = afterWarnings.size();
    res[QStringLiteral("before_warnings")] = beforeWarnings;
    res[QStringLiteral("after_warnings")] = afterWarnings;
    res[QStringLiteral("before_warning_codes")] = beforeCodes;
    res[QStringLiteral("after_warning_codes")] = afterCodes;
    res[QStringLiteral("actions_taken")] = toJsonArray(actionsTaken);
    res[QStringLiteral("actions")] = toJsonArray(actionsTaken);
    return res;
}

} // namespace

namespace rpc_internal {

int parseAxisId(const QString &axisStr)
{
    const QString a = axisStr.trimmed().toLower();
    if (a == QStringLiteral("x") || a == QStringLiteral("bottom") || a == QStringLiteral("xbottom"))
        return QwtPlot::xBottom;
    if (a == QStringLiteral("y") || a == QStringLiteral("left") || a == QStringLiteral("yleft"))
        return QwtPlot::yLeft;
    if (a == QStringLiteral("top") || a == QStringLiteral("xtop"))
        return QwtPlot::xTop;
    if (a == QStringLiteral("right") || a == QStringLiteral("yright"))
        return QwtPlot::yRight;
    throwInvalidParams(
            QStringLiteral("Unknown axis '%1' (expected 'x'|'y'|'top'|'right').").arg(axisStr));
}

ScopedGraphUndoGuard::ScopedGraphUndoGuard(MultiLayer *m, const QString &lbl, bool wrapMacro)
    : ml(m), label(lbl)
    {
        if (ml && !ml->isRestoringSnapshot()) {
            BatchExecutionContext &bctx = rpcBatchContext();
            if (bctx.active) {
                bctx.markDirtyMultiLayer(ml);
                if (!bctx.hasSnapshotted(ml)) {
                    const QString initSnap = ml->captureUndoSnapshot();
                    if (QUndoStack *us = ml->undoStack())
                        us->beginMacro(QStringLiteral("Batch Operations"));
                    bctx.markSnapshotted(ml, initSnap);
                }
                return;
            }
            if (QUndoStack *us = ml->undoStack()) {
                if (wrapMacro) {
                    us->beginMacro(label);
                    macroOpened = true;
                }
                countBefore = us->count();
                indexBefore = us->index();
            }
            beforeSnap = ml->captureUndoSnapshot();
        }
    }

ScopedGraphUndoGuard::~ScopedGraphUndoGuard()
    {
        if (macroOpened && ml) {
            if (QUndoStack *us = ml->undoStack())
                us->endMacro();
            macroOpened = false;
        }
    }

void ScopedGraphUndoGuard::checkpointBeforeSubPush()
    {
        if (!macroOpened || !ml || ml->isRestoringSnapshot() || beforeSnap.isEmpty())
            return;
        ml->pushUndoSnapshot(label, beforeSnap);
    }

void ScopedGraphUndoGuard::checkpointAfterSubPush()
    {
        if (!macroOpened || !ml || ml->isRestoringSnapshot())
            return;
        beforeSnap = ml->captureUndoSnapshot();
    }

void ScopedGraphUndoGuard::commit()
    {
        if (!ml || ml->isRestoringSnapshot())
            return;
        if (rpcBatchContext().active) {
            beforeSnap.clear();
            return;
        }
        if (macroOpened) {
            if (!beforeSnap.isEmpty())
                ml->pushUndoSnapshot(label, beforeSnap);
            if (QUndoStack *us = ml->undoStack())
                us->endMacro();
            macroOpened = false;
            beforeSnap.clear();
            return;
        }
        if (beforeSnap.isEmpty())
            return;
        if (QUndoStack *us = ml->undoStack()) {
            if (us->count() != countBefore || us->index() != indexBefore)
                return;
        }
        ml->pushUndoSnapshot(label, beforeSnap);
        beforeSnap.clear();
    }

Spectrogram::ColorMapPreset parseColorMapPreset(const QString &name, bool *outGrayScale,
                                                bool strict)
{
    if (outGrayScale)
        *outGrayScale = false;
    const QString s = name.trimmed().toLower();
    if (s == QStringLiteral("grayscale") || s == QStringLiteral("gray")
        || s == QStringLiteral("grey")) {
        if (outGrayScale)
            *outGrayScale = true;
        return Spectrogram::PresetGrayScale;
    }
    if (s == QStringLiteral("viridis"))
        return Spectrogram::PresetViridis;
    if (s == QStringLiteral("magma"))
        return Spectrogram::PresetMagma;
    if (s == QStringLiteral("inferno"))
        return Spectrogram::PresetInferno;
    if (s == QStringLiteral("plasma"))
        return Spectrogram::PresetPlasma;
    if (s == QStringLiteral("cividis"))
        return Spectrogram::PresetCividis;
    if (s == QStringLiteral("jet") || s == QStringLiteral("rainbow")
        || s == QStringLiteral("turbo"))
        return Spectrogram::PresetJet;
    if (s == QStringLiteral("bwr") || s == QStringLiteral("blue_white_red")
        || s == QStringLiteral("bluewhitered") || s == QStringLiteral("seismic"))
        return Spectrogram::PresetBlueWhiteRed;
    if (s == QStringLiteral("coolwarm") || s == QStringLiteral("cool_warm"))
        return Spectrogram::PresetCoolWarm;
    if (strict) {
        throwInvalidParams(
                QStringLiteral("Unknown colormap '%1' (expected viridis|plasma|inferno|magma|cividis|jet|coolwarm|bwr|grayscale).")
                        .arg(name));
    }
    return Spectrogram::PresetViridis;
}

QJsonObject serializeAxisInfo(Graph *g, int qwtAxis)
{
    QJsonObject ax;
    Plot *plt = g->plotWidget();
    const bool enabled = plt ? plt->axisEnabled(qwtAxis) : false;
    ax[QStringLiteral("enabled")] = enabled;
    ax[QStringLiteral("title")] = g->axisTitle(qwtAxis);

    const QFont tf = g->axisTitleFont(qwtAxis);
    const QFont nf = g->axisFont(qwtAxis);
    ax[QStringLiteral("title_font_family")] = tf.family();
    ax[QStringLiteral("title_font_size")] = fontPtJson(tf);
    ax[QStringLiteral("title_font_pt")] = fontPtJson(tf);
    ax[QStringLiteral("numbers_font_family")] = nf.family();
    ax[QStringLiteral("numbers_font_size")] = fontPtJson(nf);
    ax[QStringLiteral("numbers_font_pt")] = fontPtJson(nf);
    ax[QStringLiteral("font_family")] = tf.family();
    ax[QStringLiteral("font_size")] = fontPtJson(tf);
    ax[QStringLiteral("axis_color")] = g->axisColor(qwtAxis).name(QColor::HexRgb);
    ax[QStringLiteral("title_color")] = g->axisTitleColor(qwtAxis).name(QColor::HexRgb);
    ax[QStringLiteral("numbers_color")] = g->axisNumbersColor(qwtAxis).name(QColor::HexRgb);

    if (plt) {
        const QwtScaleDiv *div = plt->axisScaleDiv(qwtAxis);
        if (div) {
            const double lo = div->lowerBound();
            const double hi = div->upperBound();
            ax[QStringLiteral("min")] = safeJsonDouble(std::min(lo, hi));
            ax[QStringLiteral("max")] = safeJsonDouble(std::max(lo, hi));
            ax[QStringLiteral("inverted")] = (lo > hi);
        }
        const QwtScaleEngine *se = plt->axisScaleEngine(qwtAxis);
        // A broken axis has a piecewise engine; it is log10 when both sides are (as Plot reads it).
        const Graph::AxisBreakSpec brkNow = g->axisBreak(qwtAxis);
        const bool isLog = brkNow.enabled
                ? (brkNow.log10Before && brkNow.log10After)
                : (dynamic_cast<const QwtLog10ScaleEngine *>(se) != nullptr);
        ax[QStringLiteral("scale")] = isLog ? QStringLiteral("log10") : QStringLiteral("linear");

        if (auto *sd = dynamic_cast<ScaleDraw *>(plt->axisScaleDraw(qwtAxis))) {
            const int majStyle = sd->majorTicksStyle();
            ax[QStringLiteral("ticks_style")] = majStyle;
            ax[QStringLiteral("ticks_inward")] =
                    (majStyle == ScaleDraw::In || majStyle == ScaleDraw::Both);
        }
    }
    ax[QStringLiteral("step")] = safeJsonDouble(g->axisStep(qwtAxis));
    ax[QStringLiteral("backbone_visible")] = g->isAxisBackboneVisible(qwtAxis);
    ax[QStringLiteral("divide_by")] = safeJsonDouble(g->tickDivideFactor(qwtAxis));
    ax[QStringLiteral("prefix")] = g->tickPrefix(qwtAxis);
    ax[QStringLiteral("suffix")] = g->tickSuffix(qwtAxis);

    const Graph::AxisBreakSpec brk = g->axisBreak(qwtAxis);
    QJsonObject brkObj;
    brkObj[QStringLiteral("enabled")] = brk.enabled;
    brkObj[QStringLiteral("start")] = safeJsonDouble(brk.breakFrom);
    brkObj[QStringLiteral("end")] = safeJsonDouble(brk.breakTo);
    brkObj[QStringLiteral("position_pct")] = safeJsonDouble(brk.positionPct);
    brkObj[QStringLiteral("log_before")] = brk.log10Before;
    brkObj[QStringLiteral("log_after")] = brk.log10After;
    ax[QStringLiteral("axis_break")] = brkObj;
    ax[QStringLiteral("break_enabled")] = brk.enabled;
    return ax;
}

QJsonObject serializeGraphInfo(MultiLayer *ml)
{
    QJsonObject info;
    info[QStringLiteral("name")] = ml->name();
    info[QStringLiteral("label")] = ml->windowLabel();
    info[QStringLiteral("type")] = QStringLiteral("MultiLayer");
    info[QStringLiteral("rows")] = ml->getRows();
    info[QStringLiteral("cols")] = ml->getCols();
    info[QStringLiteral("page_view_mode")] = ml->pageViewMode();
    info[QStringLiteral("page_zoom")] = safeJsonDouble(ml->pageZoom());
    info[QStringLiteral("page_width")] = safeJsonDouble(ml->pageWidthPhysical());
    info[QStringLiteral("page_height")] = safeJsonDouble(ml->pageHeightPhysical());
    info[QStringLiteral("page_unit")] = pageUnitToString(ml->pageUnit());
    info[QStringLiteral("page_dpi")] = ml->pageDpi();
    info[QStringLiteral("keep_aspect_ratio")] = ml->keepAspectRatio();
    info[QStringLiteral("aspect_ratio")] = safeJsonDouble(ml->targetAspectRatio());
    info[QStringLiteral("background_color")] = ml->pageBackgroundColor().name(QColor::HexArgb);
    info[QStringLiteral("can_undo")] = ml->undoStack() ? ml->undoStack()->canUndo() : false;
    info[QStringLiteral("can_redo")] = ml->undoStack() ? ml->undoStack()->canRedo() : false;
    {
        const QString appliedPreset = ml->property("toriAppliedStylePreset").toString().trimmed();
        info[QStringLiteral("applied_style_preset")] = appliedPreset;
        info[QStringLiteral("style_preset")] = appliedPreset;
        if (!appliedPreset.isEmpty()) {
            MultiLayer::GraphStylePresetSpec presetSpec;
            if (MultiLayer::findStylePreset(appliedPreset, &presetSpec)
                && presetSpec.slidePastePercent > 0.0) {
                info[QStringLiteral("slide_paste_percent")] =
                        safeJsonDouble(presetSpec.slidePastePercent);
            }
        }
    }
    {
        QJsonObject plObj;
        plObj[QStringLiteral("enabled")] = ml->hasPanelLabels();
        plObj[QStringLiteral("placement")] =
                (ml->panelLabelPlacement() == MultiLayer::PanelInsideTopLeft)
                ? QStringLiteral("inside_top_left")
                : QStringLiteral("outside_top_left");
        plObj[QStringLiteral("pattern")] = ml->panelLabelPattern();
        plObj[QStringLiteral("font_pt")] = ml->panelLabelFontPt();
        info[QStringLiteral("panel_labels")] = plObj;
    }

    const QList<Graph *> layers = ml->layersList();
    info[QStringLiteral("layer_count")] = layers.size();

    bool allFontsArial = true;
    QJsonArray layersArr;
    for (int li = 0; li < layers.size(); ++li) {
        Graph *g = layers[li];
        if (!g)
            continue;
        QJsonObject lObj;
        lObj[QStringLiteral("index")] = li;
        lObj[QStringLiteral("is_active")] = (g == ml->activeGraph());
        lObj[QStringLiteral("framed")] = g->framed();
        lObj[QStringLiteral("frame_width")] = g->canvasFrameWidth();
        lObj[QStringLiteral("antialiasing")] = g->antialiasing();
        lObj[QStringLiteral("is_ternary")] = g->isTernaryPlot();
        lObj[QStringLiteral("has_legend")] = (g->legend() != nullptr);
        lObj[QStringLiteral("legend_visible")] = g->isLegendVisible();
        if (g->legend())
            lObj[QStringLiteral("legend_text")] = g->legend()->text();

        if (Plot *plt = g->plotWidget()) {
            const QwtText titleTxt = plt->title();
            lObj[QStringLiteral("title")] = titleTxt.text();
            lObj[QStringLiteral("title_font_family")] = titleTxt.font().family();
            lObj[QStringLiteral("title_font_size")] = fontPtJson(titleTxt.font());
            if (!titleTxt.text().isEmpty()
                && titleTxt.font().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive) != 0)
                allFontsArial = false;
        }
        if (Legend *leg = g->legend()) {
            lObj[QStringLiteral("legend_text")] = leg->text();
            lObj[QStringLiteral("legend_font_family")] = leg->font().family();
            lObj[QStringLiteral("legend_font_size")] = fontPtJson(leg->font());
            lObj[QStringLiteral("legend_columns")] = leg->numColumns();
            if (leg->font().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive) != 0)
                allFontsArial = false;
        }

        QJsonObject axesObj;
        axesObj[QStringLiteral("x")] = serializeAxisInfo(g, QwtPlot::xBottom);
        axesObj[QStringLiteral("y")] = serializeAxisInfo(g, QwtPlot::yLeft);
        axesObj[QStringLiteral("top")] = serializeAxisInfo(g, QwtPlot::xTop);
        axesObj[QStringLiteral("right")] = serializeAxisInfo(g, QwtPlot::yRight);
        lObj[QStringLiteral("axes")] = axesObj;

        for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
            if (g->plotWidget() && g->plotWidget()->axisEnabled(ax)) {
                if (g->axisFont(ax).family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive) != 0
                    || g->axisTitleFont(ax).family().compare(QStringLiteral("Arial"),
                                                             Qt::CaseInsensitive)
                            != 0) {
                    allFontsArial = false;
                }
            }
        }

        QJsonArray curvesArr;
        for (int ci = 0; ci < g->curves(); ++ci) {
            QwtPlotCurve *c = g->curve(ci);
            if (!c)
                continue;
            QJsonObject cObj;
            cObj[QStringLiteral("index")] = ci;
            cObj[QStringLiteral("title")] = c->title().text();
            cObj[QStringLiteral("type")] = g->curveType(ci);
            const int nPts = static_cast<int>(c->dataSize());
            cObj[QStringLiteral("points")] = nPts;
            cObj[QStringLiteral("y_axis")] =
                    (c->yAxis() == QwtPlot::yRight) ? QStringLiteral("right") : QStringLiteral("y");
            if (auto *bc = dynamic_cast<QwtBarCurve *>(c)) {
                cObj[QStringLiteral("bar_gap")] = bc->gap();
                cObj[QStringLiteral("bar_offset")] = bc->offset();
            }

            double xMin = std::numeric_limits<double>::infinity();
            double xMax = -std::numeric_limits<double>::infinity();
            double yMin = std::numeric_limits<double>::infinity();
            double yMax = -std::numeric_limits<double>::infinity();
            for (int k = 0; k < nPts; ++k) {
                const double xv = c->x(k);
                const double yv = c->y(k);
                if (std::isfinite(xv)) {
                    xMin = std::min(xMin, xv);
                    xMax = std::max(xMax, xv);
                }
                if (std::isfinite(yv)) {
                    yMin = std::min(yMin, yv);
                    yMax = std::max(yMax, yv);
                }
            }
            cObj[QStringLiteral("x_min")] = safeJsonDouble(xMin);
            cObj[QStringLiteral("x_max")] = safeJsonDouble(xMax);
            cObj[QStringLiteral("y_min")] = safeJsonDouble(yMin);
            cObj[QStringLiteral("y_max")] = safeJsonDouble(yMax);
            cObj[QStringLiteral("color")] = c->pen().color().name(QColor::HexRgb);
            cObj[QStringLiteral("alpha")] = c->pen().color().alpha();
            cObj[QStringLiteral("line_width")] =
                    c->pen().widthF() > 0.0 ? c->pen().widthF() : double(c->pen().width());
            cObj[QStringLiteral("line_style")] = int(c->pen().style());
            cObj[QStringLiteral("symbol_style")] = int(c->symbol().style());
            cObj[QStringLiteral("symbol_size")] = c->symbol().size().width();
            cObj[QStringLiteral("symbol_hollow")] = Graph::isSymbolWhiteHollow(c->symbol());
            cObj[QStringLiteral("visible")] = g->isCurveVisible(ci);
            if (auto *dc = dynamic_cast<DataCurve *>(c)) {
                cObj[QStringLiteral("masked_points")] = dc->maskedPointCount();
                cObj[QStringLiteral("x_column")] = dc->xColumnName();
                cObj[QStringLiteral("y_column")] = dc->yColumnName();
            }
            curvesArr.append(cObj);
        }
        lObj[QStringLiteral("curves")] = curvesArr;
        lObj[QStringLiteral("curve_count")] = curvesArr.size();

        QJsonArray specsArr;
        if (Plot *plt = g->plotWidget()) {
            const QwtPlotItemList &items = plt->itemList();
            int si = 0;
            for (QwtPlotItem *it : items) {
                if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
                        QJsonObject sObj;
                        sObj[QStringLiteral("index")] = si++;
                        sObj[QStringLiteral("matrix")] =
                                sp->matrix() ? sp->matrix()->name() : QString();
                        const QwtDoubleInterval zInt = sp->effectiveValueRange();
                        sObj[QStringLiteral("z_min")] = safeJsonDouble(zInt.minValue());
                        sObj[QStringLiteral("z_max")] = safeJsonDouble(zInt.maxValue());
                        sObj[QStringLiteral("colorbar_visible")] = sp->hasColorScale();
                        sObj[QStringLiteral("colorbar_title")] = sp->colorBarTitle();
                        sObj[QStringLiteral("levels")] = sp->levels();
                        sObj[QStringLiteral("contour_lines")] =
                                sp->testDisplayMode(QwtPlotSpectrogram::ContourMode);
                        const int cbAxis = sp->colorScaleAxis();
                        sObj[QStringLiteral("colorbar_font_family")] = g->axisFont(cbAxis).family();
                        if (sp->hasColorScale()
                            && g->axisFont(cbAxis).family().compare(QStringLiteral("Arial"),
                                                                    Qt::CaseInsensitive)
                                    != 0) {
                            allFontsArial = false;
                        }
                        specsArr.append(sObj);
                    }
                }
            }
        }
        lObj[QStringLiteral("spectrograms")] = specsArr;
        const QVector<Graph::ReferenceLineSpec> refSpecs = g->referenceLines();
        lObj[QStringLiteral("reference_line_count")] = refSpecs.size();
        QJsonArray refArr;
        for (int ri = 0; ri < refSpecs.size(); ++ri) {
            const Graph::ReferenceLineSpec &rs = refSpecs[ri];
            QJsonObject rObj;
            rObj[QStringLiteral("index")] = ri;
            rObj[QStringLiteral("orientation")] =
                    (rs.orientation == 1) ? QStringLiteral("vertical")
                                          : QStringLiteral("horizontal");
            rObj[QStringLiteral("value")] = safeJsonDouble(rs.value);
            rObj[QStringLiteral("label")] = rs.label;
            rObj[QStringLiteral("stat_mode")] = rs.statMode;
            rObj[QStringLiteral("color")] = rs.color.name(QColor::HexRgb);
            rObj[QStringLiteral("visible")] = rs.visible;
            refArr.append(rObj);
        }
        lObj[QStringLiteral("reference_lines")] = refArr;
        layersArr.append(lObj);
    }

    info[QStringLiteral("layers")] = layersArr;
    info[QStringLiteral("font_family")] =
            allFontsArial ? QStringLiteral("Arial") : QStringLiteral("Mixed");
    info[QStringLiteral("all_fonts_arial")] = allFontsArial;
    return info;
}

QJsonObject serializeGraph3DInfo(Graph3D *g3d)
{
    QJsonObject info;
    info[QStringLiteral("name")] = g3d->name();
    info[QStringLiteral("label")] = g3d->windowLabel();
    info[QStringLiteral("type")] = QStringLiteral("Graph3D");
    info[QStringLiteral("x_min")] = safeJsonDouble(g3d->xStart());
    info[QStringLiteral("x_max")] = safeJsonDouble(g3d->xStop());
    info[QStringLiteral("y_min")] = safeJsonDouble(g3d->yStart());
    info[QStringLiteral("y_max")] = safeJsonDouble(g3d->yStop());
    info[QStringLiteral("z_min")] = safeJsonDouble(g3d->zStart());
    info[QStringLiteral("z_max")] = safeJsonDouble(g3d->zStop());
    info[QStringLiteral("x_rotation")] = safeJsonDouble(g3d->xRotation());
    info[QStringLiteral("y_rotation")] = safeJsonDouble(g3d->yRotation());
    info[QStringLiteral("z_rotation")] = safeJsonDouble(g3d->zRotation());
    info[QStringLiteral("zoom")] = safeJsonDouble(g3d->zoom());
    info[QStringLiteral("x_scale")] = safeJsonDouble(g3d->xScale());
    info[QStringLiteral("y_scale")] = safeJsonDouble(g3d->yScale());
    info[QStringLiteral("z_scale")] = safeJsonDouble(g3d->zScale());
    info[QStringLiteral("title")] = g3d->plotTitle();
    info[QStringLiteral("title_font_family")] = g3d->titleFont().family();
    info[QStringLiteral("resolution")] = g3d->resolution();
    info[QStringLiteral("legend")] = g3d->isLegendOn();
    info[QStringLiteral("transparency")] = safeJsonDouble(g3d->transparency());
    info[QStringLiteral("orthogonal")] = g3d->isOrthogonal();
    info[QStringLiteral("axes_labels")] = toJsonArray(g3d->axesLabels());
    info[QStringLiteral("font_family")] = g3d->xAxisLabelFont().family();
    info[QStringLiteral("numbers_font_family")] = g3d->numbersFont().family();
    const bool arial =
            (g3d->xAxisLabelFont().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive) == 0
             && g3d->yAxisLabelFont().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive)
                     == 0
             && g3d->zAxisLabelFont().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive)
                     == 0
             && g3d->numbersFont().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive)
                     == 0);
    info[QStringLiteral("all_fonts_arial")] = arial;
    return info;
}

// Ensure every FunctionCurve on `g` is .sciprj-reload-safe:
// 1. Strip commas/tabs/newlines from FunctionCurve titles (Graph::saveCurves comma-splits header).
// 2. Convert any FunctionCurve whose formula is empty, non-muParser text (e.g. ALS baseline), or
//    evaluates to NaN into a hidden-Table-backed DataCurve preserving all points and styling.
void sanitizeGraphFunctionCurves(ApplicationWindow *app, Graph *g)
{
    if (!app || !g)
        return;
    for (int i = 0; i < g->curves(); ++i) {
        auto *fc = dynamic_cast<FunctionCurve *>(g->plotItem(i));
        if (!fc)
            continue;

        QString cleanTitle = fc->title().text();
        cleanTitle.replace(QRegularExpression(QStringLiteral("[,\\t\\r\\n]+")),
                           QStringLiteral("_"));
        if (cleanTitle.trimmed().isEmpty())
            cleanTitle = QStringLiteral("FitCurve");
        if (cleanTitle != fc->title().text())
            fc->setTitle(cleanTitle);

        if (validateFunctionCurveSilent(app, fc))
            continue;

        const int n = fc->dataSize();
        if (n < 2) {
            g->removeCurve(i);
            --i;
            continue;
        }

        std::vector<double> xv(static_cast<size_t>(n));
        std::vector<double> yv(static_cast<size_t>(n));
        auto *colX = new Column(QStringLiteral("X"), SciDAVis::ColumnMode::Numeric);
        colX->setPlotDesignation(SciDAVis::X);
        QString colYShort = cleanTitle;
        colYShort.replace(QRegularExpression(QStringLiteral("[\\s,;()\\[\\]/\\\\]+")),
                          QStringLiteral("_"));
        while (colYShort.startsWith(QLatin1Char('_')))
            colYShort.remove(0, 1);
        while (colYShort.endsWith(QLatin1Char('_')))
            colYShort.chop(1);
        if (colYShort.isEmpty())
            colYShort = QStringLiteral("Y");
        auto *colY = new Column(colYShort, SciDAVis::ColumnMode::Numeric);
        colY->setPlotDesignation(SciDAVis::Y);
        colY->setComment(cleanTitle);
        for (int k = 0; k < n; ++k) {
            xv[static_cast<size_t>(k)] = fc->x(k);
            yv[static_cast<size_t>(k)] = fc->y(k);
            colX->setValueAt(k, xv[static_cast<size_t>(k)]);
            colY->setValueAt(k, yv[static_cast<size_t>(k)]);
        }

        const QPen savedPen = fc->pen();
        const QBrush savedBrush = fc->brush();
        const QwtSymbol savedSym = fc->symbol();
        const QwtPlotCurve::CurveStyle savedStyle = fc->style();
        const int savedXAxis = fc->xAxis();
        const int savedYAxis = fc->yAxis();
        const bool savedVis = fc->isVisible();
        const int savedSymSkip = fc->symbolSkip();

        const QString tblName = app->generateUniqueName(QStringLiteral("RpcFitCurve"));
        Table *backing =
                app->newHiddenTable(tblName, cleanTitle, QList<Column *>() << colX << colY);
        if (!backing) {
            delete colX;
            delete colY;
            continue;
        }

        // Preserve canonical "TableName_ColName" on DataCurve::title() so .sciprj <curve>
        // serialization binds back to `backing` on reload without dropping the curve!
        auto *dc = new DataCurve(backing, tblName + QStringLiteral("_") + colX->name(),
                                 tblName + QStringLiteral("_") + colY->name(), 0, n - 1);
        dc->setData(xv.data(), yv.data(), n);
        dc->setPen(savedPen);
        dc->setBrush(savedBrush);
        dc->setSymbol(savedSym);
        dc->setStyle(savedStyle);
        dc->setAxis(savedXAxis, savedYAxis);
        dc->setVisible(savedVis);
        dc->setSymbolSkip(savedSymSkip);

        g->removeCurve(i);
        g->insertPlotItem(dc, Graph::Line);
        g->addFitCurve(dc);
        --i;
    }

    if (Legend *leg = g->legend()) {
        const QStringList rawLines = leg->text().split(QLatin1Char('\n'));
        QStringList deduped;
        QSet<int> seenCurveOneBased;
        for (const QString &ln : rawLines) {
            if (ln.startsWith(QStringLiteral("\\c{"))) {
                const int cb = ln.indexOf(QLatin1Char('}'));
                if (cb > 3) {
                    bool ok = false;
                    const int oneBased = ln.mid(3, cb - 3).toInt(&ok);
                    if (ok) {
                        if (seenCurveOneBased.contains(oneBased) || oneBased < 1
                            || oneBased > g->curves()) {
                            continue;
                        }
                        seenCurveOneBased.insert(oneBased);
                    }
                }
            }
            deduped.append(ln);
        }
        const QString joined = deduped.join(QLatin1Char('\n'));
        if (joined != leg->text())
            leg->setText(joined);
    }
}

bool graphHasJapaneseContext(Graph *g, int upToCurveCount)
{
    if (!g)
        return false;
    Plot *plt = g->plotWidget();
    bool anyLabel = false;
    for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
        if (plt && !plt->axisEnabled(ax))
            continue;
        const QString t = g->axisTitle(ax).trimmed();
        if (!t.isEmpty()) {
            anyLabel = true;
            if (containsJapaneseChars(t))
                return true;
        }
    }
    const int lim = std::min(upToCurveCount, g->curves());
    for (int ci = 0; ci < lim; ++ci) {
        if (QwtPlotCurve *c = g->curve(ci)) {
            const QString ct = c->title().text().trimmed();
            if (!ct.isEmpty()) {
                anyLabel = true;
                if (containsJapaneseChars(ct))
                    return true;
            }
        }
    }
    if (Legend *leg = g->legend()) {
        const QStringList lines = leg->text().split(QLatin1Char('\n'));
        for (const QString &ln : lines) {
            if (ln.startsWith(QStringLiteral("\\c{"))) {
                const int cb = ln.indexOf(QLatin1Char('}'));
                if (cb > 3) {
                    bool ok = false;
                    const int oneBased = ln.mid(3, cb - 3).toInt(&ok);
                    if (ok && oneBased >= 1 && oneBased <= lim) {
                        const QString rest = ln.mid(cb + 1).trimmed();
                        if (!rest.isEmpty()) {
                            anyLabel = true;
                            if (containsJapaneseChars(rest))
                                return true;
                        }
                    }
                }
            }
        }
    }
    return !anyLabel ? false : false;
}

void harmonizeFitCurveLabelsOnGraph(Graph *g, int curvesBefore, bool hadJapaneseBefore,
                                    const QJsonObject &params)
{
    if (!g || g->curves() <= curvesBefore)
        return;
    QString explicitLabel = params.value(QStringLiteral("fit_label")).toString().trimmed();
    if (explicitLabel.isEmpty())
        explicitLabel = params.value(QStringLiteral("label")).toString().trimmed();

    QHash<int, QString> legendLabelOverrides;
    for (int ci = std::max(0, curvesBefore); ci < g->curves(); ++ci) {
        QwtPlotCurve *c = g->curve(ci);
        if (!c)
            continue;
        auto *dc = dynamic_cast<DataCurve *>(c);
        if (!explicitLabel.isEmpty() && ci == g->curves() - 1) {
            if (dc && dc->table()) {
                const QString tName = dc->table()->name();
                const QString yFull = dc->title().text();
                const QString colShort = yFull.startsWith(tName + QLatin1Char('_'))
                        ? yFull.mid(tName.size() + 1)
                        : yFull;
                if (Column *colY = dc->table()->column(colShort)) {
                    colY->setComment(explicitLabel);
                    QString safeCol = explicitLabel;
                    safeCol.replace(QRegularExpression(QStringLiteral("[\\s,;()\\[\\]/\\\\]+")),
                                    QStringLiteral("_"));
                    if (!safeCol.isEmpty() && dc->table()->column(safeCol) == nullptr) {
                        colY->setName(safeCol);
                        dc->setTitle(tName + QStringLiteral("_") + colY->name());
                    }
                }
            } else {
                c->setTitle(explicitLabel);
            }
            legendLabelOverrides.insert(ci + 1, explicitLabel);
        } else if (!hadJapaneseBefore) {
            if (dc && dc->table()) {
                const QString tName = dc->table()->name();
                const QString yFull = dc->title().text();
                const QString colShort = yFull.startsWith(tName + QLatin1Char('_'))
                        ? yFull.mid(tName.size() + 1)
                        : yFull;
                if (Column *colY = dc->table()->column(colShort)) {
                    const QString curCom = colY->comment();
                    const QString engCom = translateJapaneseFitLabelToEnglish(curCom);
                    if (engCom != curCom && !engCom.isEmpty())
                        colY->setComment(engCom);
                }
            } else {
                const QString curTitle = c->title().text();
                const QString engTitle = translateJapaneseFitLabelToEnglish(curTitle);
                if (engTitle != curTitle && !engTitle.isEmpty())
                    c->setTitle(engTitle);
            }
        }
    }

    if (Legend *leg = g->legend()) {
        QStringList lines = leg->text().split(QLatin1Char('\n'));
        bool changed = false;
        for (int li = 0; li < lines.size(); ++li) {
            const QString &ln = lines[li];
            if (!ln.startsWith(QStringLiteral("\\c{")))
                continue;
            const int cb = ln.indexOf(QLatin1Char('}'));
            if (cb <= 3)
                continue;
            bool ok = false;
            const int oneBased = ln.mid(3, cb - 3).toInt(&ok);
            if (!ok || oneBased <= curvesBefore || oneBased > g->curves())
                continue;
            if (legendLabelOverrides.contains(oneBased)) {
                const QString nextLn =
                        QStringLiteral("\\c{%1}%2").arg(oneBased).arg(legendLabelOverrides[oneBased]);
                if (nextLn != ln) {
                    lines[li] = nextLn;
                    changed = true;
                }
            } else if (!hadJapaneseBefore) {
                const QString curLbl = ln.mid(cb + 1);
                const QString engLbl = translateJapaneseFitLabelToEnglish(curLbl);
                if (engLbl != curLbl) {
                    lines[li] = QStringLiteral("\\c{%1}%2").arg(oneBased).arg(engLbl);
                    changed = true;
                }
            }
        }
        if (changed)
            leg->setText(lines.join(QLatin1Char('\n')));
    }
}

void sanitizeAllProjectFunctionCurves(ApplicationWindow *app)
{
    if (!app)
        return;
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer")) {
            auto *ml = static_cast<MultiLayer *>(w);
            for (Graph *lg : ml->layersList())
                sanitizeGraphFunctionCurves(app, lg);
        }
    }
}

bool dispatchGraphDetailAndExportMethod(ApplicationWindow *d_app, const QString &method,
                                        const QJsonObject &params, QJsonObject &outResult)
{
    if (method == QStringLiteral("graph.set_title")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString txt = params.contains(QStringLiteral("title"))
                ? params.value(QStringLiteral("title")).toString()
                : params.value(QStringLiteral("text")).toString();
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Set Plot Title"));
        g->setTitle(txt);
        double pt = 12.0;
        if (g->plotWidget() && g->plotWidget()->title().font().pointSizeF() > 0.0)
            pt = g->plotWidget()->title().font().pointSizeF();
        if (params.contains(QStringLiteral("font_size"))
            && !params.value(QStringLiteral("font_size")).isNull()) {
            pt = std::max(4.0, parseBoundedFontPt(params, QStringLiteral("font_size"), 12.0, 1.0,
                                                  1000.0));
        }
        QFont tf(QStringLiteral("Arial"), qMax(1, qRound(pt)), QFont::Normal, false);
        tf.setPointSizeF(pt);
        tf.setHintingPreference(QFont::PreferNoHinting);
        g->setTitleFont(tf);
        if (params.contains(QStringLiteral("color"))
            && !params.value(QStringLiteral("color")).isNull()) {
            g->setTitleColor(
                    parseColorValue(params.value(QStringLiteral("color")), QColor(Qt::black)));
        }
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("title")] = txt;
        return true;
    }

    if (method == QStringLiteral("graph.set_axis")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const int ax = parseAxisId(params.contains(QStringLiteral("axis"))
                                           ? params.value(QStringLiteral("axis")).toString()
                                           : QStringLiteral("x"));

        // Auto-route top/right axis calls (when layer is omitted) to a child linked overlay layer
        const bool hasExplicitLayer = params.contains(QStringLiteral("layer"))
                && !params.value(QStringLiteral("layer")).isNull();
        if (!hasExplicitLayer && (ax == QwtPlot::xTop || ax == QwtPlot::yRight)) {
            bool parentHasCurveOnAxis = false;
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (QwtPlotCurve *pc = g->curve(ci)) {
                    if ((ax == QwtPlot::xTop && pc->xAxis() == QwtPlot::xTop)
                        || (ax == QwtPlot::yRight && pc->yAxis() == QwtPlot::yRight)) {
                        parentHasCurveOnAxis = true;
                        break;
                    }
                }
            }
            if (!parentHasCurveOnAxis) {
                for (Graph *cand : ml->layersList()) {
                    if (!cand || cand->linkedParentLayer() != g)
                        continue;
                    if ((ax == QwtPlot::xTop && cand->xLinkMode() != 0)
                        || (ax == QwtPlot::yRight
                            && (cand->yLinkMode() != 0
                                || (cand->plotWidget()
                                    && cand->plotWidget()->axisEnabled(QwtPlot::yRight))))) {
                        g = cand;
                        break;
                    }
                }
            }
        }

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Modify Axis"));
        if (params.contains(QStringLiteral("enabled"))
            && !params.value(QStringLiteral("enabled")).isNull()) {
            g->enableAxis(ax, params.value(QStringLiteral("enabled")).toBool(true));
        }

        Plot *plt = g->plotWidget();
        double curMin = 0.0, curMax = 1.0;
        bool curInv = false;
        int curScaleType = 0;
        if (plt) {
            if (const QwtScaleDiv *div = plt->axisScaleDiv(ax)) {
                curMin = std::min(div->lowerBound(), div->upperBound());
                curMax = std::max(div->lowerBound(), div->upperBound());
                curInv = (div->lowerBound() > div->upperBound());
            }
            if (dynamic_cast<const QwtLog10ScaleEngine *>(plt->axisScaleEngine(ax)))
                curScaleType = 1;
        }

        bool hasLim = false;
        double limLo = curMin, limHi = curMax;
        bool limInv = curInv;
        if (params.contains(QStringLiteral("lim")) && !params.value(QStringLiteral("lim")).isNull()) {
            if (!params.value(QStringLiteral("lim")).isArray()
                || params.value(QStringLiteral("lim")).toArray().size() != 2) {
                throwInvalidParams(
                        QStringLiteral("Parameter 'lim' must be a 2-element numeric [min, max] list."));
            }
            const QJsonArray la = params.value(QStringLiteral("lim")).toArray();
            const double raw0 = parseFiniteCoordDouble(la[0], QStringLiteral("lim[0]"));
            const double raw1 = parseFiniteCoordDouble(la[1], QStringLiteral("lim[1]"));
            validateFiniteCoordRange(raw0, raw1, QStringLiteral("Parameter 'lim'"));
            limLo = std::min(raw0, raw1);
            limHi = std::max(raw0, raw1);
            limInv = (raw0 > raw1);
            hasLim = true;
        }

        const bool hasMin = params.contains(QStringLiteral("min"))
                && !params.value(QStringLiteral("min")).isNull();
        const bool hasMax = params.contains(QStringLiteral("max"))
                && !params.value(QStringLiteral("max")).isNull();
        const bool hasStep = params.contains(QStringLiteral("step"))
                && !params.value(QStringLiteral("step")).isNull();
        const bool hasScale = params.contains(QStringLiteral("scale"))
                && !params.value(QStringLiteral("scale")).isNull();
        const bool hasInverted = params.contains(QStringLiteral("inverted"))
                && !params.value(QStringLiteral("inverted")).isNull();
        const bool hasMinor = params.contains(QStringLiteral("minor_ticks"))
                && !params.value(QStringLiteral("minor_ticks")).isNull();

        if (hasLim || hasMin || hasMax || hasStep || hasScale || hasInverted || hasMinor) {
            double lo = hasMin ? parseFiniteCoordDouble(params.value(QStringLiteral("min")),
                                                        QStringLiteral("min"))
                               : (hasLim ? limLo : curMin);
            double hi = hasMax ? parseFiniteCoordDouble(params.value(QStringLiteral("max")),
                                                        QStringLiteral("max"))
                               : (hasLim ? limHi : curMax);
            const double step = hasStep
                    ? parseFiniteCoordDouble(params.value(QStringLiteral("step")),
                                             QStringLiteral("step"))
                    : g->axisStep(ax);
            int scaleType = curScaleType;
            if (hasScale) {
                const QString sc = optionalJsonString(params, QStringLiteral("scale")).trimmed().toLower();
                if (sc == QStringLiteral("linear") || sc == QStringLiteral("lin")) {
                    scaleType = 0;
                } else if (sc == QStringLiteral("log10") || sc == QStringLiteral("log")) {
                    scaleType = 1;
                } else if (!sc.isEmpty()) {
                    throwInvalidParamsWithAvailable(
                            QStringLiteral("Unknown axis scale '%1'.").arg(sc),
                            { QStringLiteral("linear"), QStringLiteral("log10") });
                }
            }
            if (scaleType == 1 && (lo <= 0.0 || hi <= 0.0) && !hasLim && !hasMin && !hasMax) {
                const bool isX = (ax == QwtPlot::xBottom || ax == QwtPlot::xTop);
                double posMin = std::numeric_limits<double>::infinity();
                double posMax = -std::numeric_limits<double>::infinity();
                for (int ci = 0; ci < g->curves(); ++ci) {
                    const QwtPlotCurve *c = g->curve(ci);
                    if (!c)
                        continue;
                    if (isX ? (c->xAxis() != ax) : (c->yAxis() != ax))
                        continue;
                    const int nPts = c->dataSize();
                    for (int pi = 0; pi < nPts; ++pi) {
                        const double v = isX ? c->x(pi) : c->y(pi);
                        if (std::isfinite(v) && v > 0.0) {
                            posMin = std::min(posMin, v);
                            posMax = std::max(posMax, v);
                        }
                    }
                }
                if (std::isfinite(posMin) && std::isfinite(posMax) && posMin > 0.0) {
                    double autoLo = std::pow(10.0, std::floor(std::log10(posMin)));
                    double autoHi = std::pow(10.0, std::ceil(std::log10(posMax)));
                    if (autoHi <= autoLo) {
                        autoLo = posMin / 10.0;
                        autoHi = posMax * 10.0;
                    }
                    if (autoLo > 0.0 && autoHi > autoLo && std::isfinite(autoLo)
                        && std::isfinite(autoHi)) {
                        lo = autoLo;
                        hi = autoHi;
                    }
                }
            }
            validateFiniteCoordRange(lo, hi, QStringLiteral("Axis scale"), step, 2000, 1e300);
            if (scaleType == 1 && (lo <= 0.0 || hi <= 0.0)) {
                throwInvalidParams(
                        QStringLiteral("log10 axis scale requires strictly positive min and max (> 0), got [%1, %2].")
                                .arg(lo)
                                .arg(hi));
            }
            const bool inv = hasInverted ? optionalJsonBool(params, QStringLiteral("inverted"), curInv)
                                         : (hasLim ? limInv : curInv);
            const int majorTicks = plt ? std::max(4, plt->axisMaxMajor(ax)) : 8;
            int minorTicks = (scaleType == 1) ? 8 : 1;
            if (hasMinor) {
                minorTicks = parseBoundedInt(params, QStringLiteral("minor_ticks"), minorTicks, 0, 32);
            } else if (plt && scaleType == curScaleType) {
                const int existingMinor = plt->axisMaxMinor(ax);
                if (scaleType == 0 && existingMinor >= 0 && existingMinor <= 4)
                    minorTicks = existingMinor;
                else if (scaleType == 1 && existingMinor >= 1)
                    minorTicks = existingMinor;
            }
            g->recordZoomHistorySnapshot();
            g->setScale(ax, std::min(lo, hi), std::max(lo, hi), step, majorTicks, minorTicks,
                        scaleType, inv || (lo > hi));
            if (hasLim || hasMin || hasMax)
                g->setAxisUserLocked(ax, true);
        }

        if (params.contains(QStringLiteral("unit_format"))
            && !params.value(QStringLiteral("unit_format")).isNull()) {
            const QString uf =
                    params.value(QStringLiteral("unit_format")).toString().trimmed().toLower();
            if (uf == QStringLiteral("paren") || uf == QStringLiteral("parentheses")
                || uf == QStringLiteral("default")) {
                if (plt)
                    plt->setProperty("_toriAxisUnitFormatStr", QStringLiteral("paren"));
                g->setAxisUnitFormat(Column::UnitFormat::Paren);
            } else if (uf == QStringLiteral("slash") || uf == QStringLiteral("isij")
                       || uf == QStringLiteral("isij_slash")) {
                if (plt)
                    plt->setProperty("_toriAxisUnitFormatStr", QStringLiteral("slash"));
                g->setAxisUnitFormat(Column::UnitFormat::ISIJSlash);
            } else if (uf == QStringLiteral("bracket") || uf == QStringLiteral("brackets")
                       || uf == QStringLiteral("square")) {
                if (plt)
                    plt->setProperty("_toriAxisUnitFormatStr", QStringLiteral("bracket"));
                for (int ci = 0; ci < g->curves(); ++ci) {
                    auto *dc = dynamic_cast<DataCurve *>(g->curve(ci));
                    if (!dc || !dc->table())
                        continue;
                    Table *ct = dc->table();
                    const bool isX = (ax == QwtPlot::xBottom || ax == QwtPlot::xTop);
                    const int colIdx = ct->colIndex(isX ? dc->xColumnName() : dc->yColumnName());
                    if (Column *col = (colIdx >= 0 ? ct->column(colIdx) : nullptr)) {
                        const QString base = !col->longName().trimmed().isEmpty()
                                ? col->longName().trimmed()
                                : col->name().trimmed();
                        const QString u = col->units().trimmed();
                        if (!base.isEmpty() && !u.isEmpty()) {
                            g->setAxisTitleAuto(ax, false);
                            g->setAxisTitle(ax, QStringLiteral("%1 [%2]").arg(base, u));
                            break;
                        }
                    }
                }
            } else {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("Unknown unit_format '%1'.").arg(uf),
                        { QStringLiteral("paren"), QStringLiteral("slash"),
                          QStringLiteral("bracket") });
            }
        }

        if (params.contains(QStringLiteral("title"))
            && !params.value(QStringLiteral("title")).isNull()) {
            g->setAxisTitleAuto(ax, false);
            g->setAxisTitle(ax, params.value(QStringLiteral("title")).toString());
        }

        QFont tf = g->axisTitleFont(ax);
        QFont nf = g->axisFont(ax);
        if (params.contains(QStringLiteral("font_family"))
            && !params.value(QStringLiteral("font_family")).isNull()) {
            const QString fam = params.value(QStringLiteral("font_family")).toString().trimmed();
            if (!fam.isEmpty()) {
                tf.setFamily(fam);
                nf.setFamily(fam);
            }
        } else {
            if (tf.family().trimmed().isEmpty())
                tf.setFamily(QStringLiteral("Arial"));
            if (nf.family().trimmed().isEmpty())
                nf.setFamily(QStringLiteral("Arial"));
        }
        if (params.contains(QStringLiteral("font_size"))
            && !params.value(QStringLiteral("font_size")).isNull()) {
            const double pt = parseBoundedFontPt(params, QStringLiteral("font_size"), 12, 4, 96);
            tf.setPointSizeF(pt);
            nf.setPointSizeF(pt);
        }
        if (params.contains(QStringLiteral("title_font_size"))
            && !params.value(QStringLiteral("title_font_size")).isNull()) {
            tf.setPointSizeF(
                    parseBoundedFontPt(params, QStringLiteral("title_font_size"), 12, 4, 96));
        }
        if (params.contains(QStringLiteral("tick_font_size"))
            && !params.value(QStringLiteral("tick_font_size")).isNull()) {
            nf.setPointSizeF(parseBoundedFontPt(params, QStringLiteral("tick_font_size"), 10, 4, 96));
        } else if (params.contains(QStringLiteral("numbers_font_size"))
                   && !params.value(QStringLiteral("numbers_font_size")).isNull()) {
            nf.setPointSizeF(
                    parseBoundedFontPt(params, QStringLiteral("numbers_font_size"), 10, 4, 96));
        }
        tf.setHintingPreference(QFont::PreferNoHinting);
        nf.setHintingPreference(QFont::PreferNoHinting);
        g->setAxisTitleFont(ax, tf);
        g->setAxisFont(ax, nf);

        if ((params.contains(QStringLiteral("tick_format"))
             && !params.value(QStringLiteral("tick_format")).isNull())
            || (params.contains(QStringLiteral("precision"))
                && !params.value(QStringLiteral("precision")).isNull())) {
            int fmtCode = plt ? plt->axisLabelFormat(ax) : 0;
            if (params.contains(QStringLiteral("tick_format"))
                && !params.value(QStringLiteral("tick_format")).isNull()) {
                const QJsonValue fv = params.value(QStringLiteral("tick_format"));
                if (fv.isDouble()) {
                    fmtCode = std::clamp(parseStrictJsonInt(fv, QStringLiteral("tick_format")), 0, 3);
                } else {
                    const QString fs = fv.toString().trimmed().toLower();
                    if (fs == QStringLiteral("auto") || fs == QStringLiteral("automatic")
                        || fs == QStringLiteral("g")) {
                        fmtCode = 0;
                    } else if (fs == QStringLiteral("f") || fs == QStringLiteral("decimal")
                               || fs == QStringLiteral("fixed")) {
                        fmtCode = 1;
                    } else if (fs == QStringLiteral("e") || fs == QStringLiteral("scientific")
                               || fs == QStringLiteral("exp")) {
                        fmtCode = 2;
                    } else if (fs == QStringLiteral("superscript")
                               || fs == QStringLiteral("superscripts")
                               || fs == QStringLiteral("power") || fs == QStringLiteral("10^x")) {
                        fmtCode = 3;
                    } else {
                        throwInvalidParamsWithAvailable(
                                QStringLiteral("Unknown tick_format '%1'.").arg(fs),
                                { QStringLiteral("auto"), QStringLiteral("f"), QStringLiteral("e"),
                                  QStringLiteral("superscript") });
                    }
                }
            }
            int prec = plt ? plt->axisLabelPrecision(ax) : 6;
            if (params.contains(QStringLiteral("precision"))
                && !params.value(QStringLiteral("precision")).isNull()) {
                prec = std::clamp(
                        parseStrictJsonInt(params.value(QStringLiteral("precision")),
                                           QStringLiteral("precision")),
                        0, 15);
            }
            const QStringList axFormulas = g->getAxesFormulas();
            const QString curFormula = (ax >= 0 && ax < axFormulas.size()) ? axFormulas[ax] : QString();
            g->setLabelsNumericFormat(ax, fmtCode, prec, curFormula);
        }

        if ((params.contains(QStringLiteral("label_rotation"))
             && !params.value(QStringLiteral("label_rotation")).isNull())
            || (params.contains(QStringLiteral("rotation"))
                && !params.value(QStringLiteral("rotation")).isNull())) {
            const QJsonValue rv = params.contains(QStringLiteral("label_rotation"))
                    ? params.value(QStringLiteral("label_rotation"))
                    : params.value(QStringLiteral("rotation"));
            const int rot = std::clamp(qRound(rv.toDouble(0.0)), -90, 90);
            g->setAxisLabelRotation(ax, rot);
        }

        if (params.contains(QStringLiteral("show_labels"))
            && !params.value(QStringLiteral("show_labels")).isNull()) {
            const bool showLbl = params.value(QStringLiteral("show_labels")).toBool(true);
            if (showLbl && plt && !plt->axisEnabled(ax))
                g->enableAxis(ax, true);
            g->setTickLabelsOnAxis(ax, showLbl);
        }

        if (params.contains(QStringLiteral("show_ticks"))
            && !params.value(QStringLiteral("show_ticks")).isNull() && plt) {
            const bool showTks = params.value(QStringLiteral("show_ticks")).toBool(true);
            if (showTks && !plt->axisEnabled(ax))
                g->enableAxis(ax, true);
            if (!showTks) {
                plt->setMajorTicksType(ax, ScaleDraw::None);
                plt->setMinorTicksType(ax, ScaleDraw::None);
            } else {
                const int ts = params.value(QStringLiteral("ticks_inward")).toBool(true)
                        ? ScaleDraw::In
                        : ScaleDraw::Out;
                plt->setMajorTicksType(ax, ts);
                plt->setMinorTicksType(ax, ts);
            }
        }

        if (params.contains(QStringLiteral("ticks_inward"))
            && !params.value(QStringLiteral("ticks_inward")).isNull() && plt) {
            if (auto *sd = dynamic_cast<ScaleDraw *>(plt->axisScaleDraw(ax))) {
                const auto ts = params.value(QStringLiteral("ticks_inward")).toBool()
                        ? ScaleDraw::In
                        : ScaleDraw::Out;
                sd->setMajorTicksStyle(ts);
                sd->setMinorTicksStyle(ts);
            }
        }
        if ((params.contains(QStringLiteral("show_backbone"))
             && !params.value(QStringLiteral("show_backbone")).isNull())
            || (params.contains(QStringLiteral("backbone_visible"))
                && !params.value(QStringLiteral("backbone_visible")).isNull())) {
            const bool bb = params.contains(QStringLiteral("show_backbone"))
                    ? optionalJsonBool(params, QStringLiteral("show_backbone"), true)
                    : optionalJsonBool(params, QStringLiteral("backbone_visible"), true);
            g->setAxisBackboneVisible(ax, bb);
        }
        if (params.contains(QStringLiteral("color"))
            || params.contains(QStringLiteral("axis_color"))
            || params.contains(QStringLiteral("title_color"))
            || params.contains(QStringLiteral("numbers_color"))) {
            const QJsonValue baseColV = params.contains(QStringLiteral("color"))
                    ? params.value(QStringLiteral("color"))
                    : params.value(QStringLiteral("axis_color"));
            if (!baseColV.isUndefined() && !baseColV.isNull()) {
                const QColor ac = parseColorValue(baseColV, g->axisColor(ax));
                QStringList axCols = g->axesColors();
                while (axCols.size() < 4)
                    axCols.append(QStringLiteral("#000000"));
                axCols[ax] = ac.name();
                g->setAxesColors(axCols);
                if (!params.contains(QStringLiteral("title_color")))
                    g->setAxisTitleColor(ax, ac);
                if (!params.contains(QStringLiteral("numbers_color"))) {
                    QStringList numCols = g->axesNumColors();
                    while (numCols.size() < 4)
                        numCols.append(QStringLiteral("#000000"));
                    numCols[ax] = ac.name();
                    g->setAxesNumColors(numCols);
                }
            }
            if (params.contains(QStringLiteral("title_color"))
                && !params.value(QStringLiteral("title_color")).isNull()) {
                g->setAxisTitleColor(
                        ax, parseColorValue(params.value(QStringLiteral("title_color")),
                                            g->axisTitleColor(ax)));
            }
            if (params.contains(QStringLiteral("numbers_color"))
                && !params.value(QStringLiteral("numbers_color")).isNull()) {
                const QColor nc = parseColorValue(params.value(QStringLiteral("numbers_color")),
                                                  g->axisNumbersColor(ax));
                QStringList numCols = g->axesNumColors();
                while (numCols.size() < 4)
                    numCols.append(QStringLiteral("#000000"));
                numCols[ax] = nc.name();
                g->setAxesNumColors(numCols);
            }
        }
        if (params.contains(QStringLiteral("categories"))
            && !params.value(QStringLiteral("categories")).isNull()) {
            if (!params.value(QStringLiteral("categories")).isArray())
                throwInvalidParams(QStringLiteral("Parameter 'categories' must be a list of strings."));
            QStringList catList;
            for (const QJsonValue &cv : params.value(QStringLiteral("categories")).toArray())
                catList << sanitizeUtf16Surrogates(cv.toString());
            if (!catList.isEmpty())
                invokeGraphSetCategoryLabels(d_app, g, ax, catList);
        }
        if (params.contains(QStringLiteral("divide_by"))
            || params.contains(QStringLiteral("prefix"))
            || params.contains(QStringLiteral("suffix"))) {
            const double divBy = optionalJsonFiniteDouble(
                    params, QStringLiteral("divide_by"), g->tickDivideFactor(ax));
            if (divBy == 0.0)
                throwInvalidParams(QStringLiteral("Parameter 'divide_by' must be non-zero."));
            g->setTickLabelTransform(
                    ax, divBy,
                    params.contains(QStringLiteral("prefix"))
                            ? optionalJsonString(params, QStringLiteral("prefix"))
                            : g->tickPrefix(ax),
                    params.contains(QStringLiteral("suffix"))
                            ? optionalJsonString(params, QStringLiteral("suffix"))
                            : g->tickSuffix(ax));
        }
        if (plt)
            plt->updateLayout();
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult = serializeAxisInfo(g, ax);
        const QStringList curCats = invokeGraphCategoryLabels(g, ax);
        if (!curCats.isEmpty())
            outResult[QStringLiteral("categories")] = toJsonArray(curCats);
        outResult[QStringLiteral("layer")] = ml->layersList().indexOf(g);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_category_labels")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const int ax = parseAxisId(params.contains(QStringLiteral("axis"))
                                           ? optionalJsonString(params, QStringLiteral("axis"))
                                           : QStringLiteral("x"));
        const QJsonValue lblVal = params.contains(QStringLiteral("labels"))
                ? params.value(QStringLiteral("labels"))
                : params.value(QStringLiteral("categories"));
        if (!lblVal.isArray())
            throwInvalidParams(
                    QStringLiteral("graph.set_category_labels requires a 'labels' (or 'categories') string list."));
        QStringList labels;
        for (const QJsonValue &it : lblVal.toArray()) {
            if (it.isString())
                labels << sanitizeUtf16Surrogates(it.toString());
            else if (it.isDouble())
                labels << QString::number(it.toDouble(), 'g', 15);
            else
                labels << QString();
        }
        if (labels.isEmpty())
            throwInvalidParams(QStringLiteral("'labels' list must be non-empty."));
        if (labels.size() > 10000)
            throwInvalidParams(QStringLiteral("'labels' list exceeds maximum size (10,000)."));

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Set Category Labels"));
        invokeGraphSetCategoryLabels(d_app, g, ax, labels);
        if (params.contains(QStringLiteral("rotation"))
            || params.contains(QStringLiteral("label_rotation"))) {
            const int rot = params.contains(QStringLiteral("rotation"))
                    ? parseBoundedInt(params, QStringLiteral("rotation"), 0, -90, 90)
                    : parseBoundedInt(params, QStringLiteral("label_rotation"), 0, -90, 90);
            g->setAxisLabelRotation(ax, rot);
        }
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult = serializeAxisInfo(g, ax);
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("layer")] = ml->layersList().indexOf(g);
        outResult[QStringLiteral("categories")] = toJsonArray(labels);
        outResult[QStringLiteral("labels")] = toJsonArray(labels);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_axis_break")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const int ax = parseAxisId(params.contains(QStringLiteral("axis"))
                                           ? optionalJsonString(params, QStringLiteral("axis"))
                                           : QStringLiteral("x"));
        const bool enabled = optionalJsonBool(params, QStringLiteral("enabled"), true);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Modify Axis Break"));
        if (!enabled) {
            g->clearAxisBreak(ax);
        } else {
            Graph::AxisBreakSpec spec = g->axisBreak(ax);
            spec.enabled = true;
            const double bStart = params.contains(QStringLiteral("start"))
                    ? optionalJsonFiniteDouble(params, QStringLiteral("start"), spec.breakFrom)
                    : optionalJsonFiniteDouble(params, QStringLiteral("from"), spec.breakFrom);
            const double bEnd = params.contains(QStringLiteral("end"))
                    ? optionalJsonFiniteDouble(params, QStringLiteral("end"), spec.breakTo)
                    : optionalJsonFiniteDouble(params, QStringLiteral("to"), spec.breakTo);
            validateFiniteCoordRange(bStart, bEnd, QStringLiteral("Axis break [start, end]"));
            if (bStart >= bEnd) {
                throwInvalidParams(
                        QStringLiteral("Axis break requires start < end, got [%1, %2].")
                                .arg(bStart)
                                .arg(bEnd));
            }
            spec.breakFrom = bStart;
            spec.breakTo = bEnd;
            double pct = optionalJsonFiniteDouble(params, QStringLiteral("position_pct"), 0.50, 100.0);
            if (pct > 1.0)
                pct /= 100.0;
            spec.positionPct = std::clamp(pct, 0.10, 0.90);
            spec.stepBefore = optionalJsonFiniteDouble(params, QStringLiteral("step_before"),
                                                       spec.stepBefore);
            spec.stepAfter = optionalJsonFiniteDouble(params, QStringLiteral("step_after"),
                                                      spec.stepAfter);
            if (spec.stepBefore < 0.0 || spec.stepAfter < 0.0)
                throwInvalidParams(QStringLiteral("step_before and step_after must be >= 0."));
            const Plot *plt = g->plotWidget();
            const bool axisIsLog = plt && Plot::isLog10ScaleEngine(plt->axisScaleEngine(ax));
            // log_before / log_after default to the axis' current scale, as the axis dialog
            // does: an axis that already has a break keeps the scales of its two sides, any
            // other axis is broken on its own scale (a log10 axis stays log10 on both sides).
            const bool hadBreak = g->axisBreak(ax).enabled || g->axisBreakSuspended(ax);
            const bool defLogBefore = hadBreak ? spec.log10Before : axisIsLog;
            const bool defLogAfter = hadBreak ? spec.log10After : axisIsLog;
            spec.log10Before = optionalJsonBool(params, QStringLiteral("log_before"), defLogBefore);
            spec.log10After = optionalJsonBool(params, QStringLiteral("log_after"), defLogAfter);
            if ((axisIsLog || spec.log10Before) && spec.breakFrom <= 0.0) {
                throwInvalidParams(
                        QStringLiteral("Axis break on log10 scale requires start > 0, got %1.")
                                .arg(spec.breakFrom));
            }
            if ((axisIsLog || spec.log10After) && spec.breakTo <= 0.0) {
                throwInvalidParams(
                        QStringLiteral("Axis break on log10 scale requires end > 0, got %1.")
                                .arg(spec.breakTo));
            }
            if (plt && plt->axisScaleDiv(ax)) {
                const QwtScaleDiv *div = plt->axisScaleDiv(ax);
                const double axMin = std::min(div->lowerBound(), div->upperBound());
                const double axMax = std::max(div->lowerBound(), div->upperBound());
                if (std::isfinite(axMin) && std::isfinite(axMax) && axMax > axMin
                    && (spec.breakFrom <= axMin || spec.breakTo >= axMax)) {
                    throwInvalidParams(
                            QStringLiteral("Axis break [%1, %2] must lie strictly within current axis range (%3, %4).")
                                    .arg(spec.breakFrom)
                                    .arg(spec.breakTo)
                                    .arg(axMin)
                                    .arg(axMax));
                }
            }
            g->setAxisBreak(ax, spec);
        }
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult = serializeAxisInfo(g, ax);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("layer")] = ml->layersList().indexOf(g);
        return true;
    }

    if (method == QStringLiteral("graph.add_reference_line")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        Graph::ReferenceLineSpec spec;
        const QString ori = optionalJsonString(params, QStringLiteral("orientation")).trimmed().toLower();
        const QString ax = optionalJsonString(params, QStringLiteral("axis")).trimmed().toLower();
        if (!ori.isEmpty() && ori != QStringLiteral("horizontal") && ori != QStringLiteral("h")
            && ori != QStringLiteral("y") && ori != QStringLiteral("vertical")
            && ori != QStringLiteral("v") && ori != QStringLiteral("x")) {
            throwInvalidParams(
                    QStringLiteral("Unknown reference line orientation '%1' (expected 'horizontal'|'vertical').")
                            .arg(ori));
        }
        if (!ax.isEmpty() && ax != QStringLiteral("x") && ax != QStringLiteral("y")
            && ax != QStringLiteral("bottom") && ax != QStringLiteral("left")
            && ax != QStringLiteral("top") && ax != QStringLiteral("right")
            && ax != QStringLiteral("xbottom") && ax != QStringLiteral("yleft")
            && ax != QStringLiteral("xtop") && ax != QStringLiteral("yright")) {
            throwInvalidParams(
                    QStringLiteral("Unknown reference line axis '%1' (expected 'x'|'y'|'bottom'|'left'|'top'|'right').")
                            .arg(ax));
        }
        spec.orientation = (ori == QStringLiteral("vertical") || ori == QStringLiteral("v")
                            || ori == QStringLiteral("x") || ax == QStringLiteral("x")
                            || ax == QStringLiteral("bottom") || ax == QStringLiteral("top")
                            || ax == QStringLiteral("xbottom") || ax == QStringLiteral("xtop"))
                ? 1
                : 0;
        spec.value = optionalJsonFiniteDouble(params, QStringLiteral("value"), 0.0);
        spec.statMode = optionalJsonString(params, QStringLiteral("stat_mode"));
        spec.color =
                parseColorValue(params.value(QStringLiteral("color")), QColor(30, 102, 184));
        const QJsonValue stV = params.contains(QStringLiteral("style"))
                ? params.value(QStringLiteral("style"))
                : params.value(QStringLiteral("line_style"));
        spec.style = parsePenStyleValue(stV, Qt::DashLine);
        spec.width = params.contains(QStringLiteral("width"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("width"), 1.5, 50.0)
                : optionalJsonFiniteDouble(params, QStringLiteral("line_width"), 1.5, 50.0);
        spec.label = optionalJsonString(params, QStringLiteral("label"));
        spec.fillToAxis = optionalJsonBool(params, QStringLiteral("fill_to_axis"), false);
        spec.fillColor = parseColorValue(params.value(QStringLiteral("fill_color")),
                                         QColor(30, 102, 184, 32));
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Reference Line"));
        g->addReferenceLine(spec);
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("index")] = std::max(0, g->referenceLines().size() - 1);
        outResult[QStringLiteral("reference_line_count")] = g->referenceLines().size();
        return true;
    }

    if (method == QStringLiteral("graph.remove_reference_line")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        if (!params.contains(QStringLiteral("index"))
            || params.value(QStringLiteral("index")).isNull()) {
            throwInvalidParams(
                    QStringLiteral("graph.remove_reference_line requires 0-based 'index'."));
        }
        const int idx = parseStrictJsonInt(params.value(QStringLiteral("index")),
                                           QStringLiteral("index"));
        const int nRefs = g->referenceLines().size();
        if (idx < 0 || idx >= nRefs) {
            QStringList avail;
            for (int i = 0; i < nRefs; ++i)
                avail.append(QString::number(i));
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Reference line index %1 out of range (count=%2).")
                            .arg(idx)
                            .arg(nRefs),
                    avail);
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Remove Reference Line"));
        g->removeReferenceLine(idx);
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("removed_index")] = idx;
        outResult[QStringLiteral("reference_line_count")] = g->referenceLines().size();
        return true;
    }

    if (method == QStringLiteral("graph.clear_reference_lines")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const bool includeBands = optionalJsonBool(params, QStringLiteral("include_bands"), false);
        const int prevCount = g->referenceLines().size();
        const int prevBands = g->shadedBandCount();
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Clear Reference Lines"));
        g->clearReferenceLines();
        if (includeBands)
            g->clearShadedBands();
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("removed")] = prevCount;
        outResult[QStringLiteral("reference_line_count")] = 0;
        if (includeBands) {
            outResult[QStringLiteral("removed_bands")] = prevBands;
            outResult[QStringLiteral("shaded_band_count")] = g->shadedBandCount();
        }
        return true;
    }

    if (method == QStringLiteral("graph.add_shaded_band")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        QVector<double> xv, yLo, yHi;
        if (params.value(QStringLiteral("x")).isArray()
            && params.value(QStringLiteral("y_low")).isArray()
            && params.value(QStringLiteral("y_high")).isArray()) {
            const QJsonArray xa = params.value(QStringLiteral("x")).toArray();
            const QJsonArray la = params.value(QStringLiteral("y_low")).toArray();
            const QJsonArray ha = params.value(QStringLiteral("y_high")).toArray();
            const int n = std::min({ xa.size(), la.size(), ha.size() });
            if (n < 2 || n > 1000000) {
                throwInvalidParams(
                        QStringLiteral("Shaded band arrays require between 2 and 1,000,000 points."));
            }
            for (int i = 0; i < n; ++i) {
                xv.append(parseFiniteCoordDouble(xa[i], QStringLiteral("x")));
                yLo.append(parseFiniteCoordDouble(la[i], QStringLiteral("y_low")));
                yHi.append(parseFiniteCoordDouble(ha[i], QStringLiteral("y_high")));
            }
        } else if (params.contains(QStringLiteral("y_min"))
                   && params.contains(QStringLiteral("y_max"))) {
            double x0 = 0.0, x1 = 1.0;
            if (g->plotWidget() && g->plotWidget()->axisScaleDiv(QwtPlot::xBottom)) {
                const auto *d = g->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
                x0 = std::min(d->lowerBound(), d->upperBound());
                x1 = std::max(d->lowerBound(), d->upperBound());
            }
            x0 = optionalJsonFiniteDouble(params, QStringLiteral("x_min"), x0);
            x1 = optionalJsonFiniteDouble(params, QStringLiteral("x_max"), x1);
            const double yl = parseFiniteCoordDouble(params.value(QStringLiteral("y_min")),
                                                     QStringLiteral("y_min"));
            const double yh = parseFiniteCoordDouble(params.value(QStringLiteral("y_max")),
                                                     QStringLiteral("y_max"));
            xv << x0 << x1;
            yLo << yl << yl;
            yHi << yh << yh;
        } else {
            throwInvalidParams(
                    QStringLiteral("Provide ('x', 'y_low', 'y_high') arrays or ('y_min', 'y_max')."));
        }
        QColor fillCol = parseColorValue(params.value(QStringLiteral("color")),
                                         QColor(30, 102, 184, 55));
        if (params.contains(QStringLiteral("alpha"))) {
            const double rawA = params.value(QStringLiteral("alpha")).toDouble(55.0);
            const int a255 = (rawA <= 1.0 && rawA >= 0.0) ? int(std::round(rawA * 255.0))
                                                          : std::clamp(int(rawA), 0, 255);
            fillCol.setAlpha(a255);
        }
        const QString bandTitle = params.contains(QStringLiteral("title"))
                ? optionalJsonString(params, QStringLiteral("title"))
                : optionalJsonString(params, QStringLiteral("label"));
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Shaded Band"));
        g->addShadedBand(xv, yLo, yHi, fillCol, bandTitle);
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_grid")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Modify Grid"));
        Grid *grd = g->grid();
        if (grd) {
            if (params.contains(QStringLiteral("enabled"))) {
                const bool on = optionalJsonBool(params, QStringLiteral("enabled"), true);
                grd->enableX(on);
                grd->enableY(on);
            }
            if (params.contains(QStringLiteral("major_x"))
                || params.contains(QStringLiteral("x_major"))) {
                const bool on = params.contains(QStringLiteral("major_x"))
                        ? optionalJsonBool(params, QStringLiteral("major_x"), true)
                        : optionalJsonBool(params, QStringLiteral("x_major"), true);
                grd->enableX(on);
            }
            if (params.contains(QStringLiteral("major_y"))
                || params.contains(QStringLiteral("y_major"))) {
                const bool on = params.contains(QStringLiteral("major_y"))
                        ? optionalJsonBool(params, QStringLiteral("major_y"), true)
                        : optionalJsonBool(params, QStringLiteral("y_major"), true);
                grd->enableY(on);
            }
            if (params.contains(QStringLiteral("minor_x"))
                || params.contains(QStringLiteral("x_minor"))) {
                const bool on = params.contains(QStringLiteral("minor_x"))
                        ? optionalJsonBool(params, QStringLiteral("minor_x"), false)
                        : optionalJsonBool(params, QStringLiteral("x_minor"), false);
                grd->enableXMin(on);
            }
            if (params.contains(QStringLiteral("minor_y"))
                || params.contains(QStringLiteral("y_minor"))) {
                const bool on = params.contains(QStringLiteral("minor_y"))
                        ? optionalJsonBool(params, QStringLiteral("minor_y"), false)
                        : optionalJsonBool(params, QStringLiteral("y_minor"), false);
                grd->enableYMin(on);
            }
            if (params.contains(QStringLiteral("zero_x")))
                grd->enableZeroLineX(optionalJsonBool(params, QStringLiteral("zero_x"), false));
            if (params.contains(QStringLiteral("zero_y")))
                grd->enableZeroLineY(optionalJsonBool(params, QStringLiteral("zero_y"), false));
            if (params.contains(QStringLiteral("color"))
                || params.contains(QStringLiteral("style"))
                || params.contains(QStringLiteral("line_style"))
                || params.contains(QStringLiteral("width"))
                || params.contains(QStringLiteral("line_width"))) {
                QPen p = grd->majPenX();
                if (params.contains(QStringLiteral("color")))
                    p.setColor(parseColorValue(params.value(QStringLiteral("color")), p.color()));
                const QJsonValue stV = params.contains(QStringLiteral("style"))
                        ? params.value(QStringLiteral("style"))
                        : params.value(QStringLiteral("line_style"));
                if (!stV.isUndefined() && !stV.isNull())
                    p.setStyle(parsePenStyleValue(stV, p.style()));
                const double wd = params.contains(QStringLiteral("width"))
                        ? optionalJsonFiniteDouble(params, QStringLiteral("width"), 0.8, 20.0)
                        : (params.contains(QStringLiteral("line_width"))
                                   ? optionalJsonFiniteDouble(params, QStringLiteral("line_width"),
                                                              0.8, 20.0)
                                   : p.widthF());
                p.setWidthF(std::max(0.2, wd));
                grd->setMajPenX(p);
                grd->setMajPenY(p);
            }
            if (params.contains(QStringLiteral("minor_color"))
                || params.contains(QStringLiteral("minor_style"))
                || params.contains(QStringLiteral("minor_width"))) {
                QPen mp = grd->minPenX();
                if (params.contains(QStringLiteral("minor_color")))
                    mp.setColor(parseColorValue(params.value(QStringLiteral("minor_color")),
                                                mp.color()));
                if (params.contains(QStringLiteral("minor_style")))
                    mp.setStyle(parsePenStyleValue(params.value(QStringLiteral("minor_style")),
                                                   mp.style()));
                if (params.contains(QStringLiteral("minor_width")))
                    mp.setWidthF(std::max(0.2,
                                          optionalJsonFiniteDouble(params,
                                                                   QStringLiteral("minor_width"),
                                                                   0.5, 20.0)));
                grd->setMinPenX(mp);
                grd->setMinPenY(mp);
            }
        }
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_curve_style")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        int cIdx = -1;
        if (params.contains(QStringLiteral("curve_index"))
            && !params.value(QStringLiteral("curve_index")).isNull()) {
            cIdx = parseStrictJsonInt(params.value(QStringLiteral("curve_index")),
                                      QStringLiteral("curve_index"));
            if (cIdx < 0 || cIdx >= g->curves()) {
                throwInvalidParams(
                        QStringLiteral("curve_index %1 out of range [0, %2).")
                                .arg(cIdx)
                                .arg(g->curves()));
            }
        } else if (params.contains(QStringLiteral("curve"))
                   && !params.value(QStringLiteral("curve")).isNull()) {
            const QJsonValue cv = params.value(QStringLiteral("curve"));
            if (cv.isDouble()) {
                cIdx = parseStrictJsonInt(cv, QStringLiteral("curve"));
                if (cIdx < 0 || cIdx >= g->curves()) {
                    throwInvalidParams(
                            QStringLiteral("curve index %1 out of range [0, %2).")
                                    .arg(cIdx)
                                    .arg(g->curves()));
                }
            } else {
                cIdx = resolveCurveIndex(g, params, QStringLiteral("curve"), true);
            }
        } else if (g->curves() > 0) {
            cIdx = 0;
        }
        QwtPlotCurve *c = (cIdx >= 0) ? g->curve(cIdx) : nullptr;
        if (!c)
            throwInvalidParams(QStringLiteral("Curve not found on graph '%1'.").arg(ml->name()));

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Change Curve Style"));
        applyInlineCurveStyleParams(g, cIdx, params, true);
        if (auto *vc = dynamic_cast<VectorCurve *>(c)) {
            if (params.contains(QStringLiteral("color"))
                && !params.value(QStringLiteral("color")).isNull()) {
                vc->setColor(parseColorValue(params.value(QStringLiteral("color")), vc->color()));
            }
            const QJsonValue lwV = params.contains(QStringLiteral("line_width"))
                    ? params.value(QStringLiteral("line_width"))
                    : (params.contains(QStringLiteral("lw"))
                               ? params.value(QStringLiteral("lw"))
                               : params.value(QStringLiteral("linewidth")));
            if (!lwV.isUndefined() && !lwV.isNull())
                vc->setWidth(std::clamp(qRound(lwV.toDouble(1.0)), 1, 20));
            if (params.contains(QStringLiteral("head_length")))
                vc->setHeadLength(parseBoundedInt(params, QStringLiteral("head_length"), 8, 1, 64));
            if (params.contains(QStringLiteral("head_angle")))
                vc->setHeadAngle(parseBoundedInt(params, QStringLiteral("head_angle"), 45, 5, 89));
        }

        QColor col = c->pen().color();
        if (params.contains(QStringLiteral("fill_area"))) {
            if (params.value(QStringLiteral("fill_area")).toBool(false)) {
                QColor fc = parseColorValue(params.value(QStringLiteral("fill_color")), col);
                if (!params.contains(QStringLiteral("fill_color")))
                    fc.setAlpha(64);
                g->setCurveBrush(cIdx, QBrush(fc, Qt::SolidPattern));
            } else {
                g->setCurveBrush(cIdx, QBrush(Qt::NoBrush));
            }
        }

        if (params.contains(QStringLiteral("bar_gap"))
            || params.contains(QStringLiteral("bar_offset"))) {
            auto *bc = dynamic_cast<QwtBarCurve *>(c);
            if (!bc) {
                throwInvalidParams(
                        QStringLiteral("Curve %1 on '%2' is not a bar curve.")
                                .arg(cIdx)
                                .arg(ml->name()));
            }
            int gap = bc->gap();
            int off = bc->offset();
            if (params.contains(QStringLiteral("bar_gap"))
                && !params.value(QStringLiteral("bar_gap")).isNull()) {
                gap = parseStrictJsonInt(params.value(QStringLiteral("bar_gap")),
                                         QStringLiteral("bar_gap"));
                if (gap < 0 || gap >= 100)
                    throwInvalidParams(QStringLiteral("bar_gap must be in [0, 99]."));
            }
            if (params.contains(QStringLiteral("bar_offset"))
                && !params.value(QStringLiteral("bar_offset")).isNull()) {
                off = parseStrictJsonInt(params.value(QStringLiteral("bar_offset")),
                                         QStringLiteral("bar_offset"));
            }
            g->setBarsGap(cIdx, gap, off);
        }

        if (params.contains(QStringLiteral("y_axis"))
            && !params.value(QStringLiteral("y_axis")).isNull()) {
            const QString ya = params.value(QStringLiteral("y_axis")).toString().trimmed().toLower();
            int targetYAxis = QwtPlot::yLeft;
            if (ya == QStringLiteral("right") || ya == QStringLiteral("yright")
                || ya == QStringLiteral("y2")) {
                targetYAxis = QwtPlot::yRight;
            } else if (ya == QStringLiteral("left") || ya == QStringLiteral("yleft")
                       || ya == QStringLiteral("y") || ya == QStringLiteral("y1")) {
                targetYAxis = QwtPlot::yLeft;
            } else {
                throwInvalidParams(
                        QStringLiteral("Unknown y_axis '%1' (expected 'left'|'right').")
                                .arg(params.value(QStringLiteral("y_axis")).toString()));
            }
            c->setYAxis(targetYAxis);
            if (targetYAxis == QwtPlot::yRight) {
                g->enableAxis(QwtPlot::yRight, true);
                if (Plot *plt = g->plotWidget()) {
                    plt->enableAxis(QwtPlot::yRight, true);
                    if (QwtScaleDraw *sd = plt->axisScaleDraw(QwtPlot::yRight)) {
                        sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                        sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
                        sd->enableComponent(QwtAbstractScaleDraw::Backbone, true);
                    }
                }
                g->setAutoScaleAxis(QwtPlot::yRight);
                g->setAutoScaleAxis(QwtPlot::yLeft);
                enforceArialGraphFonts(g);
            } else {
                g->setAutoScaleAxis(QwtPlot::yLeft);
            }
        }

        if (params.contains(QStringLiteral("visible"))
            && !params.value(QStringLiteral("visible")).isNull()) {
            g->showCurve(cIdx, params.value(QStringLiteral("visible")).toBool(true));
        }

        g->replot();
        undoGuard.commit();
        c = g->curve(cIdx);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("curve_index")] = cIdx;
        outResult[QStringLiteral("visible")] = g->isCurveVisible(cIdx);
        outResult[QStringLiteral("symbol_hollow")] =
                c ? Graph::isSymbolWhiteHollow(c->symbol()) : false;
        outResult[QStringLiteral("y_axis")] =
                (c && c->yAxis() == QwtPlot::yRight) ? QStringLiteral("right")
                                                     : QStringLiteral("left");
        return true;
    }

    if (method == QStringLiteral("graph.set_curve_visible")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        int idx = -1;
        if (params.contains(QStringLiteral("curve"))
            && !params.value(QStringLiteral("curve")).isNull()) {
            const QJsonValue cv = params.value(QStringLiteral("curve"));
            if (cv.isDouble()) {
                idx = parseStrictJsonInt(cv, QStringLiteral("curve"));
            } else {
                idx = resolveCurveIndex(g, params, QStringLiteral("curve"), true);
            }
        } else if (params.contains(QStringLiteral("curve_index"))
                   && !params.value(QStringLiteral("curve_index")).isNull()) {
            idx = parseStrictJsonInt(params.value(QStringLiteral("curve_index")),
                                     QStringLiteral("curve_index"));
        } else if (g->curves() > 0) {
            idx = 0;
        }
        if (idx < 0 || idx >= g->curves()) {
            throwInvalidParams(
                    QStringLiteral("Curve index %1 out of range [0, %2) on graph '%3'.")
                            .arg(idx)
                            .arg(g->curves())
                            .arg(ml->name()));
        }
        const bool visible = params.contains(QStringLiteral("visible"))
                ? params.value(QStringLiteral("visible")).toBool(true)
                : true;
        ScopedGraphUndoGuard undoGuard(
                ml, visible ? QObject::tr("Show Curve") : QObject::tr("Hide Curve"));
        g->showCurve(idx, visible);
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("curve_index")] = idx;
        outResult[QStringLiteral("visible")] = g->isCurveVisible(idx);
        return true;
    }

    if (method == QStringLiteral("graph.move_curve")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        int fromIdx = -1;
        if (params.contains(QStringLiteral("from_index"))
            && !params.value(QStringLiteral("from_index")).isNull()) {
            fromIdx = parseStrictJsonInt(params.value(QStringLiteral("from_index")),
                                         QStringLiteral("from_index"));
        } else if (params.contains(QStringLiteral("curve"))
                   && !params.value(QStringLiteral("curve")).isNull()) {
            const QJsonValue cv = params.value(QStringLiteral("curve"));
            if (cv.isDouble()) {
                fromIdx = parseStrictJsonInt(cv, QStringLiteral("curve"));
            } else {
                fromIdx = resolveCurveIndex(g, params, QStringLiteral("curve"), true);
            }
        } else {
            throwInvalidParams(QStringLiteral("from_index (or curve) is required."));
        }

        int toIdx = -1;
        if (params.contains(QStringLiteral("to_index"))
            && !params.value(QStringLiteral("to_index")).isNull()) {
            toIdx = parseStrictJsonInt(params.value(QStringLiteral("to_index")),
                                       QStringLiteral("to_index"));
        } else if (params.contains(QStringLiteral("delta"))
                   && !params.value(QStringLiteral("delta")).isNull()) {
            const int d = parseBoundedInt(params, QStringLiteral("delta"), 0, -1000, 1000);
            toIdx = std::clamp(fromIdx + d, 0, std::max(0, g->curves() - 1));
        } else if (params.contains(QStringLiteral("direction"))
                   && !params.value(QStringLiteral("direction")).isNull()) {
            const QString dir = optionalJsonString(params, QStringLiteral("direction"))
                                        .trimmed()
                                        .toLower();
            const int maxIdx = std::max(0, g->curves() - 1);
            if (dir == QStringLiteral("up") || dir == QStringLiteral("forward")
                || dir == QStringLiteral("front")) {
                toIdx = std::min(maxIdx, fromIdx + 1);
            } else if (dir == QStringLiteral("down") || dir == QStringLiteral("backward")
                       || dir == QStringLiteral("back")) {
                toIdx = std::max(0, fromIdx - 1);
            } else if (dir == QStringLiteral("top") || dir == QStringLiteral("last")) {
                toIdx = maxIdx;
            } else if (dir == QStringLiteral("bottom") || dir == QStringLiteral("first")) {
                toIdx = 0;
            } else {
                throwInvalidParams(
                        QStringLiteral("Unknown move_curve direction '%1' (expected 'up'|'down'|'top'|'bottom').")
                                .arg(dir));
            }
        } else {
            throwInvalidParams(QStringLiteral("to_index, direction, or delta is required."));
        }

        if (fromIdx < 0 || fromIdx >= g->curves() || toIdx < 0 || toIdx >= g->curves()) {
            throwInvalidParams(
                    QStringLiteral("Curve index out of range [0, %1) (from=%2, to=%3).")
                            .arg(g->curves())
                            .arg(fromIdx)
                            .arg(toIdx));
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Move Curve"));
        const bool moved = (fromIdx == toIdx) ? true : g->moveCurve(fromIdx, toIdx);
        replotGraphOrDefer(g);
        if (moved && fromIdx != toIdx)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = moved;
        outResult[QStringLiteral("from_index")] = fromIdx;
        outResult[QStringLiteral("to_index")] = toIdx;
        outResult[QStringLiteral("curves")] = toJsonArray(g->plotItemsList());
        return true;
    }

    if (method == QStringLiteral("graph.set_stack_offsets")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QJsonValue modeVal = params.value(QStringLiteral("mode"));
        const QString modeStr = modeVal.toString().trimmed().toLower();
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Stack Offsets"));
        if (params.value(QStringLiteral("reset")).toBool(false)
            || modeStr == QStringLiteral("none") || modeStr == QStringLiteral("off")
            || modeStr == QStringLiteral("reset")) {
            g->resetStackOffsets();
        } else {
            int mode = 1;
            if (modeVal.isDouble()) {
                mode = modeVal.toInt(1);
            } else if (modeStr == QStringLiteral("uniform") || modeStr == QStringLiteral("0")) {
                mode = 0;
            } else if (params.contains(QStringLiteral("auto_waterfall"))
                       && !params.value(QStringLiteral("auto_waterfall")).isNull()) {
                mode = optionalJsonBool(params, QStringLiteral("auto_waterfall"), true) ? 1 : 0;
            }
            const double yGap = params.contains(QStringLiteral("y_gap_pct"))
                    ? optionalJsonFiniteDouble(params, QStringLiteral("y_gap_pct"), 15.0, 10000.0)
                    : (params.contains(QStringLiteral("y_offset_pct"))
                               ? optionalJsonFiniteDouble(params, QStringLiteral("y_offset_pct"), 15.0, 10000.0)
                               : optionalJsonFiniteDouble(params, QStringLiteral("y_offset"), 15.0, 10000.0));
            const double xGap = params.contains(QStringLiteral("x_gap_pct"))
                    ? optionalJsonFiniteDouble(params, QStringLiteral("x_gap_pct"), 0.0, 10000.0)
                    : (params.contains(QStringLiteral("x_offset_pct"))
                               ? optionalJsonFiniteDouble(params, QStringLiteral("x_offset_pct"), 0.0, 10000.0)
                               : optionalJsonFiniteDouble(params, QStringLiteral("x_offset"), 0.0, 10000.0));
            const bool norm01 = optionalJsonBool(params, QStringLiteral("normalize"), false);
            g->setStackOffsets(mode, yGap, xGap, norm01);
        }
        g->replot();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.apply_series_scheme")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const int groupSize = parseBoundedInt(params, QStringLiteral("group_size"), 2, 1, 64);
        const QString schemeStr = params.contains(QStringLiteral("scheme"))
                ? optionalJsonString(params, QStringLiteral("scheme"),
                                     QStringLiteral("bw_hollow_filled"))
                : QStringLiteral("bw_hollow_filled");
        QString normScheme = schemeStr.trimmed().toLower();
        normScheme.replace(QLatin1Char('-'), QLatin1Char('_'));
        normScheme.replace(QLatin1Char(' '), QLatin1Char('_'));
        if (normScheme == QStringLiteral("okabe_ito") || normScheme == QStringLiteral("okabe")) {
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Apply Series Style Scheme"));
            g->applyColorPaletteByName(QStringLiteral("Okabe-Ito"), false);
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (g->curveType(ci) != Graph::ErrorBars)
                    applyInlineCurveStyleParams(g, ci, params, false);
            }
            enforceArialGraphFonts(g);
            replotGraphOrDefer(g);
            undoGuard.commit();
            outResult = serializeGraphInfo(ml);
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("scheme")] = QStringLiteral("okabe_ito");
            outResult[QStringLiteral("group_size")] = groupSize;
            return true;
        }
        QString canonical;
        const Graph::SeriesStyleScheme scheme = parseSeriesStyleScheme(schemeStr, &canonical);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Apply Series Style Scheme"));
        g->applySeriesStyleScheme(scheme);
        if (params.contains(QStringLiteral("group_size")) && groupSize >= 1
            && (canonical == QStringLiteral("bw_hollow_filled")
                || canonical == QStringLiteral("blue_red_hollow"))) {
            int visIdx = 0;
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (g->curveType(ci) == Graph::ErrorBars)
                    continue;
                g->setCurveSymbolHollow(ci, ((visIdx / groupSize) % 2) == 1);
                ++visIdx;
            }
        }
        for (int ci = 0; ci < g->curves(); ++ci) {
            if (g->curveType(ci) != Graph::ErrorBars)
                applyInlineCurveStyleParams(g, ci, params, false);
        }
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("scheme")] = canonical;
        outResult[QStringLiteral("group_size")] = groupSize;
        return true;
    }

    if (method == QStringLiteral("graph.apply_palette")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const bool grad = optionalJsonBool(
                params, QStringLiteral("gradient"),
                optionalJsonBool(params, QStringLiteral("sequential"), false));
        const bool applyToSymbols =
                optionalJsonBool(params, QStringLiteral("apply_to_symbols"), true);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Apply Color Palette"));
        if (params.value(QStringLiteral("colors")).isArray()) {
            QList<QColor> cols;
            for (const QJsonValue &cv : params.value(QStringLiteral("colors")).toArray()) {
                const QColor parsed = parseColorValue(cv, QColor());
                if (!parsed.isValid()) {
                    throwInvalidParams(
                            QStringLiteral("Invalid color value '%1' in 'colors' array.")
                                    .arg(cv.toString()));
                }
                cols.append(parsed);
            }
            if (cols.isEmpty())
                throwInvalidParams(QStringLiteral("'colors' array must not be empty."));
            g->applyColorPalette(cols, grad, applyToSymbols);
        } else {
            QString rawPal = params.contains(QStringLiteral("palette"))
                    ? optionalJsonString(params, QStringLiteral("palette")).trimmed()
                    : optionalJsonString(params, QStringLiteral("name")).trimmed();
            if (rawPal.isEmpty())
                rawPal = QStringLiteral("okabe_ito");
            const QStringList allPals = Graph::allPaletteNames();
            QString wantNorm;
            const QString matchedPal =
                    PythonAutomationServer::matchPaletteName(rawPal, allPals, &wantNorm);
            QList<QColor> builtinFallbackCols;
            if (matchedPal.isEmpty()) {
                if (wantNorm == QStringLiteral("tolbright")
                    || wantNorm == QStringLiteral("paultolbright")) {
                    builtinFallbackCols = { QColor(0x44, 0x77, 0xAA), QColor(0xEE, 0x66, 0x77),
                                            QColor(0x22, 0x88, 0x33), QColor(0xCC, 0xBB, 0x44),
                                            QColor(0x66, 0xCC, 0xEE), QColor(0xAA, 0x33, 0x77),
                                            QColor(0xBB, 0xBB, 0xBB) };
                } else if (wantNorm == QStringLiteral("tolmuted")
                           || wantNorm == QStringLiteral("paultolmuted")) {
                    builtinFallbackCols = { QColor(0x33, 0x22, 0x88), QColor(0x88, 0xCC, 0xEE),
                                            QColor(0x44, 0xAA, 0x99), QColor(0x11, 0x77, 0x33),
                                            QColor(0x99, 0x99, 0x33), QColor(0xDD, 0xCC, 0x77),
                                            QColor(0xCC, 0x66, 0x77), QColor(0x88, 0x22, 0x55),
                                            QColor(0xAA, 0x44, 0x99) };
                } else if (wantNorm == QStringLiteral("isijmonochrome")
                           || wantNorm == QStringLiteral("monochrome")) {
                    builtinFallbackCols = { QColor(0x11, 0x11, 0x11), QColor(0x44, 0x44, 0x44),
                                            QColor(0x77, 0x77, 0x77), QColor(0x2B, 0x2B, 0x2B),
                                            QColor(0x5E, 0x5E, 0x5E), QColor(0x99, 0x99, 0x99) };
                } else if (wantNorm == QStringLiteral("cividis")) {
                    builtinFallbackCols = { QColor(0x00, 0x20, 0x4C), QColor(0x00, 0x33, 0x6F),
                                            QColor(0x31, 0x44, 0x6B), QColor(0x57, 0x5C, 0x6E),
                                            QColor(0x7C, 0x7B, 0x78), QColor(0xA6, 0x9D, 0x75),
                                            QColor(0xD3, 0xC1, 0x64), QColor(0xFF, 0xEA, 0x46) };
                }
            }
            if (!builtinFallbackCols.isEmpty()) {
                g->applyColorPalette(builtinFallbackCols, grad, applyToSymbols);
            } else if (matchedPal.isEmpty()) {
                QStringList availPals = allPals;
                for (const QString &extra :
                     { QStringLiteral("Paul Tol Bright"), QStringLiteral("Paul Tol Muted"),
                       QStringLiteral("ISIJ Monochrome"), QStringLiteral("Cividis") }) {
                    if (!availPals.contains(extra, Qt::CaseInsensitive))
                        availPals.append(extra);
                }
                throwInvalidParamsWithAvailable(
                        QStringLiteral("Unknown color palette '%1'.").arg(rawPal), availPals);
            } else if (!applyToSymbols) {
                const QList<QColor> palCols = Graph::paletteColors(matchedPal);
                if (!palCols.isEmpty())
                    g->applyColorPalette(palCols, grad, false);
                else
                    g->applyColorPaletteByName(matchedPal, grad);
            } else {
                g->applyColorPaletteByName(matchedPal, grad);
            }
        }
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("gradient")] = grad;
        return true;
    }

    if (method == QStringLiteral("graph.set_colorbar")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        Spectrogram *targetSpec = nullptr;
        if (g->plotWidget()) {
            for (QwtPlotItem *it : g->plotWidget()->itemList()) {
                if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                    targetSpec = dynamic_cast<Spectrogram *>(it);
                    if (targetSpec)
                        break;
                }
            }
        }
        if (!targetSpec)
            throwInvalidParams(
                    QStringLiteral("No matrix spectrogram found on graph '%1'.").arg(ml->name()));

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Modify Color Bar"));
        const QString palStr = !optionalJsonString(params, QStringLiteral("palette")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("palette")).trimmed()
                : (!optionalJsonString(params, QStringLiteral("colormap")).trimmed().isEmpty()
                           ? optionalJsonString(params, QStringLiteral("colormap")).trimmed()
                           : optionalJsonString(params, QStringLiteral("cmap")).trimmed());
        const bool hasPalette = !palStr.isEmpty();
        const bool hasReverse = params.contains(QStringLiteral("reverse"))
                && !params.value(QStringLiteral("reverse")).isNull();
        if (hasPalette) {
            bool isGray = false;
            const auto preset = parseColorMapPreset(palStr, &isGray, true);
            const bool rev = optionalJsonBool(params, QStringLiteral("reverse"), false);
            targetSpec->setColorMapPreset(isGray ? Spectrogram::PresetGrayScale : preset, rev);
        } else if (hasReverse) {
            targetSpec->setReversePalette(optionalJsonBool(params, QStringLiteral("reverse"), false));
        }
        if (params.contains(QStringLiteral("z_min")) && params.contains(QStringLiteral("z_max"))
            && !params.value(QStringLiteral("z_min")).isNull()
            && !params.value(QStringLiteral("z_max")).isNull()) {
            const double zLo = parseFiniteCoordDouble(params.value(QStringLiteral("z_min")),
                                                      QStringLiteral("z_min"));
            const double zHi = parseFiniteCoordDouble(params.value(QStringLiteral("z_max")),
                                                      QStringLiteral("z_max"));
            validateFiniteCoordRange(zLo, zHi, QStringLiteral("Colorbar [z_min, z_max]"));
            targetSpec->setCustomValueRange(true, std::min(zLo, zHi), std::max(zLo, zHi));
        }
        if (params.contains(QStringLiteral("visible"))) {
            targetSpec->showColorScale(QwtPlot::yRight,
                                       optionalJsonBool(params, QStringLiteral("visible"), true));
        } else if (!targetSpec->hasColorScale()) {
            targetSpec->showColorScale(QwtPlot::yRight, true);
        }
        if (params.contains(QStringLiteral("title")))
            targetSpec->setColorBarTitle(optionalJsonString(params, QStringLiteral("title")));
        if (params.contains(QStringLiteral("levels"))
            && !params.value(QStringLiteral("levels")).isNull()) {
            targetSpec->setLevelsNumber(parseBoundedInt(params, QStringLiteral("levels"), 8, 2, 256));
        }
        if (params.contains(QStringLiteral("show_contour_lines"))
            && !params.value(QStringLiteral("show_contour_lines")).isNull()) {
            targetSpec->setDisplayMode(
                    QwtPlotSpectrogram::ContourMode,
                    optionalJsonBool(params, QStringLiteral("show_contour_lines"), true));
        }
        if (params.contains(QStringLiteral("use_colormap_pen"))
            && !params.value(QStringLiteral("use_colormap_pen")).isNull()) {
            const bool useCmPen =
                    optionalJsonBool(params, QStringLiteral("use_colormap_pen"), true);
            targetSpec->setDefaultContourPen(useCmPen ? QPen(Qt::NoPen) : QPen(Qt::black, 1.2));
            if (useCmPen
                && (!params.contains(QStringLiteral("show_contour_lines"))
                    || params.value(QStringLiteral("show_contour_lines")).isNull())) {
                targetSpec->setDisplayMode(QwtPlotSpectrogram::ContourMode, true);
            }
        }
        if (params.contains(QStringLiteral("log")) && !params.value(QStringLiteral("log")).isNull()) {
            targetSpec->setLogColorScale(optionalJsonBool(params, QStringLiteral("log"), false));
        } else if (params.contains(QStringLiteral("log_scale"))
                   && !params.value(QStringLiteral("log_scale")).isNull()) {
            targetSpec->setLogColorScale(
                    optionalJsonBool(params, QStringLiteral("log_scale"), false));
        }
        if (params.contains(QStringLiteral("cell_blocks"))
            && !params.value(QStringLiteral("cell_blocks")).isNull()) {
            targetSpec->setCellBlocks(
                    optionalJsonBool(params, QStringLiteral("cell_blocks"), false));
        }
        targetSpec->enforceColorBarArialFont();
        targetSpec->syncColorScale();
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_legend")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Modify Legend"));

        const bool hasVisible = params.contains(QStringLiteral("visible"))
                && !params.value(QStringLiteral("visible")).isNull();
        const bool vis = hasVisible ? optionalJsonBool(params, QStringLiteral("visible"), true)
                                    : true;

        const QString posStr = optionalJsonString(params, QStringLiteral("position")).trimmed();
        const bool hasPosition = !posStr.isEmpty();
        Graph::LegendAnchor anchor = Graph::LegendAutoAvoidData;
        if (hasPosition)
            anchor = parseLegendAnchor(posStr);

        if (vis || hasPosition) {
            g->setLegendVisible(true);
            Legend *leg = g->legend();
            if (!leg)
                leg = g->newLegend();
            if (leg) {
                if (params.contains(QStringLiteral("text"))
                    && !params.value(QStringLiteral("text")).isNull()) {
                    leg->setText(optionalJsonString(params, QStringLiteral("text")));
                } else if (optionalJsonBool(params, QStringLiteral("reconstruct"), false)) {
                    g->reconstructLegend();
                    leg = g->legend();
                }
                if (leg) {
                    if (params.contains(QStringLiteral("columns"))
                        && !params.value(QStringLiteral("columns")).isNull()) {
                        leg->setNumColumns(
                                parseBoundedInt(params, QStringLiteral("columns"), 1, 1, 16));
                    }
                    const bool hasFrameKey = params.contains(QStringLiteral("frame"))
                            && !params.value(QStringLiteral("frame")).isNull();
                    const bool hasFramedKey = params.contains(QStringLiteral("framed"))
                            && !params.value(QStringLiteral("framed")).isNull();
                    if (hasFrameKey || hasFramedKey) {
                        const QString fk = hasFrameKey ? QStringLiteral("frame")
                                                       : QStringLiteral("framed");
                        const QJsonValue fv = params.value(fk);
                        int fStyle = 0;
                        if (fv.isBool()) {
                            fStyle = fv.toBool() ? int(Legend::Line) : 0;
                        } else if (fv.isDouble() && !fv.isBool()) {
                            fStyle = parseBoundedInt(params, fk, 0, 0, 2);
                        } else {
                            throwInvalidParams(
                                    QStringLiteral("Parameter '%1' must be a boolean or integer in [0, 2].")
                                            .arg(fk));
                        }
                        leg->setFrameStyle(fStyle);
                        if (fStyle == 0 && !params.contains(QStringLiteral("bg_color"))
                            && !params.contains(QStringLiteral("background_color"))
                            && !params.contains(QStringLiteral("background_alpha"))
                            && !params.contains(QStringLiteral("bg_alpha"))) {
                            leg->setBackgroundColor(QColor(255, 255, 255, 0));
                        }
                    }
                    if (params.contains(QStringLiteral("x"))
                        && params.contains(QStringLiteral("y"))
                        && !params.value(QStringLiteral("x")).isNull()
                        && !params.value(QStringLiteral("y")).isNull()) {
                        leg->setOriginCoord(
                                parseFiniteCoordDouble(params.value(QStringLiteral("x")),
                                                       QStringLiteral("x")),
                                parseFiniteCoordDouble(params.value(QStringLiteral("y")),
                                                       QStringLiteral("y")));
                    }
                    QFont lf = leg->font();
                    if (params.contains(QStringLiteral("font_family"))
                        && !params.value(QStringLiteral("font_family")).isNull()) {
                        const QString fam =
                                optionalJsonString(params, QStringLiteral("font_family")).trimmed();
                        if (!fam.isEmpty())
                            lf.setFamily(fam);
                    } else if (lf.family().trimmed().isEmpty()) {
                        lf.setFamily(QStringLiteral("Arial"));
                    }
                    if (params.contains(QStringLiteral("font_size"))
                        && !params.value(QStringLiteral("font_size")).isNull()) {
                        lf.setPointSizeF(
                                parseBoundedFontPt(params, QStringLiteral("font_size"), 10, 4, 96));
                    }
                    leg->setFont(lf);
                }
            }
        }

        QString chosenAnchor;
        double overlapScore = 0.0;
        if (hasPosition) {
            const int marginPx = parseBoundedInt(params, QStringLiteral("margin_px"), 12, 0, 256);
            chosenAnchor = g->placeLegendAnchor(anchor, &overlapScore, marginPx);
        }

        if (hasVisible && !vis)
            g->setLegendVisible(false);

        g->replot();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("visible")] = g->isLegendVisible();
        if (hasPosition) {
            outResult[QStringLiteral("anchor")] = chosenAnchor;
            outResult[QStringLiteral("overlap_score")] = safeJsonDouble(overlapScore);
        }
        return true;
    }

    if (method == QStringLiteral("graph.auto_place_legend")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const Graph::LegendAnchor anchor =
                (params.contains(QStringLiteral("position"))
                 && !params.value(QStringLiteral("position")).isNull()
                 && !params.value(QStringLiteral("position")).toString().trimmed().isEmpty())
                ? parseLegendAnchor(params.value(QStringLiteral("position")).toString())
                : Graph::LegendAutoAvoidData;
        const int marginPx =
                (params.contains(QStringLiteral("margin_px"))
                 && !params.value(QStringLiteral("margin_px")).isNull())
                ? std::max(0,
                           parseStrictJsonInt(params.value(QStringLiteral("margin_px")),
                                              QStringLiteral("margin_px")))
                : 12;

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Auto-Place Legend"));
        g->setLegendVisible(true);
        if (!g->legend())
            g->newLegend();
        if (Legend *leg = g->legend()) {
            QFont lf = leg->font();
            if (lf.family().trimmed().isEmpty()) {
                lf.setFamily(QStringLiteral("Arial"));
                leg->setFont(lf);
            }
        }
        double score = 0.0;
        const QString chosenAnchor = g->placeLegendAnchor(anchor, &score, marginPx);
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("anchor")] = chosenAnchor;
        outResult[QStringLiteral("overlap_score")] = safeJsonDouble(score);
        return true;
    }

    if (method == QStringLiteral("graph.set_panel_labels")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const bool enabled = params.contains(QStringLiteral("enabled"))
                ? params.value(QStringLiteral("enabled")).toBool(true)
                : true;
        QString placementRaw =
                (params.contains(QStringLiteral("placement"))
                 && !params.value(QStringLiteral("placement")).isNull())
                ? params.value(QStringLiteral("placement")).toString().trimmed().toLower()
                : ((params.contains(QStringLiteral("position"))
                    && !params.value(QStringLiteral("position")).isNull())
                           ? params.value(QStringLiteral("position")).toString().trimmed().toLower()
                           : QStringLiteral("outside_top_left"));
        placementRaw.replace(QLatin1Char('-'), QLatin1Char('_'));
        placementRaw.replace(QLatin1Char(' '), QLatin1Char('_'));
        MultiLayer::PanelLabelPlacement placement = MultiLayer::PanelOutsideTopLeft;
        if (placementRaw == QStringLiteral("outside_top_left")
            || placementRaw == QStringLiteral("outside")) {
            placement = MultiLayer::PanelOutsideTopLeft;
        } else if (placementRaw == QStringLiteral("inside_top_left")
                   || placementRaw == QStringLiteral("inside")) {
            placement = MultiLayer::PanelInsideTopLeft;
        } else {
            throwInvalidParams(
                    QStringLiteral(
                            "Unknown panel label placement '%1' (expected 'outside_top_left'|'inside_top_left').")
                            .arg(params.value(QStringLiteral("placement")).toString()));
        }
        const QString pattern =
                (params.contains(QStringLiteral("pattern"))
                 && !params.value(QStringLiteral("pattern")).isNull())
                ? params.value(QStringLiteral("pattern")).toString()
                : QStringLiteral("(%a)");
        const int fontPt =
                (params.contains(QStringLiteral("font_pt"))
                 && !params.value(QStringLiteral("font_pt")).isNull())
                ? std::max(0,
                           parseStrictJsonInt(params.value(QStringLiteral("font_pt")),
                                              QStringLiteral("font_pt")))
                : 0;

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Panel Labels"));
        ml->setPanelLabels(enabled, placement, pattern, fontPt);
        for (Graph *lg : ml->layersList())
            enforceArialGraphFonts(lg);
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("enabled")] = ml->hasPanelLabels();
        outResult[QStringLiteral("placement")] =
                (ml->panelLabelPlacement() == MultiLayer::PanelInsideTopLeft)
                ? QStringLiteral("inside_top_left")
                : QStringLiteral("outside_top_left");
        outResult[QStringLiteral("pattern")] = ml->panelLabelPattern();
        outResult[QStringLiteral("font_pt")] = ml->panelLabelFontPt();
        return true;
    }

    if (method == QStringLiteral("graph.apply_square_frame")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Square Frame (70%)"));
        ml->applySquareCanvasFrame70Percent();
        Graph *g = resolveGraphLayer(ml, params, false);
        int w = 0;
        int h = 0;
        if (g && g->plotWidget()) {
            Plot *plt = g->plotWidget();
            plt->updateLayout();
            const QRect cr = plt->plotLayout() ? plt->plotLayout()->canvasRect() : QRect();
            w = cr.width();
            h = cr.height();
            if ((w <= 0 || h <= 0) && plt->canvas()) {
                w = plt->canvas()->width();
                h = plt->canvas()->height();
            }
        }
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("width")] = w;
        outResult[QStringLiteral("height")] = h;
        return true;
    }

    if (method == QStringLiteral("graph.drop_columns")) {
        Table *t = resolveTable(d_app, params, QStringLiteral("table"), true);
        QJsonArray colsParam;
        const QJsonValue rawCols = params.contains(QStringLiteral("columns"))
                ? params.value(QStringLiteral("columns"))
                : params.value(QStringLiteral("cols"));
        if (rawCols.isArray()) {
            colsParam = rawCols.toArray();
        } else if (!rawCols.isUndefined() && !rawCols.isNull()) {
            colsParam.append(rawCols);
        }
        if (colsParam.isEmpty())
            throwInvalidParams(
                    QStringLiteral("cols / columns must be a non-empty list of column names."));

        QJsonArray shortCols;
        QJsonArray fullCols;
        QJsonArray colIndices;
        for (const QJsonValue &cv : colsParam) {
            const int cIdx = resolveColumnIndex(t, cv, true);
            shortCols.append(t->colLabel(cIdx));
            fullCols.append(t->colName(cIdx));
            colIndices.append(cIdx);
        }

        QJsonObject payload;
        payload[QStringLiteral("table")] = t->name();
        payload[QStringLiteral("columns")] = shortCols;
        payload[QStringLiteral("full_columns")] = fullCols;
        payload[QStringLiteral("column_names")] = fullCols;
        payload[QStringLiteral("column_indices")] = colIndices;

        const QJsonValue xVal = params.contains(QStringLiteral("x_column"))
                ? params.value(QStringLiteral("x_column"))
                : params.value(QStringLiteral("x_col"));
        if (!xVal.isUndefined() && !xVal.isNull()
            && (xVal.isDouble() || !xVal.toString().trimmed().isEmpty())) {
            const int xIdx = resolveColumnIndex(t, xVal, true);
            payload[QStringLiteral("x_column")] = t->colLabel(xIdx);
            payload[QStringLiteral("x_col")] = t->colLabel(xIdx);
            payload[QStringLiteral("x_full_column")] = t->colName(xIdx);
            payload[QStringLiteral("x_index")] = xIdx;
        } else {
            payload[QStringLiteral("x_column")] = QString();
        }

        const bool rightY = optionalJsonBool(params, QStringLiteral("right_y"), false);
        const bool replace = optionalJsonBool(params, QStringLiteral("replace"), false);
        payload[QStringLiteral("right_y")] = rightY;
        payload[QStringLiteral("replace")] = replace;

        MultiLayer *targetMl = nullptr;
        Graph *targetGraph = nullptr;
        const QString graphName = optionalJsonString(params, QStringLiteral("graph")).trimmed();
        if (!graphName.isEmpty()) {
            targetMl = resolveMultiLayer(d_app, params, QStringLiteral("graph"), true);
            targetGraph = resolveGraphLayer(targetMl, params, true);
        }

        QSet<MyWidget *> beforeWindows;
        for (MyWidget *w : d_app->windowsList()) {
            if (w)
                beforeWindows.insert(w);
        }

        QMimeData mimeData;
        mimeData.setData(QString::fromUtf8(MultiLayer::kColumnMimeType),
                         QJsonDocument(payload).toJson(QJsonDocument::Compact));

        const int targetAxisId = rightY ? QwtPlot::yRight : -1;
        const bool ok = d_app->handleColumnMimeDrop(&mimeData, targetMl, targetGraph,
                                                    targetAxisId, replace);

        MultiLayer *resMl = targetMl;
        if (!resMl) {
            for (MyWidget *w : d_app->windowsList()) {
                if (w && !beforeWindows.contains(w) && w->inherits("MultiLayer"))
                    resMl = static_cast<MultiLayer *>(w);
            }
            if (!resMl)
                resMl = resolveMultiLayer(d_app, QJsonObject(), QStringLiteral("graph"), false);
        }
        if (resMl) {
            for (Graph *lg : resMl->layersList())
                enforceArialGraphFonts(lg);
            outResult = serializeGraphInfo(resMl);
            outResult[QStringLiteral("graph")] = resMl->name();
        }
        outResult[QStringLiteral("ok")] = ok;
        return true;
    }

    if (method == QStringLiteral("graph.add_text")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString txt = optionalJsonString(params, QStringLiteral("text"));
        if (txt.trimmed().isEmpty())
            throwInvalidParams(QStringLiteral("Parameter 'text' must be a non-empty string."));
        const bool hasCoords = params.contains(QStringLiteral("x"))
                && params.contains(QStringLiteral("y"))
                && !params.value(QStringLiteral("x")).isNull()
                && !params.value(QStringLiteral("y")).isNull();
        const double xCoord = hasCoords
                ? parseFiniteCoordDouble(params.value(QStringLiteral("x")), QStringLiteral("x"))
                : 0.0;
        const double yCoord = hasCoords
                ? parseFiniteCoordDouble(params.value(QStringLiteral("y")), QStringLiteral("y"))
                : 0.0;
        const double fontPt = parseBoundedFontPt(params, QStringLiteral("font_size"), 11, 4, 144);
        int frameSt = int(Legend::None);
        if (params.contains(QStringLiteral("frame"))
            && !params.value(QStringLiteral("frame")).isNull()) {
            const QJsonValue fv = params.value(QStringLiteral("frame"));
            if (fv.isBool())
                frameSt = fv.toBool() ? int(Legend::Line) : int(Legend::None);
            else
                frameSt = parseBoundedInt(params, QStringLiteral("frame"), int(Legend::None), 0, 4);
        }
        const int angleDeg = params.contains(QStringLiteral("angle"))
                ? qRound(optionalJsonFiniteDouble(params, QStringLiteral("angle"), 0.0, 360.0))
                : qRound(optionalJsonFiniteDouble(params, QStringLiteral("rotation"), 0.0, 360.0));

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Text"));
        auto *mrk = new Legend(g->plotWidget());
        mrk->setText(txt);
        QFont f(optionalJsonString(params, QStringLiteral("font_family"), QStringLiteral("Arial")),
                qRound(fontPt));
        f.setPointSizeF(fontPt);
        f.setBold(optionalJsonBool(params, QStringLiteral("bold"), false));
        mrk->setFont(f);
        mrk->setTextColor(
                parseColorValue(params.value(QStringLiteral("color")), QColor(Qt::black)));
        if (params.contains(QStringLiteral("background_color"))
            || params.contains(QStringLiteral("bg_color"))) {
            const QJsonValue bgV = params.contains(QStringLiteral("background_color"))
                    ? params.value(QStringLiteral("background_color"))
                    : params.value(QStringLiteral("bg_color"));
            mrk->setBackgroundColor(parseColorValue(bgV, QColor(Qt::white)));
        }
        mrk->setFrameStyle(frameSt);
        mrk->setAngle(angleDeg);
        if (hasCoords)
            mrk->setOriginCoord(xCoord, yCoord);
        const long id = g->insertTextMarker(mrk);
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("id")] = static_cast<qint64>(id);
        outResult[QStringLiteral("marker_id")] = static_cast<qint64>(id);
        return true;
    }

    if (method == QStringLiteral("graph.add_arrow")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        if (!params.contains(QStringLiteral("x1")) || !params.contains(QStringLiteral("y1"))
            || !params.contains(QStringLiteral("x2")) || !params.contains(QStringLiteral("y2"))) {
            throwInvalidParams(
                    QStringLiteral("graph.add_arrow requires finite numeric 'x1', 'y1', 'x2', and 'y2'."));
        }
        const double x1 = parseFiniteCoordDouble(params.value(QStringLiteral("x1")),
                                                 QStringLiteral("x1"));
        const double y1 = parseFiniteCoordDouble(params.value(QStringLiteral("y1")),
                                                 QStringLiteral("y1"));
        const double x2 = parseFiniteCoordDouble(params.value(QStringLiteral("x2")),
                                                 QStringLiteral("x2"));
        const double y2 = parseFiniteCoordDouble(params.value(QStringLiteral("y2")),
                                                 QStringLiteral("y2"));
        const double rawLineW = params.contains(QStringLiteral("width"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("width"), 1.0, 32.0)
                : optionalJsonFiniteDouble(params, QStringLiteral("line_width"), 1.0, 32.0);
        if (rawLineW <= 0.0 || rawLineW > 32.0)
            throwInvalidParams(QStringLiteral("graph.add_arrow width must be in (0, 32]."));
        const int lineW = std::clamp(qRound(rawLineW), 1, 32);
        const QJsonValue stV = params.contains(QStringLiteral("style"))
                ? params.value(QStringLiteral("style"))
                : params.value(QStringLiteral("line_style"));
        const int headLen = parseBoundedInt(params, QStringLiteral("head_length"), 9, 1, 64);
        const int headAng = parseBoundedInt(params, QStringLiteral("head_angle"), 28, 5, 89);

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Arrow"));
        ArrowMarker arr;
        arr.setStartPoint(x1, y1);
        arr.setEndPoint(x2, y2);
        arr.setColor(parseColorValue(params.value(QStringLiteral("color")), QColor(Qt::black)));
        arr.setWidth(lineW);
        arr.setStyle(parsePenStyleValue(stV, Qt::SolidLine));
        arr.setHeadLength(headLen);
        arr.setHeadAngle(headAng);
        arr.drawStartArrow(optionalJsonBool(params, QStringLiteral("start_arrow"), false));
        arr.drawEndArrow(optionalJsonBool(params, QStringLiteral("end_arrow"), true));
        arr.fillArrowHead(optionalJsonBool(
                params, QStringLiteral("filled"),
                optionalJsonBool(params, QStringLiteral("fill_head"), true)));
        g->addArrow(&arr);
        const QVector<int> lKeys = g->lineMarkerKeys();
        const int newId = lKeys.isEmpty() ? -1 : lKeys.last();
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("id")] = newId;
        outResult[QStringLiteral("marker_id")] = newId;
        return true;
    }

    if (method == QStringLiteral("graph.remove_marker")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString kindParam = optionalJsonString(params, QStringLiteral("kind")).trimmed().toLower();
        if (!kindParam.isEmpty() && kindParam != QStringLiteral("text")
            && kindParam != QStringLiteral("arrow") && kindParam != QStringLiteral("line")
            && kindParam != QStringLiteral("all")) {
            throwInvalidParams(
                    QStringLiteral("Unknown marker kind '%1' (expected 'text'|'arrow'|'line'|'all').")
                            .arg(kindParam));
        }
        Legend *leg = g->legend();

        QVector<int> nonLegendTexts;
        for (int tid : g->textMarkerKeys()) {
            Legend *tm = g->textMarker(tid);
            if (tm && tm != leg)
                nonLegendTexts.append(tid);
        }
        const QVector<int> arrowIds = g->lineMarkerKeys();

        const bool clearAll = optionalJsonBool(params, QStringLiteral("all"), false)
                || optionalJsonBool(params, QStringLiteral("clear_all"), false);
        if (clearAll) {
            QVector<int> allToRemove = nonLegendTexts;
            for (int aid : arrowIds)
                allToRemove.append(aid);
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Clear Markers"));
            for (int mid : allToRemove) {
                g->setSelectedMarker(mid, false);
                g->removeMarker();
            }
            g->replot();
            undoGuard.commit();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("removed")] = allToRemove.size();
            return true;
        }

        int targetId = -1;
        const QJsonValue idVal = params.contains(QStringLiteral("marker_id"))
                ? params.value(QStringLiteral("marker_id"))
                : params.value(QStringLiteral("id"));
        if (!idVal.isUndefined() && !idVal.isNull()) {
            const int wantId = parseStrictJsonInt(idVal, QStringLiteral("marker_id"));
            if (nonLegendTexts.contains(wantId) || arrowIds.contains(wantId))
                targetId = wantId;
        } else if (params.contains(QStringLiteral("index"))
                   && !params.value(QStringLiteral("index")).isNull()) {
            const int idx = parseStrictJsonInt(params.value(QStringLiteral("index")),
                                               QStringLiteral("index"));
            const QString kind =
                    params.value(QStringLiteral("kind")).toString().trimmed().toLower();
            if (kind == QStringLiteral("arrow") || kind == QStringLiteral("line")) {
                if (idx >= 0 && idx < arrowIds.size())
                    targetId = arrowIds[idx];
            } else if (kind == QStringLiteral("text")) {
                if (idx >= 0 && idx < nonLegendTexts.size())
                    targetId = nonLegendTexts[idx];
            } else {
                if (idx >= 0 && idx < nonLegendTexts.size())
                    targetId = nonLegendTexts[idx];
                else if (idx >= nonLegendTexts.size()
                         && (idx - nonLegendTexts.size()) < arrowIds.size())
                    targetId = arrowIds[idx - nonLegendTexts.size()];
            }
        }

        if (targetId < 0) {
            QStringList avail;
            for (int tid : nonLegendTexts)
                avail.append(QStringLiteral("text:%1").arg(tid));
            for (int aid : arrowIds)
                avail.append(QStringLiteral("arrow:%1").arg(aid));
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Marker not found on graph '%1'.").arg(ml->name()), avail);
        }

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Remove Marker"));
        g->setSelectedMarker(targetId, false);
        g->removeMarker();
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("removed_id")] = targetId;
        return true;
    }

    if (method == QStringLiteral("graph.mask_region")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        if (!params.contains(QStringLiteral("x_min")) || params.value(QStringLiteral("x_min")).isNull()
            || !params.contains(QStringLiteral("x_max"))
            || params.value(QStringLiteral("x_max")).isNull()) {
            throwInvalidParams(
                    QStringLiteral("graph.mask_region requires finite numeric 'x_min' and 'x_max'."));
        }
        const double xMin = parseFiniteCoordDouble(params.value(QStringLiteral("x_min")),
                                                   QStringLiteral("x_min"));
        const double xMax = parseFiniteCoordDouble(params.value(QStringLiteral("x_max")),
                                                   QStringLiteral("x_max"));
        const double yMin = (params.contains(QStringLiteral("y_min"))
                             && !params.value(QStringLiteral("y_min")).isNull())
                ? parseFiniteCoordDouble(params.value(QStringLiteral("y_min")),
                                         QStringLiteral("y_min"))
                : -1e300;
        const double yMax = (params.contains(QStringLiteral("y_max"))
                             && !params.value(QStringLiteral("y_max")).isNull())
                ? parseFiniteCoordDouble(params.value(QStringLiteral("y_max")),
                                         QStringLiteral("y_max"))
                : 1e300;
        const QRectF rect(QPointF(std::min(xMin, xMax), std::min(yMin, yMax)),
                          QPointF(std::max(xMin, xMax), std::max(yMin, yMax)));
        const bool doMask = params.contains(QStringLiteral("mask"))
                ? optionalJsonBool(params, QStringLiteral("mask"), true)
                : !optionalJsonBool(params, QStringLiteral("unmask"), false);

        QList<Graph *> targetLayers;
        targetLayers << g;
        if (!params.contains(QStringLiteral("layer"))) {
            bool hasDataCurve = false;
            for (int i = 0; i < g->curves(); ++i) {
                if (dynamic_cast<DataCurve *>(g->curve(i))) {
                    hasDataCurve = true;
                    break;
                }
            }
            if (!hasDataCurve)
                targetLayers = ml->layersList();
        }

        // An explicit 'curve' that cannot be resolved on a layer must not fall back to masking every
        // DataCurve of that layer (the mask is written into the source table columns). Resolve on
        // all target layers first, skip layers where it is missing, and fail without touching any
        // data when no layer has it (same contract as graph.clear_masks).
        const bool hasExplicitCurve = params.contains(QStringLiteral("curve"))
                && !params.value(QStringLiteral("curve")).isNull();
        QVector<int> onlyIdxPerLayer(targetLayers.size(), -1);
        bool foundCurve = !hasExplicitCurve;
        if (hasExplicitCurve) {
            for (int li = 0; li < targetLayers.size(); ++li) {
                Graph *lg = targetLayers.at(li);
                if (!lg)
                    continue;
                onlyIdxPerLayer[li] =
                        resolveCurveIndex(lg, params, QStringLiteral("curve"), false);
                if (onlyIdxPerLayer[li] >= 0)
                    foundCurve = true;
            }
        }
        if (!foundCurve)
            throwInvalidParams(
                    QStringLiteral("Curve not found on graph '%1'.").arg(ml->name()));

        int totalMasked = 0;
        for (int li = 0; li < targetLayers.size(); ++li) {
            Graph *lg = targetLayers.at(li);
            if (!lg)
                continue;
            const int onlyIdx = onlyIdxPerLayer.at(li);
            if (hasExplicitCurve && onlyIdx < 0)
                continue;
            for (int i = 0; i < lg->curves(); ++i) {
                if (hasExplicitCurve && i != onlyIdx)
                    continue;
                if (auto *dc = dynamic_cast<DataCurve *>(lg->curve(i)))
                    totalMasked += dc->maskRegion(rect, doMask);
            }
            lg->replot();
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("masked_points")] = totalMasked;
        return true;
    }

    if (method == QStringLiteral("graph.clear_masks")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        QList<Graph *> targetLayers;
        if (params.contains(QStringLiteral("layer"))
            && !params.value(QStringLiteral("layer")).isNull())
            targetLayers << resolveGraphLayer(ml, params);
        else
            targetLayers = ml->layersList();
        const bool hasExplicitCurve = params.contains(QStringLiteral("curve"))
                && !params.value(QStringLiteral("curve")).isNull();
        bool foundCurve = !hasExplicitCurve;
        for (Graph *lg : targetLayers) {
            if (!lg)
                continue;
            int onlyIdx = -1;
            if (hasExplicitCurve) {
                onlyIdx = resolveCurveIndex(lg, params, QStringLiteral("curve"),
                                            targetLayers.size() == 1);
                if (onlyIdx >= 0)
                    foundCurve = true;
            }
            for (int i = 0; i < lg->curves(); ++i) {
                if (hasExplicitCurve && (onlyIdx < 0 || i != onlyIdx))
                    continue;
                if (auto *dc = dynamic_cast<DataCurve *>(lg->curve(i)))
                    dc->clearMasks();
            }
            replotGraphOrDefer(lg);
        }
        if (!foundCurve)
            throwInvalidParams(
                    QStringLiteral("Curve not found on graph '%1'.").arg(ml->name()));
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.export")) {
        const QString path = params.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty())
            throwInvalidParams(QStringLiteral("Export 'path' is required."));
        QFileInfo fi(path);
        if (!fi.dir().exists())
            QDir().mkpath(fi.dir().absolutePath());
        const QString absPath = fi.absoluteFilePath();
        const QString ext = fi.suffix().toLower();

        // Check if target is Graph3D (either explicitly named or active window)
        Graph3D *targetG3D = nullptr;
        const QString gName = params.value(QStringLiteral("graph")).toString().trimmed();
        if (!gName.isEmpty()) {
            if (MyWidget *mw = findWindowByName(d_app, gName)) {
                if (mw->inherits("Graph3D"))
                    targetG3D = static_cast<Graph3D *>(mw);
            }
        } else if (QWidget *actW = d_app->d_workspace.activeSubWindow()) {
            if (actW->inherits("Graph3D"))
                targetG3D = static_cast<Graph3D *>(actW);
        }
        if (targetG3D) {
            // Never delete the existing destination up-front: render into a sibling temp file
            // and atomically replace the target only after a non-empty result was produced.
            const bool is3dEmf = (ext == QStringLiteral("emf"));
            QString tmp3d;
            if (!is3dEmf) {
                if (!ApplicationWindow::probeExportWritableNoTruncate(absPath)) {
                    throwRpcRuntime(QStringLiteral(
                                            "Failed to export 3D graph to '%1': destination is not writable.")
                                            .arg(absPath));
                }
                const QDir dst3d = fi.dir();
                for (int attempt = 0; attempt < 16 && tmp3d.isEmpty(); ++attempt) {
                    const QString hex8 = QStringLiteral("%1").arg(
                            QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
                    const QString cand =
                            dst3d.filePath(QStringLiteral(".tgrpc_%1_%2").arg(hex8, fi.fileName()));
                    if (!QFileInfo::exists(cand))
                        tmp3d = cand;
                }
                if (tmp3d.isEmpty())
                    throwRpcRuntime(QStringLiteral("Failed to allocate a temporary export file."));
            }
            auto cleanup3dTemp = [&]() {
                if (!tmp3d.isEmpty() && QFileInfo::exists(tmp3d))
                    QFile::remove(tmp3d);
            };
            targetG3D->update();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (targetG3D->sp) {
                targetG3D->sp->makeCurrent();
                (void)targetG3D->sp->grabFramebuffer();
            }
            if (ext == QStringLiteral("pdf")) {
                targetG3D->exportPDF(tmp3d);
            } else if (ext == QStringLiteral("svg") || ext == QStringLiteral("eps")
                       || ext == QStringLiteral("ps")) {
                targetG3D->exportVector(tmp3d, ext);
            } else if (is3dEmf) {
#ifdef Q_OS_WIN
                QImage fb;
                if (targetG3D->sp) {
                    targetG3D->sp->makeCurrent();
                    fb = targetG3D->sp->grabFramebuffer();
                }
                if (fb.isNull()) {
                    QTemporaryFile tmpPng(
                            QDir::temp().filePath(QStringLiteral("torigraph_3d_XXXXXX.png")));
                    if (tmpPng.open()) {
                        const QString tmpPath = tmpPng.fileName();
                        tmpPng.close();
                        targetG3D->exportImage(tmpPath, 100, false);
                        fb.load(tmpPath);
                        QFile::remove(tmpPath);
                    }
                }
                if (fb.isNull())
                    throwRpcRuntime(QStringLiteral("Failed to capture 3D framebuffer for EMF export."));
                const auto emfRendered = SciDAVisEmf::renderFromImage(fb);
                if (emfRendered.emfBytes.isEmpty())
                    throwRpcRuntime(QStringLiteral("Failed to encode EMF from 3D graph image."));
                QSaveFile emfOut(absPath);
                if (!emfOut.open(QIODevice::WriteOnly)
                    || emfOut.write(emfRendered.emfBytes) != emfRendered.emfBytes.size()
                    || !emfOut.commit()) {
                    throwRpcRuntime(
                            QStringLiteral("Failed to write 3D EMF file '%1'.").arg(absPath));
                }
#else
                throwInvalidParams(QStringLiteral("EMF export is only supported on Windows."));
#endif
            } else {
                // 2D と同じく dpi を解像度として渡す（指定が無ければ 0 = 3D グラフ既定の
                // export_dpi / 300 dpi）。以前は dpi を渡しておらず、常に既定の寸法になっていた。
                // Graph3D::renderExportImage は 72..600 dpi・長辺 3200 px に収める。
                const int dpi3d = (params.contains(QStringLiteral("dpi"))
                                   && !params.value(QStringLiteral("dpi")).isNull())
                        ? parseBoundedInt(params, QStringLiteral("dpi"), 300, 36, 2400)
                        : 0;
                targetG3D->exportImage(tmp3d,
                                       params.value(QStringLiteral("quality")).toInt(100),
                                       params.value(QStringLiteral("transparent")).toBool(false),
                                       dpi3d);
            }
            if (!is3dEmf) {
                if (!QFileInfo::exists(tmp3d) || QFileInfo(tmp3d).size() <= 0) {
                    cleanup3dTemp();
                    throwRpcRuntime(
                            QStringLiteral("Failed to export 3D graph to '%1'.").arg(absPath));
                }
                if (!ApplicationWindow::commitExportTempFile(tmp3d, absPath)) {
                    cleanup3dTemp();
                    throwRpcRuntime(
                            QStringLiteral("Failed to export 3D graph to '%1'.").arg(absPath));
                }
            }
            if (!QFileInfo::exists(absPath) || QFileInfo(absPath).size() <= 0) {
                throwRpcRuntime(
                        QStringLiteral("Failed to export 3D graph to '%1'.").arg(absPath));
            }
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("path")] = absPath;
            outResult[QStringLiteral("bytes")] = QFileInfo(absPath).size();
            if (ext == QStringLiteral("png") || ext == QStringLiteral("jpg")
                || ext == QStringLiteral("jpeg") || ext == QStringLiteral("bmp")
                || ext == QStringLiteral("tif") || ext == QStringLiteral("tiff")) {
                QImage img(absPath);
                if (!img.isNull()) {
                    outResult[QStringLiteral("width_px")] = img.width();
                    outResult[QStringLiteral("height_px")] = img.height();
                }
            }
            return true;
        }

        MultiLayer *ml = resolveMultiLayer(d_app, params);
        for (Graph *lg : ml->layersList())
            lg->replot();

        const int defaultDpi = std::clamp(ml->pageDpi() > 0 ? ml->pageDpi() : 300, 36, 2400);
        const int dpi = parseBoundedInt(params, QStringLiteral("dpi"), defaultDpi, 36, 2400);
        const bool transparent = optionalJsonBool(params, QStringLiteral("transparent"), false);
        const int quality = parseBoundedInt(params, QStringLiteral("quality"), 100, 1, 100);
        const QString fmtParam = optionalJsonString(params, QStringLiteral("format")).trimmed().toLower();
        const QString effExt = !fmtParam.isEmpty() ? fmtParam : ext;

        const bool isVectorPsRequest =
                (effExt == QStringLiteral("eps") || effExt == QStringLiteral("ps"));
        const bool hasPsFileSuffix =
                absPath.endsWith(QStringLiteral(".eps"), Qt::CaseInsensitive)
                || absPath.endsWith(QStringLiteral(".ps"), Qt::CaseInsensitive);
#if QT_VERSION >= 0x050000
        const bool expectsQt5PdfFallback =
                (isVectorPsRequest || effExt == QStringLiteral("pdf")) && hasPsFileSuffix;
#else
        const bool expectsQt5PdfFallback = false;
#endif

        const QString expectedTargetPath =
                expectsQt5PdfFallback ? (absPath + QStringLiteral(".pdf")) : absPath;
        if (!ApplicationWindow::probeExportWritableNoTruncate(expectedTargetPath)) {
            throwRpcRuntime(
                    QStringLiteral("Failed to export graph to '%1': destination is not writable.")
                            .arg(expectedTargetPath));
        }

        const QDir dstDir = fi.dir();
        QString baseLeaf = fi.fileName();
        if (!fmtParam.isEmpty() && !isVectorPsRequest && effExt != QStringLiteral("pdf")
            && effExt != QStringLiteral("svg") && effExt != QStringLiteral("emf")
            && !baseLeaf.endsWith(QLatin1Char('.') + effExt, Qt::CaseInsensitive)) {
            baseLeaf += QLatin1Char('.') + effExt;
        }
        QString tmpPath;
        for (int attempt = 0; attempt < 16; ++attempt) {
            const QString hex8 = QStringLiteral("%1").arg(
                    QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
            QString candLeaf = QStringLiteral(".tgrpc_%1_%2").arg(hex8, baseLeaf);
            if (dstDir.absolutePath().size() + 1 + candLeaf.size() > 240) {
                const QString keepSfx = hasPsFileSuffix ? ext : (!effExt.isEmpty() ? effExt : ext);
                candLeaf = keepSfx.isEmpty() ? QStringLiteral(".tgrpc_%1.tmp").arg(hex8)
                                             : QStringLiteral(".tgrpc_%1.%2").arg(hex8, keepSfx);
            }
            const QString cand = dstDir.filePath(candLeaf);
            if (!QFileInfo::exists(cand) && !QFileInfo::exists(cand + QStringLiteral(".pdf"))) {
                tmpPath = cand;
                break;
            }
        }
        if (tmpPath.isEmpty()) {
            const QString hex8 = QStringLiteral("%1").arg(
                    QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
            tmpPath = dstDir.filePath(QStringLiteral(".tgrpc_%1_%2").arg(hex8, baseLeaf));
        }
        const QString tmpPdfPath = tmpPath + QStringLiteral(".pdf");
        auto cleanupRpcExportTemps = [&]() {
            if (QFileInfo::exists(tmpPath))
                QFile::remove(tmpPath);
            if (QFileInfo::exists(tmpPdfPath))
                QFile::remove(tmpPdfPath);
        };

        if (effExt == QStringLiteral("svg")) {
            ml->exportSVG(tmpPath);
        } else if (effExt == QStringLiteral("pdf") || isVectorPsRequest) {
            ml->exportVector(tmpPath, dpi, true);
        } else if (effExt == QStringLiteral("emf")) {
            ml->exportToFile(tmpPath);
        } else {
            ml->exportImage(tmpPath, quality, transparent, dpi);
        }

        QString producedTmpPath;
        QString finalOutPath = absPath;
        QString fallbackWarning;
#if QT_VERSION >= 0x050000
        if ((expectsQt5PdfFallback
             || (isVectorPsRequest
                 && (!QFileInfo::exists(tmpPath) || QFileInfo(tmpPath).size() <= 0)))
            && QFileInfo::exists(tmpPdfPath) && QFileInfo(tmpPdfPath).size() > 0) {
            producedTmpPath = tmpPdfPath;
            finalOutPath = absPath + QStringLiteral(".pdf");
            fallbackWarning = QStringLiteral(
                    "PostScript/EPS output is not available in Qt5; exported as PDF to '%1'.")
                                      .arg(finalOutPath);
        } else if (QFileInfo::exists(tmpPath) && QFileInfo(tmpPath).size() > 0) {
            producedTmpPath = tmpPath;
            if (isVectorPsRequest) {
                fallbackWarning = QStringLiteral(
                        "PostScript/EPS output is not available in Qt5; exported in PDF format.");
            }
        }
#else
        if (QFileInfo::exists(tmpPath) && QFileInfo(tmpPath).size() > 0)
            producedTmpPath = tmpPath;
#endif

        if (producedTmpPath.isEmpty()) {
            cleanupRpcExportTemps();
            throwRpcRuntime(QStringLiteral("Failed to export graph to '%1'.").arg(absPath));
        }
        if (!ApplicationWindow::commitExportTempFile(producedTmpPath, finalOutPath)) {
            cleanupRpcExportTemps();
            throwRpcRuntime(QStringLiteral("Failed to export graph to '%1'.").arg(finalOutPath));
        }
        cleanupRpcExportTemps();

        if (!QFileInfo::exists(finalOutPath) || QFileInfo(finalOutPath).size() <= 0)
            throwRpcRuntime(QStringLiteral("Failed to export graph to '%1'.").arg(absPath));

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("path")] = finalOutPath;
        if (finalOutPath != absPath) {
            outResult[QStringLiteral("requested_path")] = absPath;
            outResult[QStringLiteral("actual_path")] = finalOutPath;
            outResult[QStringLiteral("format")] = QStringLiteral("pdf");
        }
        if (!fallbackWarning.isEmpty())
            outResult[QStringLiteral("warning")] = fallbackWarning;
        outResult[QStringLiteral("bytes")] = QFileInfo(finalOutPath).size();
        if (effExt == QStringLiteral("png") || effExt == QStringLiteral("jpg")
            || effExt == QStringLiteral("jpeg") || effExt == QStringLiteral("bmp")
            || effExt == QStringLiteral("tif") || effExt == QStringLiteral("tiff")) {
            QImage img(finalOutPath);
            if (!img.isNull()) {
                outResult[QStringLiteral("width_px")] = img.width();
                outResult[QStringLiteral("height_px")] = img.height();
            }
        }
        return true;
    }

    if (method == QStringLiteral("graph.copy_image")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const int dpi = parseBoundedInt(params, QStringLiteral("dpi"), 300, 36, 2400);
        const QImage img = ml->copyPageAsImage(dpi);
        if (!img.isNull() && QApplication::clipboard())
            QApplication::clipboard()->setImage(img);
        outResult[QStringLiteral("ok")] = !img.isNull();
        outResult[QStringLiteral("width_px")] = img.width();
        outResult[QStringLiteral("height_px")] = img.height();
        return true;
    }

    if (method == QStringLiteral("graph.copy_ole")) {
#ifdef Q_OS_WIN
        const QString reqGraph = optionalJsonString(params, QStringLiteral("graph")).trimmed();
        Graph3D *targetG3D = nullptr;
        if (!reqGraph.isEmpty()) {
            if (MyWidget *mw = findWindowByName(d_app, reqGraph)) {
                if (mw->inherits("Graph3D"))
                    targetG3D = static_cast<Graph3D *>(mw);
            }
        } else if (QWidget *actW = d_app->d_workspace.activeSubWindow()) {
            if (actW->inherits("Graph3D"))
                targetG3D = static_cast<Graph3D *>(actW);
        }
        if (targetG3D) {
            QImage fb;
            targetG3D->update();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (targetG3D->sp) {
                targetG3D->sp->makeCurrent();
                fb = targetG3D->sp->grabFramebuffer();
            }
            const bool ok = SciDAVisOle::copyImageAsOle(d_app, fb, targetG3D->name());
            outResult[QStringLiteral("ok")] = ok;
            outResult[QStringLiteral("graph")] = targetG3D->name();
            return true;
        }
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const QString presetStr = optionalJsonString(params, QStringLiteral("preset")).trimmed();
        if (!presetStr.isEmpty()) {
            MultiLayer::GraphStylePresetSpec spec;
            if (resolveStylePresetForRpc(presetStr, &spec, true)) {
                ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Apply Style Preset"));
                applyResolvedStylePreset(ml, spec);
                undoGuard.commit();
            }
        }
        Graph *singleG = params.value(QStringLiteral("active_layer_only")).toBool(false)
                ? ml->activeGraph()
                : nullptr;
        const bool ok = SciDAVisOle::copyGraphAsOle(d_app, ml, singleG);
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("graph")] = ml->name();
        return true;
#else
        throwRpcRuntime(QStringLiteral("OLE clipboard export is only supported on Windows."));
#endif
    }

    if (method == QStringLiteral("graph.auto_layout")
        || method == QStringLiteral("graph.fix_layout")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        outResult = runAutoLayoutSolver(d_app, ml, params);
        return true;
    }

    if (method == QStringLiteral("graph.to_python")) {
        const QString reqWin = !optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("graph")).trimmed()
                : optionalJsonString(params, QStringLiteral("window")).trimmed();
        MyWidget *targetWin = nullptr;
        if (!reqWin.isEmpty()) {
            targetWin = findWindowByName(d_app, reqWin);
            if (!targetWin) {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("Window not found: '%1'.").arg(reqWin),
                        collectWindowNames(d_app));
            }
        } else {
            if (QWidget *actW = d_app->d_workspace.activeSubWindow())
                targetWin = qobject_cast<MyWidget *>(actW);
            if (!targetWin) {
                for (MyWidget *w : d_app->windowsList()) {
                    if (w && (w->inherits("MultiLayer") || w->inherits("Graph3D")))
                        targetWin = w;
                }
            }
            if (!targetWin && !d_app->windowsList().isEmpty())
                targetWin = d_app->windowsList().last();
            if (!targetWin)
                throwInvalidParams(QStringLiteral("No open graph or data window in workspace."));
        }

        const bool standalone = optionalJsonBool(params, QStringLiteral("standalone"), true);
        const QString embedData =
                optionalJsonString(params, QStringLiteral("embed_data"), QStringLiteral("inline"))
                        .trimmed()
                        .toLower();
        if (embedData != QStringLiteral("inline") && embedData != QStringLiteral("csv")
            && embedData != QStringLiteral("reference")) {
            throwInvalidParams(
                    QStringLiteral("Unknown embed_data '%1' (expected 'inline'|'csv'|'reference').")
                            .arg(embedData));
        }

        const QString outPathRaw = !optionalJsonString(params, QStringLiteral("path")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("path")).trimmed()
                : optionalJsonString(params, QStringLiteral("output_path")).trimmed();
        QString absOutPath;
        QString sidecarDir;
        if (!outPathRaw.isEmpty()) {
            const QFileInfo fi(outPathRaw);
            if (!fi.dir().exists())
                QDir().mkpath(fi.dir().absolutePath());
            absOutPath = fi.absoluteFilePath();
            sidecarDir = fi.dir().absolutePath();
        }

        QStringList refTables, refMatrices;
        const QString codeStr = generatePythonScriptInternal(
                d_app, targetWin, standalone, embedData, sidecarDir, &refTables, &refMatrices);

        if (!absOutPath.isEmpty()) {
            QSaveFile sf(absOutPath);
            const QByteArray utf8Bytes = codeStr.toUtf8();
            if (!sf.open(QIODevice::WriteOnly) || sf.write(utf8Bytes) != utf8Bytes.size()
                || !sf.commit()) {
                throwRpcRuntime(
                        QStringLiteral("Failed to write Python script file '%1'.").arg(absOutPath));
            }
        }

        const int lineCount = codeStr.trimmed().isEmpty()
                ? 0
                : codeStr.trimmed().split(QLatin1Char('\n')).size();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = targetWin->name();
        outResult[QStringLiteral("window")] = targetWin->name();
        outResult[QStringLiteral("kind")] = windowTypeString(targetWin);
        outResult[QStringLiteral("script")] = codeStr;
        outResult[QStringLiteral("code")] = codeStr;
        outResult[QStringLiteral("lines")] = lineCount;
        outResult[QStringLiteral("tables")] = toJsonArray(refTables);
        outResult[QStringLiteral("matrices")] = toJsonArray(refMatrices);
        if (!absOutPath.isEmpty()) {
            outResult[QStringLiteral("path")] = absOutPath;
            outResult[QStringLiteral("output_path")] = absOutPath;
        }
        return true;
    }

    if (method == QStringLiteral("graph.export_ole")) {
#ifdef Q_OS_WIN
        const int dpi = parseBoundedInt(params, QStringLiteral("dpi"), 300, 72, 2400);
        const bool activeLayerOnly =
                optionalJsonBool(params, QStringLiteral("active_layer_only"), false);
        const bool includeBase64 =
                optionalJsonBool(params, QStringLiteral("include_base64"), true);
        const bool skipProjectStream =
                optionalJsonBool(params, QStringLiteral("skip_project_stream"), false);
        const QString olePathRaw = !optionalJsonString(params, QStringLiteral("ole_path")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("ole_path")).trimmed()
                : optionalJsonString(params, QStringLiteral("path")).trimmed();
        const bool needProjectBytes = !skipProjectStream || !olePathRaw.isEmpty();
        const bool needOleCompound = needProjectBytes && (includeBase64 || !olePathRaw.isEmpty());
        // "graph" (default): embed the graph and the tables/matrices it plots only;
        // "project": embed the whole project (and results log), as before.
        const QString embedScopeStr =
                optionalJsonString(params, QStringLiteral("embed_scope")).trimmed().toLower();
        SciDAVisOle::EmbedScope embedScope = SciDAVisOle::EmbedScope::GraphOnly;
        if (embedScopeStr == QStringLiteral("project") || embedScopeStr == QStringLiteral("whole")
            || embedScopeStr == QStringLiteral("all"))
            embedScope = SciDAVisOle::EmbedScope::WholeProject;
        else if (!embedScopeStr.isEmpty() && embedScopeStr != QStringLiteral("graph"))
            throwInvalidParams(QStringLiteral("Unknown embed_scope '%1' (expected 'graph'|'project').")
                                       .arg(embedScopeStr));
        QStringList embeddedWindows;

        const QString reqGraph = optionalJsonString(params, QStringLiteral("graph")).trimmed();
        Graph3D *targetG3D = nullptr;
        MultiLayer *ml = nullptr;
        if (!reqGraph.isEmpty()) {
            if (MyWidget *mw = findWindowByName(d_app, reqGraph)) {
                if (mw->inherits("Graph3D"))
                    targetG3D = static_cast<Graph3D *>(mw);
            }
        } else if (QWidget *actW = d_app->d_workspace.activeSubWindow()) {
            if (actW->inherits("Graph3D"))
                targetG3D = static_cast<Graph3D *>(actW);
        }
        if (!targetG3D) {
            ml = resolveMultiLayer(d_app, params);
            const QString presetStr = optionalJsonString(params, QStringLiteral("preset")).trimmed();
            if (!presetStr.isEmpty()) {
                MultiLayer::GraphStylePresetSpec spec;
                if (resolveStylePresetForRpc(presetStr, &spec, true)) {
                    ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Apply Style Preset"));
                    applyResolvedStylePreset(ml, spec);
                    undoGuard.commit();
                }
            }
            // Fonts only: lay the layers out again and redraw, but do not run updatePlot(),
            // which re-autoscales every unlocked axis and so dropped ticks/ranges set by a
            // style preset or the legend headroom from the embedded copy (and the graph).
            for (Graph *lg : ml->layersList()) {
                if (lg) {
                    enforceArialGraphFonts(lg);
                    if (lg->plotWidget())
                        lg->plotWidget()->updateLayout();
                    lg->replot();
                }
            }
        }

        QByteArray projBytes;
        bool scopeDowngraded = false;
        bool oleExceedsLimits = false;
        QString oleScopeWarning;
        QString oleLimitReason;
        static constexpr qint64 kMaxOleStreamBytes = 128000000LL;
        if (needProjectBytes) {
            for (MyWidget *w : d_app->windowsList()) {
                if (w && w->inherits("MultiLayer")) {
                    for (Graph *lg : static_cast<MultiLayer *>(w)->layersList()) {
                        if (lg)
                            enforceArialGraphFonts(lg);
                    }
                }
            }
            sanitizeAllProjectFunctionCurves(d_app);
            MyWidget *embedTarget = targetG3D ? static_cast<MyWidget *>(targetG3D)
                                              : static_cast<MyWidget *>(ml);
            projBytes = SciDAVisOle::buildEmbeddedProjectBytes(d_app, embedTarget, embedScope,
                                                               &embeddedWindows);
            if (projBytes.size() > kMaxOleStreamBytes
                && embedScope == SciDAVisOle::EmbedScope::WholeProject) {
                embedScope = SciDAVisOle::EmbedScope::GraphOnly;
                embeddedWindows.clear();
                projBytes = SciDAVisOle::buildEmbeddedProjectBytes(d_app, embedTarget, embedScope,
                                                                   &embeddedWindows);
                scopeDowngraded = true;
                oleScopeWarning = QStringLiteral(
                        "Whole-project OLE stream exceeded 128 MB; automatically downgraded "
                        "embed_scope to 'graph' (%1 bytes).")
                                          .arg(projBytes.size());
            }
            if (projBytes.isEmpty()) {
                throwRpcRuntime(
                        QStringLiteral("Failed to serialize .sciprj payload for OLE export."));
            }
            if (projBytes.size() > kMaxOleStreamBytes) {
                oleExceedsLimits = true;
                oleLimitReason = QStringLiteral(
                        "Embedded OLE project stream (%1 bytes) exceeds the 128 MB "
                        "limit; export a static EMF/PNG image instead.")
                                         .arg(projBytes.size());
                projBytes.clear();
                embeddedWindows.clear();
            }
        }

        SciDAVisOle::OleGraphPayload payload;
        payload.projectBytes = projBytes;

        OleRenderedGraph rg;
        QString targetName;
        if (targetG3D) {
            targetName = targetG3D->name();
            payload.targetWindow = targetName;
            payload.targetLayer = 0;
            QImage fb;
            targetG3D->update();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (targetG3D->sp) {
                targetG3D->sp->makeCurrent();
                fb = targetG3D->sp->grabFramebuffer();
            }
            rg = SciDAVisEmf::renderFromImage(fb);
        } else {
            targetName = ml->name();
            payload.targetWindow = targetName;
            payload.targetLayer = 0;
            Graph *singleG = activeLayerOnly ? ml->activeGraph() : nullptr;
            if (singleG) {
                for (int i = 1; i <= ml->layers(); ++i) {
                    if (ml->layer(i) == singleG) {
                        payload.targetLayer = i;
                        break;
                    }
                }
            }
            rg = SciDAVisEmf::renderMultiLayer(ml, singleG, false);
        }
        if (rg.emfBytes.isEmpty())
            throwRpcRuntime(QStringLiteral("Failed to render vector EMF preview for OLE export."));
        if (rg.emfBytes.size() > kMaxOleStreamBytes) {
            oleExceedsLimits = true;
            if (oleLimitReason.isEmpty()) {
                oleLimitReason = QStringLiteral(
                        "Rendered EMF preview (%1 bytes) exceeds the 128 MB OLE stream "
                        "limit; export a raster PNG image instead.")
                                         .arg(rg.emfBytes.size());
            }
            projBytes.clear();
            embeddedWindows.clear();
        }

        payload.emfBytes = rg.emfBytes;
        payload.dibBytes = rg.dibBytes;
        payload.extentHm = rg.extentHm;
        payload.pixelSize = rg.pixelSize;

        // Build self-contained CFBF OLE Compound Storage (.bin) in memory when needed (zero COM out-of-proc launch!)
        QByteArray oleCompoundBytes;
        if (needOleCompound && !oleExceedsLimits) {
            bool cfbExceededLimit = false;
            qint64 cfbBytesSize = 0;
            ILockBytes *lockBytes = nullptr;
            HRESULT hr = CreateILockBytesOnHGlobal(nullptr, TRUE, &lockBytes);
            if (SUCCEEDED(hr) && lockBytes) {
                IStorage *stg = nullptr;
                hr = StgCreateDocfileOnILockBytes(
                        lockBytes, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
                if (SUCCEEDED(hr) && stg) {
                    WriteClassStg(stg, SciDAVisOle::graph_clsid);
                    hr = SciDAVisOle::savePayloadToStorage(stg, payload);
                    if (SUCCEEDED(hr))
                        stg->Commit(STGC_DEFAULT);
                    else if (hr == STG_E_INSUFFICIENTMEMORY)
                        cfbExceededLimit = true;
                    stg->Release();
                    if (SUCCEEDED(hr)) {
                        STATSTG stat{};
                        if (SUCCEEDED(lockBytes->Stat(&stat, STATFLAG_NONAME))
                            && stat.cbSize.QuadPart > 0) {
                            cfbBytesSize = static_cast<qint64>(stat.cbSize.QuadPart);
                            if (stat.cbSize.QuadPart >= 256000000ULL) {
                                cfbExceededLimit = true;
                            } else {
                                HGLOBAL hg = nullptr;
                                if (SUCCEEDED(GetHGlobalFromILockBytes(lockBytes, &hg)) && hg) {
                                    const void *rawPtr = GlobalLock(hg);
                                    if (rawPtr) {
                                        oleCompoundBytes = QByteArray(
                                                static_cast<const char *>(rawPtr),
                                                static_cast<int>(stat.cbSize.QuadPart));
                                        GlobalUnlock(hg);
                                    }
                                }
                            }
                        }
                    }
                }
                lockBytes->Release();
            }
            if (cfbExceededLimit) {
                oleExceedsLimits = true;
                oleLimitReason = cfbBytesSize > 0
                        ? QStringLiteral(
                                  "Combined OLE compound storage (%1 bytes) reaches or exceeds the "
                                  "256 MB PowerPoint container limit; export a static EMF/PNG image instead.")
                                  .arg(cfbBytesSize)
                        : QStringLiteral(
                                  "Combined OLE compound storage exceeds PowerPoint container limits; "
                                  "export a static EMF/PNG image instead.");
                projBytes.clear();
                oleCompoundBytes.clear();
                embeddedWindows.clear();
            } else if (oleCompoundBytes.isEmpty()) {
                throwRpcRuntime(QStringLiteral("Failed to build OLE Compound Storage binary."));
            }
        }

        const QString emfPathRaw = optionalJsonString(params, QStringLiteral("emf_path")).trimmed();
        QString absEmfPath;
        if (!emfPathRaw.isEmpty()) {
            const QFileInfo efi(emfPathRaw);
            if (!efi.dir().exists())
                QDir().mkpath(efi.dir().absolutePath());
            absEmfPath = efi.absoluteFilePath();
            QSaveFile ef(absEmfPath);
            if (!ef.open(QIODevice::WriteOnly) || ef.write(rg.emfBytes) != rg.emfBytes.size()
                || !ef.commit()) {
                throwRpcRuntime(
                        QStringLiteral("Failed to write EMF preview '%1'.").arg(absEmfPath));
            }
        }

        QString absOlePath;
        if (!olePathRaw.isEmpty() && !oleExceedsLimits && !oleCompoundBytes.isEmpty()) {
            const QFileInfo ofi(olePathRaw);
            if (!ofi.dir().exists())
                QDir().mkpath(ofi.dir().absolutePath());
            absOlePath = ofi.absoluteFilePath();
            QSaveFile of(absOlePath);
            if (!of.open(QIODevice::WriteOnly)
                || of.write(oleCompoundBytes) != oleCompoundBytes.size() || !of.commit()) {
                throwRpcRuntime(
                        QStringLiteral("Failed to write OLE storage file '%1'.").arg(absOlePath));
            }
        }

        const double widthMm = (rg.extentHm.cx > 0) ? (double(rg.extentHm.cx) / 100.0) : 85.0;
        const double heightMm = (rg.extentHm.cy > 0) ? (double(rg.extentHm.cy) / 100.0) : 65.0;

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = targetName;
        if (!targetG3D && ml) {
            const QString appliedPreset = ml->property("toriAppliedStylePreset").toString().trimmed();
            outResult[QStringLiteral("applied_style_preset")] = appliedPreset;
            outResult[QStringLiteral("style_preset")] = appliedPreset;
            if (!appliedPreset.isEmpty()) {
                MultiLayer::GraphStylePresetSpec presetSpec;
                if (MultiLayer::findStylePreset(appliedPreset, &presetSpec)
                    && presetSpec.slidePastePercent > 0.0) {
                    outResult[QStringLiteral("slide_paste_percent")] =
                            safeJsonDouble(presetSpec.slidePastePercent);
                }
            }
        }
        outResult[QStringLiteral("clsid")] =
                QStringLiteral("{8E6B9541-3C74-4D28-A915-7D84E5A12001}");
        outResult[QStringLiteral("prog_id")] = QStringLiteral("ToriGraph.Graph.1");
        outResult[QStringLiteral("dpi")] = dpi;
        outResult[QStringLiteral("embed_scope")] =
                embedScope == SciDAVisOle::EmbedScope::WholeProject ? QStringLiteral("project")
                                                                    : QStringLiteral("graph");
        if (scopeDowngraded) {
            outResult[QStringLiteral("scope_downgraded")] = true;
            outResult[QStringLiteral("warning")] = oleScopeWarning;
        }
        if (oleExceedsLimits) {
            outResult[QStringLiteral("ole_exceeds_limits")] = true;
            outResult[QStringLiteral("limit_reason")] = oleLimitReason;
        }
        if (needProjectBytes && !oleExceedsLimits)
            outResult[QStringLiteral("embedded_windows")] =
                    QJsonArray::fromStringList(embeddedWindows);
        outResult[QStringLiteral("width_mm")] = safeJsonDouble(widthMm);
        outResult[QStringLiteral("height_mm")] = safeJsonDouble(heightMm);
        outResult[QStringLiteral("width_hm")] = static_cast<int>(rg.extentHm.cx);
        outResult[QStringLiteral("height_hm")] = static_cast<int>(rg.extentHm.cy);
        outResult[QStringLiteral("width_px")] = rg.pixelSize.width();
        outResult[QStringLiteral("height_px")] = rg.pixelSize.height();
        if (!skipProjectStream && !oleExceedsLimits && !projBytes.isEmpty())
            outResult[QStringLiteral("sciprj_xml")] = QString::fromUtf8(projBytes);
        outResult[QStringLiteral("emf_bytes")] = rg.emfBytes.size();
        outResult[QStringLiteral("ole_bytes")] = oleCompoundBytes.size();
        if (includeBase64) {
            outResult[QStringLiteral("emf_base64")] = QString::fromLatin1(rg.emfBytes.toBase64());
            if (!oleCompoundBytes.isEmpty()) {
                outResult[QStringLiteral("ole_base64")] =
                        QString::fromLatin1(oleCompoundBytes.toBase64());
            }
        }
        if (!absEmfPath.isEmpty())
            outResult[QStringLiteral("emf_path")] = absEmfPath;
        if (!absOlePath.isEmpty()) {
            outResult[QStringLiteral("ole_path")] = absOlePath;
            outResult[QStringLiteral("path")] = absOlePath;
        }
        return true;
#else
        throwRpcRuntime(QStringLiteral("OLE export is only supported on Windows."));
#endif
    }

    if (method == QStringLiteral("graph.set_3d_options")) {
        static const QSet<QString> kAllowed3DKeys = {
            QStringLiteral("graph"),          QStringLiteral("window"),
            QStringLiteral("x_rotation"),     QStringLiteral("y_rotation"),
            QStringLiteral("z_rotation"),     QStringLiteral("zoom"),
            QStringLiteral("orthogonal"),     QStringLiteral("style"),
            QStringLiteral("axes_labels"),    QStringLiteral("title"),
            QStringLiteral("title_color"),    QStringLiteral("title_font_size"),
            QStringLiteral("colormap"),       QStringLiteral("palette"),
            QStringLiteral("reverse_colormap"), QStringLiteral("from_color"),
            QStringLiteral("to_color"),       QStringLiteral("x_scale"),
            QStringLiteral("y_scale"),        QStringLiteral("z_scale"),
            QStringLiteral("scale"),          QStringLiteral("x_range"),
            QStringLiteral("y_range"),        QStringLiteral("z_range"),
            QStringLiteral("resolution"),     QStringLiteral("legend"),
            QStringLiteral("colorbar"),       QStringLiteral("transparency"),
            QStringLiteral("alpha"),          QStringLiteral("mesh_line_width"),
            QStringLiteral("smooth_mesh"),    QStringLiteral("coord_style")
        };
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (!kAllowed3DKeys.contains(it.key())) {
                throwInvalidParams(
                        QStringLiteral("Unknown 3D option '%1' in graph.set_3d_options.")
                                .arg(it.key()));
            }
        }

        Graph3D *g3d = resolveGraph3D(d_app, params);
        if (params.contains(QStringLiteral("x_rotation"))
            || params.contains(QStringLiteral("y_rotation"))
            || params.contains(QStringLiteral("z_rotation"))) {
            const double rx = optionalJsonFiniteDouble(params, QStringLiteral("x_rotation"),
                                                       g3d->xRotation(), 36000.0);
            const double ry = optionalJsonFiniteDouble(params, QStringLiteral("y_rotation"),
                                                       g3d->yRotation(), 36000.0);
            const double rz = optionalJsonFiniteDouble(params, QStringLiteral("z_rotation"),
                                                       g3d->zRotation(), 36000.0);
            g3d->setRotation(rx, ry, rz);
        }
        if (params.contains(QStringLiteral("zoom")) && !params.value(QStringLiteral("zoom")).isNull()) {
            const double reqZ = parseFiniteCoordDouble(params.value(QStringLiteral("zoom")),
                                                       QStringLiteral("zoom"), 100000.0);
            if (reqZ <= 0.0)
                throwInvalidParams(QStringLiteral("3D 'zoom' must be > 0."));
            double baseHullZoom = 1.0;
            if (g3d->sp) {
                const Qwt3D::ParallelEpiped hull = g3d->sp->hull();
                const double d = (hull.maxVertex - hull.minVertex).length();
                if (std::isfinite(d) && d > 1e-12)
                    baseHullZoom = d / std::sqrt(3.0);
            }
            const double effZoom =
                    (reqZ <= 10.0 && baseHullZoom > 10.0) ? (reqZ * baseHullZoom) : reqZ;
            g3d->setZoom(effZoom);
        }
        if (params.contains(QStringLiteral("orthogonal")))
            g3d->setOrtho(optionalJsonBool(params, QStringLiteral("orthogonal"), true));

        if (params.contains(QStringLiteral("scale"))
            || params.contains(QStringLiteral("x_scale"))
            || params.contains(QStringLiteral("y_scale"))
            || params.contains(QStringLiteral("z_scale"))) {
            double sx = g3d->xScale();
            double sy = g3d->yScale();
            double sz = g3d->zScale();
            if (params.value(QStringLiteral("scale")).isArray()) {
                const QJsonArray sa = params.value(QStringLiteral("scale")).toArray();
                if (sa.size() != 3)
                    throwInvalidParams(QStringLiteral("'scale' must be a 3-number array [sx, sy, sz]."));
                sx = sa[0].toDouble(sx);
                sy = sa[1].toDouble(sy);
                sz = sa[2].toDouble(sz);
            }
            if (params.contains(QStringLiteral("x_scale")))
                sx = params.value(QStringLiteral("x_scale")).toDouble(sx);
            if (params.contains(QStringLiteral("y_scale")))
                sy = params.value(QStringLiteral("y_scale")).toDouble(sy);
            if (params.contains(QStringLiteral("z_scale")))
                sz = params.value(QStringLiteral("z_scale")).toDouble(sz);
            if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sz)
                || sx <= 0.0 || sy <= 0.0 || sz <= 0.0) {
                throwInvalidParams(QStringLiteral("3D axis scales must be finite positive numbers."));
            }
            g3d->setScale(sx, sy, sz);
        }

        if (params.contains(QStringLiteral("x_range"))
            || params.contains(QStringLiteral("y_range"))
            || params.contains(QStringLiteral("z_range"))) {
            double xl = g3d->xStart(), xr = g3d->xStop();
            double yl = g3d->yStart(), yr = g3d->yStop();
            double zl = g3d->zStart(), zr = g3d->zStop();
            auto parsePair = [](const QJsonValue &v, const QString &nm, double &lo, double &hi) {
                if (!v.isArray() || v.toArray().size() != 2)
                    throwInvalidParams(
                            QStringLiteral("'%1' must be a 2-number array [min, max].").arg(nm));
                const QJsonArray a = v.toArray();
                if (!a[0].isDouble() || a[0].isBool() || !a[1].isDouble() || a[1].isBool())
                    throwInvalidParams(
                            QStringLiteral("'%1' elements must be finite numbers.").arg(nm));
                lo = a[0].toDouble();
                hi = a[1].toDouble();
                if (!std::isfinite(lo) || !std::isfinite(hi) || lo >= hi)
                    throwInvalidParams(
                            QStringLiteral("'%1' requires finite min < max.").arg(nm));
            };
            const bool hasXRange = params.contains(QStringLiteral("x_range"))
                    && !params.value(QStringLiteral("x_range")).isNull();
            const bool hasYRange = params.contains(QStringLiteral("y_range"))
                    && !params.value(QStringLiteral("y_range")).isNull();
            const bool hasZRange = params.contains(QStringLiteral("z_range"))
                    && !params.value(QStringLiteral("z_range")).isNull();
            if (hasXRange)
                parsePair(params.value(QStringLiteral("x_range")), QStringLiteral("x_range"), xl, xr);
            if (hasYRange)
                parsePair(params.value(QStringLiteral("y_range")), QStringLiteral("y_range"), yl, yr);
            if (hasZRange)
                parsePair(params.value(QStringLiteral("z_range")), QStringLiteral("z_range"), zl, zr);

            auto normalizeUnsetRange = [](double &lo, double &hi, double fbStart, double fbEnd) {
                if (!std::isfinite(lo) || !std::isfinite(hi)) {
                    lo = 0.0;
                    hi = 1.0;
                } else if (lo > hi) {
                    std::swap(lo, hi);
                } else if (lo == hi) {
                    const double fLo = std::min(fbStart, fbEnd);
                    const double fHi = std::max(fbStart, fbEnd);
                    if (std::isfinite(fLo) && std::isfinite(fHi) && fLo < fHi) {
                        lo = fLo;
                        hi = fHi;
                    } else {
                        lo -= 0.5;
                        hi += 0.5;
                    }
                }
            };
            Matrix *m3d = g3d->matrix();
            if (!hasXRange)
                normalizeUnsetRange(xl, xr, m3d ? m3d->xStart() : 0.0, m3d ? m3d->xEnd() : 1.0);
            if (!hasYRange)
                normalizeUnsetRange(yl, yr, m3d ? m3d->yStart() : 0.0, m3d ? m3d->yEnd() : 1.0);
            if (!hasZRange)
                normalizeUnsetRange(zl, zr, 0.0, 1.0);

            auto applyDirect3DCoordLimits = [&]() {
                if (g3d->sp) {
                    g3d->sp->makeCurrent();
                    g3d->sp->createCoordinateSystem(Qwt3D::Triple(xl, yl, zl),
                                                    Qwt3D::Triple(xr, yr, zr));
                    if (g3d->sp->legend())
                        g3d->sp->legend()->setLimits(zl, zr);
                }
                g3d->update();
            };

            if (g3d->func) {
                if (g3d->sp)
                    g3d->sp->makeCurrent();
                g3d->func->setDomain(xl, xr, yl, yr);
                g3d->func->setMinZ(zl);
                g3d->func->setMaxZ(zr);
                g3d->func->create();
                applyDirect3DCoordLimits();
            } else if (m3d) {
                const double mxSpan = std::fabs(m3d->xEnd() - m3d->xStart());
                const double mySpan = std::fabs(m3d->yEnd() - m3d->yStart());
                if (m3d->numCols() >= 2 && m3d->numRows() >= 2 && mxSpan > 0.0 && mySpan > 0.0) {
                    const double dx = mxSpan / double(m3d->numCols() - 1);
                    const double dy = mySpan / double(m3d->numRows() - 1);
                    const double gridCols = (xr - xl) / dx;
                    const double gridRows = (yr - yl) / dy;
                    if (!std::isfinite(gridCols) || !std::isfinite(gridRows)
                        || gridCols > 4096.0 || gridRows > 4096.0) {
                        throwInvalidParams(
                                QStringLiteral("Requested 3D matrix x_range/y_range exceeds maximum grid size (4096x4096)."));
                    }
                    if (gridCols >= 1.0 && gridRows >= 1.0)
                        g3d->updateScales(xl, xr, yl, yr, zl, zr);
                    else
                        applyDirect3DCoordLimits();
                } else {
                    applyDirect3DCoordLimits();
                }
            } else if (g3d->getTable()) {
                g3d->updateScales(xl, xr, yl, yr, zl, zr);
                if (g3d->sp && g3d->sp->legend())
                    g3d->sp->legend()->setLimits(zl, zr);
                g3d->update();
            } else {
                applyDirect3DCoordLimits();
            }
        }

        if (params.contains(QStringLiteral("style"))) {
            const QString st = params.value(QStringLiteral("style")).toString().trimmed().toLower();
            if (st == QStringLiteral("wireframe") || st == QStringLiteral("mesh"))
                g3d->setLineGrid();
            else if (st == QStringLiteral("hidden_line") || st == QStringLiteral("hiddenline"))
                g3d->setHiddenLineGrid();
            else if (st == QStringLiteral("surface") || st == QStringLiteral("filled_mesh"))
                g3d->setFilledMesh();
            else if (st == QStringLiteral("filled") || st == QStringLiteral("smooth_surface"))
                g3d->setNoGrid();
            else if (st == QStringLiteral("bars"))
                g3d->setBarsPlot();
            else if (st == QStringLiteral("scatter") || st == QStringLiteral("points")
                     || st == QStringLiteral("dots"))
                g3d->setPointsMesh();
            else
                throwInvalidParams(
                        QStringLiteral("Unknown 3D style '%1' (expected surface|wireframe|hidden_line|filled|bars|scatter).")
                                .arg(st));
        }

        if (params.contains(QStringLiteral("coord_style"))) {
            const QString cs =
                    params.value(QStringLiteral("coord_style")).toString().trimmed().toLower();
            if (cs == QStringLiteral("box") || cs == QStringLiteral("boxed"))
                g3d->setBoxed();
            else if (cs == QStringLiteral("frame") || cs == QStringLiteral("framed"))
                g3d->setFramed();
            else if (cs == QStringLiteral("none") || cs == QStringLiteral("no_axes"))
                g3d->setNoAxes();
            else
                throwInvalidParams(
                        QStringLiteral("Unknown 3D coord_style '%1' (expected box|frame|none).")
                                .arg(cs));
        }

        if (params.contains(QStringLiteral("title"))
            || params.contains(QStringLiteral("title_color"))
            || params.contains(QStringLiteral("title_font_size"))) {
            const QString tStr = params.contains(QStringLiteral("title"))
                    ? params.value(QStringLiteral("title")).toString()
                    : g3d->plotTitle();
            const QColor tCol = params.contains(QStringLiteral("title_color"))
                    ? parseColorValue(params.value(QStringLiteral("title_color")), g3d->titleColor())
                    : g3d->titleColor();
            QFont tFont = g3d->titleFont();
            tFont.setFamily(QStringLiteral("Arial"));
            if (params.contains(QStringLiteral("title_font_size"))
                && !params.value(QStringLiteral("title_font_size")).isNull()) {
                tFont.setPointSize(std::max(
                        4, parseStrictJsonInt(params.value(QStringLiteral("title_font_size")),
                                              QStringLiteral("title_font_size"))));
            }
            g3d->setTitle(tStr, tCol, tFont);
        }

        if (params.value(QStringLiteral("axes_labels")).isArray()) {
            QStringList lbls;
            for (const QJsonValue &lv : params.value(QStringLiteral("axes_labels")).toArray())
                lbls << lv.toString();
            g3d->setAxesLabels(lbls);
        }

        if (params.contains(QStringLiteral("transparency"))
            || params.contains(QStringLiteral("alpha"))) {
            const double rawT = params.contains(QStringLiteral("transparency"))
                    ? params.value(QStringLiteral("transparency")).toDouble(g3d->transparency())
                    : params.value(QStringLiteral("alpha")).toDouble(g3d->transparency());
            g3d->changeTransparency(std::clamp(rawT, 0.0, 1.0));
        }

        const QString cmapStr = params.contains(QStringLiteral("colormap"))
                ? params.value(QStringLiteral("colormap")).toString().trimmed()
                : params.value(QStringLiteral("palette")).toString().trimmed();
        if (!cmapStr.isEmpty() && g3d->sp) {
            bool isGray = false;
            const auto preset = parseColorMapPreset(cmapStr, &isGray, true);
            const bool rev = params.value(QStringLiteral("reverse_colormap")).toBool(false);
            QwtLinearColorMap lmap = isGray
                    ? QwtLinearColorMap(Qt::black, Qt::white)
                    : Spectrogram::presetColorMap(preset, rev);
            if (isGray && rev)
                lmap = QwtLinearColorMap(Qt::white, Qt::black);
            const QwtDoubleInterval unitIv(0.0, 1.0);
            Qwt3D::ColorVector cv;
            cv.reserve(256);
            const double curAlpha = g3d->transparency();
            for (int i = 0; i < 256; ++i) {
                const QRgb qrgb = lmap.rgb(unitIv, double(i) / 255.0);
                Qwt3D::RGBA rgba;
                rgba.r = qRed(qrgb) / 255.0;
                rgba.g = qGreen(qrgb) / 255.0;
                rgba.b = qBlue(qrgb) / 255.0;
                rgba.a = curAlpha;
                cv.push_back(rgba);
            }
            auto *sc = new Qwt3D::StandardColor(g3d->sp);
            sc->setColorVector(cv);
            g3d->sp->setDataColor(sc);
            g3d->sp->updateData();
            if (g3d->isLegendOn()) {
                g3d->sp->showColorLegend(false);
                g3d->sp->showColorLegend(true);
            }
        } else if (params.contains(QStringLiteral("from_color"))
                   || params.contains(QStringLiteral("to_color"))) {
            const QColor cMin = params.contains(QStringLiteral("from_color"))
                    ? parseColorValue(params.value(QStringLiteral("from_color")), g3d->minDataColor())
                    : g3d->minDataColor();
            const QColor cMax = params.contains(QStringLiteral("to_color"))
                    ? parseColorValue(params.value(QStringLiteral("to_color")), g3d->maxDataColor())
                    : g3d->maxDataColor();
            g3d->setDataColors(cMin, cMax);
            if (g3d->sp)
                g3d->sp->updateData();
        }

        if (params.contains(QStringLiteral("resolution"))
            && !params.value(QStringLiteral("resolution")).isNull()) {
            g3d->setResolution(parseBoundedInt(params, QStringLiteral("resolution"), 1, 1, 32));
        }
        if (params.contains(QStringLiteral("legend"))
            || params.contains(QStringLiteral("colorbar"))) {
            const bool showLeg = params.contains(QStringLiteral("legend"))
                    ? params.value(QStringLiteral("legend")).toBool(true)
                    : params.value(QStringLiteral("colorbar")).toBool(true);
            g3d->showColorLegend(showLeg);
        }
        if (params.contains(QStringLiteral("mesh_line_width"))) {
            g3d->setMeshLineWidth(std::max(
                    1, params.value(QStringLiteral("mesh_line_width")).toInt(1)));
        }
        if (params.contains(QStringLiteral("smooth_mesh")))
            g3d->setSmoothMesh(params.value(QStringLiteral("smooth_mesh")).toBool(true));

        enforceArialGraph3DFonts(g3d);
        g3d->update();
        outResult = serializeGraph3DInfo(g3d);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    return false;
}

} // namespace rpc_internal
