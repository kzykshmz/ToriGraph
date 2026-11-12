/***************************************************************************
    File                 : PythonAutomationServer.cpp
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
 ***************************************************************************/
#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "PythonAutomationServer.h"

#ifdef _MSC_VER
#pragma warning(disable : 4267 4373)
#endif

#include "AdvancedSignalProc.h"
#include "ApplicationWindow.h"
#include "ArrowMarker.h"
#include "AutoUpdater.h"
#include "BoxCurve.h"
#include "CurveMathTools.h"
#include "Differentiation.h"
#include "FFT.h"
#include "FFTFilter.h"
#include "Folder.h"
#include "FunctionCurve.h"
#include "Graph.h"
#include "Graph3D.h"
#include "GraphDigitizer.h"
#include "GraphStatsAndSurfaceFit.h"
#include "Grid.h"
#include "Integration.h"
#include "Interpolation.h"
#include "Legend.h"
#include "Matrix.h"
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
#include "table/future_Table.h"

#ifdef Q_OS_WIN
#include "EmfPaintDevice.h"
#include "OleServer.h"
#endif

#ifdef ORIGIN_IMPORT
#include "importOPJ.h"
#endif

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_scale_div.h>
#include <qwt_scale_engine.h>
#include <qwt_symbol.h>

#include <gsl/gsl_cdf.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_statistics.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMdiSubWindow>
#include <QMetaMethod>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QTextCursor>
#include <QTextDecoder>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {

// Custom domain exception carrying JSON-RPC error code
struct RpcDispatchException : public std::runtime_error
{
    int code;
    RpcDispatchException(int c, const QString &msg)
        : std::runtime_error(msg.toUtf8().constData()), code(c)
    {
    }
};

[[noreturn]] void throwInvalidParams(const QString &msg)
{
    throw RpcDispatchException(-32602, msg);
}

[[noreturn]] void throwRpcRuntime(const QString &msg)
{
    throw RpcDispatchException(-32000, msg);
}

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
};

QJsonValue safeJsonDouble(double v)
{
    if (!std::isfinite(v))
        return QJsonValue::Null;
    return QJsonValue(v);
}

QJsonArray toJsonArray(const std::vector<double> &vec)
{
    QJsonArray arr;
    for (double v : vec)
        arr.append(safeJsonDouble(v));
    return arr;
}

QJsonArray toJsonArray(const QVector<double> &vec)
{
    QJsonArray arr;
    for (double v : vec)
        arr.append(safeJsonDouble(v));
    return arr;
}

QJsonArray toJsonArray(const QStringList &lst)
{
    QJsonArray arr;
    for (const QString &s : lst)
        arr.append(s);
    return arr;
}

double jsonToDoubleOrNaN(const QJsonValue &val)
{
    if (val.isNull() || val.isUndefined())
        return std::numeric_limits<double>::quiet_NaN();
    if (val.isDouble())
        return val.toDouble();
    if (val.isString()) {
        const QString s = val.toString().trimmed();
        if (s.isEmpty() || s.compare(QStringLiteral("nan"), Qt::CaseInsensitive) == 0
            || s.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0)
            return std::numeric_limits<double>::quiet_NaN();
        bool ok = false;
        double d = s.toDouble(&ok);
        return ok ? d : std::numeric_limits<double>::quiet_NaN();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

int parseStrictJsonInt(const QJsonValue &val, const QString &paramName)
{
    if (!val.isDouble() || val.isBool()) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' must be an integer.").arg(paramName));
    }
    const double d = val.toDouble();
    if (!std::isfinite(d) || std::floor(d) != d
        || d < double(std::numeric_limits<int>::min())
        || d > double(std::numeric_limits<int>::max())) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' must be a finite integer.").arg(paramName));
    }
    return static_cast<int>(d);
}

int parseStrictPositiveInt(const QJsonObject &params, const QString &key, int defaultVal)
{
    if (!params.contains(key) || params.value(key).isUndefined() || params.value(key).isNull())
        return defaultVal;
    const int v = parseStrictJsonInt(params.value(key), key);
    if (v <= 0) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' must be a positive integer (>= 1), got %2.")
                        .arg(key)
                        .arg(v));
    }
    return v;
}

int countValidCurvePoints(QwtPlotCurve *c, int *distinctXOut = nullptr)
{
    if (distinctXOut)
        *distinctXOut = 0;
    if (!c)
        return 0;
    const auto *dc = dynamic_cast<const DataCurve *>(c);
    const int n = static_cast<int>(c->dataSize());
    int valid = 0;
    std::vector<double> xs;
    if (distinctXOut && n > 0)
        xs.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (dc && dc->isPointMasked(i))
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
    xs.reserve(static_cast<size_t>(n));
    ys.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (dc && dc->isPointMasked(i))
            continue;
        const double xv = c->x(i);
        const double yv = c->y(i);
        if (std::isfinite(xv) && std::isfinite(yv)) {
            xs.push_back(xv);
            ys.push_back(yv);
        }
    }
}

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

bool checkMuParserExprValid(const QString &expr, const QStringList &varNames,
                            QString *errMsgOut = nullptr)
{
    const QString trimmed = expr.trimmed();
    if (trimmed.isEmpty()) {
        if (errMsgOut)
            *errMsgOut = QStringLiteral("Formula expression is empty.");
        return false;
    }
    static const QRegularExpression identRx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
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
    try {
        MyParser parser;
        std::vector<double> vals(static_cast<size_t>(uniqueVars.size()), 0.5);
        for (int i = 0; i < uniqueVars.size(); ++i) {
            vals[static_cast<size_t>(i)] = 0.5 + 0.13 * static_cast<double>(i);
            parser.DefineVar(toString<mu::string_type>(uniqueVars[i]),
                             &vals[static_cast<size_t>(i)]);
        }
        parser.SetExpr(trimmed);
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
                          int *distinctXOut = nullptr)
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

QByteArray gzipCompressBytes(const QByteArray &raw)
{
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY)
        != Z_OK) {
        throwRpcRuntime(QStringLiteral("zlib deflateInit2 failed while compressing project."));
    }
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(raw.constData()));
    zs.avail_in = static_cast<uInt>(raw.size());

    QByteArray out;
    out.reserve(std::max(128, raw.size() / 2));
    char chunk[32768];
    int ret = Z_OK;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(chunk);
        zs.avail_out = sizeof(chunk);
        ret = deflate(&zs, zs.avail_in > 0 ? Z_NO_FLUSH : Z_FINISH);
        if (ret == Z_STREAM_ERROR || ret == Z_MEM_ERROR) {
            deflateEnd(&zs);
            throwRpcRuntime(QStringLiteral("zlib deflate failed while compressing project."));
        }
        const int have = static_cast<int>(sizeof(chunk) - zs.avail_out);
        if (have > 0)
            out.append(chunk, have);
    } while (ret != Z_STREAM_END);

    deflateEnd(&zs);
    return out;
}

QColor parseColorValue(const QJsonValue &val, const QColor &fallback = QColor(Qt::black))
{
    if (val.isString()) {
        QColor c(val.toString().trimmed());
        if (c.isValid())
            return c;
    } else if (val.isArray()) {
        QJsonArray a = val.toArray();
        if (a.size() >= 3) {
            int r = std::clamp(a[0].toInt(0), 0, 255);
            int g = std::clamp(a[1].toInt(0), 0, 255);
            int b = std::clamp(a[2].toInt(0), 0, 255);
            int al = (a.size() >= 4) ? std::clamp(a[3].toInt(255), 0, 255) : 255;
            return QColor(r, g, b, al);
        }
    }
    return fallback;
}

Qt::PenStyle parsePenStyleValue(const QJsonValue &val, Qt::PenStyle fallback = Qt::SolidLine)
{
    if (val.isDouble()) {
        int s = val.toInt(int(fallback));
        if (s >= int(Qt::NoPen) && s <= int(Qt::DashDotDotLine))
            return static_cast<Qt::PenStyle>(s);
    }
    if (val.isString()) {
        const QString s = val.toString().trimmed().toLower();
        if (s == QStringLiteral("none") || s == QStringLiteral("nopen"))
            return Qt::NoPen;
        if (s == QStringLiteral("solid"))
            return Qt::SolidLine;
        if (s == QStringLiteral("dash") || s == QStringLiteral("dashed"))
            return Qt::DashLine;
        if (s == QStringLiteral("dot") || s == QStringLiteral("dotted"))
            return Qt::DotLine;
        if (s == QStringLiteral("dashdot") || s == QStringLiteral("dash_dot"))
            return Qt::DashDotLine;
        if (s == QStringLiteral("dashdotdot") || s == QStringLiteral("dash_dot_dot"))
            return Qt::DashDotDotLine;
    }
    return fallback;
}

QwtSymbol::Style parseQwtSymbolStyle(const QJsonValue &val,
                                     QwtSymbol::Style fallback = QwtSymbol::Ellipse)
{
    if (val.isDouble())
        return static_cast<QwtSymbol::Style>(val.toInt(int(fallback)));
    if (val.isString()) {
        const QString s = val.toString().trimmed().toLower();
        if (s == QStringLiteral("none") || s == QStringLiteral("nosymbol"))
            return QwtSymbol::NoSymbol;
        if (s == QStringLiteral("circle") || s == QStringLiteral("ellipse") || s == QStringLiteral("o"))
            return QwtSymbol::Ellipse;
        if (s == QStringLiteral("square") || s == QStringLiteral("rect") || s == QStringLiteral("box"))
            return QwtSymbol::Rect;
        if (s == QStringLiteral("diamond"))
            return QwtSymbol::Diamond;
        if (s == QStringLiteral("triangle") || s == QStringLiteral("utriangle")
            || s == QStringLiteral("up_triangle"))
            return QwtSymbol::UTriangle;
        if (s == QStringLiteral("dtriangle") || s == QStringLiteral("down_triangle"))
            return QwtSymbol::DTriangle;
        if (s == QStringLiteral("ltriangle") || s == QStringLiteral("left_triangle"))
            return QwtSymbol::LTriangle;
        if (s == QStringLiteral("rtriangle") || s == QStringLiteral("right_triangle"))
            return QwtSymbol::RTriangle;
        if (s == QStringLiteral("cross") || s == QStringLiteral("+"))
            return QwtSymbol::Cross;
        if (s == QStringLiteral("xcross") || s == QStringLiteral("x"))
            return QwtSymbol::XCross;
        if (s == QStringLiteral("hline"))
            return QwtSymbol::HLine;
        if (s == QStringLiteral("vline"))
            return QwtSymbol::VLine;
        if (s == QStringLiteral("star") || s == QStringLiteral("star1"))
            return QwtSymbol::Star1;
        if (s == QStringLiteral("star2"))
            return QwtSymbol::Star2;
        if (s == QStringLiteral("hexagon"))
            return QwtSymbol::Hexagon;
    }
    return fallback;
}

QString windowTypeString(MyWidget *w)
{
    if (!w)
        return QStringLiteral("unknown");
    if (w->inherits("Table"))
        return QStringLiteral("Table");
    if (w->inherits("Matrix"))
        return QStringLiteral("Matrix");
    if (w->inherits("MultiLayer"))
        return QStringLiteral("MultiLayer");
    if (w->inherits("Graph3D"))
        return QStringLiteral("Graph3D");
    if (w->inherits("Note"))
        return QStringLiteral("Note");
    return QString::fromLatin1(w->metaObject()->className());
}

MyWidget *findWindowByName(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    const QList<MyWidget *> windows = app->windowsList();
    if (name.trimmed().isEmpty()) {
        if (QWidget *act = app->d_workspace.activeSubWindow()) {
            if (auto *mw = qobject_cast<MyWidget *>(act))
                return mw;
        }
        return windows.isEmpty() ? nullptr : windows.last();
    }
    for (MyWidget *w : windows) {
        if (w && w->name() == name)
            return w;
    }
    for (MyWidget *w : windows) {
        if (w && w->name().compare(name, Qt::CaseInsensitive) == 0)
            return w;
    }
    return nullptr;
}

Table *resolveTable(ApplicationWindow *app, const QJsonObject &params,
                    const QString &key = QStringLiteral("table"), bool required = true)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = params.value(key).toString().trimmed();
    if (name.isEmpty() && key != QStringLiteral("name"))
        name = params.value(QStringLiteral("window")).toString().trimmed();
    if (!name.isEmpty()) {
        if (Table *t = app->table(name))
            return t;
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("Table"))
                return static_cast<Table *>(mw);
        }
        if (required)
            throwInvalidParams(QStringLiteral("Table not found: %1").arg(name));
        return nullptr;
    }
    if (QWidget *act = app->d_workspace.activeSubWindow()) {
        if (act->inherits("Table"))
            return static_cast<Table *>(act);
    }
    const QList<MyWidget *> windows = app->windowsList();
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows[i] && windows[i]->inherits("Table"))
            return static_cast<Table *>(windows[i]);
    }
    if (required)
        throwInvalidParams(QStringLiteral("No Table window available."));
    return nullptr;
}

Matrix *resolveMatrix(ApplicationWindow *app, const QJsonObject &params,
                      const QString &key = QStringLiteral("matrix"), bool required = true)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = params.value(key).toString().trimmed();
    if (name.isEmpty() && key != QStringLiteral("name"))
        name = params.value(QStringLiteral("window")).toString().trimmed();
    if (!name.isEmpty()) {
        if (Matrix *m = app->matrix(name))
            return m;
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("Matrix"))
                return static_cast<Matrix *>(mw);
        }
        if (required)
            throwInvalidParams(QStringLiteral("Matrix not found: %1").arg(name));
        return nullptr;
    }
    if (QWidget *act = app->d_workspace.activeSubWindow()) {
        if (act->inherits("Matrix"))
            return static_cast<Matrix *>(act);
    }
    const QList<MyWidget *> windows = app->windowsList();
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows[i] && windows[i]->inherits("Matrix"))
            return static_cast<Matrix *>(windows[i]);
    }
    if (required)
        throwInvalidParams(QStringLiteral("No Matrix window available."));
    return nullptr;
}

MultiLayer *resolveMultiLayer(ApplicationWindow *app, const QJsonObject &params,
                              const QString &key = QStringLiteral("graph"), bool required = true)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = params.value(key).toString().trimmed();
    if (name.isEmpty())
        name = params.value(QStringLiteral("graph_name")).toString().trimmed();
    if (name.isEmpty() && key != QStringLiteral("name"))
        name = params.value(QStringLiteral("window")).toString().trimmed();
    if (!name.isEmpty()) {
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("MultiLayer"))
                return static_cast<MultiLayer *>(mw);
        }
        if (required)
            throwInvalidParams(QStringLiteral("MultiLayer graph not found: %1").arg(name));
        return nullptr;
    }
    if (QWidget *act = app->d_workspace.activeSubWindow()) {
        if (act->inherits("MultiLayer"))
            return static_cast<MultiLayer *>(act);
    }
    const QList<MyWidget *> windows = app->windowsList();
    for (int i = windows.size() - 1; i >= 0; --i) {
        if (windows[i] && windows[i]->inherits("MultiLayer"))
            return static_cast<MultiLayer *>(windows[i]);
    }
    if (required)
        throwInvalidParams(QStringLiteral("No MultiLayer graph window available."));
    return nullptr;
}

Graph3D *resolveGraph3D(ApplicationWindow *app, const QJsonObject &params,
                        const QString &key = QStringLiteral("graph"), bool required = true)
{
    if (!app)
        throwRpcRuntime(QStringLiteral("ApplicationWindow is null."));
    QString name = params.value(key).toString().trimmed();
    if (name.isEmpty())
        name = params.value(QStringLiteral("window")).toString().trimmed();
    if (!name.isEmpty()) {
        if (MyWidget *mw = findWindowByName(app, name)) {
            if (mw->inherits("Graph3D"))
                return static_cast<Graph3D *>(mw);
        }
        if (required)
            throwInvalidParams(QStringLiteral("Graph3D window not found: %1").arg(name));
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
    if (required)
        throwInvalidParams(QStringLiteral("No Graph3D window available."));
    return nullptr;
}

Graph *resolveGraphLayer(MultiLayer *ml, const QJsonObject &params, bool required = true)
{
    if (!ml) {
        if (required)
            throwInvalidParams(QStringLiteral("MultiLayer graph is null."));
        return nullptr;
    }
    const QList<Graph *> layers = ml->layersList();
    if (params.contains(QStringLiteral("layer")) && !params.value(QStringLiteral("layer")).isNull()) {
        int idx = params.value(QStringLiteral("layer")).toInt(-1);
        // Support 0-based index (0..N-1); if idx == layers.size() and 1-based was passed, clamp safely
        if (idx >= 0 && idx < layers.size())
            return layers[idx];
        if (idx >= 1 && idx <= layers.size())
            return layers[idx - 1];
        if (required)
            throwInvalidParams(QStringLiteral("Layer index out of range: %1").arg(idx));
        return nullptr;
    }
    if (Graph *act = ml->activeGraph())
        return act;
    if (!layers.isEmpty())
        return layers.first();
    if (required)
        throwInvalidParams(QStringLiteral("Graph window '%1' has no layers.").arg(ml->name()));
    return nullptr;
}

int resolveColumnIndex(Table *t, const QJsonValue &colSpec, bool required = true)
{
    if (!t) {
        if (required)
            throwInvalidParams(QStringLiteral("Table is null."));
        return -1;
    }
    if (colSpec.isDouble()) {
        int idx = colSpec.toInt(-1);
        if (idx >= 0 && idx < t->numCols())
            return idx;
        if (required)
            throwInvalidParams(QStringLiteral("Column index out of range: %1").arg(idx));
        return -1;
    }
    if (colSpec.isString()) {
        const QString raw = colSpec.toString().trimmed();
        if (raw.isEmpty()) {
            if (required)
                throwInvalidParams(QStringLiteral("Column name is empty."));
            return -1;
        }
        // 1. Exact case-sensitive short column label match first (handles numeric labels like "0", "100")
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colLabel(c) == raw)
                return c;
        }
        // 2. Exact case-sensitive full column name match ("Table_Col")
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colName(c) == raw)
                return c;
        }
        // 3. Case-insensitive short column label match
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colLabel(c).compare(raw, Qt::CaseInsensitive) == 0)
                return c;
        }
        // 4. Case-insensitive full name or long name match
        for (int c = 0; c < t->numCols(); ++c) {
            if (t->colName(c).compare(raw, Qt::CaseInsensitive) == 0
                || t->colLongName(c).compare(raw, Qt::CaseInsensitive) == 0)
                return c;
        }
        int idx = t->colIndex(raw);
        if (idx >= 0 && idx < t->numCols())
            return idx;
        const QString full = QString(t->name()) + QStringLiteral("_") + raw;
        idx = t->colIndex(full);
        if (idx >= 0 && idx < t->numCols())
            return idx;
        bool okInt = false;
        int asInt = raw.toInt(&okInt);
        if (okInt && asInt >= 0 && asInt < t->numCols())
            return asInt;
        if (required)
            throwInvalidParams(
                    QStringLiteral("Column '%1' not found in table '%2'.").arg(raw, t->name()));
        return -1;
    }
    if (required)
        throwInvalidParams(QStringLiteral("Column specification must be an index or column name."));
    return -1;
}

QString fullColumnName(Table *t, int colIdx)
{
    if (!t || colIdx < 0 || colIdx >= t->numCols())
        return QString();
    return t->colName(colIdx);
}

void enforceArialGraphFonts(Graph *g)
{
    if (!g)
        return;
    for (int ax = 0; ax < QwtPlot::axisCnt; ++ax) {
        QFont nf = g->axisFont(ax);
        nf.setFamily(QStringLiteral("Arial"));
        g->setAxisFont(ax, nf);

        QFont tf = g->axisTitleFont(ax);
        tf.setFamily(QStringLiteral("Arial"));
        g->setAxisTitleFont(ax, tf);
    }
    if (Legend *leg = g->legend()) {
        QFont lf = leg->font();
        lf.setFamily(QStringLiteral("Arial"));
        leg->setFont(lf);
    }
    if (g->plotWidget()) {
        QwtText t = g->plotWidget()->title();
        QFont f = t.font();
        f.setFamily(QStringLiteral("Arial"));
        t.setFont(f);
        g->plotWidget()->setTitle(t);

        const QwtPlotItemList &items = g->plotWidget()->itemList();
        for (QwtPlotItem *it : items) {
            if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                if (auto *spec = dynamic_cast<Spectrogram *>(it))
                    spec->enforceColorBarArialFont();
            }
        }
    }
}

void enforceArialGraph3DFonts(Graph3D *g3d)
{
    if (!g3d)
        return;
    QFont xf = g3d->xAxisLabelFont();
    xf.setFamily(QStringLiteral("Arial"));
    g3d->setXAxisLabelFont(xf);

    QFont yf = g3d->yAxisLabelFont();
    yf.setFamily(QStringLiteral("Arial"));
    g3d->setYAxisLabelFont(yf);

    QFont zf = g3d->zAxisLabelFont();
    zf.setFamily(QStringLiteral("Arial"));
    g3d->setZAxisLabelFont(zf);

    QFont nf = g3d->numbersFont();
    nf.setFamily(QStringLiteral("Arial"));
    g3d->setNumbersFont(nf);

    QFont tf = g3d->titleFont();
    tf.setFamily(QStringLiteral("Arial"));
    g3d->setTitleFont(tf);
}

QString resolveCurveTitle(Graph *g, const QJsonObject &params,
                          const QString &key = QStringLiteral("curve"), bool required = true)
{
    if (!g) {
        if (required)
            throwInvalidParams(QStringLiteral("Graph layer is null."));
        return QString();
    }
    const QStringList allCurves = g->curvesList();
    const QStringList dataCurves = g->analysableCurvesList();
    if (params.contains(key) && !params.value(key).isNull()) {
        const QJsonValue v = params.value(key);
        if (v.isDouble()) {
            int idx = v.toInt(-1);
            if (idx >= 0 && idx < allCurves.size())
                return allCurves[idx];
            if (required)
                throwInvalidParams(QStringLiteral("Curve index out of range: %1").arg(idx));
            return QString();
        }
        const QString want = v.toString().trimmed();
        if (!want.isEmpty()) {
            if (g->curveIndex(want) >= 0)
                return want;
            for (const QString &c : allCurves) {
                if (c.compare(want, Qt::CaseInsensitive) == 0)
                    return c;
            }
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (auto *dc = dynamic_cast<DataCurve *>(g->curve(ci))) {
                    if (dc->table()
                        && dc->yColumnName()
                                == dc->table()->name() + QStringLiteral("_") + want) {
                        return dc->title().text();
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
                        return dc->title().text();
                    }
                }
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
                        return ct;
                    }
                }
            }
            if (required)
                throwInvalidParams(QStringLiteral("Curve '%1' not found on graph.").arg(want));
            return QString();
        }
    }
    if (params.contains(QStringLiteral("curve_index"))
        && !params.value(QStringLiteral("curve_index")).isNull()) {
        int idx = params.value(QStringLiteral("curve_index")).toInt(-1);
        if (idx >= 0 && idx < allCurves.size())
            return allCurves[idx];
        if (required)
            throwInvalidParams(QStringLiteral("curve_index out of range: %1").arg(idx));
        return QString();
    }
    if (!dataCurves.isEmpty())
        return dataCurves.first();
    if (!allCurves.isEmpty())
        return allCurves.first();
    if (required)
        throwInvalidParams(QStringLiteral("Graph layer has no curves."));
    return QString();
}

int parsePlotStyle(const QString &styleStr, int defaultStyle = Graph::LineSymbols)
{
    const QString s = styleStr.trimmed().toLower();
    if (s.isEmpty())
        return defaultStyle;
    if (s == QStringLiteral("line") || s == QStringLiteral("lines"))
        return Graph::Line;
    if (s == QStringLiteral("scatter") || s == QStringLiteral("symbols") || s == QStringLiteral("points"))
        return Graph::Scatter;
    if (s == QStringLiteral("line_symbol") || s == QStringLiteral("linesymbols")
        || s == QStringLiteral("line_symbols"))
        return Graph::LineSymbols;
    if (s == QStringLiteral("spline"))
        return Graph::Spline;
    if (s == QStringLiteral("vertical_bars") || s == QStringLiteral("vbars")
        || s == QStringLiteral("bars") || s == QStringLiteral("column"))
        return Graph::VerticalBars;
    if (s == QStringLiteral("horizontal_bars") || s == QStringLiteral("hbars")
        || s == QStringLiteral("bar"))
        return Graph::HorizontalBars;
    if (s == QStringLiteral("area"))
        return Graph::Area;
    if (s == QStringLiteral("vertical_steps") || s == QStringLiteral("vsteps")
        || s == QStringLiteral("steps"))
        return Graph::VerticalSteps;
    if (s == QStringLiteral("horizontal_steps") || s == QStringLiteral("hsteps"))
        return Graph::HorizontalSteps;
    if (s == QStringLiteral("drop_lines") || s == QStringLiteral("droplines")
        || s == QStringLiteral("stem"))
        return Graph::VerticalDropLines;
    if (s == QStringLiteral("histogram"))
        return Graph::Histogram;
    if (s == QStringLiteral("box") || s == QStringLiteral("boxplot"))
        return Graph::Box;
    if (s == QStringLiteral("pie"))
        return Graph::Pie;
    return defaultStyle;
}

