/***************************************************************************
    File                 : PythonAutomationServer_Science.cpp
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
                           [Curve Math, Statistics, Specialized Plots & Digitizer]
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

namespace CurveMathToolsInternal {
CurveMathTools::EISResult analyzeEISWithOptions(
        ApplicationWindow *app, Table *srcTable, const QString &freqCol,
        const QString &zRealCol, const QString &zImagCol,
        bool zImagAlreadyNegated, bool fitCircuit, bool idealCapacitorOnly);
}

namespace {

class RpcIntegration : public Integration
{
public:
    RpcIntegration(ApplicationWindow *app, Graph *g) : Integration(app, g) { d_silent = true; }
};

class RpcDifferentiation : public Differentiation
{
public:
    RpcDifferentiation(ApplicationWindow *app, Graph *g) : Differentiation(app, g)
    {
        d_silent = true;
    }
};

class RpcInterpolation : public Interpolation
{
public:
    RpcInterpolation(ApplicationWindow *app, Graph *g, const QString &curveTitle, int m)
        : Interpolation(app, g, curveTitle, m)
    {
        d_silent = true;
    }
    RpcInterpolation(ApplicationWindow *app, Graph *g, const QString &curveTitle, double start,
                     double end, int m)
        : Interpolation(app, g, curveTitle, start, end, m)
    {
        d_silent = true;
    }
};

template <typename CM, typename = void>
struct HasWorkHardening6Args : std::false_type {};
template <typename CM>
struct HasWorkHardening6Args<
        CM, std::void_t<decltype(CM::analyzeWorkHardening(
                    static_cast<ApplicationWindow *>(nullptr), static_cast<Graph *>(nullptr),
                    QString(), 11, true, false))>>
    : std::true_type {};

template <typename CM = CurveMathTools>
CurveMathTools::WorkHardeningResult invokeAnalyzeWorkHardening(ApplicationWindow *app, Graph *g,
                                                               const QString &curveTitle,
                                                               int smoothWin, bool createPlot,
                                                               bool kmMode)
{
    if constexpr (HasWorkHardening6Args<CM>::value)
        return CM::analyzeWorkHardening(app, g, curveTitle, smoothWin, createPlot, kmMode);
    else
        return CM::analyzeWorkHardening(app, g, curveTitle, smoothWin, createPlot);
}

template <typename CM, typename = void>
struct HasAnalyzeXrd11Args : std::false_type {};
template <typename CM>
struct HasAnalyzeXrd11Args<
        CM, std::void_t<decltype(CM::analyzeXRD(
                    static_cast<ApplicationWindow *>(nullptr), static_cast<Graph *>(nullptr),
                    QString(), 1.540598, 0.05, 0.90, QString(), false, true, true, 1.633))>>
    : std::true_type {};

template <typename CM = CurveMathTools>
CurveMathTools::XRDAnalysisResult invokeAnalyzeXrd(ApplicationWindow *app, Graph *g,
                                                   const QString &curveTitle, double wavelength,
                                                   double instrFwhm, double scherrerK,
                                                   const QString &crystalSys, bool stripKa2,
                                                   bool addTopD, bool createWh, double cOverA)
{
    if constexpr (HasAnalyzeXrd11Args<CM>::value) {
        return CM::analyzeXRD(app, g, curveTitle, wavelength, instrFwhm, scherrerK, crystalSys,
                              stripKa2, addTopD, createWh, cOverA);
    } else {
        return CM::analyzeXRD(app, g, curveTitle, wavelength, instrFwhm, scherrerK, crystalSys,
                              stripKa2, addTopD, createWh);
    }
}

} // namespace

namespace rpc_internal {

RocMetricsComputed computeRocMetricsDirect(const std::vector<int> &yTrue,
                                           const std::vector<double> &scores)
{
    RocMetricsComputed m;
    const size_t n = std::min(yTrue.size(), scores.size());
    struct Pair { double score; int y; };
    std::vector<Pair> items;
    items.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(scores[i]))
            continue;
        const int yi = (yTrue[i] != 0) ? 1 : 0;
        items.push_back({ scores[i], yi });
        if (yi == 1)
            ++m.nPos;
        else
            ++m.nNeg;
    }
    if (m.nPos == 0 || m.nNeg == 0) {
        m.auc = 0.5;
        return m;
    }

    // Mann-Whitney U / trapezoidal ROC
    std::sort(items.begin(), items.end(),
              [](const Pair &a, const Pair &b) { return a.score > b.score; });

    double tp = 0.0, fp = 0.0;
    double prevTp = 0.0, prevFp = 0.0;
    double area = 0.0;
    double bestJ = -2.0;
    size_t i = 0;
    while (i < items.size()) {
        size_t j = i;
        const double thr = items[i].score;
        while (j < items.size() && items[j].score == thr) {
            if (items[j].y == 1)
                tp += 1.0;
            else
                fp += 1.0;
            ++j;
        }
        const double tpr = tp / double(m.nPos);
        const double fpr = fp / double(m.nNeg);
        area += 0.5 * (fpr - prevFp / double(m.nNeg)) * (tpr + prevTp / double(m.nPos));
        const double spec = 1.0 - fpr;
        const double youden = tpr + spec - 1.0;
        if (youden > bestJ) {
            bestJ = youden;
            m.optimalThreshold = thr;
            m.sensitivity = tpr;
            m.specificity = spec;
        }
        prevTp = tp;
        prevFp = fp;
        i = j;
    }
    m.auc = std::clamp(area, 0.0, 1.0);
    // Hanley-McNeil SE
    const double a = m.auc;
    const double q1 = a / (2.0 - a);
    const double q2 = (2.0 * a * a) / (1.0 + a);
    const double var = (a * (1.0 - a) + double(m.nPos - 1) * (q1 - a * a)
                        + double(m.nNeg - 1) * (q2 - a * a))
            / (double(m.nPos) * double(m.nNeg));
    m.se = (var > 0.0) ? std::sqrt(var) : 0.0;
    return m;
}

void extractBinaryStateRows(Table *t, int stIdx, const QVector<int> &numericCheckCols,
                            std::vector<int> &yOut, std::vector<int> &validRowsOut)
{
    yOut.clear();
    validRowsOut.clear();
    if (!t || stIdx < 0 || stIdx >= t->numCols())
        return;
    Column *stCol = t->column(stIdx);
    if (!stCol)
        return;

    const int nRows = t->numRows();
    auto rowHasValidPredictors = [&](int r) -> bool {
        for (int ci : numericCheckCols) {
            Column *xc = t->column(ci);
            if (!xc || xc->isInvalid(r) || xc->isMasked(r))
                return false;
            if (!std::isfinite(t->cell(r, ci)))
                return false;
        }
        return true;
    };

    if (stCol->columnMode() == SciDAVis::ColumnMode::Numeric) {
        std::vector<double> rawStates;
        std::vector<double> distinctStates;
        double minSt = std::numeric_limits<double>::infinity();
        double maxSt = -std::numeric_limits<double>::infinity();
        for (int r = 0; r < nRows; ++r) {
            if (stCol->isInvalid(r) || stCol->isMasked(r) || !rowHasValidPredictors(r))
                continue;
            const double sv = t->cell(r, stIdx);
            if (!std::isfinite(sv))
                continue;
            rawStates.push_back(sv);
            validRowsOut.push_back(r);
            minSt = std::min(minSt, sv);
            maxSt = std::max(maxSt, sv);
            bool seen = false;
            for (double ds : distinctStates) {
                if (std::fabs(sv - ds) <= 1e-12 * std::max(1.0, std::fabs(ds))) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                distinctStates.push_back(sv);
                if (distinctStates.size() > 2)
                    break;
            }
        }
        if (distinctStates.size() != 2) {
            yOut.clear();
            validRowsOut.clear();
            return;
        }
        yOut.reserve(rawStates.size());
        double mid = 0.5 * minSt + 0.5 * maxSt;
        if (!(mid > minSt && mid < maxSt))
            mid = minSt;
        for (double sv : rawStates)
            yOut.push_back((sv > mid) ? 1 : 0);
    } else {
        std::vector<QString> rawTexts;
        QStringList levels;
        auto isPos = [](const QString &low) -> bool {
            return low == QStringLiteral("1") || low == QStringLiteral("true")
                    || low == QStringLiteral("yes") || low == QStringLiteral("pos")
                    || low == QStringLiteral("positive") || low == QStringLiteral("case")
                    || low == QStringLiteral("event") || low == QStringLiteral("disease")
                    || low == QStringLiteral("treated") || low == QStringLiteral("pass")
                    || low == QStringLiteral("\u967d\u6027");
        };
        auto isNeg = [](const QString &low) -> bool {
            return low == QStringLiteral("0") || low == QStringLiteral("false")
                    || low == QStringLiteral("no") || low == QStringLiteral("none")
                    || low == QStringLiteral("neg") || low == QStringLiteral("negative")
                    || low == QStringLiteral("control") || low == QStringLiteral("healthy")
                    || low == QStringLiteral("normal") || low == QStringLiteral("\u9670\u6027");
        };
        for (int r = 0; r < nRows; ++r) {
            if (stCol->isInvalid(r) || stCol->isMasked(r) || !rowHasValidPredictors(r))
                continue;
            const QString txt = stCol->textAt(r).trimmed();
            if (txt.isEmpty())
                continue;
            if (!levels.contains(txt))
                levels.append(txt);
            rawTexts.push_back(txt);
            validRowsOut.push_back(r);
        }
        if (levels.size() == 2) {
            const QString &L0 = levels[0];
            const QString &L1 = levels[1];
            const QString low0 = L0.toLower();
            const QString low1 = L1.toLower();
            const bool p0 = isPos(low0), p1 = isPos(low1);
            const bool n0 = isNeg(low0), n1 = isNeg(low1);
            const QString posLevel = ((p0 && !p1) || (n1 && !n0)) ? L0 : L1;
            yOut.reserve(rawTexts.size());
            for (const QString &s : rawTexts)
                yOut.push_back((s == posLevel) ? 1 : 0);
        } else if (levels.size() > 2) {
            bool allExplicit = true;
            for (const QString &s : rawTexts) {
                const QString low = s.toLower();
                if (isPos(low) == isNeg(low)) {
                    allExplicit = false;
                    break;
                }
            }
            if (!allExplicit) {
                yOut.clear();
                validRowsOut.clear();
                return;
            }
            yOut.reserve(rawTexts.size());
            for (const QString &s : rawTexts)
                yOut.push_back(isPos(s.toLower()) ? 1 : 0);
        } else if (levels.size() == 1) {
            const int singleVal = isPos(levels[0].toLower()) ? 1 : 0;
            yOut.assign(rawTexts.size(), singleVal);
        }
    }
}

bool dispatchCurveMathAndStatsMethod(ApplicationWindow *d_app, const QString &method,
                                     const QJsonObject &params, QJsonObject &outResult)
{
    // ------------------------------------------------------------------------
    // 7C. Curve Math & Materials Science (curvemath.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("curvemath.eval")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString keyA = params.contains(QStringLiteral("curve_a"))
                ? QStringLiteral("curve_a")
                : QStringLiteral("curve1");
        const QString keyB = params.contains(QStringLiteral("curve_b"))
                ? QStringLiteral("curve_b")
                : QStringLiteral("curve2");
        QString cA, cB;
        const bool hasA = params.contains(keyA) && !params.value(keyA).isNull();
        const bool hasB = params.contains(keyB) && !params.value(keyB).isNull();
        bool pickedDefaults = false;
        if (!hasA && !hasB) {
            QStringList defaultCurves;
            for (int ci = 0; ci < g->curves() && defaultCurves.size() < 2; ++ci) {
                if (QwtPlotCurve *pc = g->curve(ci)) {
                    if (g->curveType(ci) != Graph::ErrorBars)
                        defaultCurves.append(pc->title().text());
                }
            }
            if (defaultCurves.size() >= 2) {
                cA = defaultCurves[0];
                cB = defaultCurves[1];
                pickedDefaults = true;
            }
        }
        if (!pickedDefaults) {
            cA = resolveCurveTitle(g, params, keyA, true);
            cB = resolveCurveTitle(g, params, keyB, true);
        }
        requireCurveMinPoints(g, cA, 2, QStringLiteral("curvemath.eval"));
        requireCurveMinPoints(g, cB, 2, QStringLiteral("curvemath.eval"));
        const QString opStr =
                optionalJsonString(params, QStringLiteral("op"), QStringLiteral("sub")).trimmed();
        CurveMathTools::MathOp op = CurveMathTools::Subtract;
        if (opStr == QStringLiteral("+")
            || opStr.compare(QStringLiteral("add"), Qt::CaseInsensitive) == 0
            || opStr.compare(QStringLiteral("plus"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Add;
        else if (opStr == QStringLiteral("-")
                 || opStr.compare(QStringLiteral("sub"), Qt::CaseInsensitive) == 0
                 || opStr.compare(QStringLiteral("subtract"), Qt::CaseInsensitive) == 0
                 || opStr.compare(QStringLiteral("minus"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Subtract;
        else if (opStr == QStringLiteral("*")
                 || opStr.compare(QStringLiteral("mul"), Qt::CaseInsensitive) == 0
                 || opStr.compare(QStringLiteral("multiply"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Multiply;
        else if (opStr == QStringLiteral("/")
                 || opStr.compare(QStringLiteral("div"), Qt::CaseInsensitive) == 0
                 || opStr.compare(QStringLiteral("divide"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Divide;
        else
            throwInvalidParams(
                    QStringLiteral("Unsupported curve math op '%1' (expected 'add'|'sub'|'mul'|'div' or '+'|'-'|'*'|'/').")
                            .arg(opStr));

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Curve Math"));
        QwtPlotCurve *outC = CurveMathTools::simpleCurveMath(
                d_app, g, cA, cB, op, optionalJsonBool(params, QStringLiteral("union_grid"), false));
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    if (method == QStringLiteral("curvemath.average_curves")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        QStringList titles;
        if (params.value(QStringLiteral("curves")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("curves")).toArray()) {
                QJsonObject tmp;
                tmp[QStringLiteral("curve")] = cv;
                titles << resolveCurveTitle(g, tmp, QStringLiteral("curve"), true);
            }
        } else {
            titles = g->analysableCurvesList();
        }
        if (titles.isEmpty())
            throwInvalidParams(QStringLiteral("curvemath.average_curves requires >= 1 curve."));
        for (const QString &ct : titles)
            requireCurveMinPoints(g, ct, 2, QStringLiteral("curvemath.average_curves"));
        const int numPts = parseBoundedInt(params, QStringLiteral("points"), 200, 2, 1000000);
        const bool sdBand = params.contains(QStringLiteral("sd_band"))
                ? optionalJsonBool(params, QStringLiteral("sd_band"), true)
                : optionalJsonBool(params, QStringLiteral("add_sd_band"), true);
        Table *outTbl = nullptr;
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Average Curves"));
        QwtPlotCurve *avgC = CurveMathTools::averageMultipleCurves(
                d_app, g, titles, numPts, sdBand, &outTbl);
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        if (avgC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (avgC != nullptr);
        if (avgC)
            outResult[QStringLiteral("output_curve")] = avgC->title().text();
        if (outTbl)
            outResult[QStringLiteral("table")] = outTbl->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.normalize")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("curvemath.normalize"));
        const QString m = params.value(QStringLiteral("mode")).toString().trimmed().toLower();
        CurveMathTools::NormMode nm = CurveMathTools::Range0To1;
        if (m == QStringLiteral("max") || m == QStringLiteral("divide_by_max"))
            nm = CurveMathTools::DivideByMax;
        else if (m.isEmpty() || m == QStringLiteral("0_1") || m == QStringLiteral("range_0_1")
                 || m == QStringLiteral("minmax"))
            nm = CurveMathTools::Range0To1;
        else if (m == QStringLiteral("-1_1") || m == QStringLiteral("range_-1_1"))
            nm = CurveMathTools::RangeMinus1To1;
        else if (m == QStringLiteral("zscore") || m == QStringLiteral("z_score"))
            nm = CurveMathTools::ZScore;
        else if (m == QStringLiteral("area") || m == QStringLiteral("unit_area"))
            nm = CurveMathTools::UnitArea;
        else if (m == QStringLiteral("sum100") || m == QStringLiteral("sum_100"))
            nm = CurveMathTools::SumTo100;
        else if (m == QStringLiteral("ref_x"))
            nm = CurveMathTools::DivideByRefX;
        else
            throwInvalidParams(
                    QStringLiteral("Unsupported normalize mode '%1'.").arg(m));

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Normalize Curve"));
        QwtPlotCurve *outC = CurveMathTools::normalizeCurve(
                d_app, g, curveTitle, nm, params.value(QStringLiteral("ref_x")).toDouble(0.0));
        enforceArialGraphFonts(g);
        g->replot();
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    if (method == QStringLiteral("curvemath.integrate")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        const QString mStr = params.value(QStringLiteral("method")).toString().trimmed().toLower();
        int integMethod = 0; // Linear
        int minPts = 2;
        if (mStr.isEmpty() || mStr == QStringLiteral("linear")
            || mStr == QStringLiteral("trapezoid") || mStr == QStringLiteral("trapezoidal")) {
            integMethod = 0;
            minPts = 2;
        } else if (mStr == QStringLiteral("cubic") || mStr == QStringLiteral("spline")) {
            integMethod = 1;
            minPts = 3;
        } else if (mStr == QStringLiteral("akima")) {
            integMethod = 2;
            minPts = 5;
        } else {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Unknown integration method '%1'.").arg(mStr),
                    { QStringLiteral("linear"), QStringLiteral("cubic"),
                      QStringLiteral("akima") });
        }

        double xMin = -std::numeric_limits<double>::infinity();
        double xMax = std::numeric_limits<double>::infinity();
        const bool hasRange = resolveCurveXRange(g, curveTitle, params,
                                                 QStringLiteral("curvemath.integrate"), xMin, xMax);

        std::vector<double> xs, ys;
        extractValidUnmaskedCurveXY(g->curve(g->curveIndex(curveTitle)), xs, ys);
        std::vector<double> rangeX;
        for (size_t i = 0; i < xs.size(); ++i) {
            if (!hasRange || (xs[i] >= xMin && xs[i] <= xMax))
                rangeX.push_back(xs[i]);
        }
        if (static_cast<int>(rangeX.size()) < minPts) {
            throwInvalidParams(
                    QStringLiteral("curvemath.integrate ('%1') requires at least %2 valid data points in range (got %3).")
                            .arg(mStr.isEmpty() ? QStringLiteral("linear") : mStr)
                            .arg(minPts)
                            .arg(rangeX.size()));
        }
        std::sort(rangeX.begin(), rangeX.end());
        for (size_t i = 1; i < rangeX.size(); ++i) {
            if (!(rangeX[i] > rangeX[i - 1] + 1e-15)) {
                throwInvalidParams(
                        QStringLiteral("curvemath.integrate requires strictly distinct X values (duplicate X=%1 detected).")
                                .arg(rangeX[i]));
            }
        }

        const bool plotCurve = params.value(QStringLiteral("plot_curve")).toBool(false);
        const int curvesBefore = g->curves();
        Legend *leg = g->legend();
        const QString legBefore = leg ? leg->text() : QString();

        RpcIntegration integ(d_app, g);
        // plot_curve=false は面積を返すだけ。積分曲線を一時的に足して消すと、その間に自動
        // スケールが掛かって軸範囲（2 軸なら左軸も）が変わったまま残るので、曲線を作らない。
        integ.setPlotResult(plotCurve);
        bool okData = false;
        if (hasRange)
            okData = integ.setDataFromCurve(curveTitle, xMin, xMax, g);
        else
            okData = integ.setDataFromCurve(curveTitle, g);
        if (!okData || integ.error()) {
            throwInvalidParams(
                    QStringLiteral("Insufficient curve data in range for integration on '%1'.")
                            .arg(curveTitle));
        }
        integ.setMethod(static_cast<Integration::InterpolationMethod>(integMethod));
        ScopedGraphUndoGuard undoGuard(plotCurve ? ml : nullptr, QObject::tr("Integrate Curve"));
        const bool ok = integ.run();
        const double areaVal = integ.result();

        QString outCurveTitle;
        if (!plotCurve) {
            // 曲線は作っていないはずだが、念のため増えた分だけ戻す（増えていなければ何もしない）。
            if (g->curves() > curvesBefore) {
                while (g->curves() > curvesBefore)
                    g->removeCurve(g->curves() - 1);
                if (leg)
                    leg->setText(legBefore);
                g->replot();
            }
        } else {
            sanitizeGraphFunctionCurves(d_app, g);
            enforceArialGraphFonts(g);
            g->replot();
            if (g->curves() > curvesBefore && g->curve(g->curves() - 1))
                outCurveTitle = g->curve(g->curves() - 1)->title().text();
        }

        if (!ok || !std::isfinite(areaVal)) {
            throwInvalidParams(
                    QStringLiteral("Integration failed to produce a finite area on curve '%1'.")
                            .arg(curveTitle));
        }
        if (plotCurve && g->curves() > curvesBefore)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("curve")] = curveTitle;
        outResult[QStringLiteral("area")] = safeJsonDouble(areaVal);
        if (plotCurve && !outCurveTitle.isEmpty())
            outResult[QStringLiteral("output_curve")] = outCurveTitle;
        return true;
    }

    if (method == QStringLiteral("curvemath.differentiate")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        int distinctX = 0;
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("curvemath.differentiate"),
                              &distinctX);
        if (distinctX < 4) {
            throwInvalidParams(
                    QStringLiteral("curvemath.differentiate requires at least 4 distinct X points (got %1).")
                            .arg(distinctX));
        }

        const int winBefore = d_app->windowsList().size();
        RpcDifferentiation diff(d_app, g);
        bool okData = false;
        double xMin = 0.0, xMax = 0.0;
        if (resolveCurveXRange(g, curveTitle, params, QStringLiteral("curvemath.differentiate"),
                               xMin, xMax)) {
            okData = diff.setDataFromCurve(curveTitle, xMin, xMax, g);
        } else {
            okData = diff.setDataFromCurve(curveTitle, g);
        }
        if (!okData || diff.error()) {
            throwInvalidParams(
                    QStringLiteral("Insufficient curve data (requires >= 4 points) for differentiation on '%1'.")
                            .arg(curveTitle));
        }
        const bool ok = diff.run();
        Table *derivTbl = nullptr;
        MultiLayer *derivMl = nullptr;
        const QList<MyWidget *> winsAfter = d_app->windowsList();
        for (int i = std::max(0, winBefore); i < winsAfter.size(); ++i) {
            MyWidget *w = winsAfter[i];
            if (!w)
                continue;
            if (w->inherits("Table"))
                derivTbl = static_cast<Table *>(w);
            else if (w->inherits("MultiLayer"))
                derivMl = static_cast<MultiLayer *>(w);
        }
        QString outCurve;
        if (derivMl) {
            for (Graph *lg : derivMl->layersList())
                enforceArialGraphFonts(lg);
            if (Graph *dg = derivMl->activeGraph()) {
                if (dg->curves() > 0 && dg->curve(dg->curves() - 1))
                    outCurve = dg->curve(dg->curves() - 1)->title().text();
            }
        }
        const bool okOut = ok && (derivMl != nullptr || derivTbl != nullptr);
        outResult[QStringLiteral("ok")] = okOut;
        if (!okOut) {
            // Say why (e.g. two adjacent points with the same X) instead of a bare ok: false.
            outResult[QStringLiteral("message")] = diff.errorMessage().isEmpty()
                    ? QStringLiteral("Differentiation of '%1' produced no result.").arg(curveTitle)
                    : diff.errorMessage();
        }
        if (!diff.xHandlingNotice().isEmpty())
            outResult[QStringLiteral("notice")] = diff.xHandlingNotice();
        if (derivMl)
            outResult[QStringLiteral("graph")] = derivMl->name();
        if (derivTbl)
            outResult[QStringLiteral("table")] = derivTbl->name();
        if (!outCurve.isEmpty())
            outResult[QStringLiteral("output_curve")] = outCurve;
        return true;
    }

    if (method == QStringLiteral("curvemath.interpolate")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        const QString mStr = (params.contains(QStringLiteral("method"))
                                      ? params.value(QStringLiteral("method")).toString()
                                      : params.value(QStringLiteral("kind")).toString())
                                     .trimmed()
                                     .toLower();
        int im = Interpolation::Linear;
        int minPts = 2;
        if (mStr.isEmpty() || mStr == QStringLiteral("linear")) {
            im = Interpolation::Linear;
            minPts = 2;
        } else if (mStr == QStringLiteral("cubic") || mStr == QStringLiteral("spline")) {
            im = Interpolation::Cubic;
            minPts = 4;
        } else if (mStr == QStringLiteral("akima")) {
            im = Interpolation::Akima;
            minPts = 5;
        } else {
            throwInvalidParams(
                    QStringLiteral("Unsupported interpolation method '%1' (expected 'linear', 'cubic', or 'akima').")
                            .arg(mStr));
        }
        int distinctX = 0;
        requireCurveMinPoints(g, curveTitle, minPts, QStringLiteral("curvemath.interpolate"),
                              &distinctX);
        if (distinctX < minPts) {
            throwInvalidParams(
                    QStringLiteral("curvemath.interpolate ('%1') requires >= %2 distinct X values (got %3).")
                            .arg(mStr.isEmpty() ? QStringLiteral("linear") : mStr)
                            .arg(minPts)
                            .arg(distinctX));
        }
        double dataXMin = 0.0;
        double dataXMax = 0.0;
        std::vector<double> xs;
        {
            std::vector<double> ys;
            extractValidUnmaskedCurveXY(g->curve(g->curveIndex(curveTitle)), xs, ys);
            if (static_cast<int>(xs.size()) < minPts) {
                throwInvalidParams(
                        QStringLiteral("curvemath.interpolate requires >= %1 valid unmasked points.")
                                .arg(minPts));
            }
            const auto mm = std::minmax_element(xs.cbegin(), xs.cend());
            dataXMin = *mm.first;
            dataXMax = *mm.second;
        }
        const int outPts = parseBoundedInt(params, QStringLiteral("points"), 200, 2, 1000000);
        const QJsonValue xMinVal = params.value(QStringLiteral("x_min"));
        const QJsonValue xMaxVal = params.value(QStringLiteral("x_max"));
        const bool hasXMin = !xMinVal.isUndefined() && !xMinVal.isNull();
        const bool hasXMax = !xMaxVal.isUndefined() && !xMaxVal.isNull();
        const bool hasXRange = hasXMin || hasXMax;
        // A side that is not given is the end of the data.
        double interpXMin = dataXMin;
        double interpXMax = dataXMax;
        if (hasXRange) {
            if (hasXMin)
                interpXMin = parseFiniteCoordDouble(xMinVal, QStringLiteral("x_min"));
            if (hasXMax)
                interpXMax = parseFiniteCoordDouble(xMaxVal, QStringLiteral("x_max"));
            validateFiniteCoordRange(interpXMin, interpXMax,
                                     QStringLiteral("curvemath.interpolate [x_min, x_max]"));
            if (interpXMin >= interpXMax)
                throwInvalidParams(
                        QStringLiteral("curvemath.interpolate requires x_min < x_max (x_min = %1, "
                                       "x_max = %2; a side that is not given is the end of the "
                                       "data).")
                                .arg(interpXMin, 0, 'g', 10)
                                .arg(interpXMax, 0, 'g', 10));
            // The interpolant is not extrapolated: a range without any overlap with the data
            // has nothing to return.
            if (!(std::max(interpXMin, dataXMin) < std::min(interpXMax, dataXMax))) {
                throwInvalidParams(
                        QStringLiteral("curvemath.interpolate: the range [%1, %2] does not overlap "
                                       "the X range of the data [%3, %4] (no extrapolation).")
                                .arg(interpXMin, 0, 'g', 10)
                                .arg(interpXMax, 0, 'g', 10)
                                .arg(dataXMin, 0, 'g', 10)
                                .arg(dataXMax, 0, 'g', 10));
            }
        }
        // The X must be strictly monotonic without a repeated value over the points the
        // interpolant is built from: the points in the range and one point on each side of it
        // (Interpolation::setDataCurve takes only those when the curve repeats an X or has its
        // X out of order elsewhere; a neighbour whose X is repeated is left out). Without a
        // range, that is the whole curve.
        {
            const double from = std::max(interpXMin, dataXMin);
            const double to = std::min(interpXMax, dataXMax);
            double below = -std::numeric_limits<double>::infinity();
            double above = std::numeric_limits<double>::infinity();
            for (double xv : xs) {
                if (xv < from)
                    below = std::max(below, xv);
                if (xv > to)
                    above = std::min(above, xv);
            }
            const auto count = [&](double v) { return std::count(xs.cbegin(), xs.cend(), v); };
            const double winLo = (std::isfinite(below) && count(below) == 1) ? below : from;
            const double winHi = (std::isfinite(above) && count(above) == 1) ? above : to;
            bool inc = true, dec = true, first = true;
            double prev = 0.0;
            for (double xv : xs) {
                if (xv < winLo || xv > winHi)
                    continue;
                if (!first) {
                    const double dx = xv - prev;
                    if (!(dx > 1e-15))
                        inc = false;
                    if (!(dx < -1e-15))
                        dec = false;
                }
                first = false;
                prev = xv;
            }
            if (!inc && !dec) {
                throwInvalidParams(QStringLiteral(
                        "curvemath.interpolate requires strictly monotonic X values without "
                        "duplicates (over the range and one point on each side of it)."));
            }
        }
        const int curvesBefore = g->curves();
        std::unique_ptr<RpcInterpolation> interp = hasXRange
                ? std::make_unique<RpcInterpolation>(d_app, g, curveTitle, interpXMin, interpXMax,
                                                     im)
                : std::make_unique<RpcInterpolation>(d_app, g, curveTitle, im);
        if (interp->error()) {
            throwInvalidParams(
                    QStringLiteral("curvemath.interpolate could not take the data of curve '%1' "
                                   "(at least %2 valid points with distinct X are needed).")
                            .arg(curveTitle)
                            .arg(minPts));
        }
        interp->setOutputPoints(outPts);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Interpolate Curve"));
        const bool ok = interp->run() && (g->curves() > curvesBefore);
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        if (ok)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = ok;
        if (!ok) {
            outResult[QStringLiteral("message")] =
                    QStringLiteral("Interpolation of '%1' produced no result.").arg(curveTitle);
        }
        if (hasXRange && interp->rangeClipped()) {
            const QString clipped =
                    QStringLiteral("The range [%1, %2] reaches past the X range of the data "
                                   "[%3, %4]; the output covers [%5, %6] only (no extrapolation).")
                            .arg(interpXMin, 0, 'g', 10)
                            .arg(interpXMax, 0, 'g', 10)
                            .arg(dataXMin, 0, 'g', 10)
                            .arg(dataXMax, 0, 'g', 10)
                            .arg(interp->outputFrom(), 0, 'g', 10)
                            .arg(interp->outputTo(), 0, 'g', 10);
            outResult[QStringLiteral("warnings")] = QJsonArray{ clipped };
        }
        outResult[QStringLiteral("graph")] = ml->name();
        if (g->curves() > curvesBefore && g->curve(g->curves() - 1))
            outResult[QStringLiteral("output_curve")] = g->curve(g->curves() - 1)->title().text();
        return true;
    }

    if (method == QStringLiteral("curvemath.subtract_line")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        if (!params.contains(QStringLiteral("x1")) || !params.contains(QStringLiteral("y1"))
            || !params.contains(QStringLiteral("x2")) || !params.contains(QStringLiteral("y2"))) {
            throwInvalidParams(QStringLiteral(
                    "curvemath.subtract_line requires finite numeric 'x1', 'y1', 'x2', and 'y2'."));
        }
        const double x1 = parseFiniteCoordDouble(params.value(QStringLiteral("x1")),
                                                 QStringLiteral("x1"));
        const double y1 = parseFiniteCoordDouble(params.value(QStringLiteral("y1")),
                                                 QStringLiteral("y1"));
        const double x2 = parseFiniteCoordDouble(params.value(QStringLiteral("x2")),
                                                 QStringLiteral("x2"));
        const double y2 = parseFiniteCoordDouble(params.value(QStringLiteral("y2")),
                                                 QStringLiteral("y2"));
        validateFiniteCoordRange(x1, x2, QStringLiteral("curvemath.subtract_line [x1, x2]"));
        if (!std::isfinite(y2 - y1))
            throwInvalidParams(QStringLiteral("curvemath.subtract_line y2 - y1 overflows."));
        const QPointF p1(x1, y1);
        const QPointF p2(x2, y2);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Subtract Straight Line"));
        QwtPlotCurve *outC = CurveMathTools::subtractStraightLine(
                d_app, g, curveTitle, p1, p2,
                optionalJsonBool(params, QStringLiteral("in_place"), false));
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    if (method == QStringLiteral("curvemath.intersections")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString cA = resolveCurveTitle(g, params, QStringLiteral("curve_a"), true);
        const QString cB = resolveCurveTitle(g, params, QStringLiteral("curve_b"), true);
        Table *outTbl = nullptr;
        const QVector<QPointF> pts = CurveMathTools::findCurveIntersections(
                g, cA, cB, params.value(QStringLiteral("mark")).toBool(true), d_app, &outTbl);
        QJsonArray ptsArr;
        for (const QPointF &pt : pts) {
            QJsonObject po;
            po[QStringLiteral("x")] = safeJsonDouble(pt.x());
            po[QStringLiteral("y")] = safeJsonDouble(pt.y());
            ptsArr.append(po);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("count")] = ptsArr.size();
        outResult[QStringLiteral("points")] = ptsArr;
        if (outTbl)
            outResult[QStringLiteral("table")] = outTbl->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.lookup_xy")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        if (params.contains(QStringLiteral("x")) && !params.value(QStringLiteral("x")).isNull()) {
            const double xq = params.value(QStringLiteral("x")).toDouble();
            // X that goes out and back (loading/unloading, hysteresis, CV): sorting by X and
            // averaging equal X mixed the branches and returned a value on neither of them.
            // Each branch's value is returned instead, in row order, as findXFromY does for Y;
            // 'y' holds a value only when one branch reaches x. Many changes of direction
            // (more than one per 4 points, as for differentiate) are rows in no particular
            // order: those are sorted by X as before, with a notice.
            // A step back of X counts as a turn only when X travels back from its furthest point
            // by more than 8 times its noise (robust spread of the second difference of X): an
            // almost monotonic X with noise (x = 0.01 i + N(0, 0.005), 1000 points) steps back
            // ~170 times, was taken for branches, and ~2 % of x gave several values and 'y'
            // null. Such a curve is one branch, sorted. The threshold does not depend on the X
            // range: a floor of 1 % of the range took a short real return for noise and sorted
            // the branches into one, e.g. a partial elastic unloading of 0.075 % strain in a
            // 0-10 % tensile test, which gave a value on neither branch. It is at least 2.5 times
            // the resolution of X (the smallest step between rows, ignoring floating-point
            // differences of equal readings): a quantized X (coarse extensometer, ADC) holds each
            // level for several rows, so most second differences are 0 and so is the noise, and
            // each 1-LSB flicker back was a turn (x on those levels gave several values). A tiny
            // floor of 1e-9 of the X range covers rounding when neither applies.
            int nPts = 0;
            const int nTurns = CurveMathTools::countXTurns(g, curveTitle, &nPts);
            int nBranchTurns = nTurns;
            double xNoise = 0.0;
            double xLsb = 0.0;
            double xThr = 0.0;
            if (nTurns > 0 && 4 * nTurns <= nPts) {
                std::vector<double> xr, yr;
                extractValidUnmaskedCurveXY(g->curve(g->curveIndex(curveTitle)), xr, yr);
                if (xr.size() >= 3) {
                    std::vector<double> d2;
                    d2.reserve(xr.size());
                    for (size_t i = 1; i + 1 < xr.size(); ++i)
                        d2.push_back(xr[i] - 0.5 * (xr[i - 1] + xr[i + 1]));
                    auto median = [](std::vector<double> v) {
                        auto mid = v.begin() + v.size() / 2;
                        std::nth_element(v.begin(), mid, v.end());
                        return *mid;
                    };
                    const double med = median(d2);
                    for (double &v : d2)
                        v = std::fabs(v - med);
                    xNoise = 1.4826 * median(d2) / std::sqrt(1.5);
                    const auto mm = std::minmax_element(xr.begin(), xr.end());
                    const double span = *mm.second - *mm.first;
                    const double scale = std::max(std::fabs(*mm.first), std::fabs(*mm.second));
                    // Steps below this are floating-point differences of one reading (e.g.
                    // 0.1 + 0.2 vs 0.3, ~1e-17), not the resolution of X.
                    const double tinyDx = std::max(1e-9 * span, 1e-12 * scale);
                    for (size_t i = 1; i < xr.size(); ++i) {
                        const double dx = std::fabs(xr[i] - xr[i - 1]);
                        if (dx > tinyDx && (xLsb == 0.0 || dx < xLsb))
                            xLsb = dx;
                    }
                    const double thr = std::max({ 8.0 * xNoise, 2.5 * xLsb, 1e-9 * span });
                    xThr = thr;
                    // Direction 0 until X has left the start by more than thr; then the furthest
                    // X in the current direction, and a turn when X is thr back from it.
                    int dir = 0;
                    double ext = xr.front();
                    nBranchTurns = 0;
                    for (size_t i = 1; i < xr.size(); ++i) {
                        const double v = xr[i];
                        if (dir == 0) {
                            if (std::fabs(v - xr.front()) > thr) {
                                dir = (v > xr.front()) ? 1 : -1;
                                ext = v;
                            }
                        } else if (dir * (v - ext) >= 0.0) {
                            ext = v;
                        } else if (std::fabs(v - ext) > thr) {
                            ++nBranchTurns;
                            dir = -dir;
                            ext = v;
                        }
                    }
                }
            }
            if (nBranchTurns > 0 && 4 * nTurns <= nPts) {
                const QVector<double> ysAt = CurveMathTools::findYValuesFromX(g, curveTitle, xq);
                outResult[QStringLiteral("ok")] = !ysAt.isEmpty();
                outResult[QStringLiteral("y")] =
                        (ysAt.size() == 1) ? safeJsonDouble(ysAt.first()) : QJsonValue::Null;
                outResult[QStringLiteral("y_values")] = toJsonArray(ysAt);
                outResult[QStringLiteral("notice")] =
                        QStringLiteral("X turns back %1 time(s); the curve has %2 value(s) at x = %3, "
                                       "one per branch in row order (y_values)%4.")
                                .arg(nBranchTurns)
                                .arg(ysAt.size())
                                .arg(xq, 0, 'g', 10)
                                .arg(ysAt.size() > 1 ? QStringLiteral("; 'y' is null because "
                                                                      "the value is not unique")
                                                     : QString());
                return true;
            }
            bool ok = false;
            const double yVal = CurveMathTools::findYFromX(g, curveTitle, xq, &ok);
            outResult[QStringLiteral("ok")] = ok;
            outResult[QStringLiteral("y")] = ok ? safeJsonDouble(yVal) : QJsonValue::Null;
            if (nTurns > 0 && 4 * nTurns <= nPts) {
                outResult[QStringLiteral("notice")] =
                        QStringLiteral("X steps back %1 time(s) in %2 points, each time by no more "
                                       "than %3 (the larger of 8 times its noise, %4, and 2.5 "
                                       "times its resolution, %5); the curve was taken as one "
                                       "branch and the points were sorted by X (equal X averaged) "
                                       "before interpolating.")
                                .arg(nTurns)
                                .arg(nPts)
                                .arg(xThr, 0, 'g', 4)
                                .arg(8.0 * xNoise, 0, 'g', 4)
                                .arg(2.5 * xLsb, 0, 'g', 4);
            } else if (nTurns > 0) {
                outResult[QStringLiteral("notice")] =
                        QStringLiteral("X is not in order (%1 changes of direction in %2 points); "
                                       "the points were sorted by X (equal X averaged) before "
                                       "interpolating.")
                                .arg(nTurns)
                                .arg(nPts);
            }
            return true;
        }
        if (params.contains(QStringLiteral("y")) && !params.value(QStringLiteral("y")).isNull()) {
            const QVector<double> xs = CurveMathTools::findXFromY(
                    g, curveTitle, params.value(QStringLiteral("y")).toDouble());
            outResult[QStringLiteral("ok")] = !xs.isEmpty();
            outResult[QStringLiteral("x_values")] = toJsonArray(xs);
            return true;
        }
        throwInvalidParams(QStringLiteral("Provide either 'x' or 'y' for curvemath.lookup_xy."));
    }

    if (method == QStringLiteral("curvemath.polygon_area")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        const double area = CurveMathTools::polygonArea(
                g, curveTitle, params.value(QStringLiteral("signed")).toBool(false));
        const double arc = CurveMathTools::curveArcLength(g, curveTitle);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("area")] = safeJsonDouble(area);
        outResult[QStringLiteral("arc_length")] = safeJsonDouble(arc);
        return true;
    }

    if (method == QStringLiteral("curvemath.fill_between")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        QString cA, cB;
        const bool hasA = params.contains(QStringLiteral("curve_a"))
                && !params.value(QStringLiteral("curve_a")).isNull();
        const bool hasB = params.contains(QStringLiteral("curve_b"))
                && !params.value(QStringLiteral("curve_b")).isNull();
        bool pickedDefaults = false;
        if (!hasA && !hasB) {
            QStringList defaultCurves;
            for (int ci = 0; ci < g->curves() && defaultCurves.size() < 2; ++ci) {
                if (QwtPlotCurve *pc = g->curve(ci)) {
                    if (g->curveType(ci) != Graph::ErrorBars)
                        defaultCurves.append(pc->title().text());
                }
            }
            if (defaultCurves.size() >= 2) {
                cA = defaultCurves[0];
                cB = defaultCurves[1];
                pickedDefaults = true;
            }
        }
        if (!pickedDefaults) {
            cA = resolveCurveTitle(g, params, QStringLiteral("curve_a"), true);
            cB = resolveCurveTitle(g, params, QStringLiteral("curve_b"), true);
        }
        QColor fillCol = parseColorValue(params.value(QStringLiteral("color")),
                                         QColor(30, 102, 184, 55));
        if (params.contains(QStringLiteral("alpha")) && !params.value(QStringLiteral("alpha")).isNull()) {
            const double rawA = optionalJsonFiniteDouble(params, QStringLiteral("alpha"), 55.0, 255.0);
            const int a255 = (rawA >= 0.0 && rawA <= 1.0) ? int(std::round(rawA * 255.0))
                                                          : std::clamp(int(std::round(rawA)), 0, 255);
            fillCol.setAlpha(a255);
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Fill Between Curves"));
        QwtPlotCurve *fc = CurveMathTools::fillBetweenCurves(d_app, g, cA, cB, fillCol);
        replotGraphOrDefer(g);
        if (fc)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (fc != nullptr);
        if (!fc) {
            // Say why nothing was filled (an empty curve used to give a bare ok false).
            struct Pts
            {
                int distinctX = 0;
                double lo = 0.0, hi = 0.0;
            };
            auto validPoints = [g](const QString &title) {
                Pts p;
                const int idx = g->curveIndex(title);
                QwtPlotCurve *c = idx >= 0 ? g->curve(idx) : nullptr;
                if (!c)
                    return p;
                countValidCurvePoints(c, &p.distinctX);
                auto *dc = dynamic_cast<DataCurve *>(c);
                const bool masks = dc && dc->hasAnyMaskedPoints();
                Column *mx = nullptr;
                Column *my = nullptr;
                if (masks)
                    dc->resolveColumns(mx, my);
                bool first = true;
                for (int i = 0; i < c->dataSize(); ++i) {
                    if (masks && dc->isPointMaskedFast(i, mx, my))
                        continue;
                    const double xv = c->x(i);
                    if (!std::isfinite(xv) || !std::isfinite(c->y(i)))
                        continue;
                    p.lo = first ? xv : std::min(p.lo, xv);
                    p.hi = first ? xv : std::max(p.hi, xv);
                    first = false;
                }
                return p;
            };
            const Pts pA = validPoints(cA);
            const Pts pB = validPoints(cB);
            QString why;
            if (pA.distinctX < 2 || pB.distinctX < 2) {
                const bool aShort = pA.distinctX < 2;
                why = QStringLiteral("curvemath.fill_between requires at least 2 valid data points "
                                     "at different x on each curve (curve '%1' has %2).")
                              .arg(aShort ? cA : cB)
                              .arg(aShort ? pA.distinctX : pB.distinctX);
            } else if (!(std::min(pA.hi, pB.hi) > std::max(pA.lo, pB.lo))) {
                why = QStringLiteral("curvemath.fill_between: the x ranges of curves '%1' [%2, %3] "
                                     "and '%4' [%5, %6] do not overlap, so there is no area "
                                     "between them.")
                              .arg(cA)
                              .arg(pA.lo)
                              .arg(pA.hi)
                              .arg(cB)
                              .arg(pB.lo)
                              .arg(pB.hi);
            } else {
                why = QStringLiteral("curvemath.fill_between: no area could be filled between "
                                     "curves '%1' and '%2'.")
                              .arg(cA, cB);
            }
            outResult[QStringLiteral("error")] = why;
        }
        return true;
    }

    if (method == QStringLiteral("curvemath.offset_yield")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("curvemath.offset_yield"));

        double offsetFraction = 0.002;
        if (params.contains(QStringLiteral("offset_percent"))
            && !params.value(QStringLiteral("offset_percent")).isNull()) {
            offsetFraction = params.value(QStringLiteral("offset_percent")).toDouble(0.2) / 100.0;
        } else if (params.contains(QStringLiteral("offset"))
                   && !params.value(QStringLiteral("offset")).isNull()) {
            offsetFraction = params.value(QStringLiteral("offset")).toDouble(0.002);
        }
        if (!std::isfinite(offsetFraction) || offsetFraction <= 0.0 || offsetFraction >= 0.5) {
            throwInvalidParams(
                    QStringLiteral("Parameter 'offset' must be a finite strain fraction in (0, 0.5), e.g. 0.002 for 0.2% proof stress."));
        }

        int strainMode = -1; // auto
        if (params.contains(QStringLiteral("strain_in_percent"))
            && !params.value(QStringLiteral("strain_in_percent")).isNull()) {
            strainMode = params.value(QStringLiteral("strain_in_percent")).toBool(true) ? 1 : 0;
        }

        const double qNaN = std::numeric_limits<double>::quiet_NaN();
        double elasticMin = qNaN;
        double elasticMax = qNaN;
        if (params.contains(QStringLiteral("elastic_min"))
            && !params.value(QStringLiteral("elastic_min")).isNull()) {
            elasticMin = params.value(QStringLiteral("elastic_min")).toDouble(qNaN);
        } else if (params.contains(QStringLiteral("elastic_x_min"))
                   && !params.value(QStringLiteral("elastic_x_min")).isNull()) {
            elasticMin = params.value(QStringLiteral("elastic_x_min")).toDouble(qNaN);
        }
        if (params.contains(QStringLiteral("elastic_max"))
            && !params.value(QStringLiteral("elastic_max")).isNull()) {
            elasticMax = params.value(QStringLiteral("elastic_max")).toDouble(qNaN);
        } else if (params.contains(QStringLiteral("elastic_x_max"))
                   && !params.value(QStringLiteral("elastic_x_max")).isNull()) {
            elasticMax = params.value(QStringLiteral("elastic_x_max")).toDouble(qNaN);
        }
        if (std::isfinite(elasticMin) && std::isfinite(elasticMax) && elasticMin >= elasticMax) {
            throwInvalidParams(
                    QStringLiteral("curvemath.offset_yield requires elastic_min < elastic_max."));
        }

        const bool plotConstruction = params.contains(QStringLiteral("plot_construction"))
                ? params.value(QStringLiteral("plot_construction")).toBool(true)
                : true;
        const bool addCallout = params.contains(QStringLiteral("add_callout"))
                ? params.value(QStringLiteral("add_callout")).toBool(true)
                : true;
        const bool createTrueTable =
                params.value(QStringLiteral("create_true_stress_table")).toBool(false);

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Tensile 0.2% Proof Stress"));
        const CurveMathTools::TensileResult tr = CurveMathTools::analyzeTensile(
                d_app, g, curveTitle, offsetFraction, strainMode, elasticMin, elasticMax,
                plotConstruction, addCallout, createTrueTable);
        if (!tr.valid) {
            throwInvalidParams(tr.error.isEmpty()
                                       ? QStringLiteral("Tensile offset yield analysis failed on '%1'.")
                                                 .arg(curveTitle)
                                       : tr.error);
        }
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();

        QString sUnit = tr.stressUnit.trimmed();
        if (sUnit.isEmpty()) {
            const int cIdx = g->curveIndex(curveTitle);
            if (cIdx >= 0) {
                if (auto *dcTen = dynamic_cast<DataCurve *>(g->curve(cIdx))) {
                    if (Table *srcTbl = dcTen->table()) {
                        const int yColIdx = srcTbl->colIndex(dcTen->yColumnName());
                        if (yColIdx >= 0)
                            sUnit = srcTbl->colUnits(yColIdx).trimmed();
                    }
                }
            }
        }
        double eToGpaFactor = 1.0 / 1000.0; // default MPa -> GPa
        if (sUnit.compare(QStringLiteral("GPa"), Qt::CaseInsensitive) == 0
            || sUnit.contains(QStringLiteral("GPa"), Qt::CaseInsensitive)) {
            eToGpaFactor = 1.0;
        } else if (sUnit.compare(QStringLiteral("MPa"), Qt::CaseInsensitive) == 0
                   || sUnit.contains(QStringLiteral("MPa"), Qt::CaseInsensitive)
                   || sUnit.contains(QStringLiteral("N/mm"), Qt::CaseInsensitive)) {
            eToGpaFactor = 1.0 / 1000.0;
        } else if (sUnit.compare(QStringLiteral("kPa"), Qt::CaseInsensitive) == 0
                   || sUnit.contains(QStringLiteral("kPa"), Qt::CaseInsensitive)) {
            eToGpaFactor = 1.0e-6;
        } else if (sUnit.compare(QStringLiteral("TPa"), Qt::CaseInsensitive) == 0
                   || sUnit.contains(QStringLiteral("TPa"), Qt::CaseInsensitive)) {
            eToGpaFactor = 1000.0;
        } else if (sUnit.compare(QStringLiteral("Pa"), Qt::CaseInsensitive) == 0
                   || sUnit.compare(QStringLiteral("N/m^2"), Qt::CaseInsensitive) == 0
                   || sUnit.compare(QStringLiteral("N/m2"), Qt::CaseInsensitive) == 0) {
            eToGpaFactor = 1.0e-9;
        }

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("curve")] = curveTitle;
        outResult[QStringLiteral("stress_unit")] =
                sUnit.isEmpty() ? QStringLiteral("MPa") : sUnit;
        outResult[QStringLiteral("youngs_modulus")] = safeJsonDouble(tr.youngsModulus);
        outResult[QStringLiteral("youngs_modulus_gpa")] =
                safeJsonDouble(tr.youngsModulus * eToGpaFactor);
        outResult[QStringLiteral("elastic_slope")] = safeJsonDouble(tr.elasticSlopeRaw);
        outResult[QStringLiteral("elastic_intercept")] = safeJsonDouble(tr.elasticIntercept);
        outResult[QStringLiteral("r2_elastic")] = safeJsonDouble(tr.elasticR2);
        outResult[QStringLiteral("elastic_r2")] = safeJsonDouble(tr.elasticR2);
        outResult[QStringLiteral("elastic_x_min")] = safeJsonDouble(tr.elasticXMin);
        outResult[QStringLiteral("elastic_x_max")] = safeJsonDouble(tr.elasticXMax);
        outResult[QStringLiteral("offset")] = safeJsonDouble(tr.offsetFraction);
        outResult[QStringLiteral("strain_in_percent")] = tr.strainInPercent;
        outResult[QStringLiteral("offset_yield_x")] = safeJsonDouble(tr.offsetYieldX);
        outResult[QStringLiteral("offset_yield_y")] = safeJsonDouble(tr.offsetYieldY);
        outResult[QStringLiteral("offset_yield_strain")] = safeJsonDouble(tr.offsetYieldX);
        outResult[QStringLiteral("offset_yield_stress")] = safeJsonDouble(tr.offsetYieldY);
        outResult[QStringLiteral("proof_stress")] = safeJsonDouble(tr.offsetYieldY);
        outResult[QStringLiteral("uts_x")] = safeJsonDouble(tr.utsX);
        outResult[QStringLiteral("uts_y")] = safeJsonDouble(tr.utsY);
        outResult[QStringLiteral("uts_strain")] = safeJsonDouble(tr.utsX);
        outResult[QStringLiteral("uts")] = safeJsonDouble(tr.utsY);
        outResult[QStringLiteral("fracture_x")] = safeJsonDouble(tr.fractureX);
        outResult[QStringLiteral("fracture_y")] = safeJsonDouble(tr.fractureY);
        outResult[QStringLiteral("fracture_strain")] = safeJsonDouble(tr.fractureX);
        outResult[QStringLiteral("fracture_stress")] = safeJsonDouble(tr.fractureY);
        outResult[QStringLiteral("construction_curve")] = tr.constructionCurveTitle;
        if (tr.trueStressTable)
            outResult[QStringLiteral("true_stress_table")] = tr.trueStressTable->name();
        // F2/F3/F8/F10: unit decision, toe origin, elongations, exclusions and notes
        outResult[QStringLiteral("strain_unit_source")] = tr.strainUnitSource;
        outResult[QStringLiteral("strain_unit_ambiguous")] = tr.strainUnitAmbiguous;
        outResult[QStringLiteral("youngs_modulus_gpa_if_percent")] =
                safeJsonDouble(tr.youngsModulusIfPercentGPa);
        outResult[QStringLiteral("youngs_modulus_gpa_if_fraction")] =
                safeJsonDouble(tr.youngsModulusIfFractionGPa);
        outResult[QStringLiteral("toe_strain_x0")] = safeJsonDouble(tr.toeStrainX0);
        outResult[QStringLiteral("uniform_elongation_pct")] = safeJsonDouble(tr.uniformElongationPct);
        outResult[QStringLiteral("uniform_plastic_elongation_pct")] =
                safeJsonDouble(tr.uniformPlasticElongationPct);
        outResult[QStringLiteral("fracture_elongation_pct")] = safeJsonDouble(tr.fractureElongationPct);
        outResult[QStringLiteral("fracture_plastic_elongation_pct")] =
                safeJsonDouble(tr.fracturePlasticElongationPct);
        outResult[QStringLiteral("elastic_auto_relaxed")] = tr.elasticAutoRelaxed;
        outResult[QStringLiteral("excluded_masked_points")] = tr.excludedMaskedPoints;
        outResult[QStringLiteral("excluded_nonfinite_points")] = tr.excludedNonFinitePoints;
        outResult[QStringLiteral("warnings")] = QJsonArray::fromStringList(tr.warnings);
        return true;
    }

    if (method == QStringLiteral("curvemath.arrhenius")) {
        Table *t = resolveTable(d_app, params);
        const QString tempCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("temp_col"))
                                           ? params.value(QStringLiteral("temp_col"))
                                           : QJsonValue(0),
                                   true));
        const QString rateCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("rate_col"))
                                           ? params.value(QStringLiteral("rate_col"))
                                           : QJsonValue(1),
                                   true));
        const bool celsius = params.contains(QStringLiteral("temp_in_celsius"))
                ? optionalJsonBool(params, QStringLiteral("temp_in_celsius"), false)
                : optionalJsonBool(params, QStringLiteral("celsius"), false);
        const bool use1000 = params.contains(QStringLiteral("use_1000_over_t"))
                ? optionalJsonBool(params, QStringLiteral("use_1000_over_t"), true)
                : true;
        const bool sigmaT = optionalJsonBool(params, QStringLiteral("sigma_times_t"), false);
        const auto ar =
                CurveMathTools::createArrheniusPlot(d_app, t, tempCol, rateCol, celsius, use1000, sigmaT);
        if (ar.plot) {
            for (Graph *lg : ar.plot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = ar.valid;
        outResult[QStringLiteral("ea_ev")] = safeJsonDouble(ar.activationEnergyEV);
        outResult[QStringLiteral("ea_ev_se")] = safeJsonDouble(ar.activationEnergyEVSE);
        outResult[QStringLiteral("ea_kj_mol")] = safeJsonDouble(ar.activationEnergyKJmol);
        outResult[QStringLiteral("ea_kj_mol_se")] = safeJsonDouble(ar.activationEnergyKJmolSE);
        outResult[QStringLiteral("ln_d0")] = safeJsonDouble(ar.lnPreExponential);
        outResult[QStringLiteral("d0")] = safeJsonDouble(std::exp(ar.lnPreExponential));
        outResult[QStringLiteral("r2")] = safeJsonDouble(ar.rSquare);
        if (ar.dataTable)
            outResult[QStringLiteral("table")] = ar.dataTable->name();
        if (ar.plot)
            outResult[QStringLiteral("graph")] = ar.plot->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.tauc")) {
        Table *t = resolveTable(d_app, params);
        const QString xCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("x_col"))
                                           ? params.value(QStringLiteral("x_col"))
                                           : QJsonValue(0),
                                   true));
        const QJsonValue alphaKey = params.contains(QStringLiteral("alpha_col"))
                ? params.value(QStringLiteral("alpha_col"))
                : (params.contains(QStringLiteral("y_col"))
                           ? params.value(QStringLiteral("y_col"))
                           : QJsonValue(1));
        const QString alphaCol = t->colLabel(resolveColumnIndex(t, alphaKey, true));
        const bool isWavelength = params.contains(QStringLiteral("x_is_wavelength_nm"))
                ? optionalJsonBool(params, QStringLiteral("x_is_wavelength_nm"), true)
                : (params.contains(QStringLiteral("wavelength_nm"))
                           ? optionalJsonBool(params, QStringLiteral("wavelength_nm"), true)
                           : !optionalJsonBool(params, QStringLiteral("energy_ev"), false));
        const bool direct = params.contains(QStringLiteral("direct"))
                ? optionalJsonBool(params, QStringLiteral("direct"), true)
                : true;
        const auto trRes = CurveMathTools::createTaucPlot(d_app, t, xCol, alphaCol, isWavelength, direct);
        if (trRes.plot) {
            for (Graph *lg : trRes.plot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = trRes.valid;
        outResult[QStringLiteral("eg_ev")] = safeJsonDouble(trRes.bandgapEV);
        outResult[QStringLiteral("eg_ev_se")] = safeJsonDouble(trRes.bandgapEVSE);
        outResult[QStringLiteral("edge_slope")] = safeJsonDouble(trRes.edgeSlope);
        outResult[QStringLiteral("r2")] = safeJsonDouble(trRes.rSquare);
        if (trRes.dataTable)
            outResult[QStringLiteral("table")] = trRes.dataTable->name();
        if (trRes.plot)
            outResult[QStringLiteral("graph")] = trRes.plot->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.eis_fit")) {
        Table *t = resolveTable(d_app, params);
        const QString fCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("freq_col"))
                                           ? params.value(QStringLiteral("freq_col"))
                                           : QJsonValue(0),
                                   true));
        const QJsonValue zrKey = params.contains(QStringLiteral("z_real_col"))
                ? params.value(QStringLiteral("z_real_col"))
                : (params.contains(QStringLiteral("z_re_col"))
                           ? params.value(QStringLiteral("z_re_col"))
                           : QJsonValue(1));
        const QJsonValue ziKey = params.contains(QStringLiteral("z_imag_col"))
                ? params.value(QStringLiteral("z_imag_col"))
                : (params.contains(QStringLiteral("z_im_col"))
                           ? params.value(QStringLiteral("z_im_col"))
                           : QJsonValue(2));
        const QString zrCol = t->colLabel(resolveColumnIndex(t, zrKey, true));
        const QString ziCol = t->colLabel(resolveColumnIndex(t, ziKey, true));
        const bool negImag = optionalJsonBool(params, QStringLiteral("z_imag_negated"), false);
        bool fitCircuit = true;
        bool idealCapacitorOnly = false;
        const QString rawModel = params.contains(QStringLiteral("model"))
                ? optionalJsonString(params, QStringLiteral("model")).trimmed()
                : optionalJsonString(params, QStringLiteral("circuit")).trimmed();
        if (!rawModel.isEmpty()) {
            QString mNorm = rawModel.toLower();
            mNorm.remove(QLatin1Char('_'));
            mNorm.remove(QLatin1Char('-'));
            mNorm.remove(QLatin1Char(' '));
            if (mNorm == QStringLiteral("randles") || mNorm == QStringLiteral("rc")) {
                idealCapacitorOnly = true;
                fitCircuit = true;
            } else if (mNorm == QStringLiteral("randlescpe")
                       || mNorm == QStringLiteral("randleswarburg")
                       || mNorm == QStringLiteral("cpe")) {
                idealCapacitorOnly = false;
                fitCircuit = true;
            } else {
                throwInvalidParams(
                        QStringLiteral("Unknown EIS circuit model '%1' (expected 'Randles' or 'Randles_CPE' / 'RandlesWarburg').")
                                .arg(rawModel));
            }
        } else if (params.contains(QStringLiteral("fit_cpe"))
                   && !optionalJsonBool(params, QStringLiteral("fit_cpe"), true)) {
            fitCircuit = false;
        }
        const auto eis = CurveMathToolsInternal::analyzeEISWithOptions(
                d_app, t, fCol, zrCol, ziCol, negImag, fitCircuit, idealCapacitorOnly);
        if (eis.nyquistPlot) {
            for (Graph *lg : eis.nyquistPlot->layersList())
                enforceArialGraphFonts(lg);
        }
        if (eis.bodePlot) {
            for (Graph *lg : eis.bodePlot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = eis.valid;
        outResult[QStringLiteral("rs")] = safeJsonDouble(eis.rs);
        outResult[QStringLiteral("rct")] = safeJsonDouble(eis.rct);
        outResult[QStringLiteral("q_cpe")] = safeJsonDouble(eis.qCpe);
        outResult[QStringLiteral("n_cpe")] = safeJsonDouble(eis.nCpe);
        outResult[QStringLiteral("c_eff")] = safeJsonDouble(eis.cEff);
        outResult[QStringLiteral("f_peak_hz")] = safeJsonDouble(eis.fPeakHz);
        outResult[QStringLiteral("rmse")] = safeJsonDouble(eis.rmse);
        if (eis.resultTable)
            outResult[QStringLiteral("table")] = eis.resultTable->name();
        if (eis.nyquistPlot)
            outResult[QStringLiteral("nyquist_graph")] = eis.nyquistPlot->name();
        if (eis.bodePlot)
            outResult[QStringLiteral("bode_graph")] = eis.bodePlot->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.kissinger")) {
        const bool hasTempMin = params.contains(QStringLiteral("temp_min"))
                && !params.value(QStringLiteral("temp_min")).isNull();
        const bool hasTempMax = params.contains(QStringLiteral("temp_max"))
                && !params.value(QStringLiteral("temp_max")).isNull();
        const double tempMinFilter = hasTempMin
                ? parseFiniteCoordDouble(params.value(QStringLiteral("temp_min")),
                                         QStringLiteral("temp_min"), 1e6)
                : -std::numeric_limits<double>::infinity();
        const double tempMaxFilter = hasTempMax
                ? parseFiniteCoordDouble(params.value(QStringLiteral("temp_max")),
                                         QStringLiteral("temp_max"), 1e6)
                : std::numeric_limits<double>::infinity();
        if (hasTempMin && hasTempMax && tempMinFilter >= tempMaxFilter) {
            throwInvalidParams(
                    QStringLiteral("curvemath.kissinger requires temp_min < temp_max."));
        }

        QVector<double> rates;
        QVector<double> temps;
        const QJsonValue rArrVal = params.contains(QStringLiteral("heating_rates"))
                ? params.value(QStringLiteral("heating_rates"))
                : params.value(QStringLiteral("rates"));
        const QJsonValue tArrVal = params.contains(QStringLiteral("peak_temps"))
                ? params.value(QStringLiteral("peak_temps"))
                : params.value(QStringLiteral("temps"));
        if (rArrVal.isArray() && tArrVal.isArray()) {
            const QJsonArray ra = rArrVal.toArray();
            const QJsonArray ta = tArrVal.toArray();
            if (ra.size() != ta.size()) {
                throwInvalidParams(QStringLiteral(
                        "curvemath.kissinger requires equal-length 'heating_rates' and 'peak_temps' arrays."));
            }
            for (int i = 0; i < ra.size(); ++i) {
                const double rv =
                        parseFiniteCoordDouble(ra[i], QStringLiteral("heating_rates"), 1e12);
                const double tv =
                        parseFiniteCoordDouble(ta[i], QStringLiteral("peak_temps"), 1e6);
                if (tv >= tempMinFilter && tv <= tempMaxFilter) {
                    rates.append(rv);
                    temps.append(tv);
                }
            }
        } else if (rArrVal.isArray() && !rArrVal.toArray().isEmpty()
                   && (rArrVal.toArray().first().isArray()
                       || rArrVal.toArray().first().isObject())) {
            const QJsonArray pairArr = rArrVal.toArray();
            for (int i = 0; i < pairArr.size(); ++i) {
                double rv = std::numeric_limits<double>::quiet_NaN();
                double tv = std::numeric_limits<double>::quiet_NaN();
                if (pairArr[i].isArray() && pairArr[i].toArray().size() >= 2) {
                    const QJsonArray pr = pairArr[i].toArray();
                    rv = parseFiniteCoordDouble(pr[0], QStringLiteral("heating_rates"), 1e12);
                    tv = parseFiniteCoordDouble(pr[1], QStringLiteral("peak_temps"), 1e6);
                } else if (pairArr[i].isObject()) {
                    const QJsonObject po = pairArr[i].toObject();
                    const QJsonValue rvJ = po.contains(QStringLiteral("rate"))
                            ? po.value(QStringLiteral("rate"))
                            : po.value(QStringLiteral("heating_rate"));
                    const QJsonValue tvJ = po.contains(QStringLiteral("temp"))
                            ? po.value(QStringLiteral("temp"))
                            : po.value(QStringLiteral("peak_temp"));
                    rv = parseFiniteCoordDouble(rvJ, QStringLiteral("rate"), 1e12);
                    tv = parseFiniteCoordDouble(tvJ, QStringLiteral("temp"), 1e6);
                }
                if (std::isfinite(rv) && std::isfinite(tv) && tv >= tempMinFilter
                    && tv <= tempMaxFilter) {
                    rates.append(rv);
                    temps.append(tv);
                }
            }
        } else {
            Table *t = resolveTable(d_app, params);
            const QJsonValue specColsVal = params.value(QStringLiteral("y_cols")).isArray()
                    ? params.value(QStringLiteral("y_cols"))
                    : (params.value(QStringLiteral("spectrum_cols")).isArray()
                               ? params.value(QStringLiteral("spectrum_cols"))
                               : (params.value(QStringLiteral("temp_col")).isArray()
                                          ? params.value(QStringLiteral("temp_col"))
                                          : QJsonValue()));
            if (rArrVal.isArray() && specColsVal.isArray()) {
                const QJsonArray ra = rArrVal.toArray();
                const QJsonArray ya = specColsVal.toArray();
                if (ra.size() != ya.size()) {
                    throwInvalidParams(QStringLiteral(
                            "curvemath.kissinger requires equal-length 'heating_rates' and spectrum 'y_cols'."));
                }
                const QJsonValue tAxisSpec =
                        (params.contains(QStringLiteral("temp_col"))
                         && !params.value(QStringLiteral("temp_col")).isNull()
                         && !params.value(QStringLiteral("temp_col")).isArray())
                        ? params.value(QStringLiteral("temp_col"))
                        : (params.contains(QStringLiteral("x_col"))
                                           && !params.value(QStringLiteral("x_col")).isNull()
                                   ? params.value(QStringLiteral("x_col"))
                                   : QJsonValue(0));
                const int tCol = resolveColumnIndex(t, tAxisSpec, true);
                Column *tc = t->column(tCol);
                // Peak temperature of a spectrum. The largest sample is the peak only without
                // noise: with 2 % noise at a 1 K step it wandered by up to 3.4 K and Ea came out
                // 91 kJ/mol instead of 100. With noise a parabola is fitted by least squares to
                // the samples around the maximum of a 5-point moving average near the largest
                // sample, down to 80 % of the peak height (lower, down to 50 %, when the noise is
                // larger than 4 % of the height), and its vertex is the peak temperature.
                // Noise-free spectra keep the largest sample as before.
                // Noise is told by zigzags (the slope changes sign at two neighbouring samples),
                // not by the number of changes of sign: the maxima and minima of a noise-free
                // spectrum with two peaks (3 changes in 50 samples) counted as noise, and the
                // moving average, which flattens a narrow peak, moved the result from the
                // narrow 430 K peak to a broad one at 470 K. A zigzag needs a peak or valley one
                // sample wide, which only noise makes. The moving-average maximum is also only
                // looked for within 7 samples of the largest sample.
                auto spectrumPeakT = [](std::vector<std::pair<double, double>> pts) -> double {
                    const int n = int(pts.size());
                    if (n == 0)
                        return std::numeric_limits<double>::quiet_NaN();
                    std::stable_sort(pts.begin(), pts.end(),
                                     [](const std::pair<double, double> &a,
                                        const std::pair<double, double> &b) {
                                         return a.first < b.first;
                                     });
                    int iMax = 0;
                    for (int k = 1; k < n; ++k) {
                        if (pts[size_t(k)].second > pts[size_t(iMax)].second)
                            iMax = k;
                    }
                    const double tArgmax = pts[size_t(iMax)].first;
                    if (n < 7)
                        return tArgmax;
                    int nZigzags = 0;
                    int prevSign = 0;
                    int prevTurnAt = -10;
                    for (int k = 1; k < n; ++k) {
                        const double dy = pts[size_t(k)].second - pts[size_t(k - 1)].second;
                        const int sign = (dy > 0.0) ? 1 : (dy < 0.0 ? -1 : 0);
                        if (sign == 0)
                            continue;
                        if (prevSign != 0 && sign != prevSign) {
                            // Extremum at sample k - 1; a zigzag when the previous one was at k - 2.
                            if (prevTurnAt == k - 2)
                                ++nZigzags;
                            prevTurnAt = k - 1;
                        }
                        prevSign = sign;
                    }
                    if (nZigzags < std::max(2.0, 0.02 * n))
                        return tArgmax;
                    // Noise: robust spread of the second-difference residual.
                    std::vector<double> res2;
                    res2.reserve(size_t(n));
                    for (int k = 1; k + 1 < n; ++k) {
                        res2.push_back(pts[size_t(k)].second
                                       - 0.5 * (pts[size_t(k - 1)].second + pts[size_t(k + 1)].second));
                    }
                    auto median = [](std::vector<double> v) {
                        auto mid = v.begin() + v.size() / 2;
                        std::nth_element(v.begin(), mid, v.end());
                        return *mid;
                    };
                    const double med = median(res2);
                    for (double &v : res2)
                        v = std::fabs(v - med);
                    const double sigma = 1.4826 * median(res2) / std::sqrt(1.5);
                    const int m = 2;
                    std::vector<double> sm(static_cast<size_t>(n), 0.0);
                    for (int k = 0; k < n; ++k) {
                        const int lo = std::max(0, k - m);
                        const int hi = std::min(n - 1, k + m);
                        double s = 0.0;
                        for (int j = lo; j <= hi; ++j)
                            s += pts[size_t(j)].second;
                        sm[size_t(k)] = s / double(hi - lo + 1);
                    }
                    const int wLo = std::max(0, iMax - 7);
                    const int wHi = std::min(n - 1, iMax + 7);
                    const int i0 = int(std::max_element(sm.begin() + wLo, sm.begin() + wHi + 1)
                                       - sm.begin());
                    const double base = *std::min_element(sm.begin(), sm.end());
                    const double amp = sm[size_t(i0)] - base;
                    if (!(amp > 0.0))
                        return tArgmax;
                    const double drop = std::clamp(5.0 * sigma / amp, 0.2, 0.5);
                    const double level = base + (1.0 - drop) * amp;
                    int lo = i0, hi = i0;
                    while (lo > 0 && sm[size_t(lo - 1)] >= level)
                        --lo;
                    while (hi < n - 1 && sm[size_t(hi + 1)] >= level)
                        ++hi;
                    lo = std::max(0, std::min(lo, i0 - 2));
                    hi = std::min(n - 1, std::max(hi, i0 + 2));
                    if (hi - lo + 1 < 5)
                        return tArgmax;
                    // y = a u^2 + b u + c with u = T - T(i0), least squares.
                    const double t0 = pts[size_t(i0)].first;
                    double s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, sy = 0, suy = 0, su2y = 0;
                    for (int k = lo; k <= hi; ++k) {
                        const double u = pts[size_t(k)].first - t0;
                        const double y = pts[size_t(k)].second;
                        const double u2 = u * u;
                        s0 += 1.0;
                        s1 += u;
                        s2 += u2;
                        s3 += u2 * u;
                        s4 += u2 * u2;
                        sy += y;
                        suy += u * y;
                        su2y += u2 * y;
                    }
                    // Cramer's rule on the normal equations [s4 s3 s2; s3 s2 s1; s2 s1 s0].
                    const double det = s4 * (s2 * s0 - s1 * s1) - s3 * (s3 * s0 - s1 * s2)
                            + s2 * (s3 * s1 - s2 * s2);
                    if (!(std::fabs(det) > 0.0))
                        return t0;
                    const double a = (su2y * (s2 * s0 - s1 * s1) - s3 * (suy * s0 - s1 * sy)
                                      + s2 * (suy * s1 - s2 * sy))
                            / det;
                    const double b = (s4 * (suy * s0 - s1 * sy) - su2y * (s3 * s0 - s1 * s2)
                                      + s2 * (s3 * sy - suy * s2))
                            / det;
                    if (!(a < 0.0))
                        return t0;
                    const double uv = -b / (2.0 * a);
                    const double uLo = pts[size_t(lo)].first - t0;
                    const double uHi = pts[size_t(hi)].first - t0;
                    return (uv >= uLo && uv <= uHi) ? t0 + uv : t0;
                };
                for (int i = 0; i < ra.size(); ++i) {
                    const int yIdx = resolveColumnIndex(t, ya[i], true);
                    Column *yc = t->column(yIdx);
                    std::vector<std::pair<double, double>> specPts;
                    for (int r = 0; r < t->numRows(); ++r) {
                        if (!tc || !yc || tc->isInvalid(r) || tc->isMasked(r) || yc->isInvalid(r)
                            || yc->isMasked(r))
                            continue;
                        const double tv = t->cell(r, tCol);
                        const double yv = t->cell(r, yIdx);
                        if (std::isfinite(tv) && std::isfinite(yv) && tv >= tempMinFilter
                            && tv <= tempMaxFilter) {
                            specPts.emplace_back(tv, yv);
                        }
                    }
                    const double bestT = spectrumPeakT(std::move(specPts));
                    if (std::isfinite(bestT)) {
                        rates.append(parseFiniteCoordDouble(ra[i], QStringLiteral("heating_rates"),
                                                            1e12));
                        temps.append(bestT);
                    }
                }
            } else {
                const int rCol = resolveColumnIndex(
                        t, params.contains(QStringLiteral("rate_col"))
                                   && !params.value(QStringLiteral("rate_col")).isNull()
                                ? params.value(QStringLiteral("rate_col"))
                                : QJsonValue(0),
                        true);
                const int tCol = resolveColumnIndex(
                        t, params.contains(QStringLiteral("temp_col"))
                                   && !params.value(QStringLiteral("temp_col")).isNull()
                                ? params.value(QStringLiteral("temp_col"))
                                : QJsonValue(rCol == 0 ? 1 : 0),
                        true);
                Column *rc = t->column(rCol);
                Column *tc = t->column(tCol);
                for (int r = 0; r < t->numRows(); ++r) {
                    if (!rc || !tc || rc->isInvalid(r) || rc->isMasked(r) || tc->isInvalid(r)
                        || tc->isMasked(r))
                        continue;
                    const double rv = t->cell(r, rCol);
                    const double tv = t->cell(r, tCol);
                    if (std::isfinite(rv) && std::isfinite(tv) && tv >= tempMinFilter
                        && tv <= tempMaxFilter) {
                        rates.append(rv);
                        temps.append(tv);
                    }
                }
            }
        }
        if (rates.size() < 3) {
            throwInvalidParams(QStringLiteral(
                    "curvemath.kissinger requires at least 3 valid (heating_rate, peak_temp) pairs."));
        }
        const bool rateKPerHour = params.contains(QStringLiteral("rate_in_k_per_hour"))
                ? optionalJsonBool(params, QStringLiteral("rate_in_k_per_hour"), false)
                : optionalJsonBool(params, QStringLiteral("k_per_hour"), false);
        const bool tempInCelsius = params.contains(QStringLiteral("temp_in_celsius"))
                ? optionalJsonBool(params, QStringLiteral("temp_in_celsius"), true)
                : optionalJsonBool(params, QStringLiteral("celsius"), true);
        const bool topAxisC = params.contains(QStringLiteral("top_axis_in_celsius"))
                ? optionalJsonBool(params, QStringLiteral("top_axis_in_celsius"), true)
                : optionalJsonBool(params, QStringLiteral("top_axis_celsius"), true);
        const bool wantPlot = optionalJsonBool(params, QStringLiteral("plot"), true);

        double minRate = std::numeric_limits<double>::infinity();
        double maxRate = -std::numeric_limits<double>::infinity();
        double minTempK = std::numeric_limits<double>::infinity();
        double maxTempK = -std::numeric_limits<double>::infinity();
        for (int i = 0; i < rates.size(); ++i) {
            const double rv = rates[i];
            const double tk = tempInCelsius ? (temps[i] + 273.15) : temps[i];
            if (!std::isfinite(rv) || rv <= 0.0) {
                throwInvalidParams(QStringLiteral(
                        "curvemath.kissinger requires strictly positive heating_rates (> 0)."));
            }
            if (!std::isfinite(tk) || tk <= 0.0) {
                throwInvalidParams(QStringLiteral(
                        "curvemath.kissinger requires strictly positive absolute peak temperatures (> 0 K)."));
            }
            minRate = std::min(minRate, rv);
            maxRate = std::max(maxRate, rv);
            minTempK = std::min(minTempK, tk);
            maxTempK = std::max(maxTempK, tk);
        }
        if (!(maxRate > minRate * (1.0 + 1e-9))) {
            throwInvalidParams(QStringLiteral(
                    "curvemath.kissinger requires at least 2 distinct positive heating rates."));
        }
        if (!(maxTempK > minTempK + 1e-9)) {
            throwInvalidParams(QStringLiteral(
                    "curvemath.kissinger requires distinct peak temperatures across heating rates."));
        }

        const CurveMathTools::KissingerResult kr = CurveMathTools::analyzeKissinger(
                d_app, rates, temps, rateKPerHour, tempInCelsius, topAxisC);
        if (!kr.valid) {
            throwInvalidParams(kr.error.isEmpty()
                                       ? QStringLiteral("Kissinger thermal activation analysis failed.")
                                       : kr.error);
        }
        MultiLayer *kPlot = kr.plot;
        if (kPlot && !wantPlot) {
            closeAndDeleteEmptyPlot(d_app, kPlot);
            kPlot = nullptr;
        } else if (kPlot) {
            for (Graph *lg : kPlot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("ea_kj_mol")] = safeJsonDouble(kr.activationEnergyKJmol);
        outResult[QStringLiteral("ea_kj_mol_se")] = safeJsonDouble(kr.activationEnergyKJmolSE);
        outResult[QStringLiteral("ea_ev")] = safeJsonDouble(kr.activationEnergyEV);
        outResult[QStringLiteral("ea_ev_se")] = safeJsonDouble(kr.activationEnergyEVSE);
        outResult[QStringLiteral("pre_exponential_s_inv")] =
                safeJsonDouble(kr.preExponentialSInv);
        outResult[QStringLiteral("slope")] = safeJsonDouble(kr.slope);
        outResult[QStringLiteral("intercept")] = safeJsonDouble(kr.intercept);
        outResult[QStringLiteral("r2")] = safeJsonDouble(kr.rSquare);
        outResult[QStringLiteral("heating_rates_k_min")] = toJsonArray(kr.heatingRatesKmin);
        outResult[QStringLiteral("peak_temps_k")] = toJsonArray(kr.peakTempsK);
        if (kr.resultTable)
            outResult[QStringLiteral("table")] = kr.resultTable->name();
        if (kPlot)
            outResult[QStringLiteral("graph")] = kPlot->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.kocks_mecking")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 6, QStringLiteral("curvemath.kocks_mecking"));
        const int smoothWin =
                parseBoundedInt(params, QStringLiteral("smooth_window"), 11, 3, 201);
        const bool createPlot = params.contains(QStringLiteral("create_plot"))
                ? optionalJsonBool(params, QStringLiteral("create_plot"), true)
                : optionalJsonBool(params, QStringLiteral("plot"), true);
        QString rawMode = optionalJsonString(params, QStringLiteral("plot_mode")).trimmed().toLower();
        rawMode.replace(QLatin1Char('-'), QLatin1Char('_'));
        bool kmPlot = false;
        QString canonicalMode = QStringLiteral("considere");
        if (rawMode.isEmpty()) {
            if (params.contains(QStringLiteral("kocks_mecking_plot"))
                && !params.value(QStringLiteral("kocks_mecking_plot")).isNull()) {
                kmPlot = optionalJsonBool(params, QStringLiteral("kocks_mecking_plot"), false);
                canonicalMode = kmPlot ? QStringLiteral("kocks_mecking")
                                       : QStringLiteral("considere");
            }
        } else if (rawMode == QStringLiteral("considere") || rawMode == QStringLiteral("strain")
                   || rawMode == QStringLiteral("theta_vs_strain")) {
            kmPlot = false;
            canonicalMode = QStringLiteral("considere");
        } else if (rawMode == QStringLiteral("kocks_mecking") || rawMode == QStringLiteral("km")
                   || rawMode == QStringLiteral("theta_vs_sigma")
                   || rawMode == QStringLiteral("stress")
                   || rawMode == QStringLiteral("stage_iii")) {
            kmPlot = true;
            canonicalMode = QStringLiteral("kocks_mecking");
        } else {
            throwInvalidParams(
                    QStringLiteral("Unknown curvemath.kocks_mecking plot_mode '%1' (expected 'considere' or 'kocks_mecking').")
                            .arg(rawMode));
        }
        // Tensile settings shared with curvemath.offset_yield (F4)
        int whStrainMode = -1;
        if (params.contains(QStringLiteral("strain_in_percent"))
            && !params.value(QStringLiteral("strain_in_percent")).isNull()) {
            whStrainMode = params.value(QStringLiteral("strain_in_percent")).toBool(true) ? 1 : 0;
        }
        double whOffset = 0.002;
        if (params.contains(QStringLiteral("offset_percent"))
            && !params.value(QStringLiteral("offset_percent")).isNull()) {
            whOffset = params.value(QStringLiteral("offset_percent")).toDouble(0.2) / 100.0;
        } else if (params.contains(QStringLiteral("offset"))
                   && !params.value(QStringLiteral("offset")).isNull()) {
            whOffset = params.value(QStringLiteral("offset")).toDouble(0.002);
        }
        if (!std::isfinite(whOffset) || whOffset <= 0.0 || whOffset >= 0.5) {
            throwInvalidParams(
                    QStringLiteral("Parameter 'offset' must be a finite strain fraction in (0, 0.5), e.g. 0.002 for 0.2% proof stress."));
        }
        const double whNaN = std::numeric_limits<double>::quiet_NaN();
        double whElMin = whNaN, whElMax = whNaN;
        if (params.contains(QStringLiteral("elastic_min"))
            && !params.value(QStringLiteral("elastic_min")).isNull())
            whElMin = params.value(QStringLiteral("elastic_min")).toDouble(whNaN);
        if (params.contains(QStringLiteral("elastic_max"))
            && !params.value(QStringLiteral("elastic_max")).isNull())
            whElMax = params.value(QStringLiteral("elastic_max")).toDouble(whNaN);
        if (std::isfinite(whElMin) && std::isfinite(whElMax) && whElMin >= whElMax) {
            throwInvalidParams(
                    QStringLiteral("curvemath.kocks_mecking requires elastic_min < elastic_max."));
        }
        const CurveMathTools::WorkHardeningResult wh = CurveMathTools::analyzeWorkHardening(
                d_app, g, curveTitle, smoothWin, createPlot, kmPlot, whStrainMode, whOffset,
                whElMin, whElMax);
        if (!wh.valid) {
            throwInvalidParams(wh.error.isEmpty()
                                       ? QStringLiteral("Kocks-Mecking work-hardening analysis failed.")
                                       : wh.error);
        }
        if (wh.plot) {
            for (Graph *lg : wh.plot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("plot_mode")] = canonicalMode;
        outResult[QStringLiteral("considere_true_strain")] =
                safeJsonDouble(wh.considereTrueStrain);
        outResult[QStringLiteral("considere_true_stress")] =
                safeJsonDouble(wh.considereTrueStress);
        outResult[QStringLiteral("considere_eng_strain_pct")] =
                safeJsonDouble(wh.considereEngStrainPct);
        outResult[QStringLiteral("uniform_elongation_pct")] =
                safeJsonDouble(wh.considereEngStrainPct);
        outResult[QStringLiteral("hollomon_k")] = safeJsonDouble(wh.hollomonK);
        outResult[QStringLiteral("hollomon_n")] = safeJsonDouble(wh.hollomonN);
        outResult[QStringLiteral("voce_sigma_s")] = safeJsonDouble(wh.voceSigmaS); // null on failure
        outResult[QStringLiteral("voce_sigma_s_note")] = wh.voceNote;
        outResult[QStringLiteral("considere_from_uts")] = wh.considereFromUts;
        outResult[QStringLiteral("considere_note")] = wh.considereNote;
        outResult[QStringLiteral("post_neck_rows_masked")] = wh.postNeckRowsMasked;
        outResult[QStringLiteral("merged_duplicate_points")] = wh.mergedDuplicatePoints;
        outResult[QStringLiteral("strain_in_percent")] = wh.strainInPercent;
        outResult[QStringLiteral("youngs_modulus")] = safeJsonDouble(wh.youngsModulus);
        outResult[QStringLiteral("toe_strain_x0")] = safeJsonDouble(wh.toeStrainX0);
        outResult[QStringLiteral("warnings")] = QJsonArray::fromStringList(wh.warnings);
        if (wh.resultTable)
            outResult[QStringLiteral("table")] = wh.resultTable->name();
        if (wh.plot)
            outResult[QStringLiteral("graph")] = wh.plot->name();
        return true;
    }

    auto resolveXrdWavelengthAngstrom = [](const QJsonObject &p) -> double {
        if (p.contains(QStringLiteral("xray_energy_kev"))
            && !p.value(QStringLiteral("xray_energy_kev")).isNull()) {
            const double eKev = optionalJsonFiniteDouble(p, QStringLiteral("xray_energy_kev"),
                                                         0.0, 1e6);
            if (eKev <= 0.0)
                throwInvalidParams(QStringLiteral("'xray_energy_kev' must be > 0."));
            return 12.398419843320026 / eKev;
        }
        const QString rad = optionalJsonString(p, QStringLiteral("radiation")).trimmed().toLower();
        if (!rad.isEmpty()) {
            if (rad == QStringLiteral("cuka1") || rad == QStringLiteral("cu_ka1"))
                return 1.540598;
            if (rad == QStringLiteral("cuka") || rad == QStringLiteral("cu_ka")
                || rad == QStringLiteral("cu"))
                return 1.541838;
            if (rad == QStringLiteral("moka1") || rad == QStringLiteral("mo_ka1")
                || rad == QStringLiteral("mo"))
                return 0.709319;
            if (rad == QStringLiteral("coka1") || rad == QStringLiteral("co_ka1")
                || rad == QStringLiteral("co"))
                return 1.788965;
            if (rad == QStringLiteral("crka1") || rad == QStringLiteral("cr_ka1")
                || rad == QStringLiteral("cr"))
                return 2.289700;
            if (rad == QStringLiteral("feka1") || rad == QStringLiteral("fe_ka1")
                || rad == QStringLiteral("fe"))
                return 1.936042;
            throwInvalidParams(
                    QStringLiteral("Unknown X-ray radiation '%1' (expected CuKa1|CuKa|MoKa1|CoKa1|CrKa1|FeKa1).")
                            .arg(rad));
        }
        const double wl = p.contains(QStringLiteral("wavelength"))
                ? optionalJsonFiniteDouble(p, QStringLiteral("wavelength"), 1.540598, 1000.0)
                : optionalJsonFiniteDouble(p, QStringLiteral("wavelength_angstrom"), 1.540598,
                                           1000.0);
        if (wl <= 0.0)
            throwInvalidParams(QStringLiteral("X-ray 'wavelength' (Angstrom) must be > 0."));
        return wl;
    };

    if (method == QStringLiteral("curvemath.xrd_analyze")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 8, QStringLiteral("curvemath.xrd_analyze"));
        const double wavelength = resolveXrdWavelengthAngstrom(params);
        const double instrFwhm = std::clamp(
                optionalJsonFiniteDouble(params, QStringLiteral("instr_fwhm"), 0.05, 10.0), 0.0,
                5.0);
        const double scherrerK = std::clamp(
                optionalJsonFiniteDouble(params, QStringLiteral("scherrer_k"), 0.90, 5.0), 0.1,
                2.5);
        const QString crystalSys =
                optionalJsonString(params, QStringLiteral("crystal_system"), QStringLiteral("fcc"))
                        .trimmed()
                        .toLower();
        if (!crystalSys.isEmpty() && crystalSys != QStringLiteral("fcc")
            && crystalSys != QStringLiteral("bcc") && crystalSys != QStringLiteral("sc")
            && crystalSys != QStringLiteral("hcp") && crystalSys != QStringLiteral("none")) {
            throwInvalidParams(
                    QStringLiteral("Unknown crystal_system '%1' (expected 'fcc'|'bcc'|'sc'|'hcp'|'none').")
                            .arg(crystalSys));
        }
        const bool stripKa2 = optionalJsonBool(params, QStringLiteral("strip_kalpha2"), false);
        const bool addTopD = optionalJsonBool(params, QStringLiteral("add_top_d_axis"), true);
        const bool createWh = params.contains(QStringLiteral("create_wh_plot"))
                ? optionalJsonBool(params, QStringLiteral("create_wh_plot"), true)
                : optionalJsonBool(params, QStringLiteral("wh_plot"), true);
        double cOverA = 1.633;
        if (params.contains(QStringLiteral("c_over_a"))
            && !params.value(QStringLiteral("c_over_a")).isNull()) {
            cOverA = optionalJsonFiniteDouble(params, QStringLiteral("c_over_a"), 1.633, 100.0);
            if (cOverA <= 0.2 || cOverA > 10.0) {
                throwInvalidParams(
                        QStringLiteral("curvemath.xrd_analyze 'c_over_a' must be in (0.2, 10.0]."));
            }
        }

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("XRD Peak & Williamson-Hall Analysis"));
        const CurveMathTools::XRDAnalysisResult xr = invokeAnalyzeXrd(
                d_app, g, curveTitle, wavelength, instrFwhm, scherrerK, crystalSys, stripKa2,
                addTopD, createWh, cOverA);
        if (!xr.valid) {
            throwInvalidParams(xr.error.isEmpty() ? QStringLiteral("XRD pattern analysis failed.")
                                                  : xr.error);
        }
        if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
        }
        if (xr.whPlot) {
            for (Graph *lg : xr.whPlot->layersList())
                enforceArialGraphFonts(lg);
        }
        undoGuard.commit();

        QJsonArray pksArr;
        for (const auto &pk : xr.peaks) {
            QJsonObject po;
            po[QStringLiteral("two_theta_deg")] = safeJsonDouble(pk.twoThetaDeg);
            po[QStringLiteral("d_spacing_angstrom")] = safeJsonDouble(pk.dSpacingAngstrom);
            po[QStringLiteral("fwhm_deg")] = safeJsonDouble(pk.fwhmDeg);
            po[QStringLiteral("intensity")] = safeJsonDouble(pk.intensity);
            po[QStringLiteral("scherrer_size_nm")] = safeJsonDouble(pk.scherrerSizeNm);
            po[QStringLiteral("hkl")] = pk.hklLabel;
            pksArr.append(po);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml ? ml->name() : QString();
        outResult[QStringLiteral("wavelength_angstrom")] = safeJsonDouble(xr.wavelengthAngstrom);
        outResult[QStringLiteral("c_over_a")] = safeJsonDouble(cOverA);
        outResult[QStringLiteral("mean_scherrer_size_nm")] = safeJsonDouble(xr.meanScherrerSizeNm);
        outResult[QStringLiteral("scherrer_size_nm")] = safeJsonDouble(xr.meanScherrerSizeNm);
        outResult[QStringLiteral("wh_size_nm")] = safeJsonDouble(xr.whSizeNm);
        outResult[QStringLiteral("wh_microstrain")] = safeJsonDouble(xr.whMicrostrain);
        outResult[QStringLiteral("wh_r2")] = safeJsonDouble(xr.whRSquare);
        outResult[QStringLiteral("lattice_param_angstrom")] =
                safeJsonDouble(xr.latticeParamAngstrom);
        if (crystalSys == QStringLiteral("hcp") && std::isfinite(xr.latticeParamAngstrom)
            && xr.latticeParamAngstrom > 0.0) {
            outResult[QStringLiteral("lattice_c_angstrom")] =
                    safeJsonDouble(xr.latticeParamAngstrom * cOverA);
        }
        outResult[QStringLiteral("peak_count")] = pksArr.size();
        outResult[QStringLiteral("peaks")] = pksArr;
        if (xr.peakTable)
            outResult[QStringLiteral("table")] = xr.peakTable->name();
        if (xr.whPlot)
            outResult[QStringLiteral("wh_graph")] = xr.whPlot->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.bragg_convert")) {
        const double wavelength = resolveXrdWavelengthAngstrom(params);
        QString mode = optionalJsonString(params, QStringLiteral("mode"),
                                          QStringLiteral("2theta_to_d"))
                               .trimmed()
                               .toLower();
        mode.replace(QLatin1Char('-'), QLatin1Char('_'));
        if (mode == QStringLiteral("d_spacing") || mode == QStringLiteral("d"))
            mode = QStringLiteral("2theta_to_d");
        else if (mode == QStringLiteral("q"))
            mode = QStringLiteral("2theta_to_q");
        if (mode != QStringLiteral("2theta_to_d") && mode != QStringLiteral("d_to_2theta")
            && mode != QStringLiteral("2theta_to_q") && mode != QStringLiteral("q_to_2theta")
            && mode != QStringLiteral("d_to_q") && mode != QStringLiteral("q_to_d")) {
            throwInvalidParams(
                    QStringLiteral("Unknown Bragg conversion mode '%1' (expected '2theta_to_d'|'d_to_2theta'|'2theta_to_q'|'q_to_2theta'|'d_to_q'|'q_to_d').")
                            .arg(mode));
        }

        constexpr double kPi = 3.14159265358979323846;
        auto convertOne = [&](double v) -> double {
            if (!std::isfinite(v))
                return std::numeric_limits<double>::quiet_NaN();
            if (mode == QStringLiteral("2theta_to_d")) {
                if (v <= 0.0 || v > 180.0)
                    return std::numeric_limits<double>::quiet_NaN();
                const double s = std::sin(0.5 * v * kPi / 180.0);
                return (s > 1e-12) ? (wavelength / (2.0 * s))
                                   : std::numeric_limits<double>::quiet_NaN();
            }
            if (mode == QStringLiteral("d_to_2theta")) {
                if (v <= 0.0)
                    return std::numeric_limits<double>::quiet_NaN();
                const double arg = wavelength / (2.0 * v);
                return (arg > 0.0 && arg <= 1.0 + 1e-12)
                        ? (2.0 * std::asin(std::min(1.0, arg)) * 180.0 / kPi)
                        : std::numeric_limits<double>::quiet_NaN();
            }
            if (mode == QStringLiteral("2theta_to_q")) {
                if (v < 0.0 || v > 180.0)
                    return std::numeric_limits<double>::quiet_NaN();
                return 4.0 * kPi * std::sin(0.5 * v * kPi / 180.0) / wavelength;
            }
            if (mode == QStringLiteral("q_to_2theta")) {
                if (v < 0.0)
                    return std::numeric_limits<double>::quiet_NaN();
                const double arg = v * wavelength / (4.0 * kPi);
                return (arg >= 0.0 && arg <= 1.0 + 1e-12)
                        ? (2.0 * std::asin(std::min(1.0, arg)) * 180.0 / kPi)
                        : std::numeric_limits<double>::quiet_NaN();
            }
            if (mode == QStringLiteral("d_to_q")) {
                return (v > 1e-12) ? (2.0 * kPi / v)
                                   : std::numeric_limits<double>::quiet_NaN();
            }
            // q_to_d
            return (v > 1e-12) ? (2.0 * kPi / v) : std::numeric_limits<double>::quiet_NaN();
        };

        QString outUnit = QStringLiteral("Å");
        QString outColBase = QStringLiteral("d_spacing");
        QString outLongName = QStringLiteral("d-spacing");
        if (mode.endsWith(QStringLiteral("to_2theta"))) {
            outUnit = QStringLiteral("deg");
            outColBase = QStringLiteral("TwoTheta");
            outLongName = QStringLiteral("2θ");
        } else if (mode.endsWith(QStringLiteral("to_q"))) {
            outUnit = QStringLiteral("Å^-1");
            outColBase = QStringLiteral("Q");
            outLongName = QStringLiteral("Scattering Vector q");
        }

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("mode")] = mode;
        outResult[QStringLiteral("unit")] = outUnit;
        outResult[QStringLiteral("wavelength_angstrom")] = safeJsonDouble(wavelength);

        if (params.value(QStringLiteral("values")).isArray()) {
            const QJsonArray inArr = params.value(QStringLiteral("values")).toArray();
            if (inArr.size() > 1000000)
                throwInvalidParams(
                        QStringLiteral("'values' exceeds maximum 1,000,000 elements."));
            QJsonArray convArr;
            for (const QJsonValue &iv : inArr)
                convArr.append(safeJsonDouble(convertOne(jsonToDoubleOrNaN(iv))));
            outResult[QStringLiteral("converted")] = convArr;
            outResult[QStringLiteral("count")] = convArr.size();
            return true;
        }

        const bool wantGraphTopAxis = optionalJsonBool(params, QStringLiteral("add_top_axis"), false)
                || (params.contains(QStringLiteral("graph"))
                    && !params.contains(QStringLiteral("table"))
                    && !params.contains(QStringLiteral("col")));
        if (wantGraphTopAxis) {
            MultiLayer *ml = resolveMultiLayer(d_app, params);
            Graph *parentG = resolveGraphLayer(ml, params);
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Bragg Linked Axis"));
            QString xExpr;
            if (mode == QStringLiteral("2theta_to_q")) {
                xExpr = QStringLiteral("4*3.141592653589793*sin(x*3.141592653589793/360)/(%1)")
                                .arg(wavelength, 0, 'g', 12);
            } else if (mode == QStringLiteral("2theta_to_d")) {
                xExpr = QStringLiteral("(%1)/(2*sin(x*3.141592653589793/360))")
                                .arg(wavelength, 0, 'g', 12);
            } else if (mode == QStringLiteral("d_to_2theta")) {
                xExpr = QStringLiteral("360/3.141592653589793*asin((%1)/(2*x))")
                                .arg(wavelength, 0, 'g', 12);
            } else if (mode == QStringLiteral("q_to_2theta")) {
                xExpr = QStringLiteral("360/3.141592653589793*asin(x*(%1)/(4*3.141592653589793))")
                                .arg(wavelength, 0, 'g', 12);
            } else {
                // d_to_q and q_to_d are both 2*pi / x
                xExpr = QStringLiteral("2*3.141592653589793/x");
            }
            Graph *linkedG =
                    ml->addLinkedLayer(parentG, true, false, xExpr, QStringLiteral("y"));
            if (linkedG) {
                linkedG->setAxisTitleAuto(QwtPlot::xTop, false);
                linkedG->setTopAxisTitle(QStringLiteral("%1 (%2)").arg(outLongName, outUnit));
                enforceArialGraphFonts(linkedG);
                replotGraphOrDefer(linkedG);
            }
            undoGuard.commit();
            outResult[QStringLiteral("graph")] = ml->name();
            outResult[QStringLiteral("linked_layer")] =
                    linkedG ? ml->layersList().indexOf(linkedG) : -1;
            return true;
        }

        Table *t = resolveTable(d_app, params);
        const int srcCol = resolveColumnIndex(
                t, params.contains(QStringLiteral("col")) ? params.value(QStringLiteral("col"))
                                                          : QJsonValue(0),
                true);
        QString dstName = optionalJsonString(params, QStringLiteral("out_col")).trimmed();
        if (dstName.isEmpty())
            dstName = outColBase;
        int dstIdx = t->colIndex(t->name() + QStringLiteral("_") + dstName);
        if (dstIdx < 0) {
            QString uniqueCol = dstName;
            int suffix = 2;
            while (t->colNames().contains(uniqueCol))
                uniqueCol = QStringLiteral("%1_%2").arg(dstName).arg(suffix++);
            t->addCol(SciDAVis::Y);
            dstIdx = t->numCols() - 1;
            t->setColName(dstIdx, uniqueCol);
        }
        t->setColLongName(dstIdx, outLongName);
        t->setColUnits(dstIdx, outUnit);
        applyDefaultNumericFormat6g(t, dstIdx);
        Column *srcC = t->column(srcCol);
        Column *dstC = t->column(dstIdx);
        QVector<double> outVec(t->numRows(), std::numeric_limits<double>::quiet_NaN());
        QJsonArray previewArr;
        for (int r = 0; r < t->numRows(); ++r) {
            if (!srcC || srcC->isInvalid(r)) {
                if (dstC)
                    dstC->setInvalid(r, true);
                if (r < 2000)
                    previewArr.append(QJsonValue::Null);
                continue;
            }
            const double cv = convertOne(t->cell(r, srcCol));
            outVec[r] = cv;
            if (r < 2000)
                previewArr.append(safeJsonDouble(cv));
        }
        if (dstC) {
            dstC->replaceValues(0, outVec);
            for (int r = 0; r < t->numRows(); ++r) {
                if (!std::isfinite(outVec[r]))
                    dstC->setInvalid(r, true);
            }
        }
        outResult[QStringLiteral("table")] = t->name();
        outResult[QStringLiteral("column")] = t->colLabel(dstIdx);
        outResult[QStringLiteral("converted")] = previewArr;
        return true;
    }

    auto importVaspFileToTable = [](ApplicationWindow *app, const QString &filePath,
                                    double *outEf) -> Table * {
        if (outEf)
            *outEf = 0.0;
        const QFileInfo fi(filePath);
        if (filePath.trimmed().isEmpty() || !fi.exists() || !fi.isFile())
            throwInvalidParams(QStringLiteral("VASP input file does not exist: '%1'.").arg(filePath));
        QFile vf(fi.absoluteFilePath());
        if (!vf.open(QIODevice::ReadOnly))
            throwRpcRuntime(QStringLiteral("Failed to open VASP input file '%1'.").arg(filePath));
        const QByteArray raw = vf.readAll();
        vf.close();

        const QString baseStem = fi.baseName().trimmed().isEmpty()
                ? QStringLiteral("VASPData")
                : fi.baseName().trimmed();
        const QString uniqueName = app->generateUniqueName(baseStem);
        double ef = 0.0;
        future::Table *ft =
                AsciiTableImportFilter::importVaspTextData(raw, uniqueName, fi.absoluteFilePath(), &ef);
        if (!ft)
            ft = AsciiTableImportFilter::importLabTextData(raw, uniqueName, fi.absoluteFilePath());
        if (!ft)
            throwInvalidParams(QStringLiteral("Failed to parse VASP data file '%1'.").arg(filePath));

        QList<Column *> clonedCols;
        for (int i = 0; i < ft->columnCount(); ++i) {
            if (Column *src = ft->column(i)) {
                auto *dst = new Column(src->name(), src->columnMode());
                dst->copy(src);
                dst->setPlotDesignation(src->plotDesignation());
                dst->setLongName(src->longName());
                dst->setUnits(src->units());
                dst->setComment(src->comment());
                clonedCols.append(dst);
            }
        }
        const QString commentLabel = ft->comment().isEmpty() ? fi.fileName() : ft->comment();
        delete ft;
        if (clonedCols.isEmpty())
            throwInvalidParams(QStringLiteral("VASP file '%1' contained zero columns.").arg(filePath));
        if (outEf)
            *outEf = ef;
        return app->newTable(uniqueName, commentLabel, clonedCols);
    };

    if (method == QStringLiteral("curvemath.vasp_dos")) {
        Table *t = nullptr;
        double importedFermi = 0.0;
        bool hasImportedFermi = false;
        const QString doscarPath = !optionalJsonString(params, QStringLiteral("doscar_path")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("doscar_path")).trimmed()
                : optionalJsonString(params, QStringLiteral("path")).trimmed();
        if ((!params.contains(QStringLiteral("table"))
             || params.value(QStringLiteral("table")).isNull()
             || params.value(QStringLiteral("table")).toString().trimmed().isEmpty())
            && !doscarPath.isEmpty()) {
            t = importVaspFileToTable(d_app, doscarPath, &importedFermi);
            hasImportedFermi = true;
        } else {
            t = resolveTable(d_app, params);
        }
        const int eIdx = resolveColumnIndex(
                t, params.contains(QStringLiteral("energy_col"))
                           && !params.value(QStringLiteral("energy_col")).isNull()
                        ? params.value(QStringLiteral("energy_col"))
                        : QJsonValue(0),
                true);
        const QString energyCol = t->colLabel(eIdx);
        QStringList dosCols;
        const QJsonValue upVal = params.contains(QStringLiteral("dos_cols"))
                ? params.value(QStringLiteral("dos_cols"))
                : params.value(QStringLiteral("up_cols"));
        if (upVal.isArray()) {
            for (const QJsonValue &cv : upVal.toArray())
                dosCols << t->colLabel(resolveColumnIndex(t, cv, true));
        } else if (upVal.isString() && !upVal.toString().trimmed().isEmpty()) {
            dosCols << t->colLabel(resolveColumnIndex(t, upVal, true));
        } else if (upVal.isDouble()) {
            dosCols << t->colLabel(resolveColumnIndex(t, upVal, true));
        }
        QStringList downCols;
        const QJsonValue dnVal = params.contains(QStringLiteral("down_dos_cols"))
                ? params.value(QStringLiteral("down_dos_cols"))
                : params.value(QStringLiteral("down_cols"));
        if (dnVal.isArray()) {
            for (const QJsonValue &cv : dnVal.toArray())
                downCols << t->colLabel(resolveColumnIndex(t, cv, true));
        } else if (dnVal.isString() && !dnVal.toString().trimmed().isEmpty()) {
            downCols << t->colLabel(resolveColumnIndex(t, dnVal, true));
        } else if (dnVal.isDouble()) {
            downCols << t->colLabel(resolveColumnIndex(t, dnVal, true));
        }
        double eFermi = params.contains(QStringLiteral("fermi_energy"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("fermi_energy"), 0.0, 1e6)
                : optionalJsonFiniteDouble(params, QStringLiteral("e_fermi"), 0.0, 1e6);
        if (hasImportedFermi && qFuzzyIsNull(eFermi) && !qFuzzyIsNull(importedFermi))
            eFermi = importedFermi;
        const bool vertE = optionalJsonBool(params, QStringLiteral("vertical_energy"), false);

        const CurveMathTools::VaspPlotResult vr =
                CurveMathTools::plotVaspDos(d_app, t, energyCol, dosCols, downCols, eFermi, vertE);
        if (!vr.valid) {
            throwInvalidParams(vr.error.isEmpty() ? QStringLiteral("VASP DOS plot failed.")
                                                  : vr.error);
        }
        if (vr.plot) {
            for (Graph *lg : vr.plot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("fermi_energy_ev")] = safeJsonDouble(vr.fermiEnergyEV);
        outResult[QStringLiteral("bandgap_ev")] = safeJsonDouble(vr.bandgapEV);
        outResult[QStringLiteral("d_band_center_ev")] = safeJsonDouble(vr.dBandCenterEV);
        if (vr.dataTable)
            outResult[QStringLiteral("table")] = vr.dataTable->name();
        if (vr.plot)
            outResult[QStringLiteral("graph")] = vr.plot->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.vasp_bands")) {
        Table *t = nullptr;
        double importedFermi = 0.0;
        bool hasImportedFermi = false;
        const QString eigenPath = !optionalJsonString(params, QStringLiteral("eigenval_path")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("eigenval_path")).trimmed()
                : optionalJsonString(params, QStringLiteral("path")).trimmed();
        if ((!params.contains(QStringLiteral("table"))
             || params.value(QStringLiteral("table")).isNull()
             || params.value(QStringLiteral("table")).toString().trimmed().isEmpty())
            && !eigenPath.isEmpty()) {
            t = importVaspFileToTable(d_app, eigenPath, &importedFermi);
            hasImportedFermi = true;
        } else {
            t = resolveTable(d_app, params);
        }
        const QJsonValue kKey = (params.contains(QStringLiteral("k_col"))
                                 && !params.value(QStringLiteral("k_col")).isNull())
                ? params.value(QStringLiteral("k_col"))
                : ((params.contains(QStringLiteral("x_col"))
                    && !params.value(QStringLiteral("x_col")).isNull())
                           ? params.value(QStringLiteral("x_col"))
                           : QJsonValue(0));
        const int kIdx = resolveColumnIndex(t, kKey, true);
        const QString kCol = t->colLabel(kIdx);
        QStringList bandCols;
        const QJsonValue bVal = params.contains(QStringLiteral("band_cols"))
                ? params.value(QStringLiteral("band_cols"))
                : params.value(QStringLiteral("y_cols"));
        if (bVal.isArray()) {
            for (const QJsonValue &cv : bVal.toArray())
                bandCols << t->colLabel(resolveColumnIndex(t, cv, true));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != kIdx && t->column(c)
                    && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    bandCols << t->colLabel(c);
                }
            }
        }
        QVector<double> kTicks;
        if (params.value(QStringLiteral("k_ticks")).isArray()) {
            for (const QJsonValue &kv : params.value(QStringLiteral("k_ticks")).toArray())
                kTicks.append(parseFiniteCoordDouble(kv, QStringLiteral("k_ticks")));
        }
        QStringList kLabels;
        if (params.value(QStringLiteral("k_labels")).isArray()) {
            for (const QJsonValue &lv : params.value(QStringLiteral("k_labels")).toArray()) {
                QString lbl = lv.toString().trimmed();
                if (lbl == QStringLiteral("G")
                    || lbl.compare(QStringLiteral("\\Gamma"), Qt::CaseInsensitive) == 0
                    || lbl.compare(QStringLiteral("Gamma"), Qt::CaseInsensitive) == 0) {
                    lbl = QStringLiteral("\u0393");
                }
                kLabels << lbl;
            }
        }
        double eFermi = params.contains(QStringLiteral("fermi_energy"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("fermi_energy"), 0.0, 1e6)
                : optionalJsonFiniteDouble(params, QStringLiteral("e_fermi"), 0.0, 1e6);
        if (hasImportedFermi && qFuzzyIsNull(eFermi) && !qFuzzyIsNull(importedFermi))
            eFermi = importedFermi;
        const double eMin = optionalJsonFiniteDouble(params, QStringLiteral("e_min"), -6.0, 1e6);
        const double eMax = optionalJsonFiniteDouble(params, QStringLiteral("e_max"), 6.0, 1e6);
        validateFiniteCoordRange(eMin, eMax, QStringLiteral("curvemath.vasp_bands [e_min, e_max]"));

        const CurveMathTools::VaspPlotResult vr = CurveMathTools::plotVaspBands(
                d_app, t, kCol, bandCols, kTicks, kLabels, eFermi, std::min(eMin, eMax),
                std::max(eMin, eMax));
        if (!vr.valid) {
            throwInvalidParams(vr.error.isEmpty() ? QStringLiteral("VASP band structure plot failed.")
                                                  : vr.error);
        }
        if (vr.plot) {
            for (Graph *lg : vr.plot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("fermi_energy_ev")] = safeJsonDouble(vr.fermiEnergyEV);
        outResult[QStringLiteral("bandgap_ev")] = safeJsonDouble(vr.bandgapEV);
        if (vr.dataTable)
            outResult[QStringLiteral("table")] = vr.dataTable->name();
        if (vr.plot)
            outResult[QStringLiteral("graph")] = vr.plot->name();
        return true;
    }

    if (method == QStringLiteral("curvemath.vasp_band_dos")) {
        Table *bandTbl = nullptr;
        Table *dosTbl = nullptr;
        double importedBandFermi = 0.0;
        double importedDosFermi = 0.0;
        bool hasImportedBandFermi = false;
        bool hasImportedDosFermi = false;

        const QString eigenPath = !optionalJsonString(params, QStringLiteral("eigenval_path")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("eigenval_path")).trimmed()
                : optionalJsonString(params, QStringLiteral("band_path")).trimmed();
        const QString doscarPath = !optionalJsonString(params, QStringLiteral("doscar_path")).trimmed().isEmpty()
                ? optionalJsonString(params, QStringLiteral("doscar_path")).trimmed()
                : optionalJsonString(params, QStringLiteral("dos_path")).trimmed();

        const bool hasBandTblParam =
                (params.contains(QStringLiteral("band_table"))
                 && !params.value(QStringLiteral("band_table")).isNull()
                 && !params.value(QStringLiteral("band_table")).toString().trimmed().isEmpty())
                || (params.contains(QStringLiteral("bands_table"))
                    && !params.value(QStringLiteral("bands_table")).isNull()
                    && !params.value(QStringLiteral("bands_table")).toString().trimmed().isEmpty())
                || (params.contains(QStringLiteral("table"))
                    && !params.value(QStringLiteral("table")).isNull()
                    && !params.value(QStringLiteral("table")).toString().trimmed().isEmpty());
        if (hasBandTblParam) {
            const QString key = params.contains(QStringLiteral("band_table"))
                            && !params.value(QStringLiteral("band_table")).isNull()
                    ? QStringLiteral("band_table")
                    : (params.contains(QStringLiteral("bands_table"))
                                       && !params.value(QStringLiteral("bands_table")).isNull()
                               ? QStringLiteral("bands_table")
                               : QStringLiteral("table"));
            bandTbl = resolveTable(d_app, params, key, true);
        } else if (!eigenPath.isEmpty()) {
            bandTbl = importVaspFileToTable(d_app, eigenPath, &importedBandFermi);
            hasImportedBandFermi = true;
        } else {
            throwInvalidParams(QStringLiteral(
                    "curvemath.vasp_band_dos requires 'band_table' (or 'table') or 'eigenval_path'."));
        }

        const bool hasDosTblParam = params.contains(QStringLiteral("dos_table"))
                && !params.value(QStringLiteral("dos_table")).isNull()
                && !params.value(QStringLiteral("dos_table")).toString().trimmed().isEmpty();
        if (hasDosTblParam) {
            dosTbl = resolveTable(d_app, params, QStringLiteral("dos_table"), true);
        } else if (!doscarPath.isEmpty()) {
            dosTbl = importVaspFileToTable(d_app, doscarPath, &importedDosFermi);
            hasImportedDosFermi = true;
        } else {
            throwInvalidParams(QStringLiteral(
                    "curvemath.vasp_band_dos requires 'dos_table' or 'doscar_path'."));
        }

        const QJsonValue kKey = (params.contains(QStringLiteral("k_col"))
                                 && !params.value(QStringLiteral("k_col")).isNull())
                ? params.value(QStringLiteral("k_col"))
                : QJsonValue(0);
        const int kIdx = resolveColumnIndex(bandTbl, kKey, true);
        const QString kCol = bandTbl->colLabel(kIdx);
        QStringList bandCols;
        const QJsonValue bVal = params.contains(QStringLiteral("band_cols"))
                ? params.value(QStringLiteral("band_cols"))
                : params.value(QStringLiteral("y_cols"));
        if (bVal.isArray()) {
            for (const QJsonValue &cv : bVal.toArray())
                bandCols << bandTbl->colLabel(resolveColumnIndex(bandTbl, cv, true));
        } else {
            for (int c = 0; c < bandTbl->numCols(); ++c) {
                if (c != kIdx && bandTbl->column(c)
                    && bandTbl->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    bandCols << bandTbl->colLabel(c);
                }
            }
        }
        QVector<double> kTicks;
        if (params.value(QStringLiteral("k_ticks")).isArray()) {
            for (const QJsonValue &kv : params.value(QStringLiteral("k_ticks")).toArray())
                kTicks.append(parseFiniteCoordDouble(kv, QStringLiteral("k_ticks")));
        }
        QStringList kLabels;
        if (params.value(QStringLiteral("k_labels")).isArray()) {
            for (const QJsonValue &lv : params.value(QStringLiteral("k_labels")).toArray()) {
                QString lbl = lv.toString().trimmed();
                if (lbl == QStringLiteral("G")
                    || lbl.compare(QStringLiteral("\\Gamma"), Qt::CaseInsensitive) == 0
                    || lbl.compare(QStringLiteral("Gamma"), Qt::CaseInsensitive) == 0) {
                    lbl = QStringLiteral("\u0393");
                }
                kLabels << lbl;
            }
        }

        const int eIdx = resolveColumnIndex(
                dosTbl, params.contains(QStringLiteral("energy_col"))
                                && !params.value(QStringLiteral("energy_col")).isNull()
                        ? params.value(QStringLiteral("energy_col"))
                        : QJsonValue(0),
                true);
        const QString energyCol = dosTbl->colLabel(eIdx);
        QStringList dosCols;
        const QJsonValue upVal = params.contains(QStringLiteral("dos_cols"))
                ? params.value(QStringLiteral("dos_cols"))
                : params.value(QStringLiteral("up_cols"));
        if (upVal.isArray()) {
            for (const QJsonValue &cv : upVal.toArray())
                dosCols << dosTbl->colLabel(resolveColumnIndex(dosTbl, cv, true));
        } else if ((upVal.isString() && !upVal.toString().trimmed().isEmpty()) || upVal.isDouble()) {
            dosCols << dosTbl->colLabel(resolveColumnIndex(dosTbl, upVal, true));
        }
        QStringList downCols;
        const QJsonValue dnVal = params.contains(QStringLiteral("down_dos_cols"))
                ? params.value(QStringLiteral("down_dos_cols"))
                : params.value(QStringLiteral("down_cols"));
        if (dnVal.isArray()) {
            for (const QJsonValue &cv : dnVal.toArray())
                downCols << dosTbl->colLabel(resolveColumnIndex(dosTbl, cv, true));
        } else if ((dnVal.isString() && !dnVal.toString().trimmed().isEmpty()) || dnVal.isDouble()) {
            downCols << dosTbl->colLabel(resolveColumnIndex(dosTbl, dnVal, true));
        }

        const double reqFermi = params.contains(QStringLiteral("fermi_energy"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("fermi_energy"), 0.0, 1e6)
                : optionalJsonFiniteDouble(params, QStringLiteral("e_fermi"), 0.0, 1e6);
        const double eFermiBand = (hasImportedBandFermi && qFuzzyIsNull(reqFermi)
                                   && !qFuzzyIsNull(importedBandFermi))
                ? importedBandFermi
                : reqFermi;
        const double eFermiDos = (hasImportedDosFermi && qFuzzyIsNull(reqFermi)
                                  && !qFuzzyIsNull(importedDosFermi))
                ? importedDosFermi
                : eFermiBand;
        const double eMin = optionalJsonFiniteDouble(params, QStringLiteral("e_min"), -6.0, 1e6);
        const double eMax = optionalJsonFiniteDouble(params, QStringLiteral("e_max"), 6.0, 1e6);
        validateFiniteCoordRange(eMin, eMax,
                                 QStringLiteral("curvemath.vasp_band_dos [e_min, e_max]"));
        const double eLo = std::min(eMin, eMax);
        const double eHi = std::max(eMin, eMax);

        const CurveMathTools::VaspPlotResult vrBand = CurveMathTools::plotVaspBands(
                d_app, bandTbl, kCol, bandCols, kTicks, kLabels, eFermiBand, eLo, eHi);
        if (!vrBand.valid || !vrBand.plot) {
            if (vrBand.plot)
                closeAndDeleteEmptyPlot(d_app, vrBand.plot);
            throwInvalidParams(vrBand.error.isEmpty()
                                       ? QStringLiteral("VASP band structure plot failed.")
                                       : vrBand.error);
        }

        const CurveMathTools::VaspPlotResult vrDos = CurveMathTools::plotVaspDos(
                d_app, dosTbl, energyCol, dosCols, downCols, eFermiDos, true);
        if (!vrDos.valid || !vrDos.plot) {
            if (vrDos.plot)
                closeAndDeleteEmptyPlot(d_app, vrDos.plot);
            closeAndDeleteEmptyPlot(d_app, vrBand.plot);
            throwInvalidParams(vrDos.error.isEmpty() ? QStringLiteral("VASP DOS plot failed.")
                                                     : vrDos.error);
        }

        MultiLayer *ml = vrBand.plot;
        if (Graph *gDos = ml->addLayer()) {
            if (Table *dt = vrDos.dataTable) {
                static const QColor kDosPalette[6] = {
                    QColor(25, 45, 85),    QColor(0, 114, 178),   QColor(0, 158, 115),
                    QColor(213, 94, 0),    QColor(204, 121, 167), QColor(230, 159, 0)
                };
                const int nUp = std::max(1, dosCols.isEmpty() ? (dt->numCols() - 1) : dosCols.size());
                const int nRows = dt->numRows();
                for (int c = 1; c < dt->numCols(); ++c) {
                    if (gDos->insertCurve(dt, dt->colName(c), dt->colName(0), Graph::Line, 0,
                                          std::max(0, nRows - 1))) {
                        const int ci = gDos->curves() - 1;
                        const bool isDown = !downCols.isEmpty() && (c - 1 >= nUp);
                        const int palIdx = isDown ? ((c - 1 - nUp) % 6) : ((c - 1) % 6);
                        gDos->setCurvePen(
                                ci,
                                QPen(kDosPalette[palIdx], 1.8,
                                     isDown ? Qt::DashLine : Qt::SolidLine));
                    }
                }
            }
            Graph::ReferenceLineSpec efSpec;
            efSpec.orientation = 0; // horizontal y = 0
            efSpec.value = 0.0;
            efSpec.color = QColor(100, 116, 139);
            efSpec.style = Qt::DashLine;
            efSpec.width = 1.3;
            gDos->addReferenceLine(efSpec);

            gDos->applyFramePreset(Graph::FrameBoxInwardTicks);
            gDos->setTitle(QString());
            gDos->setXAxisTitle(QStringLiteral("DOS / states eV<sup>-1</sup>"));
            gDos->setAxisTitleAuto(QwtPlot::yLeft, false);
            gDos->setYAxisTitle(QString());
            if (auto *sd = gDos->plotWidget() ? gDos->plotWidget()->axisScaleDraw(QwtPlot::yLeft)
                                              : nullptr) {
                sd->enableComponent(QwtAbstractScaleDraw::Labels, false);
            }
            gDos->setAutoScale();
            gDos->setScale(QwtPlot::yLeft, eLo, eHi, 0.0, 6, 1, 0, false);
            gDos->setAxisUserLocked(QwtPlot::yLeft, true);
        }

        closeAndDeleteEmptyPlot(d_app, vrDos.plot);

        ml->resize(760, 460);
        ml->setCols(2);
        ml->setRows(1);
        ml->arrangeLayers(false, false);
        for (Graph *lg : ml->layersList()) {
            enforceArialGraphFonts(lg);
            lg->replot();
        }
        ml->setActiveGraph(ml->layersList().first());

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("layers")] = ml->layersList().size();
        outResult[QStringLiteral("fermi_energy_ev")] = safeJsonDouble(vrBand.fermiEnergyEV);
        outResult[QStringLiteral("bandgap_ev")] = safeJsonDouble(vrBand.bandgapEV);
        outResult[QStringLiteral("dos_bandgap_ev")] = safeJsonDouble(vrDos.bandgapEV);
        outResult[QStringLiteral("d_band_center_ev")] = safeJsonDouble(vrDos.dBandCenterEV);
        if (vrBand.dataTable)
            outResult[QStringLiteral("band_table")] = vrBand.dataTable->name();
        if (vrDos.dataTable)
            outResult[QStringLiteral("dos_table")] = vrDos.dataTable->name();
        return true;
    }

    // ------------------------------------------------------------------------
    // 7D. Parametric, Nonparametric, Survival, Reliability & Multivariate Stats
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("stats.normality")) {
        Table *t = resolveTable(d_app, params);
        const int colIdx = resolveColumnIndex(
                t, params.contains(QStringLiteral("col"))
                           ? params.value(QStringLiteral("col"))
                           : QJsonValue(0),
                true);
        const QString colName = t->colLabel(colIdx);
        const int nFinite = countFiniteUnmaskedInTableColumn(t, colIdx);
        if (nFinite < 3) {
            throwInvalidParams(
                    QStringLiteral("stats.normality requires at least 3 finite unmasked values in column '%1' (got %2).")
                            .arg(colName)
                            .arg(nFinite));
        }
        Graph *overlayG = nullptr;
        if (params.contains(QStringLiteral("graph"))) {
            if (MultiLayer *ml = resolveMultiLayer(d_app, params, QStringLiteral("graph"), false))
                overlayG = resolveGraphLayer(ml, params, false);
        }
        const auto nr = GraphStatsAndSurfaceFit::runNormalityTest(d_app, t, colName, overlayG);
        if (nr.n < 3) {
            throwInvalidParams(
                    QStringLiteral("stats.normality requires at least 3 valid numeric observations in '%1' (got %2).")
                            .arg(colName)
                            .arg(nr.n));
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("n")] = nr.n;
        outResult[QStringLiteral("mean")] = safeJsonDouble(nr.mean);
        outResult[QStringLiteral("sd")] = safeJsonDouble(nr.sd);
        outResult[QStringLiteral("shapiro_wilk_w")] = safeJsonDouble(nr.shapiroWilkW);
        outResult[QStringLiteral("shapiro_wilk_p")] = safeJsonDouble(nr.shapiroWilkP);
        outResult[QStringLiteral("ks_d")] = safeJsonDouble(nr.ksD);
        outResult[QStringLiteral("ks_p")] = safeJsonDouble(nr.ksP);
        outResult[QStringLiteral("anderson_darling_a2")] = safeJsonDouble(nr.andersonDarlingA2);
        outResult[QStringLiteral("is_normal")] = nr.isNormalAt005;
        if (nr.reportTable)
            outResult[QStringLiteral("table")] = nr.reportTable->name();
        return true;
    }

    if (method == QStringLiteral("stats.outlier")) {
        Table *t = resolveTable(d_app, params);
        const QString colName = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("col"))
                                           ? params.value(QStringLiteral("col"))
                                           : QJsonValue(0),
                                   true));
        const QString mStr = optionalJsonString(params, QStringLiteral("method"),
                                                QStringLiteral("grubbs"))
                                     .trimmed()
                                     .toLower();
        int om = 0; // Grubbs
        if (mStr.isEmpty() || mStr == QStringLiteral("grubbs"))
            om = 0;
        else if (mStr == QStringLiteral("gesd") || mStr == QStringLiteral("esd"))
            om = 1;
        else if (mStr == QStringLiteral("dixon") || mStr == QStringLiteral("dixon_q"))
            om = 2;
        else
            throwInvalidParams(
                    QStringLiteral("Unknown outlier method '%1' (expected 'grubbs'|'gesd'|'dixon').")
                            .arg(mStr));
        const double alpha =
                std::clamp(optionalJsonFiniteDouble(params, QStringLiteral("alpha"), 0.05, 1.0),
                           1e-6, 0.5);
        const int maxOut = parseBoundedInt(params, QStringLiteral("max_outliers"), 5, 1, 1000);
        const bool maskDet = optionalJsonBool(params, QStringLiteral("mask_outliers"), false);
        const auto ot = GraphStatsAndSurfaceFit::runOutlierTest(d_app, t, colName, om, alpha,
                                                                maxOut, maskDet);
        QJsonArray rowsArr;
        for (int r : ot.outlierRows)
            rowsArr.append(r);
        outResult[QStringLiteral("ok")] = ot.valid;
        outResult[QStringLiteral("n")] = ot.n;
        outResult[QStringLiteral("outlier_rows")] = rowsArr;
        outResult[QStringLiteral("outlier_values")] = toJsonArray(ot.outlierValues);
        outResult[QStringLiteral("statistic")] = safeJsonDouble(ot.primaryStatistic);
        outResult[QStringLiteral("critical_value")] = safeJsonDouble(ot.criticalValue);
        outResult[QStringLiteral("p_value")] = safeJsonDouble(ot.pValue);
        if (ot.reportTable)
            outResult[QStringLiteral("table")] = ot.reportTable->name();
        return true;
    }

    if (method == QStringLiteral("stats.anova_oneway")) {
        Table *t = resolveTable(d_app, params);
        QStringList colNames;
        if (params.value(QStringLiteral("cols")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("cols")).toArray()) {
                const QString lbl = t->colLabel(resolveColumnIndex(t, cv, true));
                if (!colNames.contains(lbl))
                    colNames << lbl;
            }
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric)
                    colNames << t->colLabel(c);
            }
        }

        const QString rawPh = (params.contains(QStringLiteral("posthoc"))
                                       ? params.value(QStringLiteral("posthoc")).toString()
                                       : (params.contains(QStringLiteral("post_hoc"))
                                                  ? params.value(QStringLiteral("post_hoc")).toString()
                                                  : params.value(QStringLiteral("method")).toString()))
                                      .trimmed()
                                      .toLower();
        QString canonicalPosthoc = QStringLiteral("holm");
        if (rawPh.isEmpty() || rawPh == QStringLiteral("holm") || rawPh == QStringLiteral("welch")
            || rawPh == QStringLiteral("holm_welch") || rawPh == QStringLiteral("welch_holm")
            || rawPh == QStringLiteral("bonferroni")) {
            canonicalPosthoc = QStringLiteral("holm");
        } else if (rawPh == QStringLiteral("tukey") || rawPh == QStringLiteral("tukey_hsd")
                   || rawPh == QStringLiteral("tukey-hsd")
                   || rawPh == QStringLiteral("tukey_kramer") || rawPh == QStringLiteral("hsd")) {
            canonicalPosthoc = QStringLiteral("tukey");
        } else if (rawPh == QStringLiteral("games_howell")
                   || rawPh == QStringLiteral("games-howell") || rawPh == QStringLiteral("gh")) {
            canonicalPosthoc = QStringLiteral("games_howell");
        } else if (rawPh == QStringLiteral("dunnett")) {
            canonicalPosthoc = QStringLiteral("dunnett");
        } else {
            throwInvalidParams(
                    QStringLiteral("Unsupported ANOVA posthoc '%1' (expected 'holm', 'tukey', 'games_howell', or 'dunnett').")
                            .arg(rawPh));
        }

        // Collect the groups with at least one valid unmasked finite value. A group of one value
        // enters the ANOVA as in the textbook test (between-group sum of squares, N - k); it
        // used to be dropped without a word (A(5), B(5), C(1) gave F of A and B alone).
        struct AnovaGroupData
        {
            QString label;
            std::vector<double> vals;
            double mean = 0.0;
            double var = 0.0;
        };
        std::vector<AnovaGroupData> groups;
        int N = 0;
        double grandSum = 0.0;
        for (const QString &cn : colNames) {
            const int ci = resolveColumnIndex(t, cn, true);
            std::vector<double> gv;
            for (int r = 0; r < t->numRows(); ++r) {
                if (t->column(ci) && !t->column(ci)->isInvalid(r) && !t->column(ci)->isMasked(r)) {
                    const double v = t->cell(r, ci);
                    if (std::isfinite(v))
                        gv.push_back(v);
                }
            }
            if (!gv.empty()) {
                const double m = std::accumulate(gv.begin(), gv.end(), 0.0) / double(gv.size());
                double ss = 0.0;
                for (double v : gv)
                    ss += (v - m) * (v - m);
                const double s2 = (gv.size() > 1) ? ss / double(gv.size() - 1) : 0.0;
                for (double v : gv)
                    grandSum += v;
                N += static_cast<int>(gv.size());
                groups.push_back({ cn, std::move(gv), m, s2 });
            }
        }
        if (groups.size() < 2) {
            throwInvalidParams(QStringLiteral(
                    "One-way ANOVA requires at least 2 groups with valid unmasked numeric values."));
        }
        if (N <= static_cast<int>(groups.size())) {
            throwInvalidParams(
                    QStringLiteral("One-way ANOVA has no within-group degrees of freedom: all %1 "
                                   "groups have a single value (N - k = 0). At least one group "
                                   "needs 2 or more values.")
                            .arg(groups.size()));
        }
        QStringList singletonGroups;
        for (const auto &gItem : groups) {
            if (gItem.vals.size() == 1)
                singletonGroups << gItem.label;
        }

        int controlIdx = 0;
        const QJsonValue ctrlSpec = params.contains(QStringLiteral("control"))
                ? params.value(QStringLiteral("control"))
                : (params.contains(QStringLiteral("control_col"))
                           ? params.value(QStringLiteral("control_col"))
                           : params.value(QStringLiteral("control_group")));
        if (!ctrlSpec.isUndefined() && !ctrlSpec.isNull()) {
            const int ctrlCol = resolveColumnIndex(t, ctrlSpec, true);
            const QString wantLbl = t->colLabel(ctrlCol);
            controlIdx = -1;
            for (size_t gi = 0; gi < groups.size(); ++gi) {
                if (groups[gi].label == wantLbl) {
                    controlIdx = static_cast<int>(gi);
                    break;
                }
            }
            if (controlIdx < 0) {
                throwInvalidParams(
                        QStringLiteral("Control group '%1' is not among the valid ANOVA groups.")
                                .arg(wantLbl));
            }
        }
        const QString controlLabel = groups[static_cast<size_t>(controlIdx)].label;

        Graph *annG = nullptr;
        MultiLayer *annMl = nullptr;
        const bool hasGraphSpec = params.contains(QStringLiteral("graph"))
                && !params.value(QStringLiteral("graph")).isNull()
                && !optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty();
        const bool wantAnovaBrackets = params.contains(QStringLiteral("add_brackets"))
                && !params.value(QStringLiteral("add_brackets")).isNull()
                ? optionalJsonBool(params, QStringLiteral("add_brackets"), hasGraphSpec)
                : hasGraphSpec;
        if (wantAnovaBrackets) {
            annMl = hasGraphSpec
                    ? resolveMultiLayer(d_app, params, QStringLiteral("graph"), true)
                    : resolveMultiLayer(d_app, params, QStringLiteral("graph"), false);
            if (annMl)
                annG = resolveGraphLayer(annMl, params, false);
        }

        struct ScopedTableAnovaProps
        {
            Table *tbl;
            explicit ScopedTableAnovaProps(Table *tPtr, const QString &ph, const QString &ctrl)
                : tbl(tPtr)
            {
                if (tbl) {
                    tbl->setProperty("torigraph_anova_posthoc", ph);
                    tbl->setProperty("torigraph_anova_control", ctrl);
                }
            }
            ~ScopedTableAnovaProps()
            {
                if (tbl) {
                    tbl->setProperty("torigraph_anova_posthoc", QVariant());
                    tbl->setProperty("torigraph_anova_control", QVariant());
                }
            }
        } scopedAnovaProps(t, canonicalPosthoc, controlLabel);

        QStringList validGroupLabels;
        for (const auto &gItem : groups)
            validGroupLabels << gItem.label;
        Table *repTbl = GraphStatsAndSurfaceFit::runGroupComparisonAndAnnotate(
                d_app, t, validGroupLabels, annG);
        if (!repTbl)
            throwRpcRuntime(QStringLiteral("One-way ANOVA / group comparison failed."));

        const int kGroups = static_cast<int>(groups.size());
        const int df1 = kGroups - 1;
        const int df2 = N - kGroups;
        const double grandMean = grandSum / double(N);
        double ssb = 0.0, ssw = 0.0;
        for (const auto &gv : groups) {
            ssb += double(gv.vals.size()) * (gv.mean - grandMean) * (gv.mean - grandMean);
            for (double v : gv.vals)
                ssw += (v - gv.mean) * (v - gv.mean);
        }
        const double msw = (df2 > 0) ? (ssw / double(df2)) : 0.0;
        double fStat = 0.0, pVal = 1.0;
        if (df1 > 0 && df2 > 0) {
            if (ssw > 0.0) {
                fStat = (ssb / double(df1)) / msw;
                pVal = gsl_cdf_fdist_Q(fStat, double(df1), double(df2));
            } else if (ssb > 0.0) {
                fStat = std::numeric_limits<double>::infinity();
                pVal = 0.0;
            } else {
                fStat = 0.0;
                pVal = 1.0;
            }
        }

        // Satterthwaite denominator sum(v^2 / df) of the Welch / Games-Howell variance va + vb.
        // A group of one value has no variance of its own and takes the pooled within-group
        // variance (N - k degrees of freedom). When both groups have one value they share that
        // one estimate, so its two parts are a single term with N - k degrees of freedom: as two
        // independent terms they gave 2 (N - k) and too small a p (A = [10], B = [13],
        // C = [1..5]: df 8, p 0.217 instead of the pooled t with df 4, p 0.251; at t = 4,
        // p 0.0039 instead of 0.016).
        auto welchSatterthwaiteDenom = [](double va, double na, double vb, double nb,
                                          double dfPool) -> double {
            const double vOwnA = (na > 1.0) ? va : 0.0;
            const double vOwnB = (nb > 1.0) ? vb : 0.0;
            const double vPool = va + vb - vOwnA - vOwnB;
            return ((na > 1.0) ? vOwnA * vOwnA / (na - 1.0) : 0.0)
                    + ((nb > 1.0) ? vOwnB * vOwnB / (nb - 1.0) : 0.0)
                    + ((vPool > 0.0 && dfPool > 0.0) ? vPool * vPool / dfPool : 0.0);
        };

        // Studentized Range upper-tail probability P(Q_{k, nu} >= q) via Hartley-Tukey quadrature
        auto studentizedRangeP = [](double q, int k, double nu) -> double {
            if (!(q > 0.0) || k < 2 || !(nu > 0.0))
                return 1.0;
            auto normalRangeCdf = [](double w, int m) -> double {
                if (w <= 0.0)
                    return 0.0;
                constexpr int kStepsZ = 128;
                constexpr double zLo = -8.5, zHi = 8.5;
                const double hz = (zHi - zLo) / double(kStepsZ);
                constexpr double invSqrt2Pi = 0.39894228040143267794;
                double acc = 0.0;
                for (int iz = 0; iz < kStepsZ; ++iz) {
                    const double z = zLo + (double(iz) + 0.5) * hz;
                    const double phi = invSqrt2Pi * std::exp(-0.5 * z * z);
                    const double diff =
                            std::max(0.0, gsl_cdf_ugaussian_P(z + w) - gsl_cdf_ugaussian_P(z));
                    acc += phi * std::pow(diff, m - 1);
                }
                return std::clamp(double(m) * acc * hz, 0.0, 1.0);
            };
            const double halfNu = 0.5 * nu;
            const double logConst = std::log(2.0) + halfNu * std::log(halfNu) - gsl_sf_lngamma(halfNu);
            const double sdU = 1.0 / std::sqrt(2.0 * nu);
            const double uLo = std::max(1e-4, 1.0 - 7.5 * sdU);
            const double uHi = 1.0 + 7.5 * sdU;
            constexpr int kStepsU = 72;
            const double hu = (uHi - uLo) / double(kStepsU);
            double cdf = 0.0;
            double wNorm = 0.0;
            for (int iu = 0; iu < kStepsU; ++iu) {
                const double u = uLo + (double(iu) + 0.5) * hu;
                const double fu = std::exp(logConst + (nu - 1.0) * std::log(u) - halfNu * u * u);
                wNorm += fu * hu;
                cdf += fu * normalRangeCdf(q * u, k) * hu;
            }
            if (wNorm > 0.0)
                cdf /= wNorm;
            return std::clamp(1.0 - cdf, 0.0, 1.0);
        };

        // Exact two-sided Dunnett upper-tail probability for common control n0 vs treatment sizes trtSizes
        auto dunnettTwoSidedP = [](double tAbs, double nu, int n0,
                                   const std::vector<int> &trtSizes) -> double {
            if (!(tAbs > 0.0) || trtSizes.empty() || !(nu > 0.0) || n0 < 1)
                return 1.0;
            const double halfNu = 0.5 * nu;
            const double logConst = std::log(2.0) + halfNu * std::log(halfNu) - gsl_sf_lngamma(halfNu);
            const double sdU = 1.0 / std::sqrt(2.0 * nu);
            const double uLo = std::max(1e-4, 1.0 - 7.5 * sdU);
            const double uHi = 1.0 + 7.5 * sdU;
            constexpr int kStepsU = 56;
            constexpr int kStepsZ = 96;
            constexpr double zLo = -8.0, zHi = 8.0;
            const double hu = (uHi - uLo) / double(kStepsU);
            const double hz = (zHi - zLo) / double(kStepsZ);
            constexpr double invSqrt2Pi = 0.39894228040143267794;
            const double sqrtN0 = std::sqrt(double(n0));
            double cdf = 0.0;
            double wNorm = 0.0;
            for (int iu = 0; iu < kStepsU; ++iu) {
                const double u = uLo + (double(iu) + 0.5) * hu;
                const double fu = std::exp(logConst + (nu - 1.0) * std::log(u) - halfNu * u * u);
                wNorm += fu * hu;
                double inner = 0.0;
                for (int iz = 0; iz < kStepsZ; ++iz) {
                    const double z0 = zLo + (double(iz) + 0.5) * hz;
                    const double phi0 = invSqrt2Pi * std::exp(-0.5 * z0 * z0);
                    double prod = 1.0;
                    for (int nj : trtSizes) {
                        const double shift = std::sqrt(double(nj)) * z0 / sqrtN0;
                        const double halfSpan =
                                tAbs * u * std::sqrt(double(nj + n0)) / sqrtN0;
                        const double pr = std::max(
                                0.0,
                                gsl_cdf_ugaussian_P(shift + halfSpan)
                                        - gsl_cdf_ugaussian_P(shift - halfSpan));
                        prod *= pr;
                    }
                    inner += phi0 * prod * hz;
                }
                cdf += fu * inner * hu;
            }
            if (wNorm > 0.0)
                cdf /= wNorm;
            return std::clamp(1.0 - cdf, 0.0, 1.0);
        };

        struct PairwiseCompRow
        {
            QString a;
            QString b;
            double meanDiff = 0.0;
            double se = 0.0;
            double stat = 0.0;
            double df = 1.0;
            double pRaw = 1.0;
            double pAdj = 1.0;
        };
        std::vector<PairwiseCompRow> comps;

        if (canonicalPosthoc == QStringLiteral("dunnett")) {
            std::vector<int> trtSizes;
            for (int gi = 0; gi < kGroups; ++gi) {
                if (gi != controlIdx)
                    trtSizes.push_back(static_cast<int>(groups[size_t(gi)].vals.size()));
            }
            const auto &ctrlG = groups[size_t(controlIdx)];
            const int n0 = static_cast<int>(ctrlG.vals.size());
            for (int gi = 0; gi < kGroups; ++gi) {
                if (gi == controlIdx)
                    continue;
                const auto &trtG = groups[size_t(gi)];
                const int ni = static_cast<int>(trtG.vals.size());
                const double diff = trtG.mean - ctrlG.mean;
                const double se = (msw > 0.0)
                        ? std::sqrt(msw * (1.0 / double(ni) + 1.0 / double(n0)))
                        : 0.0;
                const double tStat = (se > 0.0) ? (std::abs(diff) / se) : 0.0;
                const double pRaw = (se > 0.0 && df2 > 0)
                        ? (2.0 * gsl_cdf_tdist_Q(tStat, double(df2)))
                        : ((diff == 0.0) ? 1.0 : 0.0);
                const double pAdj = (se > 0.0 && df2 > 0)
                        ? dunnettTwoSidedP(tStat, double(df2), n0, trtSizes)
                        : pRaw;
                comps.push_back({ trtG.label, ctrlG.label, diff, se, tStat, double(df2), pRaw,
                                  pAdj });
            }
        } else {
            for (int i = 0; i < kGroups; ++i) {
                for (int j = i + 1; j < kGroups; ++j) {
                    const auto &ga = groups[size_t(i)];
                    const auto &gb = groups[size_t(j)];
                    const double na = double(ga.vals.size());
                    const double nb = double(gb.vals.size());
                    const double diff = ga.mean - gb.mean;
                    if (canonicalPosthoc == QStringLiteral("tukey")) {
                        const double seQ = (msw > 0.0)
                                ? std::sqrt(0.5 * msw * (1.0 / na + 1.0 / nb))
                                : 0.0;
                        const double qStat = (seQ > 0.0) ? (std::abs(diff) / seQ) : 0.0;
                        const double tStat = qStat / std::sqrt(2.0);
                        const double pRaw = (seQ > 0.0 && df2 > 0)
                                ? (2.0 * gsl_cdf_tdist_Q(tStat, double(df2)))
                                : ((diff == 0.0) ? 1.0 : 0.0);
                        const double pAdj = (seQ > 0.0 && df2 > 0)
                                ? studentizedRangeP(qStat, kGroups, double(df2))
                                : pRaw;
                        comps.push_back({ ga.label, gb.label, diff, seQ, qStat, double(df2), pRaw,
                                          pAdj });
                    } else if (canonicalPosthoc == QStringLiteral("games_howell")) {
                        // A group of one value: pooled within-group variance (see
                        // welchSatterthwaiteDenom for its degrees of freedom).
                        const double va = ((na > 1.0) ? ga.var : msw) / na;
                        const double vb = ((nb > 1.0) ? gb.var : msw) / nb;
                        const double vSum = va + vb;
                        const double denom = welchSatterthwaiteDenom(va, na, vb, nb, double(df2));
                        const double seQ = (vSum > 0.0) ? std::sqrt(0.5 * vSum) : 0.0;
                        const double dfW = (denom > 0.0) ? ((vSum * vSum) / denom) : (na + nb - 2.0);
                        const double qStat = (seQ > 0.0) ? (std::abs(diff) / seQ) : 0.0;
                        const double tStat = qStat / std::sqrt(2.0);
                        const double pRaw = (seQ > 0.0 && dfW > 0.0)
                                ? (2.0 * gsl_cdf_tdist_Q(tStat, dfW))
                                : ((diff == 0.0) ? 1.0 : 0.0);
                        const double pAdj = (seQ > 0.0 && dfW > 0.0)
                                ? studentizedRangeP(qStat, kGroups, dfW)
                                : pRaw;
                        comps.push_back({ ga.label, gb.label, diff, seQ, qStat, dfW, pRaw, pAdj });
                    } else {
                        // Welch t-test + Holm-Bonferroni step-down adjustment (a group of one
                        // value: pooled within-group variance, N - k degrees of freedom)
                        const double va = ((na > 1.0) ? ga.var : msw) / na;
                        const double vb = ((nb > 1.0) ? gb.var : msw) / nb;
                        const double vSum = va + vb;
                        const double denom = welchSatterthwaiteDenom(va, na, vb, nb, double(df2));
                        const double seW = (vSum > 0.0) ? std::sqrt(vSum) : 0.0;
                        const double dfW = (denom > 0.0) ? ((vSum * vSum) / denom) : (na + nb - 2.0);
                        const double tStat = (seW > 0.0) ? (std::abs(diff) / seW) : 0.0;
                        const double pRaw = (seW > 0.0 && dfW > 0.0)
                                ? (2.0 * gsl_cdf_tdist_Q(tStat, dfW))
                                : ((diff == 0.0) ? 1.0 : 0.0);
                        comps.push_back({ ga.label, gb.label, diff, seW, tStat, dfW, pRaw, pRaw });
                    }
                }
            }
            if (canonicalPosthoc == QStringLiteral("holm") && !comps.empty()) {
                std::vector<size_t> order(comps.size());
                std::iota(order.begin(), order.end(), 0);
                std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                    return comps[a].pRaw < comps[b].pRaw;
                });
                const size_t m = comps.size();
                double runningMax = 0.0;
                for (size_t rank = 0; rank < m; ++rank) {
                    const size_t idx = order[rank];
                    runningMax = std::max(runningMax,
                                          std::min(1.0, comps[idx].pRaw * double(m - rank)));
                    comps[idx].pAdj = runningMax;
                }
            }
        }

        QJsonArray compArr;
        for (const auto &cRow : comps) {
            QJsonObject cObj;
            cObj[QStringLiteral("group_a")] = cRow.a;
            cObj[QStringLiteral("group_b")] = cRow.b;
            cObj[QStringLiteral("mean_diff")] = safeJsonDouble(cRow.meanDiff);
            cObj[QStringLiteral("se")] = safeJsonDouble(cRow.se);
            cObj[QStringLiteral("statistic")] = safeJsonDouble(cRow.stat);
            cObj[QStringLiteral("df")] = safeJsonDouble(cRow.df);
            cObj[QStringLiteral("p_raw")] = safeJsonDouble(cRow.pRaw);
            cObj[QStringLiteral("p_adj")] = safeJsonDouble(cRow.pAdj);
            cObj[QStringLiteral("p_value")] = safeJsonDouble(cRow.pAdj);
            cObj[QStringLiteral("significant")] = (cRow.pAdj < 0.05);
            compArr.append(cObj);
        }

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("posthoc")] = canonicalPosthoc;
        if (canonicalPosthoc == QStringLiteral("dunnett"))
            outResult[QStringLiteral("control")] = controlLabel;
        outResult[QStringLiteral("f_statistic")] = safeJsonDouble(fStat);
        outResult[QStringLiteral("p_value")] = safeJsonDouble(pVal);
        outResult[QStringLiteral("df_between")] = df1;
        outResult[QStringLiteral("df_within")] = df2;
        if (!singletonGroups.isEmpty()) {
            outResult[QStringLiteral("notice")] =
                    QStringLiteral("Group(s) %1 have a single value: they enter the ANOVA (N - k) "
                                   "but not the within-group variance%2.")
                            .arg(singletonGroups.join(QStringLiteral(", ")))
                            .arg((canonicalPosthoc == QStringLiteral("holm")
                                  || canonicalPosthoc == QStringLiteral("games_howell"))
                                         ? QStringLiteral("; in the Welch / Games-Howell "
                                                          "comparisons their variance is the "
                                                          "pooled within-group variance")
                                         : QString());
        }
        outResult[QStringLiteral("comparisons")] = compArr;
        outResult[QStringLiteral("table")] = repTbl->name();
        outResult[QStringLiteral("report")] = extractTableAsJson(repTbl);
        return true;
    }

    if (method == QStringLiteral("stats.anova_twoway")) {
        Table *t = resolveTable(d_app, params);
        const QJsonValue fAVal = params.contains(QStringLiteral("factor_a"))
                ? params.value(QStringLiteral("factor_a"))
                : (params.contains(QStringLiteral("factor_a_col"))
                           ? params.value(QStringLiteral("factor_a_col"))
                           : QJsonValue(0));
        const QJsonValue fBVal = params.contains(QStringLiteral("factor_b"))
                ? params.value(QStringLiteral("factor_b"))
                : (params.contains(QStringLiteral("factor_b_col"))
                           ? params.value(QStringLiteral("factor_b_col"))
                           : QJsonValue(1));
        const QJsonValue respVal = params.contains(QStringLiteral("response"))
                ? params.value(QStringLiteral("response"))
                : (params.contains(QStringLiteral("response_col"))
                           ? params.value(QStringLiteral("response_col"))
                           : QJsonValue(2));
        const QString fA = t->colLabel(resolveColumnIndex(t, fAVal, true));
        const QString fB = t->colLabel(resolveColumnIndex(t, fBVal, true));
        const QString resp = t->colLabel(resolveColumnIndex(t, respVal, true));
        const bool plotInt = params.contains(QStringLiteral("plot"))
                ? optionalJsonBool(params, QStringLiteral("plot"), true)
                : true;
        const auto tw = GraphStatsAndSurfaceFit::runTwoWayANOVA(d_app, t, fA, fB, resp, plotInt);
        if (tw.interactionGraph) {
            for (Graph *lg : tw.interactionGraph->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = tw.valid;
        outResult[QStringLiteral("f_factor_a")] = safeJsonDouble(tw.fFactorA);
        outResult[QStringLiteral("p_factor_a")] = safeJsonDouble(tw.pFactorA);
        outResult[QStringLiteral("f_factor_b")] = safeJsonDouble(tw.fFactorB);
        outResult[QStringLiteral("p_factor_b")] = safeJsonDouble(tw.pFactorB);
        outResult[QStringLiteral("f_interaction")] = safeJsonDouble(tw.fInteraction);
        outResult[QStringLiteral("p_interaction")] = safeJsonDouble(tw.pInteraction);
        if (tw.reportTable)
            outResult[QStringLiteral("table")] = tw.reportTable->name();
        if (tw.interactionGraph)
            outResult[QStringLiteral("graph")] = tw.interactionGraph->name();
        return true;
    }

    if (method == QStringLiteral("stats.nonparametric")) {
        Table *t = resolveTable(d_app, params);
        QStringList colNames;
        QVector<int> colIdxs;
        if (params.value(QStringLiteral("cols")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("cols")).toArray()) {
                const int ci = resolveColumnIndex(t, cv, true);
                colIdxs.append(ci);
                colNames << t->colLabel(ci);
            }
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                colIdxs.append(c);
                colNames << t->colLabel(c);
            }
        }
        const QString mStr = (params.contains(QStringLiteral("method"))
                                      ? optionalJsonString(params, QStringLiteral("method"))
                                      : optionalJsonString(params, QStringLiteral("test")))
                                     .trimmed()
                                     .toLower();
        if (mStr == QStringLiteral("friedman")) {
            // Exact Friedman repeated-measures rank test across k >= 3 columns (with tie correction)
            const int k = colIdxs.size();
            if (k < 3)
                throwInvalidParams(QStringLiteral("Friedman test requires >= 3 columns."));
            std::vector<double> rankSums(size_t(k), 0.0);
            int nBlocks = 0;
            double tieSum = 0.0;
            for (int r = 0; r < t->numRows(); ++r) {
                std::vector<std::pair<double, int>> rowVals;
                rowVals.reserve(size_t(k));
                for (int j = 0; j < k; ++j) {
                    Column *cPtr = t->column(colIdxs[j]);
                    if (!cPtr || cPtr->isInvalid(r) || cPtr->isMasked(r))
                        break;
                    const double v = t->cell(r, colIdxs[j]);
                    if (std::isfinite(v))
                        rowVals.push_back({ v, j });
                }
                if (int(rowVals.size()) != k)
                    continue;
                std::sort(rowVals.begin(), rowVals.end());
                for (int j = 0; j < k;) {
                    int e = j;
                    while (e + 1 < k && rowVals[size_t(e + 1)].first == rowVals[size_t(j)].first)
                        ++e;
                    const int tieCount = e - j + 1;
                    if (tieCount > 1)
                        tieSum += double(tieCount) * double(tieCount * tieCount - 1);
                    const double avgRank = 0.5 * double(j + e) + 1.0;
                    for (int m = j; m <= e; ++m)
                        rankSums[size_t(rowVals[size_t(m)].second)] += avgRank;
                    j = e + 1;
                }
                ++nBlocks;
            }
            if (nBlocks < 2)
                throwInvalidParams(
                        QStringLiteral("Friedman test requires >= 2 complete unmasked rows."));
            double sumR2 = 0.0;
            for (double rs : rankSums)
                sumR2 += rs * rs;
            const double rawQ = (12.0 / (double(nBlocks) * double(k) * double(k + 1))) * sumR2
                    - 3.0 * double(nBlocks) * double(k + 1);
            const double tieFactor =
                    1.0 - tieSum / (double(nBlocks) * double(k) * double(k * k - 1));
            const double qStat = (tieFactor > 1e-12) ? std::max(0.0, rawQ / tieFactor) : 0.0;
            const double pVal =
                    (tieFactor > 1e-12) ? gsl_cdf_chisq_Q(qStat, double(k - 1)) : 1.0;
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("test")] = QStringLiteral("Friedman");
            outResult[QStringLiteral("n_blocks")] = nBlocks;
            outResult[QStringLiteral("statistic")] = safeJsonDouble(qStat);
            outResult[QStringLiteral("p_value")] = safeJsonDouble(pVal);
            return true;
        }

        auto nm = GraphStatsAndSurfaceFit::TestAutoSmartSwitch;
        if (mStr.isEmpty() || mStr == QStringLiteral("auto"))
            nm = GraphStatsAndSurfaceFit::TestAutoSmartSwitch;
        else if (mStr == QStringLiteral("mann_whitney") || mStr == QStringLiteral("mann_whitney_u"))
            nm = GraphStatsAndSurfaceFit::TestMannWhitneyU;
        else if (mStr == QStringLiteral("wilcoxon")
                 || mStr == QStringLiteral("wilcoxon_signed_rank"))
            nm = GraphStatsAndSurfaceFit::TestWilcoxonSignedRank;
        else if (mStr == QStringLiteral("kruskal_wallis") || mStr == QStringLiteral("kruskal"))
            nm = GraphStatsAndSurfaceFit::TestKruskalWallisDunn;
        else
            throwInvalidParams(
                    QStringLiteral("Unknown nonparametric test '%1' (expected 'mann_whitney'|'wilcoxon'|'kruskal_wallis'|'friedman').")
                            .arg(mStr));

        MultiLayer *annMl = nullptr;
        Graph *annG = nullptr;
        const bool hasGraphParam = params.contains(QStringLiteral("graph"))
                && !params.value(QStringLiteral("graph")).isNull()
                && !optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty();
        const bool wantBrackets = params.contains(QStringLiteral("add_brackets"))
                && !params.value(QStringLiteral("add_brackets")).isNull()
                ? optionalJsonBool(params, QStringLiteral("add_brackets"), hasGraphParam)
                : hasGraphParam;
        if (wantBrackets) {
            annMl = hasGraphParam
                    ? resolveMultiLayer(d_app, params, QStringLiteral("graph"), true)
                    : resolveMultiLayer(d_app, params, QStringLiteral("graph"), false);
            if (annMl)
                annG = resolveGraphLayer(annMl, params, false);
        }
        std::unique_ptr<ScopedGraphUndoGuard> npUndoGuard;
        if (annMl && annG) {
            npUndoGuard = std::make_unique<ScopedGraphUndoGuard>(
                    annMl, QStringLiteral("Nonparametric Brackets"));
        }
        const auto np = GraphStatsAndSurfaceFit::runNonparametricComparison(d_app, t, colNames, nm,
                                                                            annG);
        if (annG) {
            enforceArialGraphFonts(annG);
            replotGraphOrDefer(annG);
            if (npUndoGuard)
                npUndoGuard->commit();
        }
        outResult[QStringLiteral("ok")] = np.valid;
        outResult[QStringLiteral("test")] = np.chosenTestName;
        if (!np.valid) {
            outResult[QStringLiteral("message")] = np.message.isEmpty()
                    ? QStringLiteral("The nonparametric test could not be run on these columns.")
                    : np.message;
        }
        outResult[QStringLiteral("statistic")] = safeJsonDouble(np.statistic);
        outResult[QStringLiteral("p_value")] = safeJsonDouble(np.pValue);
        if (annMl && annG)
            outResult[QStringLiteral("annotated_graph")] = annMl->name();
        if (np.reportTable) {
            outResult[QStringLiteral("table")] = np.reportTable->name();
            outResult[QStringLiteral("report")] = extractTableAsJson(np.reportTable);
        }
        return true;
    }

    if (method == QStringLiteral("stats.paired_plot")) {
        Table *t = resolveTable(d_app, params);
        const QJsonValue bKey = params.contains(QStringLiteral("before_col"))
                ? params.value(QStringLiteral("before_col"))
                : (params.contains(QStringLiteral("col_a"))
                           ? params.value(QStringLiteral("col_a"))
                           : QJsonValue(0));
        const QJsonValue aKey = params.contains(QStringLiteral("after_col"))
                ? params.value(QStringLiteral("after_col"))
                : (params.contains(QStringLiteral("col_b"))
                           ? params.value(QStringLiteral("col_b"))
                           : QJsonValue(1));
        const QString bCol = t->colLabel(resolveColumnIndex(t, bKey, true));
        const QString aCol = t->colLabel(resolveColumnIndex(t, aKey, true));
        double pVal = 1.0;
        MultiLayer *ml = GraphStatsAndSurfaceFit::plotPairedComparison(
                d_app, t, bCol, aCol, optionalJsonBool(params, QStringLiteral("wilcoxon"), false),
                &pVal);
        if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = (ml != nullptr);
        outResult[QStringLiteral("p_value")] = safeJsonDouble(pVal);
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("stats.bland_altman")) {
        Table *t = resolveTable(d_app, params);
        const QString aCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("col_a"))
                                           ? params.value(QStringLiteral("col_a"))
                                           : QJsonValue(0),
                                   true));
        const QString bCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("col_b"))
                                           ? params.value(QStringLiteral("col_b"))
                                           : QJsonValue(1),
                                   true));
        double bias = 0.0, loaLo = 0.0, loaHi = 0.0;
        MultiLayer *ml = GraphStatsAndSurfaceFit::plotBlandAltman(d_app, t, aCol, bCol, &bias,
                                                                  &loaLo, &loaHi);
        if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = (ml != nullptr);
        outResult[QStringLiteral("mean_bias")] = safeJsonDouble(bias);
        outResult[QStringLiteral("loa_lower")] = safeJsonDouble(loaLo);
        outResult[QStringLiteral("loa_upper")] = safeJsonDouble(loaHi);
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("stats.passing_bablok")) {
        Table *t = resolveTable(d_app, params);
        const QString xCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("x_col"))
                                           ? params.value(QStringLiteral("x_col"))
                                           : (params.contains(QStringLiteral("col_a"))
                                                      ? params.value(QStringLiteral("col_a"))
                                                      : QJsonValue(0)),
                                   true));
        const QString yCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("y_col"))
                                           ? params.value(QStringLiteral("y_col"))
                                           : (params.contains(QStringLiteral("col_b"))
                                                      ? params.value(QStringLiteral("col_b"))
                                                      : QJsonValue(1)),
                                   true));
        const double lam = params.value(QStringLiteral("lambda")).toDouble(1.0);
        const bool createPlot = params.contains(QStringLiteral("plot"))
                ? params.value(QStringLiteral("plot")).toBool(true)
                : true;
        const auto pb = GraphStatsAndSurfaceFit::runPassingBablokDeming(d_app, t, xCol, yCol, lam,
                                                                        createPlot);
        if (pb.comparisonPlot) {
            for (Graph *lg : pb.comparisonPlot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = pb.valid;
        outResult[QStringLiteral("n")] = pb.n;
        outResult[QStringLiteral("slope")] = safeJsonDouble(pb.pbSlope);
        outResult[QStringLiteral("slope_ci_low")] = safeJsonDouble(pb.pbSlopeCILow);
        outResult[QStringLiteral("slope_ci_high")] = safeJsonDouble(pb.pbSlopeCIHigh);
        outResult[QStringLiteral("intercept")] = safeJsonDouble(pb.pbIntercept);
        outResult[QStringLiteral("intercept_ci_low")] = safeJsonDouble(pb.pbInterceptCILow);
        outResult[QStringLiteral("intercept_ci_high")] = safeJsonDouble(pb.pbInterceptCIHigh);
        outResult[QStringLiteral("deming_slope")] = safeJsonDouble(pb.demingSlope);
        outResult[QStringLiteral("deming_intercept")] = safeJsonDouble(pb.demingIntercept);
        outResult[QStringLiteral("cusum_stat")] = safeJsonDouble(pb.cusumStat);
        outResult[QStringLiteral("cusum_p")] = safeJsonDouble(pb.cusumLinearityP);
        if (pb.summaryTable)
            outResult[QStringLiteral("table")] = pb.summaryTable->name();
        if (pb.comparisonPlot)
            outResult[QStringLiteral("graph")] = pb.comparisonPlot->name();
        return true;
    }

    if (method == QStringLiteral("stats.weibull")) {
        Table *t = resolveTable(d_app, params);
        const int dColIdx = resolveColumnIndex(
                t, params.contains(QStringLiteral("col"))
                           ? params.value(QStringLiteral("col"))
                           : QJsonValue(0),
                true);
        const QString dCol = t->colLabel(dColIdx);
        const bool threeP = optionalJsonBool(params, QStringLiteral("three_param"), false);
        const int minWeibullRows = threeP ? 3 : 2;
        int posFinite = 0;
        if (Column *dc = t->column(dColIdx)) {
            if (dc->columnMode() == SciDAVis::ColumnMode::Numeric) {
                for (int r = 0; r < t->numRows(); ++r) {
                    if (dc->isInvalid(r) || dc->isMasked(r))
                        continue;
                    const double v = dc->valueAt(r);
                    if (std::isfinite(v) && v > 0.0)
                        ++posFinite;
                }
            }
        }
        if (posFinite < minWeibullRows) {
            throwInvalidParams(
                    QStringLiteral("stats.weibull requires at least %1 positive finite observations in column '%2' (got %3).")
                            .arg(minWeibullRows)
                            .arg(dCol)
                            .arg(posFinite));
        }
        QString cCol;
        if (params.contains(QStringLiteral("censor_col"))
            && !params.value(QStringLiteral("censor_col")).isNull()) {
            cCol = t->colLabel(
                    resolveColumnIndex(t, params.value(QStringLiteral("censor_col")), true));
        }
        const int censorCoding =
                parseBoundedInt(params, QStringLiteral("censor_coding"), 0, 0, 10);
        const bool useMle = optionalJsonBool(
                params, QStringLiteral("mle"),
                optionalJsonBool(params, QStringLiteral("use_mle"), false));
        const bool wantPlot = optionalJsonBool(params, QStringLiteral("plot"), true);
        const auto wb = GraphStatsAndSurfaceFit::plotWeibullProbability(
                d_app, t, dCol, cCol, censorCoding, threeP, useMle, wantPlot);
        MultiLayer *wbMl = wb.weibullPlot;
        if (wbMl && !wantPlot) {
            closeAndDeleteEmptyPlot(d_app, wbMl);
            wbMl = nullptr;
        } else if (wbMl) {
            for (Graph *lg : wbMl->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = wb.valid;
        outResult[QStringLiteral("beta")] = safeJsonDouble(wb.modulusM);
        outResult[QStringLiteral("modulus_m")] = safeJsonDouble(wb.modulusM);
        outResult[QStringLiteral("beta_se")] = safeJsonDouble(wb.modulusMSE);
        outResult[QStringLiteral("eta")] = safeJsonDouble(wb.scaleSigma0);
        outResult[QStringLiteral("scale_sigma0")] = safeJsonDouble(wb.scaleSigma0);
        outResult[QStringLiteral("eta_se")] = safeJsonDouble(wb.scaleSigma0SE);
        outResult[QStringLiteral("used_mle")] = wb.usedMle;
        outResult[QStringLiteral("mle_beta")] = safeJsonDouble(wb.mleModulusM);
        outResult[QStringLiteral("mle_modulus_m")] = safeJsonDouble(wb.mleModulusM);
        outResult[QStringLiteral("mle_beta_se")] = safeJsonDouble(wb.mleModulusMSE);
        outResult[QStringLiteral("mle_eta")] = safeJsonDouble(wb.mleScaleSigma0);
        outResult[QStringLiteral("mle_scale_sigma0")] = safeJsonDouble(wb.mleScaleSigma0);
        outResult[QStringLiteral("mle_eta_se")] = safeJsonDouble(wb.mleScaleSigma0SE);
        outResult[QStringLiteral("gamma")] = safeJsonDouble(wb.thresholdGamma);
        outResult[QStringLiteral("three_param")] = wb.threeParameter;
        outResult[QStringLiteral("censored_count")] = wb.censoredCount;
        outResult[QStringLiteral("b10")] = safeJsonDouble(wb.b10Life);
        outResult[QStringLiteral("b50")] = safeJsonDouble(wb.b50Life);
        outResult[QStringLiteral("r2")] = safeJsonDouble(wb.rSquare);
        if (wb.reportTable)
            outResult[QStringLiteral("table")] = wb.reportTable->name();
        if (wbMl)
            outResult[QStringLiteral("graph")] = wbMl->name();
        return true;
    }

    if (method == QStringLiteral("stats.probability_plot")) {
        Table *t = resolveTable(d_app, params);
        const int cIdx = resolveColumnIndex(
                t, params.contains(QStringLiteral("col")) ? params.value(QStringLiteral("col"))
                                                          : QJsonValue(0),
                true);
        const QString distStr =
                optionalJsonString(params, QStringLiteral("distribution"), QStringLiteral("lognormal"))
                        .trimmed()
                        .toLower();
        ExtraPlotTypes::ProbabilityDistribution dist = ExtraPlotTypes::LogNormal;
        if (distStr == QStringLiteral("gumbel") || distStr == QStringLiteral("extreme")
            || distStr == QStringLiteral("gev") || distStr == QStringLiteral("evd"))
            dist = ExtraPlotTypes::Gumbel;
        else if (distStr != QStringLiteral("lognormal") && distStr != QStringLiteral("log-normal"))
            throwInvalidParams(QStringLiteral("Unknown distribution '%1' (expected lognormal|gumbel).")
                                       .arg(distStr));
        const QString posStr = optionalJsonString(params, QStringLiteral("position")).trimmed().toLower();
        ExtraPlotTypes::PlottingPosition pos =
                dist == ExtraPlotTypes::Gumbel ? ExtraPlotTypes::MeanRank : ExtraPlotTypes::MedianRank;
        if (posStr == QStringLiteral("median"))
            pos = ExtraPlotTypes::MedianRank;
        else if (posStr == QStringLiteral("mean"))
            pos = ExtraPlotTypes::MeanRank;
        else if (posStr == QStringLiteral("hazen"))
            pos = ExtraPlotTypes::Hazen;
        else if (posStr == QStringLiteral("blom"))
            pos = ExtraPlotTypes::Blom;
        else if (!posStr.isEmpty())
            throwInvalidParams(QStringLiteral("Unknown position '%1' (expected median|mean|hazen|blom).")
                                       .arg(posStr));
        const double T = optionalJsonFiniteDouble(params, QStringLiteral("return_period"), 0.0);
        const bool wantPlot = optionalJsonBool(params, QStringLiteral("plot"), true);
        const auto pr = ExtraPlotTypes::plotProbability(d_app, t, t->colName(cIdx), dist, pos, T,
                                                        wantPlot);
        if (!pr.valid)
            throwInvalidParams(pr.error);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("n")] = pr.n;
        outResult[QStringLiteral("excluded")] = pr.excluded;
        outResult[QStringLiteral("location")] = safeJsonDouble(pr.location);
        outResult[QStringLiteral("scale")] = safeJsonDouble(pr.scale);
        if (dist == ExtraPlotTypes::LogNormal) {
            outResult[QStringLiteral("mu")] = safeJsonDouble(pr.location);
            outResult[QStringLiteral("sigma")] = safeJsonDouble(pr.scale);
            outResult[QStringLiteral("median")] = safeJsonDouble(std::exp(pr.location));
        } else {
            outResult[QStringLiteral("lambda")] = safeJsonDouble(pr.location);
            outResult[QStringLiteral("delta")] = safeJsonDouble(pr.scale);
            if (pr.returnPeriod > 1.0)
                outResult[QStringLiteral("return_value")] = safeJsonDouble(pr.returnValue);
        }
        outResult[QStringLiteral("r2")] = safeJsonDouble(pr.rSquare);
        if (pr.table)
            outResult[QStringLiteral("table")] = pr.table->name();
        if (pr.plot)
            outResult[QStringLiteral("graph")] = pr.plot->name();
        return true;
    }

    if (method == QStringLiteral("plot.color_scatter")) {
        Table *t = resolveTable(d_app, params);
        auto colIdxOf = [&](const char *key, int def) {
            const QString k = QString::fromLatin1(key);
            return resolveColumnIndex(
                    t, params.contains(k) ? params.value(k) : QJsonValue(def), true);
        };
        const int xIdx = colIdxOf("x", 0), yIdx = colIdxOf("y", 1), zIdx = colIdxOf("z", 2);
        const QString xc = t->colName(xIdx), yc = t->colName(yIdx), zc = t->colName(zIdx);
        QString sc;
        if (params.contains(QStringLiteral("size")) && !params.value(QStringLiteral("size")).isNull())
            sc = t->colName(resolveColumnIndex(t, params.value(QStringLiteral("size")), true));
        const QString pal = optionalJsonString(params, QStringLiteral("palette"), QStringLiteral("viridis"));
        const Spectrogram::ColorMapPreset preset = parseColorMapPreset(pal, nullptr, true);
        const bool rev = optionalJsonBool(params, QStringLiteral("reverse"), false);
        const bool attachToExisting =
                !optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty();

        bool hasZRange = false;
        double parsedZMin = 0.0;
        double parsedZMax = 1.0;
        if (params.contains(QStringLiteral("z_min")) && params.contains(QStringLiteral("z_max"))
            && !params.value(QStringLiteral("z_min")).isNull()
            && !params.value(QStringLiteral("z_max")).isNull()) {
            const double zLo = parseFiniteCoordDouble(params.value(QStringLiteral("z_min")),
                                                      QStringLiteral("z_min"));
            const double zHi = parseFiniteCoordDouble(params.value(QStringLiteral("z_max")),
                                                      QStringLiteral("z_max"));
            validateFiniteCoordRange(zLo, zHi, QStringLiteral("Colorbar [z_min, z_max]"));
            parsedZMin = std::min(zLo, zHi);
            parsedZMax = std::max(zLo, zHi);
            hasZRange = true;
        }
        const bool hasColorBarTitle = params.contains(QStringLiteral("colorbar_title"))
                && !params.value(QStringLiteral("colorbar_title")).isNull();
        const QString reqColorBarTitle = hasColorBarTitle
                ? optionalJsonString(params, QStringLiteral("colorbar_title"))
                : QString();
        const bool hasShowColorBar = params.contains(QStringLiteral("show_colorbar"))
                && !params.value(QStringLiteral("show_colorbar")).isNull();
        const bool reqShowColorBar = hasShowColorBar
                ? optionalJsonBool(params, QStringLiteral("show_colorbar"), true)
                : true;

        MultiLayer *ml = nullptr;
        Graph *targetG = nullptr;
        DataCurve *cmDc = nullptr;
        std::unique_ptr<ScopedGraphUndoGuard> undoGuard;
        if (attachToExisting) {
            Column *cx = t->column(xIdx);
            Column *cy = t->column(yIdx);
            Column *cz = t->column(zIdx);
            if (!cx || !cy || !cz
                || cx->columnMode() != SciDAVis::ColumnMode::Numeric
                || cy->columnMode() != SciDAVis::ColumnMode::Numeric
                || cz->columnMode() != SciDAVis::ColumnMode::Numeric) {
                throwInvalidParams(QStringLiteral("plot.color_scatter needs numeric x, y and z columns."));
            }
            ml = resolveMultiLayer(d_app, params);
            targetG = resolveGraphLayer(ml, params);
            undoGuard = std::make_unique<ScopedGraphUndoGuard>(
                    ml, QObject::tr("Add Color-Mapped Scatter"));
            if (!targetG->insertCurve(t, xc, yc, Graph::Scatter))
                throwRpcRuntime(QStringLiteral("Failed to add color-mapped scatter curve."));
            const int idx = targetG->curves() - 1;
            // 新規作成（ExtraPlotTypes::plotColorMappedScatter）と同じく、Scatter の
            // 既定レイアウト（NoCurve）を当てる。当てないと作成時だけ点が線で結ばれる。
            {
                CurveLayout cl = targetG->initCurveLayout(Graph::Scatter, 1);
                targetG->updateCurveLayout(idx, &cl);
            }
            cmDc = dynamic_cast<DataCurve *>(targetG->curve(idx));
            if (cmDc) {
                QwtSymbol sym = cmDc->symbol();
                if (sym.style() == QwtSymbol::NoSymbol)
                    sym.setStyle(QwtSymbol::Ellipse);
                sym.setSize(QSize(8, 8));
                const QwtLinearColorMap map = Spectrogram::presetColorMap(preset, rev);
                const QColor mid(map.rgb(QwtDoubleInterval(0.0, 1.0), 0.5));
                sym.setBrush(QBrush(mid));
                sym.setPen(QPen(mid.darker(150), 1.0));
                targetG->setCurveSymbol(idx, sym);
                DataCurve::PointColorMap pm;
                pm.enabled = true;
                pm.zColumn = zc;
                pm.sizeColumn = sc;
                pm.preset = int(preset);
                pm.reverse = rev;
                pm.autoRange = true;
                pm.colorBarTitle = cz->longName().trimmed().isEmpty() ? cz->name()
                                                                      : cz->longName().trimmed();
                if (!cz->units().trimmed().isEmpty())
                    pm.colorBarTitle += QStringLiteral(" (%1)").arg(cz->units().trimmed());
                cmDc->setPointColorMap(pm);
            }
            targetG->syncPointColorBar();
            replotGraphOrDefer(targetG);
        } else {
            ml = ExtraPlotTypes::plotColorMappedScatter(d_app, t, xc, yc, zc, sc, int(preset), rev);
            if (!ml)
                throwInvalidParams(QStringLiteral("plot.color_scatter needs numeric x, y and z columns."));
            applyRequestedWindowName(d_app, ml, params);
            targetG = ml->activeGraph();
            cmDc = targetG ? targetG->pointColorMapCurve() : nullptr;
        }
        if (cmDc && targetG) {
            bool pmChanged = false;
            DataCurve::PointColorMap pm = cmDc->pointColorMap();
            if (hasZRange) {
                pm.autoRange = false;
                pm.zMin = parsedZMin;
                pm.zMax = parsedZMax;
                pmChanged = true;
            }
            if (hasColorBarTitle) {
                pm.colorBarTitle = reqColorBarTitle;
                pmChanged = true;
            }
            if (hasShowColorBar) {
                pm.showColorBar = reqShowColorBar;
                pmChanged = true;
            }
            if (pmChanged) {
                cmDc->setPointColorMap(pm);
                targetG->syncPointColorBar();
                replotGraphOrDefer(targetG);
            }
        }
        if (undoGuard)
            undoGuard->commit();
        outResult[QStringLiteral("ok")] = true;
        if (ml) {
            outResult[QStringLiteral("graph")] = ml->name();
            outResult[QStringLiteral("name")] = ml->name();
        }
        if (cmDc) {
            double lo = 0.0, hi = 1.0;
            cmDc->pointColorRange(&lo, &hi);
            outResult[QStringLiteral("z_min")] = safeJsonDouble(lo);
            outResult[QStringLiteral("z_max")] = safeJsonDouble(hi);
        }
        return true;
    }

    if (method == QStringLiteral("plot.histogram_cumulative")) {
        Table *t = resolveTable(d_app, params);
        const int cIdx = resolveColumnIndex(
                t, params.contains(QStringLiteral("col")) ? params.value(QStringLiteral("col"))
                                                          : QJsonValue(0),
                true);
        const auto hr = ExtraPlotTypes::plotHistogramCumulative(
                d_app, t, t->colName(cIdx),
                optionalJsonFiniteDouble(params, QStringLiteral("bin_width"), 0.0),
                optionalJsonFiniteDouble(params, QStringLiteral("begin"),
                                         std::numeric_limits<double>::quiet_NaN()));
        if (!hr.valid)
            throwInvalidParams(hr.error.isEmpty() ? QStringLiteral("Histogram failed.") : hr.error);
        if (hr.plot)
            applyRequestedWindowName(d_app, hr.plot, params);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("bins")] = hr.bins;
        outResult[QStringLiteral("bin_width")] = safeJsonDouble(hr.binWidth);
        outResult[QStringLiteral("total")] = hr.total;
        QJsonArray edges, cum;
        for (int i = 0; i < hr.cumulativePct.size(); ++i) {
            edges.append(safeJsonDouble(hr.upperEdges[i]));
            cum.append(safeJsonDouble(hr.cumulativePct[i]));
        }
        outResult[QStringLiteral("upper_edges")] = edges;
        outResult[QStringLiteral("cumulative_pct")] = cum;
        if (hr.table)
            outResult[QStringLiteral("table")] = hr.table->name();
        if (hr.plot) {
            outResult[QStringLiteral("graph")] = hr.plot->name();
            outResult[QStringLiteral("name")] = hr.plot->name();
        }
        return true;
    }

    if (method == QStringLiteral("graph.add_error_band")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        // curve: Y column name ("Table1_B" or "B") or the curve's index as graph.get_info lists it
        // (the same number as set_curve_style / add_error_bars; it used to count only the data
        // curves of tables, so a function curve or a heatmap before it shifted the number)
        DataCurve *master = nullptr;
        QList<DataCurve *> dataCurves;
        for (int i = 0; i < g->curves(); ++i) {
            if (g->curveType(i) == Graph::ErrorBars)
                continue;
            if (auto *dc = dynamic_cast<DataCurve *>(g->curve(i)))
                if (dc->table())
                    dataCurves << dc;
        }
        const QJsonValue cv = params.value(QStringLiteral("curve"));
        if (cv.isDouble()) {
            const int ci = resolveCurveIndex(g, params, QStringLiteral("curve"), true);
            auto *dc = dynamic_cast<DataCurve *>(g->curve(ci));
            if (!dc || !dc->table() || g->curveType(ci) == Graph::ErrorBars) {
                const QString cTitle = g->curve(ci) ? g->curve(ci)->title().text() : QString();
                throwInvalidParams(
                        QStringLiteral("graph.add_error_band: curve %1 ('%2') is not a data curve "
                                       "of a table.")
                                .arg(ci)
                                .arg(cTitle));
            }
            master = dc;
        } else {
            const QString cn = cv.toString().trimmed();
            for (DataCurve *dc : dataCurves) {
                if (dc->title().text() == cn
                    || dc->title().text() == dc->table()->name() + QStringLiteral("_") + cn) {
                    master = dc;
                    break;
                }
            }
        }
        if (!master)
            throwInvalidParams(QStringLiteral("graph.add_error_band: curve not found."));
        Table *errT = master->table();
        if (!optionalJsonString(params, QStringLiteral("table")).trimmed().isEmpty())
            errT = resolveTable(d_app, params);
        const QString errPlus = errT->colName(
                resolveColumnIndex(errT, params.value(QStringLiteral("err")), true));
        QString errMinus;
        if (params.contains(QStringLiteral("err_minus"))
            && !params.value(QStringLiteral("err_minus")).isNull())
            errMinus = errT->colName(
                    resolveColumnIndex(errT, params.value(QStringLiteral("err_minus")), true));
        int a255 = 64;
        if (params.contains(QStringLiteral("alpha"))) {
            const double rawA = params.value(QStringLiteral("alpha")).toDouble(64.0);
            a255 = (rawA <= 1.0 && rawA >= 0.0) ? int(std::round(rawA * 255.0))
                                                : std::clamp(int(rawA), 0, 255);
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Error Band"));
        const int added = ExtraPlotTypes::addErrorBand(g, master->title().text(), errT, errPlus,
                                                       errMinus, a255);
        if (added == 0)
            throwInvalidParams(QStringLiteral("graph.add_error_band: the band could not be added."));
        replotGraphOrDefer(g);
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("band_curves")] = added;
        outResult[QStringLiteral("error_band_count")] = g->errorBandCount();
        return true;
    }

    if (method == QStringLiteral("plot.density2d")) {
        Table *t = resolveTable(d_app, params);
        auto colOf = [&](const char *key, int def) {
            const QString k = QString::fromLatin1(key);
            return t->colName(resolveColumnIndex(
                    t, params.contains(k) ? params.value(k) : QJsonValue(def), true));
        };
        const QString xc = colOf("x", 0), yc = colOf("y", 1);
        const QString pal = optionalJsonString(params, QStringLiteral("palette"), QStringLiteral("viridis"));
        const Spectrogram::ColorMapPreset preset = parseColorMapPreset(pal, nullptr, true);
        const auto dr = ExtraPlotTypes::plotDensity2D(
                d_app, t, xc, yc, params.value(QStringLiteral("bins_x")).toInt(0),
                params.value(QStringLiteral("bins_y")).toInt(0),
                optionalJsonFiniteDouble(params, QStringLiteral("width_x"), 0.0),
                optionalJsonFiniteDouble(params, QStringLiteral("width_y"), 0.0),
                optionalJsonBool(params, QStringLiteral("log"), false), int(preset),
                optionalJsonBool(params, QStringLiteral("plot"), true));
        if (!dr.valid)
            throwInvalidParams(dr.error.isEmpty() ? QStringLiteral("2D density failed.") : dr.error);
        if (dr.plot)
            applyRequestedWindowName(d_app, dr.plot, params);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("n")] = dr.n;
        outResult[QStringLiteral("excluded")] = dr.excluded;
        outResult[QStringLiteral("bins_x")] = dr.binsX;
        outResult[QStringLiteral("bins_y")] = dr.binsY;
        outResult[QStringLiteral("x0")] = safeJsonDouble(dr.x0);
        outResult[QStringLiteral("y0")] = safeJsonDouble(dr.y0);
        outResult[QStringLiteral("width_x")] = safeJsonDouble(dr.widthX);
        outResult[QStringLiteral("width_y")] = safeJsonDouble(dr.widthY);
        outResult[QStringLiteral("max_count")] = safeJsonDouble(dr.maxCount);
        if (dr.matrix)
            outResult[QStringLiteral("matrix")] = dr.matrix->name();
        if (dr.plot) {
            outResult[QStringLiteral("graph")] = dr.plot->name();
            outResult[QStringLiteral("name")] = dr.plot->name();
        }
        return true;
    }

    if (method == QStringLiteral("graph.rebuild_density2d")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        if (!ExtraPlotTypes::rebuildDensity2D(d_app, g))
            throwInvalidParams(QStringLiteral("graph.rebuild_density2d: not a 2D density graph, or its source columns are gone."));
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.pole_figure")) {
        ExtraPlotTypes::PoleFigureOptions opt;
        const QString proj =
                optionalJsonString(params, QStringLiteral("projection"), QStringLiteral("schmidt")).trimmed().toLower();
        if (proj == QStringLiteral("wulff") || proj == QStringLiteral("equal-angle")
            || proj == QStringLiteral("equal_angle") || proj == QStringLiteral("stereographic"))
            opt.projection = ExtraPlotTypes::EqualAngle;
        else if (proj != QStringLiteral("schmidt") && proj != QStringLiteral("equal-area")
                 && proj != QStringLiteral("equal_area") && proj != QStringLiteral("lambert"))
            throwInvalidParams(QStringLiteral("Unknown projection '%1' (expected schmidt|wulff).").arg(proj));
        opt.normalizeMRD = optionalJsonBool(params, QStringLiteral("mrd"), true);
        opt.alphaFromRim = optionalJsonBool(params, QStringLiteral("alpha_from_rim"), false);
        opt.betaZeroAtTop = optionalJsonBool(params, QStringLiteral("beta_zero_top"), false);
        opt.clockwise = optionalJsonBool(params, QStringLiteral("clockwise"), false);
        const QString rd = optionalJsonString(params, QStringLiteral("rd"), QStringLiteral("top")).trimmed().toLower();
        if (rd != QStringLiteral("top") && rd != QStringLiteral("right"))
            throwInvalidParams(QStringLiteral("Unknown rd '%1' (expected top|right).").arg(rd));
        opt.rdTop = rd == QStringLiteral("top");
        opt.gridSize = params.value(QStringLiteral("grid")).toInt(201);
        opt.levels = params.value(QStringLiteral("levels")).toInt(8);
        opt.preset = int(parseColorMapPreset(
                optionalJsonString(params, QStringLiteral("palette"), QStringLiteral("viridis")), nullptr, true));
        opt.label = optionalJsonString(params, QStringLiteral("label"));
        opt.axis1Label = optionalJsonString(params, QStringLiteral("axis1_label"));
        opt.axis2Label = optionalJsonString(params, QStringLiteral("axis2_label"));
        opt.showTiltCircles = optionalJsonBool(params, QStringLiteral("tilt_circles"), false);
        const bool wantPlot = optionalJsonBool(params, QStringLiteral("plot"), true);
        const bool attachToExisting =
                wantPlot && !optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty();
        ExtraPlotTypes::PoleFigureResult pr;
        QString sourceTagAttrs;
        auto xmlEscAttr = [](const QString &s) {
            QString out = s;
            out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
            out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
            out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
            out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
            return out;
        };
        if (!optionalJsonString(params, QStringLiteral("matrix")).trimmed().isEmpty()) {
            Matrix *m = resolveMatrix(d_app, params);
            pr = ExtraPlotTypes::plotPoleFigureFromMatrix(d_app, m, opt,
                                                          wantPlot && !attachToExisting);
            if (m) {
                sourceTagAttrs = QStringLiteral(" sourceMatrix=\"%1\"").arg(xmlEscAttr(m->name()));
            }
        } else {
            Table *t = resolveTable(d_app, params);
            auto colOf = [&](const char *key, int def) {
                const QString k = QString::fromLatin1(key);
                return t->colName(resolveColumnIndex(
                        t, params.contains(k) ? params.value(k) : QJsonValue(def), true));
            };
            const QString aCol = colOf("alpha", 0);
            const QString bCol = colOf("beta", 1);
            const QString iCol = colOf("intensity", 2);
            pr = ExtraPlotTypes::plotPoleFigure(d_app, t, aCol, bCol, iCol, opt,
                                                wantPlot && !attachToExisting);
            if (t) {
                const auto shortName = [&](const QString &full) {
                    const int idx = t->colIndex(full);
                    return (idx >= 0) ? t->colLabel(idx) : full;
                };
                sourceTagAttrs = QStringLiteral(" table=\"%1\" a=\"%2\" b=\"%3\" i=\"%4\"")
                                         .arg(xmlEscAttr(t->name()), xmlEscAttr(shortName(aCol)),
                                              xmlEscAttr(shortName(bCol)),
                                              xmlEscAttr(shortName(iCol)));
            }
        }
        if (!pr.valid)
            throwInvalidParams(pr.error.isEmpty() ? QStringLiteral("Pole figure failed.") : pr.error);
        if (attachToExisting && pr.matrix) {
            MultiLayer *ml = resolveMultiLayer(d_app, params);
            Graph *targetG = resolveGraphLayer(ml, params);
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Plot Pole Figure"));
            targetG->plotSpectrogram(pr.matrix, Graph::ColorMap);
            if (targetG->plotWidget()) {
                for (QwtPlotItem *it : targetG->plotWidget()->itemList()) {
                    if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                        if (auto *sp = dynamic_cast<Spectrogram *>(it);
                            sp && sp->matrix() == pr.matrix) {
                            sp->setMaskInvalidCells(true);
                            sp->setColorMapPreset(static_cast<Spectrogram::ColorMapPreset>(opt.preset),
                                                  false);
                            sp->setDisplayMode(QwtPlotSpectrogram::ContourMode, true);
                            sp->setDefaultContourPen(QPen(QColor(25, 25, 25), 1));
                            sp->setLevelsNumber(qBound(2, opt.levels, 50));
                            sp->setColorBarTitle(opt.normalizeMRD ? QStringLiteral("MRD")
                                                                  : QStringLiteral("Intensity"));
                            sp->syncColorScale();
                            break;
                        }
                    }
                }
            }
            if (targetG->legend())
                targetG->removeLegend();
            QString extraAttrs;
            if (!opt.axis1Label.trimmed().isEmpty())
                extraAttrs += QStringLiteral(" ax1=\"%1\"").arg(xmlEscAttr(opt.axis1Label.trimmed()));
            if (!opt.axis2Label.trimmed().isEmpty())
                extraAttrs += QStringLiteral(" ax2=\"%1\"").arg(xmlEscAttr(opt.axis2Label.trimmed()));
            if (opt.showTiltCircles)
                extraAttrs += QStringLiteral(" rings=\"1\"");
            const QString tag = QStringLiteral("<ToriPoleFigure matrix=\"")
                    + xmlEscAttr(pr.matrix->name()) + QStringLiteral("\"") + sourceTagAttrs
                    + QStringLiteral(" proj=\"") + QString::number(int(opt.projection))
                    + QStringLiteral("\" mrd=\"") + QString::number(opt.normalizeMRD ? 1 : 0)
                    + QStringLiteral("\" rim=\"") + QString::number(opt.alphaFromRim ? 1 : 0)
                    + QStringLiteral("\" zeroTop=\"") + QString::number(opt.betaZeroAtTop ? 1 : 0)
                    + QStringLiteral("\" cw=\"") + QString::number(opt.clockwise ? 1 : 0)
                    + QStringLiteral("\" rdTop=\"") + QString::number(opt.rdTop ? 1 : 0)
                    + QStringLiteral("\"") + extraAttrs + QStringLiteral(" label=\"")
                    + xmlEscAttr(opt.label) + QStringLiteral("\"/>");
            ExtraPlotTypes::restoreGraphTag(targetG, tag);
            pr.plot = ml;
            undoGuard.commit();
        } else if (pr.plot) {
            applyRequestedWindowName(d_app, pr.plot, params);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("max")] = safeJsonDouble(pr.maxValue);
        outResult[QStringLiteral("max_alpha")] = safeJsonDouble(pr.maxAlpha);
        outResult[QStringLiteral("max_beta")] = safeJsonDouble(pr.maxBeta);
        outResult[QStringLiteral("max_x")] = safeJsonDouble(pr.maxX);
        outResult[QStringLiteral("max_y")] = safeJsonDouble(pr.maxY);
        outResult[QStringLiteral("mean_intensity")] = safeJsonDouble(pr.meanIntensity);
        outResult[QStringLiteral("alpha_max")] = safeJsonDouble(pr.alphaMax);
        outResult[QStringLiteral("n_alpha")] = pr.nAlpha;
        outResult[QStringLiteral("n_beta")] = pr.nBeta;
        outResult[QStringLiteral("missing")] = pr.missing;
        if (pr.matrix)
            outResult[QStringLiteral("matrix")] = pr.matrix->name();
        if (pr.plot) {
            outResult[QStringLiteral("graph")] = pr.plot->name();
            outResult[QStringLiteral("name")] = pr.plot->name();
        }
        return true;
    }

    if (method == QStringLiteral("stats.kaplan_meier")) {
        Table *t = resolveTable(d_app, params);
        const QString timeCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("time_col"))
                                           ? params.value(QStringLiteral("time_col"))
                                           : QJsonValue(0),
                                   true));
        const QString evCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("event_col"))
                                           ? params.value(QStringLiteral("event_col"))
                                           : QJsonValue(1),
                                   true));
        QString grpCol;
        if (params.contains(QStringLiteral("group_col"))
            && !params.value(QStringLiteral("group_col")).isNull()) {
            grpCol = t->colLabel(
                    resolveColumnIndex(t, params.value(QStringLiteral("group_col")), true));
        }
        double logRankP = 1.0;
        MultiLayer *ml =
                GraphStatsAndSurfaceFit::plotKaplanMeier(d_app, t, timeCol, evCol, grpCol, &logRankP);
        if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = (ml != nullptr);
        outResult[QStringLiteral("log_rank_p")] = safeJsonDouble(logRankP);
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("stats.roc")) {
        Table *t = resolveTable(d_app, params);
        const int stIdx = resolveColumnIndex(t, params.contains(QStringLiteral("state_col"))
                                                     ? params.value(QStringLiteral("state_col"))
                                                     : QJsonValue(0),
                                             true);
        QVector<int> scIdxs;
        if (params.value(QStringLiteral("score_cols")).isArray()
            && !params.value(QStringLiteral("score_cols")).toArray().isEmpty()) {
            for (const QJsonValue &sv : params.value(QStringLiteral("score_cols")).toArray())
                scIdxs.append(resolveColumnIndex(t, sv, true));
        } else {
            const int scIdx0 = resolveColumnIndex(
                    t, params.contains(QStringLiteral("score_col"))
                            ? params.value(QStringLiteral("score_col"))
                            : QJsonValue(stIdx == 0 ? 1 : 0),
                    true);
            scIdxs.append(scIdx0);
        }
        const int scIdx = scIdxs.first();
        const bool wantPlot = optionalJsonBool(params, QStringLiteral("plot"), true);
        double aucPlot = 0.5;
        MultiLayer *ml = nullptr;
        if (wantPlot) {
            ml = GraphStatsAndSurfaceFit::plotROCCurve(d_app, t, t->colLabel(stIdx),
                                                       t->colLabel(scIdx), &aucPlot);
            if (ml) {
                for (Graph *lg : ml->layersList())
                    enforceArialGraphFonts(lg);
            }
        }
        std::vector<int> yTrue;
        std::vector<int> validRows;
        extractBinaryStateRows(t, stIdx, QVector<int>{ scIdx }, yTrue, validRows);
        std::vector<double> scVec;
        scVec.reserve(validRows.size());
        for (int r : validRows)
            scVec.push_back(t->cell(r, scIdx));
        const auto rm = computeRocMetricsDirect(yTrue, scVec);
        const double finalAuc = (rm.nPos > 0 && rm.nNeg > 0) ? rm.auc : aucPlot;
        outResult[QStringLiteral("ok")] = ((!wantPlot || ml != nullptr) && rm.nPos > 0 && rm.nNeg > 0);
        outResult[QStringLiteral("auc")] = safeJsonDouble(finalAuc);
        outResult[QStringLiteral("se")] = safeJsonDouble(rm.se);
        outResult[QStringLiteral("optimal_threshold")] = safeJsonDouble(rm.optimalThreshold);
        outResult[QStringLiteral("sensitivity")] = safeJsonDouble(rm.sensitivity);
        outResult[QStringLiteral("specificity")] = safeJsonDouble(rm.specificity);
        outResult[QStringLiteral("n_pos")] = rm.nPos;
        outResult[QStringLiteral("n_neg")] = rm.nNeg;
        if (scIdxs.size() > 1) {
            QJsonArray curvesMetrics;
            for (int sci : scIdxs) {
                std::vector<int> yt;
                std::vector<int> vr;
                extractBinaryStateRows(t, stIdx, QVector<int>{ sci }, yt, vr);
                std::vector<double> sv;
                sv.reserve(vr.size());
                for (int r : vr)
                    sv.push_back(t->cell(r, sci));
                const auto rmi = computeRocMetricsDirect(yt, sv);
                QJsonObject co;
                co[QStringLiteral("col")] = t->colLabel(sci);
                co[QStringLiteral("auc")] = safeJsonDouble(rmi.auc);
                co[QStringLiteral("se")] = safeJsonDouble(rmi.se);
                curvesMetrics.append(co);
            }
            outResult[QStringLiteral("curves")] = curvesMetrics;
        }
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("stats.pca")) {
        Table *t = resolveTable(d_app, params);
        const bool standardize = optionalJsonBool(params, QStringLiteral("standardize"), true);
        const bool wantBiplot = optionalJsonBool(params, QStringLiteral("biplot"), true);
        QStringList colNames;
        QVector<int> colIdxs;
        if (params.contains(QStringLiteral("cols")) && !params.value(QStringLiteral("cols")).isNull()) {
            const QJsonValue colsVal = params.value(QStringLiteral("cols"));
            if (colsVal.isArray()) {
                for (const QJsonValue &cv : colsVal.toArray()) {
                    const int ci = resolveColumnIndex(t, cv, true);
                    colIdxs.append(ci);
                    colNames << t->colLabel(ci);
                }
            } else if (colsVal.isString() || colsVal.isDouble()) {
                const int ci = resolveColumnIndex(t, colsVal, true);
                colIdxs.append(ci);
                colNames << t->colLabel(ci);
            } else {
                throwInvalidParams(
                        QStringLiteral("stats.pca 'cols' must be a list of column names or indices."));
            }
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    colIdxs.append(c);
                    colNames << t->colLabel(c);
                }
            }
        }
        if (colIdxs.size() < 2) {
            throwInvalidParams(
                    QStringLiteral("stats.pca requires at least 2 numeric columns (got %1).")
                            .arg(colIdxs.size()));
        }
        int completeRows = 0;
        for (int r = 0; r < t->numRows(); ++r) {
            bool okRow = true;
            for (int ci : colIdxs) {
                Column *cc = t->column(ci);
                if (!cc || cc->columnMode() != SciDAVis::ColumnMode::Numeric || cc->isInvalid(r)
                    || cc->isMasked(r) || !std::isfinite(cc->valueAt(r))) {
                    okRow = false;
                    break;
                }
            }
            if (okRow)
                ++completeRows;
        }
        if (completeRows < 3) {
            throwInvalidParams(
                    QStringLiteral("stats.pca requires at least 3 complete finite rows (got %1).")
                            .arg(completeRows));
        }
        Table *scoresTbl = nullptr;
        MultiLayer *ml = GraphStatsAndSurfaceFit::runPCAAndBiplot(d_app, t, colNames, &scoresTbl,
                                                                  standardize);
        if (!ml && !scoresTbl) {
            throwInvalidParams(
                    QStringLiteral("stats.pca failed due to insufficient valid data in '%1'.")
                            .arg(t->name()));
        }
        if (ml && !wantBiplot) {
            closeAndDeleteEmptyPlot(d_app, ml);
            ml = nullptr;
        } else if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = (ml != nullptr || scoresTbl != nullptr);
        outResult[QStringLiteral("standardized")] = standardize;
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        if (scoresTbl) {
            outResult[QStringLiteral("scores_table")] = scoresTbl->name();
            outResult[QStringLiteral("scores")] = extractTableAsJson(scoresTbl);
        }
        return true;
    }

    if (method == QStringLiteral("stats.correlation")) {
        Table *t = resolveTable(d_app, params);
        QStringList colNames;
        QVector<int> colIdxs;
        if (params.value(QStringLiteral("cols")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("cols")).toArray()) {
                const int ci = resolveColumnIndex(t, cv, true);
                colIdxs.append(ci);
                colNames << t->colLabel(ci);
            }
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    colIdxs.append(c);
                    colNames << t->colLabel(c);
                }
            }
        }
        const QString corrMethod =
                optionalJsonString(params, QStringLiteral("method"), QStringLiteral("pearson"))
                        .trimmed()
                        .toLower();
        if (!corrMethod.isEmpty() && corrMethod != QStringLiteral("pearson")
            && corrMethod != QStringLiteral("spearman")) {
            throwInvalidParams(
                    QStringLiteral("Unknown correlation method '%1' (expected 'pearson' or 'spearman').")
                            .arg(corrMethod));
        }
        const bool spearman = (corrMethod == QStringLiteral("spearman"))
                || optionalJsonBool(params, QStringLiteral("spearman"), false);
        const bool wantHeatmap = optionalJsonBool(params, QStringLiteral("plot_heatmap"), true);
        Matrix *corrM = nullptr;
        MultiLayer *ml = GraphStatsAndSurfaceFit::plotCorrelationMatrixHeatmap(
                d_app, t, colNames, spearman, &corrM);
        if (ml && !wantHeatmap) {
            closeAndDeleteEmptyPlot(d_app, ml);
            ml = nullptr;
        } else if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
        }
        auto countPairwiseValid = [&](int idxA, int idxB) -> int {
            if (idxA < 0 || idxA >= colIdxs.size() || idxB < 0 || idxB >= colIdxs.size())
                return 0;
            Column *cA = t->column(colIdxs[idxA]);
            Column *cB = t->column(colIdxs[idxB]);
            if (!cA || !cB)
                return 0;
            int cnt = 0;
            for (int r = 0; r < t->numRows(); ++r) {
                if (!cA->isInvalid(r) && !cA->isMasked(r) && !cB->isInvalid(r) && !cB->isMasked(r)
                    && std::isfinite(t->cell(r, colIdxs[idxA]))
                    && std::isfinite(t->cell(r, colIdxs[idxB]))) {
                    ++cnt;
                }
            }
            return cnt;
        };
        // A column that does not vary over the rows a pair has in common (a constant column)
        // has no correlation: r and p are null, not r = 0 and p = 1 (the heatmap matrix keeps 0
        // as its colour). p is also null when fewer than 3 rows leave no degrees of freedom.
        auto pairVaries = [&](int idxA, int idxB) -> bool {
            if (idxA < 0 || idxA >= colIdxs.size() || idxB < 0 || idxB >= colIdxs.size())
                return false;
            Column *cA = t->column(colIdxs[idxA]);
            Column *cB = t->column(colIdxs[idxB]);
            if (!cA || !cB)
                return false;
            bool first = true, variesA = false, variesB = false;
            double a0 = 0.0, b0 = 0.0;
            for (int r = 0; r < t->numRows(); ++r) {
                if (cA->isInvalid(r) || cA->isMasked(r) || cB->isInvalid(r) || cB->isMasked(r))
                    continue;
                const double a = t->cell(r, colIdxs[idxA]);
                const double b = t->cell(r, colIdxs[idxB]);
                if (!std::isfinite(a) || !std::isfinite(b))
                    continue;
                if (first) {
                    a0 = a;
                    b0 = b;
                    first = false;
                } else {
                    variesA = variesA || a != a0;
                    variesB = variesB || b != b0;
                }
            }
            return variesA && variesB;
        };
        QJsonArray rRows, pRows;
        if (corrM) {
            const int k = corrM->numRows();
            for (int i = 0; i < k; ++i) {
                QJsonArray rRow, pRow;
                for (int j = 0; j < corrM->numCols(); ++j) {
                    const double rv = corrM->cell(i, j);
                    if (!pairVaries(i, j)) {
                        rRow.append(QJsonValue::Null);
                        pRow.append(QJsonValue::Null);
                        continue;
                    }
                    rRow.append(safeJsonDouble(rv));
                    if (i == j) {
                        pRow.append(0.0);
                    } else {
                        const int nij = countPairwiseValid(i, j);
                        if (nij < 3 || !std::isfinite(rv)) {
                            pRow.append(QJsonValue::Null);
                        } else {
                            const double df = double(nij - 2);
                            const double rSafe = std::clamp(rv, -0.9999999, 0.9999999);
                            const double tStat =
                                    std::abs(rSafe) * std::sqrt(df / (1.0 - rSafe * rSafe));
                            pRow.append(safeJsonDouble(2.0 * gsl_cdf_tdist_Q(tStat, df)));
                        }
                    }
                }
                rRows.append(rRow);
                pRows.append(pRow);
            }
        }
        outResult[QStringLiteral("ok")] = (ml != nullptr || corrM != nullptr);
        outResult[QStringLiteral("columns")] = toJsonArray(colNames);
        outResult[QStringLiteral("r_matrix")] = rRows;
        outResult[QStringLiteral("p_matrix")] = pRows;
        if (corrM)
            outResult[QStringLiteral("matrix")] = corrM->name();
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("stats.clustering")) {
        Table *t = resolveTable(d_app, params);
        QStringList colNames;
        QVector<int> colIdxs;
        if (params.contains(QStringLiteral("cols")) && !params.value(QStringLiteral("cols")).isNull()) {
            const QJsonValue colsVal = params.value(QStringLiteral("cols"));
            if (colsVal.isArray()) {
                for (const QJsonValue &cv : colsVal.toArray()) {
                    const int ci = resolveColumnIndex(t, cv, true);
                    colIdxs.append(ci);
                    colNames << t->colLabel(ci);
                }
            } else if (colsVal.isString() || colsVal.isDouble()) {
                const int ci = resolveColumnIndex(t, colsVal, true);
                colIdxs.append(ci);
                colNames << t->colLabel(ci);
            } else {
                throwInvalidParams(
                        QStringLiteral("stats.clustering 'cols' must be a column name, index, or list."));
            }
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    colIdxs.append(c);
                    colNames << t->colLabel(c);
                }
            }
        }
        if (colIdxs.isEmpty()) {
            throwInvalidParams(QStringLiteral("stats.clustering requires at least 1 numeric column."));
        }
        int completeRows = 0;
        for (int r = 0; r < t->numRows(); ++r) {
            bool okRow = true;
            for (int ci : colIdxs) {
                Column *cc = t->column(ci);
                if (!cc || cc->columnMode() != SciDAVis::ColumnMode::Numeric || cc->isInvalid(r)
                    || cc->isMasked(r) || !std::isfinite(cc->valueAt(r))) {
                    okRow = false;
                    break;
                }
            }
            if (okRow)
                ++completeRows;
        }
        if (completeRows < 2) {
            throwInvalidParams(
                    QStringLiteral("stats.clustering requires at least 2 complete finite rows (got %1).")
                            .arg(completeRows));
        }
        const QString alg = optionalJsonString(params, QStringLiteral("method"),
                                               QStringLiteral("kmeans"))
                                    .trimmed()
                                    .toLower();
        const QString lnkStr = optionalJsonString(params, QStringLiteral("linkage"),
                                                  QStringLiteral("ward"))
                                       .trimmed()
                                       .toLower();
        int lnk = 0; // Ward
        if (lnkStr.isEmpty() || lnkStr == QStringLiteral("ward"))
            lnk = 0;
        else if (lnkStr == QStringLiteral("complete"))
            lnk = 1;
        else if (lnkStr == QStringLiteral("average"))
            lnk = 2;
        else
            throwInvalidParams(
                    QStringLiteral("Unknown hierarchical linkage '%1' (expected 'ward'|'complete'|'average').")
                            .arg(lnkStr));
        const bool plotC = params.contains(QStringLiteral("plot"))
                ? optionalJsonBool(params, QStringLiteral("plot"), true)
                : true;
        if (alg == QStringLiteral("hierarchical") || alg == QStringLiteral("dendrogram")) {
            const int D = colIdxs.size();
            std::vector<int> validRows;
            std::vector<std::vector<double>> rawX;
            for (int r = 0; r < t->numRows(); ++r) {
                bool okRow = true;
                std::vector<double> pt(size_t(D), 0.0);
                for (int d = 0; d < D; ++d) {
                    Column *cc = t->column(colIdxs[d]);
                    if (!cc || cc->columnMode() != SciDAVis::ColumnMode::Numeric || cc->isInvalid(r)
                        || cc->isMasked(r) || !std::isfinite(cc->valueAt(r))) {
                        okRow = false;
                        break;
                    }
                    pt[size_t(d)] = cc->valueAt(r);
                }
                if (okRow) {
                    validRows.push_back(r);
                    rawX.push_back(std::move(pt));
                }
            }
            const int N = static_cast<int>(rawX.size());
            if (N < 2) {
                throwInvalidParams(
                        QStringLiteral("stats.clustering (hierarchical) failed due to insufficient valid data in '%1'.")
                                .arg(t->name()));
            }
            constexpr int kMaxHierarchicalRows = 2000;
            if (N > kMaxHierarchicalRows) {
                throwInvalidParams(
                        QStringLiteral("stats.clustering (hierarchical) supports at most %1 complete rows (got %2); use method='kmeans' for larger datasets.")
                                .arg(kMaxHierarchicalRows)
                                .arg(N));
            }
            const bool hasExplicitK = params.contains(QStringLiteral("k"))
                    && !params.value(QStringLiteral("k")).isNull();
            int kCut = hasExplicitK ? parseBoundedInt(params, QStringLiteral("k"), 3, 2, 256)
                                    : std::min(3, N);
            if (hasExplicitK && kCut > N) {
                throwInvalidParams(
                        QStringLiteral("stats.clustering (hierarchical) requires at least k=%1 complete finite rows (got %2).")
                                .arg(kCut)
                                .arg(N));
            }
            kCut = std::clamp(kCut, 1, N);

            // Standardize features (matching GraphStatsAndSurfaceFit::plotHierarchicalDendrogram)
            std::vector<double> meanF(size_t(D), 0.0), sdF(size_t(D), 1.0);
            for (int d = 0; d < D; ++d) {
                double s = 0.0;
                for (int i = 0; i < N; ++i)
                    s += rawX[size_t(i)][size_t(d)];
                meanF[size_t(d)] = s / double(N);
                double ss = 0.0;
                for (int i = 0; i < N; ++i) {
                    const double dev = rawX[size_t(i)][size_t(d)] - meanF[size_t(d)];
                    ss += dev * dev;
                }
                const double sd = std::sqrt(ss / double(N - 1));
                sdF[size_t(d)] = (sd > 1e-15) ? sd : 1.0;
            }
            const int totalNodes = 2 * N - 1;
            struct HNode
            {
                int left = -1;
                int right = -1;
                int count = 1;
                double height = 0.0;
                bool active = false;
            };
            std::vector<HNode> nodes(static_cast<size_t>(totalNodes));
            for (int i = 0; i < N; ++i)
                nodes[size_t(i)].active = true;
            std::vector<double> distMat(size_t(totalNodes) * size_t(totalNodes), 0.0);
            for (int i = 0; i < N; ++i) {
                for (int j = i + 1; j < N; ++j) {
                    double d2 = 0.0;
                    for (int d = 0; d < D; ++d) {
                        const double diff =
                                (rawX[size_t(i)][size_t(d)] - rawX[size_t(j)][size_t(d)])
                                / sdF[size_t(d)];
                        d2 += diff * diff;
                    }
                    const double val = (lnk == 0) ? d2 : std::sqrt(d2);
                    distMat[size_t(i) * size_t(totalNodes) + size_t(j)] = val;
                    distMat[size_t(j) * size_t(totalNodes) + size_t(i)] = val;
                }
            }
            QJsonArray linkageArr;
            std::vector<int> cutActive;
            const int cutStep = N - kCut;
            if (cutStep == 0) {
                for (int i = 0; i < N; ++i)
                    cutActive.push_back(i);
            }
            for (int step = 0; step < N - 1; ++step) {
                int bestA = -1, bestB = -1;
                double minD = std::numeric_limits<double>::max();
                const int curMax = N + step;
                for (int a = 0; a < curMax; ++a) {
                    if (!nodes[size_t(a)].active)
                        continue;
                    for (int b = a + 1; b < curMax; ++b) {
                        if (!nodes[size_t(b)].active)
                            continue;
                        const double dab = distMat[size_t(a) * size_t(totalNodes) + size_t(b)];
                        if (dab < minD) {
                            minD = dab;
                            bestA = a;
                            bestB = b;
                        }
                    }
                }
                if (bestA < 0 || bestB < 0)
                    break;
                const int newNode = N + step;
                const int nA = nodes[size_t(bestA)].count;
                const int nB = nodes[size_t(bestB)].count;
                const double rawHeight =
                        (lnk == 0) ? std::sqrt(std::max(0.0, minD)) : std::max(0.0, minD);
                const double childMaxH =
                        std::max(nodes[size_t(bestA)].height, nodes[size_t(bestB)].height);
                const double h = std::max(rawHeight, childMaxH);
                nodes[size_t(newNode)].left = bestA;
                nodes[size_t(newNode)].right = bestB;
                nodes[size_t(newNode)].count = nA + nB;
                nodes[size_t(newNode)].height = h;
                nodes[size_t(newNode)].active = true;
                nodes[size_t(bestA)].active = false;
                nodes[size_t(bestB)].active = false;

                QJsonArray mergeRow;
                mergeRow.append(bestA);
                mergeRow.append(bestB);
                mergeRow.append(safeJsonDouble(h));
                mergeRow.append(nA + nB);
                linkageArr.append(mergeRow);

                for (int c = 0; c < newNode; ++c) {
                    if (!nodes[size_t(c)].active)
                        continue;
                    const double dAC = distMat[size_t(bestA) * size_t(totalNodes) + size_t(c)];
                    const double dBC = distMat[size_t(bestB) * size_t(totalNodes) + size_t(c)];
                    const int nC = nodes[size_t(c)].count;
                    double dNew = 0.0;
                    if (lnk == 0) {
                        const double denom = double(nA + nB + nC);
                        dNew = (double(nA + nC) * dAC + double(nB + nC) * dBC - double(nC) * minD)
                                / denom;
                        dNew = std::max(0.0, dNew);
                    } else if (lnk == 1) {
                        dNew = std::max(dAC, dBC);
                    } else {
                        dNew = (double(nA) * dAC + double(nB) * dBC) / double(nA + nB);
                    }
                    distMat[size_t(newNode) * size_t(totalNodes) + size_t(c)] = dNew;
                    distMat[size_t(c) * size_t(totalNodes) + size_t(newNode)] = dNew;
                }
                if (step + 1 == cutStep) {
                    for (int c = 0; c <= newNode; ++c) {
                        if (nodes[size_t(c)].active)
                            cutActive.push_back(c);
                    }
                }
            }

            const int assignLen = t->numRows();
            QVector<int> hAssign(assignLen, -1);
            for (int cid = 0; cid < static_cast<int>(cutActive.size()); ++cid) {
                QVector<int> stk;
                stk.push_back(cutActive[size_t(cid)]);
                while (!stk.isEmpty()) {
                    const int u = stk.takeLast();
                    if (u < 0)
                        continue;
                    if (u < N) {
                        const int rowIdx = validRows[size_t(u)];
                        if (rowIdx >= 0 && rowIdx < assignLen)
                            hAssign[rowIdx] = cid;
                    } else {
                        stk.push_back(nodes[size_t(u)].left);
                        stk.push_back(nodes[size_t(u)].right);
                    }
                }
            }
            QJsonArray assignArr;
            for (int a : hAssign)
                assignArr.append(a);

            MultiLayer *ml = nullptr;
            if (plotC) {
                ml = GraphStatsAndSurfaceFit::plotHierarchicalDendrogram(d_app, t, colNames, lnk);
                if (!ml) {
                    throwInvalidParams(
                            QStringLiteral("stats.clustering (hierarchical) failed due to insufficient valid data in '%1'.")
                                    .arg(t->name()));
                }
                for (Graph *lg : ml->layersList())
                    enforceArialGraphFonts(lg);
            }
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("method")] = QStringLiteral("hierarchical");
            outResult[QStringLiteral("linkage")] =
                    (lnk == 0) ? QStringLiteral("ward")
                               : ((lnk == 1) ? QStringLiteral("complete")
                                             : QStringLiteral("average"));
            outResult[QStringLiteral("k")] = static_cast<int>(cutActive.size());
            outResult[QStringLiteral("assignments")] = assignArr;
            outResult[QStringLiteral("linkage_matrix")] = linkageArr;
            if (ml)
                outResult[QStringLiteral("graph")] = ml->name();
            return true;
        }
        if (!alg.isEmpty() && alg != QStringLiteral("kmeans") && alg != QStringLiteral("k_means")
            && alg != QStringLiteral("k-means")) {
            throwInvalidParams(
                    QStringLiteral("Unknown clustering method '%1' (expected 'kmeans' or 'hierarchical').")
                            .arg(alg));
        }
        const int k = parseBoundedInt(params, QStringLiteral("k"), 3, 2, 256);
        if (completeRows < k) {
            throwInvalidParams(
                    QStringLiteral("stats.clustering (kmeans) requires at least k=%1 complete finite rows (got %2).")
                            .arg(k)
                            .arg(completeRows));
        }
        const auto km =
                GraphStatsAndSurfaceFit::runKMeansClustering(d_app, t, colNames, k, plotC);
        if (!km.valid) {
            if (km.clusterPlot)
                closeAndDeleteEmptyPlot(d_app, km.clusterPlot);
            throwInvalidParams(
                    QStringLiteral("stats.clustering (kmeans) failed due to insufficient valid data in '%1'.")
                            .arg(t->name()));
        }
        if (km.clusterPlot) {
            for (Graph *lg : km.clusterPlot->layersList())
                enforceArialGraphFonts(lg);
        }
        QJsonArray assignArr;
        for (int a : km.assignments)
            assignArr.append(a);
        outResult[QStringLiteral("ok")] = km.valid;
        outResult[QStringLiteral("k")] = km.k;
        outResult[QStringLiteral("silhouette")] = safeJsonDouble(km.silhouetteScore);
        outResult[QStringLiteral("assignments")] = assignArr;
        if (km.resultTable)
            outResult[QStringLiteral("table")] = km.resultTable->name();
        if (km.clusterPlot)
            outResult[QStringLiteral("graph")] = km.clusterPlot->name();
        return true;
    }

    if (method == QStringLiteral("stats.confidence_ellipse")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString curveTitle = resolveCurveTitle(g, params, QStringLiteral("curve"), true);
        const double conf = params.contains(QStringLiteral("conf_level"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("conf_level"), 0.95, 1.0)
                : optionalJsonFiniteDouble(params, QStringLiteral("level"), 0.95, 1.0);
        if (conf <= 0.0 || conf >= 1.0)
            throwInvalidParams(QStringLiteral("Confidence level must satisfy 0 < level < 1."));
        const bool pred = optionalJsonBool(params, QStringLiteral("prediction"), false);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Confidence Ellipse"));
        QwtPlotCurve *ec =
                GraphStatsAndSurfaceFit::addConfidenceEllipse(d_app, g, curveTitle, conf, pred);
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        if (ec)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (ec != nullptr);
        if (ec) {
            outResult[QStringLiteral("ellipse_curve")] = ec->title().text();
        } else {
            // Say why there is no ellipse (an empty curve used to give a bare ok false).
            const int idx = g->curveIndex(curveTitle);
            const int valid = countValidCurvePoints(idx >= 0 ? g->curve(idx) : nullptr);
            outResult[QStringLiteral("error")] = valid < 3
                    ? QStringLiteral("stats.confidence_ellipse requires at least 3 valid data "
                                     "points (curve '%1' has %2).")
                              .arg(curveTitle)
                              .arg(valid)
                    : QStringLiteral("stats.confidence_ellipse: no ellipse could be made for "
                                     "curve '%1'.")
                              .arg(curveTitle);
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // 7E. Graph Image Digitizer (digitizer.extract)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("digitizer.extract")) {
        const QString path = params.contains(QStringLiteral("path"))
                ? optionalJsonString(params, QStringLiteral("path")).trimmed()
                : optionalJsonString(params, QStringLiteral("image_path")).trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path) || !QFileInfo(path).isFile())
            throwInvalidParams(QStringLiteral("Image file not found: %1").arg(path));
        const QString modeStr =
                optionalJsonString(params, QStringLiteral("mode"), QStringLiteral("auto_curve"))
                        .trimmed()
                        .toLower();
        if (!modeStr.isEmpty() && modeStr != QStringLiteral("auto_curve")
            && modeStr != QStringLiteral("color_trace") && modeStr != QStringLiteral("auto")) {
            throwInvalidParams(
                    QStringLiteral("Unknown digitizer mode '%1' (expected 'auto_curve').")
                            .arg(modeStr));
        }
        QImage img(path);
        if (img.isNull())
            throwRpcRuntime(QStringLiteral("Failed to decode image '%1'.").arg(path));

        GraphDigitizer::AxisCalibration cal;
        cal.pixelX1 = QPointF(optionalJsonFiniteDouble(params, QStringLiteral("px_x1"), 0.0),
                              optionalJsonFiniteDouble(params, QStringLiteral("py_x1"),
                                                       double(img.height() - 1)));
        cal.pixelX2 = QPointF(optionalJsonFiniteDouble(params, QStringLiteral("px_x2"),
                                                       double(img.width() - 1)),
                              optionalJsonFiniteDouble(params, QStringLiteral("py_x2"),
                                                       double(img.height() - 1)));
        cal.valueX1 = optionalJsonFiniteDouble(params, QStringLiteral("val_x1"), 0.0);
        cal.valueX2 = optionalJsonFiniteDouble(params, QStringLiteral("val_x2"), 1.0);
        cal.logX = optionalJsonBool(params, QStringLiteral("log_x"), false);

        cal.pixelY1 = QPointF(optionalJsonFiniteDouble(params, QStringLiteral("px_y1"), 0.0),
                              optionalJsonFiniteDouble(params, QStringLiteral("py_y1"),
                                                       double(img.height() - 1)));
        cal.pixelY2 = QPointF(optionalJsonFiniteDouble(params, QStringLiteral("px_y2"), 0.0),
                              optionalJsonFiniteDouble(params, QStringLiteral("py_y2"), 0.0));
        cal.valueY1 = optionalJsonFiniteDouble(params, QStringLiteral("val_y1"), 0.0);
        cal.valueY2 = optionalJsonFiniteDouble(params, QStringLiteral("val_y2"), 1.0);
        cal.logY = optionalJsonBool(params, QStringLiteral("log_y"), false);

        if (params.contains(QStringLiteral("cal_points"))
            && !params.value(QStringLiteral("cal_points")).isNull()) {
            if (!params.value(QStringLiteral("cal_points")).isArray())
                throwInvalidParams(
                        QStringLiteral("'cal_points' must be a list of 2 or 4 calibration points."));
            const QJsonArray cp = params.value(QStringLiteral("cal_points")).toArray();
            auto readPtTriplet = [](const QJsonValue &v, double &px, double &py, double &val1,
                                    double *val2 = nullptr) {
                if (v.isArray()) {
                    const QJsonArray a = v.toArray();
                    if (a.size() < (val2 ? 4 : 3))
                        throwInvalidParams(
                                QStringLiteral("Each calibration point array must have >= %1 numbers.")
                                        .arg(val2 ? 4 : 3));
                    px = parseFiniteCoordDouble(a[0], QStringLiteral("cal_points.px"));
                    py = parseFiniteCoordDouble(a[1], QStringLiteral("cal_points.py"));
                    val1 = parseFiniteCoordDouble(a[2], QStringLiteral("cal_points.val"));
                    if (val2)
                        *val2 = parseFiniteCoordDouble(a[3], QStringLiteral("cal_points.val_y"));
                } else if (v.isObject()) {
                    const QJsonObject o = v.toObject();
                    px = optionalJsonFiniteDouble(
                            o, o.contains(QStringLiteral("px")) ? QStringLiteral("px")
                                                                : QStringLiteral("x"),
                            0.0);
                    py = optionalJsonFiniteDouble(
                            o, o.contains(QStringLiteral("py")) ? QStringLiteral("py")
                                                                : QStringLiteral("y"),
                            0.0);
                    val1 = optionalJsonFiniteDouble(
                            o, o.contains(QStringLiteral("val")) ? QStringLiteral("val")
                                                                 : QStringLiteral("val_x"),
                            0.0);
                    if (val2) {
                        *val2 = optionalJsonFiniteDouble(
                                o, o.contains(QStringLiteral("val_y")) ? QStringLiteral("val_y")
                                                                       : QStringLiteral("val"),
                                0.0);
                    }
                } else {
                    throwInvalidParams(
                            QStringLiteral("Each calibration point must be a list or dict."));
                }
            };
            if (cp.size() == 4) {
                double px1 = 0, py1 = 0, vx1 = 0;
                double px2 = 0, py2 = 0, vx2 = 1;
                double pyx1 = 0, pyy1 = 0, vy1 = 0;
                double pyx2 = 0, pyy2 = 0, vy2 = 1;
                readPtTriplet(cp[0], px1, py1, vx1);
                readPtTriplet(cp[1], px2, py2, vx2);
                readPtTriplet(cp[2], pyx1, pyy1, vy1);
                readPtTriplet(cp[3], pyx2, pyy2, vy2);
                cal.pixelX1 = QPointF(px1, py1);
                cal.pixelX2 = QPointF(px2, py2);
                cal.valueX1 = vx1;
                cal.valueX2 = vx2;
                cal.pixelY1 = QPointF(pyx1, pyy1);
                cal.pixelY2 = QPointF(pyx2, pyy2);
                cal.valueY1 = vy1;
                cal.valueY2 = vy2;
            } else if (cp.size() == 3) {
                // 3-point L-shaped calibration: [Origin(px0, py0, x1[, y1]), X_end(px_x2, py_x2, x2), Y_end(px_y2, py_y2, y2)]
                double px0 = 0.0, py0 = 0.0, vx1 = 0.0, vy1 = 0.0;
                if (cp[0].isArray()) {
                    const QJsonArray a0 = cp[0].toArray();
                    if (a0.size() < 3) {
                        throwInvalidParams(QStringLiteral(
                                "3-point cal_points[0] origin must have >= 3 numbers [px, py, val_x(, val_y)]."));
                    }
                    px0 = parseFiniteCoordDouble(a0[0], QStringLiteral("cal_points[0].px"));
                    py0 = parseFiniteCoordDouble(a0[1], QStringLiteral("cal_points[0].py"));
                    vx1 = parseFiniteCoordDouble(a0[2], QStringLiteral("cal_points[0].val_x"));
                    vy1 = (a0.size() >= 4)
                            ? parseFiniteCoordDouble(a0[3], QStringLiteral("cal_points[0].val_y"))
                            : vx1;
                } else if (cp[0].isObject()) {
                    const QJsonObject o0 = cp[0].toObject();
                    px0 = optionalJsonFiniteDouble(
                            o0, o0.contains(QStringLiteral("px")) ? QStringLiteral("px")
                                                                  : QStringLiteral("x"),
                            0.0);
                    py0 = optionalJsonFiniteDouble(
                            o0, o0.contains(QStringLiteral("py")) ? QStringLiteral("py")
                                                                  : QStringLiteral("y"),
                            0.0);
                    vx1 = optionalJsonFiniteDouble(
                            o0, o0.contains(QStringLiteral("val_x"))
                                    ? QStringLiteral("val_x")
                                    : (o0.contains(QStringLiteral("vx")) ? QStringLiteral("vx")
                                                                         : QStringLiteral("val")),
                            0.0);
                    vy1 = optionalJsonFiniteDouble(
                            o0, o0.contains(QStringLiteral("val_y"))
                                    ? QStringLiteral("val_y")
                                    : (o0.contains(QStringLiteral("vy")) ? QStringLiteral("vy")
                                                                         : QStringLiteral("val")),
                            vx1);
                } else {
                    throwInvalidParams(
                            QStringLiteral("Each calibration point must be a list or dict."));
                }

                double pxX2 = 0.0, pyX2 = 0.0, vx2 = 1.0;
                readPtTriplet(cp[1], pxX2, pyX2, vx2);

                double pxY2 = 0.0, pyY2 = 0.0, vy2 = 1.0;
                if (cp[2].isArray()) {
                    const QJsonArray a2 = cp[2].toArray();
                    if (a2.size() < 3) {
                        throwInvalidParams(QStringLiteral(
                                "3-point cal_points[2] Y-axis point must have >= 3 numbers [px, py, val_y]."));
                    }
                    pxY2 = parseFiniteCoordDouble(a2[0], QStringLiteral("cal_points[2].px"));
                    pyY2 = parseFiniteCoordDouble(a2[1], QStringLiteral("cal_points[2].py"));
                    vy2 = (a2.size() >= 4)
                            ? parseFiniteCoordDouble(a2[3], QStringLiteral("cal_points[2].val_y"))
                            : parseFiniteCoordDouble(a2[2], QStringLiteral("cal_points[2].val_y"));
                } else if (cp[2].isObject()) {
                    const QJsonObject o2 = cp[2].toObject();
                    pxY2 = optionalJsonFiniteDouble(
                            o2, o2.contains(QStringLiteral("px")) ? QStringLiteral("px")
                                                                  : QStringLiteral("x"),
                            0.0);
                    pyY2 = optionalJsonFiniteDouble(
                            o2, o2.contains(QStringLiteral("py")) ? QStringLiteral("py")
                                                                  : QStringLiteral("y"),
                            0.0);
                    vy2 = optionalJsonFiniteDouble(
                            o2, o2.contains(QStringLiteral("val_y"))
                                    ? QStringLiteral("val_y")
                                    : (o2.contains(QStringLiteral("vy")) ? QStringLiteral("vy")
                                                                         : QStringLiteral("val")),
                            1.0);
                } else {
                    throwInvalidParams(
                            QStringLiteral("Each calibration point must be a list or dict."));
                }

                cal.pixelX1 = QPointF(px0, py0);
                cal.pixelX2 = QPointF(pxX2, pyX2);
                cal.valueX1 = vx1;
                cal.valueX2 = vx2;
                cal.pixelY1 = QPointF(px0, py0);
                cal.pixelY2 = QPointF(pxY2, pyY2);
                cal.valueY1 = vy1;
                cal.valueY2 = vy2;
            } else if (cp.size() == 2) {
                double px1 = 0, py1 = 0, vx1 = 0, vy1 = 0;
                double px2 = 0, py2 = 0, vx2 = 1, vy2 = 1;
                readPtTriplet(cp[0], px1, py1, vx1, &vy1);
                readPtTriplet(cp[1], px2, py2, vx2, &vy2);
                cal.pixelX1 = QPointF(px1, py1);
                cal.pixelX2 = QPointF(px2, py1);
                cal.valueX1 = vx1;
                cal.valueX2 = vx2;
                cal.pixelY1 = QPointF(px1, py1);
                cal.pixelY2 = QPointF(px1, py2);
                cal.valueY1 = vy1;
                cal.valueY2 = vy2;
            } else {
                throwInvalidParams(
                        QStringLiteral("'cal_points' must contain 4 axis points [X1, X2, Y1, Y2], 3 L-axis points [Origin, X2, Y2], or 2 corner points."));
            }
        }

        const QJsonValue colVal = params.contains(QStringLiteral("color"))
                ? params.value(QStringLiteral("color"))
                : params.value(QStringLiteral("target_color"));
        const QColor targetColor = parseColorValue(colVal, QColor(220, 30, 30));
        const int tol = params.contains(QStringLiteral("tolerance"))
                ? parseBoundedInt(params, QStringLiteral("tolerance"), 50, 1, 255)
                : parseBoundedInt(params, QStringLiteral("color_tol"), 50, 1, 255);
        const int nSamples = params.contains(QStringLiteral("samples"))
                ? parseBoundedInt(params, QStringLiteral("samples"), 100, 5, 4000)
                : (params.contains(QStringLiteral("n_points"))
                           ? parseBoundedInt(params, QStringLiteral("n_points"), 100, 5, 4000)
                           : parseBoundedInt(params, QStringLiteral("points"), 100, 5, 4000));
        QRect roi;
        if (params.contains(QStringLiteral("roi")) && params.value(QStringLiteral("roi")).isArray()) {
            const QJsonArray ra = params.value(QStringLiteral("roi")).toArray();
            if (ra.size() >= 4)
                roi = QRect(ra[0].toInt(), ra[1].toInt(), ra[2].toInt(), ra[3].toInt());
        }

        const QVector<QPointF> pts = GraphDigitizer::autoTraceCurveByColor(img, cal, targetColor,
                                                                           tol, nSamples, roi);
        MultiLayer *outPlot = nullptr;
        MultiLayer **plotPtr =
                optionalJsonBool(params, QStringLiteral("plot"), false) ? &outPlot : nullptr;
        Table *outTbl = GraphDigitizer::exportPointsToTable(
                d_app, pts, optionalJsonString(params, QStringLiteral("table_name")), plotPtr);
        if (outPlot) {
            for (Graph *lg : outPlot->layersList())
                enforceArialGraphFonts(lg);
        }

        QJsonArray xArr, yArr;
        for (const QPointF &pt : pts) {
            xArr.append(safeJsonDouble(pt.x()));
            yArr.append(safeJsonDouble(pt.y()));
        }
        outResult[QStringLiteral("ok")] = !pts.isEmpty();
        outResult[QStringLiteral("count")] = pts.size();
        outResult[QStringLiteral("x")] = xArr;
        outResult[QStringLiteral("y")] = yArr;
        if (outTbl)
            outResult[QStringLiteral("table")] = outTbl->name();
        if (outPlot)
            outResult[QStringLiteral("graph")] = outPlot->name();
        return true;
    }

    return false;
}

} // namespace rpc_internal
