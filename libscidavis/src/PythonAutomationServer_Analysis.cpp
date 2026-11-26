/***************************************************************************
    File                 : PythonAutomationServer_Analysis.cpp
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
                           [Fitting, Gadgets & Signal Processing RPC Handlers]
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

// Headless wrappers setting Filter::d_silent = true & optional Fit::d_suppress_curve_output
class RpcLinearFit : public LinearFit
{
public:
    RpcLinearFit(ApplicationWindow *app, Graph *g) : LinearFit(app, g) { d_silent = true; }
    void setSuppressCurveOutput(bool suppress) { d_suppress_curve_output = suppress; }
    QStringList parameterNames() const { return d_param_names; }
};

class RpcPolynomialFit : public PolynomialFit
{
public:
    RpcPolynomialFit(ApplicationWindow *app, Graph *g, int order, bool showLegend = false)
        : PolynomialFit(app, g, order, showLegend)
    {
        d_silent = true;
    }
    void setSuppressCurveOutput(bool suppress) { d_suppress_curve_output = suppress; }
    QStringList parameterNames() const { return d_param_names; }
};

class RpcNonLinearFit : public NonLinearFit
{
public:
    RpcNonLinearFit(ApplicationWindow *app, Graph *g) : NonLinearFit(app, g) { d_silent = true; }
    void setSuppressCurveOutput(bool suppress) { d_suppress_curve_output = suppress; }
    QStringList parameterNames() const { return d_param_names; }
};

class RpcMultiPeakFit : public MultiPeakFit
{
public:
    RpcMultiPeakFit(ApplicationWindow *app, Graph *g, PeakProfile prof, int peaks = 1)
        : MultiPeakFit(app, g, prof, peaks)
    {
        d_silent = true;
    }
    void setSuppressCurveOutput(bool suppress) { d_suppress_curve_output = suppress; }
    QStringList parameterNames() const { return d_param_names; }
};

class RpcSmoothFilter : public SmoothFilter
{
public:
    RpcSmoothFilter(ApplicationWindow *app, Graph *g, const QString &curveTitle, int m)
        : SmoothFilter(app, g, curveTitle, m)
    {
        d_silent = true;
    }
};

class RpcFFT : public FFT
{
public:
    RpcFFT(ApplicationWindow *app, Graph *g, const QString &curveTitle)
        : FFT(app, g, curveTitle)
    {
        d_silent = true;
    }
    RpcFFT(ApplicationWindow *app, Table *t, const QString &realCol,
           const QString &imagCol = QString())
        : FFT(app, t, realCol, imagCol)
    {
        d_silent = true;
    }
};

class RpcFFTFilter : public FFTFilter
{
public:
    RpcFFTFilter(ApplicationWindow *app, Graph *g, const QString &curveTitle, int m)
        : FFTFilter(app, g, curveTitle, m)
    {
        d_silent = true;
    }
};

QString normalizeImplicitExprForValidation(const QString &raw)
{
    const QString s = raw.trimmed();
    const int eqIdx = s.indexOf(QLatin1Char('='));
    if (eqIdx > 0 && !s.contains(QStringLiteral("==")) && !s.contains(QStringLiteral("!="))
        && !s.contains(QStringLiteral("<=")) && !s.contains(QStringLiteral(">="))) {
        const QString lhs = s.left(eqIdx).trimmed();
        const QString rhs = s.mid(eqIdx + 1).trimmed();
        if (!lhs.isEmpty() && !rhs.isEmpty())
            return QStringLiteral("(%1) - (%2)").arg(lhs, rhs);
    }
    return s;
}

MultiPeakFit::PeakProfile parsePeakProfile(const QString &profStr)
{
    const QString s = profStr.trimmed().toLower();
    if (s.isEmpty() || s == QStringLiteral("gauss") || s == QStringLiteral("gaussian"))
        return MultiPeakFit::Gauss;
    if (s == QStringLiteral("gaussamp") || s == QStringLiteral("gauss_amp")
        || s == QStringLiteral("gaussian_amp"))
        return MultiPeakFit::GaussAmp;
    if (s == QStringLiteral("lorentz") || s == QStringLiteral("lorentzian")
        || s == QStringLiteral("cauchy"))
        return MultiPeakFit::Lorentz;
    if (s == QStringLiteral("pseudovoigt") || s == QStringLiteral("pseudo_voigt")
        || s == QStringLiteral("pseudo-voigt"))
        return MultiPeakFit::PseudoVoigt;
    if (s == QStringLiteral("voigt"))
        return MultiPeakFit::Voigt;
    if (s == QStringLiteral("pearsonvii") || s == QStringLiteral("pearson7")
        || s == QStringLiteral("pearson_vii"))
        return MultiPeakFit::PearsonVII;
    if (s == QStringLiteral("bigaussian") || s == QStringLiteral("bi_gaussian")
        || s == QStringLiteral("splitpearsonvii") || s == QStringLiteral("split_pearson_vii"))
        return MultiPeakFit::BiGaussian;
    if (s == QStringLiteral("emg") || s == QStringLiteral("expmodgauss")
        || s == QStringLiteral("exp_mod_gauss"))
        return MultiPeakFit::EMG;
    if (s == QStringLiteral("lognormal") || s == QStringLiteral("log_normal")
        || s == QStringLiteral("beta"))
        return MultiPeakFit::LogNormal;
    if (s == QStringLiteral("asym2sig") || s == QStringLiteral("asymmetricdoublesig")
        || s == QStringLiteral("asymmetric_double_sig"))
        return MultiPeakFit::Asym2Sig;
    if (s == QStringLiteral("doniachsunjic") || s == QStringLiteral("doniach_sunjic")
        || s == QStringLiteral("doniach-sunjic"))
        return MultiPeakFit::DoniachSunjic;
    throwInvalidParams(
            QStringLiteral("Unknown peak profile '%1' (expected gauss|gauss_amp|lorentz|pseudovoigt|voigt|pearson7|bigaussian|emg|lognormal|asym2sig|doniach_sunjic).")
                    .arg(profStr));
}

MultiPeakFit::BaselineMode parseBaselineMode(const QString &modeStr)
{
    const QString s = modeStr.trimmed().toLower();
    if (s.isEmpty() || s == QStringLiteral("linear") || s == QStringLiteral("endpoints"))
        return MultiPeakFit::BaselineLinear;
    if (s == QStringLiteral("none") || s == QStringLiteral("zero"))
        return MultiPeakFit::BaselineZero;
    if (s == QStringLiteral("min") || s == QStringLiteral("constant") || s == QStringLiteral("y0"))
        return MultiPeakFit::BaselineConstant;
    if (s == QStringLiteral("als") || s == QStringLiteral("snip") || s == QStringLiteral("asls"))
        return MultiPeakFit::BaselineALS;
    if (s == QStringLiteral("anchor") || s == QStringLiteral("anchors")
        || s == QStringLiteral("spline"))
        return MultiPeakFit::BaselineAnchors;
    throwInvalidParams(
            QStringLiteral("Unknown baseline mode '%1' (expected none|constant|linear|als|anchors).")
                    .arg(modeStr));
}

Fit::WeightingMethod parseWeightingMethod(const QString &wStr)
{
    const QString s = wStr.trimmed().toLower();
    if (s.isEmpty() || s == QStringLiteral("none") || s == QStringLiteral("unweighted"))
        return Fit::WeightNone;
    if (s == QStringLiteral("instrumental"))
        return Fit::WeightInstrumental;
    if (s == QStringLiteral("direct"))
        return Fit::WeightDirect;
    if (s == QStringLiteral("statistical") || s == QStringLiteral("poisson"))
        return Fit::WeightStatistical;
    if (s == QStringLiteral("proportional"))
        return Fit::WeightProportional;
    if (s == QStringLiteral("variance"))
        return Fit::WeightVariance;
    if (s == QStringLiteral("dataset") || s == QStringLiteral("arbitrary"))
        return Fit::WeightArbitraryDataset;
    throwInvalidParams(
            QStringLiteral("Unknown weighting method '%1' (expected none|instrumental|direct|statistical|proportional|variance|dataset).")
                    .arg(wStr));
}

RoiGadgetTool::GadgetMode parseRoiGadgetMode(const QString &modeStr)
{
    const QString s = modeStr.trimmed().toLower();
    if (s.isEmpty() || s == QStringLiteral("roi_stats") || s == QStringLiteral("region_stats")
        || s == QStringLiteral("stats"))
        return RoiGadgetTool::RegionStats;
    if (s == QStringLiteral("quick_fit") || s == QStringLiteral("fit"))
        return RoiGadgetTool::QuickFit;
    if (s == QStringLiteral("quick_peaks") || s == QStringLiteral("peaks"))
        return RoiGadgetTool::QuickPeaks;
    if (s == QStringLiteral("quick_sigmoidal") || s == QStringLiteral("sigmoidal"))
        return RoiGadgetTool::QuickSigmoidal;
    if (s == QStringLiteral("integrate") || s == QStringLiteral("integration"))
        return RoiGadgetTool::Integrate;
    if (s == QStringLiteral("interpolate"))
        return RoiGadgetTool::Interpolate;
    if (s == QStringLiteral("fft"))
        return RoiGadgetTool::FFT;
    if (s == QStringLiteral("rise_time") || s == QStringLiteral("risetime"))
        return RoiGadgetTool::RiseTime;
    if (s == QStringLiteral("cluster"))
        return RoiGadgetTool::Cluster;
    if (s == QStringLiteral("mask") || s == QStringLiteral("mask_region"))
        return RoiGadgetTool::MaskRegion;
    if (s == QStringLiteral("unmask") || s == QStringLiteral("unmask_region"))
        return RoiGadgetTool::UnmaskRegion;
    if (s == QStringLiteral("differentiate") || s == QStringLiteral("derivative"))
        return RoiGadgetTool::Differentiate;
    if (s == QStringLiteral("intersect") || s == QStringLiteral("intersections"))
        return RoiGadgetTool::Intersect;
    if (s == QStringLiteral("integrate_2d") || s == QStringLiteral("integrate2d"))
        return RoiGadgetTool::Integrate2D;
    throwInvalidParams(
            QStringLiteral("Unknown ROI gadget mode '%1' (expected roi_stats|quick_fit|quick_peaks|quick_sigmoidal|integrate|interpolate|fft|rise_time|cluster|mask|unmask|differentiate|intersect|integrate_2d).")
                    .arg(modeStr));
}

} // namespace

namespace rpc_internal {

int countValidCurvePoints(QwtPlotCurve *c, int *distinctXOut)
{
    if (distinctXOut)
        *distinctXOut = 0;
    if (!c)
        return 0;
    const auto *dc = dynamic_cast<const DataCurve *>(c);
    const bool checkMasks = (dc != nullptr) && dc->hasAnyMaskedPoints();
    Column *maskXCol = nullptr;
    Column *maskYCol = nullptr;
    if (checkMasks)
        dc->resolveColumns(maskXCol, maskYCol);
    const int n = static_cast<int>(c->dataSize());
    int valid = 0;
    std::vector<double> xs;
    if (distinctXOut && n > 0)
        xs.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (checkMasks && dc->isPointMaskedFast(i, maskXCol, maskYCol))
            continue;
        const double xv = c->x(i);
        const double yv = c->y(i);
        if (std::isfinite(xv) && std::isfinite(yv)) {
            ++valid;
            if (distinctXOut)
                xs.push_back(xv);
        }
    }
    if (distinctXOut && !xs.empty()) {
        std::sort(xs.begin(), xs.end());
        int uniq = 1;
        for (size_t i = 1; i < xs.size(); ++i) {
            if (std::abs(xs[i] - xs[i - 1]) > 1e-15)
                ++uniq;
        }
        *distinctXOut = uniq;
    }
    return valid;
}

void extractValidUnmaskedCurveXY(QwtPlotCurve *c, std::vector<double> &xs, std::vector<double> &ys)
{
    xs.clear();
    ys.clear();
    if (!c)
        return;
    const auto *dc = dynamic_cast<const DataCurve *>(c);
    const int n = static_cast<int>(c->dataSize());
    if (n <= 0)
        return;
    const bool checkMasks = (dc != nullptr) && dc->hasAnyMaskedPoints();
    Column *maskXCol = nullptr;
    Column *maskYCol = nullptr;
    if (checkMasks)
        dc->resolveColumns(maskXCol, maskYCol);
    xs.reserve(static_cast<size_t>(n));
    ys.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (checkMasks && dc->isPointMaskedFast(i, maskXCol, maskYCol))
            continue;
        const double xv = c->x(i);
        const double yv = c->y(i);
        if (std::isfinite(xv) && std::isfinite(yv)) {
            xs.push_back(xv);
            ys.push_back(yv);
        }
    }
}

bool checkMuParserExprValid(const QString &expr, const QStringList &varNames,
                            QString *errMsgOut)
{
    const QString trimmed = expr.trimmed();
    if (trimmed.isEmpty()) {
        if (errMsgOut)
            *errMsgOut = QStringLiteral("Formula expression is empty.");
        return false;
    }
    static const QRegularExpression identRx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    static const QRegularExpression assignRx(
            QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(.+)$"));
    QStringList uniqueVars;
    for (const QString &rawVar : varNames) {
        const QString v = rawVar.trimmed();
        if (v.isEmpty() || !identRx.match(v).hasMatch()) {
            if (errMsgOut)
                *errMsgOut = QStringLiteral("Invalid variable/parameter identifier '%1'.").arg(rawVar);
            return false;
        }
        if (!uniqueVars.contains(v))
            uniqueVars.append(v);
    }

    QStringList nonEmptyLines;
    const QStringList rawLines = trimmed.split(QLatin1Char('\n'));
    for (QString ln : rawLines) {
        const int hashIdx = ln.indexOf(QLatin1Char('#'));
        if (hashIdx >= 0)
            ln = ln.left(hashIdx);
        ln = ln.trimmed();
        if (!ln.isEmpty())
            nonEmptyLines.append(ln);
    }
    if (nonEmptyLines.isEmpty()) {
        if (errMsgOut)
            *errMsgOut = QStringLiteral("Formula expression is empty.");
        return false;
    }

    try {
        MyParser parser;
        std::deque<double> varStorage;
        QHash<QString, double *> boundVars;
        for (int i = 0; i < uniqueVars.size(); ++i) {
            varStorage.push_back(0.5 + 0.13 * static_cast<double>(i));
            double *ptr = &varStorage.back();
            parser.DefineVar(toString<mu::string_type>(uniqueVars[i]), ptr);
            boundVars.insert(uniqueVars[i], ptr);
        }
        for (int li = 0; li + 1 < nonEmptyLines.size(); ++li) {
            const QString &stmt = nonEmptyLines[li];
            const QRegularExpressionMatch m = assignRx.match(stmt);
            if (!m.hasMatch() || m.captured(2).trimmed().startsWith(QLatin1Char('='))) {
                if (errMsgOut) {
                    *errMsgOut = QStringLiteral(
                                         "Leading formula line '%1' must be a 'param = expr' assignment.")
                                         .arg(stmt);
                }
                return false;
            }
            const QString lhs = m.captured(1).trimmed();
            const QString rhs = m.captured(2).trimmed();
            parser.SetExpr(rhs);
            const double cVal = parser.Eval();
            if (!std::isfinite(cVal)) {
                if (errMsgOut) {
                    *errMsgOut = QStringLiteral(
                                         "Parameter assignment '%1' evaluated to non-finite value.")
                                         .arg(lhs);
                }
                return false;
            }
            if (boundVars.contains(lhs)) {
                *boundVars[lhs] = cVal;
            } else {
                varStorage.push_back(cVal);
                double *ptr = &varStorage.back();
                parser.DefineVar(toString<mu::string_type>(lhs), ptr);
                boundVars.insert(lhs, ptr);
            }
        }
        parser.SetExpr(nonEmptyLines.last());
        (void)parser.Eval();
        return true;
    } catch (const mu::ParserError &e) {
        if (errMsgOut)
            *errMsgOut = QStringFromString(e.GetMsg());
        return false;
    } catch (const std::exception &e) {
        if (errMsgOut)
            *errMsgOut = QString::fromUtf8(e.what());
        return false;
    } catch (...) {
        if (errMsgOut)
            *errMsgOut = QStringLiteral("Unknown formula syntax error.");
        return false;
    }
}

void validateMuParserExprOrThrow(const QString &expr, const QStringList &varNames,
                                 const QString &opName)
{
    QString errMsg;
    if (!checkMuParserExprValid(expr, varNames, &errMsg)) {
        throwInvalidParams(
                QStringLiteral("%1: invalid formula '%2': %3").arg(opName, expr, errMsg));
    }
}

int requireCurveMinPoints(Graph *g, const QString &curveTitle, int minPts, const QString &opName,
                          int *distinctXOut)
{
    QwtPlotCurve *c = (g && g->curveIndex(curveTitle) >= 0)
            ? g->curve(g->curveIndex(curveTitle))
            : nullptr;
    if (!c) {
        throwInvalidParams(
                QStringLiteral("%1: curve '%2' not found on graph.").arg(opName, curveTitle));
    }
    int distinctX = 0;
    const int validPts = countValidCurvePoints(c, &distinctX);
    if (distinctXOut)
        *distinctXOut = distinctX;
    if (validPts < minPts) {
        throwInvalidParams(
                QStringLiteral("%1 requires at least %2 valid data points (curve '%3' has %4).")
                        .arg(opName)
                        .arg(minPts)
                        .arg(curveTitle)
                        .arg(validPts));
    }
    return validPts;
}

bool resolveCurveXRange(Graph *g, const QString &curveTitle, const QJsonObject &params,
                        const QString &opName, double &xMin, double &xMax)
{
    const QJsonValue loVal = params.value(QStringLiteral("x_min"));
    const QJsonValue hiVal = params.value(QStringLiteral("x_max"));
    const bool hasLo = !loVal.isUndefined() && !loVal.isNull();
    const bool hasHi = !hiVal.isUndefined() && !hiVal.isNull();
    if (!hasLo && !hasHi)
        return false;
    auto parse = [&](const QJsonValue &v, const QString &key) {
        const double d = v.toDouble(std::numeric_limits<double>::quiet_NaN());
        if (!v.isDouble() || !std::isfinite(d))
            throwInvalidParams(QStringLiteral("%1: '%2' must be a finite number.").arg(opName, key));
        return d;
    };
    if (hasLo)
        xMin = parse(loVal, QStringLiteral("x_min"));
    if (hasHi)
        xMax = parse(hiVal, QStringLiteral("x_max"));
    if (!hasLo || !hasHi) {
        QwtPlotCurve *c = (g && g->curveIndex(curveTitle) >= 0)
                ? g->curve(g->curveIndex(curveTitle))
                : nullptr;
        std::vector<double> xs, ys;
        extractValidUnmaskedCurveXY(c, xs, ys);
        if (xs.empty()) {
            throwInvalidParams(
                    QStringLiteral("%1: curve '%2' has no valid data points.").arg(opName, curveTitle));
        }
        const auto mm = std::minmax_element(xs.cbegin(), xs.cend());
        if (!hasLo)
            xMin = *mm.first;
        if (!hasHi)
            xMax = *mm.second;
    }
    if (!(xMin < xMax)) {
        throwInvalidParams(
                QStringLiteral("%1 requires x_min < x_max (x_min = %2, x_max = %3; a side that is "
                               "not given is the end of the data).")
                        .arg(opName)
                        .arg(xMin, 0, 'g', 10)
                        .arg(xMax, 0, 'g', 10));
    }
    return true;
}

int resolveCurveIndex(Graph *g, const QJsonObject &params, const QString &key, bool required,
                      bool *matchedByLabel)
{
    if (matchedByLabel)
        *matchedByLabel = false;
    if (!g) {
        if (required)
            throwInvalidParams(QStringLiteral("Graph layer is null."));
        return -1;
    }
    // 番号での指定は graph.get_info が曲線に付ける index（レイヤーの項目の並び。ヒートマップ・
    // 等高線も 1 つ数える）。以前はヒートマップを飛ばして数えていたので、ヒートマップのある
    // レイヤーでは get_info の番号と 1 つずれ、set_curve_style などの番号とも食い違って、
    // 別の曲線を消したりフィットしたりしていた。ヒートマップの番号は曲線ではないのでエラー。
    QVector<int> curveIdxOf;
    for (int i = 0; i < g->curves(); ++i) {
        if (g->curve(i))
            curveIdxOf << i;
    }
    const QStringList allCurves = g->curvesList();
    auto byNumber = [&](int idx, const QString &what) -> int {
        if (idx >= 0 && idx < g->curves() && g->curve(idx))
            return idx;
        if (required) {
            if (idx >= 0 && idx < g->curves()) {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("%1 %2 is a heatmap / contour map, not a curve (curve "
                                       "numbers are the indices graph.get_info lists).")
                                .arg(what)
                                .arg(idx),
                        allCurves);
            }
            throwInvalidParamsWithAvailable(
                    QStringLiteral("%1 %2 out of range (curve_count=%3; curve numbers are the "
                                   "indices graph.get_info lists).")
                            .arg(what)
                            .arg(idx)
                            .arg(allCurves.size()),
                    allCurves);
        }
        return -1;
    };
    if (params.contains(key) && !params.value(key).isNull()) {
        const QJsonValue v = params.value(key);
        if (v.isDouble() && !v.isBool())
            return byNumber(parseStrictJsonInt(v, key), QStringLiteral("Curve index"));
        if (!v.isString()) {
            throwInvalidParams(
                    QStringLiteral("Parameter '%1' must be a 0-based curve index or curve name.")
                            .arg(key));
        }
        const QString want = v.toString().trimmed();
        if (!want.isEmpty()) {
            const int exact = g->curveIndex(want);
            if (exact >= 0)
                return exact;
            for (int k = 0; k < curveIdxOf.size(); ++k) {
                if (g->curve(curveIdxOf[k])->title().text().compare(want, Qt::CaseInsensitive)
                    == 0)
                    return curveIdxOf[k];
            }
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (auto *dc = dynamic_cast<DataCurve *>(g->curve(ci))) {
                    if (dc->table()
                        && dc->yColumnName()
                                == dc->table()->name() + QStringLiteral("_") + want) {
                        return ci;
                    }
                }
            }
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (auto *dc = dynamic_cast<DataCurve *>(g->curve(ci))) {
                    if (dc->table()
                        && dc->yColumnName().compare(
                                   dc->table()->name() + QStringLiteral("_") + want,
                                   Qt::CaseInsensitive)
                                == 0) {
                        return ci;
                    }
                }
            }
            // 曲線の独自ラベル（label= で付けた凡例の名前）。タイトルは Y 列のままなので、
            // 同じ Y 列の曲線が複数あるとタイトルでは区別できない。ラベルで当たった曲線の
            // 番号をそのまま返す。
            const int byLabel = g->curveIndexByCustomLabel(want);
            if (byLabel >= 0 && g->curve(byLabel)) {
                if (matchedByLabel)
                    *matchedByLabel = true;
                return byLabel;
            }
            for (int ci = 0; ci < g->curves(); ++ci) {
                QwtPlotCurve *pc = g->curve(ci);
                if (!pc)
                    continue;
                auto *dc = dynamic_cast<DataCurve *>(pc);
                if (!dc || !dc->table()) {
                    const QString ct = pc->title().text();
                    if (ct.endsWith(QStringLiteral("_") + want)
                        || ct.endsWith(QStringLiteral("_") + want, Qt::CaseInsensitive)) {
                        return ci;
                    }
                }
            }
            if (required) {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("Curve '%1' not found on graph.").arg(want), allCurves);
            }
            return -1;
        }
    }
    if (params.contains(QStringLiteral("curve_index"))
        && !params.value(QStringLiteral("curve_index")).isNull()) {
        return byNumber(parseStrictJsonInt(params.value(QStringLiteral("curve_index")),
                                           QStringLiteral("curve_index")),
                        QStringLiteral("curve_index"));
    }
    for (int ci = 0; ci < g->curves(); ++ci) {
        if (g->curve(ci) && g->curveType(ci) != Graph::ErrorBars)
            return ci;
    }
    if (!curveIdxOf.isEmpty())
        return curveIdxOf.first();
    if (required)
        throwInvalidParamsWithAvailable(QStringLiteral("Graph layer has no curves."), allCurves);
    return -1;
}

QString resolveCurveTitle(Graph *g, const QJsonObject &params,
                          const QString &key, bool required)
{
    bool byLabel = false;
    const int idx = resolveCurveIndex(g, params, key, required, &byLabel);
    QwtPlotCurve *c = (g && idx >= 0) ? g->curve(idx) : nullptr;
    if (!c)
        return QString();
    const QString title = c->title().text();
    // 呼び出し側はタイトルで曲線を引き直す（最初に一致した曲線になる）。ラベルで指定された
    // 曲線より前に同じタイトル（同じ Y 列）の曲線があると別の曲線に当たるので、黙って
    // 取り違えずに知らせる。
    if (byLabel && g->curveIndex(title) != idx) {
        throwInvalidParams(
                QStringLiteral("Curve label '%1' names curve %2, but curve %3 plots the same "
                               "column '%4' and this method selects curves by column, so it "
                               "cannot tell them apart. Plot the data in a separate layer or "
                               "graph to use it here.")
                        .arg(params.value(key).toString().trimmed())
                        .arg(idx)
                        .arg(g->curveIndex(title))
                        .arg(title));
    }
    return title;
}

// Resolves either an existing (MultiLayer*, Graph*, curveTitle) OR auto-plots from (table, x_col, y_col)
void resolveGraphAndCurveOrAutoPlot(ApplicationWindow *app, const QJsonObject &params,
                                    MultiLayer *&outMl, Graph *&outGraph, QString &outCurveTitle)
{
    const bool hasExplicitGraph = params.contains(QStringLiteral("graph"))
            || params.contains(QStringLiteral("graph_name"));
    if (!hasExplicitGraph
        && (params.contains(QStringLiteral("table")) || params.contains(QStringLiteral("y_col")))) {
        Table *t = resolveTable(app, params, QStringLiteral("table"), true);
        int xIdx = -1;
        // x_col を名指ししたら、その列と y_col の組で曲線を探す／作る。列の X 指定は書き換えない
        // （以前は X 指定に書き換えたまま残し、しかも作図は y の左の X 列と組にしていたので、
        // 別の X でフィットしていた）。
        const bool explicitX = params.contains(QStringLiteral("x_col"))
                && !params.value(QStringLiteral("x_col")).isNull();
        if (explicitX) {
            xIdx = resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true);
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->colPlotDesignation(c) == SciDAVis::X) {
                    xIdx = c;
                    break;
                }
            }
            if (xIdx < 0 && t->numCols() >= 2) {
                xIdx = 0;
                t->setColPlotDesignation(0, SciDAVis::X);
            }
        }
        int yIdx = -1;
        if (params.contains(QStringLiteral("y_col")) && !params.value(QStringLiteral("y_col")).isNull()) {
            yIdx = resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true);
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != xIdx && t->colPlotDesignation(c) == SciDAVis::Y) {
                    yIdx = c;
                    break;
                }
            }
            if (yIdx < 0)
                yIdx = (t->numCols() >= 2) ? 1 : 0;
        }
        if (t->colPlotDesignation(yIdx) == SciDAVis::noDesignation)
            t->setColPlotDesignation(yIdx, SciDAVis::Y);

        // Reuse an existing curve for this exact column if already plotted, unless auto_plot=true
        const QString wantColFull = fullColumnName(t, yIdx);
        const QString wantXFull = explicitX ? fullColumnName(t, xIdx) : QString();
        // 呼び出し側は曲線をタイトル（Y 列）で引き直す（層の中で最初に一致した曲線になる）ので、
        // x_col を名指ししたときは、その最初の曲線の X 列も一致する層だけを使い回す。
        auto matchesX = [&](Graph *lg, int ci) {
            if (!explicitX)
                return true;
            const auto *dc = dynamic_cast<const DataCurve *>(lg->curve(ci));
            return dc && dc->table() == t && dc->xColumnName() == wantXFull;
        };
        if (!params.value(QStringLiteral("new_graph")).toBool(false)) {
            for (MyWidget *mw : app->windowsList()) {
                if (mw && mw->inherits("MultiLayer")) {
                    auto *candMl = static_cast<MultiLayer *>(mw);
                    for (Graph *lg : candMl->layersList()) {
                        const int ci = lg ? lg->curveIndex(wantColFull) : -1;
                        if (ci >= 0 && matchesX(lg, ci)) {
                            outMl = candMl;
                            outGraph = lg;
                            outCurveTitle = wantColFull;
                            return;
                        }
                    }
                }
            }
        }

        QStringList yCols;
        yCols << wantColFull;
        outMl = app->multilayerPlot(t, yCols, Graph::LineSymbols, 0, t->numRows() - 1, wantXFull);
        if (!outMl)
            throwRpcRuntime(QStringLiteral("Failed to create plot from table '%1'.").arg(t->name()));
        outGraph = resolveGraphLayer(outMl, params, true);
        enforceArialGraphFonts(outGraph);
        outCurveTitle = resolveCurveTitle(outGraph, params, QStringLiteral("curve"), true);
        return;
    }

    outMl = resolveMultiLayer(app, params, QStringLiteral("graph"), true);
    outGraph = resolveGraphLayer(outMl, params, true);
    outCurveTitle = resolveCurveTitle(outGraph, params, QStringLiteral("curve"), true);
}

QJsonObject extractTableAsJson(Table *t, int maxRows)
{
    QJsonObject obj;
    if (!t)
        return obj;
    obj[QStringLiteral("name")] = t->name();
    obj[QStringLiteral("rows")] = t->numRows();
    obj[QStringLiteral("cols")] = t->numCols();
    QJsonArray colNames;
    for (int c = 0; c < t->numCols(); ++c)
        colNames.append(t->colLabel(c));
    obj[QStringLiteral("columns")] = colNames;

    const int rLimit = std::min(t->numRows(), maxRows);
    QJsonArray rowsArr;
    for (int r = 0; r < rLimit; ++r) {
        QJsonObject rowObj;
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            const QString key = t->colLabel(c);
            if (!col || col->isInvalid(r)) {
                rowObj[key] = QJsonValue::Null;
            } else if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                rowObj[key] = safeJsonDouble(col->valueAt(r));
            } else {
                rowObj[key] = col->textAt(r);
            }
        }
        rowsArr.append(rowObj);
    }
    obj[QStringLiteral("data")] = rowsArr;
    return obj;
}

bool dispatchAnalysisAndSignalMethod(ApplicationWindow *d_app, const QString &method,
                                     const QJsonObject &params, QJsonObject &outResult)
{
    // ------------------------------------------------------------------------
    // 6. All Curve Fitting & Peak Analysis (analysis.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("analysis.list_fit_models")) {
        const QString reqCat = optionalJsonString(params, QStringLiteral("category")).trimmed();
        const QStringList names = NonLinearFit::builtInModelNames();
        QJsonArray arr;
        for (const QString &nm : names) {
            const QString cat = NonLinearFit::builtInModelCategory(nm);
            if (!reqCat.isEmpty() && cat.compare(reqCat, Qt::CaseInsensitive) != 0
                && !cat.contains(reqCat, Qt::CaseInsensitive)) {
                continue;
            }
            QJsonObject mObj;
            mObj[QStringLiteral("name")] = nm;
            mObj[QStringLiteral("category")] = cat;
            mObj[QStringLiteral("formula")] = NonLinearFit::builtInModelFormula(nm);
            mObj[QStringLiteral("parameters")] =
                    toJsonArray(NonLinearFit::builtInModelParameters(nm));
            mObj[QStringLiteral("description")] = NonLinearFit::builtInModelDescription(nm);
            arr.append(mObj);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("count")] = arr.size();
        outResult[QStringLiteral("models")] = arr;
        return true;
    }

    if (method == QStringLiteral("analysis.fit_linear")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("analysis.fit_linear"));

        RpcLinearFit lf(d_app, g);
        const bool plotCurve = params.contains(QStringLiteral("plot_curve"))
                ? params.value(QStringLiteral("plot_curve")).toBool(true)
                : true;
        lf.setSuppressCurveOutput(!plotCurve);
        lf.setApparentFit(params.value(QStringLiteral("apparent")).toBool(false));
        if (params.contains(QStringLiteral("scale_errors")))
            lf.scaleErrors(params.value(QStringLiteral("scale_errors")).toBool(true));

        bool dataOk = false;
        double xMin = 0.0, xMax = 0.0;
        if (resolveCurveXRange(g, curveTitle, params, QStringLiteral("analysis.fit_linear"), xMin,
                               xMax)) {
            dataOk = lf.setDataFromCurve(curveTitle, xMin, xMax, g);
        } else {
            dataOk = lf.setDataFromCurve(curveTitle, g);
        }
        if (!dataOk || lf.error())
            throwInvalidParams(
                    QStringLiteral("Insufficient data for linear fit on '%1'.").arg(curveTitle));
        {
            const auto &xv = lf.x();
            if (xv.size() < 2) {
                throwInvalidParams(
                        QStringLiteral("Insufficient data for linear fit on '%1'.").arg(curveTitle));
            }
            double xMean = 0.0;
            for (double v : xv)
                xMean += v;
            xMean /= double(xv.size());
            double sxx = 0.0;
            for (double v : xv) {
                const double dx = v - xMean;
                sxx += dx * dx;
            }
            if (!std::isfinite(sxx) || sxx <= 1e-30) {
                throwInvalidParams(
                        QStringLiteral("Linear fit requires at least 2 distinct X values (collinear X = const on '%1').")
                                .arg(curveTitle));
            }
        }

        if (params.contains(QStringLiteral("weighting"))
            && !params.value(QStringLiteral("weighting")).isNull()) {
            const auto wm =
                    parseWeightingMethod(params.value(QStringLiteral("weighting")).toString());
            QString wCol = params.value(QStringLiteral("weight_col")).toString().trimmed();
            if (wm == Fit::WeightArbitraryDataset && wCol.isEmpty()) {
                throwInvalidParams(
                        QStringLiteral("Weighting 'dataset' requires a valid 'weight_col'."));
            }
            if (!wCol.isEmpty()) {
                Table *wtTbl = nullptr;
                if (params.contains(QStringLiteral("table"))
                    && !params.value(QStringLiteral("table")).isNull()) {
                    wtTbl = resolveTable(d_app, params, QStringLiteral("table"), true);
                } else if (g && g->curveIndex(curveTitle) >= 0) {
                    if (auto *dc = dynamic_cast<DataCurve *>(g->curve(g->curveIndex(curveTitle))))
                        wtTbl = dc->table();
                }
                if (!wtTbl) {
                    throwInvalidParams(
                            QStringLiteral("Cannot resolve source table for 'weight_col' '%1'.")
                                    .arg(wCol));
                }
                const int wi = resolveColumnIndex(wtTbl, wCol, true);
                wCol = fullColumnName(wtTbl, wi);
            }
            if (!lf.setWeightingMethod(wm, wCol)) {
                throwInvalidParams(
                        QStringLiteral("Failed to apply weighting '%1' (weight_col='%2').")
                                .arg(params.value(QStringLiteral("weighting")).toString(), wCol));
            }
        }
        const double confLevel = params.value(QStringLiteral("conf_level")).toDouble(0.95);
        if (params.value(QStringLiteral("conf_band")).toBool(false))
            lf.enableConfidenceBand(true, confLevel);
        if (params.value(QStringLiteral("pred_band")).toBool(false))
            lf.enablePredictionBand(true, confLevel);

        const int curvesBefore = g->curves();
        const bool hadJpBefore = graphHasJapaneseContext(g, curvesBefore);
        ScopedGraphUndoGuard undoGuard(plotCurve ? ml : nullptr, QObject::tr("Linear Fit"));
        lf.fit();
        const auto &res = lf.results();
        if (lf.error() || res.size() < 2 || !std::isfinite(res[0]) || !std::isfinite(res[1])) {
            while (g->curves() > curvesBefore)
                g->removeCurve(g->curves() - 1);
            g->replot();
            throwInvalidParams(
                    QStringLiteral("Linear fit failed to produce finite slope/intercept on '%1'.")
                            .arg(curveTitle));
        }
        sanitizeGraphFunctionCurves(d_app, g);
        harmonizeFitCurveLabelsOnGraph(g, curvesBefore, hadJpBefore, params);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();

        const auto &err = lf.errors();

        const double intercept = res[0];
        const double slope = res[1];
        const double interceptSE =
                (err.size() >= 1) ? err[0] : std::numeric_limits<double>::quiet_NaN();
        const double slopeSE =
                (err.size() >= 2) ? err[1] : std::numeric_limits<double>::quiet_NaN();
        const int dof = lf.degreesOfFreedom();
        double tSlope = std::numeric_limits<double>::quiet_NaN();
        double pValue = std::numeric_limits<double>::quiet_NaN();
        if (dof > 0 && std::isfinite(slopeSE)) {
            if (slopeSE > 0.0) {
                tSlope = std::abs(slope) / slopeSE;
                pValue = 2.0 * gsl_cdf_tdist_Q(tSlope, double(dof));
            } else {
                pValue = lf.anovaPValue();
            }
        }
        (void)tSlope;

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("curve")] = curveTitle;
        outResult[QStringLiteral("slope")] = safeJsonDouble(slope);
        outResult[QStringLiteral("intercept")] = safeJsonDouble(intercept);
        outResult[QStringLiteral("slope_se")] = safeJsonDouble(slopeSE);
        outResult[QStringLiteral("intercept_se")] = safeJsonDouble(interceptSE);
        outResult[QStringLiteral("r2")] = safeJsonDouble(lf.rSquare());
        outResult[QStringLiteral("adj_r2")] = safeJsonDouble(lf.adjustedRSquare());
        outResult[QStringLiteral("pearson_r")] = safeJsonDouble(lf.pearsonR());
        outResult[QStringLiteral("rss")] = safeJsonDouble(lf.rss());
        outResult[QStringLiteral("chi2_dof")] = safeJsonDouble(lf.reducedChiSquare());
        outResult[QStringLiteral("aic")] = safeJsonDouble(lf.aic());
        outResult[QStringLiteral("aicc")] = safeJsonDouble(lf.aicc());
        outResult[QStringLiteral("bic")] = safeJsonDouble(lf.bic());
        outResult[QStringLiteral("f_value")] = safeJsonDouble(lf.anovaFValue());
        outResult[QStringLiteral("p_value")] = safeJsonDouble(pValue);
        outResult[QStringLiteral("n")] = lf.dataSize();
        return true;
    }

    if (method == QStringLiteral("analysis.fit_york")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        Table *t = resolveTable(d_app, params, QStringLiteral("table"), false);
        QString xErrFull = params.value(QStringLiteral("x_err_col")).toString().trimmed();
        QString yErrFull = params.value(QStringLiteral("y_err_col")).toString().trimmed();
        if (t) {
            if (!xErrFull.isEmpty()) {
                const int xi = resolveColumnIndex(t, xErrFull, false);
                if (xi >= 0)
                    xErrFull = fullColumnName(t, xi);
            }
            if (!yErrFull.isEmpty()) {
                const int yi = resolveColumnIndex(t, yErrFull, false);
                if (yi >= 0)
                    yErrFull = fullColumnName(t, yi);
            }
        }

        RpcLinearFit lf(d_app, g);
        requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("analysis.fit_york"));
        lf.setSuppressCurveOutput(!params.value(QStringLiteral("plot_curve")).toBool(true));
        if (!lf.setDataFromCurve(curveTitle, g) || lf.error())
            throwInvalidParams(
                    QStringLiteral("Insufficient data for York fit on '%1'.").arg(curveTitle));

        const int curvesBefore = g->curves();
        const bool hadJpBefore = graphHasJapaneseContext(g, curvesBefore);
        const bool plotYorkCurve = params.value(QStringLiteral("plot_curve")).toBool(true);
        ScopedGraphUndoGuard undoGuard(plotYorkCurve ? ml : nullptr, QObject::tr("York Fit"));
        const bool ok = lf.fitWithXYErrors(xErrFull, yErrFull);
        if (!ok)
            throwRpcRuntime(QStringLiteral("York XY-error regression failed to converge."));
        sanitizeGraphFunctionCurves(d_app, g);
        harmonizeFitCurveLabelsOnGraph(g, curvesBefore, hadJpBefore, params);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("converged")] = ok;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("slope")] = safeJsonDouble(lf.yorkSlope());
        outResult[QStringLiteral("intercept")] = safeJsonDouble(lf.yorkIntercept());
        outResult[QStringLiteral("slope_se")] = safeJsonDouble(lf.yorkSlopeError());
        outResult[QStringLiteral("intercept_se")] = safeJsonDouble(lf.yorkInterceptError());
        outResult[QStringLiteral("r2")] = safeJsonDouble(lf.rSquare());
        outResult[QStringLiteral("chi2_dof")] = safeJsonDouble(lf.reducedChiSquare());
        return true;
    }

    if (method == QStringLiteral("analysis.fit_polynomial")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        int order = 2;
        if (params.contains(QStringLiteral("order")) && !params.value(QStringLiteral("order")).isNull()) {
            order = parseStrictJsonInt(params.value(QStringLiteral("order")), QStringLiteral("order"));
            if (order < 1 || order > 9)
                throwInvalidParams(
                        QStringLiteral("Polynomial 'order' (%1) must be between 1 and 9.").arg(order));
        }
        requireCurveMinPoints(g, curveTitle, order + 1, QStringLiteral("analysis.fit_polynomial"));
        const bool showLegend = params.value(QStringLiteral("show_legend")).toBool(false);
        RpcPolynomialFit pf(d_app, g, order, showLegend);
        pf.setSuppressCurveOutput(!params.value(QStringLiteral("plot_curve")).toBool(true));
        if (!pf.setDataFromCurve(curveTitle, g) || pf.error())
            throwInvalidParams(
                    QStringLiteral("Insufficient data for polynomial fit (order %1).").arg(order));
        const double confLevel = params.value(QStringLiteral("conf_level")).toDouble(0.95);
        if (params.value(QStringLiteral("conf_band")).toBool(false))
            pf.enableConfidenceBand(true, confLevel);
        if (params.value(QStringLiteral("pred_band")).toBool(false))
            pf.enablePredictionBand(true, confLevel);

        const int curvesBefore = g->curves();
        const bool hadJpBefore = graphHasJapaneseContext(g, curvesBefore);
        const bool plotPolyCurve = params.value(QStringLiteral("plot_curve")).toBool(true);
        ScopedGraphUndoGuard undoGuard(plotPolyCurve ? ml : nullptr, QObject::tr("Polynomial Fit"));
        pf.fit();
        sanitizeGraphFunctionCurves(d_app, g);
        harmonizeFitCurveLabelsOnGraph(g, curvesBefore, hadJpBefore, params);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("order")] = order;
        outResult[QStringLiteral("param_names")] = toJsonArray(pf.parameterNames());
        outResult[QStringLiteral("params")] = toJsonArray(pf.results());
        outResult[QStringLiteral("errors")] = toJsonArray(pf.errors());
        outResult[QStringLiteral("r2")] = safeJsonDouble(pf.rSquare());
        outResult[QStringLiteral("adj_r2")] = safeJsonDouble(pf.adjustedRSquare());
        outResult[QStringLiteral("chi2_dof")] = safeJsonDouble(pf.reducedChiSquare());
        outResult[QStringLiteral("aic")] = safeJsonDouble(pf.aic());
        outResult[QStringLiteral("aicc")] = safeJsonDouble(pf.aicc());
        outResult[QStringLiteral("bic")] = safeJsonDouble(pf.bic());
        return true;
    }

    if (method == QStringLiteral("analysis.fit_nonlinear")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        RpcNonLinearFit nlf(d_app, g);
        const bool plotCurve = params.contains(QStringLiteral("plot_curve"))
                ? optionalJsonBool(params, QStringLiteral("plot_curve"), true)
                : true;
        nlf.setSuppressCurveOutput(!plotCurve);
        nlf.setMaximumIterations(
                parseBoundedInt(params, QStringLiteral("max_iter"), 1000, 1, 50000));
        nlf.setTolerance(std::max(
                1e-15, optionalJsonFiniteDouble(params, QStringLiteral("tolerance"), 1e-7, 1.0)));
        if (params.contains(QStringLiteral("points"))
            && !params.value(QStringLiteral("points")).isNull()) {
            nlf.generateFunction(
                    true, parseBoundedInt(params, QStringLiteral("points"), 200, 10, 1000000));
        }

        const QString modelName = optionalJsonString(params, QStringLiteral("model")).trimmed();
        const QString customFormula =
                optionalJsonString(params, QStringLiteral("formula")).trimmed();
        if (!modelName.isEmpty() && !customFormula.isEmpty()) {
            throwInvalidParams(
                    QStringLiteral("Provide either 'model' or 'formula' in analysis.fit_nonlinear, not both."));
        }
        QString resolvedBuiltInModel = modelName;
        bool isLinearBuiltIn = false;
        if (!modelName.isEmpty()) {
            const QString mLow = modelName.toLower();
            if (mLow == QStringLiteral("linear") || mLow == QStringLiteral("line"))
                isLinearBuiltIn = true;
            else if (mLow == QStringLiteral("gaussian") || mLow == QStringLiteral("normal"))
                resolvedBuiltInModel = QStringLiteral("Gauss");
            else if (mLow == QStringLiteral("gaussianamp") || mLow == QStringLiteral("gaussian_amp")
                     || mLow == QStringLiteral("gauss_amp"))
                resolvedBuiltInModel = QStringLiteral("GaussAmp");
            else if (mLow == QStringLiteral("lorentzian") || mLow == QStringLiteral("cauchy"))
                resolvedBuiltInModel = QStringLiteral("Lorentz");
            else if (mLow == QStringLiteral("sigmoidal") || mLow == QStringLiteral("sigmoid"))
                resolvedBuiltInModel = QStringLiteral("Boltzmann");
            else if (mLow == QStringLiteral("exponential") || mLow == QStringLiteral("expdecay")
                     || mLow == QStringLiteral("exponential_decay"))
                resolvedBuiltInModel = QStringLiteral("ExpDec1");
            else if (mLow == QStringLiteral("exponential_growth"))
                resolvedBuiltInModel = QStringLiteral("ExpGrow1");
        }
        bool customFormulaSyntaxOk = true;
        if (isLinearBuiltIn) {
            nlf.setParametersList(QStringList{ QStringLiteral("A"), QStringLiteral("B") });
            nlf.setFormula(QStringLiteral("A + B*x"));
        } else if (!resolvedBuiltInModel.isEmpty()) {
            if (!nlf.setBuiltInModel(resolvedBuiltInModel)) {
                QStringList availModels = NonLinearFit::builtInModelNames();
                if (!availModels.contains(QStringLiteral("Linear"), Qt::CaseInsensitive))
                    availModels.prepend(QStringLiteral("Linear"));
                throwInvalidParamsWithAvailable(
                        QStringLiteral("Unknown built-in nonlinear fit model '%1'.").arg(modelName),
                        availModels);
            }
        } else if (!customFormula.isEmpty()) {
            QStringList pNames;
            for (const QJsonValue &pv : params.value(QStringLiteral("param_names")).toArray()) {
                const QString pn = pv.toString().trimmed();
                if (!pn.isEmpty())
                    pNames << pn;
            }
            if (pNames.isEmpty())
                throwInvalidParams(QStringLiteral(
                        "Custom nonlinear 'formula' requires non-empty 'param_names' array."));
            QStringList checkVars = pNames;
            checkVars << QStringLiteral("x");
            validateMuParserExprOrThrow(customFormula, checkVars,
                                        QStringLiteral("analysis.fit_nonlinear"));
            customFormulaSyntaxOk = true;
            nlf.setParametersList(pNames);
            nlf.setFormula(customFormula);
        } else {
            throwInvalidParams(
                    QStringLiteral("Provide either built-in 'model' name or custom 'formula'."));
        }

        bool dataOk = false;
        requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("analysis.fit_nonlinear"));
        double xMin = 0.0, xMax = 0.0;
        if (resolveCurveXRange(g, curveTitle, params, QStringLiteral("analysis.fit_nonlinear"),
                               xMin, xMax)) {
            dataOk = nlf.setDataFromCurve(curveTitle, xMin, xMax, g);
        } else {
            dataOk = nlf.setDataFromCurve(curveTitle, g);
        }
        if (!dataOk || nlf.error())
            throwInvalidParams(
                    QStringLiteral("Insufficient data on curve '%1' for nonlinear fit.").arg(curveTitle));

        if (isLinearBuiltIn) {
            const auto &xv = nlf.x();
            const auto &yv = nlf.y();
            double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
            const size_t nLin = std::min(xv.size(), yv.size());
            for (size_t i = 0; i < nLin; ++i) {
                sx += xv[i];
                sy += yv[i];
                sxx += xv[i] * xv[i];
                sxy += xv[i] * yv[i];
            }
            const double denom = double(nLin) * sxx - sx * sx;
            const double bGuess =
                    (nLin >= 2 && std::abs(denom) > 1e-15) ? ((double(nLin) * sxy - sx * sy) / denom) : 1.0;
            const double aGuess = (nLin >= 1) ? ((sy - bGuess * sx) / double(nLin)) : 0.0;
            nlf.setInitialGuess(0, aGuess);
            nlf.setInitialGuess(1, bGuess);
        } else if (!resolvedBuiltInModel.isEmpty())
            nlf.guessBuiltInParameters(resolvedBuiltInModel);
        else if (customFormulaSyntaxOk)
            nlf.guessInitialValues();

        const QStringList pNames = nlf.parameterNames();
        // A name that is not a parameter of the model (e.g. "Y0" for "y0") must not be dropped
        // silently: the fit would run without the fixed value / bound / initial value.
        auto requireKnownNames = [&](const QStringList &names, const QString &key) {
            QStringList unknown;
            for (const QString &n : names) {
                if (!pNames.contains(n))
                    unknown << n;
            }
            if (!unknown.isEmpty()) {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("analysis.fit_nonlinear: unknown parameter name(s) in '%1': "
                                       "%2 (names are case-sensitive).")
                                .arg(key, unknown.join(QStringLiteral(", "))),
                        pNames);
            }
        };
        auto requireAtMostParams = [&](int count, const QString &key) {
            if (count > pNames.size()) {
                throwInvalidParamsWithAvailable(
                        QStringLiteral("analysis.fit_nonlinear: '%1' has %2 entries but the model "
                                       "has %3 parameters.")
                                .arg(key)
                                .arg(count)
                                .arg(pNames.size()),
                        pNames);
            }
        };
        // Explicit initial guesses (dict by param name OR array by index)
        const QString initKey = params.contains(QStringLiteral("initial_values"))
                ? QStringLiteral("initial_values")
                : QStringLiteral("init_params");
        const QJsonValue initGuessVal = params.value(initKey);
        if (!initGuessVal.isUndefined() && !initGuessVal.isNull()) {
            if (initGuessVal.isArray()) {
                const QJsonArray ia = initGuessVal.toArray();
                requireAtMostParams(ia.size(), initKey);
                for (int i = 0; i < std::min(pNames.size(), ia.size()); ++i)
                    nlf.setInitialGuess(i, ia[i].toDouble());
            } else if (initGuessVal.isObject()) {
                const QJsonObject io = initGuessVal.toObject();
                requireKnownNames(io.keys(), initKey);
                for (int i = 0; i < pNames.size(); ++i) {
                    if (io.contains(pNames[i]))
                        nlf.setInitialGuess(i, io.value(pNames[i]).toDouble());
                }
            }
        }

        // Fixed parameter flags
        if (params.contains(QStringLiteral("fixed"))) {
            const QJsonValue fv = params.value(QStringLiteral("fixed"));
            if (fv.isArray()) {
                const QJsonArray fa = fv.toArray();
                QStringList names;
                int boolCount = 0;
                for (const QJsonValue &v : fa) {
                    if (v.isString())
                        names << v.toString();
                    else if (v.isBool())
                        ++boolCount;
                }
                requireKnownNames(names, QStringLiteral("fixed"));
                if (names.isEmpty())
                    requireAtMostParams(boolCount, QStringLiteral("fixed"));
                for (int i = 0; i < fa.size(); ++i) {
                    if (fa[i].isBool() && i < pNames.size())
                        nlf.setParameterFixed(i, fa[i].toBool());
                    else if (fa[i].isString()) {
                        const int idx = pNames.indexOf(fa[i].toString());
                        if (idx >= 0)
                            nlf.setParameterFixed(idx, true);
                    }
                }
            } else if (fv.isObject()) {
                const QJsonObject fo = fv.toObject();
                requireKnownNames(fo.keys(), QStringLiteral("fixed"));
                for (int i = 0; i < pNames.size(); ++i) {
                    if (fo.contains(pNames[i]))
                        nlf.setParameterFixed(i, fo.value(pNames[i]).toBool());
                }
            }
        }

        // Box bounds
        if (params.contains(QStringLiteral("bounds"))
            && params.value(QStringLiteral("bounds")).isObject()) {
            const QJsonObject bo = params.value(QStringLiteral("bounds")).toObject();
            requireKnownNames(bo.keys(), QStringLiteral("bounds"));
            for (int i = 0; i < pNames.size(); ++i) {
                if (bo.contains(pNames[i]) && bo.value(pNames[i]).isArray()) {
                    const QJsonArray ba = bo.value(pNames[i]).toArray();
                    if (ba.size() >= 2) {
                        // An open side is infinite (as in the fit dialog): a finite stand-in
                        // such as 1e30 made a two-sided box whose start point was clamped to
                        // lo + 1e-4 * 1e30, far from the initial value (t = 1e26 "converged").
                        const double lo = ba[0].isNull() ? -std::numeric_limits<double>::infinity()
                                                         : ba[0].toDouble();
                        const double hi = ba[1].isNull() ? std::numeric_limits<double>::infinity()
                                                         : ba[1].toDouble();
                        // Swapping a reversed pair silently would fit a box the caller did
                        // not mean (often a typo in one of the two numbers).
                        if (lo > hi) {
                            throwInvalidParams(
                                    QStringLiteral("analysis.fit_nonlinear: bounds for '%1' have "
                                                   "lower %2 > upper %3; give [lower, upper].")
                                            .arg(pNames[i])
                                            .arg(lo, 0, 'g', 10)
                                            .arg(hi, 0, 'g', 10));
                        }
                        nlf.setParameterBounds(i, lo, hi);
                    }
                }
            }
        }

        // Linear equality/inequality constraints
        if (params.contains(QStringLiteral("constraints"))) {
            const QString cExpr = params.value(QStringLiteral("constraints")).toString().trimmed();
            if (!cExpr.isEmpty())
                nlf.setLinearConstraintExpr(cExpr);
        }

        if (params.contains(QStringLiteral("weighting"))
            && !params.value(QStringLiteral("weighting")).isNull()) {
            const auto wm =
                    parseWeightingMethod(params.value(QStringLiteral("weighting")).toString());
            QString wCol = params.value(QStringLiteral("weight_col")).toString().trimmed();
            if (wm == Fit::WeightArbitraryDataset && wCol.isEmpty()) {
                throwInvalidParams(
                        QStringLiteral("Weighting 'dataset' requires a valid 'weight_col'."));
            }
            if (!wCol.isEmpty()) {
                Table *wtTbl = nullptr;
                if (params.contains(QStringLiteral("table"))
                    && !params.value(QStringLiteral("table")).isNull()) {
                    wtTbl = resolveTable(d_app, params, QStringLiteral("table"), true);
                } else if (g && g->curveIndex(curveTitle) >= 0) {
                    if (auto *dc = dynamic_cast<DataCurve *>(g->curve(g->curveIndex(curveTitle))))
                        wtTbl = dc->table();
                }
                if (!wtTbl) {
                    throwInvalidParams(
                            QStringLiteral("Cannot resolve source table for 'weight_col' '%1'.")
                                    .arg(wCol));
                }
                const int wi = resolveColumnIndex(wtTbl, wCol, true);
                wCol = fullColumnName(wtTbl, wi);
            }
            if (!nlf.setWeightingMethod(wm, wCol)) {
                throwInvalidParams(
                        QStringLiteral("Failed to apply weighting '%1' (weight_col='%2').")
                                .arg(params.value(QStringLiteral("weighting")).toString(), wCol));
            }
        }

        const double confLevel = params.value(QStringLiteral("conf_level")).toDouble(0.95);
        if (params.value(QStringLiteral("conf_band")).toBool(false))
            nlf.enableConfidenceBand(true, confLevel);
        if (params.value(QStringLiteral("pred_band")).toBool(false))
            nlf.enablePredictionBand(true, confLevel);

        const QString algo = params.value(QStringLiteral("algorithm")).toString().toLower();
        if (algo == QStringLiteral("simplex") || algo == QStringLiteral("nelder_mead"))
            nlf.setAlgorithm(Fit::NelderMeadSimplex);
        else if (algo == QStringLiteral("lm_unscaled"))
            nlf.setAlgorithm(Fit::UnscaledLevenbergMarquardt);

        const int curvesBefore = g->curves();
        const bool hadJpBefore = graphHasJapaneseContext(g, curvesBefore);
        ScopedGraphUndoGuard undoGuard(plotCurve ? ml : nullptr, QObject::tr("Nonlinear Fit"));
        bool fitExecuted = false;
        if (customFormulaSyntaxOk) {
            nlf.fit();
            fitExecuted = true;
        }
        sanitizeGraphFunctionCurves(d_app, g);
        harmonizeFitCurveLabelsOnGraph(g, curvesBefore, hadJpBefore, params);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();

        const std::vector<double> resVec =
                fitExecuted ? nlf.results() : std::vector<double>{};
        const std::vector<double> errVec =
                (fitExecuted && !nlf.error()) ? nlf.errors() : std::vector<double>{};
        QJsonObject paramDict, errDict;
        for (int i = 0; i < pNames.size() && size_t(i) < resVec.size(); ++i) {
            paramDict[pNames[i]] = safeJsonDouble(resVec[size_t(i)]);
            errDict[pNames[i]] =
                    safeJsonDouble((size_t(i) < errVec.size()) ? errVec[size_t(i)] : 0.0);
        }

        const double chi2Val = nlf.chiSquare();
        const bool validChi2 = customFormulaSyntaxOk && std::isfinite(chi2Val) && chi2Val >= 0.0;
        const bool converged = validChi2 && !nlf.error() && !resVec.empty() && nlf.converged()
                && std::all_of(resVec.begin(), resVec.end(),
                               [](double v) { return std::isfinite(v); });

        QString convergenceMsg = nlf.convergenceMessage();
        if (!converged && convergenceMsg.isEmpty()) {
            // The solver stopped with an error (e.g. exp overflow at the parameters it reached):
            // there is no result to report, so say why instead of an empty message.
            convergenceMsg = QObject::tr(
                    "Not converged (the fit failed: the model gave no finite values at the "
                    "parameters the solver reached; check the initial values)");
        }
        outResult[QStringLiteral("ok")] = converged;
        outResult[QStringLiteral("converged")] = converged;
        outResult[QStringLiteral("convergence_message")] = convergenceMsg;
        if (fitExecuted && !nlf.fitNotes().isEmpty())
            outResult[QStringLiteral("notes")] = QJsonArray::fromStringList(nlf.fitNotes());
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("curve")] = curveTitle;
        outResult[QStringLiteral("model")] = modelName.isEmpty() ? customFormula : modelName;
        outResult[QStringLiteral("formula")] = customFormulaSyntaxOk ? nlf.formula() : customFormula;
        outResult[QStringLiteral("param_names")] = toJsonArray(pNames);
        outResult[QStringLiteral("params")] = paramDict;
        outResult[QStringLiteral("param_values")] = toJsonArray(resVec);
        outResult[QStringLiteral("errors")] = errDict;
        outResult[QStringLiteral("error_values")] = toJsonArray(errVec);
        outResult[QStringLiteral("chi2")] = validChi2 ? safeJsonDouble(chi2Val) : QJsonValue::Null;
        outResult[QStringLiteral("chi2_dof")] =
                validChi2 ? safeJsonDouble(nlf.reducedChiSquare()) : QJsonValue::Null;
        outResult[QStringLiteral("rss")] =
                validChi2 ? safeJsonDouble(nlf.rss()) : QJsonValue::Null;
        // Undefined R^2 (failed fit, no residual degrees of freedom, constant y) is null; note
        // that std::min(1.0, NaN) is 1.0, so the clamp must not see a NaN.
        auto r2Json = [&](double v) -> QJsonValue {
            if (!validChi2 || !std::isfinite(v))
                return QJsonValue(QJsonValue::Null);
            return QJsonValue(std::min(1.0, v));
        };
        outResult[QStringLiteral("r2")] = r2Json(nlf.rSquare());
        outResult[QStringLiteral("adj_r2")] = r2Json(nlf.adjustedRSquare());
        outResult[QStringLiteral("aic")] =
                validChi2 ? safeJsonDouble(nlf.aic()) : QJsonValue::Null;
        outResult[QStringLiteral("aicc")] =
                validChi2 ? safeJsonDouble(nlf.aicc()) : QJsonValue::Null;
        outResult[QStringLiteral("bic")] =
                validChi2 ? safeJsonDouble(nlf.bic()) : QJsonValue::Null;
        outResult[QStringLiteral("f_value")] =
                validChi2 ? safeJsonDouble(nlf.anovaFValue()) : QJsonValue::Null;
        outResult[QStringLiteral("p_value")] =
                validChi2 ? safeJsonDouble(nlf.anovaPValue()) : QJsonValue::Null;

        const int bootSamples = params.contains(QStringLiteral("bootstrap"))
                ? parseBoundedInt(params, QStringLiteral("bootstrap"), 0, 0, 10000)
                : parseBoundedInt(params, QStringLiteral("bootstrap_samples"), 0, 0, 10000);
        if (bootSamples > 0 && converged) {
            const auto boot = nlf.runBootstrapCI(bootSamples, confLevel, 0);
            QJsonObject bootObj;
            bootObj[QStringLiteral("valid")] = boot.valid;
            bootObj[QStringLiteral("n_success")] = boot.nSuccess;
            QJsonObject ciByParam;
            for (int i = 0; i < pNames.size() && i < boot.ciLow.size() && i < boot.ciHigh.size();
                 ++i) {
                QJsonArray pair;
                pair.append(safeJsonDouble(boot.ciLow[i]));
                pair.append(safeJsonDouble(boot.ciHigh[i]));
                ciByParam[pNames[i]] = pair;
            }
            bootObj[QStringLiteral("ci")] = ciByParam;
            bootObj[QStringLiteral("ci_low")] = toJsonArray(boot.ciLow);
            bootObj[QStringLiteral("ci_high")] = toJsonArray(boot.ciHigh);
            if (boot.reportTable)
                bootObj[QStringLiteral("table")] = boot.reportTable->name();
            outResult[QStringLiteral("bootstrap_ci")] = bootObj;
        }
        return true;
    }

    if (method == QStringLiteral("analysis.fit_global")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        QStringList curveTitles;
        if (params.value(QStringLiteral("curves")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("curves")).toArray()) {
                QJsonObject tmp;
                tmp[QStringLiteral("curve")] = cv;
                curveTitles << resolveCurveTitle(g, tmp, QStringLiteral("curve"), true);
            }
        } else {
            curveTitles = g->analysableCurvesList();
        }
        if (curveTitles.size() < 2)
            throwInvalidParams(
                    QStringLiteral("Global fit requires at least 2 curves on the graph."));

        RpcNonLinearFit nlf(d_app, g);
        nlf.setMaximumIterations(
                parseBoundedInt(params, QStringLiteral("max_iter"), 200, 1, 50000));
        nlf.setTolerance(std::max(
                1e-14, optionalJsonFiniteDouble(params, QStringLiteral("tolerance"), 1e-4, 1.0)));
        const QString modelName = optionalJsonString(params, QStringLiteral("model")).trimmed();
        const QString customFormula =
                optionalJsonString(params, QStringLiteral("formula")).trimmed();
        if (!modelName.isEmpty() && !customFormula.isEmpty()) {
            throwInvalidParams(
                    QStringLiteral("Provide either 'model' or 'formula' in analysis.fit_global, not both."));
        }
        if (!modelName.isEmpty()) {
            QString resolvedGlobalModel = modelName;
            const QString mLow = modelName.toLower();
            if (mLow == QStringLiteral("linear") || mLow == QStringLiteral("line")) {
                nlf.setParametersList(QStringList{ QStringLiteral("A"), QStringLiteral("B") });
                nlf.setFormula(QStringLiteral("A + B*x"));
            } else {
                if (mLow == QStringLiteral("gaussian") || mLow == QStringLiteral("normal"))
                    resolvedGlobalModel = QStringLiteral("Gauss");
                else if (mLow == QStringLiteral("gaussianamp")
                         || mLow == QStringLiteral("gaussian_amp")
                         || mLow == QStringLiteral("gauss_amp"))
                    resolvedGlobalModel = QStringLiteral("GaussAmp");
                else if (mLow == QStringLiteral("lorentzian") || mLow == QStringLiteral("cauchy"))
                    resolvedGlobalModel = QStringLiteral("Lorentz");
                else if (mLow == QStringLiteral("sigmoidal") || mLow == QStringLiteral("sigmoid"))
                    resolvedGlobalModel = QStringLiteral("Boltzmann");
                else if (mLow == QStringLiteral("exponential")
                         || mLow == QStringLiteral("expdecay")
                         || mLow == QStringLiteral("exponential_decay"))
                    resolvedGlobalModel = QStringLiteral("ExpDec1");
                else if (mLow == QStringLiteral("exponential_growth"))
                    resolvedGlobalModel = QStringLiteral("ExpGrow1");
                if (!nlf.setBuiltInModel(resolvedGlobalModel)) {
                    QStringList availModels = NonLinearFit::builtInModelNames();
                    if (!availModels.contains(QStringLiteral("Linear"), Qt::CaseInsensitive))
                        availModels.prepend(QStringLiteral("Linear"));
                    throwInvalidParamsWithAvailable(
                            QStringLiteral("Unknown built-in model '%1'.").arg(modelName),
                            availModels);
                }
            }
        } else if (!customFormula.isEmpty()) {
            QStringList pNames;
            for (const QJsonValue &pv : params.value(QStringLiteral("param_names")).toArray()) {
                const QString pn = pv.toString().trimmed();
                if (!pn.isEmpty())
                    pNames << pn;
            }
            if (pNames.isEmpty()) {
                throwInvalidParams(QStringLiteral(
                        "Custom global fit 'formula' requires non-empty 'param_names' array."));
            }
            QStringList checkVars = pNames;
            checkVars << QStringLiteral("x");
            validateMuParserExprOrThrow(customFormula, checkVars,
                                        QStringLiteral("analysis.fit_global"));
            nlf.setParametersList(pNames);
            nlf.setFormula(customFormula);
        } else {
            throwInvalidParams(QStringLiteral("Global fit requires 'model' or 'formula'."));
        }

        const QStringList pNames = nlf.parameterNames();
        QVector<bool> sharedFlags(pNames.size(), false);
        const QJsonValue sv = params.contains(QStringLiteral("shared"))
                ? params.value(QStringLiteral("shared"))
                : params.value(QStringLiteral("shared_params"));
        if (sv.isArray()) {
            const QJsonArray sa = sv.toArray();
            for (int i = 0; i < sa.size(); ++i) {
                if (sa[i].isBool() && i < sharedFlags.size()) {
                    sharedFlags[i] = sa[i].toBool();
                } else if (sa[i].isString()) {
                    const int idx = pNames.indexOf(sa[i].toString());
                    if (idx >= 0)
                        sharedFlags[idx] = true;
                }
            }
        } else if (sv.isObject()) {
            const QJsonObject so = sv.toObject();
            for (int i = 0; i < pNames.size(); ++i)
                sharedFlags[i] = so.value(pNames[i]).toBool(false);
        } else {
            if (!sharedFlags.isEmpty())
                sharedFlags.last() = true;
        }

        const int beforeCurves = g->curves();
        Table *sumTbl = nullptr;
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Global Fit"));
        const bool ok = nlf.fitGlobal(curveTitles, sharedFlags, &sumTbl);
        if (ok && sumTbl && g->curves() >= beforeCurves + curveTitles.size()) {
            const int P = pNames.size();
            const QStringList formulaLines =
                    nlf.formula().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            const QString fallbackExpr =
                    formulaLines.isEmpty() ? QString() : formulaLines.last().trimmed();
            for (int m = 0; m < curveTitles.size(); ++m) {
                if (auto *fc = dynamic_cast<FunctionCurve *>(g->plotItem(beforeCurves + m))) {
                    const QStringList fcLines =
                            fc->formulas().isEmpty() ? QStringList()
                                                     : fc->formulas().first().split(
                                                               QLatin1Char('\n'),
                                                               Qt::SkipEmptyParts);
                    const QString rawExpr =
                            fcLines.isEmpty() ? fallbackExpr : fcLines.last().trimmed();
                    if (!rawExpr.isEmpty()) {
                        QString fullFormula;
                        for (int j = 0; j < P; ++j) {
                            const int rowIdx = m * P + j;
                            const double pv = (rowIdx < sumTbl->numRows() && sumTbl->column(3))
                                    ? sumTbl->column(3)->valueAt(rowIdx)
                                    : 0.0;
                            fullFormula +=
                                    QStringLiteral("%1=%2\n").arg(pNames[j]).arg(pv, 0, 'g', 15);
                        }
                        fullFormula += QLatin1Char('\n');
                        fullFormula += rawExpr;
                        fc->setFunctionType(FunctionCurve::Normal);
                        fc->setVariable(QStringLiteral("x"));
                        fc->setFormula(fullFormula);
                    }
                }
            }
        }
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();
        // converged follows the solver state (a stall is not converged), and the notes say which
        // parameters ended on a bound, as analysis.fit_nonlinear does.
        const bool globalConverged = ok && nlf.converged();
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("converged")] = globalConverged;
        outResult[QStringLiteral("convergence_message")] = nlf.convergenceMessage();
        if (!nlf.fitNotes().isEmpty())
            outResult[QStringLiteral("notes")] = QJsonArray::fromStringList(nlf.fitNotes());
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("param_names")] = toJsonArray(pNames);
        if (sumTbl) {
            outResult[QStringLiteral("summary_table")] = sumTbl->name();
            outResult[QStringLiteral("summary")] = extractTableAsJson(sumTbl);
        }
        return true;
    }

    if (method == QStringLiteral("analysis.fit_piecewise")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        int segs = 2;
        if (params.contains(QStringLiteral("segments"))
            && !params.value(QStringLiteral("segments")).isNull()) {
            segs = parseStrictJsonInt(params.value(QStringLiteral("segments")),
                                      QStringLiteral("segments"));
            if (segs < 2 || segs > 3)
                throwInvalidParams(
                        QStringLiteral("Piecewise 'segments' (%1) must be 2 or 3.").arg(segs));
        }
        requireCurveMinPoints(g, curveTitle, (segs == 3) ? 6 : 4,
                              QStringLiteral("analysis.fit_piecewise"));
        const bool annotate = params.contains(QStringLiteral("annotate"))
                ? params.value(QStringLiteral("annotate")).toBool(true)
                : true;
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Piecewise Fit"));
        const auto pw = PiecewiseAndRobustFit::fitPiecewiseLinear(d_app, g, curveTitle, segs,
                                                                  annotate);
        if (auto *fc = dynamic_cast<FunctionCurve *>(pw.fittedCurve)) {
            if (!pw.slopes.isEmpty()) {
                QString expr = QStringLiteral("(%1)+(%2)*x")
                                       .arg(pw.intercept0, 0, 'g', 15)
                                       .arg(pw.slopes[0], 0, 'g', 15);
                for (int j = 0; j + 1 < pw.slopes.size() && j < pw.breakpointsX.size(); ++j) {
                    const double dSlope = pw.slopes[j + 1] - pw.slopes[j];
                    const double bp = pw.breakpointsX[j];
                    expr += QStringLiteral("+(%1)*0.5*((x-(%2))+abs(x-(%2)))")
                                    .arg(dSlope, 0, 'g', 15)
                                    .arg(bp, 0, 'g', 15);
                }
                fc->setFunctionType(FunctionCurve::Normal);
                fc->setVariable(QStringLiteral("x"));
                if (fc->dataSize() >= 2)
                    fc->setRange(fc->x(0), fc->x(fc->dataSize() - 1));
                fc->setFormula(expr);
            }
        }
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = pw.converged;
        outResult[QStringLiteral("converged")] = pw.converged;
        outResult[QStringLiteral("segments")] = pw.segments;
        outResult[QStringLiteral("breakpoints_x")] = toJsonArray(pw.breakpointsX);
        outResult[QStringLiteral("breakpoints_x_se")] = toJsonArray(pw.breakpointsXSE);
        outResult[QStringLiteral("breakpoints_y")] = toJsonArray(pw.breakpointsY);
        if (!pw.breakpointsX.isEmpty())
            outResult[QStringLiteral("breakpoint")] = safeJsonDouble(pw.breakpointsX.first());
        outResult[QStringLiteral("slopes")] = toJsonArray(pw.slopes);
        outResult[QStringLiteral("intercept0")] = safeJsonDouble(pw.intercept0);
        outResult[QStringLiteral("r2")] = safeJsonDouble(pw.rSquare);
        outResult[QStringLiteral("aic")] = safeJsonDouble(pw.aic);
        if (pw.reportTable)
            outResult[QStringLiteral("table")] = pw.reportTable->name();
        return true;
    }

    if (method == QStringLiteral("analysis.fit_robust")
        || method == QStringLiteral("analysis.fit_quantile")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        const bool annotate = params.contains(QStringLiteral("annotate"))
                ? params.value(QStringLiteral("annotate")).toBool(true)
                : true;
        const bool plotCurve = params.contains(QStringLiteral("plot_curve"))
                ? params.value(QStringLiteral("plot_curve")).toBool(true)
                : true;
        if (method == QStringLiteral("analysis.fit_quantile")) {
            requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("analysis.fit_quantile"));
            double tau = 0.5;
            if (params.contains(QStringLiteral("tau")) && !params.value(QStringLiteral("tau")).isNull()) {
                tau = params.value(QStringLiteral("tau")).toDouble(-1.0);
                if (!std::isfinite(tau) || tau <= 0.0 || tau >= 1.0)
                    throwInvalidParams(QStringLiteral("Quantile 'tau' must satisfy 0 < tau < 1."));
            }
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Quantile Fit"));
            const auto rq = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                    d_app, g, curveTitle, PiecewiseAndRobustFit::QuantileReg, tau, annotate);
            if (auto *fc = dynamic_cast<FunctionCurve *>(rq.fittedCurve)) {
                fc->setFunctionType(FunctionCurve::Normal);
                fc->setVariable(QStringLiteral("x"));
                if (fc->dataSize() >= 2)
                    fc->setRange(fc->x(0), fc->x(fc->dataSize() - 1));
                fc->setFormula(QStringLiteral("(%1)+(%2)*x")
                                       .arg(rq.intercept, 0, 'g', 15)
                                       .arg(rq.slope, 0, 'g', 15));
            }
            sanitizeGraphFunctionCurves(d_app, g);
            enforceArialGraphFonts(g);
            g->replot();
            undoGuard.commit();
            outResult[QStringLiteral("ok")] = rq.converged;
            outResult[QStringLiteral("converged")] = rq.converged;
            outResult[QStringLiteral("tau")] = tau;
            outResult[QStringLiteral("slope")] = safeJsonDouble(rq.slope);
            outResult[QStringLiteral("intercept")] = safeJsonDouble(rq.intercept);
            outResult[QStringLiteral("slope_se")] = safeJsonDouble(rq.slopeSE);
            outResult[QStringLiteral("intercept_se")] = safeJsonDouble(rq.interceptSE);
            if (rq.reportTable)
                outResult[QStringLiteral("table")] = rq.reportTable->name();
            return true;
        }

        const QString mStr = params.value(QStringLiteral("method")).toString().trimmed().toLower();
        if (!mStr.isEmpty() && mStr != QStringLiteral("bisquare") && mStr != QStringLiteral("tukey")
            && mStr != QStringLiteral("huber") && mStr != QStringLiteral("siegel")
            && mStr != QStringLiteral("theil_sen") && mStr != QStringLiteral("theil-sen")
            && mStr != QStringLiteral("theilsen")) {
            throwInvalidParams(
                    QStringLiteral("Unsupported robust fit method '%1' (expected 'bisquare', 'huber', 'siegel', or 'theil_sen').")
                            .arg(mStr));
        }

        auto sortedMedian = [](std::vector<double> &v) -> double {
            if (v.empty())
                return 0.0;
            std::sort(v.begin(), v.end());
            const size_t m = v.size();
            return (m % 2 == 1) ? v[m / 2] : 0.5 * (v[(m - 1) / 2] + v[m / 2]);
        };

        if (mStr == QStringLiteral("siegel") || mStr == QStringLiteral("theil_sen")
            || mStr == QStringLiteral("theil-sen") || mStr == QStringLiteral("theilsen")) {
            int distinctX = 0;
            requireCurveMinPoints(g, curveTitle, 3, QStringLiteral("analysis.fit_robust"),
                                  &distinctX);
            if (distinctX < 2)
                throwInvalidParams(
                        QStringLiteral("Robust fit requires >= 2 distinct finite X values."));

            QwtPlotCurve *c = g->curve(g->curveIndex(curveTitle));
            std::vector<double> xs, ys;
            extractValidUnmaskedCurveXY(c, xs, ys);
            if (xs.size() < 3) {
                throwInvalidParams(
                        QStringLiteral("Robust fit requires >= 3 valid unmasked points."));
            }
            double xMin = *std::min_element(xs.begin(), xs.end());
            double xMax = *std::max_element(xs.begin(), xs.end());
            const int n = static_cast<int>(xs.size());
            const bool isSiegel = (mStr == QStringLiteral("siegel"));
            double slope = 0.0;
            if (isSiegel) {
                // Exact Siegel repeated-median estimator (50% breakdown point)
                std::vector<double> medSlopes;
                medSlopes.reserve(size_t(n));
                for (int i = 0; i < n; ++i) {
                    std::vector<double> rowSlopes;
                    rowSlopes.reserve(size_t(n));
                    for (int j = 0; j < n; ++j) {
                        if (i == j)
                            continue;
                        const double dx = xs[size_t(j)] - xs[size_t(i)];
                        if (std::abs(dx) > 1e-15)
                            rowSlopes.push_back((ys[size_t(j)] - ys[size_t(i)]) / dx);
                    }
                    if (!rowSlopes.empty())
                        medSlopes.push_back(sortedMedian(rowSlopes));
                }
                slope = sortedMedian(medSlopes);
            } else {
                // Exact Theil-Sen median-of-pairwise-slopes estimator
                std::vector<double> pairSlopes;
                pairSlopes.reserve(size_t(n) * size_t(n - 1) / 2);
                for (int i = 0; i < n; ++i) {
                    for (int j = i + 1; j < n; ++j) {
                        const double dx = xs[size_t(j)] - xs[size_t(i)];
                        if (std::abs(dx) > 1e-15)
                            pairSlopes.push_back((ys[size_t(j)] - ys[size_t(i)]) / dx);
                    }
                }
                slope = sortedMedian(pairSlopes);
            }

            std::vector<double> intercepts;
            intercepts.reserve(size_t(n));
            for (int i = 0; i < n; ++i)
                intercepts.push_back(ys[size_t(i)] - slope * xs[size_t(i)]);
            const double intercept = sortedMedian(intercepts);

            const QString canonicalMethod =
                    isSiegel ? QStringLiteral("siegel") : QStringLiteral("theil_sen");
            QString fittedTitle;
            if (plotCurve && std::isfinite(xMin) && std::isfinite(xMax) && xMax > xMin) {
                const QString expr = QStringLiteral("(%1)+(%2)*x")
                                             .arg(intercept, 0, 'g', 15)
                                             .arg(slope, 0, 'g', 15);
                fittedTitle = QStringLiteral("%1_%2")
                                      .arg(curveTitle, isSiegel ? QStringLiteral("SiegelFit")
                                                                : QStringLiteral("TheilSenFit"));
                ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Robust Fit"));
                if (g->addFunctionCurve(d_app, FunctionCurve::Normal, QStringList() << expr,
                                        QStringLiteral("x"), QList<double>() << xMin << xMax, 200,
                                        fittedTitle)) {
                    if (QwtPlotCurve *fc = g->curve(g->curves() - 1)) {
                        // フィットした曲線と同じ軸に付ける（右軸の曲線が左軸の範囲を変えないように）。
                        if (QwtPlotCurve *src = g->curve(curveTitle))
                            fc->setAxis(src->xAxis(), src->yAxis());
                        QPen pen(QColor(QStringLiteral("#D55E00")));
                        pen.setWidthF(1.5);
                        fc->setPen(pen);
                        fittedTitle = fc->title().text();
                    }
                }
                sanitizeGraphFunctionCurves(d_app, g);
                enforceArialGraphFonts(g);
                g->replot();
                undoGuard.commit();
            } else {
                sanitizeGraphFunctionCurves(d_app, g);
                enforceArialGraphFonts(g);
                g->replot();
            }

            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("converged")] = true;
            outResult[QStringLiteral("graph")] = ml->name();
            outResult[QStringLiteral("curve")] = curveTitle;
            if (!fittedTitle.isEmpty())
                outResult[QStringLiteral("fitted_curve")] = fittedTitle;
            outResult[QStringLiteral("method")] = canonicalMethod;
            outResult[QStringLiteral("slope")] = safeJsonDouble(slope);
            outResult[QStringLiteral("intercept")] = safeJsonDouble(intercept);
            outResult[QStringLiteral("n")] = n;
            return true;
        }

        requireCurveMinPoints(g, curveTitle, 3, QStringLiteral("analysis.fit_robust"));
        const auto rm = (mStr == QStringLiteral("huber")) ? PiecewiseAndRobustFit::HuberM
                                                          : PiecewiseAndRobustFit::TukeyBisquare;
        ScopedGraphUndoGuard undoGuard(plotCurve ? ml : nullptr, QObject::tr("Robust Fit"));
        const auto rf = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(d_app, g, curveTitle, rm,
                                                                         0.5, annotate);
        if (!plotCurve && rf.fittedCurve) {
            const int ci = g->curveIndex(rf.fittedCurve);
            if (ci >= 0)
                g->removeCurve(ci);
        } else if (auto *fc = dynamic_cast<FunctionCurve *>(rf.fittedCurve)) {
            fc->setFunctionType(FunctionCurve::Normal);
            fc->setVariable(QStringLiteral("x"));
            if (fc->dataSize() >= 2)
                fc->setRange(fc->x(0), fc->x(fc->dataSize() - 1));
            fc->setFormula(QStringLiteral("(%1)+(%2)*x")
                                   .arg(rf.intercept, 0, 'g', 15)
                                   .arg(rf.slope, 0, 'g', 15));
        }
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();
        outResult[QStringLiteral("ok")] = rf.converged;
        outResult[QStringLiteral("converged")] = rf.converged;
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("curve")] = curveTitle;
        outResult[QStringLiteral("method")] =
                (rm == PiecewiseAndRobustFit::HuberM) ? QStringLiteral("huber")
                                                      : QStringLiteral("bisquare");
        outResult[QStringLiteral("slope")] = safeJsonDouble(rf.slope);
        outResult[QStringLiteral("intercept")] = safeJsonDouble(rf.intercept);
        outResult[QStringLiteral("slope_se")] = safeJsonDouble(rf.slopeSE);
        outResult[QStringLiteral("intercept_se")] = safeJsonDouble(rf.interceptSE);
        outResult[QStringLiteral("scale_mad")] = safeJsonDouble(rf.scaleMAD);
        if (rf.reportTable)
            outResult[QStringLiteral("table")] = rf.reportTable->name();
        return true;
    }

    if (method == QStringLiteral("analysis.fit_logistic")) {
        Table *t = resolveTable(d_app, params);
        const int yIdx = resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true);
        QStringList xColNames;
        QVector<int> xIdxs;
        if (params.value(QStringLiteral("x_cols")).isArray()) {
            for (const QJsonValue &xv : params.value(QStringLiteral("x_cols")).toArray()) {
                const int xi = resolveColumnIndex(t, xv, true);
                xIdxs.append(xi);
                xColNames << t->colLabel(xi);
            }
        } else if (params.contains(QStringLiteral("x_col"))) {
            const int xi = resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true);
            xIdxs.append(xi);
            xColNames << t->colLabel(xi);
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != yIdx) {
                    xIdxs.append(c);
                    xColNames << t->colLabel(c);
                }
            }
        }
        const bool plotProb = params.contains(QStringLiteral("plot"))
                ? params.value(QStringLiteral("plot")).toBool(true)
                : true;
        const auto lr = PiecewiseAndRobustFit::runLogisticRegression(d_app, t, t->colLabel(yIdx),
                                                                     xColNames, plotProb);
        if (lr.probGraph) {
            for (Graph *lg : lr.probGraph->layersList()) {
                if (!lg)
                    continue;
                if (lr.betas.size() == 2) {
                    for (int ci = 0; ci < lg->curves(); ++ci) {
                        if (auto *fc = dynamic_cast<FunctionCurve *>(lg->plotItem(ci))) {
                            if (fc->formulas().isEmpty() || fc->formulas().first().trimmed().isEmpty()) {
                                fc->setFunctionType(FunctionCurve::Normal);
                                fc->setVariable(QStringLiteral("x"));
                                if (fc->dataSize() >= 2)
                                    fc->setRange(fc->x(0), fc->x(fc->dataSize() - 1));
                                fc->setFormula(QStringLiteral("1/(1+exp(-((%1)+(%2)*x)))")
                                                       .arg(lr.betas[0], 0, 'g', 15)
                                                       .arg(lr.betas[1], 0, 'g', 15));
                            }
                        }
                    }
                }
                sanitizeGraphFunctionCurves(d_app, lg);
                enforceArialGraphFonts(lg);
                lg->replot();
            }
        }

        // Compute predicted probabilities -> ROC AUC across valid unmasked rows
        std::vector<int> yBin;
        std::vector<int> validRows;
        std::vector<double> probs;
        if (lr.betas.size() == xIdxs.size() + 1) {
            extractBinaryStateRows(t, yIdx, xIdxs, yBin, validRows);
            probs.reserve(validRows.size());
            for (int r : validRows) {
                double eta = lr.betas[0];
                for (int k = 0; k < xIdxs.size(); ++k)
                    eta += lr.betas[k + 1] * t->cell(r, xIdxs[k]);
                probs.push_back(1.0 / (1.0 + std::exp(-std::clamp(eta, -50.0, 50.0))));
            }
        }
        const auto roc = computeRocMetricsDirect(yBin, probs);

        outResult[QStringLiteral("ok")] = lr.converged;
        outResult[QStringLiteral("converged")] = lr.converged;
        outResult[QStringLiteral("betas")] = toJsonArray(lr.betas);
        outResult[QStringLiteral("std_errors")] = toJsonArray(lr.stdErrors);
        outResult[QStringLiteral("wald_z")] = toJsonArray(lr.waldZ);
        outResult[QStringLiteral("p_values")] = toJsonArray(lr.pValues);
        outResult[QStringLiteral("odds_ratios")] = toJsonArray(lr.oddsRatios);
        outResult[QStringLiteral("auc")] = safeJsonDouble(roc.auc);
        outResult[QStringLiteral("log_likelihood")] = safeJsonDouble(lr.logLikelihood);
        outResult[QStringLiteral("lr_chi2")] = safeJsonDouble(lr.lrChi2);
        outResult[QStringLiteral("lr_p_value")] = safeJsonDouble(lr.lrPValue);
        outResult[QStringLiteral("mcfadden_r2")] = safeJsonDouble(lr.mcFaddenR2);
        outResult[QStringLiteral("aic")] = safeJsonDouble(lr.aic);
        if (lr.reportTable)
            outResult[QStringLiteral("table")] = lr.reportTable->name();
        if (lr.probGraph)
            outResult[QStringLiteral("graph")] = lr.probGraph->name();
        return true;
    }

    if (method == QStringLiteral("analysis.fit_mlr")) {
        Table *t = resolveTable(d_app, params);
        const int yIdx = resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true);
        QStringList xColNames;
        if (params.value(QStringLiteral("x_cols")).isArray()) {
            for (const QJsonValue &xv : params.value(QStringLiteral("x_cols")).toArray())
                xColNames << t->colLabel(resolveColumnIndex(t, xv, true));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != yIdx)
                    xColNames << t->colLabel(c);
            }
        }
        const bool plotDiag = params.value(QStringLiteral("plot_diagnostics")).toBool(false);
        const auto mlr = PiecewiseAndRobustFit::fitMultipleLinearRegression(
                d_app, t, t->colLabel(yIdx), xColNames, plotDiag);
        if (mlr.diagnosticGraph) {
            for (Graph *lg : mlr.diagnosticGraph->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = mlr.valid;
        outResult[QStringLiteral("betas")] = toJsonArray(mlr.betas);
        outResult[QStringLiteral("std_errors")] = toJsonArray(mlr.stdErrors);
        outResult[QStringLiteral("t_values")] = toJsonArray(mlr.tValues);
        outResult[QStringLiteral("p_values")] = toJsonArray(mlr.pValues);
        outResult[QStringLiteral("vif")] = toJsonArray(mlr.vif);
        outResult[QStringLiteral("r2")] = safeJsonDouble(mlr.rSquare);
        outResult[QStringLiteral("adj_r2")] = safeJsonDouble(mlr.adjRSquare);
        outResult[QStringLiteral("f_statistic")] = safeJsonDouble(mlr.fStatistic);
        outResult[QStringLiteral("f_p_value")] = safeJsonDouble(mlr.fPValue);
        if (mlr.reportTable)
            outResult[QStringLiteral("table")] = mlr.reportTable->name();
        if (mlr.diagnosticGraph)
            outResult[QStringLiteral("graph")] = mlr.diagnosticGraph->name();
        return true;
    }

    if (method == QStringLiteral("analysis.fit_implicit_odr")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        RpcNonLinearFit nlf(d_app, g);
        nlf.setMaximumIterations(
                parseBoundedInt(params, QStringLiteral("max_iter"), 150, 1, 10000));
        if (params.contains(QStringLiteral("tolerance"))
            && !params.value(QStringLiteral("tolerance")).isNull()) {
            nlf.setTolerance(std::clamp(
                    optionalJsonFiniteDouble(params, QStringLiteral("tolerance"), 1e-6, 1.0),
                    1e-14, 1.0));
        }
        if (!nlf.setDataFromCurve(curveTitle, g) || nlf.error())
            throwRpcRuntime(QStringLiteral("Failed to load curve '%1' for implicit ODR fit.")
                                    .arg(curveTitle));

        const auto &xv = nlf.x();
        const auto &yv = nlf.y();
        if (xv.size() < 4)
            throwInvalidParams(QStringLiteral("Implicit ODR fit requires >= 4 points."));

        double xMean = std::accumulate(xv.begin(), xv.end(), 0.0) / double(xv.size());
        double yMean = std::accumulate(yv.begin(), yv.end(), 0.0) / double(yv.size());
        auto [xMinIt, xMaxIt] = std::minmax_element(xv.begin(), xv.end());
        auto [yMinIt, yMaxIt] = std::minmax_element(yv.begin(), yv.end());
        double rxGuess = std::max(1e-6, 0.5 * (*xMaxIt - *xMinIt));
        double ryGuess = std::max(1e-6, 0.5 * (*yMaxIt - *yMinIt));
        double rGuess = 0.5 * (rxGuess + ryGuess);

        const QString rawModelStr = params.contains(QStringLiteral("model"))
                ? optionalJsonString(params, QStringLiteral("model"))
                : optionalJsonString(params, QStringLiteral("shape"));
        const QString model = rawModelStr.trimmed().toLower();
        const QString customFormula =
                optionalJsonString(params, QStringLiteral("formula")).trimmed();
        if (!model.isEmpty() && model != QStringLiteral("custom") && !customFormula.isEmpty()) {
            throwInvalidParams(QStringLiteral(
                    "Provide either built-in 'model' or custom 'formula' in analysis.fit_implicit_odr, not both."));
        }
        QString resolvedModel = model;
        QString expr;
        QStringList pNames;
        QVector<double> initVals;

        if (!customFormula.isEmpty() || model == QStringLiteral("custom")) {
            resolvedModel = QStringLiteral("custom");
            expr = customFormula;
            for (const QJsonValue &pv : params.value(QStringLiteral("param_names")).toArray()) {
                const QString pn = pv.toString().trimmed();
                if (!pn.isEmpty())
                    pNames << pn;
            }
            if (expr.isEmpty() || pNames.isEmpty()) {
                throwInvalidParams(QStringLiteral(
                        "Custom implicit ODR fit requires non-empty 'formula' and 'param_names'."));
            }
            QStringList checkVars = pNames;
            checkVars << QStringLiteral("x") << QStringLiteral("y");
            validateMuParserExprOrThrow(normalizeImplicitExprForValidation(expr), checkVars,
                                        QStringLiteral("analysis.fit_implicit_odr"));
            initVals.fill(1.0, pNames.size());
        } else if (model == QStringLiteral("circle") || model.isEmpty()) {
            // Algebraic Kasa linear pre-seed for exact circle starting point
            resolvedModel = QStringLiteral("circle");
            expr = QStringLiteral("(x - xc)^2 + (y - yc)^2 - R^2");
            pNames << QStringLiteral("xc") << QStringLiteral("yc") << QStringLiteral("R");
            initVals << (0.5 * (*xMinIt + *xMaxIt)) << (0.5 * (*yMinIt + *yMaxIt)) << rGuess;
        } else if (model == QStringLiteral("ellipse")) {
            resolvedModel = QStringLiteral("ellipse");
            expr = QStringLiteral(
                    "((x - xc)*cos(theta) + (y - yc)*sin(theta))^2 / a^2 + "
                    "(-(x - xc)*sin(theta) + (y - yc)*cos(theta))^2 / b^2 - 1");
            pNames << QStringLiteral("xc") << QStringLiteral("yc") << QStringLiteral("a")
                   << QStringLiteral("b") << QStringLiteral("theta");
            initVals << xMean << yMean << rxGuess << ryGuess << 0.1;
        } else if (model == QStringLiteral("superellipse")) {
            resolvedModel = QStringLiteral("superellipse");
            expr = QStringLiteral("abs((x - xc)/a)^n + abs((y - yc)/b)^n - 1");
            pNames << QStringLiteral("xc") << QStringLiteral("yc") << QStringLiteral("a")
                   << QStringLiteral("b") << QStringLiteral("n");
            initVals << xMean << yMean << rxGuess << ryGuess << 2.0;
        } else {
            throwInvalidParams(
                    QStringLiteral("Unknown implicit ODR model '%1' (expected 'circle'|'ellipse'|'superellipse'|'custom').")
                            .arg(rawModelStr));
        }

        if (params.contains(QStringLiteral("initial_values"))) {
            const QJsonValue iv = params.value(QStringLiteral("initial_values"));
            if (iv.isArray()) {
                const QJsonArray ia = iv.toArray();
                for (int i = 0; i < std::min(initVals.size(), ia.size()); ++i)
                    initVals[i] = ia[i].toDouble();
            } else if (iv.isObject()) {
                const QJsonObject io = iv.toObject();
                for (int i = 0; i < pNames.size(); ++i) {
                    if (io.contains(pNames[i]))
                        initVals[i] = io.value(pNames[i]).toDouble();
                }
            }
        }

        const bool plotCurve = optionalJsonBool(params, QStringLiteral("plot_curve"), true);
        const bool isAnalyticalConic = (resolvedModel == QStringLiteral("circle")
                                        || resolvedModel == QStringLiteral("ellipse"));
        if (!plotCurve || isAnalyticalConic)
            nlf.setSuppressCurveOutput(true);

        ScopedGraphUndoGuard undoGuard(plotCurve ? ml : nullptr, QObject::tr("Implicit ODR Fit"));
        const bool ok = nlf.fitImplicitODR(expr, pNames, initVals);
        const auto &resVec = nlf.results();
        const auto &errVec = nlf.errors();
        if (ok && plotCurve && isAnalyticalConic) {
            constexpr double kTwoPi = 6.283185307179586476925286766559;
            QStringList paramFormulas;
            QString fitTitle;
            if (resolvedModel == QStringLiteral("circle") && resVec.size() >= 3) {
                const double xc = resVec[0];
                const double yc = resVec[1];
                const double rAbs = std::max(1e-12, std::abs(resVec[2]));
                paramFormulas << QStringLiteral("(%1)+(%2)*cos(t)")
                                         .arg(xc, 0, 'g', 15)
                                         .arg(rAbs, 0, 'g', 15)
                              << QStringLiteral("(%1)+(%2)*sin(t)")
                                         .arg(yc, 0, 'g', 15)
                                         .arg(rAbs, 0, 'g', 15);
                fitTitle = QStringLiteral("ODR Circle Fit");
            } else if (resolvedModel == QStringLiteral("ellipse") && resVec.size() >= 5) {
                const double xc = resVec[0];
                const double yc = resVec[1];
                const double aAbs = std::max(1e-12, std::abs(resVec[2]));
                const double bAbs = std::max(1e-12, std::abs(resVec[3]));
                const double th = resVec[4];
                const double ct = std::cos(th);
                const double st = std::sin(th);
                paramFormulas << QStringLiteral("(%1)+(%2)*cos(t)-(%3)*sin(t)")
                                         .arg(xc, 0, 'g', 15)
                                         .arg(aAbs * ct, 0, 'g', 15)
                                         .arg(bAbs * st, 0, 'g', 15)
                              << QStringLiteral("(%1)+(%2)*cos(t)+(%3)*sin(t)")
                                         .arg(yc, 0, 'g', 15)
                                         .arg(aAbs * st, 0, 'g', 15)
                                         .arg(bAbs * ct, 0, 'g', 15);
                fitTitle = QStringLiteral("ODR Ellipse Fit");
            }
            if (paramFormulas.size() == 2) {
                QList<double> tRange;
                tRange << 0.0 << kTwoPi;
                if (g->addFunctionCurve(d_app, FunctionCurve::Parametric, paramFormulas,
                                        QStringLiteral("t"), tRange, 256, fitTitle)) {
                    const int fci = g->curves() - 1;
                    if (fci >= 0)
                        g->setCurvePen(fci, QPen(QColor(213, 94, 0), 2.0, Qt::SolidLine));
                    // 当てはめた曲線と同じ軸に付ける。
                    QwtPlotCurve *src = g->curve(curveTitle);
                    QwtPlotCurve *fcc = (fci >= 0) ? g->curve(fci) : nullptr;
                    if (src && fcc && fcc != src)
                        fcc->setAxis(src->xAxis(), src->yAxis());
                }
            }
        }
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();
        QJsonObject paramDict, errDict;
        for (int i = 0; i < pNames.size() && size_t(i) < resVec.size(); ++i) {
            double pv = resVec[size_t(i)];
            if (pNames[i] == QStringLiteral("R") || pNames[i] == QStringLiteral("a")
                || pNames[i] == QStringLiteral("b"))
                pv = std::abs(pv);
            paramDict[pNames[i]] = safeJsonDouble(pv);
            errDict[pNames[i]] =
                    safeJsonDouble((size_t(i) < errVec.size()) ? errVec[size_t(i)] : 0.0);
        }
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("converged")] = ok;
        outResult[QStringLiteral("model")] = resolvedModel;
        outResult[QStringLiteral("shape")] = resolvedModel;
        outResult[QStringLiteral("param_names")] = toJsonArray(pNames);
        outResult[QStringLiteral("params")] = paramDict;
        outResult[QStringLiteral("errors")] = errDict;
        outResult[QStringLiteral("r2")] = safeJsonDouble(nlf.rSquare());
        return true;
    }

    if (method == QStringLiteral("analysis.fit_surface_3d")) {
        const QString rawModel = params.value(QStringLiteral("model")).toString().trimmed();
        const QString mStr = rawModel.toLower();
        const QString customExpr = params.value(QStringLiteral("formula")).toString().trimmed();
        GraphStatsAndSurfaceFit::SurfaceModel sm = GraphStatsAndSurfaceFit::Gauss2D;
        if (!customExpr.isEmpty() || mStr == QStringLiteral("custom")) {
            if (customExpr.isEmpty()) {
                throwInvalidParams(QStringLiteral(
                        "analysis.fit_surface_3d model='custom' requires non-empty 'formula'."));
            }
            sm = GraphStatsAndSurfaceFit::Custom2D;
        } else if (mStr.isEmpty() || mStr == QStringLiteral("gauss2d")
                   || mStr == QStringLiteral("gaussian2d") || mStr == QStringLiteral("gauss")) {
            sm = GraphStatsAndSurfaceFit::Gauss2D;
        } else if (mStr == QStringLiteral("plane") || mStr == QStringLiteral("plane2d")
                   || mStr == QStringLiteral("linear")) {
            sm = GraphStatsAndSurfaceFit::Plane2D;
        } else if (mStr == QStringLiteral("poly2d") || mStr == QStringLiteral("quadratic")
                   || mStr == QStringLiteral("polynomial")
                   || mStr == QStringLiteral("paraboloid")) {
            sm = GraphStatsAndSurfaceFit::Poly2DQuadratic;
        } else if (mStr == QStringLiteral("lorentz2d") || mStr == QStringLiteral("lorentz")
                   || mStr == QStringLiteral("lorentzian2d")
                   || mStr == QStringLiteral("lorentzian")) {
            sm = GraphStatsAndSurfaceFit::Lorentz2D;
        } else if (mStr == QStringLiteral("pseudovoigt2d") || mStr == QStringLiteral("pseudovoigt")
                   || mStr == QStringLiteral("pseudo_voigt2d")) {
            sm = GraphStatsAndSurfaceFit::PseudoVoigt2D;
        } else if (mStr == QStringLiteral("exp2d") || mStr == QStringLiteral("exponential2d")
                   || mStr == QStringLiteral("exponential")) {
            sm = GraphStatsAndSurfaceFit::Exponential2D;
        } else if (mStr == QStringLiteral("chebyshev2d") || mStr == QStringLiteral("chebyshev")) {
            sm = GraphStatsAndSurfaceFit::Chebyshev2D;
        } else {
            throwInvalidParams(
                    QStringLiteral("Unknown 3D surface fit model '%1' (expected plane|paraboloid|poly2d|gaussian2d|gauss2d|lorentzian2d|lorentz2d|pseudovoigt2d|exp2d|chebyshev2d|custom).")
                            .arg(rawModel));
        }

        GraphStatsAndSurfaceFit::SurfaceFitResult sr;
        if (params.contains(QStringLiteral("matrix"))
            && !params.value(QStringLiteral("matrix")).isNull()
            && !optionalJsonString(params, QStringLiteral("matrix")).trimmed().isEmpty()) {
            Matrix *m = resolveMatrix(d_app, params, QStringLiteral("matrix"), true);
            sr = GraphStatsAndSurfaceFit::fitSurfaceMatrix(d_app, m, sm, customExpr);
        } else if (!params.contains(QStringLiteral("table"))
                   && resolveMatrix(d_app, params, QStringLiteral("matrix"), false) != nullptr) {
            Matrix *m = resolveMatrix(d_app, params, QStringLiteral("matrix"), true);
            sr = GraphStatsAndSurfaceFit::fitSurfaceMatrix(d_app, m, sm, customExpr);
        } else {
            Table *t = resolveTable(d_app, params, QStringLiteral("table"), true);
            const QString xCol = t->colLabel(
                    resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true));
            const QString yCol = t->colLabel(
                    resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true));
            const QString zCol = t->colLabel(
                    resolveColumnIndex(t, params.value(QStringLiteral("z_col")), true));
            const int gridSide =
                    parseBoundedInt(params, QStringLiteral("grid_side"), 40, 4, 512);
            sr = GraphStatsAndSurfaceFit::fitSurfaceXYZ(d_app, t, xCol, yCol, zCol, sm, customExpr,
                                                        gridSide);
        }

        QJsonObject pDict, eDict;
        for (int i = 0; i < sr.paramNames.size() && i < sr.params.size(); ++i) {
            pDict[sr.paramNames[i]] = safeJsonDouble(sr.params[i]);
            eDict[sr.paramNames[i]] =
                    safeJsonDouble((i < sr.errors.size()) ? sr.errors[i] : 0.0);
        }
        outResult[QStringLiteral("ok")] = sr.converged;
        outResult[QStringLiteral("converged")] = sr.converged;
        outResult[QStringLiteral("convergence_message")] = sr.convergenceNote;
        outResult[QStringLiteral("r2")] = safeJsonDouble(sr.rSquare);
        outResult[QStringLiteral("adj_r2")] = safeJsonDouble(sr.adjRSquare);
        outResult[QStringLiteral("chi2_dof")] = safeJsonDouble(sr.reducedChi2);
        outResult[QStringLiteral("param_names")] = toJsonArray(sr.paramNames);
        outResult[QStringLiteral("params")] = pDict;
        outResult[QStringLiteral("errors")] = eDict;
        if (sr.fittedMatrix)
            outResult[QStringLiteral("fitted_matrix")] = sr.fittedMatrix->name();
        if (sr.paramTable)
            outResult[QStringLiteral("param_table")] = sr.paramTable->name();
        return true;
    }

    if (method == QStringLiteral("analysis.compare_models")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        QStringList models;
        for (const QJsonValue &mv : params.value(QStringLiteral("models")).toArray())
            models << mv.toString().trimmed();
        if (models.size() < 2)
            models = QStringList{ QStringLiteral("ExpDec1"), QStringLiteral("ExpDec2") };

        Table *cmpTbl = Fit::compareModels(d_app, g, curveTitle, models);
        if (!cmpTbl)
            throwRpcRuntime(QStringLiteral("Model comparison failed."));

        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = cmpTbl->name();
        outResult[QStringLiteral("comparison")] = extractTableAsJson(cmpTbl);
        if (cmpTbl->numRows() > 0)
            outResult[QStringLiteral("best_model")] = cmpTbl->text(0, 0);
        return true;
    }

    if (method == QStringLiteral("analysis.compare_datasets")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        QStringList curveTitles;
        if (params.value(QStringLiteral("curves")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("curves")).toArray()) {
                QJsonObject tmp;
                tmp[QStringLiteral("curve")] = cv;
                curveTitles << resolveCurveTitle(g, tmp, QStringLiteral("curve"), true);
            }
        } else {
            curveTitles = g->analysableCurvesList();
        }
        const QString modelName = params.contains(QStringLiteral("model"))
                ? params.value(QStringLiteral("model")).toString()
                : QStringLiteral("ExpDec1");
        const auto dc = PiecewiseAndRobustFit::compareDatasets(d_app, g, curveTitles, modelName);
        outResult[QStringLiteral("ok")] = dc.valid;
        outResult[QStringLiteral("rss_shared")] = safeJsonDouble(dc.rssShared);
        outResult[QStringLiteral("df_shared")] = dc.dfShared;
        outResult[QStringLiteral("rss_separate")] = safeJsonDouble(dc.rssSeparate);
        outResult[QStringLiteral("df_separate")] = dc.dfSeparate;
        outResult[QStringLiteral("f_statistic")] = safeJsonDouble(dc.fStatistic);
        outResult[QStringLiteral("p_value")] = safeJsonDouble(dc.pValue);
        outResult[QStringLiteral("delta_aicc")] = safeJsonDouble(dc.deltaAICc);
        outResult[QStringLiteral("prefer_separate")] = dc.preferSeparate;
        if (dc.reportTable)
            outResult[QStringLiteral("table")] = dc.reportTable->name();
        return true;
    }

    if (method == QStringLiteral("analysis.analyze_peaks")) {
        // Validate peak-count driving inputs before anything is plotted or allocated. Doublet
        // indices feed MultiPeakFit::setNumPeaks(max(secondary, primary) + 1) and 'centers' feeds
        // setNumPeaks(centers.size()); both share the 500-peak cap of num_peaks / max_peaks so that
        // 3 * peaks + 1 cannot overflow and the GSL vectors stay allocatable.
        constexpr int kMaxAnalyzePeaks = 500;
        struct ParsedDoublet
        {
            int secondary;
            int primary;
            double areaRatio;
            double centerOffset;
            bool shareWidth;
        };
        QVector<ParsedDoublet> parsedDoublets;
        if (params.value(QStringLiteral("doublets")).isArray()) {
            for (const QJsonValue &dv : params.value(QStringLiteral("doublets")).toArray()) {
                if (!dv.isObject())
                    continue;
                const QJsonObject dobj = dv.toObject();
                ParsedDoublet pd;
                pd.secondary = parseBoundedInt(dobj, QStringLiteral("secondary"), 1, 0,
                                               kMaxAnalyzePeaks - 1);
                pd.primary = parseBoundedInt(dobj, QStringLiteral("primary"), 0, 0,
                                             kMaxAnalyzePeaks - 1);
                pd.areaRatio = dobj.value(QStringLiteral("area_ratio")).toDouble(0.5);
                pd.centerOffset = dobj.value(QStringLiteral("center_offset")).toDouble(0.0);
                pd.shareWidth = dobj.value(QStringLiteral("share_width")).toBool(true);
                parsedDoublets.append(pd);
            }
        }
        const QJsonArray centersArr = params.value(QStringLiteral("centers")).toArray();
        if (centersArr.size() > kMaxAnalyzePeaks) {
            throwInvalidParams(
                    QStringLiteral("Parameter 'centers' must contain at most %1 entries, got %2.")
                            .arg(kMaxAnalyzePeaks)
                            .arg(centersArr.size()));
        }

        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        const auto profile = parsePeakProfile(params.value(QStringLiteral("profile")).toString());
        const QString baseStr = params.contains(QStringLiteral("baseline_mode"))
                ? params.value(QStringLiteral("baseline_mode")).toString()
                : params.value(QStringLiteral("baseline")).toString();
        const auto baseMode = parseBaselineMode(baseStr);
        const int reqPeaks = params.contains(QStringLiteral("num_peaks"))
                ? parseBoundedInt(params, QStringLiteral("num_peaks"), 0, 0, 500)
                : (params.contains(QStringLiteral("peak_count"))
                           ? parseBoundedInt(params, QStringLiteral("peak_count"), 0, 0, 500)
                           : parseBoundedInt(params, QStringLiteral("peaks"), 0, 0, 500));
        const double prominence =
                std::clamp(params.value(QStringLiteral("prominence")).toDouble(0.08), 0.005, 0.95);
        const bool plotPeaks = params.contains(QStringLiteral("plot_peaks"))
                ? params.value(QStringLiteral("plot_peaks")).toBool(true)
                : (params.contains(QStringLiteral("plot_components"))
                           ? params.value(QStringLiteral("plot_components")).toBool(true)
                           : true);
        const bool plotFit = params.contains(QStringLiteral("plot_curve"))
                ? params.value(QStringLiteral("plot_curve")).toBool(true)
                : true;
        const bool plotBase = params.value(QStringLiteral("plot_baseline")).toBool(false);
        const bool doIntegrate = params.value(QStringLiteral("integrate")).toBool(false);
        const bool createSummary = params.value(QStringLiteral("create_summary")).toBool(false);
        const bool createResiduals = params.value(QStringLiteral("create_residuals")).toBool(false);

        RpcMultiPeakFit mpf(d_app, g, profile, std::max(1, reqPeaks));
        mpf.setSuppressCurveOutput(!plotFit);
        mpf.enablePeakCurves(plotPeaks && plotFit);
        if (params.contains(QStringLiteral("voigt_mu"))
            && !params.value(QStringLiteral("voigt_mu")).isNull()) {
            mpf.setLorentzFraction(params.value(QStringLiteral("voigt_mu")).toDouble(0.5));
        }
        if (params.contains(QStringLiteral("pearson_m"))
            && !params.value(QStringLiteral("pearson_m")).isNull()) {
            mpf.setPearsonShape(params.value(QStringLiteral("pearson_m")).toDouble(2.0));
        }
        if (params.contains(QStringLiteral("asymmetry"))
            && !params.value(QStringLiteral("asymmetry")).isNull()) {
            mpf.setAsymmetryFactor(params.value(QStringLiteral("asymmetry")).toDouble(1.5));
        }
        if (params.contains(QStringLiteral("tail_tau"))
            && !params.value(QStringLiteral("tail_tau")).isNull()) {
            mpf.setTailFactor(params.value(QStringLiteral("tail_tau")).toDouble(1.2));
        }
        mpf.setNegativePeaksAllowed(
                params.value(QStringLiteral("allow_negative")).toBool(false));
        mpf.generateFunction(
                true, parseBoundedInt(params, QStringLiteral("points"), 300, 10, 1000000));

        if (!mpf.setDataFromCurve(curveTitle, g) || mpf.error())
            throwRpcRuntime(
                    QStringLiteral("Failed to load curve '%1' for peak analysis.").arg(curveTitle));

        mpf.setBaselineMode(baseMode);
        if (params.value(QStringLiteral("baseline_anchors")).isArray()) {
            QVector<QPointF> anchors;
            for (const QJsonValue &av : params.value(QStringLiteral("baseline_anchors")).toArray()) {
                if (av.isArray() && av.toArray().size() >= 2)
                    anchors.append(QPointF(av.toArray()[0].toDouble(), av.toArray()[1].toDouble()));
                else if (av.isObject())
                    anchors.append(QPointF(av.toObject().value(QStringLiteral("x")).toDouble(),
                                           av.toObject().value(QStringLiteral("y")).toDouble()));
            }
            if (!anchors.isEmpty())
                mpf.setBaselineAnchors(
                        anchors, params.value(QStringLiteral("anchor_spline")).toBool(true));
        }

        const bool defaultSubtract = (baseMode == MultiPeakFit::BaselineLinear
                                      || baseMode == MultiPeakFit::BaselineALS
                                      || baseMode == MultiPeakFit::BaselineAnchors);
        const bool subtractBaseline = params.contains(QStringLiteral("subtract_baseline"))
                ? params.value(QStringLiteral("subtract_baseline")).toBool(defaultSubtract)
                : defaultSubtract;
        if (subtractBaseline && baseMode != MultiPeakFit::BaselineZero)
            mpf.subtractBaselineFromData();

        if (params.value(QStringLiteral("centers")).isArray() && !centersArr.isEmpty()) {
            std::vector<double> pickedCenters;
            for (const QJsonValue &cv : centersArr)
                pickedCenters.push_back(cv.toDouble());
            mpf.setNumPeaks(static_cast<int>(pickedCenters.size()));
            mpf.seedPeaksFromCenters(pickedCenters, prominence);
        } else if (params.value(QStringLiteral("use_second_derivative")).toBool(false)) {
            const int sgWin = std::max(5, params.value(QStringLiteral("sg_window")).toInt(9));
            const QVector<double> d2Centers = mpf.detectPeaksSecondDerivative(sgWin, prominence);
            if (!d2Centers.isEmpty()) {
                std::vector<double> cStd(d2Centers.begin(), d2Centers.end());
                if (reqPeaks > 0 && int(cStd.size()) > reqPeaks) {
                    // Keep the reqPeaks tallest detections (height of the data above its median
                    // at the nearest sample), not the first ones in X order, which are often
                    // noise at the low-X end. Two detections closer than half the narrower one's
                    // full width at half height (or one smoothing window) are the same peak seen
                    // through a noisy second derivative.
                    const std::vector<double> &xs = mpf.x();
                    const std::vector<double> &ys = mpf.y();
                    std::vector<double> sortedY = ys;
                    double med = 0.0;
                    if (!sortedY.empty()) {
                        std::nth_element(sortedY.begin(),
                                         sortedY.begin() + long(sortedY.size() / 2),
                                         sortedY.end());
                        med = sortedY[sortedY.size() / 2];
                    }
                    const bool allowNeg = mpf.negativePeaksAllowed();
                    auto nearestIdx = [&](double xc) -> long {
                        size_t best = 0;
                        for (size_t i = 1; i < xs.size(); ++i) {
                            if (std::abs(xs[i] - xc) < std::abs(xs[best] - xc))
                                best = i;
                        }
                        return long(best);
                    };
                    // Full width at half height, in samples, of the data around sample i.
                    auto widthAt = [&](long i) -> long {
                        const long n = long(ys.size());
                        const double sgn = (allowNeg && ys[size_t(i)] < med) ? -1.0 : 1.0;
                        const double half = 0.5 * sgn * (ys[size_t(i)] - med);
                        long l = i, r = i;
                        while (l > 0 && sgn * (ys[size_t(l - 1)] - med) > half)
                            --l;
                        while (r + 1 < n && sgn * (ys[size_t(r + 1)] - med) > half)
                            ++r;
                        return r - l + 1;
                    };
                    struct D2Pick
                    {
                        double xc;
                        long idx;
                        double height;
                        long width;
                    };
                    std::vector<D2Pick> picks;
                    if (!xs.empty() && ys.size() == xs.size()) {
                        for (double xc : cStd) {
                            const long i = nearestIdx(xc);
                            const double h = ys[size_t(i)] - med;
                            picks.push_back({ xc, i, allowNeg ? std::abs(h) : h, widthAt(i) });
                        }
                    }
                    std::stable_sort(picks.begin(), picks.end(),
                                     [](const D2Pick &a, const D2Pick &b) {
                                         return a.height > b.height;
                                     });
                    std::vector<D2Pick> kept;
                    for (int pass = 0; pass < 2 && int(kept.size()) < reqPeaks; ++pass) {
                        for (const D2Pick &pk : picks) {
                            if (int(kept.size()) >= reqPeaks)
                                break;
                            bool take = true;
                            for (const D2Pick &k : kept) {
                                // pass 0: resolved detections only; pass 1 fills up the rest
                                const long sep =
                                        std::max(long(sgWin), std::min(k.width, pk.width) / 2);
                                if ((pass == 0 && std::labs(k.idx - pk.idx) < sep)
                                    || k.xc == pk.xc) {
                                    take = false;
                                    break;
                                }
                            }
                            if (take)
                                kept.push_back(pk);
                        }
                    }
                    if (int(kept.size()) == reqPeaks) {
                        cStd.clear();
                        for (const D2Pick &k : kept)
                            cStd.push_back(k.xc);
                    } else {
                        cStd.resize(size_t(reqPeaks));
                    }
                    std::sort(cStd.begin(), cStd.end());
                }
                mpf.setNumPeaks(static_cast<int>(cStd.size()));
                mpf.seedPeaksFromCenters(cStd, prominence);
            } else {
                mpf.autoDetectAndUpdateNumPeaks(reqPeaks > 0 ? reqPeaks : 10, prominence);
            }
        } else if (reqPeaks > 0) {
            mpf.setNumPeaks(reqPeaks);
            mpf.autoDetectPeaks(reqPeaks, prominence);
        } else {
            const int maxPeaks =
                    parseBoundedInt(params, QStringLiteral("max_peaks"), 10, 1, 500);
            mpf.autoDetectAndUpdateNumPeaks(maxPeaks, prominence);
        }

        for (const ParsedDoublet &pd : parsedDoublets) {
            mpf.addPeakDoubletConstraint(pd.secondary, pd.primary, pd.areaRatio, pd.centerOffset,
                                         pd.shareWidth);
        }

        const int curvesBefore = g->curves();
        const bool hadJpBefore = graphHasJapaneseContext(g, curvesBefore);
        double rawYMin = std::numeric_limits<double>::infinity();
        double rawYMax = -std::numeric_limits<double>::infinity();
        for (double yv : mpf.y()) {
            if (std::isfinite(yv)) {
                rawYMin = std::min(rawYMin, yv);
                rawYMax = std::max(rawYMax, yv);
            }
        }
        const double rawYSpan =
                (std::isfinite(rawYMax - rawYMin) && rawYMax >= rawYMin) ? (rawYMax - rawYMin) : 0.0;

        if (rawYSpan <= 1e-12) {
            outResult[QStringLiteral("ok")] = false;
            outResult[QStringLiteral("graph")] = ml->name();
            outResult[QStringLiteral("curve")] = curveTitle;
            outResult[QStringLiteral("peak_count")] = 0;
            outResult[QStringLiteral("peaks")] = QJsonArray();
            outResult[QStringLiteral("r2")] = QJsonValue::Null;
            outResult[QStringLiteral("adj_r2")] = QJsonValue::Null;
            outResult[QStringLiteral("chi2_dof")] = 0.0;
            outResult[QStringLiteral("aic")] = QJsonValue::Null;
            return true;
        }

        ScopedGraphUndoGuard undoGuard(plotFit ? ml : nullptr, QObject::tr("Peak Analysis"));
        mpf.fit();
        if (plotBase && plotFit)
            mpf.insertBaselineCurve();

        Table *summaryTbl = createSummary ? mpf.createPeakSummaryTable() : nullptr;
        Table *integTbl = doIntegrate ? mpf.integratePeaksToTable() : nullptr;
        Table *residTbl = createResiduals ? mpf.createResidualsTable() : nullptr;

        sanitizeGraphFunctionCurves(d_app, g);
        harmonizeFitCurveLabelsOnGraph(g, curvesBefore, hadJpBefore, params);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();

        std::vector<MultiPeakFit::PeakSummaryRow> rows = mpf.computePeakSummary(mpf.results());
        std::sort(rows.begin(), rows.end(),
                  [](const MultiPeakFit::PeakSummaryRow &a,
                     const MultiPeakFit::PeakSummaryRow &b) { return a.center < b.center; });

        double shapeParamVal = 0.0;
        if (profile == MultiPeakFit::PseudoVoigt || profile == MultiPeakFit::Voigt)
            shapeParamVal = mpf.lorentzFraction();
        else if (profile == MultiPeakFit::PearsonVII)
            shapeParamVal = mpf.pearsonShape();
        else if (profile == MultiPeakFit::EMG)
            shapeParamVal = mpf.tailFactor();
        else
            shapeParamVal = mpf.asymmetryFactor();

        const double ampEps = std::max(1e-12, 1e-6 * rawYSpan);
        const double r2 = mpf.rSquare();
        QJsonArray peaksArr;
        int validIdx = 0;
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto &r = rows[i];
            if (!std::isfinite(r.height) || std::abs(r.height) <= ampEps)
                continue;
            QJsonObject pObj;
            pObj[QStringLiteral("index")] = validIdx++;
            pObj[QStringLiteral("center")] = safeJsonDouble(r.center);
            pObj[QStringLiteral("center_se")] = safeJsonDouble(r.centerSE);
            pObj[QStringLiteral("amplitude")] = safeJsonDouble(r.height);
            pObj[QStringLiteral("height")] = safeJsonDouble(r.height);
            pObj[QStringLiteral("amplitude_se")] = safeJsonDouble(r.heightSE);
            pObj[QStringLiteral("fwhm")] = safeJsonDouble(r.fwhm);
            pObj[QStringLiteral("fwhm_se")] = safeJsonDouble(r.fwhmSE);
            pObj[QStringLiteral("area")] = safeJsonDouble(r.area);
            pObj[QStringLiteral("area_se")] = safeJsonDouble(r.areaSE);
            pObj[QStringLiteral("area_pct")] = safeJsonDouble(r.areaPercent);
            pObj[QStringLiteral("baseline")] = safeJsonDouble(r.baseline);
            pObj[QStringLiteral("shape_param")] = safeJsonDouble(shapeParamVal);
            pObj[QStringLiteral("r2")] = safeJsonDouble(r2);
            peaksArr.append(pObj);
        }

        outResult[QStringLiteral("ok")] = !peaksArr.isEmpty();
        outResult[QStringLiteral("converged")] = mpf.converged();
        outResult[QStringLiteral("convergence_message")] = mpf.convergenceMessage();
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("curve")] = curveTitle;
        outResult[QStringLiteral("peak_count")] = peaksArr.size();
        outResult[QStringLiteral("peaks")] = peaksArr;
        outResult[QStringLiteral("r2")] = safeJsonDouble(r2);
        outResult[QStringLiteral("adj_r2")] = safeJsonDouble(mpf.adjustedRSquare());
        outResult[QStringLiteral("chi2_dof")] = safeJsonDouble(mpf.reducedChiSquare());
        outResult[QStringLiteral("aic")] = safeJsonDouble(mpf.aic());
        if (summaryTbl)
            outResult[QStringLiteral("summary_table")] = summaryTbl->name();
        if (integTbl) {
            outResult[QStringLiteral("integration_table")] = integTbl->name();
            outResult[QStringLiteral("integration")] = extractTableAsJson(integTbl);
        }
        if (residTbl)
            outResult[QStringLiteral("residuals_table")] = residTbl->name();
        return true;
    }

    if (method == QStringLiteral("analysis.batch_peaks")) {
        const auto profile = parsePeakProfile(params.value(QStringLiteral("profile")).toString());
        const QString baseStr = params.contains(QStringLiteral("baseline_mode"))
                ? optionalJsonString(params, QStringLiteral("baseline_mode"))
                : optionalJsonString(params, QStringLiteral("baseline"));
        const auto baseMode = parseBaselineMode(baseStr);
        const bool allowNeg = params.value(QStringLiteral("allow_negative")).toBool(false);

        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QStringList curveTitles;
        if (params.contains(QStringLiteral("table"))) {
            Table *t = resolveTable(d_app, params, QStringLiteral("table"), true);
            int xIdx = params.contains(QStringLiteral("x_col"))
                    ? resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true)
                    : t->firstXCol();
            if (xIdx < 0 && t->numCols() >= 2) {
                xIdx = 0;
                t->setColPlotDesignation(0, SciDAVis::X);
            }
            QStringList yCols;
            if (params.value(QStringLiteral("y_cols")).isArray()) {
                for (const QJsonValue &yv : params.value(QStringLiteral("y_cols")).toArray()) {
                    const int yi = resolveColumnIndex(t, yv, true);
                    t->setColPlotDesignation(yi, SciDAVis::Y);
                    yCols << fullColumnName(t, yi);
                }
            } else {
                for (int c = 0; c < t->numCols(); ++c) {
                    if (c != xIdx && t->colPlotDesignation(c) == SciDAVis::Y)
                        yCols << fullColumnName(t, c);
                }
            }
            ml = d_app->multilayerPlot(t, yCols, Graph::Line, 0, t->numRows() - 1);
            if (!ml)
                throwRpcRuntime(QStringLiteral("Failed to plot table columns for batch peaks."));
            g = ml->activeGraph();
            curveTitles = g ? g->analysableCurvesList() : QStringList();
        } else {
            ml = resolveMultiLayer(d_app, params);
            g = resolveGraphLayer(ml, params);
            curveTitles = g->analysableCurvesList();
        }
        Table *batchTbl = MultiPeakFit::batchAnalyzeCurves(d_app, g, curveTitles, profile, baseMode,
                                                           allowNeg);
        if (g) {
            sanitizeGraphFunctionCurves(d_app, g);
            enforceArialGraphFonts(g);
            g->replot();
        }
        if (!batchTbl)
            throwRpcRuntime(QStringLiteral("Batch peak analysis produced no peaks."));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = batchTbl->name();
        outResult[QStringLiteral("summary")] = extractTableAsJson(batchTbl);
        return true;
    }

    // ------------------------------------------------------------------------
    // 7A. Interactive Gadgets (gadget.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("gadget.attach")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString typeStr = params.value(QStringLiteral("type")).toString().trimmed().toLower();

        if (typeStr == QStringLiteral("vertical_cursor") || typeStr == QStringLiteral("vcursor")) {
            auto *vc = VerticalCursorTool::attachCursor(
                    d_app, g, params.value(QStringLiteral("sync_layers")).toBool(true));
            if (params.contains(QStringLiteral("x")))
                vc->setCursorX(params.value(QStringLiteral("x")).toDouble());
            outResult[QStringLiteral("ok")] = (vc != nullptr);
            outResult[QStringLiteral("type")] = QStringLiteral("vertical_cursor");
            outResult[QStringLiteral("readout")] = vc ? vc->readoutText() : QString();
            return true;
        }
        if (typeStr == QStringLiteral("delta_cursor") || typeStr == QStringLiteral("delta")) {
            auto *dc = DeltaCursorTool::attach(d_app, g, DeltaCursorTool::SnapInterpolateCurve);
            if (params.contains(QStringLiteral("curve")))
                dc->setTargetCurve(resolveCurveTitle(g, params, QStringLiteral("curve"), false));
            if (params.contains(QStringLiteral("x1")) && params.contains(QStringLiteral("x2")))
                dc->setCursorPositions(params.value(QStringLiteral("x1")).toDouble(),
                                       params.value(QStringLiteral("x2")).toDouble());
            const auto rd = dc->currentReadout();
            outResult[QStringLiteral("ok")] = (dc != nullptr);
            outResult[QStringLiteral("type")] = QStringLiteral("delta_cursor");
            outResult[QStringLiteral("dx")] = safeJsonDouble(rd.dx);
            outResult[QStringLiteral("dy")] = safeJsonDouble(rd.dy);
            outResult[QStringLiteral("slope")] = safeJsonDouble(rd.slope);
            outResult[QStringLiteral("area")] = safeJsonDouble(rd.area);
            outResult[QStringLiteral("rms")] = safeJsonDouble(rd.rms);
            outResult[QStringLiteral("readout")] = dc->readoutText();
            return true;
        }
        if (typeStr == QStringLiteral("image_profile") || typeStr == QStringLiteral("profile_2d")) {
            auto *ip = ImageProfileGadgetTool::attach(g, d_app);
            if (params.contains(QStringLiteral("x")) && params.contains(QStringLiteral("y")))
                ip->setCrosshairPosition(params.value(QStringLiteral("x")).toDouble(),
                                         params.value(QStringLiteral("y")).toDouble());
            outResult[QStringLiteral("ok")] = (ip != nullptr);
            outResult[QStringLiteral("type")] = QStringLiteral("image_profile");
            outResult[QStringLiteral("z")] = ip ? safeJsonDouble(ip->currentZ()) : QJsonValue::Null;
            outResult[QStringLiteral("horizontal_fwhm")] =
                    ip ? safeJsonDouble(ip->horizontalFwhm()) : QJsonValue::Null;
            outResult[QStringLiteral("vertical_fwhm")] =
                    ip ? safeJsonDouble(ip->verticalFwhm()) : QJsonValue::Null;
            return true;
        }

        const auto roiMode = parseRoiGadgetMode(typeStr);
        auto *roi = RoiGadgetTool::attachGadget(d_app, g, roiMode);
        if (!roi)
            throwRpcRuntime(QStringLiteral("Failed to attach ROI gadget."));
        if (params.contains(QStringLiteral("curve")))
            roi->setTargetCurve(resolveCurveTitle(g, params, QStringLiteral("curve"), false));
        if (params.contains(QStringLiteral("x_min")) && params.contains(QStringLiteral("x_max"))) {
            QRectF r = roi->roiRect();
            const double x0 = params.value(QStringLiteral("x_min")).toDouble(r.left());
            const double x1 = params.value(QStringLiteral("x_max")).toDouble(r.right());
            const double y0 = params.value(QStringLiteral("y_min")).toDouble(r.top());
            const double y1 = params.value(QStringLiteral("y_max")).toDouble(r.bottom());
            roi->setRoiRect(QRectF(QPointF(std::min(x0, x1), std::min(y0, y1)),
                                   QPointF(std::max(x0, x1), std::max(y0, y1))));
        }
        roi->recalculate();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("type")] = typeStr;
        outResult[QStringLiteral("summary")] = roi->summaryText();
        outResult[QStringLiteral("primary_metric")] = safeJsonDouble(roi->primaryMetric());
        outResult[QStringLiteral("secondary_metric")] = safeJsonDouble(roi->secondaryMetric());
        return true;
    }

    if (method == QStringLiteral("gadget.set_roi")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        PlotToolInterface *tool = g->activeTool();
        if (!tool)
            throwInvalidParams(QStringLiteral("No active gadget on graph '%1'.").arg(ml->name()));

        if (auto *roi = dynamic_cast<RoiGadgetTool *>(tool)) {
            QRectF cur = roi->roiRect();
            const double x0 = params.value(QStringLiteral("x_min")).toDouble(cur.left());
            const double x1 = params.value(QStringLiteral("x_max")).toDouble(cur.right());
            const double y0 = params.value(QStringLiteral("y_min")).toDouble(cur.top());
            const double y1 = params.value(QStringLiteral("y_max")).toDouble(cur.bottom());
            roi->setRoiRect(QRectF(QPointF(std::min(x0, x1), std::min(y0, y1)),
                                   QPointF(std::max(x0, x1), std::max(y0, y1))));
            roi->recalculate();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("summary")] = roi->summaryText();
            outResult[QStringLiteral("primary_metric")] = safeJsonDouble(roi->primaryMetric());
            outResult[QStringLiteral("secondary_metric")] = safeJsonDouble(roi->secondaryMetric());
            return true;
        }
        if (auto *dc = dynamic_cast<DeltaCursorTool *>(tool)) {
            dc->setCursorPositions(params.value(QStringLiteral("x1")).toDouble(),
                                   params.value(QStringLiteral("x2")).toDouble());
            const auto rd = dc->currentReadout();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("dx")] = safeJsonDouble(rd.dx);
            outResult[QStringLiteral("dy")] = safeJsonDouble(rd.dy);
            outResult[QStringLiteral("slope")] = safeJsonDouble(rd.slope);
            outResult[QStringLiteral("area")] = safeJsonDouble(rd.area);
            return true;
        }
        if (auto *vc = dynamic_cast<VerticalCursorTool *>(tool)) {
            vc->setCursorX(params.value(QStringLiteral("x")).toDouble());
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("readout")] = vc->readoutText();
            return true;
        }
        if (auto *ip = dynamic_cast<ImageProfileGadgetTool *>(tool)) {
            ip->setCrosshairPosition(params.value(QStringLiteral("x")).toDouble(),
                                     params.value(QStringLiteral("y")).toDouble());
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("z")] = safeJsonDouble(ip->currentZ());
            return true;
        }
        throwRpcRuntime(QStringLiteral("Unsupported active tool on graph."));
    }

    if (method == QStringLiteral("gadget.query")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        PlotToolInterface *tool = g->activeTool();
        if (!tool)
            throwInvalidParams(QStringLiteral("No active gadget on graph '%1'.").arg(ml->name()));

        if (auto *roi = dynamic_cast<RoiGadgetTool *>(tool)) {
            roi->recalculate();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("type")] = QStringLiteral("roi");
            outResult[QStringLiteral("summary")] = roi->summaryText();
            outResult[QStringLiteral("primary_metric")] = safeJsonDouble(roi->primaryMetric());
            outResult[QStringLiteral("secondary_metric")] = safeJsonDouble(roi->secondaryMetric());
            if (params.value(QStringLiteral("create_table")).toBool(false)) {
                if (Table *ot = roi->createOutputTable()) {
                    outResult[QStringLiteral("table")] = ot->name();
                    outResult[QStringLiteral("data")] = extractTableAsJson(ot);
                }
            }
            return true;
        }
        if (auto *dc = dynamic_cast<DeltaCursorTool *>(tool)) {
            const auto rd = dc->currentReadout();
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("type")] = QStringLiteral("delta_cursor");
            outResult[QStringLiteral("x1")] = safeJsonDouble(rd.x1);
            outResult[QStringLiteral("y1")] = safeJsonDouble(rd.y1);
            outResult[QStringLiteral("x2")] = safeJsonDouble(rd.x2);
            outResult[QStringLiteral("y2")] = safeJsonDouble(rd.y2);
            outResult[QStringLiteral("dx")] = safeJsonDouble(rd.dx);
            outResult[QStringLiteral("dy")] = safeJsonDouble(rd.dy);
            outResult[QStringLiteral("slope")] = safeJsonDouble(rd.slope);
            outResult[QStringLiteral("ratio")] = safeJsonDouble(rd.ratio);
            outResult[QStringLiteral("area")] = safeJsonDouble(rd.area);
            outResult[QStringLiteral("rms")] = safeJsonDouble(rd.rms);
            outResult[QStringLiteral("min_y")] = safeJsonDouble(rd.minY);
            outResult[QStringLiteral("max_y")] = safeJsonDouble(rd.maxY);
            outResult[QStringLiteral("readout")] = dc->readoutText();
            return true;
        }
        if (auto *vc = dynamic_cast<VerticalCursorTool *>(tool)) {
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("type")] = QStringLiteral("vertical_cursor");
            outResult[QStringLiteral("x")] = safeJsonDouble(vc->cursorX());
            outResult[QStringLiteral("readout")] = vc->readoutText();
            QJsonObject valsObj;
            for (const auto &kv : vc->interpolatedValues())
                valsObj[kv.first] = safeJsonDouble(kv.second);
            outResult[QStringLiteral("values")] = valsObj;
            return true;
        }
        if (auto *ip = dynamic_cast<ImageProfileGadgetTool *>(tool)) {
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("type")] = QStringLiteral("image_profile");
            outResult[QStringLiteral("x")] = safeJsonDouble(ip->crosshairPosition().x());
            outResult[QStringLiteral("y")] = safeJsonDouble(ip->crosshairPosition().y());
            outResult[QStringLiteral("z")] = safeJsonDouble(ip->currentZ());
            outResult[QStringLiteral("horizontal_fwhm")] = safeJsonDouble(ip->horizontalFwhm());
            outResult[QStringLiteral("vertical_fwhm")] = safeJsonDouble(ip->verticalFwhm());
            outResult[QStringLiteral("readout")] = ip->readoutText();
            if (params.value(QStringLiteral("create_table")).toBool(false)) {
                if (Table *pt = ip->extractProfilesToTableAndGraph(
                            params.value(QStringLiteral("plot")).toBool(false))) {
                    outResult[QStringLiteral("table")] = pt->name();
                    outResult[QStringLiteral("data")] = extractTableAsJson(pt);
                }
            }
            return true;
        }
        throwRpcRuntime(QStringLiteral("Unsupported active tool on graph."));
    }

    if (method == QStringLiteral("gadget.clear")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString typeStr =
                optionalJsonString(params, QStringLiteral("type")).trimmed().toLower();
        bool cleared = false;
        PlotToolInterface *tool = g->activeTool();
        if (typeStr.isEmpty() || typeStr == QStringLiteral("all")) {
            if (tool) {
                RoiGadgetTool::clearGadget(g);
                cleared = true;
            }
        } else if (typeStr == QStringLiteral("vertical_cursor")
                   || typeStr == QStringLiteral("vcursor")) {
            if (dynamic_cast<VerticalCursorTool *>(tool)) {
                RoiGadgetTool::clearGadget(g);
                cleared = true;
            }
        } else if (typeStr == QStringLiteral("delta_cursor")
                   || typeStr == QStringLiteral("delta")) {
            if (dynamic_cast<DeltaCursorTool *>(tool)) {
                RoiGadgetTool::clearGadget(g);
                cleared = true;
            }
        } else if (typeStr == QStringLiteral("image_profile")
                   || typeStr == QStringLiteral("profile_2d")) {
            if (dynamic_cast<ImageProfileGadgetTool *>(tool)) {
                RoiGadgetTool::clearGadget(g);
                cleared = true;
            }
        } else if (typeStr == QStringLiteral("roi")) {
            if (dynamic_cast<RoiGadgetTool *>(tool)) {
                RoiGadgetTool::clearGadget(g);
                cleared = true;
            }
        } else {
            const auto targetMode = parseRoiGadgetMode(typeStr);
            if (auto *roi = dynamic_cast<RoiGadgetTool *>(tool)) {
                if (roi->gadgetMode() == targetMode) {
                    RoiGadgetTool::clearGadget(g);
                    cleared = true;
                }
            }
        }
        if (cleared)
            g->replot();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("cleared")] = cleared;
        return true;
    }

    // ------------------------------------------------------------------------
    // 7B. Signal Processing (signal.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("signal.smooth")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        const int nPts = requireCurveMinPoints(g, curveTitle, 3, QStringLiteral("signal.smooth"));

        const QString mStr =
                optionalJsonString(params, QStringLiteral("method"), QStringLiteral("savgol"))
                        .trimmed()
                        .toLower();
        bool isGaussianKernel = false;
        int sm = SmoothFilter::SavitzkyGolay;
        if (mStr.isEmpty() || mStr == QStringLiteral("savgol")
            || mStr == QStringLiteral("savitzky_golay") || mStr == QStringLiteral("savitzky-golay"))
            sm = SmoothFilter::SavitzkyGolay;
        else if (mStr == QStringLiteral("whittaker"))
            sm = SmoothFilter::Whittaker;
        else if (mStr == QStringLiteral("lowess") || mStr == QStringLiteral("loess"))
            sm = SmoothFilter::LOWESS;
        else if (mStr == QStringLiteral("median"))
            sm = SmoothFilter::Median;
        else if (mStr == QStringLiteral("fft"))
            sm = SmoothFilter::FFT;
        else if (mStr == QStringLiteral("moving_avg") || mStr == QStringLiteral("average")
                 || mStr == QStringLiteral("moving_average"))
            sm = SmoothFilter::Average;
        else if (mStr == QStringLiteral("gaussian") || mStr == QStringLiteral("gauss"))
            isGaussianKernel = true;
        else
            throwInvalidParams(
                    QStringLiteral("Unsupported smooth method '%1' (expected 'savgol', 'whittaker', 'lowess', 'median', 'fft', 'average', or 'gaussian').")
                            .arg(mStr));

        int win = std::min(7, nPts);
        if (params.contains(QStringLiteral("window")) && !params.value(QStringLiteral("window")).isNull()) {
            win = parseStrictJsonInt(params.value(QStringLiteral("window")), QStringLiteral("window"));
            if (win < 2 || win > nPts)
                throwInvalidParams(
                        QStringLiteral("signal.smooth 'window' (%1) must satisfy 2 <= window <= %2 (curve points).")
                                .arg(win)
                                .arg(nPts));
        } else if (params.contains(QStringLiteral("points"))
                   && !params.value(QStringLiteral("points")).isNull()) {
            win = parseStrictJsonInt(params.value(QStringLiteral("points")), QStringLiteral("points"));
            if (win < 2 || win > nPts)
                throwInvalidParams(
                        QStringLiteral("signal.smooth 'points' (%1) must satisfy 2 <= points <= %2 (curve points).")
                                .arg(win)
                                .arg(nPts));
        }

        int halfLeft = (win - 1) / 2;
        int halfRight = win - 1 - halfLeft;
        if (params.contains(QStringLiteral("left_points"))
            && !params.value(QStringLiteral("left_points")).isNull()) {
            halfLeft = parseStrictJsonInt(params.value(QStringLiteral("left_points")),
                                          QStringLiteral("left_points"));
            if (halfLeft < 0)
                throwInvalidParams(QStringLiteral("signal.smooth 'left_points' must be >= 0."));
        }
        if (params.contains(QStringLiteral("right_points"))
            && !params.value(QStringLiteral("right_points")).isNull()) {
            halfRight = parseStrictJsonInt(params.value(QStringLiteral("right_points")),
                                           QStringLiteral("right_points"));
            if (halfRight < 0)
                throwInvalidParams(QStringLiteral("signal.smooth 'right_points' must be >= 0."));
        }
        const int totalWin = halfLeft + halfRight + 1;
        if (totalWin < 2 || totalWin > nPts) {
            throwInvalidParams(
                    QStringLiteral("signal.smooth window (%1) must satisfy 2 <= window <= %2 (curve points).")
                            .arg(totalWin)
                            .arg(nPts));
        }

        if (isGaussianKernel) {
            std::vector<double> xs, ys;
            extractValidUnmaskedCurveXY(g->curve(g->curveIndex(curveTitle)), xs, ys);
            const int nValid = static_cast<int>(xs.size());
            if (nValid < 2)
                throwInvalidParams(
                        QStringLiteral("signal.smooth ('gaussian') requires >= 2 valid points."));
            const double sigma = params.contains(QStringLiteral("sigma"))
                    ? std::max(0.25,
                               optionalJsonFiniteDouble(params, QStringLiteral("sigma"),
                                                        std::max(0.75, double(totalWin) / 4.0),
                                                        1e6))
                    : std::max(0.75, double(totalWin) / 4.0);
            std::vector<double> ySmooth(size_t(nValid), 0.0);
            for (int i = 0; i < nValid; ++i) {
                double wSum = 0.0;
                double acc = 0.0;
                const int jLo = std::max(0, i - halfLeft);
                const int jHi = std::min(nValid - 1, i + halfRight);
                for (int j = jLo; j <= jHi; ++j) {
                    const double z = double(j - i) / sigma;
                    const double w = std::exp(-0.5 * z * z);
                    wSum += w;
                    acc += w * ys[size_t(j)];
                }
                ySmooth[size_t(i)] = (wSum > 0.0) ? (acc / wSum) : ys[size_t(i)];
            }
            Table *smTbl = createUniqueTableHeadless(d_app, nValid, 2, QStringLiteral("GaussSmooth"));
            smTbl->setColName(0, QStringLiteral("X"));
            smTbl->setColName(1, QStringLiteral("Smoothed"));
            smTbl->setColPlotDesignation(0, SciDAVis::X);
            smTbl->setColPlotDesignation(1, SciDAVis::Y);
            smTbl->column(0)->replaceValues(0, QVector<double>::fromStdVector(xs));
            smTbl->column(1)->replaceValues(0, QVector<double>::fromStdVector(ySmooth));
            smTbl->hide();
            const int curvesBefore = g->curves();
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Smooth Curve"));
            const QString fullY = fullColumnName(smTbl, 1);
            g->insertCurve(smTbl, fullColumnName(smTbl, 0), fullY, Graph::Line);
            if (g->curves() > curvesBefore) {
                QPen p(QColor("#D55E00"), 1.8);
                g->setCurvePen(g->curves() - 1, p);
                // 平滑化した曲線は元の曲線と同じ軸に付ける（右軸の曲線で左軸が広がらないように）。
                CurveMathTools::attachToSourceAxes(g, g->curve(g->curves() - 1),
                                                   g->curve(g->curveIndex(curveTitle)));
                g->updatePlot();
            }
            enforceArialGraphFonts(g);
            replotGraphOrDefer(g);
            if (g->curves() > curvesBefore)
                undoGuard.commit();
            outResult[QStringLiteral("ok")] = (g->curves() > curvesBefore);
            outResult[QStringLiteral("graph")] = ml->name();
            outResult[QStringLiteral("table")] = smTbl->name();
            outResult[QStringLiteral("curve_count")] = g->curves();
            if (g->curves() > curvesBefore && g->curve(g->curves() - 1))
                outResult[QStringLiteral("output_curve")] = g->curve(g->curves() - 1)->title().text();
            // The kernel works on the point index: say so when X is not equally spaced or not
            // monotonic (as the other smoothers and differentiate do).
            const QString gaussNotice = SmoothFilter::indexSmoothingXNotice(xs, QString());
            if (!gaussNotice.isEmpty())
                outResult[QStringLiteral("notice")] = gaussNotice;
            return true;
        }

        int polyOrder = std::min(2, totalWin - 1);
        const QJsonValue polyVal = params.contains(QStringLiteral("poly_order"))
                ? params.value(QStringLiteral("poly_order"))
                : params.value(QStringLiteral("order"));
        if (sm == SmoothFilter::SavitzkyGolay && !polyVal.isUndefined() && !polyVal.isNull()) {
            polyOrder = parseStrictJsonInt(polyVal, QStringLiteral("poly_order"));
            if (polyOrder < 1 || polyOrder >= totalWin) {
                throwInvalidParams(
                        QStringLiteral("Savitzky-Golay 'poly_order' (%1) must satisfy 1 <= poly_order < window (%2).")
                                .arg(polyOrder)
                                .arg(totalWin));
            }
        }
        double lowessSpan = 0.2;
        if (sm == SmoothFilter::LOWESS && params.contains(QStringLiteral("span"))
            && !params.value(QStringLiteral("span")).isNull()) {
            lowessSpan = params.value(QStringLiteral("span")).toDouble(-1.0);
            if (!std::isfinite(lowessSpan) || lowessSpan <= 0.0 || lowessSpan > 1.0)
                throwInvalidParams(QStringLiteral("LOWESS 'span' must satisfy 0 < span <= 1."));
        }
        double whittakerLambda = 100.0;
        const bool hasLambda = params.contains(QStringLiteral("lambda"))
                && !params.value(QStringLiteral("lambda")).isNull();
        const bool hasLambdaVal = params.contains(QStringLiteral("lambda_val"))
                && !params.value(QStringLiteral("lambda_val")).isNull();
        if (sm == SmoothFilter::Whittaker && (hasLambda || hasLambdaVal)) {
            const QJsonValue lv = hasLambda ? params.value(QStringLiteral("lambda"))
                                            : params.value(QStringLiteral("lambda_val"));
            whittakerLambda = lv.toDouble(-1.0);
            if (!std::isfinite(whittakerLambda) || whittakerLambda <= 0.0)
                throwInvalidParams(QStringLiteral("Whittaker 'lambda' must be > 0."));
        }

        const int curvesBefore = g->curves();
        RpcSmoothFilter sf(d_app, g, curveTitle, sm);
        if (sm == SmoothFilter::Average) {
            // SmoothFilter::smoothAverage uses p2 = d_right_points / 2 (window = 2*p2 + 1 <= nPts)
            const int avgPts = (totalWin % 2 == 0) ? std::max(1, totalWin - 1) : totalWin;
            sf.setSmoothPoints(avgPts, avgPts / 2);
        } else if (sm == SmoothFilter::FFT) {
            // SmoothFilter::smoothFFT uses cutoff lf = (1/dx) / d_right_points (full window points,
            // matching SmoothCurveDialog::smooth()); passing halfRight made window=5 give lf = fs/2 (no-op).
            sf.setSmoothPoints(totalWin, 0);
        } else if (sm == SmoothFilter::SavitzkyGolay) {
            // Reset polynom order to 1 before setSmoothPoints so default order=2 never clashes when totalWin==2
            sf.setPolynomOrder(1);
            sf.setSmoothPoints(halfRight, halfLeft);
            sf.setPolynomOrder(polyOrder);
        } else {
            sf.setSmoothPoints(halfRight, halfLeft);
        }
        if (sm == SmoothFilter::LOWESS)
            sf.setLowessSpan(std::clamp(lowessSpan, 0.02, 1.0));
        if (sm == SmoothFilter::Whittaker)
            sf.setWhittakerLambda(whittakerLambda);

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Smooth Curve"));
        const bool ok = sf.run() && (g->curves() > curvesBefore);
        enforceArialGraphFonts(g);
        g->replot();
        if (ok)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = ok;
        // How X was treated (not equally spaced / not monotonic): it only went to the results
        // log before; differentiate returns its notice in the result, and so does smooth now.
        if (!sf.xHandlingNotice().isEmpty())
            outResult[QStringLiteral("notice")] = sf.xHandlingNotice();
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("curve_count")] = g->curves();
        if (g->curves() > curvesBefore && g->curve(g->curves() - 1))
            outResult[QStringLiteral("output_curve")] = g->curve(g->curves() - 1)->title().text();
        return true;
    }

    if (method == QStringLiteral("signal.savgol_derivative")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        const int nPts =
                requireCurveMinPoints(g, curveTitle, 5, QStringLiteral("signal.savgol_derivative"));

        int derivOrder = 1;
        const QJsonValue dOrdVal = params.contains(QStringLiteral("deriv_order"))
                ? params.value(QStringLiteral("deriv_order"))
                : params.value(QStringLiteral("order"));
        if (!dOrdVal.isUndefined() && !dOrdVal.isNull()) {
            derivOrder = parseStrictJsonInt(dOrdVal, QStringLiteral("order"));
            if (derivOrder < 1 || derivOrder > 3)
                throwInvalidParams(QStringLiteral("'order' / 'deriv_order' must be 1, 2, or 3."));
        }
        int win = std::min(9, (nPts % 2 == 1) ? nPts : (nPts - 1));
        if (params.contains(QStringLiteral("window")) && !params.value(QStringLiteral("window")).isNull()) {
            win = parseStrictJsonInt(params.value(QStringLiteral("window")), QStringLiteral("window"));
            if (win < 5 || win > nPts)
                throwInvalidParams(
                        QStringLiteral("signal.savgol_derivative 'window' (%1) must satisfy 5 <= window <= %2.")
                                .arg(win)
                                .arg(nPts));
        }
        int poly = std::min(3, win - 1);
        if (params.contains(QStringLiteral("poly_order"))
            && !params.value(QStringLiteral("poly_order")).isNull()) {
            poly = parseStrictJsonInt(params.value(QStringLiteral("poly_order")),
                                      QStringLiteral("poly_order"));
            if (poly < derivOrder || poly >= win)
                throwInvalidParams(
                        QStringLiteral("signal.savgol_derivative 'poly_order' (%1) must satisfy %2 <= poly_order < window (%3).")
                                .arg(poly)
                                .arg(derivOrder)
                                .arg(win));
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Savitzky-Golay Derivative"));
        QwtPlotCurve *outC = AdvancedSignalProc::savitzkyGolayDerivative(d_app, g, curveTitle,
                                                                         derivOrder, win, poly);
        enforceArialGraphFonts(g);
        g->replot();
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        outResult[QStringLiteral("graph")] = ml->name();
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    if (method == QStringLiteral("signal.fft")) {
        const int beforeTables = d_app->windowsList().size();
        const bool hasExplicitSampling =
                (params.contains(QStringLiteral("sampling"))
                 && !params.value(QStringLiteral("sampling")).isNull())
                || (params.contains(QStringLiteral("dt"))
                    && !params.value(QStringLiteral("dt")).isNull());
        double explicitDt = 0.0;
        if (hasExplicitSampling) {
            const QJsonValue sv = params.contains(QStringLiteral("sampling"))
                    ? params.value(QStringLiteral("sampling"))
                    : params.value(QStringLiteral("dt"));
            explicitDt = sv.toDouble(-1.0);
            if (!std::isfinite(explicitDt) || explicitDt <= 0.0)
                throwInvalidParams(
                        QStringLiteral("signal.fft 'sampling'/'dt' interval must be finite and > 0."));
        }
        bool ok = false;
        if (params.contains(QStringLiteral("table")) && !params.contains(QStringLiteral("graph"))) {
            Table *t = resolveTable(d_app, params);
            const int rCol = resolveColumnIndex(
                    t, params.contains(QStringLiteral("real_col"))
                            ? params.value(QStringLiteral("real_col"))
                            : params.value(QStringLiteral("y_col")),
                    true);
            int validRows = 0;
            if (Column *colPtr = t->column(rCol)) {
                for (int r = 0; r < t->numRows(); ++r) {
                    if (!colPtr->isInvalid(r) && !colPtr->isMasked(r)
                        && std::isfinite(colPtr->valueAt(r)))
                        ++validRows;
                }
            }
            if (validRows < 2) {
                throwInvalidParams(
                        QStringLiteral("signal.fft requires >= 2 valid unmasked numeric rows in column '%1' (got %2).")
                                .arg(t->colLabel(rCol))
                                .arg(validRows));
            }
            QString imagName;
            if (params.contains(QStringLiteral("imag_col"))
                && !params.value(QStringLiteral("imag_col")).isNull()) {
                imagName = fullColumnName(
                        t, resolveColumnIndex(t, params.value(QStringLiteral("imag_col")), true));
            }
            RpcFFT fft(d_app, t, fullColumnName(t, rCol), imagName);
            if (hasExplicitSampling)
                fft.setSampling(explicitDt);
            fft.setInverseFFT(params.value(QStringLiteral("inverse")).toBool(false));
            fft.normalizeAmplitudes(params.value(QStringLiteral("normalize")).toBool(true));
            fft.shiftFrequencies(params.value(QStringLiteral("shift")).toBool(false));
            ok = fft.run();
        } else {
            MultiLayer *ml = nullptr;
            Graph *g = nullptr;
            QString curveTitle;
            resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
            requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("signal.fft"));
            std::vector<double> xs, ys;
            extractValidUnmaskedCurveXY(g->curve(g->curveIndex(curveTitle)), xs, ys);
            if (xs.size() < 2) {
                throwInvalidParams(
                        QStringLiteral("signal.fft requires >= 2 valid unmasked points on curve '%1'.")
                                .arg(curveTitle));
            }
            RpcFFT fft(d_app, g, curveTitle);
            // Without an explicit interval keep the one FFT derived from its own uniform grid
            // (ensureUniformGridAndSampling: span / (valid points - 1)). |x1 - x0| is wrong for
            // non-uniform, descending or partly invalid X and shifted the frequency axis.
            if (hasExplicitSampling)
                fft.setSampling(explicitDt);
            fft.setInverseFFT(params.value(QStringLiteral("inverse")).toBool(false));
            fft.normalizeAmplitudes(params.value(QStringLiteral("normalize")).toBool(true));
            fft.shiftFrequencies(params.value(QStringLiteral("shift")).toBool(false));
            ok = fft.run();
        }
        Table *fftTbl = nullptr;
        const auto allWin = d_app->windowsList();
        for (int i = allWin.size() - 1; i >= std::max(0, beforeTables); --i) {
            if (allWin[i] && allWin[i]->inherits("Table")) {
                fftTbl = static_cast<Table *>(allWin[i]);
                break;
            }
        }
        outResult[QStringLiteral("ok")] = ok && (fftTbl != nullptr);
        if (fftTbl) {
            outResult[QStringLiteral("table")] = fftTbl->name();
            outResult[QStringLiteral("result_table")] = fftTbl->name();
        }
        return true;
    }

    if (method == QStringLiteral("signal.fft_filter")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("signal.fft_filter"));
        {
            std::vector<double> xs, ys;
            extractValidUnmaskedCurveXY(g->curve(g->curveIndex(curveTitle)), xs, ys);
            if (xs.size() < 4) {
                throwInvalidParams(QStringLiteral(
                        "signal.fft_filter requires >= 4 valid unmasked curve points."));
            }
            bool inc = true, dec = true;
            for (size_t k = 1; k < xs.size(); ++k) {
                const double dx = xs[k] - xs[k - 1];
                if (!(dx > 1e-15))
                    inc = false;
                if (!(dx < -1e-15))
                    dec = false;
            }
            if (!inc && !dec) {
                throwInvalidParams(QStringLiteral(
                        "signal.fft_filter requires strictly monotonic X coordinates without duplicates."));
            }
        }

        const QString fType = (params.contains(QStringLiteral("filter_type"))
                                       ? params.value(QStringLiteral("filter_type")).toString()
                                       : (params.contains(QStringLiteral("pass_type"))
                                                  ? params.value(QStringLiteral("pass_type")).toString()
                                                  : params.value(QStringLiteral("type")).toString()))
                                      .trimmed()
                                      .toLower();
        int ft = FFTFilter::LowPass;
        if (fType.isEmpty() || fType == QStringLiteral("lowpass")
            || fType == QStringLiteral("low_pass"))
            ft = FFTFilter::LowPass;
        else if (fType == QStringLiteral("highpass") || fType == QStringLiteral("high_pass"))
            ft = FFTFilter::HighPass;
        else if (fType == QStringLiteral("bandpass") || fType == QStringLiteral("band_pass"))
            ft = FFTFilter::BandPass;
        else if (fType == QStringLiteral("bandstop") || fType == QStringLiteral("bandblock")
                 || fType == QStringLiteral("band_block") || fType == QStringLiteral("band_stop"))
            ft = FFTFilter::BandBlock;
        else
            throwInvalidParams(
                    QStringLiteral("Unsupported FFT filter type '%1' (expected 'lowpass', 'highpass', 'bandpass', or 'bandstop').")
                            .arg(fType));

        const double lowFreq = params.contains(QStringLiteral("cutoff_low"))
                ? params.value(QStringLiteral("cutoff_low")).toDouble(-1.0)
                : (params.contains(QStringLiteral("low_freq"))
                           ? params.value(QStringLiteral("low_freq")).toDouble(-1.0)
                           : params.value(QStringLiteral("cutoff")).toDouble(0.1));
        const double highFreq = params.contains(QStringLiteral("cutoff_high"))
                ? params.value(QStringLiteral("cutoff_high")).toDouble(-1.0)
                : (params.contains(QStringLiteral("high_cutoff"))
                           ? params.value(QStringLiteral("high_cutoff")).toDouble(-1.0)
                           : params.value(QStringLiteral("high_freq")).toDouble(1.0));
        if (ft == FFTFilter::BandPass || ft == FFTFilter::BandBlock) {
            if (!std::isfinite(lowFreq) || !std::isfinite(highFreq) || lowFreq <= 0.0
                || highFreq <= lowFreq) {
                throwInvalidParams(QStringLiteral(
                        "signal.fft_filter bandpass/bandstop requires 0 < cutoff_low < cutoff_high."));
            }
        } else {
            const double cutoff = params.contains(QStringLiteral("cutoff"))
                    ? params.value(QStringLiteral("cutoff")).toDouble(-1.0)
                    : (params.contains(QStringLiteral("cutoff_low"))
                                       || params.contains(QStringLiteral("low_freq"))
                               ? lowFreq
                               : 1.0);
            if (!std::isfinite(cutoff) || cutoff <= 0.0)
                throwInvalidParams(QStringLiteral("signal.fft_filter 'cutoff' must be > 0."));
        }

        const int curvesBefore = g->curves();
        RpcFFTFilter ff(d_app, g, curveTitle, ft);
        if (ft == FFTFilter::BandPass || ft == FFTFilter::BandBlock) {
            ff.setBand(lowFreq, highFreq);
            ff.enableOffset(params.value(QStringLiteral("keep_dc")).toBool(true));
        } else {
            ff.setCutoff(params.contains(QStringLiteral("cutoff"))
                                 ? params.value(QStringLiteral("cutoff")).toDouble(1.0)
                                 : (params.contains(QStringLiteral("cutoff_low"))
                                                    || params.contains(QStringLiteral("low_freq"))
                                            ? lowFreq
                                            : 1.0));
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("FFT Filter"));
        const bool ok = ff.run() && (g->curves() > curvesBefore);
        enforceArialGraphFonts(g);
        g->replot();
        if (ok)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("graph")] = ml->name();
        if (g->curves() > curvesBefore && g->curve(g->curves() - 1))
            outResult[QStringLiteral("output_curve")] = g->curve(g->curves() - 1)->title().text();
        return true;
    }

    if (method == QStringLiteral("signal.iir_filter")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("signal.iir_filter"));

        const QString dStr = params.value(QStringLiteral("design")).toString().trimmed().toLower();
        auto design = AdvancedSignalProc::Butterworth;
        if (dStr.isEmpty() || dStr == QStringLiteral("butterworth"))
            design = AdvancedSignalProc::Butterworth;
        else if (dStr == QStringLiteral("chebyshev1") || dStr == QStringLiteral("chebyshev"))
            design = AdvancedSignalProc::ChebyshevI;
        else if (dStr == QStringLiteral("bessel"))
            design = AdvancedSignalProc::Bessel;
        else
            throwInvalidParams(
                    QStringLiteral("Unsupported IIR design '%1' (expected 'butterworth', 'chebyshev1', or 'bessel').")
                            .arg(dStr));

        const QString pStr = (params.contains(QStringLiteral("filter_type"))
                                      ? optionalJsonString(params, QStringLiteral("filter_type"))
                                      : (params.contains(QStringLiteral("mode"))
                                                 ? optionalJsonString(params, QStringLiteral("mode"))
                                                 : (params.contains(QStringLiteral("pass_type"))
                                                            ? optionalJsonString(
                                                                      params,
                                                                      QStringLiteral("pass_type"))
                                                            : optionalJsonString(
                                                                      params,
                                                                      QStringLiteral("type")))))
                                     .trimmed()
                                     .toLower();
        auto pass = AdvancedSignalProc::LowPass;
        QString canonicalPass = QStringLiteral("lowpass");
        if (pStr.isEmpty() || pStr == QStringLiteral("lowpass") || pStr == QStringLiteral("low_pass")) {
            pass = AdvancedSignalProc::LowPass;
            canonicalPass = QStringLiteral("lowpass");
        } else if (pStr == QStringLiteral("highpass") || pStr == QStringLiteral("high_pass")) {
            pass = AdvancedSignalProc::HighPass;
            canonicalPass = QStringLiteral("highpass");
        } else if (pStr == QStringLiteral("bandpass") || pStr == QStringLiteral("band_pass")) {
            pass = AdvancedSignalProc::BandPass;
            canonicalPass = QStringLiteral("bandpass");
        } else if (pStr == QStringLiteral("bandstop") || pStr == QStringLiteral("bandblock")
                   || pStr == QStringLiteral("band_stop") || pStr == QStringLiteral("band_block")) {
            pass = AdvancedSignalProc::BandStop;
            canonicalPass = QStringLiteral("bandstop");
        } else {
            throwInvalidParams(
                    QStringLiteral("Unsupported IIR pass_type '%1' (expected 'lowpass', 'highpass', 'bandpass', or 'bandstop').")
                            .arg(pStr));
        }

        const int order = std::clamp(params.value(QStringLiteral("order")).toInt(4), 1, 10);
        const double fLow = params.contains(QStringLiteral("cutoff"))
                ? params.value(QStringLiteral("cutoff")).toDouble(10.0)
                : (params.contains(QStringLiteral("cutoff_low"))
                           ? params.value(QStringLiteral("cutoff_low")).toDouble(10.0)
                           : params.value(QStringLiteral("low_freq")).toDouble(10.0));
        const double fHigh = params.contains(QStringLiteral("cutoff_high"))
                ? params.value(QStringLiteral("cutoff_high")).toDouble(0.0)
                : params.value(QStringLiteral("high_freq")).toDouble(0.0);
        if (!std::isfinite(fLow) || fLow <= 0.0)
            throwInvalidParams(QStringLiteral("signal.iir_filter cutoff frequency must be > 0."));
        const bool zeroPhase = params.contains(QStringLiteral("zero_phase"))
                ? params.value(QStringLiteral("zero_phase")).toBool(true)
                : true;
        const double ripple = params.value(QStringLiteral("ripple_db")).toDouble(1.0);

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("IIR Filter"));
        QwtPlotCurve *outC = AdvancedSignalProc::iirFilter(d_app, g, curveTitle, design, pass, order,
                                                           fLow, fHigh, zeroPhase, ripple);
        enforceArialGraphFonts(g);
        g->replot();
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        outResult[QStringLiteral("graph")] = ml->name();
        outResult[QStringLiteral("pass_type")] = canonicalPass;
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    if (method == QStringLiteral("signal.stft")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 8, QStringLiteral("signal.stft"));
        const int winSz = parseBoundedInt(params, QStringLiteral("window_size"), 64, 8, 65536);
        int hopSz = 16;
        if (params.contains(QStringLiteral("hop_size"))
            && !params.value(QStringLiteral("hop_size")).isNull()) {
            hopSz = parseBoundedInt(params, QStringLiteral("hop_size"), 16, 1, winSz);
        } else if (params.contains(QStringLiteral("overlap"))
                   && !params.value(QStringLiteral("overlap")).isNull()) {
            const int ov = parseBoundedInt(params, QStringLiteral("overlap"), winSz / 2, 0, winSz - 1);
            hopSz = std::max(1, winSz - ov);
        } else {
            hopSz = std::max(1, winSz / 4);
        }
        const bool plotSpec = optionalJsonBool(params, QStringLiteral("plot"), true);
        Matrix *stftM = AdvancedSignalProc::computeSTFT(d_app, g, curveTitle, winSz, hopSz, plotSpec);
        if (!stftM)
            throwRpcRuntime(QStringLiteral("STFT computation failed."));
        outResult = serializeMatrixInfo(stftM);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("signal.envelope")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("signal.envelope"));
        const QString mode = params.value(QStringLiteral("method")).toString().trimmed().toLower();
        if (mode == QStringLiteral("peak")) {
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Signal Envelope"));
            QwtPlotCurve *lo = nullptr;
            QwtPlotCurve *hi = AdvancedSignalProc::computePeakEnvelope(
                    d_app, g, curveTitle, AdvancedSignalProc::EnvelopeBoth,
                    std::max(1, params.value(QStringLiteral("min_dist")).toInt(3)), &lo);
            enforceArialGraphFonts(g);
            g->replot();
            if (hi || lo)
                undoGuard.commit();
            outResult[QStringLiteral("ok")] = (hi != nullptr);
            if (hi)
                outResult[QStringLiteral("upper_curve")] = hi->title().text();
            if (lo)
                outResult[QStringLiteral("lower_curve")] = lo->title().text();
            return true;
        }
        Table *envTbl = nullptr;
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Signal Envelope"));
        QwtPlotCurve *envC = AdvancedSignalProc::hilbertEnvelope(
                d_app, g, curveTitle, params.value(QStringLiteral("plot_lower")).toBool(false),
                &envTbl);
        enforceArialGraphFonts(g);
        g->replot();
        if (envC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (envC != nullptr);
        if (envC)
            outResult[QStringLiteral("output_curve")] = envC->title().text();
        if (envTbl)
            outResult[QStringLiteral("table")] = envTbl->name();
        return true;
    }

    if (method == QStringLiteral("signal.wavelet_cwt")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("signal.wavelet_cwt"));
        const int nScales = parseBoundedInt(params, QStringLiteral("scales"), 32, 2, 1024);
        const QString wt =
                optionalJsonString(params, QStringLiteral("wavelet"), QStringLiteral("morlet"))
                        .trimmed()
                        .toLower();
        if (!wt.isEmpty() && wt != QStringLiteral("morlet") && wt != QStringLiteral("ricker")
            && wt != QStringLiteral("mexican_hat")) {
            throwInvalidParams(
                    QStringLiteral("Unknown wavelet '%1' (expected 'morlet' or 'ricker').")
                            .arg(wt));
        }
        auto wtype = (wt == QStringLiteral("ricker") || wt == QStringLiteral("mexican_hat"))
                ? AdvancedSignalProc::RickerMexicanHat
                : AdvancedSignalProc::Morlet;
        Matrix *cwtM = AdvancedSignalProc::waveletCWT(
                d_app, g, curveTitle, nScales, wtype,
                optionalJsonBool(params, QStringLiteral("plot"), true));
        if (!cwtM)
            throwRpcRuntime(QStringLiteral("CWT scalogram computation failed."));
        outResult = serializeMatrixInfo(cwtM);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("signal.wavelet_denoise")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("signal.wavelet_denoise"));
        const QString wt = optionalJsonString(params, QStringLiteral("wavelet")).toLower();
        auto wtype = (wt == QStringLiteral("haar")) ? AdvancedSignalProc::Haar
                                                    : AdvancedSignalProc::Daubechies4;
        const int levels = parseBoundedInt(params, QStringLiteral("levels"), 4, 1, 16);
        const bool soft = params.contains(QStringLiteral("soft"))
                ? optionalJsonBool(params, QStringLiteral("soft"), true)
                : true;
        const double scale =
                optionalJsonFiniteDouble(params, QStringLiteral("threshold_scale"), 1.0, 1e6);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Wavelet Denoise"));
        QwtPlotCurve *outC = AdvancedSignalProc::waveletDenoiseDWT(d_app, g, curveTitle, wtype,
                                                                   levels, soft, scale);
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    if (method == QStringLiteral("signal.welch_psd")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 8, QStringLiteral("signal.welch_psd"));
        const int segLen = params.contains(QStringLiteral("segment_len"))
                ? parseBoundedInt(params, QStringLiteral("segment_len"), 128, 8, 65536)
                : parseBoundedInt(params, QStringLiteral("nperseg"), 128, 8, 65536);
        Table *psdTbl = AdvancedSignalProc::welchPSD(
                d_app, g, curveTitle, segLen, optionalJsonBool(params, QStringLiteral("plot"), true));
        if (!psdTbl)
            throwRpcRuntime(QStringLiteral("Welch PSD computation failed."));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = psdTbl->name();
        return true;
    }

    if (method == QStringLiteral("signal.coherence")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString cA, cB;
        const bool hasTableParams = params.contains(QStringLiteral("table"))
                && !params.contains(QStringLiteral("graph"));
        if (hasTableParams) {
            Table *t = resolveTable(d_app, params, QStringLiteral("table"), true);
            int xIdx = params.contains(QStringLiteral("x_col"))
                    ? resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true)
                    : t->firstXCol();
            if (xIdx < 0 && t->numCols() >= 3) {
                xIdx = 0;
                t->setColPlotDesignation(0, SciDAVis::X);
            } else if (xIdx >= 0) {
                t->setColPlotDesignation(xIdx, SciDAVis::X);
            }
            const QJsonValue y1V = params.contains(QStringLiteral("y1_col"))
                    ? params.value(QStringLiteral("y1_col"))
                    : (params.contains(QStringLiteral("col_a"))
                               ? params.value(QStringLiteral("col_a"))
                               : QJsonValue(xIdx == 0 ? 1 : 0));
            const QJsonValue y2V = params.contains(QStringLiteral("y2_col"))
                    ? params.value(QStringLiteral("y2_col"))
                    : (params.contains(QStringLiteral("col_b"))
                               ? params.value(QStringLiteral("col_b"))
                               : QJsonValue(xIdx <= 1 ? 2 : 1));
            const int y1Idx = resolveColumnIndex(t, y1V, true);
            const int y2Idx = resolveColumnIndex(t, y2V, true);
            t->setColPlotDesignation(y1Idx, SciDAVis::Y);
            t->setColPlotDesignation(y2Idx, SciDAVis::Y);
            QStringList yCols;
            yCols << fullColumnName(t, y1Idx) << fullColumnName(t, y2Idx);
            ml = d_app->multilayerPlot(t, yCols, Graph::Line, 0, t->numRows() - 1);
            if (!ml || !ml->activeGraph() || ml->activeGraph()->curves() < 2
                || !ml->activeGraph()->curve(0) || !ml->activeGraph()->curve(1)) {
                closeAndDeleteEmptyPlot(d_app, ml);
                throwInvalidParams(
                        QStringLiteral("signal.coherence requires two valid numeric signal columns."));
            }
            g = ml->activeGraph();
            enforceArialGraphFonts(g);
            cA = g->curve(0)->title().text();
            cB = g->curve(1)->title().text();
        } else {
            ml = resolveMultiLayer(d_app, params);
            g = resolveGraphLayer(ml, params);
            const QString keyA = params.contains(QStringLiteral("curve_a"))
                    ? QStringLiteral("curve_a")
                    : QStringLiteral("curve1");
            const QString keyB = params.contains(QStringLiteral("curve_b"))
                    ? QStringLiteral("curve_b")
                    : QStringLiteral("curve2");
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
        }
        requireCurveMinPoints(g, cA, 8, QStringLiteral("signal.coherence"));
        requireCurveMinPoints(g, cB, 8, QStringLiteral("signal.coherence"));
        const int segLen = params.contains(QStringLiteral("segment_len"))
                ? parseBoundedInt(params, QStringLiteral("segment_len"), 128, 8, 65536)
                : parseBoundedInt(params, QStringLiteral("nperseg"), 128, 8, 65536);
        Table *cohTbl = AdvancedSignalProc::crossCoherence(
                d_app, g, cA, cB, segLen, optionalJsonBool(params, QStringLiteral("plot"), true));
        if (!cohTbl)
            throwRpcRuntime(QStringLiteral("Cross-coherence computation failed."));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = cohTbl->name();
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("signal.decimate")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        const int nPts = requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("signal.decimate"));
        int factor = 4;
        if (params.contains(QStringLiteral("factor")) && !params.value(QStringLiteral("factor")).isNull()) {
            factor = parseStrictJsonInt(params.value(QStringLiteral("factor")), QStringLiteral("factor"));
            if (factor < 2 || factor >= nPts)
                throwInvalidParams(
                        QStringLiteral("signal.decimate 'factor' (%1) must satisfy 2 <= factor < %2.")
                                .arg(factor)
                                .arg(nPts));
        }
        const bool aa = params.contains(QStringLiteral("anti_alias"))
                ? params.value(QStringLiteral("anti_alias")).toBool(true)
                : true;
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Decimate Curve"));
        QwtPlotCurve *outC = AdvancedSignalProc::decimateCurve(d_app, g, curveTitle, factor, aa);
        enforceArialGraphFonts(g);
        g->replot();
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    if (method == QStringLiteral("signal.fsd")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 4, QStringLiteral("signal.fsd"));
        const double gamma = params.contains(QStringLiteral("fwhm"))
                ? std::max(1e-6, optionalJsonFiniteDouble(params, QStringLiteral("fwhm"), 5.0, 1e6))
                : std::max(1e-6, optionalJsonFiniteDouble(params, QStringLiteral("gamma"), 5.0, 1e6));
        double kSharp = 1.8;
        if (params.contains(QStringLiteral("k")) && !params.value(QStringLiteral("k")).isNull()) {
            kSharp = std::max(1.01, optionalJsonFiniteDouble(params, QStringLiteral("k"), 1.8, 100.0));
        } else if (params.contains(QStringLiteral("smoothing"))
                   && !params.value(QStringLiteral("smoothing")).isNull()) {
            const double smVal =
                    optionalJsonFiniteDouble(params, QStringLiteral("smoothing"), 0.2, 100.0);
            kSharp = (smVal > 0.0 && smVal < 1.0) ? std::clamp(1.0 / smVal, 1.05, 8.0)
                                                  : std::max(1.01, smVal);
        }
        const QString apodStr =
                optionalJsonString(params, QStringLiteral("apodization"), QStringLiteral("bessel"))
                        .trimmed()
                        .toLower();
        const int apod = (apodStr == QStringLiteral("gaussian")) ? 1 : 0;
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Fourier Self-Deconvolution"));
        QwtPlotCurve *outC = AdvancedSignalProc::fourierSelfDeconvolve(d_app, g, curveTitle, gamma,
                                                                       kSharp, apod);
        enforceArialGraphFonts(g);
        replotGraphOrDefer(g);
        if (outC)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    return false;
}

} // namespace rpc_internal