QString sanitizeWindowNameForSciDAVis(const QString &raw,
                                      const QString &fallbackPrefix = QStringLiteral("Win"),
                                      bool preserveUnderscores = false)
{
    QString s = raw.trimmed();
    if (!preserveUnderscores) {
        s.replace(QLatin1Char('-'), QLatin1Char('_'));
        s.replace(QRegExp(QStringLiteral("\\W+")), QStringLiteral("_"));
        while (s.startsWith(QLatin1Char('_')))
            s.remove(0, 1);
        while (s.endsWith(QLatin1Char('_')))
            s.chop(1);
        if (s.isEmpty())
            s = fallbackPrefix;
        s.replace(QLatin1Char('_'), QLatin1Char('-'));
        return s;
    }

    QString out;
    out.reserve(s.size());
    bool prevSep = false;
    for (int i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        const bool isAsciiWordOrDash = (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
                || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
                || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                || ch == QLatin1Char('_') || ch == QLatin1Char('-');
        if (isAsciiWordOrDash) {
            out.append(ch);
            prevSep = false;
        } else if (!prevSep && !out.isEmpty()) {
            out.append(QLatin1Char('_'));
            prevSep = true;
        }
    }
    while (out.endsWith(QLatin1Char('_')) || out.endsWith(QLatin1Char('-')))
        out.chop(1);
    while (out.startsWith(QLatin1Char('_')) || out.startsWith(QLatin1Char('-')))
        out.remove(0, 1);
    if (out.isEmpty())
        out = fallbackPrefix;
    return out;
}

QString makeUniqueValidWindowName(ApplicationWindow *app, const QString &sanitizedBase,
                                  MyWidget *targetWidget = nullptr)
{
    auto hasConflict = [&](const QString &cand) -> bool {
        if (app && app->alreadyUsedName(cand))
            return true;
        if (targetWidget && targetWidget->inherits("Table")) {
            if (static_cast<Table *>(targetWidget)->colNames().contains(cand))
                return true;
        }
        return false;
    };
    if (!hasConflict(sanitizedBase))
        return sanitizedBase;
    for (int suffix = 2; suffix < 100000; ++suffix) {
        const QString cand = QStringLiteral("%1-%2").arg(sanitizedBase).arg(suffix);
        if (!hasConflict(cand))
            return cand;
    }
    return app->generateUniqueName(sanitizedBase + QStringLiteral("-"), false);
}

Table *createUniqueTableHeadless(ApplicationWindow *app, int rows, int cols, const QString &reqName)
{
    rows = std::max(1, rows);
    cols = std::max(1, cols);
    QString finalName;
    if (reqName.trimmed().isEmpty()) {
        finalName = app->generateUniqueName(QStringLiteral("Table"), true);
    } else {
        const QString base = sanitizeWindowNameForSciDAVis(reqName, QStringLiteral("Table"), true);
        finalName = makeUniqueValidWindowName(app, base, nullptr);
    }
    Table *t = app->newTable(rows, cols, finalName, QString());
    if (!t)
        throwRpcRuntime(QStringLiteral("Failed to create Table '%1'.").arg(finalName));
    if (RecalcManager *rm = RecalcManager::instance(app))
        t->setAutoRecalcFormulas(rm->recalcMode() == RecalcManager::Auto);
    return t;
}

Matrix *createUniqueMatrixHeadless(ApplicationWindow *app, int rows, int cols, const QString &reqName)
{
    rows = std::max(1, rows);
    cols = std::max(1, cols);
    QString finalName;
    if (reqName.trimmed().isEmpty()) {
        finalName = app->generateUniqueName(QStringLiteral("Matrix"), true);
    } else {
        const QString base = sanitizeWindowNameForSciDAVis(reqName, QStringLiteral("Matrix"), true);
        finalName = makeUniqueValidWindowName(app, base, nullptr);
    }
    Matrix *m = app->newMatrix(finalName, rows, cols);
    if (!m)
        throwRpcRuntime(QStringLiteral("Failed to create Matrix '%1'.").arg(finalName));
    return m;
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
        if (params.contains(QStringLiteral("x_col")) && !params.value(QStringLiteral("x_col")).isNull()) {
            xIdx = resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true);
            if (t->colPlotDesignation(xIdx) != SciDAVis::X)
                t->setColPlotDesignation(xIdx, SciDAVis::X);
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
        if (!params.value(QStringLiteral("new_graph")).toBool(false)) {
            for (MyWidget *mw : app->windowsList()) {
                if (mw && mw->inherits("MultiLayer")) {
                    auto *candMl = static_cast<MultiLayer *>(mw);
                    for (Graph *lg : candMl->layersList()) {
                        if (lg && lg->curveIndex(wantColFull) >= 0) {
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
        outMl = app->multilayerPlot(t, yCols, Graph::LineSymbols, 0, t->numRows() - 1);
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

QJsonObject extractTableAsJson(Table *t, int maxRows = 5000)
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

    auto collectAllProjectWindows = [app]() -> QList<MyWidget *> {
        QList<MyWidget *> all = app->windowsList();
        std::function<void(Folder *)> walkFolder = [&](Folder *f) {
            if (!f)
                return;
            const QList<MyWidget *> fw = f->windowsList();
            for (MyWidget *w : fw) {
                if (w && !all.contains(w))
                    all.append(w);
            }
            const QList<Folder *> subs = f->folders();
            for (Folder *sub : subs)
                walkFolder(sub);
        };
        walkFolder(app->projectFolder());
        return all;
    };

    // Close dependent graph windows first, then tables/matrices/notes
    QList<MyWidget *> pass1 = collectAllProjectWindows();
    for (MyWidget *w : pass1) {
        if (w && (w->inherits("MultiLayer") || w->inherits("Graph3D"))) {
            w->askOnCloseEvent(false);
            w->hide();
            w->close();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QList<MyWidget *> pass2 = collectAllProjectWindows();
    for (MyWidget *w : pass2) {
        if (w) {
            w->askOnCloseEvent(false);
            w->hide();
            w->close();
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // Ensure no residual windows remain in lists or subfolders
    QList<MyWidget *> rem = collectAllProjectWindows();
    for (MyWidget *w : rem) {
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

} // namespace

// ============================================================================
// PythonAutomationServer — Transports & JSON-RPC 2.0 Framing
// ============================================================================

PythonAutomationServer::PythonAutomationServer(ApplicationWindow *app, QObject *parent)
    : QObject(parent), d_app(app)
{
}

PythonAutomationServer::~PythonAutomationServer()
{
    stop();
}

bool PythonAutomationServer::startTcp(quint16 port, const QHostAddress &bindAddress)
{
    if (d_tcp_server) {
        if (d_tcp_server->isListening()
            && (port == 0 || d_tcp_server->serverPort() == port)
            && d_bind_address == bindAddress) {
            d_tcp_port = d_tcp_server->serverPort();
            d_prefer_local_pipe = false;
            return true;
        }
        for (auto it = d_tcp_buffers.begin(); it != d_tcp_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->disconnectFromHost();
                it.key()->deleteLater();
            }
        }
        d_tcp_buffers.clear();
        d_tcp_server->close();
        delete d_tcp_server;
        d_tcp_server = nullptr;
        d_tcp_port = 0;
    }

    d_tcp_server = new QTcpServer(this);
    connect(d_tcp_server, &QTcpServer::newConnection, this,
            &PythonAutomationServer::onNewTcpConnection);

    if (!d_tcp_server->listen(bindAddress, port)) {
        delete d_tcp_server;
        d_tcp_server = nullptr;
        d_tcp_port = 0;
        return false;
    }

    d_bind_address = bindAddress;
    d_tcp_port = d_tcp_server->serverPort();
    d_prefer_local_pipe = false;
    return true;
}

quint16 PythonAutomationServer::tcpPort() const
{
    return (d_tcp_server && d_tcp_server->isListening()) ? d_tcp_server->serverPort() : d_tcp_port;
}

bool PythonAutomationServer::isTcpListening() const
{
    return d_tcp_server && d_tcp_server->isListening();
}

bool PythonAutomationServer::startLocalPipe(const QString &pipeName)
{
    QString targetName = pipeName.trimmed();
    if (targetName.isEmpty()) {
        targetName = QStringLiteral("torigraph_rpc_%1")
                             .arg(QCoreApplication::applicationPid());
    }

    if (d_local_server) {
        if (d_local_server->isListening() && d_local_pipe_name == targetName) {
            d_prefer_local_pipe = true;
            return true;
        }
        for (auto it = d_local_buffers.begin(); it != d_local_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->disconnectFromServer();
                it.key()->deleteLater();
            }
        }
        d_local_buffers.clear();
        d_local_server->close();
        delete d_local_server;
        d_local_server = nullptr;
        d_local_pipe_name.clear();
    }

    QLocalServer::removeServer(targetName);
    d_local_server = new QLocalServer(this);
    d_local_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(d_local_server, &QLocalServer::newConnection, this,
            &PythonAutomationServer::onNewLocalConnection);

    if (!d_local_server->listen(targetName)) {
        delete d_local_server;
        d_local_server = nullptr;
        return false;
    }

    d_local_pipe_name = targetName;
    d_prefer_local_pipe = true;
    return true;
}

QString PythonAutomationServer::localPipeName() const
{
    return d_local_pipe_name;
}

bool PythonAutomationServer::isLocalPipeListening() const
{
    return d_local_server && d_local_server->isListening();
}

void PythonAutomationServer::stop()
{
    if (d_tcp_server) {
        for (auto it = d_tcp_buffers.begin(); it != d_tcp_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->close();
                it.key()->deleteLater();
            }
        }
        d_tcp_buffers.clear();
        d_tcp_server->close();
        delete d_tcp_server;
        d_tcp_server = nullptr;
        d_tcp_port = 0;
    }

    if (d_local_server) {
        for (auto it = d_local_buffers.begin(); it != d_local_buffers.end(); ++it) {
            if (it.key()) {
                it.key()->disconnect(this);
                it.key()->close();
                it.key()->deleteLater();
            }
        }
        d_local_buffers.clear();
        const QString savedName = d_local_pipe_name;
        d_local_server->close();
        delete d_local_server;
        d_local_server = nullptr;
        d_local_pipe_name.clear();
        if (!savedName.isEmpty())
            QLocalServer::removeServer(savedName);
    }
    d_prefer_local_pipe = false;
}

QString PythonAutomationServer::endpointString() const
{
    if (d_prefer_local_pipe && d_local_server && d_local_server->isListening()) {
        return QStringLiteral("pipe://%1").arg(d_local_pipe_name);
    }
    if (d_tcp_server && d_tcp_server->isListening()) {
        QString host = d_bind_address.toString();
        if (host.isEmpty() || d_bind_address == QHostAddress::Any
            || d_bind_address == QHostAddress::AnyIPv4) {
            host = QStringLiteral("127.0.0.1");
        }
        return QStringLiteral("tcp://%1:%2").arg(host).arg(d_tcp_server->serverPort());
    }
    if (d_local_server && d_local_server->isListening()) {
        return QStringLiteral("pipe://%1").arg(d_local_pipe_name);
    }
    return QString();
}

bool PythonAutomationServer::writeEndpointPortFile(const QString &filePath) const
{
    const QString ep = endpointString();
    if (filePath.trimmed().isEmpty() || ep.isEmpty())
        return false;
    QFileInfo fi(filePath);
    if (fi.dir().exists() == false)
        QDir().mkpath(fi.dir().absolutePath());

    QSaveFile sf(filePath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray payload = (ep + QStringLiteral("\n")).toUtf8();
    if (sf.write(payload) != payload.size())
        return false;
    return sf.commit();
}

void PythonAutomationServer::setCustomCommandHook(CustomCommandHook hook)
{
    d_custom_hook = std::move(hook);
}

void PythonAutomationServer::onNewTcpConnection()
{
    if (!d_tcp_server)
        return;
    while (QTcpSocket *sock = d_tcp_server->nextPendingConnection()) {
        d_tcp_buffers.insert(sock, QByteArray());
        connect(sock, &QTcpSocket::readyRead, this, &PythonAutomationServer::onTcpReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &PythonAutomationServer::onTcpDisconnected);
    }
}

void PythonAutomationServer::onTcpReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock || !d_tcp_buffers.contains(sock))
        return;

    QByteArray &buf = d_tcp_buffers[sock];
    buf.append(sock->readAll());

    while (true) {
        if (!d_tcp_buffers.contains(sock))
            break;
        int nlPos = buf.indexOf('\n');
        if (nlPos < 0)
            break;
        QByteArray line = buf.left(nlPos);
        buf.remove(0, nlPos + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.trimmed().isEmpty())
            continue;

        QByteArray resp = handleRpcLine(line);
        if (!resp.isEmpty() && d_tcp_buffers.contains(sock)
            && sock->state() == QAbstractSocket::ConnectedState) {
            if (!resp.endsWith('\n'))
                resp.append('\n');
            sock->write(resp);
            sock->flush();
        }
    }
}

void PythonAutomationServer::onTcpDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    d_tcp_buffers.remove(sock);
    sock->deleteLater();
}

void PythonAutomationServer::onNewLocalConnection()
{
    if (!d_local_server)
        return;
    while (QLocalSocket *sock = d_local_server->nextPendingConnection()) {
        d_local_buffers.insert(sock, QByteArray());
        connect(sock, &QLocalSocket::readyRead, this, &PythonAutomationServer::onLocalReadyRead);
        connect(sock, &QLocalSocket::disconnected, this,
                &PythonAutomationServer::onLocalDisconnected);
    }
}

void PythonAutomationServer::onLocalReadyRead()
{
    auto *sock = qobject_cast<QLocalSocket *>(sender());
    if (!sock || !d_local_buffers.contains(sock))
        return;

    QByteArray &buf = d_local_buffers[sock];
    buf.append(sock->readAll());

    while (true) {
        if (!d_local_buffers.contains(sock))
            break;
        int nlPos = buf.indexOf('\n');
        if (nlPos < 0)
            break;
        QByteArray line = buf.left(nlPos);
        buf.remove(0, nlPos + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.trimmed().isEmpty())
            continue;

        QByteArray resp = handleRpcLine(line);
        if (!resp.isEmpty() && d_local_buffers.contains(sock)
            && sock->state() == QLocalSocket::ConnectedState) {
            if (!resp.endsWith('\n'))
                resp.append('\n');
            sock->write(resp);
            sock->flush();
        }
    }
}

void PythonAutomationServer::onLocalDisconnected()
{
    auto *sock = qobject_cast<QLocalSocket *>(sender());
    if (!sock)
        return;
    d_local_buffers.remove(sock);
    sock->deleteLater();
}

QByteArray PythonAutomationServer::handleRpcLine(const QByteArray &line)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = -32700;
        errObj[QStringLiteral("message")] =
                QStringLiteral("Parse error: %1").arg(parseErr.errorString());

        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = QJsonValue::Null;
        resp[QStringLiteral("error")] = errObj;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
    }

    if (doc.isArray()) {
        const QJsonArray reqArr = doc.array();
        if (reqArr.isEmpty()) {
            QJsonObject errObj;
            errObj[QStringLiteral("code")] = -32600;
            errObj[QStringLiteral("message")] = QStringLiteral("Invalid Request: empty batch.");
            QJsonObject resp;
            resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            resp[QStringLiteral("id")] = QJsonValue::Null;
            resp[QStringLiteral("error")] = errObj;
            return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
        }
        QJsonArray respArr;
        for (const QJsonValue &item : reqArr) {
            if (!item.isObject()) {
                QJsonObject errObj;
                errObj[QStringLiteral("code")] = -32600;
                errObj[QStringLiteral("message")] = QStringLiteral("Invalid Request");
                QJsonObject r;
                r[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
                r[QStringLiteral("id")] = QJsonValue::Null;
                r[QStringLiteral("error")] = errObj;
                respArr.append(r);
            } else {
                const QJsonObject r = handleRpcRequest(item.toObject());
                if (!r.isEmpty())
                    respArr.append(r);
            }
        }
        if (respArr.isEmpty())
            return QByteArray();
        return QJsonDocument(respArr).toJson(QJsonDocument::Compact) + "\n";
    }

    if (!doc.isObject()) {
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = -32600;
        errObj[QStringLiteral("message")] = QStringLiteral("Invalid Request: root must be object.");
        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = QJsonValue::Null;
        resp[QStringLiteral("error")] = errObj;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
    }

    const QJsonObject resp = handleRpcRequest(doc.object());
    if (resp.isEmpty())
        return QByteArray();
    return QJsonDocument(resp).toJson(QJsonDocument::Compact) + "\n";
}

QJsonObject PythonAutomationServer::handleRpcRequest(const QJsonObject &req)
{
    const bool hasId = req.contains(QStringLiteral("id"));
    const QJsonValue idVal = hasId ? req.value(QStringLiteral("id")) : QJsonValue::Null;

    auto makeError = [&](int code, const QString &message) -> QJsonObject {
        QJsonObject errObj;
        errObj[QStringLiteral("code")] = code;
        errObj[QStringLiteral("message")] = message;

        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = idVal;
        resp[QStringLiteral("error")] = errObj;
        return resp;
    };

    if (!req.value(QStringLiteral("method")).isString()) {
        return makeError(-32600, QStringLiteral("Invalid Request: missing string 'method'."));
    }

    const QString method = req.value(QStringLiteral("method")).toString().trimmed();
    if (method.isEmpty()) {
        return makeError(-32600, QStringLiteral("Invalid Request: empty 'method'."));
    }

    QJsonObject params;
    if (req.contains(QStringLiteral("params")) && !req.value(QStringLiteral("params")).isNull()) {
        const QJsonValue pv = req.value(QStringLiteral("params"));
        if (pv.isObject()) {
            params = pv.toObject();
        } else if (pv.isArray()) {
            params[QStringLiteral("_args")] = pv.toArray();
        } else {
            return makeError(-32602,
                             QStringLiteral("Invalid params: 'params' must be object or array."));
        }
    }

    try {
        const QJsonObject result = dispatchMethod(method, params);
        emit rpcRequestHandled(method, true);
        if (!hasId)
            return QJsonObject(); // JSON-RPC 2.0 notification

        QJsonObject resp;
        resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        resp[QStringLiteral("id")] = idVal;
        resp[QStringLiteral("result")] = result;
        return resp;
    } catch (const RpcDispatchException &ex) {
        emit rpcRequestHandled(method, false);
        if (!hasId)
            return QJsonObject();
        return makeError(ex.code, QString::fromUtf8(ex.what()));
    } catch (const std::exception &ex) {
        emit rpcRequestHandled(method, false);
        if (!hasId)
            return QJsonObject();
        return makeError(-32000, QString::fromUtf8(ex.what()));
    } catch (...) {
        emit rpcRequestHandled(method, false);
        if (!hasId)
            return QJsonObject();
        return makeError(-32603, QStringLiteral("Internal error during RPC execution."));
    }
}

QString PythonAutomationServer::resolvePythonExecutable(bool *explicitInvalidOut)
{
    if (explicitInvalidOut)
        *explicitInvalidOut = false;

    const QString envPy = qEnvironmentVariable("TORIGRAPH_PYTHON_EXECUTABLE").trimmed();
    if (!envPy.isEmpty()) {
        if (QFileInfo::exists(envPy))
            return envPy;
        const QString foundEnv = QStandardPaths::findExecutable(envPy);
        if (!foundEnv.isEmpty())
            return foundEnv;
        if (explicitInvalidOut)
            *explicitInvalidOut = true;
        return envPy;
    }

    QString found = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (!found.isEmpty())
        return found;
    found = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (!found.isEmpty())
        return found;
    found = QStandardPaths::findExecutable(QStringLiteral("py"));
    if (!found.isEmpty())
        return found;
    return QStringLiteral("python");
}

QString PythonAutomationServer::resolvePythonExecutable()
{
    return resolvePythonExecutable(nullptr);
}

QString PythonAutomationServer::resolvePythonSdkDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    candidates << QDir(appDir).filePath(QStringLiteral("python"))
               << QDir(appDir).filePath(QStringLiteral("../python"))
               << QDir(appDir).filePath(QStringLiteral("../../python"))
               << QDir(appDir).filePath(QStringLiteral("../../../python"))
               << QDir::current().filePath(QStringLiteral("python"));

    for (const QString &cand : candidates) {
        QFileInfo fi(QDir(cand).filePath(QStringLiteral("torigraph/__init__.py")));
        if (fi.exists())
            return QFileInfo(cand).canonicalFilePath();
    }
    return QFileInfo(QDir(appDir).filePath(QStringLiteral("python"))).absoluteFilePath();
}

QProcess *PythonAutomationServer::startPythonScriptProcess(const QString &scriptPath,
                                                           const QStringList &extraArgs,
                                                           QObject *parent)
{
    if (!isTcpListening() && !isLocalPipeListening())
        startTcp(0, QHostAddress::LocalHost);

    auto *proc = new QProcess(parent ? parent : this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TORIGRAPH_RPC_ENDPOINT"), endpointString());
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));

    const QString sdkDir = resolvePythonSdkDir();
    const QString sep =
#ifdef Q_OS_WIN
            QStringLiteral(";");
#else
            QStringLiteral(":");
#endif
    const QString existingPyPath = env.value(QStringLiteral("PYTHONPATH"));
    const QString newPyPath = existingPyPath.isEmpty()
            ? sdkDir
            : (sdkDir + sep + existingPyPath);
    env.insert(QStringLiteral("PYTHONPATH"), newPyPath);
    proc->setProcessEnvironment(env);

    const QFileInfo sfi(scriptPath);
    if (sfi.exists())
        proc->setWorkingDirectory(sfi.absolutePath());

    QStringList args;
    args << QStringLiteral("-u") << scriptPath;
    args.append(extraArgs);
    proc->start(resolvePythonExecutable(), args);
    return proc;
}

// ============================================================================
// Category 1 (app.*), Category 2 (table.*), Category 3 (matrix.*) Helpers
// ============================================================================

namespace {

QString extractCurrentVersionClean()
{
    QString raw = SciDAVis::versionString().trimmed();
    QRegularExpression rx(QStringLiteral("([0-9]+\\.[0-9]+(?:\\.[0-9]+)?(?:-[A-Za-z0-9._-]+)?)"));
    QRegularExpressionMatch m = rx.match(raw);
    if (m.hasMatch())
        return m.captured(1);
    return QStringLiteral("2.4.0");
}

QJsonObject checkUpdatesBlockingFallback(const QJsonObject &params)
{
    QString feedUrl = params.value(QStringLiteral("feed_url")).toString().trimmed();
    if (feedUrl.isEmpty())
        feedUrl = params.value(QStringLiteral("url")).toString().trimmed();
    if (feedUrl.isEmpty())
        feedUrl = qEnvironmentVariable("TORIGRAPH_UPDATE_FEED_URL").trimmed();
    if (feedUrl.isEmpty()) {
        feedUrl = QStringLiteral("https://api.github.com/repos/kzykshmz/ToriGraph/releases/latest");
    }

    const bool includePrerelease = params.value(QStringLiteral("include_prerelease")).toBool(false);
    const QString currentVer = params.contains(QStringLiteral("current_version"))
            ? params.value(QStringLiteral("current_version")).toString().trimmed()
            : extractCurrentVersionClean();

    QByteArray rawPayload;
    QUrl qurl(feedUrl);
    if (!qurl.isValid() || qurl.scheme().isEmpty()) {
        QFile f(feedUrl);
        if (!f.open(QIODevice::ReadOnly))
            throwRpcRuntime(QStringLiteral("Failed to open local update feed: %1").arg(feedUrl));
        rawPayload = f.readAll();
    } else if (qurl.isLocalFile()) {
        QFile f(qurl.toLocalFile());
        if (!f.open(QIODevice::ReadOnly))
            throwRpcRuntime(
                    QStringLiteral("Failed to open local update feed: %1").arg(qurl.toLocalFile()));
        rawPayload = f.readAll();
    } else {
        QNetworkAccessManager nam;
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
        const int timeoutMs = std::clamp(params.value(QStringLiteral("timeout_ms")).toInt(10000),
                                         500, 60000);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();

        if (!reply->isFinished()) {
            reply->abort();
            reply->deleteLater();
            throwRpcRuntime(QStringLiteral("Update check timed out fetching %1").arg(feedUrl));
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
        assetUrl = chosenAsset.value(QStringLiteral("browser_download_url")).toString();
        assetSize = static_cast<qint64>(chosenAsset.value(QStringLiteral("size")).toDouble(0));
        const QString digest = chosenAsset.value(QStringLiteral("digest")).toString().trimmed();
        if (digest.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
            sha256Hex = digest.mid(7).trimmed().toLower();
    }

    const QString body = relObj.value(QStringLiteral("body")).toString();
    if (sha256Hex.isEmpty() && !body.isEmpty()) {
        QRegularExpression shaRx(QStringLiteral("\\b([A-Fa-f0-9]{64})\\b"));
        QRegularExpressionMatch sm = shaRx.match(body);
        if (sm.hasMatch())
            sha256Hex = sm.captured(1).toLower();
    }

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
    out[QStringLiteral("html_url")] = relObj.value(QStringLiteral("html_url")).toString();
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

void writeBulkColumnArray(Table *t, int colIdx, const QJsonArray &arr, const QString &modeHint)
{
    Column *col = t->column(colIdx);
    if (!col)
        throwRpcRuntime(QStringLiteral("Null column at index %1.").arg(colIdx));

    const int n = arr.size();
    if (n > t->numRows())
        t->setNumRows(n);

    bool useText = (modeHint.compare(QStringLiteral("text"), Qt::CaseInsensitive) == 0);
    if (!useText && modeHint.isEmpty()) {
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
            else if (v.isDouble())
                texts.append(QString::number(v.toDouble(), 'g', 15));
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

void applyColumnMetadataObject(Table *t, int colIdx, const QJsonObject &meta)
{
    Column *col = t->column(colIdx);
    if (!col)
        return;

    if (meta.contains(QStringLiteral("name")) && meta.value(QStringLiteral("name")).isString()) {
        const QString newName = meta.value(QStringLiteral("name")).toString().trimmed();
        if (!newName.isEmpty())
            t->setColName(colIdx, newName);
    }
    if (meta.contains(QStringLiteral("long_name")))
        t->setColLongName(colIdx, meta.value(QStringLiteral("long_name")).toString());
    if (meta.contains(QStringLiteral("units")))
        t->setColUnits(colIdx, meta.value(QStringLiteral("units")).toString());
    if (meta.contains(QStringLiteral("comment")))
        t->setColComment(colIdx, meta.value(QStringLiteral("comment")).toString());
    if (meta.contains(QStringLiteral("plot_designation"))) {
        const QString pdStr = meta.value(QStringLiteral("plot_designation")).toString().trimmed();
        if (pdStr.compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0
            && col->columnMode() != SciDAVis::ColumnMode::Text) {
            col->setColumnMode(SciDAVis::ColumnMode::Text);
        }
        t->setColPlotDesignation(colIdx, parsePlotDesignation(pdStr, t->colPlotDesignation(colIdx)));
    }
    if (meta.contains(QStringLiteral("numeric_format"))
        || meta.contains(QStringLiteral("precision"))) {
        if (auto *f = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
            if (meta.contains(QStringLiteral("numeric_format"))) {
                const QString fmt =
                        meta.value(QStringLiteral("numeric_format")).toString().trimmed();
                if (!fmt.isEmpty()) {
                    const char fc = fmt.at(0).toLatin1();
                    if (fc == 'f' || fc == 'e' || fc == 'E' || fc == 'g' || fc == 'G')
                        f->setNumericFormat(fc);
                }
            }
            if (meta.contains(QStringLiteral("precision"))) {
                f->setNumDigits(std::clamp(meta.value(QStringLiteral("precision")).toInt(6), 0, 16));
            }
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
    const double sd = (n > 1) ? std::sqrt(varSum / double(n - 1)) : 0.0;
    const double se = (n > 0) ? (sd / std::sqrt(double(n))) : 0.0;

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

double computeMatrixDeterminantHeadless(Matrix *m)
{
    const int n = m->numRows();
    if (n <= 0 || n != m->numCols())
        throwInvalidParams(
                QStringLiteral("Matrix '%1' must be square (%2x%3).")
                        .arg(m->name())
                        .arg(m->numRows())
                        .arg(m->numCols()));

    gsl_matrix *gm = gsl_matrix_alloc(size_t(n), size_t(n));
    gsl_permutation *perm = gsl_permutation_alloc(size_t(n));
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            const double v = m->cell(r, c);
            gsl_matrix_set(gm, size_t(r), size_t(c), std::isfinite(v) ? v : 0.0);
        }
    }
    int signum = 0;
    gsl_linalg_LU_decomp(gm, perm, &signum);
    const double det = gsl_linalg_LU_det(gm, signum);
    gsl_permutation_free(perm);
    gsl_matrix_free(gm);
    return det;
}

void invertMatrixHeadless(ApplicationWindow *app, Matrix *m)
{
    const int n = m->numRows();
    if (n <= 0 || n != m->numCols())
        throwInvalidParams(
                QStringLiteral("Matrix '%1' must be square to invert.").arg(m->name()));

    gsl_matrix *gm = gsl_matrix_alloc(size_t(n), size_t(n));
    gsl_matrix *inv = gsl_matrix_alloc(size_t(n), size_t(n));
    gsl_permutation *perm = gsl_permutation_alloc(size_t(n));
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            const double v = m->cell(r, c);
            gsl_matrix_set(gm, size_t(r), size_t(c), std::isfinite(v) ? v : 0.0);
        }
    }
    int signum = 0;
    gsl_linalg_LU_decomp(gm, perm, &signum);
    const double det = gsl_linalg_LU_det(gm, signum);
    if (!std::isfinite(det) || std::abs(det) < 1e-300) {
        gsl_permutation_free(perm);
        gsl_matrix_free(inv);
        gsl_matrix_free(gm);
        throwRpcRuntime(QStringLiteral("Matrix '%1' is singular and cannot be inverted.")
                                .arg(m->name()));
    }
    gsl_linalg_LU_invert(gm, perm, inv);
    for (int c = 0; c < n; ++c) {
        QVector<qreal> colVec(n);
        for (int r = 0; r < n; ++r)
            colVec[r] = gsl_matrix_get(inv, size_t(r), size_t(c));
        m->d_future_matrix->setColumnCells(c, 0, n - 1, colVec);
    }
    gsl_permutation_free(perm);
    gsl_matrix_free(inv);
    gsl_matrix_free(gm);
    if (app)
        app->updateMatrixPlots(m);
}

} // namespace

// ============================================================================
// Category 4 (plot.*), Category 5 (graph.*), Category 6/7 Helpers
// ============================================================================

namespace {

QString pageUnitToString(MultiLayer::PageUnit u)
{
    switch (u) {
    case MultiLayer::UnitMm:
        return QStringLiteral("mm");
    case MultiLayer::UnitCm:
        return QStringLiteral("cm");
    case MultiLayer::UnitInch:
        return QStringLiteral("inch");
    default:
        return QStringLiteral("px");
    }
}

MultiLayer::PageUnit parsePageUnit(const QString &s)
{
    const QString u = s.trimmed().toLower();
    if (u == QStringLiteral("mm") || u == QStringLiteral("millimeter"))
        return MultiLayer::UnitMm;
    if (u == QStringLiteral("cm") || u == QStringLiteral("centimeter"))
        return MultiLayer::UnitCm;
    if (u == QStringLiteral("in") || u == QStringLiteral("inch") || u == QStringLiteral("inches"))
        return MultiLayer::UnitInch;
    return MultiLayer::UnitPx;
}

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

Spectrogram::ColorMapPreset parseColorMapPreset(const QString &name, bool *outGrayScale = nullptr,
                                                bool strict = false)
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

void applyRequestedWindowName(ApplicationWindow *app, MyWidget *w, const QJsonObject &params,
                              const QString &explicitKey = QString())
{
    if (!app || !w)
        return;
    QString req;
    if (!explicitKey.isEmpty()) {
        req = params.value(explicitKey).toString().trimmed();
    } else {
        req = params.value(QStringLiteral("graph_name")).toString().trimmed();
        if (req.isEmpty())
            req = params.value(QStringLiteral("name")).toString().trimmed();
    }
    if (req.isEmpty())
        return;
    const QString fallback = w->inherits("Table")
            ? QStringLiteral("Table")
            : (w->inherits("Matrix") ? QStringLiteral("Matrix") : QStringLiteral("Graph"));
    const QString sanitized = sanitizeWindowNameForSciDAVis(req, fallback);
    if (sanitized == w->name())
        return;
    const QString targetName = makeUniqueValidWindowName(app, sanitized, w);
    if (targetName != w->name())
        app->renameWindow(w, targetName);
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
    ax[QStringLiteral("title_font_size")] = tf.pointSize();
    ax[QStringLiteral("numbers_font_family")] = nf.family();
    ax[QStringLiteral("numbers_font_size")] = nf.pointSize();
    ax[QStringLiteral("font_family")] = tf.family();
    ax[QStringLiteral("font_size")] = tf.pointSize();

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
        const bool isLog = (dynamic_cast<const QwtLog10ScaleEngine *>(se) != nullptr);
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
        if (g->legend())
            lObj[QStringLiteral("legend_text")] = g->legend()->text();

        if (Plot *plt = g->plotWidget()) {
            const QwtText titleTxt = plt->title();
            lObj[QStringLiteral("title")] = titleTxt.text();
            lObj[QStringLiteral("title_font_family")] = titleTxt.font().family();
            lObj[QStringLiteral("title_font_size")] = titleTxt.font().pointSize();
            if (!titleTxt.text().isEmpty()
                && titleTxt.font().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive) != 0)
                allFontsArial = false;
        }
        if (Legend *leg = g->legend()) {
            lObj[QStringLiteral("legend_text")] = leg->text();
            lObj[QStringLiteral("legend_font_family")] = leg->font().family();
            lObj[QStringLiteral("legend_font_size")] = leg->font().pointSize();
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
        lObj[QStringLiteral("reference_line_count")] = g->referenceLines().size();
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

// Headless format clipboard so graph.copy_format / graph.paste_format work even when window is hidden
struct HeadlessFormatClipboard
{
    bool valid = false;
    bool pageViewMode = false;
    MultiLayer::PageUnit pageUnit = MultiLayer::UnitPx;
    double pageW = 520.0;
    double pageH = 520.0;
    int pageDpi = 300;
    QColor pageBg = QColor(Qt::white);
    bool keepAspect = false;
    double aspect = 1.0;
    int lm = 5, rm = 5, tm = 5, bm = 5;
    int rSpace = 5, cSpace = 5;
    QFont axisTitleFonts[4];
    QFont axisNumFonts[4];
    bool hasPlotTitleFont = false;
    QFont plotTitleFont;
    bool hasLegendFont = false;
    QFont legendFont;
    bool axisEnabled[4] = { true, false, true, false };
    bool backboneVis[4] = { true, true, true, true };
    int majTicks[4] = { ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out };
    int minTicks[4] = { ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out };
    bool framed = false;
    int frameWidth = 1;
    QColor frameColor = QColor(Qt::black);
};

HeadlessFormatClipboard &rpcFormatClipboard()
{
    static HeadlessFormatClipboard s_clip;
    return s_clip;
}

double standardNormalInvCdf(double p)
{
    p = std::clamp(p, 1e-12, 1.0 - 1e-12);
    return gsl_cdf_ugaussian_Pinv(p);
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

// Compute ROC AUC, DeLong/Hanley-McNeil SE, and Youden J optimal threshold directly
struct RocMetricsComputed
{
    double auc = 0.0;
    double se = 0.0;
    double optimalThreshold = 0.0;
    double sensitivity = 0.0;
    double specificity = 0.0;
    int nPos = 0;
    int nNeg = 0;
};

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
        }
        yOut.reserve(rawStates.size());
        if (!rawStates.empty() && maxSt > minSt) {
            const double mid = 0.5 * (minSt + maxSt);
            for (double sv : rawStates)
                yOut.push_back((sv > mid) ? 1 : 0);
        } else {
            for (double sv : rawStates)
                yOut.push_back((sv >= 0.5) ? 1 : 0);
        }
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
        auto *colY = new Column(cleanTitle, SciDAVis::ColumnMode::Numeric);
        colY->setPlotDesignation(SciDAVis::Y);
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

        const QString tblName = app->generateUniqueName(QStringLiteral("RpcFitCurve"));
        Table *backing =
                app->newHiddenTable(tblName, cleanTitle, QList<Column *>() << colX << colY);
        if (!backing) {
            delete colX;
            delete colY;
            continue;
        }

        auto *dc = new DataCurve(backing, tblName + QStringLiteral("_") + colX->name(),
                                 tblName + QStringLiteral("_") + colY->name(), 0, n - 1);
        dc->setData(xv.data(), yv.data(), n);
        dc->setPen(savedPen);
        dc->setBrush(savedBrush);
        dc->setSymbol(savedSym);
        dc->setStyle(savedStyle);
        dc->setAxis(savedXAxis, savedYAxis);
        dc->setVisible(savedVis);

        g->removeCurve(i);
        g->insertPlotItem(dc, Graph::Line);
        g->addFitCurve(dc);
        g->addLegendItem(cleanTitle);
        --i;
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

} // namespace

// ============================================================================
// Method Introspection & Categories 1-5 Dispatch (app / table / matrix / plot / graph)
// ============================================================================

QStringList PythonAutomationServer::supportedMethods()
{
    return QStringList{
        // Category 1: App / Project / GUI / Action Escape Hatch
        QStringLiteral("ping"),
        QStringLiteral("app.info"),
        QStringLiteral("app.supported_methods"),
        QStringLiteral("app.new_project"),
        QStringLiteral("app.open_project"),
        QStringLiteral("app.save_project"),
        QStringLiteral("app.import_csv"),
        QStringLiteral("app.import_opj"),
        QStringLiteral("app.list_windows"),
        QStringLiteral("app.window_info"),
        QStringLiteral("app.activate_window"),
        QStringLiteral("app.rename_window"),
        QStringLiteral("app.close_window"),
        QStringLiteral("app.clone_window"),
        QStringLiteral("app.set_language"),
        QStringLiteral("app.get_language"),
        QStringLiteral("app.set_recalc_mode"),
        QStringLiteral("app.recalculate"),
        QStringLiteral("app.set_focus_hud"),
        QStringLiteral("app.get_focus_hud"),
        QStringLiteral("app.set_workspace_mode"),
        QStringLiteral("app.check_updates"),
        QStringLiteral("app.list_actions"),
        QStringLiteral("app.trigger_action"),

        // Category 2: Worksheet (table.*)
        QStringLiteral("table.create"),
        QStringLiteral("table.get_info"),
        QStringLiteral("table.resize"),
        QStringLiteral("table.add_column"),
        QStringLiteral("table.remove_column"),
        QStringLiteral("table.set_column_data"),
        QStringLiteral("table.get_column_data"),
        QStringLiteral("table.set_data_dict"),
        QStringLiteral("table.set_cell"),
        QStringLiteral("table.get_cell"),
        QStringLiteral("table.set_column_meta"),
        QStringLiteral("table.get_column_meta"),
        QStringLiteral("table.set_formula"),
        QStringLiteral("table.recalculate"),
        QStringLiteral("table.mask_rows"),
        QStringLiteral("table.unmask_rows"),
        QStringLiteral("table.clear_masks"),
        QStringLiteral("table.get_masked_rows"),
        QStringLiteral("table.sort"),
        QStringLiteral("table.column_stats"),
        QStringLiteral("table.row_stats"),
        QStringLiteral("table.export_csv"),
        QStringLiteral("table.to_matrix"),

        // Category 3: Matrix (matrix.*)
        QStringLiteral("matrix.create"),
        QStringLiteral("matrix.get_info"),
        QStringLiteral("matrix.set_coordinates"),
        QStringLiteral("matrix.set_data"),
        QStringLiteral("matrix.get_data"),
        QStringLiteral("matrix.set_formula"),
        QStringLiteral("matrix.transpose"),
        QStringLiteral("matrix.invert"),
        QStringLiteral("matrix.determinant"),
        QStringLiteral("matrix.flip"),
        QStringLiteral("matrix.fft2d"),
        QStringLiteral("matrix.filter2d"),
        QStringLiteral("matrix.analyze_2d_peaks"),
        QStringLiteral("matrix.to_table"),

        // Category 4: 2D / 3D / Matrix Plotting (plot.*)
        QStringLiteral("plot.xy"),
        QStringLiteral("plot.stacked_bars"),
        QStringLiteral("plot.add_curve"),
        QStringLiteral("plot.remove_curve"),
        QStringLiteral("plot.add_error_bars"),
        QStringLiteral("plot.double_y"),
        QStringLiteral("plot.waterfall"),
        QStringLiteral("plot.histogram"),
        QStringLiteral("plot.box"),
        QStringLiteral("plot.violin"),
        QStringLiteral("plot.pareto"),
        QStringLiteral("plot.qq"),
        QStringLiteral("plot.pie"),
        QStringLiteral("plot.vectors"),
        QStringLiteral("plot.ternary"),
        QStringLiteral("plot.function"),
        QStringLiteral("plot.matrix"),
        QStringLiteral("plot.3d_matrix"),
        QStringLiteral("plot.3d_xyz"),
        QStringLiteral("plot.3d_function"),

        // Category 5: Graph Page, Multi-Layer, Axes, Breaks, Frame, Style, Colorbar & Export (graph.*)
        QStringLiteral("graph.get_info"),
        QStringLiteral("graph.add_layer"),
        QStringLiteral("graph.add_linked_layer"),
        QStringLiteral("graph.add_inset_layer"),
        QStringLiteral("graph.arrange_layers"),
        QStringLiteral("graph.set_page_size"),
        QStringLiteral("graph.fit_page_to_layers"),
        QStringLiteral("graph.fit_layers_to_page"),
        QStringLiteral("graph.apply_style_preset"),
        QStringLiteral("graph.apply_frame_preset"),
        QStringLiteral("graph.set_aspect_ratio"),
        QStringLiteral("graph.copy_format"),
        QStringLiteral("graph.paste_format"),
        QStringLiteral("graph.batch_clone"),
        QStringLiteral("graph.set_axis"),
        QStringLiteral("graph.set_axis_break"),
        QStringLiteral("graph.add_reference_line"),
        QStringLiteral("graph.add_shaded_band"),
        QStringLiteral("graph.set_grid"),
        QStringLiteral("graph.set_curve_style"),
        QStringLiteral("graph.set_stack_offsets"),
        QStringLiteral("graph.apply_palette"),
        QStringLiteral("graph.set_colorbar"),
        QStringLiteral("graph.set_legend"),
        QStringLiteral("graph.add_text"),
        QStringLiteral("graph.add_arrow"),
        QStringLiteral("graph.mask_region"),
        QStringLiteral("graph.clear_masks"),
        QStringLiteral("graph.export"),
        QStringLiteral("graph.copy_image"),
        QStringLiteral("graph.copy_ole"),
        QStringLiteral("graph.set_3d_options"),

        // Category 6: All Curve Fitting & Peak Analysis (analysis.*)
        QStringLiteral("analysis.list_fit_models"),
        QStringLiteral("analysis.fit_linear"),
        QStringLiteral("analysis.fit_york"),
        QStringLiteral("analysis.fit_polynomial"),
        QStringLiteral("analysis.fit_nonlinear"),
        QStringLiteral("analysis.fit_global"),
        QStringLiteral("analysis.fit_piecewise"),
        QStringLiteral("analysis.fit_robust"),
        QStringLiteral("analysis.fit_quantile"),
        QStringLiteral("analysis.fit_logistic"),
        QStringLiteral("analysis.fit_mlr"),
        QStringLiteral("analysis.fit_implicit_odr"),
        QStringLiteral("analysis.fit_surface_3d"),
        QStringLiteral("analysis.compare_models"),
        QStringLiteral("analysis.compare_datasets"),
        QStringLiteral("analysis.analyze_peaks"),
        QStringLiteral("analysis.batch_peaks"),

        // Category 7: Gadgets, Signal Processing, Curve Math, MatSci, Stats & Digitizer
        QStringLiteral("gadget.attach"),
        QStringLiteral("gadget.set_roi"),
        QStringLiteral("gadget.query"),
        QStringLiteral("gadget.clear"),

        QStringLiteral("signal.smooth"),
        QStringLiteral("signal.savgol_derivative"),
        QStringLiteral("signal.fft"),
        QStringLiteral("signal.fft_filter"),
        QStringLiteral("signal.iir_filter"),
        QStringLiteral("signal.stft"),
        QStringLiteral("signal.envelope"),
        QStringLiteral("signal.wavelet_cwt"),
        QStringLiteral("signal.wavelet_denoise"),
        QStringLiteral("signal.welch_psd"),
        QStringLiteral("signal.coherence"),
        QStringLiteral("signal.decimate"),
        QStringLiteral("signal.fsd"),

        QStringLiteral("curvemath.eval"),
        QStringLiteral("curvemath.average_curves"),
        QStringLiteral("curvemath.normalize"),
        QStringLiteral("curvemath.integrate"),
        QStringLiteral("curvemath.differentiate"),
        QStringLiteral("curvemath.interpolate"),
        QStringLiteral("curvemath.subtract_line"),
        QStringLiteral("curvemath.intersections"),
        QStringLiteral("curvemath.lookup_xy"),
        QStringLiteral("curvemath.polygon_area"),
        QStringLiteral("curvemath.fill_between"),
        QStringLiteral("curvemath.arrhenius"),
        QStringLiteral("curvemath.tauc"),
        QStringLiteral("curvemath.eis_fit"),

        QStringLiteral("stats.normality"),
        QStringLiteral("stats.outlier"),
        QStringLiteral("stats.anova_oneway"),
        QStringLiteral("stats.anova_twoway"),
        QStringLiteral("stats.nonparametric"),
        QStringLiteral("stats.paired_plot"),
        QStringLiteral("stats.bland_altman"),
        QStringLiteral("stats.passing_bablok"),
        QStringLiteral("stats.weibull"),
        QStringLiteral("stats.kaplan_meier"),
        QStringLiteral("stats.roc"),
        QStringLiteral("stats.pca"),
        QStringLiteral("stats.correlation"),
        QStringLiteral("stats.clustering"),
        QStringLiteral("stats.confidence_ellipse"),

        QStringLiteral("digitizer.extract")
    };
}

namespace {

bool dispatchCoreAndGraphMethod(PythonAutomationServer *server, ApplicationWindow *d_app,
                                bool &focusHudFallback, QString &workspaceModeFallback,
                                const QString &method, const QJsonObject &params,
                                QJsonObject &outResult)
{
    // ------------------------------------------------------------------------
    // 1. App / Project / GUI / Action Escape Hatch
    // ------------------------------------------------------------------------
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
        resetProjectInPlaceHeadless(d_app);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("window_count")] = d_app->windowsList().size();
        return true;
    }

    if (method == QStringLiteral("app.open_project")) {
        const QString path = params.value(QStringLiteral("path")).toString().trimmed();
        const QFileInfo pfi(path);
        if (path.isEmpty() || !pfi.exists() || !pfi.isFile())
            throwInvalidParams(QStringLiteral("Project file does not exist: %1").arg(path));
        const QString absPath = pfi.absoluteFilePath();
        const bool append = params.value(QStringLiteral("append")).toBool(false);
        if (append) {
            QFile hdrFile(absPath);
            if (!hdrFile.open(QIODevice::ReadOnly)) {
                throwRpcRuntime(QStringLiteral("Failed to open project file '%1': %2")
                                        .arg(path, hdrFile.errorString()));
            }
            const QByteArray rawHead = hdrFile.read(128);
            hdrFile.close();
            if (rawHead.isEmpty())
                throwRpcRuntime(QStringLiteral("Project file is empty: %1").arg(path));
            const bool isOpjExt = absPath.endsWith(QStringLiteral(".opj"), Qt::CaseInsensitive)
                    || absPath.endsWith(QStringLiteral(".opju"), Qt::CaseInsensitive);
            const bool hasGzipMagic = rawHead.size() >= 2
                    && static_cast<unsigned char>(rawHead[0]) == 0x1F
                    && static_cast<unsigned char>(rawHead[1]) == 0x8B;
            const bool hasCpyaMagic = rawHead.startsWith("CPYA");
            QByteArray trimmedHead = rawHead;
            if (trimmedHead.startsWith("\xEF\xBB\xBF"))
                trimmedHead.remove(0, 3);
            trimmedHead = trimmedHead.trimmed();
            const bool hasSciHeader = trimmedHead.startsWith("SciDAVis")
                    || trimmedHead.startsWith("QtiPlot") || trimmedHead.startsWith("<?xml")
                    || trimmedHead.startsWith("<scidavis");
            if (isOpjExt ? !hasCpyaMagic : !(hasGzipMagic || hasCpyaMagic || hasSciHeader)) {
                throwRpcRuntime(
                        QStringLiteral("Invalid or corrupt project header in '%1'.").arg(path));
            }

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

        resetProjectInPlaceHeadless(d_app);
        const bool ok = d_app->loadProject(absPath);
        if (!ok)
            throwRpcRuntime(QStringLiteral("Failed to load project: %1").arg(path));
        d_app->projectname = absPath;
        d_app->savedProject();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("path")] = d_app->projectname;
        outResult[QStringLiteral("window_count")] = d_app->windowsList().size();
        return true;
    }

    if (method == QStringLiteral("app.save_project")) {
        QString path = params.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty())
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
                || params.value(QStringLiteral("compress")).toBool(false);
        const QString finalPath = (compress && !absPath.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive))
                ? (absPath + QStringLiteral(".gz"))
                : absPath;
        if (QFileInfo(finalPath).exists() && QFileInfo(finalPath).isDir())
            throwInvalidParams(
                    QStringLiteral("Output project 'path' is an existing directory: %1").arg(finalPath));

        sanitizeAllProjectFunctionCurves(d_app);
        const QByteArray rawBytes = d_app->saveProjectToByteArray();
        if (rawBytes.isEmpty())
            throwRpcRuntime(QStringLiteral("Failed to serialize project in memory."));

        const QByteArray outBytes = finalPath.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive)
                ? gzipCompressBytes(rawBytes)
                : rawBytes;

        bool wroteOk = false;
        QString writeErr;
        {
            QSaveFile sf(finalPath);
            if (sf.open(QIODevice::WriteOnly)) {
                if (sf.write(outBytes) == outBytes.size() && sf.commit())
                    wroteOk = true;
                else
                    writeErr = sf.errorString();
            } else {
                writeErr = sf.errorString();
            }
        }
        if (!wroteOk) {
            QFile directOut(finalPath);
            if (directOut.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && directOut.write(outBytes) == outBytes.size() && directOut.flush()) {
                directOut.close();
                wroteOk = true;
            } else {
                writeErr = directOut.errorString();
            }
        }
        if (!wroteOk || !QFileInfo::exists(finalPath) || QFileInfo(finalPath).size() <= 0) {
            throwRpcRuntime(
                    QStringLiteral("Failed to write project file '%1': %2")
                            .arg(finalPath, writeErr));
        }
        d_app->projectname = finalPath;
        d_app->savedProject();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("path")] = finalPath;
        outResult[QStringLiteral("compressed")] =
                finalPath.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive);
        outResult[QStringLiteral("bytes")] = QFileInfo(finalPath).size();
        return true;
    }

    if (method == QStringLiteral("app.import_csv")) {
        const QString path = params.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path) || !QFileInfo(path).isFile())
            throwInvalidParams(QStringLiteral("CSV/ASCII file does not exist: %1").arg(path));
        QString sep = params.value(QStringLiteral("separator")).toString();
        if (sep.isEmpty()) {
            sep = path.endsWith(QStringLiteral(".tsv"), Qt::CaseInsensitive)
                    ? QStringLiteral("\t")
                    : QStringLiteral(",");
        }
        const int sepLen = sep.size();
        const int ignoredLines = std::max(0, params.value(QStringLiteral("skip_lines")).toInt(0));
        const bool hasHeader = params.contains(QStringLiteral("header"))
                ? params.value(QStringLiteral("header")).toBool(true)
                : true;
        const bool stripSpaces = params.contains(QStringLiteral("strip_spaces"))
                ? params.value(QStringLiteral("strip_spaces")).toBool(true)
                : true;
        const bool simplifySpaces = params.value(QStringLiteral("simplify_spaces")).toBool(false);
        const bool convertNumeric = params.contains(QStringLiteral("numeric"))
                ? params.value(QStringLiteral("numeric")).toBool(true)
                : true;

        const QString absPath = QFileInfo(path).absoluteFilePath();
        QFile inFile(absPath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            throwRpcRuntime(QStringLiteral("Failed to open CSV/ASCII file '%1': %2")
                                    .arg(absPath, inFile.errorString()));
        }
        QByteArray rawBytes = inFile.readAll();
        inFile.close();
        if (rawBytes.startsWith("\xEF\xBB\xBF"))
            rawBytes.remove(0, 3);
        const QString text = QString::fromUtf8(rawBytes);
        const int len = text.size();

        int pos = 0;
        for (int skipped = 0; skipped < ignoredLines && pos < len; ++skipped) {
            while (pos < len && text[pos] != QLatin1Char('\r') && text[pos] != QLatin1Char('\n'))
                ++pos;
            if (pos < len && text[pos] == QLatin1Char('\r')) {
                ++pos;
                if (pos < len && text[pos] == QLatin1Char('\n'))
                    ++pos;
            } else if (pos < len && text[pos] == QLatin1Char('\n')) {
                ++pos;
            }
        }

        struct CsvCell
        {
            QString value;
            bool quoted = false;
        };

        QVector<QVector<CsvCell>> records;
        int maxCols = 0;
        while (pos < len) {
            QVector<CsvCell> row;
            while (true) {
                CsvCell cell;
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
                    ++pos; // opening '"'
                    while (pos < len) {
                        const QChar ch = text[pos];
                        if (ch == QLatin1Char('"')) {
                            if (pos + 1 < len && text[pos + 1] == QLatin1Char('"')) {
                                cell.value.append(QLatin1Char('"'));
                                pos += 2;
                            } else {
                                ++pos; // closing '"'
                                break;
                            }
                        } else {
                            cell.value.append(ch);
                            ++pos;
                        }
                    }
                    while (pos < len && text[pos] != QLatin1Char('\r')
                           && text[pos] != QLatin1Char('\n')
                           && text.midRef(pos, sepLen) != sep) {
                        if (text[pos] != QLatin1Char(' ') && text[pos] != QLatin1Char('\t'))
                            cell.value.append(text[pos]);
                        ++pos;
                    }
                } else {
                    pos = fieldStart;
                    const int rawStart = pos;
                    while (pos < len && text[pos] != QLatin1Char('\r')
                           && text[pos] != QLatin1Char('\n')
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
            maxCols = std::max(maxCols, row.size());
            records.append(row);
        }

        maxCols = std::max(1, maxCols);
        while (!records.isEmpty() && maxCols > 1 && records.last().size() == 1
               && !records.last()[0].quoted && records.last()[0].value.isEmpty()) {
            records.removeLast();
        }

        const bool withComments = params.contains(QStringLiteral("with_comments"))
                ? params.value(QStringLiteral("with_comments")).toBool(false)
                : params.value(QStringLiteral("comments")).toBool(false);
        QVector<CsvCell> headerRow;
        QVector<CsvCell> commentRow;
        int dataStartIdx = 0;
        if (hasHeader && !records.isEmpty()) {
            headerRow = records[0];
            dataStartIdx = 1;
        }
        if (withComments && dataStartIdx < records.size()) {
            commentRow = records[dataStartIdx];
            ++dataStartIdx;
        }
        const int dataRowCount = std::max(0, records.size() - dataStartIdx);

        Table *t = createUniqueTableHeadless(
                d_app, std::max(1, dataRowCount), maxCols,
                params.value(QStringLiteral("name")).toString());
        t->setWindowLabel(absPath);

        if (hasHeader && !headerRow.isEmpty()) {
            for (int c = 0; c < maxCols; ++c)
                t->setColName(c, QStringLiteral("__import_col_%1__").arg(c));
        }

        QLocale cLoc = QLocale::c();
        cLoc.setNumberOptions(QLocale::RejectGroupSeparator);

        for (int c = 0; c < maxCols; ++c) {
            Column *col = t->column(c);
            if (!col)
                continue;

            bool useText = !convertNumeric;
            bool anyQuoted = false;
            bool anyFiniteNumber = false;
            QVector<qreal> parsedNums(dataRowCount, std::numeric_limits<double>::quiet_NaN());
            QVector<int> invalidRows;

            if (convertNumeric) {
                for (int r = 0; r < dataRowCount; ++r) {
                    const QVector<CsvCell> &rec = records[dataStartIdx + r];
                    const QString valStr = (c < rec.size()) ? rec[c].value : QString();
                    const bool wasQuoted = (c < rec.size()) ? rec[c].quoted : false;
                    if (wasQuoted)
                        anyQuoted = true;
                    const QString trimmed = valStr.trimmed();
                    if (trimmed.isEmpty()) {
                        invalidRows.append(r);
                        continue;
                    }
                    if (!wasQuoted
                        && (trimmed.compare(QStringLiteral("nan"), Qt::CaseInsensitive) == 0
                            || trimmed.compare(QStringLiteral("+nan"), Qt::CaseInsensitive) == 0
                            || trimmed.compare(QStringLiteral("-nan"), Qt::CaseInsensitive) == 0
                            || trimmed.compare(QStringLiteral("inf"), Qt::CaseInsensitive) == 0
                            || trimmed.compare(QStringLiteral("+inf"), Qt::CaseInsensitive) == 0
                            || trimmed.compare(QStringLiteral("-inf"), Qt::CaseInsensitive) == 0)) {
                        invalidRows.append(r);
                        continue;
                    }
                    bool okNum = false;
                    const double numVal = cLoc.toDouble(trimmed, &okNum);
                    if (okNum && std::isfinite(numVal)) {
                        parsedNums[r] = numVal;
                        anyFiniteNumber = true;
                    } else {
                        useText = true;
                        break;
                    }
                }
                if (!useText && anyQuoted && !anyFiniteNumber)
                    useText = true;
            }

            if (useText) {
                if (col->columnMode() != SciDAVis::ColumnMode::Text)
                    col->setColumnMode(SciDAVis::ColumnMode::Text);
                if (dataRowCount > 0) {
                    QStringList texts;
                    texts.reserve(dataRowCount);
                    for (int r = 0; r < dataRowCount; ++r) {
                        const QVector<CsvCell> &rec = records[dataStartIdx + r];
                        texts.append((c < rec.size()) ? rec[c].value : QString());
                    }
                    col->replaceTexts(0, texts);
                } else {
                    col->setInvalid(Interval<int>(0, t->numRows() - 1), true);
                }
            } else {
                if (col->columnMode() != SciDAVis::ColumnMode::Numeric)
                    col->setColumnMode(SciDAVis::ColumnMode::Numeric);
                if (dataRowCount > 0) {
                    col->clearValidity();
                    col->replaceValues(0, parsedNums);
                    int k = 0;
                    while (k < invalidRows.size()) {
                        const int startR = invalidRows[k];
                        int endR = startR;
                        while (k + 1 < invalidRows.size() && invalidRows[k + 1] == endR + 1) {
                            endR = invalidRows[++k];
                        }
                        col->setInvalid(Interval<int>(startR, endR), true);
                        ++k;
                    }
                } else {
                    col->setInvalid(Interval<int>(0, t->numRows() - 1), true);
                }
            }

            if (hasHeader && !headerRow.isEmpty()) {
                QString hdrName = (c < headerRow.size()) ? headerRow[c].value : QString();
                if (hdrName.trimmed().isEmpty())
                    hdrName = QString::number(c + 1);
                t->setColName(c, hdrName);
            }
            if (withComments) {
                const QString cmtStr = (c < commentRow.size()) ? commentRow[c].value : QString();
                t->setColComment(c, cmtStr);
            }
            t->setColPlotDesignation(c, (c == 0) ? SciDAVis::X : SciDAVis::Y);
        }

        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("app.import_opj")) {
        const QString path = params.value(QStringLiteral("path")).toString().trimmed();
        const QFileInfo opjFi(path);
        if (path.isEmpty() || !opjFi.exists() || !opjFi.isFile())
            throwInvalidParams(QStringLiteral("OPJ file does not exist: %1").arg(path));
#ifdef ORIGIN_IMPORT
        QFile opjCheck(opjFi.absoluteFilePath());
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
        const QString codec = params.value(QStringLiteral("codec")).toString().trimmed();
        ImportOPJ importer(d_app, opjFi.absoluteFilePath(), codec);
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
        const QString filterType = params.value(QStringLiteral("type")).toString().trimmed();
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
        const QString name = params.value(QStringLiteral("name")).toString().trimmed();
        MyWidget *w = findWindowByName(d_app, name);
        if (!w)
            throwInvalidParams(QStringLiteral("Window not found: %1").arg(name));
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
        const QString name = params.value(QStringLiteral("name")).toString().trimmed();
        MyWidget *w = findWindowByName(d_app, name);
        if (!w)
            throwInvalidParams(QStringLiteral("Window not found: %1").arg(name));
        d_app->activateSubWindow(w);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("name")] = w->name();
        outResult[QStringLiteral("type")] = windowTypeString(w);
        return true;
    }

    if (method == QStringLiteral("app.rename_window")) {
        const QString oldName = params.value(QStringLiteral("name")).toString().trimmed();
        const QString newName = params.value(QStringLiteral("new_name")).toString().trimmed();
        if (newName.isEmpty())
            throwInvalidParams(QStringLiteral("new_name must not be empty."));
        MyWidget *w = findWindowByName(d_app, oldName);
        if (!w)
            throwInvalidParams(QStringLiteral("Window not found: %1").arg(oldName));
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
        const QString name = params.value(QStringLiteral("name")).toString().trimmed();
        MyWidget *w = findWindowByName(d_app, name);
        if (!w)
            throwInvalidParams(QStringLiteral("Window not found: %1").arg(name));
        const QString closedName = w->name();
        w->askOnCloseEvent(false);
        w->hide();
        w->close();
        if (w->folder())
            d_app->closeWindow(w);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("closed")] = closedName;
        return true;
    }

    if (method == QStringLiteral("app.clone_window")) {
        const QString name = params.value(QStringLiteral("name")).toString().trimmed();
        MyWidget *w = findWindowByName(d_app, name);
        if (!w)
            throwInvalidParams(QStringLiteral("Window not found: %1").arg(name));
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
        QString lang = params.value(QStringLiteral("language")).toString().trimmed().toLower();
        if (lang.isEmpty())
            lang = params.value(QStringLiteral("lang")).toString().trimmed().toLower();
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
        const QString m = params.value(QStringLiteral("mode")).toString().trimmed().toLower();
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

    if (method == QStringLiteral("app.recalculate")) {
        RecalcManager *rm = RecalcManager::instance(d_app);
        QVector<Table *> allTables;
        for (MyWidget *w : d_app->windowsList()) {
            if (w && w->inherits("Table"))
                allTables.append(static_cast<Table *>(w));
        }
        const int maxPasses = std::max(2, allTables.size() + 2);
        for (int pass = 0; pass < maxPasses; ++pass) {
            bool anyDelta = false;
            for (Table *t : allTables) {
                for (int c = 0; c < t->numCols(); ++c) {
                    Column *col = t->column(c);
                    if (!col || col->formula(0).trimmed().isEmpty())
                        continue;
                    const int nRows = t->numRows();
                    std::vector<double> beforeVals(static_cast<size_t>(std::max(0, nRows)), 0.0);
                    std::vector<unsigned char> beforeInv(static_cast<size_t>(std::max(0, nRows)), 0);
                    for (int r = 0; r < nRows; ++r) {
                        beforeInv[static_cast<size_t>(r)] = col->isInvalid(r) ? 1 : 0;
                        if (!col->isInvalid(r)
                            && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                            beforeVals[static_cast<size_t>(r)] = col->valueAt(r);
                        }
                    }
                    t->recalculate(c, false);
                    for (int r = 0; r < t->numRows(); ++r) {
                        const unsigned char nowInv = col->isInvalid(r) ? 1 : 0;
                        if (r >= nRows || nowInv != beforeInv[static_cast<size_t>(r)]) {
                            anyDelta = true;
                            break;
                        }
                        if (!nowInv && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                            const double nv = col->valueAt(r);
                            const double ov = beforeVals[static_cast<size_t>(r)];
                            if ((std::isnan(nv) != std::isnan(ov))
                                || (!std::isnan(nv) && std::abs(nv - ov) > 1e-15)) {
                                anyDelta = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (!anyDelta)
                break;
        }
        for (Table *t : allTables) {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && !t->column(c)->formula(0).trimmed().isEmpty())
                    d_app->updateCurves(t, fullColumnName(t, c));
            }
        }
        const int updatedLinks = rm->recalculateAll();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("updated_links")] = updatedLinks;
        outResult[QStringLiteral("tables_checked")] = allTables.size();
        return true;
    }

    if (method == QStringLiteral("app.set_focus_hud")) {
        const bool enabled = params.contains(QStringLiteral("enabled"))
                ? params.value(QStringLiteral("enabled")).toBool()
                : !focusHudFallback;
        focusHudFallback = enabled;
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("enabled")] = focusHudFallback;
        return true;
    }

    if (method == QStringLiteral("app.get_focus_hud")) {
        outResult[QStringLiteral("enabled")] = focusHudFallback;
        return true;
    }

    if (method == QStringLiteral("app.set_workspace_mode")) {
        const QString mode = params.value(QStringLiteral("mode")).toString().trimmed().toLower();
        if (mode == QStringLiteral("tabs") || mode == QStringLiteral("tabbed")) {
            d_app->setTabbedWorkspaceView(true);
            workspaceModeFallback = QStringLiteral("tabs");
        } else if (mode == QStringLiteral("mdi") || mode == QStringLiteral("free")) {
            d_app->setTabbedWorkspaceView(false);
            workspaceModeFallback = QStringLiteral("mdi");
        } else if (mode == QStringLiteral("split")) {
            d_app->setTabbedWorkspaceView(false);
            d_app->d_workspace.tileSubWindows();
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

    if (method == QStringLiteral("app.check_updates")) {
        outResult = checkUpdatesBlockingFallback(params);
        return true;
    }

    if (method == QStringLiteral("app.list_actions")) {
        const QString query = params.value(QStringLiteral("filter")).toString().trimmed();
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
        const QString name = params.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty())
            throwInvalidParams(QStringLiteral("Action or slot 'name' is required."));

        auto isBlockedInteractiveDialogTarget = [](const QString &ident,
                                                   const QString &menuText = QString()) -> bool {
            const QString id = ident.trimmed();
            if (id == QStringLiteral("openPythonConsole")
                || id == QStringLiteral("actionPythonConsole")) {
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
            };
            if (kBlockedInteractiveTargets.contains(id))
                return true;
            if (id.isEmpty()
                && (menuText.endsWith(QStringLiteral("..."))
                    || menuText.endsWith(QStringLiteral("\u2026")))) {
                return true;
            }
            return false;
        };

        const bool nonInteractive = d_app->isNonInteractive()
                || ApplicationWindow::isNonInteractiveContext(d_app);

        const QList<QAction *> actions = d_app->findChildren<QAction *>();
        for (QAction *act : actions) {
            if (!act)
                continue;
            const QString cleanTxt = act->text().remove(QLatin1Char('&')).trimmed();
            if (act->objectName() == name || cleanTxt == name) {
                if (nonInteractive
                    && isBlockedInteractiveDialogTarget(act->objectName(), cleanTxt)) {
                    throwRpcRuntime(
                            QStringLiteral("Interactive modal dialog action '%1' cannot be triggered headlessly via RPC; use the dedicated JSON-RPC method instead.")
                                    .arg(act->objectName().isEmpty() ? cleanTxt : act->objectName()));
                }
                if (params.contains(QStringLiteral("checked")) && act->isCheckable()) {
                    const bool want = params.value(QStringLiteral("checked")).toBool();
                    if (act->isChecked() != want)
                        act->trigger();
                } else {
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
                if (nonInteractive && isBlockedInteractiveDialogTarget(name)) {
                    throwRpcRuntime(
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
    // 2. Worksheet (table.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("table.create")) {
        const int rows = parseStrictPositiveInt(params, QStringLiteral("rows"), 30);
        const int cols = parseStrictPositiveInt(params, QStringLiteral("cols"), 2);
        const QString reqName = params.value(QStringLiteral("name")).toString();
        Table *t = createUniqueTableHeadless(d_app, rows, cols, reqName);
        if (params.contains(QStringLiteral("columns"))
            && params.value(QStringLiteral("columns")).isArray()) {
            const QJsonArray cArr = params.value(QStringLiteral("columns")).toArray();
            for (int c = 0; c < std::min(cols, cArr.size()); ++c) {
                if (cArr[c].isString())
                    t->setColName(c, cArr[c].toString());
                else if (cArr[c].isObject())
                    applyColumnMetadataObject(t, c, cArr[c].toObject());
            }
        }
        if (params.contains(QStringLiteral("col_names"))
            && params.value(QStringLiteral("col_names")).isArray()) {
            const QJsonArray nArr = params.value(QStringLiteral("col_names")).toArray();
            for (int c = 0; c < std::min(cols, nArr.size()); ++c) {
                const QString nm = nArr[c].toString().trimmed();
                if (!nm.isEmpty())
                    t->setColName(c, nm);
            }
        }
        if (params.contains(QStringLiteral("designations"))) {
            const QJsonValue dv = params.value(QStringLiteral("designations"));
            if (dv.isArray()) {
                const QJsonArray dArr = dv.toArray();
                for (int c = 0; c < std::min(cols, dArr.size()); ++c) {
                    QJsonObject mo;
                    mo[QStringLiteral("plot_designation")] = dArr[c].toString();
                    applyColumnMetadataObject(t, c, mo);
                }
            } else if (dv.isObject()) {
                const QJsonObject dObj = dv.toObject();
                for (auto it = dObj.begin(); it != dObj.end(); ++it) {
                    const int c = resolveColumnIndex(t, QJsonValue(it.key()), false);
                    if (c >= 0) {
                        QJsonObject mo;
                        mo[QStringLiteral("plot_designation")] = it.value().toString();
                        applyColumnMetadataObject(t, c, mo);
                    }
                }
            }
        }
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
        if (params.contains(QStringLiteral("rows")) && !params.value(QStringLiteral("rows")).isNull())
            t->setNumRows(parseStrictPositiveInt(params, QStringLiteral("rows"), t->numRows()));
        if (params.contains(QStringLiteral("cols")) && !params.value(QStringLiteral("cols")).isNull())
            t->setNumCols(parseStrictPositiveInt(params, QStringLiteral("cols"), t->numCols()));
        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("table.add_column")) {
        Table *t = resolveTable(d_app, params);
        const SciDAVis::PlotDesignation pd = parsePlotDesignation(
                params.value(QStringLiteral("plot_designation")).toString(), SciDAVis::Y);
        t->addCol(pd);
        const int newIdx = t->numCols() - 1;
        applyColumnMetadataObject(t, newIdx, params);
        const QJsonValue initData = params.contains(QStringLiteral("data"))
                ? params.value(QStringLiteral("data"))
                : params.value(QStringLiteral("values"));
        if (initData.isArray()) {
            writeBulkColumnArray(t, newIdx, initData.toArray(),
                                 params.value(QStringLiteral("mode")).toString());
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
        writeBulkColumnArray(t, idx, arr, params.value(QStringLiteral("mode")).toString());
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
        QJsonArray dataArr;
        QJsonArray maskArr;
        for (int r = 0; r < n; ++r) {
            maskArr.append(col ? col->isMasked(r) : false);
            if (!col || col->isInvalid(r)) {
                dataArr.append(QJsonValue::Null);
            } else if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                dataArr.append(safeJsonDouble(col->valueAt(r)));
            } else {
                dataArr.append(col->textAt(r));
            }
        }
        outResult = serializeColumnMeta(t, idx);
        outResult[QStringLiteral("table")] = t->name();
        outResult[QStringLiteral("rows")] = n;
        outResult[QStringLiteral("data")] = dataArr;
        outResult[QStringLiteral("values")] = dataArr;
        outResult[QStringLiteral("masked")] = maskArr;
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

        Table *t = resolveTable(d_app, params, QStringLiteral("table"), false);
        if (!t) {
            t = createUniqueTableHeadless(d_app, maxRows, keys.size(),
                                          params.value(QStringLiteral("name")).toString());
        } else {
            if (t->numCols() < keys.size())
                t->setNumCols(keys.size());
            if (t->numRows() < maxRows)
                t->setNumRows(maxRows);
        }

        const QJsonObject metaMap = params.value(QStringLiteral("meta")).toObject();
        const QJsonObject unitsMap = params.value(QStringLiteral("units")).toObject();
        const QJsonObject longNamesMap = params.value(QStringLiteral("long_names")).toObject();
        const QJsonObject commentsMap = params.value(QStringLiteral("comments")).toObject();
        const QJsonObject desigMap = params.value(QStringLiteral("designations")).toObject();
        for (int c = 0; c < keys.size(); ++c) {
            const QString colKey = keys[c];
            t->setColName(c, colKey);
            t->setColPlotDesignation(c, (c == 0) ? SciDAVis::X : SciDAVis::Y);
            if (dataDict.value(colKey).isArray()) {
                writeBulkColumnArray(t, c, dataDict.value(colKey).toArray(), QString());
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

    if (method == QStringLiteral("table.set_cell")) {
        Table *t = resolveTable(d_app, params);
        const int row = params.value(QStringLiteral("row")).toInt(-1);
        const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        if (row < 0)
            throwInvalidParams(QStringLiteral("Row index must be >= 0."));
        if (row >= t->numRows())
            t->setNumRows(row + 1);
        Column *col = t->column(colIdx);
        const QJsonValue val = params.value(QStringLiteral("value"));
        if (val.isNull() || val.isUndefined()) {
            t->clearCell(row, colIdx);
        } else if (col && col->columnMode() == SciDAVis::ColumnMode::Text) {
            col->setTextAt(row, val.isString() ? val.toString()
                                               : QString::number(val.toDouble(), 'g', 15));
        } else if (val.isString()) {
            bool ok = false;
            const double d = val.toString().toDouble(&ok);
            if (ok && std::isfinite(d)) {
                t->setCell(row, colIdx, d);
            } else {
                if (col && col->columnMode() != SciDAVis::ColumnMode::Text)
                    col->setColumnMode(SciDAVis::ColumnMode::Text);
                t->setText(row, colIdx, val.toString());
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
        const int row = params.value(QStringLiteral("row")).toInt(-1);
        const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
        if (row < 0 || row >= t->numRows())
            throwInvalidParams(QStringLiteral("Row index out of range: %1").arg(row));
        Column *col = t->column(colIdx);
        outResult[QStringLiteral("row")] = row;
        outResult[QStringLiteral("col")] = colIdx;
        outResult[QStringLiteral("masked")] = col ? col->isMasked(row) : false;
        outResult[QStringLiteral("invalid")] = col ? col->isInvalid(row) : true;
        if (!col || col->isInvalid(row)) {
            outResult[QStringLiteral("value")] = QJsonValue::Null;
        } else if (col->columnMode() == SciDAVis::ColumnMode::Numeric) {
            outResult[QStringLiteral("value")] = safeJsonDouble(col->valueAt(row));
        } else {
            outResult[QStringLiteral("value")] = col->textAt(row);
        }
        outResult[QStringLiteral("text")] = t->text(row, colIdx);
        return true;
    }

    if (method == QStringLiteral("table.set_column_meta")) {
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
        const QString formula = params.value(QStringLiteral("formula")).toString();
        t->setCommand(colIdx, formula);
        const bool doRecalc = params.contains(QStringLiteral("recalculate"))
                ? params.value(QStringLiteral("recalculate")).toBool(true)
                : true;
        bool okRecalc = true;
        if (doRecalc && !formula.trimmed().isEmpty()) {
            okRecalc = t->recalculate(colIdx, false);
            d_app->updateCurves(t, fullColumnName(t, colIdx));
        }
        outResult[QStringLiteral("ok")] = okRecalc;
        outResult[QStringLiteral("col")] = t->colLabel(colIdx);
        outResult[QStringLiteral("formula")] = formula;
        return true;
    }

    if (method == QStringLiteral("table.recalculate")) {
        Table *t = resolveTable(d_app, params);
        bool allOk = true;
        if (params.contains(QStringLiteral("col")) && !params.value(QStringLiteral("col")).isNull()) {
            const int colIdx = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
            allOk = t->recalculate(colIdx, false);
            d_app->updateCurves(t, fullColumnName(t, colIdx));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && !t->column(c)->formula(0).trimmed().isEmpty()) {
                    allOk = t->recalculate(c, false) && allOk;
                    d_app->updateCurves(t, fullColumnName(t, c));
                }
            }
        }
        outResult[QStringLiteral("ok")] = allOk;
        outResult[QStringLiteral("table")] = t->name();
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
        for (int c : targetCols) {
            if (Column *col = t->column(c)) {
                for (int r : rowsToChange)
                    col->setMasked(r, maskFlag);
                d_app->updateCurves(t, fullColumnName(t, c));
            }
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("modified_rows")] = rowsToChange.size();
        return true;
    }

    if (method == QStringLiteral("table.clear_masks")) {
        Table *t = resolveTable(d_app, params);
        if (params.contains(QStringLiteral("col")) && !params.value(QStringLiteral("col")).isNull()) {
            const int c = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
            if (t->column(c))
                t->column(c)->clearMasks();
            d_app->updateCurves(t, fullColumnName(t, c));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c))
                    t->column(c)->clearMasks();
                d_app->updateCurves(t, fullColumnName(t, c));
            }
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
                const int r = rv.toInt(-1);
                if (r >= 0 && r < t->numRows())
                    targetRows.append(r);
            }
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

        auto quoteField = [&sep](const QString &field) -> QString {
            const bool mustQuote = (!sep.isEmpty() && field.contains(sep))
                    || field.contains(QLatin1Char('"')) || field.contains(QLatin1Char('\n'))
                    || field.contains(QLatin1Char('\r'));
            if (!mustQuote)
                return field;
            QString escaped = field;
            escaped.replace(QLatin1String("\""), QLatin1String("\"\""));
            return QLatin1Char('"') + escaped + QLatin1Char('"');
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
                    rowFields << (std::isfinite(v) ? cLoc.toString(v, 'g', 15) : QString());
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
        Matrix *m = createUniqueMatrixHeadless(
                d_app, rows, cols, params.value(QStringLiteral("name")).toString());
        for (int c = 0; c < cols; ++c) {
            Column *col = t->column(c);
            QVector<qreal> colVec(rows, 0.0);
            for (int r = 0; r < rows; ++r) {
                if (col && !col->isInvalid(r)
                    && col->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    const double v = col->valueAt(r);
                    colVec[r] = std::isfinite(v) ? v : 0.0;
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
        const int rows = parseStrictPositiveInt(params, QStringLiteral("rows"), 32);
        const int cols = parseStrictPositiveInt(params, QStringLiteral("cols"), 32);
        Matrix *m = createUniqueMatrixHeadless(
                d_app, rows, cols, params.value(QStringLiteral("name")).toString());
        if (params.contains(QStringLiteral("x_min")) || params.contains(QStringLiteral("x_max"))
            || params.contains(QStringLiteral("y_min"))
            || params.contains(QStringLiteral("y_max"))) {
            const double xMin = params.value(QStringLiteral("x_min")).toDouble(m->xStart());
            const double xMax = params.value(QStringLiteral("x_max")).toDouble(m->xEnd());
            const double yMin = params.value(QStringLiteral("y_min")).toDouble(m->yStart());
            const double yMax = params.value(QStringLiteral("y_max")).toDouble(m->yEnd());
            m->setCoordinates(xMin, xMax, yMin, yMax);
        }
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
        const double xMin = params.value(QStringLiteral("x_min")).toDouble(m->xStart());
        const double xMax = params.value(QStringLiteral("x_max")).toDouble(m->xEnd());
        const double yMin = params.value(QStringLiteral("y_min")).toDouble(m->yStart());
        const double yMax = params.value(QStringLiteral("y_max")).toDouble(m->yEnd());
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
            if (rv.isArray())
                nCols = std::max(nCols, rv.toArray().size());
        }
        if (nRows <= 0 || nCols <= 0)
            throwInvalidParams(QStringLiteral("Matrix 'data' must be non-empty 2D array."));
        if (m->numRows() != nRows || m->numCols() != nCols)
            m->setDimensions(nRows, nCols);

        for (int c = 0; c < nCols; ++c) {
            QVector<qreal> colVec(nRows, 0.0);
            for (int r = 0; r < nRows; ++r) {
                const QJsonArray rArr = rowsArr[r].toArray();
                if (c < rArr.size()) {
                    const double d = jsonToDoubleOrNaN(rArr[c]);
                    colVec[r] = std::isfinite(d) ? d : std::numeric_limits<double>::quiet_NaN();
                }
            }
            m->d_future_matrix->setColumnCells(c, 0, nRows - 1, colVec);
        }
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
        const QString formula = params.value(QStringLiteral("formula")).toString();
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
        if (mode == QStringLiteral("h") || mode == QStringLiteral("horizontal")) {
            m->d_future_matrix->mirrorHorizontally();
        } else if (mode == QStringLiteral("v") || mode == QStringLiteral("vertical")) {
            m->d_future_matrix->mirrorVertically();
        } else if (mode == QStringLiteral("rot90_cw") || mode == QStringLiteral("cw")) {
            m->transpose();
            m->d_future_matrix->mirrorHorizontally();
        } else if (mode == QStringLiteral("rot90_ccw") || mode == QStringLiteral("ccw")) {
            m->transpose();
            m->d_future_matrix->mirrorVertically();
        } else {
            throwInvalidParams(
                    QStringLiteral("Unknown flip mode '%1' (expected 'h'|'v'|'rot90_cw'|'rot90_ccw').")
                            .arg(mode));
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
        const QString fStr = params.value(QStringLiteral("method")).toString().trimmed().toLower();
        AdvancedSignalProc::MatrixFilter2DMethod fm = AdvancedSignalProc::FilterGaussian2D;
        if (fStr == QStringLiteral("gaussian") || fStr == QStringLiteral("gauss"))
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
                ? params.value(QStringLiteral("sigma")).toDouble(1.5)
                : params.value(QStringLiteral("param")).toDouble(1.5);
        const int radius = std::max(1, params.value(QStringLiteral("radius")).toInt(1));
        const bool inPlace = params.value(QStringLiteral("in_place")).toBool(false);
        const bool plotHeatmap = params.value(QStringLiteral("plot_heatmap")).toBool(false);
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
        const int maxPeaks = parseStrictPositiveInt(params, QStringLiteral("max_peaks"), 10);
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
        for (int c = 0; c < cols; ++c) {
            QVector<qreal> colVec(rows, 0.0);
            for (int r = 0; r < rows; ++r)
                colVec[r] = m->cell(r, c);
            t->column(c)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            t->column(c)->replaceValues(0, colVec);
            t->setColPlotDesignation(c, SciDAVis::Y);
        }
        outResult = serializeTableInfo(t);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    // ------------------------------------------------------------------------
    // 4. 2D / 3D / Matrix Plotting (plot.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("plot.xy") || method == QStringLiteral("plot.stacked_bars")) {
        Table *t = resolveTable(d_app, params);
        int xIdx = -1;
        if (params.contains(QStringLiteral("x_col")) && !params.value(QStringLiteral("x_col")).isNull()) {
            xIdx = resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true);
            t->setColPlotDesignation(xIdx, SciDAVis::X);
        } else {
            xIdx = t->firstXCol();
            if (xIdx < 0 && t->numCols() >= 2) {
                xIdx = 0;
                t->setColPlotDesignation(0, SciDAVis::X);
            }
        }

        QVector<int> yIdxs;
        QStringList yFullCols;
        if (params.contains(QStringLiteral("y_cols")) && !params.value(QStringLiteral("y_cols")).isNull()) {
            const QJsonValue yv = params.value(QStringLiteral("y_cols"));
            if (yv.isArray()) {
                for (const QJsonValue &it : yv.toArray()) {
                    const int yi = resolveColumnIndex(t, it, true);
                    if (t->colPlotDesignation(yi) == SciDAVis::noDesignation
                        || t->colPlotDesignation(yi) == SciDAVis::X)
                        t->setColPlotDesignation(yi, SciDAVis::Y);
                    yIdxs.append(yi);
                    yFullCols << fullColumnName(t, yi);
                }
            } else {
                const int yi = resolveColumnIndex(t, yv, true);
                if (t->colPlotDesignation(yi) == SciDAVis::noDesignation
                    || t->colPlotDesignation(yi) == SciDAVis::X)
                    t->setColPlotDesignation(yi, SciDAVis::Y);
                yIdxs.append(yi);
                yFullCols << fullColumnName(t, yi);
            }
        } else if (params.contains(QStringLiteral("y_col"))
                   && !params.value(QStringLiteral("y_col")).isNull()) {
            const int yi = resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true);
            t->setColPlotDesignation(yi, SciDAVis::Y);
            yIdxs.append(yi);
            yFullCols << fullColumnName(t, yi);
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != xIdx && t->colPlotDesignation(c) == SciDAVis::Y) {
                    yIdxs.append(c);
                    yFullCols << fullColumnName(t, c);
                }
            }
            if (yFullCols.isEmpty() && t->numCols() >= 2) {
                const int yi = (xIdx == 0) ? 1 : 0;
                t->setColPlotDesignation(yi, SciDAVis::Y);
                yIdxs.append(yi);
                yFullCols << fullColumnName(t, yi);
            }
        }
        if (yFullCols.isEmpty())
            throwInvalidParams(QStringLiteral("No Y columns specified or available in table '%1'.")
                                       .arg(t->name()));

        const QString styleRaw = params.value(QStringLiteral("style")).toString().trimmed().toLower();
        const bool isStackedBars = (method == QStringLiteral("plot.stacked_bars"))
                || styleRaw == QStringLiteral("stacked_bars")
                || styleRaw == QStringLiteral("stacked_bar")
                || styleRaw == QStringLiteral("stacked_column")
                || styleRaw == QStringLiteral("stack_bar")
                || styleRaw == QStringLiteral("stack_column")
                || styleRaw == QStringLiteral("stacked_horizontal_bars")
                || styleRaw == QStringLiteral("stacked_hbars");

        if (isStackedBars) {
            const int nRows = t->numRows();
            if (nRows < 1)
                throwInvalidParams(QStringLiteral("Stacked bar plot requires at least 1 row."));
            int barGap = 25;
            if (params.contains(QStringLiteral("bar_gap"))
                && !params.value(QStringLiteral("bar_gap")).isNull()) {
                barGap = parseStrictJsonInt(params.value(QStringLiteral("bar_gap")),
                                            QStringLiteral("bar_gap"));
                if (barGap < 0 || barGap >= 100)
                    throwInvalidParams(QStringLiteral("bar_gap must be in [0, 99]."));
            }
            int barOffset = 0;
            if (params.contains(QStringLiteral("bar_offset"))
                && !params.value(QStringLiteral("bar_offset")).isNull()) {
                barOffset = parseStrictJsonInt(params.value(QStringLiteral("bar_offset")),
                                               QStringLiteral("bar_offset"));
            }
            const bool horiz = styleRaw.contains(QStringLiteral("horiz"))
                    || styleRaw == QStringLiteral("stacked_hbars")
                    || params.value(QStringLiteral("horizontal")).toBool(false);

            const int K = yIdxs.size();
            QList<Column *> stCols;
            auto *colX = new Column(QStringLiteral("X"), SciDAVis::ColumnMode::Numeric);
            colX->setPlotDesignation(SciDAVis::X);
            for (int r = 0; r < nRows; ++r) {
                double xv = double(r + 1);
                if (xIdx >= 0 && t->column(xIdx) && !t->column(xIdx)->isInvalid(r)) {
                    const double cand = t->cell(r, xIdx);
                    if (std::isfinite(cand))
                        xv = cand;
                }
                colX->setValueAt(r, xv);
            }
            stCols << colX;

            QVector<Column *> cumCols(K, nullptr);
            for (int k = 0; k < K; ++k) {
                QString lbl = t->colLabel(yIdxs[k]);
                if (lbl.isEmpty())
                    lbl = QStringLiteral("Series_%1").arg(k + 1);
                cumCols[k] = new Column(lbl, SciDAVis::ColumnMode::Numeric);
                cumCols[k]->setPlotDesignation(SciDAVis::Y);
                stCols << cumCols[k];
            }

            for (int r = 0; r < nRows; ++r) {
                double posCum = 0.0;
                double negCum = 0.0;
                for (int k = 0; k < K; ++k) {
                    double yv = 0.0;
                    if (t->column(yIdxs[k]) && !t->column(yIdxs[k])->isInvalid(r)
                        && !t->column(yIdxs[k])->isMasked(r)) {
                        const double cand = t->cell(r, yIdxs[k]);
                        if (std::isfinite(cand))
                            yv = cand;
                    }
                    if (yv >= 0.0) {
                        posCum += yv;
                        cumCols[k]->setValueAt(r, posCum);
                    } else {
                        negCum += yv;
                        cumCols[k]->setValueAt(r, negCum);
                    }
                }
            }

            const QString stTblName = d_app->generateUniqueName(QStringLiteral("StackedBars"));
            Table *stTbl = d_app->newHiddenTable(stTblName, QStringLiteral("StackedBars"), stCols);
            if (!stTbl) {
                qDeleteAll(stCols);
                throwRpcRuntime(QStringLiteral("Failed to create backing table for stacked bars."));
            }

            MultiLayer *ml = d_app->newGraph(d_app->generateUniqueName(QStringLiteral("Graph")));
            if (!ml)
                throwRpcRuntime(QStringLiteral("Failed to create Stacked Bars graph."));
            Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->addLayer();

            static const char *const kOkabeIto[8] = {
                "#0072B2", "#E69F00", "#009E73", "#D55E00",
                "#CC79A7", "#56B4E9", "#F0E442", "#333333"
            };
            const int barCurveType = horiz ? Graph::HorizontalBars : Graph::VerticalBars;
            // Insert from top stack (K-1) down to base stack (0) so lower cumulative bars paint in front
            for (int k = K - 1; k >= 0; --k) {
                g->insertCurve(stTbl, stTbl->colName(0), stTbl->colName(k + 1), barCurveType, 0,
                               nRows - 1);
                const int ci = g->curves() - 1;
                if (ci >= 0) {
                    const QColor fillCol(QString::fromLatin1(kOkabeIto[k % 8]));
                    g->setCurvePen(ci, QPen(QColor(Qt::black), 0.8, Qt::SolidLine));
                    g->setCurveBrush(ci, QBrush(fillCol, Qt::SolidPattern));
                    g->setBarsGap(ci, barGap, barOffset);
                }
            }
            if (xIdx >= 0 && t->column(xIdx)) {
                const QString xt = t->column(xIdx)->formattedAxisTitle().trimmed();
                if (!xt.isEmpty())
                    g->setAxisTitle(horiz ? QwtPlot::yLeft : QwtPlot::xBottom, xt);
            }
            if (!yIdxs.isEmpty() && t->column(yIdxs.first())) {
                const QString yt = t->column(yIdxs.first())->formattedAxisTitle().trimmed();
                if (!yt.isEmpty())
                    g->setAxisTitle(horiz ? QwtPlot::xBottom : QwtPlot::yLeft, yt);
            }
            g->reconstructLegend();
            g->setAutoScale();
            enforceArialGraphFonts(g);
            g->replot();
            applyRequestedWindowName(d_app, ml, params);

            outResult = serializeGraphInfo(ml);
            outResult[QStringLiteral("stacked")] = true;
            outResult[QStringLiteral("bar_gap")] = barGap;
            outResult[QStringLiteral("ok")] = true;
            return true;
        }

        const int style = parsePlotStyle(params.value(QStringLiteral("style")).toString(),
                                         Graph::LineSymbols);
        MultiLayer *ml = d_app->multilayerPlot(t, yFullCols, style, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create 2D XY plot."));

        applyRequestedWindowName(d_app, ml, params);

        Graph *g = ml->activeGraph();
        if (g) {
            if (params.contains(QStringLiteral("bar_gap"))
                || params.contains(QStringLiteral("bar_offset"))) {
                int barGap = 0;
                int barOffset = 0;
                const bool hasGap = params.contains(QStringLiteral("bar_gap"))
                        && !params.value(QStringLiteral("bar_gap")).isNull();
                const bool hasOff = params.contains(QStringLiteral("bar_offset"))
                        && !params.value(QStringLiteral("bar_offset")).isNull();
                if (hasGap) {
                    barGap = parseStrictJsonInt(params.value(QStringLiteral("bar_gap")),
                                                QStringLiteral("bar_gap"));
                    if (barGap < 0 || barGap >= 100)
                        throwInvalidParams(QStringLiteral("bar_gap must be in [0, 99]."));
                }
                if (hasOff) {
                    barOffset = parseStrictJsonInt(params.value(QStringLiteral("bar_offset")),
                                                   QStringLiteral("bar_offset"));
                }
                for (int ci = 0; ci < g->curves(); ++ci) {
                    if (auto *bc = dynamic_cast<QwtBarCurve *>(g->curve(ci))) {
                        g->setBarsGap(ci, hasGap ? barGap : bc->gap(),
                                      hasOff ? barOffset : bc->offset());
                    }
                }
            }
            const QString xFull = (xIdx >= 0) ? fullColumnName(t, xIdx) : QString();
            if (params.contains(QStringLiteral("y_err_col"))
                && !params.value(QStringLiteral("y_err_col")).isNull()) {
                const int yeIdx = resolveColumnIndex(t, params.value(QStringLiteral("y_err_col")), true);
                if (!xFull.isEmpty())
                    g->addErrorBars(xFull, yFullCols.first(), t, fullColumnName(t, yeIdx), 1);
                else
                    g->addErrorBars(yFullCols.first(), t, fullColumnName(t, yeIdx), 1);
            }
            if (params.contains(QStringLiteral("x_err_col"))
                && !params.value(QStringLiteral("x_err_col")).isNull()) {
                const int xeIdx = resolveColumnIndex(t, params.value(QStringLiteral("x_err_col")), true);
                if (!xFull.isEmpty())
                    g->addErrorBars(xFull, yFullCols.first(), t, fullColumnName(t, xeIdx), 0);
            }
            g->refreshAutoAxisTitlesFromColumns();
            enforceArialGraphFonts(g);
            g->replot();
        }
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.add_curve")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        Table *t = resolveTable(d_app, params);
        int xIdx = params.contains(QStringLiteral("x_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true)
                : t->firstXCol();
        const int yIdx = resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true);
        const int style = parsePlotStyle(params.value(QStringLiteral("style")).toString(),
                                         Graph::LineSymbols);
        bool ok = false;
        if (xIdx >= 0) {
            ok = g->insertCurve(t, fullColumnName(t, xIdx), fullColumnName(t, yIdx), style, 0,
                                t->numRows() - 1);
        } else {
            ok = g->insertCurve(t, fullColumnName(t, yIdx), style);
        }
        if (!ok)
            throwRpcRuntime(QStringLiteral("Failed to add curve to graph '%1'.").arg(ml->name()));
        if (params.value(QStringLiteral("right_axis")).toBool(false) && g->curves() > 0) {
            if (QwtPlotCurve *c = g->curve(g->curves() - 1)) {
                c->setYAxis(QwtPlot::yRight);
                g->enableAxis(QwtPlot::yRight, true);
            }
        }
        g->setAutoScale();
        enforceArialGraphFonts(g);
        g->replot();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.remove_curve")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString title = resolveCurveTitle(g, params, QStringLiteral("curve"), true);
        g->removeCurve(title);
        g->replot();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.add_error_bars")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        Table *t = resolveTable(d_app, params);
        const QString curveTitle = resolveCurveTitle(g, params, QStringLiteral("curve"), true);
        const int errIdx = resolveColumnIndex(t, params.value(QStringLiteral("err_col")), true);
        const QString dirStr = params.value(QStringLiteral("direction")).toString().toLower();
        const int type = (dirStr == QStringLiteral("x") || dirStr == QStringLiteral("horizontal"))
                ? 0
                : 1;
        const QColor col = parseColorValue(params.value(QStringLiteral("color")), QColor(Qt::black));
        const int width = std::max(1, params.value(QStringLiteral("width")).toInt(1));
        const int cap = std::max(0, params.value(QStringLiteral("cap")).toInt(8));
        bool ok = g->addErrorBars(curveTitle, t, fullColumnName(t, errIdx), type, width, cap, col);
        g->replot();
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("plot.double_y")) {
        Table *t = resolveTable(d_app, params);
        int xIdx = params.contains(QStringLiteral("x_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true)
                : t->firstXCol();
        if (xIdx < 0 && t->numCols() >= 3) {
            xIdx = 0;
            t->setColPlotDesignation(0, SciDAVis::X);
        } else if (xIdx >= 0) {
            t->setColPlotDesignation(xIdx, SciDAVis::X);
        }
        int y1Idx = params.contains(QStringLiteral("y1_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("y1_col")), true)
                : -1;
        int y2Idx = params.contains(QStringLiteral("y2_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("y2_col")), true)
                : -1;
        if (y1Idx < 0 || y2Idx < 0) {
            QList<int> yCands;
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != xIdx)
                    yCands << c;
            }
            if (yCands.size() < 2)
                throwInvalidParams(
                        QStringLiteral("Double-Y plot requires at least two Y columns."));
            if (y1Idx < 0)
                y1Idx = yCands[0];
            if (y2Idx < 0)
                y2Idx = yCands[1];
        }
        t->setColPlotDesignation(y1Idx, SciDAVis::Y);
        t->setColPlotDesignation(y2Idx, SciDAVis::Y);

        QStringList cols;
        cols << fullColumnName(t, y1Idx) << fullColumnName(t, y2Idx);
        MultiLayer *ml = d_app->multilayerPlot(t, cols, Graph::LineSymbols, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Double-Y plot."));
        Graph *g = ml->activeGraph();
        if (g && g->curves() >= 2) {
            QwtPlotCurve *cRight = g->curve(1);
            cRight->setYAxis(QwtPlot::yRight);
            g->enableAxis(QwtPlot::yRight, true);
            if (QwtScaleDraw *sd = g->plotWidget()->axisScaleDraw(QwtPlot::yRight)) {
                sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
                sd->enableComponent(QwtAbstractScaleDraw::Ticks, true);
            }
            QString rt = t->column(y2Idx) ? t->column(y2Idx)->formattedAxisTitle().trimmed()
                                          : t->colLabel(y2Idx);
            if (rt.isEmpty())
                rt = t->colLabel(y2Idx);
            g->setRightAxisTitle(rt);
            double yMin = std::numeric_limits<double>::infinity();
            double yMax = -std::numeric_limits<double>::infinity();
            for (int k = 0; k < int(cRight->dataSize()); ++k) {
                const double yv = cRight->y(k);
                if (std::isfinite(yv)) {
                    yMin = std::min(yMin, yv);
                    yMax = std::max(yMax, yv);
                }
            }
            if (yMin <= yMax) {
                double span = (yMax > yMin) ? (yMax - yMin) : 1.0;
                g->setScale(QwtPlot::yRight, yMin - 0.05 * span, yMax + 0.05 * span);
            }
            g->setAutoScale();
            enforceArialGraphFonts(g);
            g->replot();
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.waterfall")) {
        Table *t = resolveTable(d_app, params);
        int xIdx = params.contains(QStringLiteral("x_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true)
                : t->firstXCol();
        if (xIdx >= 0)
            t->setColPlotDesignation(xIdx, SciDAVis::X);
        QStringList yCols;
        if (params.value(QStringLiteral("y_cols")).isArray()) {
            for (const QJsonValue &it : params.value(QStringLiteral("y_cols")).toArray()) {
                const int yi = resolveColumnIndex(t, it, true);
                t->setColPlotDesignation(yi, SciDAVis::Y);
                yCols << fullColumnName(t, yi);
            }
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != xIdx && t->colPlotDesignation(c) == SciDAVis::Y)
                    yCols << fullColumnName(t, c);
            }
        }
        if (yCols.isEmpty())
            throwInvalidParams(QStringLiteral("Waterfall plot requires Y columns."));
        MultiLayer *ml = d_app->multilayerPlot(t, yCols, Graph::Line, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create waterfall plot."));
        Graph *g = ml->activeGraph();
        if (g) {
            const double yGap = params.value(QStringLiteral("y_offset_pct")).toDouble(15.0);
            const double xGap = params.value(QStringLiteral("x_offset_pct")).toDouble(5.0);
            const bool norm01 = params.value(QStringLiteral("normalize")).toBool(false);
            g->setStackOffsets(1, yGap, xGap, norm01);
            enforceArialGraphFonts(g);
            g->replot();
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.histogram") || method == QStringLiteral("plot.box")
        || method == QStringLiteral("plot.violin")) {
        Table *t = resolveTable(d_app, params);
        QStringList cols;
        if (params.value(QStringLiteral("cols")).isArray()) {
            for (const QJsonValue &it : params.value(QStringLiteral("cols")).toArray())
                cols << fullColumnName(t, resolveColumnIndex(t, it, true));
        } else if (params.contains(QStringLiteral("col"))) {
            cols << fullColumnName(t, resolveColumnIndex(t, params.value(QStringLiteral("col")), true));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->colPlotDesignation(c) == SciDAVis::Y)
                    cols << fullColumnName(t, c);
            }
            if (cols.isEmpty() && t->numCols() > 0)
                cols << fullColumnName(t, 0);
        }
        const int plotType = (method == QStringLiteral("plot.histogram")) ? Graph::Histogram
                                                                          : Graph::Box;
        MultiLayer *ml = d_app->multilayerPlot(t, cols, plotType, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create statistical plot."));
        Graph *g = ml->activeGraph();
        if (g) {
            if (method == QStringLiteral("plot.violin")) {
                for (int i = 0; i < g->curves(); ++i) {
                    if (auto *bc = dynamic_cast<BoxCurve *>(g->curve(i))) {
                        bc->setViolinMode(true);
                        bc->setShowDataPoints(
                                params.value(QStringLiteral("show_points")).toBool(true));
                    }
                }
            }
            enforceArialGraphFonts(g);
            g->replot();
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.pareto")) {
        Table *srcTable = resolveTable(d_app, params);
        const int yCol = params.contains(QStringLiteral("col"))
                ? resolveColumnIndex(srcTable, params.value(QStringLiteral("col")), true)
                : ((srcTable->numCols() >= 2) ? 1 : 0);
        QVector<double> vals;
        double totalSum = 0.0;
        for (int r = 0; r < srcTable->numRows(); ++r) {
            if (srcTable->column(yCol) && !srcTable->column(yCol)->isInvalid(r)
                && !srcTable->column(yCol)->isMasked(r)) {
                const double v = srcTable->cell(r, yCol);
                if (std::isfinite(v) && v >= 0.0) {
                    vals.append(v);
                    totalSum += v;
                }
            }
        }
        if (vals.size() < 2 || totalSum <= 0.0)
            throwInvalidParams(QStringLiteral(
                    "Pareto plot requires >= 2 non-negative numeric values with positive sum."));
        std::sort(vals.begin(), vals.end(), std::greater<double>());

        Table *pt = createUniqueTableHeadless(d_app, vals.size(), 3,
                                              srcTable->name() + QStringLiteral("Pareto"));
        pt->setColName(0, QStringLiteral("Rank"));
        pt->setColPlotDesignation(0, SciDAVis::X);
        pt->setColName(1, srcTable->colLabel(yCol) + QStringLiteral("_Sorted"));
        pt->setColPlotDesignation(1, SciDAVis::Y);
        pt->setColName(2, QStringLiteral("Cumulative_Pct"));
        pt->setColPlotDesignation(2, SciDAVis::Y);
        double runSum = 0.0;
        for (int i = 0; i < vals.size(); ++i) {
            runSum += vals[i];
            pt->setCell(i, 0, i + 1);
            pt->setCell(i, 1, vals[i]);
            pt->setCell(i, 2, (runSum / totalSum) * 100.0);
        }
        MultiLayer *ml = d_app->multilayerPlot(pt, QStringList() << pt->colName(1),
                                               Graph::VerticalBars, 0, vals.size() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Pareto plot."));
        if (Graph *g = ml->activeGraph()) {
            g->insertCurve(pt, pt->colName(0), pt->colName(2), Graph::LineSymbols, 0,
                           vals.size() - 1);
            if (g->curves() >= 2 && g->curve(g->curves() - 1)) {
                g->curve(g->curves() - 1)->setYAxis(QwtPlot::yRight);
                g->curve(g->curves() - 1)->setPen(QPen(QColor(213, 94, 0), 2.0));
            }
            g->enableAxis(QwtPlot::yRight, true);
            g->setScale(QwtPlot::yLeft, 0.0, totalSum * 1.05);
            g->setScale(QwtPlot::yRight, 0.0, 105.0, 20.0);
            enforceArialGraphFonts(g);
            g->replot();
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("data_table")] = pt->name();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.qq")) {
        Table *srcTable = resolveTable(d_app, params);
        const int yCol = params.contains(QStringLiteral("col"))
                ? resolveColumnIndex(srcTable, params.value(QStringLiteral("col")), true)
                : ((srcTable->numCols() >= 2) ? 1 : 0);
        QVector<double> vals;
        for (int r = 0; r < srcTable->numRows(); ++r) {
            if (srcTable->column(yCol) && !srcTable->column(yCol)->isInvalid(r)
                && !srcTable->column(yCol)->isMasked(r)) {
                const double v = srcTable->cell(r, yCol);
                if (std::isfinite(v))
                    vals.append(v);
            }
        }
        const int n = vals.size();
        if (n < 3)
            throwInvalidParams(QStringLiteral("Normal Q-Q plot requires >= 3 numeric values."));
        std::sort(vals.begin(), vals.end());
        double sum = 0.0;
        for (double v : vals)
            sum += v;
        const double mean = sum / double(n);
        double varSum = 0.0;
        for (double v : vals)
            varSum += (v - mean) * (v - mean);
        const double sd = (n > 1) ? std::sqrt(varSum / double(n - 1)) : 1.0;

        Table *qt = createUniqueTableHeadless(d_app, n, 3, srcTable->name() + QStringLiteral("QQ"));
        qt->setColName(0, QStringLiteral("Theoretical_Quantile"));
        qt->setColPlotDesignation(0, SciDAVis::X);
        qt->setColName(1, QStringLiteral("Sample_Quantile"));
        qt->setColPlotDesignation(1, SciDAVis::Y);
        qt->setColName(2, QStringLiteral("Normal_Ref_Line"));
        qt->setColPlotDesignation(2, SciDAVis::Y);
        for (int i = 0; i < n; ++i) {
            const double p = (i + 1.0 - 0.375) / (n + 0.25);
            const double z = standardNormalInvCdf(p);
            qt->setCell(i, 0, z);
            qt->setCell(i, 1, vals[i]);
            qt->setCell(i, 2, mean + sd * z);
        }
        MultiLayer *ml = d_app->multilayerPlot(qt, QStringList() << qt->colName(1), Graph::Scatter,
                                               0, n - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Q-Q plot."));
        if (Graph *g = ml->activeGraph()) {
            g->insertCurve(qt, qt->colName(0), qt->colName(2), Graph::Line, 0, n - 1);
            if (g->curves() >= 2 && g->curve(g->curves() - 1))
                g->curve(g->curves() - 1)->setPen(QPen(QColor(213, 94, 0), 2.0, Qt::DashLine));
            g->setAutoScale();
            enforceArialGraphFonts(g);
            g->replot();
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("data_table")] = qt->name();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.pie")) {
        Table *t = resolveTable(d_app, params);
        const int colIdx = params.contains(QStringLiteral("col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("col")), true)
                : ((t->numCols() >= 2) ? 1 : 0);
        MultiLayer *ml = d_app->multilayerPlot(t, QStringList() << fullColumnName(t, colIdx),
                                               Graph::Pie, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Pie plot."));
        if (Graph *g = ml->activeGraph())
            enforceArialGraphFonts(g);
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.vectors")) {
        Table *t = resolveTable(d_app, params);
        QVector<int> colIdxs;
        if (params.value(QStringLiteral("cols")).isArray()) {
            for (const QJsonValue &it : params.value(QStringLiteral("cols")).toArray())
                colIdxs.append(resolveColumnIndex(t, it, true));
        } else {
            for (int c = 0; c < std::min(4, t->numCols()); ++c)
                colIdxs.append(c);
        }
        if (colIdxs.size() < 4)
            throwInvalidParams(QStringLiteral("Vector plot requires 4 columns (X, Y, dX/A, dY/M)."));
        Column *col0 = t->column(colIdxs[0]);
        Column *col1 = t->column(colIdxs[1]);
        Column *col2 = t->column(colIdxs[2]);
        Column *col3 = t->column(colIdxs[3]);
        if (!col0 || !col1 || !col2 || !col3)
            throwInvalidParams(QStringLiteral("Vector plot requires 4 valid table columns."));

        const QString mode = params.value(QStringLiteral("mode")).toString().trimmed().toLower();
        if (!mode.isEmpty() && mode != QStringLiteral("xyxy") && mode != QStringLiteral("cartesian")
            && mode != QStringLiteral("xyam") && mode != QStringLiteral("polar")) {
            throwInvalidParams(
                    QStringLiteral("Unsupported vector plot mode '%1' (expected 'xyxy' or 'xyam').")
                            .arg(mode));
        }
        const int vStyle = (mode == QStringLiteral("xyam") || mode == QStringLiteral("polar"))
                ? Graph::VectXYAM
                : Graph::VectXYXY;

        int validVectorRows = 0;
        for (int r = 0; r < t->numRows(); ++r) {
            if (col0->isInvalid(r) || col1->isInvalid(r) || col2->isInvalid(r) || col3->isInvalid(r))
                continue;
            if (col0->isMasked(r) || col1->isMasked(r) || col2->isMasked(r) || col3->isMasked(r))
                continue;
            const double v0 = t->cell(r, colIdxs[0]);
            const double v1 = t->cell(r, colIdxs[1]);
            const double v2 = t->cell(r, colIdxs[2]);
            const double v3 = t->cell(r, colIdxs[3]);
            if (std::isfinite(v0) && std::isfinite(v1) && std::isfinite(v2) && std::isfinite(v3))
                ++validVectorRows;
        }
        if (validVectorRows < 1) {
            throwInvalidParams(
                    QStringLiteral("Vector plot requires at least 1 row with finite unmasked values across all 4 columns."));
        }

        QStringList cols;
        for (int k = 0; k < 4; ++k)
            cols << fullColumnName(t, colIdxs[k]);

        MultiLayer *ml = d_app->newGraph(d_app->generateUniqueName(QStringLiteral("Graph")));
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Vector graph."));
        Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->addLayer();
        if (g)
            g->plotVectorCurve(t, cols, vStyle, 0, t->numRows() - 1);
        if (!g || g->curves() < 1) {
            ml->askOnCloseEvent(false);
            ml->hide();
            ml->close();
            if (ml->folder())
                d_app->closeWindow(ml);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            throwInvalidParams(
                    QStringLiteral("Vector plot failed: no valid vector curve could be created from the specified columns."));
        }
        enforceArialGraphFonts(g);
        g->replot();
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.ternary")) {
        Table *srcTable = resolveTable(d_app, params);
        int colA = 0, colB = 1, colC = 2;
        if (params.value(QStringLiteral("cols")).isArray()
            && params.value(QStringLiteral("cols")).toArray().size() >= 3) {
            const QJsonArray ca = params.value(QStringLiteral("cols")).toArray();
            colA = resolveColumnIndex(srcTable, ca[0], true);
            colB = resolveColumnIndex(srcTable, ca[1], true);
            colC = resolveColumnIndex(srcTable, ca[2], true);
        } else if (params.contains(QStringLiteral("a_col"))
                   && params.contains(QStringLiteral("b_col"))
                   && params.contains(QStringLiteral("c_col"))) {
            colA = resolveColumnIndex(srcTable, params.value(QStringLiteral("a_col")), true);
            colB = resolveColumnIndex(srcTable, params.value(QStringLiteral("b_col")), true);
            colC = resolveColumnIndex(srcTable, params.value(QStringLiteral("c_col")), true);
        } else if (srcTable->numCols() < 3) {
            throwInvalidParams(QStringLiteral("Ternary plot requires 3 columns (A, B, C)."));
        }
        QVector<QPointF> pts;
        Column *cAObj = srcTable->column(colA);
        Column *cBObj = srcTable->column(colB);
        Column *cCObj = srcTable->column(colC);
        for (int r = 0; r < srcTable->numRows(); ++r) {
            if (!cAObj || !cBObj || !cCObj || cAObj->isInvalid(r) || cBObj->isInvalid(r)
                || cCObj->isInvalid(r) || cAObj->isMasked(r) || cBObj->isMasked(r)
                || cCObj->isMasked(r)) {
                continue;
            }
            const double a = srcTable->cell(r, colA);
            const double b = srcTable->cell(r, colB);
            const double c = srcTable->cell(r, colC);
            if (std::isfinite(a) && std::isfinite(b) && std::isfinite(c) && (a + b + c) != 0.0)
                pts.append(Graph::ternaryToCartesian(a, b, c));
        }
        if (pts.isEmpty())
            throwInvalidParams(QStringLiteral("No valid ternary points (A + B + C != 0)."));
        Table *tt = createUniqueTableHeadless(d_app, pts.size(), 2,
                                              srcTable->name() + QStringLiteral("Ternary"));
        tt->setColName(0, QStringLiteral("Ternary_X"));
        tt->setColPlotDesignation(0, SciDAVis::X);
        tt->setColName(1, QStringLiteral("Ternary_Y"));
        tt->setColPlotDesignation(1, SciDAVis::Y);
        for (int i = 0; i < pts.size(); ++i) {
            tt->setCell(i, 0, pts[i].x());
            tt->setCell(i, 1, pts[i].y());
        }
        MultiLayer *ml = d_app->multilayerPlot(tt, QStringList() << tt->colName(1), Graph::Scatter,
                                               0, pts.size() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Ternary plot."));
        if (Graph *g = ml->activeGraph()) {
            g->setupTernaryPlot(srcTable->colLabel(colA), srcTable->colLabel(colB),
                                srcTable->colLabel(colC));
            enforceArialGraphFonts(g);
            g->replot();
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("data_table")] = tt->name();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.function")) {
        const QString formula = params.value(QStringLiteral("formula")).toString().trimmed();
        if (formula.isEmpty())
            throwInvalidParams(QStringLiteral("'formula' is required for plot.function."));
        const QString var = params.contains(QStringLiteral("variable"))
                ? params.value(QStringLiteral("variable")).toString()
                : QStringLiteral("x");
        const double xMin = params.value(QStringLiteral("x_min")).toDouble(0.0);
        const double xMax = params.value(QStringLiteral("x_max")).toDouble(10.0);
        const int points = std::max(10, params.value(QStringLiteral("points")).toInt(200));
        QList<double> ranges;
        ranges << xMin << xMax;

        MultiLayer *ml = nullptr;
        const bool createdNewGraph = !params.contains(QStringLiteral("graph"));
        if (!createdNewGraph)
            ml = resolveMultiLayer(d_app, params, QStringLiteral("graph"), true);
        else
            ml = d_app->newGraph(d_app->generateUniqueName(QStringLiteral("Graph")));
        Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->addLayer();
        if (!g->addFunctionCurve(d_app, FunctionCurve::Normal, QStringList() << formula, var, ranges,
                                 points, params.value(QStringLiteral("title")).toString())) {
            throwRpcRuntime(QStringLiteral("Failed to plot function '%1'.").arg(formula));
        }
        enforceArialGraphFonts(g);
        g->replot();
        if (createdNewGraph)
            applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.matrix")) {
        Matrix *m = resolveMatrix(d_app, params);
        const QString style = params.value(QStringLiteral("style")).toString().trimmed().toLower();
        Graph::CurveType cType = Graph::ColorMap;
        if (style == QStringLiteral("contour") || style == QStringLiteral("contour_map"))
            cType = Graph::ContourMap;
        else if (style == QStringLiteral("grayscale") || style == QStringLiteral("gray"))
            cType = Graph::GrayMap;
        MultiLayer *ml = d_app->plotSpectrogram(m, cType);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create matrix spectrogram plot."));
        if (Graph *g = ml->activeGraph()) {
            const bool hasPalette = params.contains(QStringLiteral("palette"))
                    && !params.value(QStringLiteral("palette")).isNull();
            const bool hasReverse = params.contains(QStringLiteral("reverse"))
                    && !params.value(QStringLiteral("reverse")).isNull();
            if (hasPalette || hasReverse) {
                bool isGray = false;
                const auto preset = hasPalette
                        ? parseColorMapPreset(params.value(QStringLiteral("palette")).toString(),
                                              &isGray, true)
                        : Spectrogram::PresetViridis;
                const bool rev = params.value(QStringLiteral("reverse")).toBool(false);
                for (QwtPlotItem *it : g->plotWidget()->itemList()) {
                    if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                        if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
                            if (hasPalette) {
                                sp->setColorMapPreset(
                                        isGray ? Spectrogram::PresetGrayScale : preset, rev);
                            } else if (hasReverse) {
                                sp->setReversePalette(rev);
                            }
                            sp->enforceColorBarArialFont();
                            sp->syncColorScale();
                        }
                    }
                }
            }
            enforceArialGraphFonts(g);
            g->replot();
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.3d_matrix")) {
        Matrix *m = resolveMatrix(d_app, params);
        const QString style = params.value(QStringLiteral("style")).toString().trimmed().toLower();
        const QString caption = d_app->generateUniqueName(QStringLiteral("Graph3D"), true);

        auto *plot = new Graph3D(QString(), &d_app->d_workspace, nullptr);
        plot->setAttribute(Qt::WA_DeleteOnClose);
        plot->addMatrixData(m);
        if (style == QStringLiteral("wireframe") || style == QStringLiteral("mesh"))
            plot->setLineGrid();
        else if (style == QStringLiteral("hidden_line"))
            plot->setHiddenLineGrid();
        else if (style == QStringLiteral("bars"))
            plot->setBarsPlot();
        else if (style == QStringLiteral("scatter") || style == QStringLiteral("dots")
                 || style == QStringLiteral("points"))
            plot->setPointsMesh();
        else
            plot->setFilledMesh();
        d_app->customPlot3D(plot);
        enforceArialGraph3DFonts(plot);
        plot->update();
        plot->resize(520, 420);
        plot->setWindowTitle(caption);
        plot->setName(caption);
        d_app->initPlot3D(plot);
        applyRequestedWindowName(d_app, plot, params);

        outResult = serializeGraph3DInfo(plot);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.3d_xyz")) {
        Table *t = resolveTable(d_app, params);
        const int xIdx = params.contains(QStringLiteral("x_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true)
                : 0;
        const int yIdx = params.contains(QStringLiteral("y_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true)
                : std::min(1, t->numCols() - 1);
        const int zIdx = params.contains(QStringLiteral("z_col"))
                ? resolveColumnIndex(t, params.value(QStringLiteral("z_col")), true)
                : std::min(2, t->numCols() - 1);
        t->setColPlotDesignation(xIdx, SciDAVis::X);
        t->setColPlotDesignation(yIdx, SciDAVis::Y);
        t->setColPlotDesignation(zIdx, SciDAVis::Z);

        const QString style = params.value(QStringLiteral("style")).toString().trimmed().toLower();
        int plotType = Graph3D::Scatter;
        if (style == QStringLiteral("trajectory") || style == QStringLiteral("line"))
            plotType = Graph3D::Trajectory;
        else if (style == QStringLiteral("bars"))
            plotType = Graph3D::Bars;

        Graph3D *plot = d_app->dataPlotXYZ(t, fullColumnName(t, zIdx), plotType);
        if (!plot)
            throwRpcRuntime(QStringLiteral("Failed to create 3D XYZ plot."));
        enforceArialGraph3DFonts(plot);
        applyRequestedWindowName(d_app, plot, params);
        outResult = serializeGraph3DInfo(plot);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.3d_function")) {
        const QString formula = params.value(QStringLiteral("formula")).toString().trimmed();
        if (formula.isEmpty())
            throwInvalidParams(QStringLiteral("'formula' is required for plot.3d_function."));
        const double xl = params.value(QStringLiteral("x_min")).toDouble(-5.0);
        const double xr = params.value(QStringLiteral("x_max")).toDouble(5.0);
        const double yl = params.value(QStringLiteral("y_min")).toDouble(-5.0);
        const double yr = params.value(QStringLiteral("y_max")).toDouble(5.0);
        const double zl = params.value(QStringLiteral("z_min")).toDouble(-5.0);
        const double zr = params.value(QStringLiteral("z_max")).toDouble(5.0);
        if (!std::isfinite(xl) || !std::isfinite(xr) || xl >= xr
            || !std::isfinite(yl) || !std::isfinite(yr) || yl >= yr
            || !std::isfinite(zl) || !std::isfinite(zr) || zl >= zr) {
            throwInvalidParams(QStringLiteral("plot.3d_function requires finite min < max on X, Y, and Z."));
        }
        validateMuParserExprOrThrow(formula, { QStringLiteral("x"), QStringLiteral("y") },
                                    QStringLiteral("plot.3d_function"));
        Graph3D *plot = d_app->newPlot3D(formula, xl, xr, yl, yr, zl, zr);
        if (!plot)
            throwRpcRuntime(QStringLiteral("Failed to create 3D function surface plot."));
        enforceArialGraph3DFonts(plot);
        applyRequestedWindowName(d_app, plot, params);
        outResult = serializeGraph3DInfo(plot);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    // ------------------------------------------------------------------------
    // 5. Graph Page, Multi-Layer, Axes, Breaks, Frame, Style, Colorbar & Export
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("graph.get_info")) {
        const QString name = params.value(QStringLiteral("graph")).toString().trimmed();
        if (!name.isEmpty()) {
            if (MyWidget *mw = findWindowByName(d_app, name)) {
                if (mw->inherits("Graph3D")) {
                    outResult = serializeGraph3DInfo(static_cast<Graph3D *>(mw));
                    return true;
                }
            }
        }
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        outResult = serializeGraphInfo(ml);
        return true;
    }

    if (method == QStringLiteral("graph.add_layer")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const int x = params.value(QStringLiteral("x")).toInt(0);
        const int y = params.value(QStringLiteral("y")).toInt(0);
        const int w = params.value(QStringLiteral("width")).toInt(0);
        const int h = params.value(QStringLiteral("height")).toInt(0);
        Graph *g = ml->addLayer(x, y, w, h);
        enforceArialGraphFonts(g);
        if (params.value(QStringLiteral("arrange")).toBool(true) && w == 0 && h == 0)
            ml->arrangeLayers(true, false);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = (g != nullptr);
        return true;
    }

    if (method == QStringLiteral("graph.add_linked_layer")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *parentG = resolveGraphLayer(ml, params, true);
        const bool linkX = params.contains(QStringLiteral("link_x"))
                ? params.value(QStringLiteral("link_x")).toBool(true)
                : true;
        const bool linkY = params.contains(QStringLiteral("link_y"))
                ? params.value(QStringLiteral("link_y")).toBool(false)
                : params.value(QStringLiteral("right_y")).toBool(false);
        const QString xExpr = params.contains(QStringLiteral("x_formula"))
                ? params.value(QStringLiteral("x_formula")).toString()
                : QStringLiteral("x");
        const QString yExpr = params.contains(QStringLiteral("y_formula"))
                ? params.value(QStringLiteral("y_formula")).toString()
                : QStringLiteral("y");
        Graph *childG = ml->addLinkedLayer(parentG, linkX, linkY, xExpr, yExpr);
        enforceArialGraphFonts(childG);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("link_x")] = linkX;
        outResult[QStringLiteral("link_y")] = linkY;
        outResult[QStringLiteral("ok")] = (childG != nullptr);
        return true;
    }

    if (method == QStringLiteral("graph.add_inset_layer")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *srcG = resolveGraphLayer(ml, params, true);
        QRectF roi;
        if (params.contains(QStringLiteral("x_min")) && params.contains(QStringLiteral("x_max"))
            && params.contains(QStringLiteral("y_min"))
            && params.contains(QStringLiteral("y_max"))) {
            const double xMin = params.value(QStringLiteral("x_min")).toDouble();
            const double xMax = params.value(QStringLiteral("x_max")).toDouble();
            const double yMin = params.value(QStringLiteral("y_min")).toDouble();
            const double yMax = params.value(QStringLiteral("y_max")).toDouble();
            roi = QRectF(QPointF(std::min(xMin, xMax), std::min(yMin, yMax)),
                         QPointF(std::max(xMin, xMax), std::max(yMin, yMax)));
        }
        Graph *insetG = ml->addInsetLayer(srcG, roi);
        enforceArialGraphFonts(insetG);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = (insetG != nullptr);
        return true;
    }

    if (method == QStringLiteral("graph.arrange_layers")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        if (params.contains(QStringLiteral("rows")) && !params.value(QStringLiteral("rows")).isNull()) {
            const int r = params.value(QStringLiteral("rows")).toInt(0);
            if (r > 0)
                ml->setRows(r);
        }
        if (params.contains(QStringLiteral("cols")) && !params.value(QStringLiteral("cols")).isNull()) {
            const int c = params.value(QStringLiteral("cols")).toInt(0);
            if (c > 0)
                ml->setCols(c);
        }
        if (params.contains(QStringLiteral("row_gap"))
            || params.contains(QStringLiteral("col_gap"))) {
            ml->setSpacing(params.value(QStringLiteral("row_gap")).toInt(ml->rowsSpacing()),
                           params.value(QStringLiteral("col_gap")).toInt(ml->colsSpacing()));
        }
        ml->arrangeLayers(params.value(QStringLiteral("fit")).toBool(false), false);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_page_size")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const double w = params.value(QStringLiteral("width")).toDouble(ml->pageWidthPhysical());
        const double h = params.value(QStringLiteral("height")).toDouble(ml->pageHeightPhysical());
        const MultiLayer::PageUnit unit = params.contains(QStringLiteral("unit"))
                ? parsePageUnit(params.value(QStringLiteral("unit")).toString())
                : ml->pageUnit();
        const int dpi = std::max(72, params.value(QStringLiteral("dpi")).toInt(ml->pageDpi()));
        ml->setPageSizePhysical(w, h, unit, dpi);
        if (params.contains(QStringLiteral("page_view")))
            ml->setPageViewMode(params.value(QStringLiteral("page_view")).toBool(true));
        if (params.contains(QStringLiteral("zoom")))
            ml->setPageZoom(params.value(QStringLiteral("zoom")).toDouble(1.0));
        if (params.value(QStringLiteral("fit_layers")).toBool(true))
            ml->fitLayersToPage();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.fit_page_to_layers")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const int margin = std::max(0, params.value(QStringLiteral("margin_px")).toInt(14));
        ml->fitPageToLayers(margin);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.fit_layers_to_page")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        ml->fitLayersToPage();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.apply_style_preset")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const QString p = params.value(QStringLiteral("preset")).toString().trimmed().toLower();
        if (p == QStringLiteral("isij_85mm") || p == QStringLiteral("isij")
            || p == QStringLiteral("journal")) {
            ml->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
        } else if (p == QStringLiteral("slide_28pt") || p == QStringLiteral("slide")
                   || p == QStringLiteral("presentation")) {
            ml->applyStylePreset(MultiLayer::StylePresentationSlide28pt);
        } else if (p == QStringLiteral("nature_89mm") || p == QStringLiteral("nature")) {
            ml->applyStylePreset(MultiLayer::StyleNatureCompact89mm);
        } else {
            throwInvalidParams(
                    QStringLiteral("Unknown style preset '%1' (expected 'isij_85mm'|'slide_28pt'|'nature_89mm').")
                            .arg(p));
        }
        for (Graph *lg : ml->layersList())
            enforceArialGraphFonts(lg);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.apply_frame_preset")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString p = params.value(QStringLiteral("preset")).toString().trimmed().toLower();
        if (p == QStringLiteral("box_inward") || p == QStringLiteral("inward"))
            g->applyFramePreset(Graph::FrameBoxInwardTicks);
        else if (p == QStringLiteral("box_plain") || p == QStringLiteral("plain"))
            g->applyFramePreset(Graph::FrameBoxPlainOpposite);
        else if (p == QStringLiteral("open_l") || p == QStringLiteral("l"))
            g->applyFramePreset(Graph::FrameOpenL);
        else if (p == QStringLiteral("zero_crossed") || p == QStringLiteral("origin"))
            g->applyFramePreset(Graph::FrameZeroCrossed);
        else
            throwInvalidParams(
                    QStringLiteral("Unknown frame preset '%1' (expected 'box_inward'|'box_plain'|'open_l'|'zero_crossed').")
                            .arg(p));
        enforceArialGraphFonts(g);
        g->replot();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_aspect_ratio")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const double ratio = params.value(QStringLiteral("ratio")).toDouble(1.0);
        const bool keep = params.contains(QStringLiteral("keep"))
                ? params.value(QStringLiteral("keep")).toBool(true)
                : (params.contains(QStringLiteral("enabled"))
                           ? params.value(QStringLiteral("enabled")).toBool(true)
                           : true);
        if (keep) {
            if (ratio > 0.0)
                ml->applyAspectRatioPreset(ratio, true);
            ml->setKeepAspectRatio(true);
        } else {
            if (ratio > 0.0)
                ml->setTargetAspectRatio(ratio);
            ml->setKeepAspectRatio(false);
        }
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.copy_format")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        d_app->activateSubWindow(ml);
        d_app->copyGraphFormat();

        HeadlessFormatClipboard &clip = rpcFormatClipboard();
        clip.valid = true;
        clip.pageViewMode = ml->pageViewMode();
        clip.pageUnit = ml->pageUnit();
        clip.pageW = ml->pageWidthPhysical();
        clip.pageH = ml->pageHeightPhysical();
        clip.pageDpi = ml->pageDpi();
        clip.pageBg = ml->pageBackgroundColor();
        clip.keepAspect = ml->keepAspectRatio();
        clip.aspect = ml->targetAspectRatio();
        clip.lm = ml->leftMargin();
        clip.rm = ml->rightMargin();
        clip.tm = ml->topMargin();
        clip.bm = ml->bottomMargin();
        clip.rSpace = ml->rowsSpacing();
        clip.cSpace = ml->colsSpacing();
        if (Graph *g = resolveGraphLayer(ml, params, false)) {
            clip.framed = g->framed();
            clip.frameWidth = g->canvasFrameWidth();
            clip.frameColor = g->canvasFrameColor();
            if (g->plotWidget()) {
                clip.hasPlotTitleFont = true;
                clip.plotTitleFont = g->plotWidget()->title().font();
            } else {
                clip.hasPlotTitleFont = false;
            }
            if (Legend *leg = g->legend()) {
                clip.hasLegendFont = true;
                clip.legendFont = leg->font();
            } else {
                clip.hasLegendFont = false;
            }
            for (int ax = 0; ax < 4; ++ax) {
                clip.axisTitleFonts[ax] = g->axisTitleFont(ax);
                clip.axisNumFonts[ax] = g->axisFont(ax);
                clip.axisEnabled[ax] = g->plotWidget() ? g->plotWidget()->axisEnabled(ax) : false;
                clip.backboneVis[ax] = g->isAxisBackboneVisible(ax);
                if (g->plotWidget()) {
                    if (auto *sd = dynamic_cast<ScaleDraw *>(g->plotWidget()->axisScaleDraw(ax))) {
                        clip.majTicks[ax] = sd->majorTicksStyle();
                        clip.minTicks[ax] = sd->minorTicksStyle();
                    }
                }
            }
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("source")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("graph.paste_format")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        d_app->activateSubWindow(ml);
        d_app->pasteGraphFormat();

        const HeadlessFormatClipboard &clip = rpcFormatClipboard();
        if (clip.valid) {
            ml->setMargins(clip.lm, clip.rm, clip.tm, clip.bm);
            ml->setSpacing(clip.rSpace, clip.cSpace);
            ml->setPageBackgroundColor(clip.pageBg);
            if (clip.pageW > 0.0 && clip.pageH > 0.0)
                ml->setPageSizePhysical(clip.pageW, clip.pageH, clip.pageUnit, clip.pageDpi);
            ml->setPageViewMode(clip.pageViewMode);
            ml->setTargetAspectRatio(clip.aspect);
            ml->setKeepAspectRatio(clip.keepAspect);
            for (Graph *lg : ml->layersList()) {
                lg->drawCanvasFrame(clip.framed, clip.frameWidth, clip.frameColor);
                if (clip.hasPlotTitleFont && lg->plotWidget()) {
                    QwtText t = lg->plotWidget()->title();
                    t.setFont(clip.plotTitleFont);
                    lg->plotWidget()->setTitle(t);
                }
                if (clip.hasLegendFont && lg->legend()) {
                    lg->legend()->setFont(clip.legendFont);
                }
                for (int ax = 0; ax < 4; ++ax) {
                    lg->enableAxis(ax, clip.axisEnabled[ax]);
                    lg->setAxisBackboneVisible(ax, clip.backboneVis[ax]);
                    lg->setAxisTitleFont(ax, clip.axisTitleFonts[ax]);
                    lg->setAxisFont(ax, clip.axisNumFonts[ax]);
                    if (lg->plotWidget()) {
                        if (auto *sd =
                                    dynamic_cast<ScaleDraw *>(lg->plotWidget()->axisScaleDraw(ax))) {
                            sd->setMajorTicksStyle(
                                    static_cast<ScaleDraw::TicksStyle>(clip.majTicks[ax]));
                            sd->setMinorTicksStyle(
                                    static_cast<ScaleDraw::TicksStyle>(clip.minTicks[ax]));
                        }
                    }
                }
                lg->replot();
            }
        }
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.batch_clone")) {
        MultiLayer *srcMl = resolveMultiLayer(d_app, params, QStringLiteral("graph"), true);
        if (params.value(QStringLiteral("tables")).isArray()) {
            QJsonArray clonedArr;
            for (const QJsonValue &tv : params.value(QStringLiteral("tables")).toArray()) {
                QJsonObject subP;
                subP[QStringLiteral("table")] = tv.toString();
                Table *dstT = resolveTable(d_app, subP, QStringLiteral("table"), true);
                if (MultiLayer *cl = d_app->batchCloneGraphToTable(srcMl, dstT))
                    clonedArr.append(cl->name());
            }
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("cloned_graphs")] = clonedArr;
            return true;
        }
        Table *dstT = resolveTable(d_app, params, QStringLiteral("table"), true);
        MultiLayer *cloned = d_app->batchCloneGraphToTable(srcMl, dstT);
        if (!cloned)
            throwRpcRuntime(QStringLiteral("Failed to batch-clone graph '%1' to table '%2'.")
                                    .arg(srcMl->name(), dstT->name()));
        outResult = serializeGraphInfo(cloned);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_axis")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const int ax = parseAxisId(params.contains(QStringLiteral("axis"))
                                           ? params.value(QStringLiteral("axis")).toString()
                                           : QStringLiteral("x"));
        if (params.contains(QStringLiteral("enabled")))
            g->enableAxis(ax, params.value(QStringLiteral("enabled")).toBool(true));

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
        if (params.contains(QStringLiteral("min")) || params.contains(QStringLiteral("max"))
            || params.contains(QStringLiteral("step")) || params.contains(QStringLiteral("scale"))
            || params.contains(QStringLiteral("inverted"))) {
            const double lo = params.value(QStringLiteral("min")).toDouble(curMin);
            const double hi = params.value(QStringLiteral("max")).toDouble(curMax);
            const double step = params.value(QStringLiteral("step")).toDouble(g->axisStep(ax));
            int scaleType = curScaleType;
            if (params.contains(QStringLiteral("scale"))) {
                const QString sc = params.value(QStringLiteral("scale")).toString().toLower();
                scaleType = (sc == QStringLiteral("log10") || sc == QStringLiteral("log")) ? 1 : 0;
            }
            const bool inv = params.contains(QStringLiteral("inverted"))
                    ? params.value(QStringLiteral("inverted")).toBool(curInv)
                    : curInv;
            g->setScale(ax, lo, hi, step, 5, 5, scaleType, inv);
        }

        if (params.contains(QStringLiteral("title"))) {
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
            const int pt = std::max(4, params.value(QStringLiteral("font_size")).toInt(12));
            tf.setPointSize(pt);
            nf.setPointSize(pt);
        }
        if (params.contains(QStringLiteral("title_font_size"))
            && !params.value(QStringLiteral("title_font_size")).isNull()) {
            tf.setPointSize(std::max(4, params.value(QStringLiteral("title_font_size")).toInt()));
        }
        if (params.contains(QStringLiteral("numbers_font_size"))
            && !params.value(QStringLiteral("numbers_font_size")).isNull()) {
            nf.setPointSize(std::max(4, params.value(QStringLiteral("numbers_font_size")).toInt()));
        }
        g->setAxisTitleFont(ax, tf);
        g->setAxisFont(ax, nf);

        if (params.contains(QStringLiteral("ticks_inward")) && plt) {
            if (auto *sd = dynamic_cast<ScaleDraw *>(plt->axisScaleDraw(ax))) {
                const auto ts = params.value(QStringLiteral("ticks_inward")).toBool()
                        ? ScaleDraw::In
                        : ScaleDraw::Out;
                sd->setMajorTicksStyle(ts);
                sd->setMinorTicksStyle(ts);
            }
        }
        if (params.contains(QStringLiteral("show_backbone")))
            g->setAxisBackboneVisible(ax, params.value(QStringLiteral("show_backbone")).toBool());
        if (params.contains(QStringLiteral("divide_by"))
            || params.contains(QStringLiteral("prefix"))
            || params.contains(QStringLiteral("suffix"))) {
            g->setTickLabelTransform(
                    ax,
                    params.value(QStringLiteral("divide_by")).toDouble(g->tickDivideFactor(ax)),
                    params.contains(QStringLiteral("prefix"))
                            ? params.value(QStringLiteral("prefix")).toString()
                            : g->tickPrefix(ax),
                    params.contains(QStringLiteral("suffix"))
                            ? params.value(QStringLiteral("suffix")).toString()
                            : g->tickSuffix(ax));
        }
        g->replot();
        outResult = serializeAxisInfo(g, ax);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_axis_break")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const int ax = parseAxisId(params.contains(QStringLiteral("axis"))
                                           ? params.value(QStringLiteral("axis")).toString()
                                           : QStringLiteral("x"));
        const bool enabled = params.contains(QStringLiteral("enabled"))
                ? params.value(QStringLiteral("enabled")).toBool(true)
                : true;
        if (!enabled) {
            g->clearAxisBreak(ax);
        } else {
            Graph::AxisBreakSpec spec = g->axisBreak(ax);
            spec.enabled = true;
            spec.breakFrom = params.value(QStringLiteral("start")).toDouble(spec.breakFrom);
            spec.breakTo = params.value(QStringLiteral("end")).toDouble(spec.breakTo);
            double pct = params.value(QStringLiteral("position_pct")).toDouble(0.50);
            if (pct > 1.0)
                pct /= 100.0;
            spec.positionPct = std::clamp(pct, 0.10, 0.90);
            spec.stepBefore = params.value(QStringLiteral("step_before")).toDouble(spec.stepBefore);
            spec.stepAfter = params.value(QStringLiteral("step_after")).toDouble(spec.stepAfter);
            spec.log10Before =
                    params.value(QStringLiteral("log_before")).toBool(spec.log10Before);
            spec.log10After = params.value(QStringLiteral("log_after")).toBool(spec.log10After);
            g->setAxisBreak(ax, spec);
        }
        g->replot();
        outResult = serializeAxisInfo(g, ax);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.add_reference_line")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        Graph::ReferenceLineSpec spec;
        const QString ori =
                params.value(QStringLiteral("orientation")).toString().trimmed().toLower();
        const QString ax = params.value(QStringLiteral("axis")).toString().trimmed().toLower();
        spec.orientation = (ori == QStringLiteral("vertical") || ori == QStringLiteral("v")
                            || ax == QStringLiteral("x"))
                ? 1
                : 0;
        spec.value = params.value(QStringLiteral("value")).toDouble(0.0);
        spec.statMode = params.value(QStringLiteral("stat_mode")).toString();
        spec.color =
                parseColorValue(params.value(QStringLiteral("color")), QColor(30, 102, 184));
        spec.style = parsePenStyleValue(params.value(QStringLiteral("style")), Qt::DashLine);
        spec.width = params.value(QStringLiteral("width")).toDouble(1.5);
        spec.label = params.value(QStringLiteral("label")).toString();
        spec.fillToAxis = params.value(QStringLiteral("fill_to_axis")).toBool(false);
        spec.fillColor = parseColorValue(params.value(QStringLiteral("fill_color")),
                                         QColor(30, 102, 184, 32));
        g->addReferenceLine(spec);
        g->replot();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("reference_line_count")] = g->referenceLines().size();
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
            for (int i = 0; i < n; ++i) {
                xv.append(xa[i].toDouble());
                yLo.append(la[i].toDouble());
                yHi.append(ha[i].toDouble());
            }
        } else if (params.contains(QStringLiteral("y_min"))
                   && params.contains(QStringLiteral("y_max"))) {
            double x0 = 0.0, x1 = 1.0;
            if (g->plotWidget() && g->plotWidget()->axisScaleDiv(QwtPlot::xBottom)) {
                const auto *d = g->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
                x0 = std::min(d->lowerBound(), d->upperBound());
                x1 = std::max(d->lowerBound(), d->upperBound());
            }
            x0 = params.value(QStringLiteral("x_min")).toDouble(x0);
            x1 = params.value(QStringLiteral("x_max")).toDouble(x1);
            const double yl = params.value(QStringLiteral("y_min")).toDouble();
            const double yh = params.value(QStringLiteral("y_max")).toDouble();
            xv << x0 << x1;
            yLo << yl << yl;
            yHi << yh << yh;
        } else {
            throwInvalidParams(
                    QStringLiteral("Provide ('x', 'y_low', 'y_high') arrays or ('y_min', 'y_max')."));
        }
        QColor fillCol = parseColorValue(params.value(QStringLiteral("color")),
                                         QColor(30, 102, 184, 55));
        if (params.contains(QStringLiteral("alpha")))
            fillCol.setAlpha(std::clamp(params.value(QStringLiteral("alpha")).toInt(55), 0, 255));
        g->addShadedBand(xv, yLo, yHi, fillCol, params.value(QStringLiteral("title")).toString());
        g->replot();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_grid")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        Grid *grd = g->grid();
        if (grd) {
            if (params.contains(QStringLiteral("major_x")))
                grd->enableX(params.value(QStringLiteral("major_x")).toBool());
            if (params.contains(QStringLiteral("major_y")))
                grd->enableY(params.value(QStringLiteral("major_y")).toBool());
            if (params.contains(QStringLiteral("minor_x")))
                grd->enableXMin(params.value(QStringLiteral("minor_x")).toBool());
            if (params.contains(QStringLiteral("minor_y")))
                grd->enableYMin(params.value(QStringLiteral("minor_y")).toBool());
            if (params.contains(QStringLiteral("enabled"))) {
                const bool on = params.value(QStringLiteral("enabled")).toBool();
                grd->enableX(on);
                grd->enableY(on);
            }
            if (params.contains(QStringLiteral("zero_x")))
                grd->enableZeroLineX(params.value(QStringLiteral("zero_x")).toBool());
            if (params.contains(QStringLiteral("zero_y")))
                grd->enableZeroLineY(params.value(QStringLiteral("zero_y")).toBool());
            if (params.contains(QStringLiteral("color"))
                || params.contains(QStringLiteral("style"))
                || params.contains(QStringLiteral("width"))) {
                QPen p = grd->majPenX();
                if (params.contains(QStringLiteral("color")))
                    p.setColor(parseColorValue(params.value(QStringLiteral("color")), p.color()));
                if (params.contains(QStringLiteral("style")))
                    p.setStyle(parsePenStyleValue(params.value(QStringLiteral("style")), p.style()));
                if (params.contains(QStringLiteral("width")))
                    p.setWidthF(std::max(0.2, params.value(QStringLiteral("width")).toDouble(0.8)));
                grd->setMajPenX(p);
                grd->setMajPenY(p);
            }
        }
        g->replot();
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
            const QString ct = resolveCurveTitle(g, params, QStringLiteral("curve"), true);
            cIdx = g->curveIndex(ct);
        } else if (g->curves() > 0) {
            cIdx = 0;
        }
        QwtPlotCurve *c = (cIdx >= 0) ? g->curve(cIdx) : nullptr;
        if (!c)
            throwInvalidParams(QStringLiteral("Curve not found on graph '%1'.").arg(ml->name()));

        QPen pen = c->pen();
        QColor col = pen.color();
        if (params.contains(QStringLiteral("color")))
            col = parseColorValue(params.value(QStringLiteral("color")), col);
        if (params.contains(QStringLiteral("alpha"))) {
            const double rawA = params.value(QStringLiteral("alpha")).toDouble(255.0);
            const int a255 = (rawA <= 1.0 && rawA >= 0.0) ? int(std::round(rawA * 255.0))
                                                          : std::clamp(int(rawA), 0, 255);
            col.setAlpha(a255);
        }
        pen.setColor(col);
        if (params.contains(QStringLiteral("line_width")))
            pen.setWidthF(std::max(0.0, params.value(QStringLiteral("line_width")).toDouble(1.5)));
        if (params.contains(QStringLiteral("line_style")))
            pen.setStyle(parsePenStyleValue(params.value(QStringLiteral("line_style")), pen.style()));
        g->setCurvePen(cIdx, pen);

        if (params.contains(QStringLiteral("symbol_type"))
            || params.contains(QStringLiteral("symbol_size"))) {
            QwtSymbol sym = c->symbol();
            if (params.contains(QStringLiteral("symbol_type")))
                sym.setStyle(parseQwtSymbolStyle(params.value(QStringLiteral("symbol_type")),
                                                 sym.style()));
            if (params.contains(QStringLiteral("symbol_size"))) {
                const int sz = std::max(1, params.value(QStringLiteral("symbol_size")).toInt(7));
                sym.setSize(sz, sz);
            }
            QPen sp = sym.pen();
            sp.setColor(col);
            sym.setPen(sp);
            QBrush sb = sym.brush();
            sb.setColor(col);
            if (sb.style() == Qt::NoBrush && sym.style() != QwtSymbol::NoSymbol)
                sb.setStyle(Qt::SolidPattern);
            sym.setBrush(sb);
            g->setCurveSymbol(cIdx, sym);
        }

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

        g->replot();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("curve_index")] = cIdx;
        return true;
    }

    if (method == QStringLiteral("graph.set_stack_offsets")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QJsonValue modeVal = params.value(QStringLiteral("mode"));
        const QString modeStr = modeVal.toString().trimmed().toLower();
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
            }
            const double yGap = params.contains(QStringLiteral("y_gap_pct"))
                    ? params.value(QStringLiteral("y_gap_pct")).toDouble(15.0)
                    : params.value(QStringLiteral("y_offset_pct")).toDouble(15.0);
            const double xGap = params.contains(QStringLiteral("x_gap_pct"))
                    ? params.value(QStringLiteral("x_gap_pct")).toDouble(0.0)
                    : params.value(QStringLiteral("x_offset_pct")).toDouble(0.0);
            const bool norm01 = params.value(QStringLiteral("normalize")).toBool(false);
            g->setStackOffsets(mode, yGap, xGap, norm01);
        }
        g->replot();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.apply_palette")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const bool grad = params.value(QStringLiteral("gradient")).toBool(false);
        if (params.value(QStringLiteral("colors")).isArray()) {
            QList<QColor> cols;
            for (const QJsonValue &cv : params.value(QStringLiteral("colors")).toArray())
                cols.append(parseColorValue(cv));
            g->applyColorPalette(cols, grad, true);
        } else {
            const QString palName = params.contains(QStringLiteral("palette"))
                    ? params.value(QStringLiteral("palette")).toString()
                    : QStringLiteral("Okabe-Ito");
            g->applyColorPaletteByName(palName, grad);
        }
        g->replot();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
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

        const bool hasPalette = params.contains(QStringLiteral("palette"))
                && !params.value(QStringLiteral("palette")).isNull();
        const bool hasReverse = params.contains(QStringLiteral("reverse"))
                && !params.value(QStringLiteral("reverse")).isNull();
        if (hasPalette) {
            bool isGray = false;
            const auto preset = parseColorMapPreset(
                    params.value(QStringLiteral("palette")).toString(), &isGray, true);
            const bool rev = params.value(QStringLiteral("reverse")).toBool(false);
            targetSpec->setColorMapPreset(isGray ? Spectrogram::PresetGrayScale : preset, rev);
        } else if (hasReverse) {
            targetSpec->setReversePalette(params.value(QStringLiteral("reverse")).toBool(false));
        }
        if (params.contains(QStringLiteral("z_min")) && params.contains(QStringLiteral("z_max"))) {
            targetSpec->setCustomValueRange(true,
                                            params.value(QStringLiteral("z_min")).toDouble(),
                                            params.value(QStringLiteral("z_max")).toDouble());
        }
        if (params.contains(QStringLiteral("visible"))) {
            targetSpec->showColorScale(QwtPlot::yRight,
                                       params.value(QStringLiteral("visible")).toBool(true));
        } else if (!targetSpec->hasColorScale()) {
            targetSpec->showColorScale(QwtPlot::yRight, true);
        }
        if (params.contains(QStringLiteral("title")))
            targetSpec->setColorBarTitle(params.value(QStringLiteral("title")).toString());
        if (params.contains(QStringLiteral("levels")))
            targetSpec->setLevelsNumber(std::max(2, params.value(QStringLiteral("levels")).toInt(8)));
        if (params.contains(QStringLiteral("show_contour_lines"))) {
            targetSpec->setDisplayMode(QwtPlotSpectrogram::ContourMode,
                                       params.value(QStringLiteral("show_contour_lines")).toBool());
        }
        targetSpec->enforceColorBarArialFont();
        targetSpec->syncColorScale();
        enforceArialGraphFonts(g);
        g->replot();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_legend")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const bool vis = params.contains(QStringLiteral("visible"))
                ? params.value(QStringLiteral("visible")).toBool(true)
                : true;
        if (!vis) {
            g->removeLegend();
        } else {
            Legend *leg = g->legend();
            if (!leg)
                leg = g->newLegend();
            if (params.contains(QStringLiteral("text")))
                leg->setText(params.value(QStringLiteral("text")).toString());
            else if (params.value(QStringLiteral("reconstruct")).toBool(false))
                g->reconstructLegend();
            if (params.contains(QStringLiteral("columns")))
                leg->setNumColumns(std::max(1, params.value(QStringLiteral("columns")).toInt(1)));
            if (params.contains(QStringLiteral("frame")))
                leg->setFrameStyle(params.value(QStringLiteral("frame")).toInt(0));
            if (params.contains(QStringLiteral("x")) && params.contains(QStringLiteral("y")))
                leg->setOriginCoord(params.value(QStringLiteral("x")).toDouble(),
                                    params.value(QStringLiteral("y")).toDouble());
            QFont lf = leg->font();
            if (params.contains(QStringLiteral("font_family"))
                && !params.value(QStringLiteral("font_family")).isNull()) {
                const QString fam = params.value(QStringLiteral("font_family")).toString().trimmed();
                if (!fam.isEmpty())
                    lf.setFamily(fam);
            } else if (lf.family().trimmed().isEmpty()) {
                lf.setFamily(QStringLiteral("Arial"));
            }
            if (params.contains(QStringLiteral("font_size")))
                lf.setPointSize(std::max(4, params.value(QStringLiteral("font_size")).toInt(10)));
            leg->setFont(lf);
        }
        g->replot();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.add_text")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString txt = params.value(QStringLiteral("text")).toString();
        auto *mrk = new Legend(g->plotWidget());
        mrk->setText(txt);
        QFont f(params.contains(QStringLiteral("font_family"))
                        ? params.value(QStringLiteral("font_family")).toString()
                        : QStringLiteral("Arial"),
                std::max(4, params.value(QStringLiteral("font_size")).toInt(11)));
        mrk->setFont(f);
        mrk->setTextColor(
                parseColorValue(params.value(QStringLiteral("color")), QColor(Qt::black)));
        mrk->setFrameStyle(params.value(QStringLiteral("frame")).toInt(int(Legend::None)));
        if (params.contains(QStringLiteral("x")) && params.contains(QStringLiteral("y"))) {
            mrk->setOriginCoord(params.value(QStringLiteral("x")).toDouble(),
                                params.value(QStringLiteral("y")).toDouble());
        }
        const long id = g->insertTextMarker(mrk);
        g->replot();
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("marker_id")] = static_cast<qint64>(id);
        return true;
    }

    if (method == QStringLiteral("graph.add_arrow")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        auto *arr = new ArrowMarker();
        arr->setStartPoint(params.value(QStringLiteral("x1")).toDouble(0.0),
                           params.value(QStringLiteral("y1")).toDouble(0.0));
        arr->setEndPoint(params.value(QStringLiteral("x2")).toDouble(1.0),
                         params.value(QStringLiteral("y2")).toDouble(1.0));
        arr->setColor(parseColorValue(params.value(QStringLiteral("color")), QColor(Qt::black)));
        arr->setWidth(std::max(1, params.value(QStringLiteral("width")).toInt(1)));
        arr->setStyle(parsePenStyleValue(params.value(QStringLiteral("style")), Qt::SolidLine));
        arr->drawStartArrow(params.value(QStringLiteral("start_arrow")).toBool(false));
        arr->drawEndArrow(params.contains(QStringLiteral("end_arrow"))
                                  ? params.value(QStringLiteral("end_arrow")).toBool(true)
                                  : true);
        arr->fillArrowHead(params.contains(QStringLiteral("filled"))
                                   ? params.value(QStringLiteral("filled")).toBool(true)
                                   : true);
        g->addArrow(arr);
        g->replot();
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.mask_region")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const double xMin = params.value(QStringLiteral("x_min")).toDouble();
        const double xMax = params.value(QStringLiteral("x_max")).toDouble();
        const double yMin = params.value(QStringLiteral("y_min")).toDouble();
        const double yMax = params.value(QStringLiteral("y_max")).toDouble();
        const QRectF rect(QPointF(std::min(xMin, xMax), std::min(yMin, yMax)),
                          QPointF(std::max(xMin, xMax), std::max(yMin, yMax)));
        const bool doMask = params.contains(QStringLiteral("mask"))
                ? params.value(QStringLiteral("mask")).toBool(true)
                : !params.value(QStringLiteral("unmask")).toBool(false);

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

        int totalMasked = 0;
        for (Graph *lg : targetLayers) {
            if (!lg)
                continue;
            int onlyIdx = -1;
            if (params.contains(QStringLiteral("curve"))
                && !params.value(QStringLiteral("curve")).isNull()) {
                const QString ct = resolveCurveTitle(lg, params, QStringLiteral("curve"), false);
                if (!ct.isEmpty())
                    onlyIdx = lg->curveIndex(ct);
            }
            for (int i = 0; i < lg->curves(); ++i) {
                if (onlyIdx >= 0 && i != onlyIdx)
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
        if (params.contains(QStringLiteral("layer")))
            targetLayers << resolveGraphLayer(ml, params);
        else
            targetLayers = ml->layersList();
        for (Graph *lg : targetLayers) {
            if (!lg)
                continue;
            for (int i = 0; i < lg->curves(); ++i) {
                if (auto *dc = dynamic_cast<DataCurve *>(lg->curve(i)))
                    dc->clearMasks();
            }
            lg->replot();
        }
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
            QFile::remove(absPath);
            targetG3D->update();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (targetG3D->sp) {
                targetG3D->sp->makeCurrent();
                (void)targetG3D->sp->grabFramebuffer();
            }
            if (ext == QStringLiteral("pdf")) {
                targetG3D->exportPDF(absPath);
            } else if (ext == QStringLiteral("svg") || ext == QStringLiteral("eps")) {
                targetG3D->exportVector(absPath, ext);
            } else if (ext == QStringLiteral("emf")) {
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
                targetG3D->exportImage(absPath,
                                       params.value(QStringLiteral("quality")).toInt(100),
                                       params.value(QStringLiteral("transparent")).toBool(false));
            }
            if (!QFileInfo::exists(absPath) || QFileInfo(absPath).size() <= 0) {
                throwRpcRuntime(
                        QStringLiteral("Failed to export 3D graph to '%1'.").arg(absPath));
            }
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("path")] = absPath;
            outResult[QStringLiteral("bytes")] = QFileInfo(absPath).size();
            return true;
        }

        MultiLayer *ml = resolveMultiLayer(d_app, params);
        for (Graph *lg : ml->layersList())
            lg->replot();

        const int dpi = params.value(QStringLiteral("dpi")).toInt(ml->pageDpi() > 0 ? ml->pageDpi() : 300);
        const bool transparent = params.value(QStringLiteral("transparent")).toBool(false);
        const int quality = params.value(QStringLiteral("quality")).toInt(100);

        if (ext == QStringLiteral("svg")) {
            ml->exportSVG(absPath);
        } else if (ext == QStringLiteral("pdf") || ext == QStringLiteral("eps")
                   || ext == QStringLiteral("ps")) {
            ml->exportVector(absPath, dpi, true);
        } else if (ext == QStringLiteral("emf")) {
            ml->exportToFile(absPath);
        } else {
            ml->exportImage(absPath, quality, transparent, dpi);
        }

        if (!QFileInfo::exists(absPath) || QFileInfo(absPath).size() <= 0)
            throwRpcRuntime(QStringLiteral("Failed to export graph to '%1'.").arg(absPath));

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

    if (method == QStringLiteral("graph.copy_image")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const int dpi = std::max(72, params.value(QStringLiteral("dpi")).toInt(300));
        const QImage img = ml->copyPageAsImage(dpi);
        if (!img.isNull() && QApplication::clipboard())
            QApplication::clipboard()->setImage(img);
        outResult[QStringLiteral("ok")] = !img.isNull();
        outResult[QStringLiteral("width_px")] = img.width();
        outResult[QStringLiteral("height_px")] = img.height();
        return true;
    }

    if (method == QStringLiteral("graph.copy_ole")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
#ifdef Q_OS_WIN
        Graph *singleG = params.value(QStringLiteral("active_layer_only")).toBool(false)
                ? ml->activeGraph()
                : nullptr;
        const bool ok = SciDAVisOle::copyGraphAsOle(d_app, ml, singleG);
        outResult[QStringLiteral("ok")] = ok;
        return true;
#else
        Q_UNUSED(ml)
        throwRpcRuntime(QStringLiteral("OLE clipboard export is only supported on Windows."));
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
            g3d->setRotation(params.value(QStringLiteral("x_rotation")).toDouble(g3d->xRotation()),
                             params.value(QStringLiteral("y_rotation")).toDouble(g3d->yRotation()),
                             params.value(QStringLiteral("z_rotation")).toDouble(g3d->zRotation()));
        }
        if (params.contains(QStringLiteral("zoom")))
            g3d->setZoom(params.value(QStringLiteral("zoom")).toDouble(g3d->zoom()));
        if (params.contains(QStringLiteral("orthogonal")))
            g3d->setOrtho(params.value(QStringLiteral("orthogonal")).toBool(true));

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
            g3d->setResolution(parseStrictPositiveInt(params, QStringLiteral("resolution"), 1));
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

} // namespace

// ============================================================================
// Categories 6 & 7 Dispatch (analysis / gadget / signal / curvemath / stats / digitizer)
// ============================================================================

namespace {

bool dispatchAnalysisAndScienceMethod(ApplicationWindow *d_app, const QString &method,
                                      const QJsonObject &params, QJsonObject &outResult)
{
    // ------------------------------------------------------------------------
    // 6. All Curve Fitting & Peak Analysis (analysis.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("analysis.list_fit_models")) {
        const QStringList names = NonLinearFit::builtInModelNames();
        QJsonArray arr;
        for (const QString &nm : names) {
            QJsonObject mObj;
            mObj[QStringLiteral("name")] = nm;
            mObj[QStringLiteral("category")] = NonLinearFit::builtInModelCategory(nm);
            mObj[QStringLiteral("formula")] = NonLinearFit::builtInModelFormula(nm);
            mObj[QStringLiteral("parameters")] =
                    toJsonArray(NonLinearFit::builtInModelParameters(nm));
            mObj[QStringLiteral("description")] = NonLinearFit::builtInModelDescription(nm);
            arr.append(mObj);
        }
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
        if (params.contains(QStringLiteral("x_min")) && params.contains(QStringLiteral("x_max"))) {
            const double xMin = params.value(QStringLiteral("x_min")).toDouble();
            const double xMax = params.value(QStringLiteral("x_max")).toDouble();
            if (!std::isfinite(xMin) || !std::isfinite(xMax) || xMin >= xMax)
                throwInvalidParams(QStringLiteral("analysis.fit_linear requires finite x_min < x_max."));
            dataOk = lf.setDataFromCurve(curveTitle, xMin, xMax, g);
        } else {
            dataOk = lf.setDataFromCurve(curveTitle, g);
        }
        if (!dataOk || lf.error())
            throwInvalidParams(
                    QStringLiteral("Insufficient data for linear fit on '%1'.").arg(curveTitle));

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

        lf.fit();
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();

        const auto &res = lf.results();
        const auto &err = lf.errors();
        if (res.size() < 2)
            throwRpcRuntime(QStringLiteral("Linear fit failed to produce parameters."));

        const double intercept = res[0];
        const double slope = res[1];
        const double interceptSE = (err.size() >= 1) ? err[0] : 0.0;
        const double slopeSE = (err.size() >= 2) ? err[1] : 0.0;
        const int dof = std::max(1, lf.dataSize() - 2);
        const double tSlope = (slopeSE > 0.0) ? (std::abs(slope) / slopeSE) : 0.0;
        const double pValue = (slopeSE > 0.0) ? (2.0 * gsl_cdf_tdist_Q(tSlope, double(dof)))
                                              : lf.anovaPValue();

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

        const bool ok = lf.fitWithXYErrors(xErrFull, yErrFull);
        if (!ok)
            throwRpcRuntime(QStringLiteral("York XY-error regression failed to converge."));
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();

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

        pf.fit();
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();

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
                ? params.value(QStringLiteral("plot_curve")).toBool(true)
                : true;
        nlf.setSuppressCurveOutput(!plotCurve);
        nlf.setMaximumIterations(
                std::max(20, params.value(QStringLiteral("max_iter")).toInt(1000)));
        nlf.setTolerance(params.value(QStringLiteral("tolerance")).toDouble(1e-7));
        if (params.contains(QStringLiteral("points")))
            nlf.generateFunction(true, std::max(20, params.value(QStringLiteral("points")).toInt(200)));

        const QString modelName = params.value(QStringLiteral("model")).toString().trimmed();
        const QString customFormula = params.value(QStringLiteral("formula")).toString().trimmed();
        bool customFormulaSyntaxOk = true;
        if (!modelName.isEmpty()) {
            if (!nlf.setBuiltInModel(modelName))
                throwInvalidParams(
                        QStringLiteral("Unknown built-in nonlinear fit model '%1'.").arg(modelName));
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
            customFormulaSyntaxOk = checkMuParserExprValid(customFormula, checkVars);
            nlf.setParametersList(pNames);
            if (customFormulaSyntaxOk)
                nlf.setFormula(customFormula);
        } else {
            throwInvalidParams(
                    QStringLiteral("Provide either built-in 'model' name or custom 'formula'."));
        }

        bool dataOk = false;
        requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("analysis.fit_nonlinear"));
        if (params.contains(QStringLiteral("x_min")) && params.contains(QStringLiteral("x_max"))) {
            const double xMin = params.value(QStringLiteral("x_min")).toDouble();
            const double xMax = params.value(QStringLiteral("x_max")).toDouble();
            if (!std::isfinite(xMin) || !std::isfinite(xMax) || xMin >= xMax)
                throwInvalidParams(QStringLiteral("analysis.fit_nonlinear requires finite x_min < x_max."));
            dataOk = nlf.setDataFromCurve(curveTitle, xMin, xMax, g);
        } else {
            dataOk = nlf.setDataFromCurve(curveTitle, g);
        }
        if (!dataOk || nlf.error())
            throwInvalidParams(
                    QStringLiteral("Insufficient data on curve '%1' for nonlinear fit.").arg(curveTitle));

        if (!modelName.isEmpty())
            nlf.guessBuiltInParameters(modelName);
        else if (customFormulaSyntaxOk)
            nlf.guessInitialValues();

        const QStringList pNames = nlf.parameterNames();
        // Explicit initial guesses (dict by param name OR array by index)
        if (params.contains(QStringLiteral("initial_values"))) {
            const QJsonValue iv = params.value(QStringLiteral("initial_values"));
            if (iv.isArray()) {
                const QJsonArray ia = iv.toArray();
                for (int i = 0; i < std::min(pNames.size(), ia.size()); ++i)
                    nlf.setInitialGuess(i, ia[i].toDouble());
            } else if (iv.isObject()) {
                const QJsonObject io = iv.toObject();
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
            for (int i = 0; i < pNames.size(); ++i) {
                if (bo.contains(pNames[i]) && bo.value(pNames[i]).isArray()) {
                    const QJsonArray ba = bo.value(pNames[i]).toArray();
                    if (ba.size() >= 2) {
                        const double lo = ba[0].isNull() ? -1e30 : ba[0].toDouble();
                        const double hi = ba[1].isNull() ? 1e30 : ba[1].toDouble();
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

        if (customFormulaSyntaxOk)
            nlf.fit();
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();

        const auto &resVec = nlf.results();
        const auto &errVec = nlf.errors();
        QJsonObject paramDict, errDict;
        for (int i = 0; i < pNames.size() && size_t(i) < resVec.size(); ++i) {
            paramDict[pNames[i]] = safeJsonDouble(resVec[size_t(i)]);
            errDict[pNames[i]] =
                    safeJsonDouble((size_t(i) < errVec.size()) ? errVec[size_t(i)] : 0.0);
        }

        const double chi2Val = nlf.chiSquare();
        const bool validChi2 = customFormulaSyntaxOk && std::isfinite(chi2Val) && chi2Val >= 0.0;
        const bool converged = validChi2 && !nlf.error() && !resVec.empty()
                && std::all_of(resVec.begin(), resVec.end(),
                               [](double v) { return std::isfinite(v); });

        outResult[QStringLiteral("ok")] = converged;
        outResult[QStringLiteral("converged")] = converged;
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
        outResult[QStringLiteral("r2")] =
                validChi2 ? safeJsonDouble(std::min(1.0, nlf.rSquare())) : QJsonValue(0.0);
        outResult[QStringLiteral("adj_r2")] =
                validChi2 ? safeJsonDouble(std::min(1.0, nlf.adjustedRSquare())) : QJsonValue(0.0);
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

        const int bootSamples = params.value(QStringLiteral("bootstrap")).toInt(
                params.value(QStringLiteral("bootstrap_samples")).toInt(0));
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
        const QString modelName = params.value(QStringLiteral("model")).toString().trimmed();
        const QString customFormula = params.value(QStringLiteral("formula")).toString().trimmed();
        if (!modelName.isEmpty()) {
            if (!nlf.setBuiltInModel(modelName))
                throwInvalidParams(QStringLiteral("Unknown built-in model '%1'.").arg(modelName));
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
        const QJsonValue sv = params.value(QStringLiteral("shared"));
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
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("converged")] = ok;
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
                if (g->addFunctionCurve(d_app, FunctionCurve::Normal, QStringList() << expr,
                                        QStringLiteral("x"), QList<double>() << xMin << xMax, 200,
                                        fittedTitle)) {
                    if (QwtPlotCurve *fc = g->curve(g->curves() - 1)) {
                        QPen pen(QColor(QStringLiteral("#D55E00")));
                        pen.setWidthF(1.5);
                        fc->setPen(pen);
                        fittedTitle = fc->title().text();
                    }
                }
            }
            sanitizeGraphFunctionCurves(d_app, g);
            enforceArialGraphFonts(g);
            g->replot();

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
                ? params.value(QStringLiteral("model")).toString()
                : params.value(QStringLiteral("shape")).toString();
        const QString model = rawModelStr.trimmed().toLower();
        const QString customFormula = params.value(QStringLiteral("formula")).toString().trimmed();
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

        const int beforeCurves = g->curves();
        const bool ok = nlf.fitImplicitODR(expr, pNames, initVals);
        const auto &resVec = nlf.results();
        const auto &errVec = nlf.errors();
        if (ok && g->curves() > beforeCurves) {
            if (auto *fc = dynamic_cast<FunctionCurve *>(g->plotItem(g->curves() - 1))) {
                constexpr double kTwoPi = 6.283185307179586476925286766559;
                if (resolvedModel == QStringLiteral("circle") && resVec.size() >= 3) {
                    const double xc = resVec[0];
                    const double yc = resVec[1];
                    const double rAbs = std::abs(resVec[2]);
                    fc->setFunctionType(FunctionCurve::Parametric);
                    fc->setVariable(QStringLiteral("t"));
                    fc->setRange(0.0, kTwoPi);
                    fc->setFormulas(QStringList{
                            QStringLiteral("(%1)+(%2)*cos(t)").arg(xc, 0, 'g', 15).arg(rAbs, 0, 'g', 15),
                            QStringLiteral("(%1)+(%2)*sin(t)").arg(yc, 0, 'g', 15).arg(rAbs, 0, 'g', 15) });
                } else if (resolvedModel == QStringLiteral("ellipse") && resVec.size() >= 5) {
                    const double xc = resVec[0];
                    const double yc = resVec[1];
                    const double aAbs = std::abs(resVec[2]);
                    const double bAbs = std::abs(resVec[3]);
                    const double th = resVec[4];
                    const double ct = std::cos(th);
                    const double st = std::sin(th);
                    fc->setFunctionType(FunctionCurve::Parametric);
                    fc->setVariable(QStringLiteral("t"));
                    fc->setRange(0.0, kTwoPi);
                    fc->setFormulas(QStringList{
                            QStringLiteral("(%1)+(%2)*cos(t)-(%3)*sin(t)")
                                    .arg(xc, 0, 'g', 15)
                                    .arg(aAbs * ct, 0, 'g', 15)
                                    .arg(bAbs * st, 0, 'g', 15),
                            QStringLiteral("(%1)+(%2)*cos(t)+(%3)*sin(t)")
                                    .arg(yc, 0, 'g', 15)
                                    .arg(aAbs * st, 0, 'g', 15)
                                    .arg(bAbs * ct, 0, 'g', 15) });
                }
            }
        }
        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();
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
                   || mStr == QStringLiteral("polynomial")) {
            sm = GraphStatsAndSurfaceFit::Poly2DQuadratic;
        } else if (mStr == QStringLiteral("lorentz2d") || mStr == QStringLiteral("lorentz")) {
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
                    QStringLiteral("Unknown 3D surface fit model '%1' (expected plane|poly2d|gauss2d|lorentz2d|pseudovoigt2d|exp2d|chebyshev2d|custom).")
                            .arg(rawModel));
        }

        GraphStatsAndSurfaceFit::SurfaceFitResult sr;
        if (params.contains(QStringLiteral("matrix"))) {
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
            const int gridSide = std::max(10, params.value(QStringLiteral("grid_side")).toInt(40));
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
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);

        const auto profile = parsePeakProfile(params.value(QStringLiteral("profile")).toString());
        const QString baseStr = params.contains(QStringLiteral("baseline_mode"))
                ? params.value(QStringLiteral("baseline_mode")).toString()
                : params.value(QStringLiteral("baseline")).toString();
        const auto baseMode = parseBaselineMode(baseStr);
        const int reqPeaks = params.value(QStringLiteral("num_peaks")).toInt(
                params.value(QStringLiteral("peak_count")).toInt(
                        params.value(QStringLiteral("peaks")).toInt(0)));
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
        mpf.generateFunction(true, std::max(100, params.value(QStringLiteral("points")).toInt(300)));

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

        if (params.value(QStringLiteral("centers")).isArray()
            && !params.value(QStringLiteral("centers")).toArray().isEmpty()) {
            std::vector<double> pickedCenters;
            for (const QJsonValue &cv : params.value(QStringLiteral("centers")).toArray())
                pickedCenters.push_back(cv.toDouble());
            mpf.setNumPeaks(static_cast<int>(pickedCenters.size()));
            mpf.seedPeaksFromCenters(pickedCenters, prominence);
        } else if (params.value(QStringLiteral("use_second_derivative")).toBool(false)) {
            const int sgWin = std::max(5, params.value(QStringLiteral("sg_window")).toInt(9));
            const QVector<double> d2Centers = mpf.detectPeaksSecondDerivative(sgWin, prominence);
            if (!d2Centers.isEmpty()) {
                std::vector<double> cStd(d2Centers.begin(), d2Centers.end());
                if (reqPeaks > 0 && int(cStd.size()) > reqPeaks)
                    cStd.resize(size_t(reqPeaks));
                mpf.setNumPeaks(static_cast<int>(cStd.size()));
                mpf.seedPeaksFromCenters(cStd, prominence);
            } else {
                mpf.autoDetectAndUpdateNumPeaks(reqPeaks > 0 ? reqPeaks : 10, prominence);
            }
        } else if (reqPeaks > 0) {
            mpf.setNumPeaks(reqPeaks);
            mpf.autoDetectPeaks(reqPeaks, prominence);
        } else {
            const int maxPeaks = std::max(1, params.value(QStringLiteral("max_peaks")).toInt(10));
            mpf.autoDetectAndUpdateNumPeaks(maxPeaks, prominence);
        }

        if (params.value(QStringLiteral("doublets")).isArray()) {
            for (const QJsonValue &dv : params.value(QStringLiteral("doublets")).toArray()) {
                if (!dv.isObject())
                    continue;
                const QJsonObject dobj = dv.toObject();
                mpf.addPeakDoubletConstraint(
                        dobj.value(QStringLiteral("secondary")).toInt(1),
                        dobj.value(QStringLiteral("primary")).toInt(0),
                        dobj.value(QStringLiteral("area_ratio")).toDouble(0.5),
                        dobj.value(QStringLiteral("center_offset")).toDouble(0.0),
                        dobj.value(QStringLiteral("share_width")).toBool(true));
            }
        }

        mpf.fit();
        if (plotBase && plotFit)
            mpf.insertBaselineCurve();

        Table *summaryTbl = createSummary ? mpf.createPeakSummaryTable() : nullptr;
        Table *integTbl = doIntegrate ? mpf.integratePeaksToTable() : nullptr;
        Table *residTbl = createResiduals ? mpf.createResidualsTable() : nullptr;

        sanitizeGraphFunctionCurves(d_app, g);
        enforceArialGraphFonts(g);
        g->replot();

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

        const double r2 = mpf.rSquare();
        QJsonArray peaksArr;
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto &r = rows[i];
            QJsonObject pObj;
            pObj[QStringLiteral("index")] = static_cast<int>(i);
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
        const auto baseMode =
                parseBaselineMode(params.value(QStringLiteral("baseline_mode")).toString());
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
        RoiGadgetTool::clearGadget(g);
        g->replot();
        outResult[QStringLiteral("ok")] = true;
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

        const QString mStr = params.value(QStringLiteral("method")).toString().trimmed().toLower();
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
        else
            throwInvalidParams(
                    QStringLiteral("Unsupported smooth method '%1' (expected 'savgol', 'whittaker', 'lowess', 'median', 'fft', or 'moving_avg').")
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

        int polyOrder = std::min(2, totalWin - 1);
        if (sm == SmoothFilter::SavitzkyGolay && params.contains(QStringLiteral("poly_order"))
            && !params.value(QStringLiteral("poly_order")).isNull()) {
            polyOrder = parseStrictJsonInt(params.value(QStringLiteral("poly_order")),
                                           QStringLiteral("poly_order"));
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

        const bool ok = sf.run() && (g->curves() > curvesBefore);
        enforceArialGraphFonts(g);
        g->replot();
        outResult[QStringLiteral("ok")] = ok;
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
        if (params.contains(QStringLiteral("deriv_order"))
            && !params.value(QStringLiteral("deriv_order")).isNull()) {
            derivOrder = parseStrictJsonInt(params.value(QStringLiteral("deriv_order")),
                                            QStringLiteral("deriv_order"));
            if (derivOrder < 1 || derivOrder > 2)
                throwInvalidParams(QStringLiteral("'deriv_order' must be 1 or 2."));
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
        QwtPlotCurve *outC = AdvancedSignalProc::savitzkyGolayDerivative(d_app, g, curveTitle,
                                                                         derivOrder, win, poly);
        enforceArialGraphFonts(g);
        g->replot();
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
            if (hasExplicitSampling) {
                fft.setSampling(explicitDt);
            } else {
                const double dtGuess = std::abs(xs[1] - xs[0]);
                if (std::isfinite(dtGuess) && dtGuess > 0.0)
                    fft.setSampling(dtGuess);
            }
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
                : params.value(QStringLiteral("low_freq")).toDouble(0.1);
        const double highFreq = params.contains(QStringLiteral("cutoff_high"))
                ? params.value(QStringLiteral("cutoff_high")).toDouble(-1.0)
                : params.value(QStringLiteral("high_freq")).toDouble(1.0);
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
        const bool ok = ff.run() && (g->curves() > curvesBefore);
        enforceArialGraphFonts(g);
        g->replot();
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
                                      ? params.value(QStringLiteral("filter_type")).toString()
                                      : (params.contains(QStringLiteral("pass_type"))
                                                 ? params.value(QStringLiteral("pass_type")).toString()
                                                 : params.value(QStringLiteral("type")).toString()))
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

        QwtPlotCurve *outC = AdvancedSignalProc::iirFilter(d_app, g, curveTitle, design, pass, order,
                                                           fLow, fHigh, zeroPhase, ripple);
        enforceArialGraphFonts(g);
        g->replot();
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
        const int winSz = std::max(8, params.value(QStringLiteral("window_size")).toInt(64));
        const int hopSz = std::max(1, params.value(QStringLiteral("hop_size")).toInt(16));
        const bool plotSpec = params.value(QStringLiteral("plot")).toBool(true);
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
            QwtPlotCurve *lo = nullptr;
            QwtPlotCurve *hi = AdvancedSignalProc::computePeakEnvelope(
                    d_app, g, curveTitle, AdvancedSignalProc::EnvelopeBoth,
                    std::max(1, params.value(QStringLiteral("min_dist")).toInt(3)), &lo);
            enforceArialGraphFonts(g);
            g->replot();
            outResult[QStringLiteral("ok")] = (hi != nullptr);
            if (hi)
                outResult[QStringLiteral("upper_curve")] = hi->title().text();
            if (lo)
                outResult[QStringLiteral("lower_curve")] = lo->title().text();
            return true;
        }
        Table *envTbl = nullptr;
        QwtPlotCurve *envC = AdvancedSignalProc::hilbertEnvelope(
                d_app, g, curveTitle, params.value(QStringLiteral("plot_lower")).toBool(false),
                &envTbl);
        enforceArialGraphFonts(g);
        g->replot();
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
        const int nScales = std::max(4, params.value(QStringLiteral("scales")).toInt(32));
        const QString wt = params.value(QStringLiteral("wavelet")).toString().toLower();
        auto wtype = (wt == QStringLiteral("ricker") || wt == QStringLiteral("mexican_hat"))
                ? AdvancedSignalProc::RickerMexicanHat
                : AdvancedSignalProc::Morlet;
        Matrix *cwtM = AdvancedSignalProc::waveletCWT(
                d_app, g, curveTitle, nScales, wtype,
                params.value(QStringLiteral("plot")).toBool(true));
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
        const QString wt = params.value(QStringLiteral("wavelet")).toString().toLower();
        auto wtype = (wt == QStringLiteral("haar")) ? AdvancedSignalProc::Haar
                                                    : AdvancedSignalProc::Daubechies4;
        const int levels = std::clamp(params.value(QStringLiteral("levels")).toInt(4), 1, 10);
        const bool soft = params.contains(QStringLiteral("soft"))
                ? params.value(QStringLiteral("soft")).toBool(true)
                : true;
        const double scale = params.value(QStringLiteral("threshold_scale")).toDouble(1.0);
        QwtPlotCurve *outC = AdvancedSignalProc::waveletDenoiseDWT(d_app, g, curveTitle, wtype,
                                                                   levels, soft, scale);
        enforceArialGraphFonts(g);
        g->replot();
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
        const int segLen = std::max(16, params.value(QStringLiteral("segment_len")).toInt(128));
        Table *psdTbl = AdvancedSignalProc::welchPSD(
                d_app, g, curveTitle, segLen, params.value(QStringLiteral("plot")).toBool(true));
        if (!psdTbl)
            throwRpcRuntime(QStringLiteral("Welch PSD computation failed."));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = psdTbl->name();
        return true;
    }

    if (method == QStringLiteral("signal.coherence")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        QJsonObject pA = params, pB = params;
        const QString cA = resolveCurveTitle(g, pA, QStringLiteral("curve_a"), true);
        const QString cB = resolveCurveTitle(g, pB, QStringLiteral("curve_b"), true);
        requireCurveMinPoints(g, cA, 8, QStringLiteral("signal.coherence"));
        requireCurveMinPoints(g, cB, 8, QStringLiteral("signal.coherence"));
        const int segLen = std::max(16, params.value(QStringLiteral("segment_len")).toInt(128));
        Table *cohTbl = AdvancedSignalProc::crossCoherence(
                d_app, g, cA, cB, segLen, params.value(QStringLiteral("plot")).toBool(true));
        if (!cohTbl)
            throwRpcRuntime(QStringLiteral("Cross-coherence computation failed."));
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("table")] = cohTbl->name();
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
        QwtPlotCurve *outC = AdvancedSignalProc::decimateCurve(d_app, g, curveTitle, factor, aa);
        enforceArialGraphFonts(g);
        g->replot();
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
        const double gamma = std::max(1e-6, params.value(QStringLiteral("fwhm")).toDouble(5.0));
        const double kSharp = std::max(1.01, params.value(QStringLiteral("k")).toDouble(1.8));
        const int apod = (params.value(QStringLiteral("apodization")).toString().toLower()
                          == QStringLiteral("gaussian"))
                ? 1
                : 0;
        QwtPlotCurve *outC = AdvancedSignalProc::fourierSelfDeconvolve(d_app, g, curveTitle, gamma,
                                                                       kSharp, apod);
        enforceArialGraphFonts(g);
        g->replot();
        outResult[QStringLiteral("ok")] = (outC != nullptr);
        if (outC)
            outResult[QStringLiteral("output_curve")] = outC->title().text();
        return true;
    }

    // ------------------------------------------------------------------------
    // 7C. Curve Math & Materials Science (curvemath.*)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("curvemath.eval")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString cA = resolveCurveTitle(g, params, QStringLiteral("curve_a"), true);
        const QString cB = resolveCurveTitle(g, params, QStringLiteral("curve_b"), true);
        requireCurveMinPoints(g, cA, 2, QStringLiteral("curvemath.eval"));
        requireCurveMinPoints(g, cB, 2, QStringLiteral("curvemath.eval"));
        const QString opStr = params.value(QStringLiteral("op")).toString().trimmed();
        CurveMathTools::MathOp op = CurveMathTools::Subtract;
        if (opStr == QStringLiteral("+") || opStr.compare(QStringLiteral("add"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Add;
        else if (opStr == QStringLiteral("-")
                 || opStr.compare(QStringLiteral("subtract"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Subtract;
        else if (opStr == QStringLiteral("*")
                 || opStr.compare(QStringLiteral("multiply"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Multiply;
        else if (opStr == QStringLiteral("/")
                 || opStr.compare(QStringLiteral("divide"), Qt::CaseInsensitive) == 0)
            op = CurveMathTools::Divide;
        else
            throwInvalidParams(
                    QStringLiteral("Unsupported curve math op '%1' (expected '+'|'-'|'*'|'/').")
                            .arg(opStr));

        QwtPlotCurve *outC = CurveMathTools::simpleCurveMath(
                d_app, g, cA, cB, op, params.value(QStringLiteral("union_grid")).toBool(false));
        enforceArialGraphFonts(g);
        g->replot();
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
        Table *outTbl = nullptr;
        QwtPlotCurve *avgC = CurveMathTools::averageMultipleCurves(
                d_app, g, titles, std::max(10, params.value(QStringLiteral("points")).toInt(200)),
                params.value(QStringLiteral("sd_band")).toBool(true), &outTbl);
        enforceArialGraphFonts(g);
        g->replot();
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

        QwtPlotCurve *outC = CurveMathTools::normalizeCurve(
                d_app, g, curveTitle, nm, params.value(QStringLiteral("ref_x")).toDouble(0.0));
        enforceArialGraphFonts(g);
        g->replot();
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
        requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("curvemath.integrate"));
        RpcIntegration integ(d_app, g);
        bool okData = false;
        if (params.contains(QStringLiteral("x_min")) && params.contains(QStringLiteral("x_max"))) {
            const double xMin = params.value(QStringLiteral("x_min")).toDouble();
            const double xMax = params.value(QStringLiteral("x_max")).toDouble();
            if (!std::isfinite(xMin) || !std::isfinite(xMax) || xMin >= xMax)
                throwInvalidParams(QStringLiteral("curvemath.integrate requires finite x_min < x_max."));
            okData = integ.setDataFromCurve(curveTitle, xMin, xMax, g);
        } else {
            okData = integ.setDataFromCurve(curveTitle, g);
        }
        if (!okData || integ.error())
            throwInvalidParams(QStringLiteral("Insufficient curve data in range for integration on '%1'.")
                                       .arg(curveTitle));
        const bool ok = integ.run();
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("area")] = safeJsonDouble(integ.result());
        return true;
    }

    if (method == QStringLiteral("curvemath.differentiate")) {
        MultiLayer *ml = nullptr;
        Graph *g = nullptr;
        QString curveTitle;
        resolveGraphAndCurveOrAutoPlot(d_app, params, ml, g, curveTitle);
        requireCurveMinPoints(g, curveTitle, 2, QStringLiteral("curvemath.differentiate"));
        RpcDifferentiation diff(d_app, g);
        if (!diff.setDataFromCurve(curveTitle, g) || diff.error())
            throwInvalidParams(
                    QStringLiteral("Insufficient curve data for differentiation on '%1'.")
                            .arg(curveTitle));
        const bool ok = diff.run();
        outResult[QStringLiteral("ok")] = ok;
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
        {
            std::vector<double> xs, ys;
            extractValidUnmaskedCurveXY(g->curve(g->curveIndex(curveTitle)), xs, ys);
            if (static_cast<int>(xs.size()) < minPts) {
                throwInvalidParams(
                        QStringLiteral("curvemath.interpolate requires >= %1 valid unmasked points.")
                                .arg(minPts));
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
                        "curvemath.interpolate requires strictly monotonic X values without duplicates."));
            }
        }
        int outPts = 200;
        if (params.contains(QStringLiteral("points")) && !params.value(QStringLiteral("points")).isNull()) {
            outPts = parseStrictJsonInt(params.value(QStringLiteral("points")), QStringLiteral("points"));
            if (outPts < 2)
                throwInvalidParams(QStringLiteral("curvemath.interpolate 'points' must be >= 2."));
        }
        const int curvesBefore = g->curves();
        RpcInterpolation interp(d_app, g, curveTitle, im);
        interp.setOutputPoints(outPts);
        const bool ok = interp.run() && (g->curves() > curvesBefore);
        enforceArialGraphFonts(g);
        g->replot();
        outResult[QStringLiteral("ok")] = ok;
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
        const QPointF p1(params.value(QStringLiteral("x1")).toDouble(),
                         params.value(QStringLiteral("y1")).toDouble());
        const QPointF p2(params.value(QStringLiteral("x2")).toDouble(),
                         params.value(QStringLiteral("y2")).toDouble());
        QwtPlotCurve *outC = CurveMathTools::subtractStraightLine(
                d_app, g, curveTitle, p1, p2,
                params.value(QStringLiteral("in_place")).toBool(false));
        enforceArialGraphFonts(g);
        g->replot();
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
            bool ok = false;
            const double yVal = CurveMathTools::findYFromX(
                    g, curveTitle, params.value(QStringLiteral("x")).toDouble(), &ok);
            outResult[QStringLiteral("ok")] = ok;
            outResult[QStringLiteral("y")] = ok ? safeJsonDouble(yVal) : QJsonValue::Null;
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
        const QString cA = resolveCurveTitle(g, params, QStringLiteral("curve_a"), true);
        const QString cB = resolveCurveTitle(g, params, QStringLiteral("curve_b"), true);
        const QColor fillCol = parseColorValue(params.value(QStringLiteral("color")),
                                               QColor(30, 102, 184, 55));
        QwtPlotCurve *fc = CurveMathTools::fillBetweenCurves(d_app, g, cA, cB, fillCol);
        g->replot();
        outResult[QStringLiteral("ok")] = (fc != nullptr);
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
        const bool celsius = params.value(QStringLiteral("celsius")).toBool(false);
        const bool use1000 = params.contains(QStringLiteral("use_1000_over_t"))
                ? params.value(QStringLiteral("use_1000_over_t")).toBool(true)
                : true;
        const bool sigmaT = params.value(QStringLiteral("sigma_times_t")).toBool(false);
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
        const QString alphaCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("alpha_col"))
                                           ? params.value(QStringLiteral("alpha_col"))
                                           : QJsonValue(1),
                                   true));
        const bool isWavelength = params.contains(QStringLiteral("wavelength_nm"))
                ? params.value(QStringLiteral("wavelength_nm")).toBool(true)
                : !params.value(QStringLiteral("energy_ev")).toBool(false);
        const bool direct = params.contains(QStringLiteral("direct"))
                ? params.value(QStringLiteral("direct")).toBool(true)
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
        const bool negImag = params.value(QStringLiteral("z_imag_negated")).toBool(false);
        const bool fitCpe = params.contains(QStringLiteral("fit_cpe"))
                ? params.value(QStringLiteral("fit_cpe")).toBool(true)
                : true;
        const auto eis = CurveMathTools::analyzeEIS(d_app, t, fCol, zrCol, ziCol, negImag, fitCpe);
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

    // ------------------------------------------------------------------------
    // 7D. Parametric, Nonparametric, Survival, Reliability & Multivariate Stats
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("stats.normality")) {
        Table *t = resolveTable(d_app, params);
        const QString colName = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("col"))
                                           ? params.value(QStringLiteral("col"))
                                           : QJsonValue(0),
                                   true));
        Graph *overlayG = nullptr;
        if (params.contains(QStringLiteral("graph"))) {
            if (MultiLayer *ml = resolveMultiLayer(d_app, params, QStringLiteral("graph"), false))
                overlayG = resolveGraphLayer(ml, params, false);
        }
        const auto nr = GraphStatsAndSurfaceFit::runNormalityTest(d_app, t, colName, overlayG);
        outResult[QStringLiteral("ok")] = (nr.n >= 3);
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
        const QString mStr = params.value(QStringLiteral("method")).toString().toLower();
        int om = 0; // Grubbs
        if (mStr == QStringLiteral("gesd") || mStr == QStringLiteral("esd"))
            om = 1;
        else if (mStr == QStringLiteral("dixon") || mStr == QStringLiteral("dixon_q"))
            om = 2;
        const double alpha = params.value(QStringLiteral("alpha")).toDouble(0.05);
        const int maxOut = std::max(1, params.value(QStringLiteral("max_outliers")).toInt(5));
        const bool maskDet = params.value(QStringLiteral("mask_outliers")).toBool(false);
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

        // Collect valid unmasked groups with >= 2 finite observations first
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
            // Only include groups with >= 2 valid observations so singletons never corrupt df1/df2
            if (gv.size() >= 2) {
                const double m = std::accumulate(gv.begin(), gv.end(), 0.0) / double(gv.size());
                double ss = 0.0;
                for (double v : gv)
                    ss += (v - m) * (v - m);
                const double s2 = ss / double(gv.size() - 1);
                for (double v : gv)
                    grandSum += v;
                N += static_cast<int>(gv.size());
                groups.push_back({ cn, std::move(gv), m, s2 });
            }
        }
        if (groups.size() < 2) {
            throwInvalidParams(QStringLiteral(
                    "One-way ANOVA requires at least 2 groups each having >= 2 valid unmasked numeric values."));
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
        if (params.contains(QStringLiteral("graph"))
            && !params.value(QStringLiteral("graph")).isNull()) {
            if (MultiLayer *ml = resolveMultiLayer(d_app, params, QStringLiteral("graph"), false))
                annG = resolveGraphLayer(ml, params, false);
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
        if (df1 > 0 && df2 > 0 && ssw > 0.0) {
            fStat = (ssb / double(df1)) / msw;
            pVal = gsl_cdf_fdist_Q(fStat, double(df1), double(df2));
        }

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
                        const double va = ga.var / na;
                        const double vb = gb.var / nb;
                        const double vSum = va + vb;
                        const double seQ = (vSum > 0.0) ? std::sqrt(0.5 * vSum) : 0.0;
                        const double denom = (va * va) / (na - 1.0) + (vb * vb) / (nb - 1.0);
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
                        // Welch t-test + Holm-Bonferroni step-down adjustment
                        const double va = ga.var / na;
                        const double vb = gb.var / nb;
                        const double vSum = va + vb;
                        const double seW = (vSum > 0.0) ? std::sqrt(vSum) : 0.0;
                        const double denom = (va * va) / (na - 1.0) + (vb * vb) / (nb - 1.0);
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
        outResult[QStringLiteral("comparisons")] = compArr;
        outResult[QStringLiteral("table")] = repTbl->name();
        outResult[QStringLiteral("report")] = extractTableAsJson(repTbl);
        return true;
    }

    if (method == QStringLiteral("stats.anova_twoway")) {
        Table *t = resolveTable(d_app, params);
        const QString fA = t->colLabel(
                resolveColumnIndex(t, params.value(QStringLiteral("factor_a")), true));
        const QString fB = t->colLabel(
                resolveColumnIndex(t, params.value(QStringLiteral("factor_b")), true));
        const QString resp = t->colLabel(
                resolveColumnIndex(t, params.value(QStringLiteral("response")), true));
        const bool plotInt = params.contains(QStringLiteral("plot"))
                ? params.value(QStringLiteral("plot")).toBool(true)
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
        const QString mStr = params.value(QStringLiteral("method")).toString().trimmed().toLower();
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
        if (mStr == QStringLiteral("mann_whitney") || mStr == QStringLiteral("mann_whitney_u"))
            nm = GraphStatsAndSurfaceFit::TestMannWhitneyU;
        else if (mStr == QStringLiteral("wilcoxon"))
            nm = GraphStatsAndSurfaceFit::TestWilcoxonSignedRank;
        else if (mStr == QStringLiteral("kruskal_wallis") || mStr == QStringLiteral("kruskal"))
            nm = GraphStatsAndSurfaceFit::TestKruskalWallisDunn;

        const auto np = GraphStatsAndSurfaceFit::runNonparametricComparison(d_app, t, colNames, nm);
        outResult[QStringLiteral("ok")] = np.valid;
        outResult[QStringLiteral("test")] = np.chosenTestName;
        outResult[QStringLiteral("statistic")] = safeJsonDouble(np.statistic);
        outResult[QStringLiteral("p_value")] = safeJsonDouble(np.pValue);
        if (np.reportTable) {
            outResult[QStringLiteral("table")] = np.reportTable->name();
            outResult[QStringLiteral("report")] = extractTableAsJson(np.reportTable);
        }
        return true;
    }

    if (method == QStringLiteral("stats.paired_plot")) {
        Table *t = resolveTable(d_app, params);
        const QString bCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("before_col"))
                                           ? params.value(QStringLiteral("before_col"))
                                           : QJsonValue(0),
                                   true));
        const QString aCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("after_col"))
                                           ? params.value(QStringLiteral("after_col"))
                                           : QJsonValue(1),
                                   true));
        double pVal = 1.0;
        MultiLayer *ml = GraphStatsAndSurfaceFit::plotPairedComparison(
                d_app, t, bCol, aCol, params.value(QStringLiteral("wilcoxon")).toBool(false),
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
        const QString dCol = t->colLabel(
                resolveColumnIndex(t, params.contains(QStringLiteral("col"))
                                           ? params.value(QStringLiteral("col"))
                                           : QJsonValue(0),
                                   true));
        QString cCol;
        if (params.contains(QStringLiteral("censor_col"))
            && !params.value(QStringLiteral("censor_col")).isNull()) {
            cCol = t->colLabel(
                    resolveColumnIndex(t, params.value(QStringLiteral("censor_col")), true));
        }
        const int censorCoding = params.value(QStringLiteral("censor_coding")).toInt(0);
        const bool threeP = params.value(QStringLiteral("three_param")).toBool(false);
        const auto wb = GraphStatsAndSurfaceFit::plotWeibullProbability(d_app, t, dCol, cCol,
                                                                        censorCoding, threeP);
        if (wb.weibullPlot) {
            for (Graph *lg : wb.weibullPlot->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = wb.valid;
        outResult[QStringLiteral("beta")] = safeJsonDouble(wb.modulusM);
        outResult[QStringLiteral("modulus_m")] = safeJsonDouble(wb.modulusM);
        outResult[QStringLiteral("beta_se")] = safeJsonDouble(wb.modulusMSE);
        outResult[QStringLiteral("eta")] = safeJsonDouble(wb.scaleSigma0);
        outResult[QStringLiteral("scale_sigma0")] = safeJsonDouble(wb.scaleSigma0);
        outResult[QStringLiteral("eta_se")] = safeJsonDouble(wb.scaleSigma0SE);
        outResult[QStringLiteral("gamma")] = safeJsonDouble(wb.thresholdGamma);
        outResult[QStringLiteral("three_param")] = wb.threeParameter;
        outResult[QStringLiteral("censored_count")] = wb.censoredCount;
        outResult[QStringLiteral("b10")] = safeJsonDouble(wb.b10Life);
        outResult[QStringLiteral("b50")] = safeJsonDouble(wb.b50Life);
        outResult[QStringLiteral("r2")] = safeJsonDouble(wb.rSquare);
        if (wb.reportTable)
            outResult[QStringLiteral("table")] = wb.reportTable->name();
        if (wb.weibullPlot)
            outResult[QStringLiteral("graph")] = wb.weibullPlot->name();
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
        const int scIdx = resolveColumnIndex(t, params.contains(QStringLiteral("score_col"))
                                                     ? params.value(QStringLiteral("score_col"))
                                                     : QJsonValue(1),
                                             true);
        double aucPlot = 0.5;
        MultiLayer *ml = GraphStatsAndSurfaceFit::plotROCCurve(d_app, t, t->colLabel(stIdx),
                                                               t->colLabel(scIdx), &aucPlot);
        if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
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
        outResult[QStringLiteral("ok")] = (ml != nullptr && rm.nPos > 0 && rm.nNeg > 0);
        outResult[QStringLiteral("auc")] = safeJsonDouble(finalAuc);
        outResult[QStringLiteral("se")] = safeJsonDouble(rm.se);
        outResult[QStringLiteral("optimal_threshold")] = safeJsonDouble(rm.optimalThreshold);
        outResult[QStringLiteral("sensitivity")] = safeJsonDouble(rm.sensitivity);
        outResult[QStringLiteral("specificity")] = safeJsonDouble(rm.specificity);
        outResult[QStringLiteral("n_pos")] = rm.nPos;
        outResult[QStringLiteral("n_neg")] = rm.nNeg;
        if (ml)
            outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("stats.pca")) {
        Table *t = resolveTable(d_app, params);
        QStringList colNames;
        if (params.value(QStringLiteral("cols")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("cols")).toArray())
                colNames << t->colLabel(resolveColumnIndex(t, cv, true));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric)
                    colNames << t->colLabel(c);
            }
        }
        Table *scoresTbl = nullptr;
        MultiLayer *ml = GraphStatsAndSurfaceFit::runPCAAndBiplot(d_app, t, colNames, &scoresTbl);
        if (ml) {
            for (Graph *lg : ml->layersList())
                enforceArialGraphFonts(lg);
        }
        outResult[QStringLiteral("ok")] = (ml != nullptr);
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
        const bool spearman = (params.value(QStringLiteral("method")).toString().toLower()
                               == QStringLiteral("spearman"))
                || params.value(QStringLiteral("spearman")).toBool(false);
        Matrix *corrM = nullptr;
        MultiLayer *ml = GraphStatsAndSurfaceFit::plotCorrelationMatrixHeatmap(
                d_app, t, colNames, spearman, &corrM);
        if (ml) {
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
        QJsonArray rRows, pRows;
        if (corrM) {
            const int k = corrM->numRows();
            for (int i = 0; i < k; ++i) {
                QJsonArray rRow, pRow;
                for (int j = 0; j < corrM->numCols(); ++j) {
                    const double rv = corrM->cell(i, j);
                    rRow.append(safeJsonDouble(rv));
                    if (i == j) {
                        pRow.append(0.0);
                    } else {
                        const int nij = countPairwiseValid(i, j);
                        if (nij < 3 || !std::isfinite(rv)) {
                            pRow.append(1.0);
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
        if (params.value(QStringLiteral("cols")).isArray()) {
            for (const QJsonValue &cv : params.value(QStringLiteral("cols")).toArray())
                colNames << t->colLabel(resolveColumnIndex(t, cv, true));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->column(c) && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric)
                    colNames << t->colLabel(c);
            }
        }
        const QString alg = params.value(QStringLiteral("method")).toString().toLower();
        if (alg == QStringLiteral("hierarchical") || alg == QStringLiteral("dendrogram")) {
            const QString lnkStr = params.value(QStringLiteral("linkage")).toString().toLower();
            int lnk = 0; // Ward
            if (lnkStr == QStringLiteral("complete"))
                lnk = 1;
            else if (lnkStr == QStringLiteral("average"))
                lnk = 2;
            MultiLayer *ml = GraphStatsAndSurfaceFit::plotHierarchicalDendrogram(d_app, t, colNames,
                                                                                 lnk);
            if (ml) {
                for (Graph *lg : ml->layersList())
                    enforceArialGraphFonts(lg);
            }
            outResult[QStringLiteral("ok")] = (ml != nullptr);
            if (ml)
                outResult[QStringLiteral("graph")] = ml->name();
            return true;
        }
        const int k = std::max(2, params.value(QStringLiteral("k")).toInt(3));
        const bool plotC = params.contains(QStringLiteral("plot"))
                ? params.value(QStringLiteral("plot")).toBool(true)
                : true;
        const auto km =
                GraphStatsAndSurfaceFit::runKMeansClustering(d_app, t, colNames, k, plotC);
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
        const double conf = params.value(QStringLiteral("conf_level")).toDouble(0.95);
        const bool pred = params.value(QStringLiteral("prediction")).toBool(false);
        QwtPlotCurve *ec =
                GraphStatsAndSurfaceFit::addConfidenceEllipse(d_app, g, curveTitle, conf, pred);
        enforceArialGraphFonts(g);
        g->replot();
        outResult[QStringLiteral("ok")] = (ec != nullptr);
        if (ec)
            outResult[QStringLiteral("ellipse_curve")] = ec->title().text();
        return true;
    }

    // ------------------------------------------------------------------------
    // 7E. Graph Image Digitizer (digitizer.extract)
    // ------------------------------------------------------------------------
    if (method == QStringLiteral("digitizer.extract")) {
        const QString path = params.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path))
            throwInvalidParams(QStringLiteral("Image file not found: %1").arg(path));
        QImage img(path);
        if (img.isNull())
            throwRpcRuntime(QStringLiteral("Failed to decode image '%1'.").arg(path));

        GraphDigitizer::AxisCalibration cal;
        cal.pixelX1 = QPointF(params.value(QStringLiteral("px_x1")).toDouble(0.0),
                              params.value(QStringLiteral("py_x1")).toDouble(img.height() - 1));
        cal.pixelX2 = QPointF(params.value(QStringLiteral("px_x2")).toDouble(img.width() - 1),
                              params.value(QStringLiteral("py_x2")).toDouble(img.height() - 1));
        cal.valueX1 = params.value(QStringLiteral("val_x1")).toDouble(0.0);
        cal.valueX2 = params.value(QStringLiteral("val_x2")).toDouble(1.0);
        cal.logX = params.value(QStringLiteral("log_x")).toBool(false);

        cal.pixelY1 = QPointF(params.value(QStringLiteral("px_y1")).toDouble(0.0),
                              params.value(QStringLiteral("py_y1")).toDouble(img.height() - 1));
        cal.pixelY2 = QPointF(params.value(QStringLiteral("px_y2")).toDouble(0.0),
                              params.value(QStringLiteral("py_y2")).toDouble(0.0));
        cal.valueY1 = params.value(QStringLiteral("val_y1")).toDouble(0.0);
        cal.valueY2 = params.value(QStringLiteral("val_y2")).toDouble(1.0);
        cal.logY = params.value(QStringLiteral("log_y")).toBool(false);

        const QColor targetColor = parseColorValue(params.value(QStringLiteral("color")),
                                                   QColor(220, 30, 30));
        const int tol = std::clamp(params.value(QStringLiteral("tolerance")).toInt(50), 1, 255);
        const int nSamples =
                std::clamp(params.value(QStringLiteral("samples")).toInt(100), 5, 4000);
        QRect roi;
        if (params.contains(QStringLiteral("roi")) && params.value(QStringLiteral("roi")).isArray()) {
            const QJsonArray ra = params.value(QStringLiteral("roi")).toArray();
            if (ra.size() >= 4)
                roi = QRect(ra[0].toInt(), ra[1].toInt(), ra[2].toInt(), ra[3].toInt());
        }

        const QVector<QPointF> pts = GraphDigitizer::autoTraceCurveByColor(img, cal, targetColor,
                                                                           tol, nSamples, roi);
        MultiLayer *outPlot = nullptr;
        MultiLayer **plotPtr = params.value(QStringLiteral("plot")).toBool(false) ? &outPlot : nullptr;
        Table *outTbl = GraphDigitizer::exportPointsToTable(
                d_app, pts, params.value(QStringLiteral("table_name")).toString(), plotPtr);
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

} // namespace

QJsonObject PythonAutomationServer::dispatchMethod(const QString &method,
                                                   const QJsonObject &params)
{
    struct ScopedNonInteractiveGuard
    {
        QCoreApplication *appInstance;
        QVariant prevProp;
        explicit ScopedNonInteractiveGuard(QCoreApplication *qa)
            : appInstance(qa),
              prevProp(qa ? qa->property("torigraph_noninteractive") : QVariant())
        {
            if (appInstance)
                appInstance->setProperty("torigraph_noninteractive", true);
        }
        ~ScopedNonInteractiveGuard()
        {
            if (appInstance)
                appInstance->setProperty("torigraph_noninteractive", prevProp);
        }
    } nonInteractiveGuard(QCoreApplication::instance());

    if (d_custom_hook) {
        bool handled = false;
        const QJsonObject hookedRes = d_custom_hook(method, params, &handled);
        if (handled)
            return hookedRes;
    }

    QJsonObject result;
    if (dispatchCoreAndGraphMethod(this, d_app, d_focus_hud_fallback, d_workspace_mode_fallback,
                                   method, params, result)) {
        return result;
    }
    if (dispatchAnalysisAndScienceMethod(d_app, method, params, result)) {
        return result;
    }

    throw RpcDispatchException(-32601, QStringLiteral("Method not found: %1").arg(method));
}


// ============================================================================
// In-App Modeless Python Automation Console (PythonConsoleDialog)
// ============================================================================

QPointer<PythonConsoleDialog> PythonConsoleDialog::s_instance;

PythonConsoleDialog::PythonConsoleDialog(ApplicationWindow *app, PythonAutomationServer *server,
                                         QWidget *parent)
    : QDialog(parent ? parent : reinterpret_cast<QWidget *>(app)), d_app(app), d_server(server)
{
    setObjectName(QStringLiteral("PythonConsoleDialog"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::NonModal);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(840, 640);

    buildUi();
    retranslateUi();
    refreshEndpointDisplay();
}

PythonConsoleDialog::~PythonConsoleDialog()
{
    stopRunningScript();
    cleanupTempScript();
    delete d_stdout_decoder;
    d_stdout_decoder = nullptr;
    delete d_stderr_decoder;
    d_stderr_decoder = nullptr;
}

void PythonConsoleDialog::resetStreamDecoders()
{
    delete d_stdout_decoder;
    d_stdout_decoder = nullptr;
    delete d_stderr_decoder;
    d_stderr_decoder = nullptr;
    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        d_stdout_decoder = utf8->makeDecoder(QTextCodec::IgnoreHeader);
        d_stderr_decoder = utf8->makeDecoder(QTextCodec::IgnoreHeader);
    }
}

void PythonConsoleDialog::closeEvent(QCloseEvent *event)
{
    stopRunningScript();
    QDialog::closeEvent(event);
}

void PythonConsoleDialog::reject()
{
    stopRunningScript();
    QDialog::reject();
}

PythonConsoleDialog *PythonConsoleDialog::showDialog(ApplicationWindow *app,
                                                     PythonAutomationServer *server)
{
    if (server && !server->isTcpListening() && !server->isLocalPipeListening())
        server->startTcp(0, QHostAddress::LocalHost);

    if (!s_instance) {
        s_instance = new PythonConsoleDialog(app, server, reinterpret_cast<QWidget *>(app));
    } else {
        s_instance->d_app = app;
        s_instance->d_server = server;
        s_instance->retranslateUi();
        s_instance->refreshEndpointDisplay();
    }

    s_instance->show();
    s_instance->raise();
    s_instance->activateWindow();
    return s_instance.data();
}

void PythonConsoleDialog::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);

    // Top row: Endpoint + Example Selector
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    d_endpoint_label = new QLabel(this);
    d_endpoint_edit = new QLineEdit(this);
    d_endpoint_edit->setReadOnly(true);
    d_endpoint_edit->setFont(monoFont);
    d_endpoint_edit->setMinimumWidth(210);

    d_example_label = new QLabel(this);
    d_example_combo = new QComboBox(this);
    for (int i = 0; i < 6; ++i)
        d_example_combo->addItem(QString());
    d_example_combo->setMinimumWidth(260);

    topRow->addWidget(d_endpoint_label);
    topRow->addWidget(d_endpoint_edit, 1);
    topRow->addSpacing(8);
    topRow->addWidget(d_example_label);
    topRow->addWidget(d_example_combo, 1);
    rootLayout->addLayout(topRow);

    // Button bar
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    d_run_btn = new QPushButton(this);
    d_run_btn->setDefault(true);
    d_stop_btn = new QPushButton(this);
    d_stop_btn->setEnabled(false);
    d_open_btn = new QPushButton(this);
    d_save_btn = new QPushButton(this);
    d_clear_btn = new QPushButton(this);
    d_close_btn = new QPushButton(this);

    btnRow->addWidget(d_run_btn);
    btnRow->addWidget(d_stop_btn);
    btnRow->addSpacing(8);
    btnRow->addWidget(d_open_btn);
    btnRow->addWidget(d_save_btn);
    btnRow->addWidget(d_clear_btn);
    btnRow->addStretch(1);
    btnRow->addWidget(d_close_btn);
    rootLayout->addLayout(btnRow);

    // Vertical splitter: Editor (top) + Output (bottom)
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);

    auto *editorPane = new QWidget(splitter);
    auto *editorLayout = new QVBoxLayout(editorPane);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(4);
    d_editor_label = new QLabel(editorPane);
    d_editor = new QPlainTextEdit(editorPane);
    d_editor->setFont(monoFont);
    d_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    d_editor->setTabStopDistance(32.0);
#endif
    d_editor->setPlainText(exampleScriptCode(1));
    editorLayout->addWidget(d_editor_label);
    editorLayout->addWidget(d_editor, 1);

    auto *outputPane = new QWidget(splitter);
    auto *outputLayout = new QVBoxLayout(outputPane);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(4);
    d_output_label = new QLabel(outputPane);
    d_output = new QPlainTextEdit(outputPane);
    d_output->setReadOnly(true);
    d_output->setFont(monoFont);
    outputLayout->addWidget(d_output_label);
    outputLayout->addWidget(d_output, 1);

    splitter->addWidget(editorPane);
    splitter->addWidget(outputPane);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    rootLayout->addWidget(splitter, 1);

    // Connections
    connect(d_run_btn, &QPushButton::clicked, this, &PythonConsoleDialog::runCurrentScript);
    connect(d_stop_btn, &QPushButton::clicked, this, &PythonConsoleDialog::stopRunningScript);
    connect(d_open_btn, &QPushButton::clicked, this, &PythonConsoleDialog::openScriptDialog);
    connect(d_save_btn, &QPushButton::clicked, this, &PythonConsoleDialog::saveScriptDialog);
    connect(d_clear_btn, &QPushButton::clicked, this, &PythonConsoleDialog::clearOutput);
    connect(d_close_btn, &QPushButton::clicked, this, &QDialog::close);
    connect(d_example_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PythonConsoleDialog::onExampleSelected);

    auto *scReturn = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), this);
    connect(scReturn, &QShortcut::activated, this, &PythonConsoleDialog::runCurrentScript);
    auto *scEnter = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Enter), this);
    connect(scEnter, &QShortcut::activated, this, &PythonConsoleDialog::runCurrentScript);
}

void PythonConsoleDialog::retranslateUi()
{
    setWindowTitle(tr("Python Automation Console"));

    if (d_endpoint_label)
        d_endpoint_label->setText(tr("RPC Endpoint:"));
    if (d_endpoint_edit) {
        d_endpoint_edit->setToolTip(
                tr("Active loopback JSON-RPC 2.0 endpoint used by torigraph.connect()"));
    }
    if (d_example_label)
        d_example_label->setText(tr("Examples:"));

    if (d_example_combo && d_example_combo->count() >= 6) {
        const bool oldBlock = d_example_combo->blockSignals(true);
        d_example_combo->setItemText(0, tr("Select Example Workflow..."));
        d_example_combo->setItemText(1, tr("1. ISIJ 85mm Stress-Strain Plot"));
        d_example_combo->setItemText(2, tr("2. Pseudo-Voigt TDS Peak Deconvolution"));
        d_example_combo->setItemText(3, tr("3. EIS Randles Nyquist Fit"));
        d_example_combo->setItemText(4, tr("4. Weibull 3P Reliability & ANOVA"));
        d_example_combo->setItemText(5, tr("5. 2D Matrix Filter & Heatmap"));
        d_example_combo->blockSignals(oldBlock);
    }

    if (d_run_btn) {
        d_run_btn->setText(tr("Run (Ctrl+Enter)"));
        d_run_btn->setToolTip(tr("Execute the Python script against this ToriGraph instance"));
    }
    if (d_stop_btn) {
        d_stop_btn->setText(tr("Stop"));
        d_stop_btn->setToolTip(tr("Terminate the currently running Python script"));
    }
    if (d_open_btn)
        d_open_btn->setText(tr("Open .py..."));
    if (d_save_btn)
        d_save_btn->setText(tr("Save .py..."));
    if (d_clear_btn)
        d_clear_btn->setText(tr("Clear Output"));
    if (d_close_btn)
        d_close_btn->setText(tr("Close"));

    if (d_editor_label)
        d_editor_label->setText(tr("Python Script (import torigraph as tg):"));
    if (d_output_label)
        d_output_label->setText(tr("Output (stdout / stderr):"));

    if (d_editor) {
        d_editor->setPlaceholderText(tr(
                "Write Python code using 'import torigraph as tg' and press Ctrl+Enter to run..."));
    }
    if (d_output)
        d_output->setPlaceholderText(tr("Script output will appear here."));

    refreshEndpointDisplay();
}

void PythonConsoleDialog::changeEvent(QEvent *event)
{
    if (event && event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void PythonConsoleDialog::refreshEndpointDisplay()
{
    if (!d_endpoint_edit)
        return;
    if (d_server && !d_server->endpointString().isEmpty())
        d_endpoint_edit->setText(d_server->endpointString());
    else
        d_endpoint_edit->setText(tr("Not listening"));
}

bool PythonConsoleDialog::loadScriptFile(const QString &pyFilePath)
{
    QFile f(pyFilePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendOutput(tr("[Error] Failed to read script file: %1\n").arg(pyFilePath), true);
        return false;
    }
    const QString code = QString::fromUtf8(f.readAll());
    f.close();
    d_current_file_path = QFileInfo(pyFilePath).absoluteFilePath();
    if (d_editor)
        d_editor->setPlainText(code);
    return true;
}

bool PythonConsoleDialog::saveScriptFile(const QString &pyFilePath)
{
    QSaveFile f(pyFilePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendOutput(tr("[Error] Failed to save script file: %1\n").arg(pyFilePath), true);
        return false;
    }
    const QByteArray bytes = d_editor ? d_editor->toPlainText().toUtf8() : QByteArray();
    f.write(bytes);
    if (!f.commit()) {
        appendOutput(tr("[Error] Failed to save script file: %1\n").arg(pyFilePath), true);
        return false;
    }
    d_current_file_path = QFileInfo(pyFilePath).absoluteFilePath();
    appendOutput(tr("[Saved] %1\n").arg(d_current_file_path), false);
    return true;
}

void PythonConsoleDialog::loadAndRunScriptFile(const QString &pyFilePath)
{
    if (loadScriptFile(pyFilePath))
        runCurrentScript();
}

void PythonConsoleDialog::openScriptDialog()
{
    const QString startDir = d_current_file_path.isEmpty()
            ? QDir::homePath()
            : QFileInfo(d_current_file_path).absolutePath();
    const QString chosen = QFileDialog::getOpenFileName(
            this, tr("Open Python Automation Script"), startDir,
            tr("Python Scripts (*.py);;All Files (*)"));
    if (!chosen.isEmpty())
        loadScriptFile(chosen);
}

void PythonConsoleDialog::saveScriptDialog()
{
    const QString startPath = d_current_file_path.isEmpty()
            ? QDir(QDir::homePath()).filePath(QStringLiteral("torigraph_script.py"))
            : d_current_file_path;
    const QString chosen = QFileDialog::getSaveFileName(
            this, tr("Save Python Automation Script"), startPath,
            tr("Python Scripts (*.py);;All Files (*)"));
    if (!chosen.isEmpty())
        saveScriptFile(chosen);
}

void PythonConsoleDialog::clearOutput()
{
    if (d_output)
        d_output->clear();
}

void PythonConsoleDialog::runCurrentScript()
{
    if (d_process && d_process->state() != QProcess::NotRunning) {
        appendOutput(tr("[Info] A Python script is already running.\n"), false);
        return;
    }
    if (!d_server) {
        appendOutput(tr("[Error] PythonAutomationServer is not available.\n"), true);
        return;
    }
    if (!d_server->isTcpListening() && !d_server->isLocalPipeListening()) {
        if (!d_server->startTcp(0, QHostAddress::LocalHost)) {
            appendOutput(tr("[Error] Failed to start loopback RPC listener.\n"), true);
            return;
        }
    }
    refreshEndpointDisplay();
    cleanupTempScript();

    bool invalidEnvPy = false;
    const QString pyExe = PythonAutomationServer::resolvePythonExecutable(&invalidEnvPy);
    if (invalidEnvPy) {
        appendOutput(
                tr("[Process Error] Failed to start Python interpreter (%1). Set TORIGRAPH_PYTHON_EXECUTABLE if needed.\n")
                        .arg(pyExe),
                true);
        return;
    }

    QTemporaryFile tmp(QDir::temp().filePath(QStringLiteral("torigraph_console_XXXXXX.py")));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        appendOutput(tr("[Error] Failed to create temporary Python script file.\n"), true);
        return;
    }
    d_temp_script_path = tmp.fileName();
    tmp.write(d_editor ? d_editor->toPlainText().toUtf8() : QByteArray());
    tmp.close();

    appendOutput(tr("[Running] %1 (Endpoint: %2)\n").arg(pyExe, d_server->endpointString()), false);

    if (d_run_btn)
        d_run_btn->setEnabled(false);
    if (d_stop_btn)
        d_stop_btn->setEnabled(true);

    if (d_process) {
        d_process->disconnect(this);
        d_process->deleteLater();
        d_process = nullptr;
    }

    resetStreamDecoders();
    d_process = d_server->startPythonScriptProcess(d_temp_script_path, QStringList(), this);
    if (!d_current_file_path.isEmpty() && QFileInfo::exists(d_current_file_path)) {
        d_process->setWorkingDirectory(QFileInfo(d_current_file_path).absolutePath());
    }

    connect(d_process, &QProcess::readyReadStandardOutput, this,
            &PythonConsoleDialog::onProcessReadyReadStdout);
    connect(d_process, &QProcess::readyReadStandardError, this,
            &PythonConsoleDialog::onProcessReadyReadStderr);
    connect(d_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus st) { onProcessFinished(code, int(st)); });
    connect(d_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError err) { onProcessErrorOccurred(int(err)); });
}

void PythonConsoleDialog::stopRunningScript()
{
    if (!d_process || d_process->state() == QProcess::NotRunning)
        return;

    d_process->disconnect(this);
    d_process->kill();
    d_process->waitForFinished(1500);
    d_process->deleteLater();
    d_process = nullptr;

    cleanupTempScript();
    if (d_run_btn)
        d_run_btn->setEnabled(true);
    if (d_stop_btn)
        d_stop_btn->setEnabled(false);
    appendOutput(tr("[Stopped] Python script terminated by user.\n"), true);
}

void PythonConsoleDialog::onExampleSelected(int index)
{
    if (index <= 0)
        return;
    const QString code = exampleScriptCode(index);
    if (!code.isEmpty() && d_editor)
        d_editor->setPlainText(code);
}

void PythonConsoleDialog::onProcessReadyReadStdout()
{
    if (!d_process)
        return;
    const QByteArray raw = d_process->readAllStandardOutput();
    if (!raw.isEmpty()) {
        const QString decoded = d_stdout_decoder ? d_stdout_decoder->toUnicode(raw)
                                                 : QString::fromUtf8(raw);
        appendOutput(decoded, false);
    }
}

void PythonConsoleDialog::onProcessReadyReadStderr()
{
    if (!d_process)
        return;
    const QByteArray raw = d_process->readAllStandardError();
    if (!raw.isEmpty()) {
        const QString decoded = d_stderr_decoder ? d_stderr_decoder->toUnicode(raw)
                                                 : QString::fromUtf8(raw);
        appendOutput(decoded, true);
    }
}

void PythonConsoleDialog::onProcessFinished(int exitCode, int exitStatus)
{
    onProcessReadyReadStdout();
    onProcessReadyReadStderr();
    cleanupTempScript();

    if (d_run_btn)
        d_run_btn->setEnabled(true);
    if (d_stop_btn)
        d_stop_btn->setEnabled(false);

    if (exitStatus == int(QProcess::NormalExit)) {
        appendOutput(tr("[Finished] Exit code: %1\n").arg(exitCode), exitCode != 0);
    } else {
        appendOutput(tr("[Crashed] Python process terminated abnormally (code %1).\n")
                             .arg(exitCode),
                     true);
    }
}

void PythonConsoleDialog::onProcessErrorOccurred(int processError)
{
    if (processError == int(QProcess::FailedToStart)) {
        cleanupTempScript();
        if (d_run_btn)
            d_run_btn->setEnabled(true);
        if (d_stop_btn)
            d_stop_btn->setEnabled(false);
        appendOutput(
                tr("[Process Error] Failed to start Python interpreter (%1). Set TORIGRAPH_PYTHON_EXECUTABLE if needed.\n")
                        .arg(PythonAutomationServer::resolvePythonExecutable()),
                true);
    } else {
        appendOutput(tr("[Process Error] Python process error (code %1).\n").arg(processError),
                     true);
    }
}

void PythonConsoleDialog::appendOutput(const QString &text, bool /*isError*/)
{
    if (!d_output || text.isEmpty())
        return;
    d_output->moveCursor(QTextCursor::End);
    d_output->insertPlainText(text);
    d_output->moveCursor(QTextCursor::End);
    if (QScrollBar *sb = d_output->verticalScrollBar())
        sb->setValue(sb->maximum());
}

void PythonConsoleDialog::cleanupTempScript()
{
    if (!d_temp_script_path.isEmpty()) {
        QFile::remove(d_temp_script_path);
        d_temp_script_path.clear();
    }
}

QString PythonConsoleDialog::exampleScriptCode(int index) const
{
    switch (index) {
    case 1:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "strain = [0.0005 * i for i in range(25)]\n"
                "stress = [205000.0 * e if e <= 0.002 else 410.0 + 18000.0 * (e - 0.002) for e in strain]\n\n"
                "tbl = app.create_table(rows=len(strain), cols=2, name='TensileSS')\n"
                "tbl.set_data_dict(\n"
                "    {'Strain': strain, 'Stress': stress},\n"
                "    units={'Strain': '-', 'Stress': 'MPa'},\n"
                "    long_names={'Strain': 'Engineering Strain', 'Stress': 'Engineering Stress'},\n"
                ")\n\n"
                "g = app.plot_xy(tbl.name, x_col='Strain', y_cols=['Stress'], style='line_symbol')\n"
                "g.apply_style_preset('isij_85mm')\n"
                "g.apply_frame_preset('box_inward')\n"
                "elastic = app.fit_linear(graph=g.name, x_min=0.0, x_max=0.002)\n"
                "print(f\"Young's Modulus E = {elastic['slope']:.1f} MPa (R2 = {elastic['r2']:.5f})\")\n");

    case 2:
        return QStringLiteral(
                "import math\n"
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "temp = [300.0 + 2.5 * i for i in range(161)]\n"
                "flux = [\n"
                "    0.04 + 0.0001 * (t - 300.0)\n"
                "    + 1.8 * math.exp(-((t - 420.0) / 24.0) ** 2)\n"
                "    + 1.1 / (1.0 + ((t - 535.0) / 20.0) ** 2)\n"
                "    for t in temp\n"
                "]\n\n"
                "tbl = app.create_table(rows=len(temp), cols=2, name='TDS_Data')\n"
                "tbl.set_data_dict({'Temp_K': temp, 'Desorption': flux}, units={'Temp_K': 'K', 'Desorption': 'arb. u.'})\n"
                "g = app.plot_xy(tbl.name, x_col='Temp_K', y_cols=['Desorption'], style='scatter')\n"
                "res = app.analyze_peaks(\n"
                "    graph=g.name, profile='PseudoVoigt', peak_count=2, baseline='linear', plot_components=True\n"
                ")\n"
                "for idx, pk in enumerate(res.get('peaks', []), 1):\n"
                "    print(f\"Peak {idx}: T_c = {pk['center']:.2f} K, Area = {pk['area']:.2f}, FWHM = {pk['fwhm']:.2f} K\")\n");

    case 3:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "rs, rct, cdl = 25.0, 480.0, 2.2e-5\n"
                "freqs = [10.0 ** (-1.0 + 0.25 * i) for i in range(25)]\n"
                "z_re, z_im = [], []\n"
                "for f in freqs:\n"
                "    w = 2.0 * 3.141592653589793 * f\n"
                "    denom = 1.0 + (w * rct * cdl) ** 2\n"
                "    z_re.append(rs + rct / denom)\n"
                "    z_im.append(-(w * (rct ** 2) * cdl) / denom)\n\n"
                "tbl = app.create_table(rows=len(freqs), cols=3, name='EIS_Randles')\n"
                "tbl.set_data_dict({'Freq': freqs, 'Z_re': z_re, 'Z_im': z_im})\n"
                "fit = app.eis_fit(table=tbl.name, freq_col='Freq', z_re_col='Z_re', z_im_col='Z_im', model='Randles')\n"
                "print('EIS Randles Fit:', fit.get('params', fit))\n");

    case 4:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "batch_a = [410.0, 435.0, 448.0, 462.0, 475.0, 489.0, 502.0, 518.0, 535.0, 558.0]\n"
                "batch_b = [465.0, 488.0, 504.0, 519.0, 532.0, 546.0, 561.0, 578.0, 595.0, 615.0]\n\n"
                "tbl = app.create_table(rows=len(batch_a), cols=2, name='CeramicsStrength')\n"
                "tbl.set_data_dict({'Batch_A': batch_a, 'Batch_B': batch_b}, units={'Batch_A': 'MPa', 'Batch_B': 'MPa'})\n"
                "anova = app.anova_one_way(table=tbl.name, cols=['Batch_A', 'Batch_B'])\n"
                "wb = app.weibull_fit(table=tbl.name, col='Batch_A', three_param=True, plot=True)\n"
                "print(f\"ANOVA p = {anova['p_value']:.4e}, Weibull beta = {wb['beta']:.2f}, eta = {wb['eta']:.1f} MPa\")\n");

    case 5:
        return QStringLiteral(
                "import torigraph as tg\n\n"
                "app = tg.connect()\n"
                "m = app.create_matrix(rows=64, cols=64, name='BeamProfile', x_min=-2.0, x_max=2.0, y_min=-2.0, y_max=2.0)\n"
                "m.set_formula('exp(-(x*x + y*y)) + 0.55*exp(-((x-0.85)*(x-0.85) + (y+0.65)*(y+0.65))/0.18)')\n"
                "m.filter2d('gaussian', sigma=1.1, in_place=True)\n"
                "g = app.plot_matrix(m.name, style='colormap')\n"
                "g.set_colorbar(palette='viridis', show_contour_lines=True, levels=8, title='Intensity (a.u.)')\n"
                "pks = m.analyze_2d_peaks(prominence=0.15, max_peaks=4)\n"
                "print(f\"Detected {pks['peak_count']} 2D peaks on {g.name}\")\n");

    default:
        return QString();
    }
}
