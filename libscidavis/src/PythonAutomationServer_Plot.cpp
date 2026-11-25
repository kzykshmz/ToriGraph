/***************************************************************************
    File                 : PythonAutomationServer_Plot.cpp
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Headless JSON-RPC 2.0 Automation Server (TCP &
                           Named Pipe) and In-App Python Automation Console
                           [Plot Creation, Layer Management & Graph Inspection]
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

// C++17 SFINAE trait dispatchers for w1 / w2 contracts
template <typename B, typename = void>
struct HasBarCurveSetStacked : std::false_type {};
template <typename B>
struct HasBarCurveSetStacked<B, std::void_t<decltype(std::declval<B *>()->setStacked(true))>>
    : std::true_type {};

template <typename B = QwtBarCurve>
void invokeBarCurveSetStacked(B *bc, bool on)
{
    if (!bc)
        return;
    if constexpr (HasBarCurveSetStacked<B>::value)
        bc->setStacked(on);
}

template <typename G, typename = void>
struct HasGraphSetupPolarPlot : std::false_type {};
template <typename G>
struct HasGraphSetupPolarPlot<
        G, std::void_t<decltype(std::declval<G *>()->setupPolarPlot(
                   QVector<double>(), QVector<double>(), QString(), false))>>
    : std::true_type {};

template <typename G, typename = void>
struct HasGraphShadedBandCount : std::false_type {};
template <typename G>
struct HasGraphShadedBandCount<G, std::void_t<decltype(std::declval<const G *>()->shadedBandCount())>>
    : std::true_type {};

template <typename G = Graph>
int invokeGraphShadedBandCount(const G *g)
{
    if (!g)
        return 0;
    if constexpr (HasGraphShadedBandCount<G>::value) {
        return g->shadedBandCount();
    } else {
        return -1;
    }
}

QwtSymbol::Style parseQwtSymbolStyle(const QJsonValue &val,
                                     QwtSymbol::Style fallback = QwtSymbol::Ellipse)
{
    if (val.isUndefined() || val.isNull())
        return fallback;
    if (val.isDouble() && !val.isBool()) {
        const int s = parseStrictJsonInt(val, QStringLiteral("marker"));
        if (s >= int(QwtSymbol::NoSymbol) && s <= int(QwtSymbol::Hexagon))
            return static_cast<QwtSymbol::Style>(s);
        throwInvalidParams(QStringLiteral("Marker style code %1 out of range.").arg(s));
    }
    if (val.isString()) {
        const QString raw = val.toString().trimmed();
        if (raw.isEmpty())
            return fallback;
        const QString s = raw.toLower();
        if (s == QStringLiteral("none") || s == QStringLiteral("nosymbol")
            || s == QStringLiteral("no_symbol") || s == QStringLiteral("off"))
            return QwtSymbol::NoSymbol;
        if (s == QStringLiteral("o") || s == QStringLiteral("circle")
            || s == QStringLiteral("ellipse") || s == QStringLiteral("dot"))
            return QwtSymbol::Ellipse;
        if (s == QStringLiteral("s") || s == QStringLiteral("square") || s == QStringLiteral("rect")
            || s == QStringLiteral("box"))
            return QwtSymbol::Rect;
        if (s == QStringLiteral("d") || s == QStringLiteral("diamond")
            || s == QStringLiteral("thin_diamond"))
            return QwtSymbol::Diamond;
        if (s == QStringLiteral("^") || s == QStringLiteral("triangle")
            || s == QStringLiteral("utriangle") || s == QStringLiteral("up_triangle")
            || s == QStringLiteral("triangle_up"))
            return QwtSymbol::UTriangle;
        if (s == QStringLiteral("v") || s == QStringLiteral("dtriangle")
            || s == QStringLiteral("down_triangle") || s == QStringLiteral("triangle_down"))
            return QwtSymbol::DTriangle;
        if (s == QStringLiteral("<") || s == QStringLiteral("ltriangle")
            || s == QStringLiteral("left_triangle") || s == QStringLiteral("triangle_left"))
            return QwtSymbol::LTriangle;
        if (s == QStringLiteral(">") || s == QStringLiteral("rtriangle")
            || s == QStringLiteral("right_triangle") || s == QStringLiteral("triangle_right"))
            return QwtSymbol::RTriangle;
        if (s == QStringLiteral("+") || s == QStringLiteral("cross")
            || s == QStringLiteral("plus"))
            return QwtSymbol::Cross;
        if (s == QStringLiteral("x") || s == QStringLiteral("xcross"))
            return QwtSymbol::XCross;
        if (s == QStringLiteral("_") || s == QStringLiteral("hline"))
            return QwtSymbol::HLine;
        if (s == QStringLiteral("|") || s == QStringLiteral("vline"))
            return QwtSymbol::VLine;
        if (s == QStringLiteral("*") || s == QStringLiteral("star") || s == QStringLiteral("star1")
            || s == QStringLiteral("asterisk"))
            return QwtSymbol::Star1;
        if (s == QStringLiteral("star2"))
            return QwtSymbol::Star2;
        if (s == QStringLiteral("h") || s == QStringLiteral("hexagon")
            || s == QStringLiteral("hex"))
            return QwtSymbol::Hexagon;
        static const QStringList kMarkerStyles = {
            QStringLiteral("o"),      QStringLiteral("s"),       QStringLiteral("^"),
            QStringLiteral("v"),      QStringLiteral("<"),       QStringLiteral(">"),
            QStringLiteral("D"),      QStringLiteral("x"),       QStringLiteral("+"),
            QStringLiteral("*"),      QStringLiteral("h"),       QStringLiteral("circle"),
            QStringLiteral("square"), QStringLiteral("diamond"), QStringLiteral("triangle"),
            QStringLiteral("none")
        };
        throwInvalidParamsWithAvailable(
                QStringLiteral("Unknown marker/symbol style '%1'.").arg(raw), kMarkerStyles);
    }
    throwInvalidParams(QStringLiteral("Marker/symbol style must be a string or integer code."));
}

int parsePlotStyle(const QString &styleStr, int defaultStyle = Graph::LineSymbols)
{
    QString s = styleStr.trimmed().toLower();
    s.replace(QLatin1Char('-'), QLatin1Char('_'));
    s.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (s.isEmpty())
        return defaultStyle;
    if (s == QStringLiteral("line") || s == QStringLiteral("lines"))
        return Graph::Line;
    if (s == QStringLiteral("scatter") || s == QStringLiteral("symbols")
        || s == QStringLiteral("points"))
        return Graph::Scatter;
    if (s == QStringLiteral("line_symbol") || s == QStringLiteral("linesymbols")
        || s == QStringLiteral("line_symbols"))
        return Graph::LineSymbols;
    if (s == QStringLiteral("spline"))
        return Graph::Spline;
    if (s == QStringLiteral("vertical_bars") || s == QStringLiteral("vbars")
        || s == QStringLiteral("bars") || s == QStringLiteral("column")
        || s == QStringLiteral("columns"))
        return Graph::VerticalBars;
    if (s == QStringLiteral("horizontal_bars") || s == QStringLiteral("hbars")
        || s == QStringLiteral("bar"))
        return Graph::HorizontalBars;
    if (s == QStringLiteral("area"))
        return Graph::Area;
    if (s == QStringLiteral("vertical_steps") || s == QStringLiteral("vsteps")
        || s == QStringLiteral("steps") || s == QStringLiteral("step"))
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
    static const QStringList kPlotStyles = {
        QStringLiteral("line"),            QStringLiteral("scatter"),
        QStringLiteral("line_symbol"),     QStringLiteral("spline"),
        QStringLiteral("vertical_bars"),   QStringLiteral("horizontal_bars"),
        QStringLiteral("area"),            QStringLiteral("vertical_steps"),
        QStringLiteral("horizontal_steps"), QStringLiteral("drop_lines"),
        QStringLiteral("histogram"),       QStringLiteral("box"),
        QStringLiteral("pie")
    };
    throwInvalidParamsWithAvailable(
            QStringLiteral("Unknown plot style '%1'.").arg(styleStr), kPlotStyles);
}

void applyHorizontalBarCategoryHeadroom(Graph *g, int nRows, int nSeries)
{
    if (!g || !g->plotWidget() || nRows < 1)
        return;
    Plot *plt = g->plotWidget();
    const int legRows = std::max(1, (std::max(1, nSeries) + 1) / 2);
    const double topFrac = std::min(0.45, 0.13 * double(legRows) + 0.03);
    const double topPadCats =
            (nSeries >= 2) ? std::max(1.55, 0.45 + double(nRows) * topFrac / (1.0 - topFrac))
                           : 0.60;
    const double cLo = 0.45;
    const double cHi = double(nRows) + topPadCats;
    const bool isInv = plt->axisScaleEngine(QwtPlot::yLeft)
            && plt->axisScaleEngine(QwtPlot::yLeft)->testAttribute(QwtScaleEngine::Inverted);
    g->setScale(QwtPlot::yLeft, cLo, cHi, 1.0, std::min(std::max(nRows + 2, 4), 32), 0, 0, isInv);
    g->setAxisUserLocked(QwtPlot::yLeft, true);

    QwtValueList catMaj;
    for (int i = 1; i <= nRows; ++i)
        catMaj += double(i);
    QwtValueList emptyTicks;
    QwtValueList catTicks[QwtScaleDiv::NTickTypes];
    catTicks[QwtScaleDiv::MinorTick] = emptyTicks;
    catTicks[QwtScaleDiv::MediumTick] = emptyTicks;
    catTicks[QwtScaleDiv::MajorTick] = catMaj;
    auto *catEng = new QwtLinearScaleEngine();
    QwtScaleDiv catDiv(cLo, cHi, catTicks);
    if (isInv) {
        catEng->setAttribute(QwtScaleEngine::Inverted);
        catDiv.invert();
    }
    plt->setAxisMaxMinor(QwtPlot::yLeft, 0);
    plt->setMinorTicksType(QwtPlot::yLeft, ScaleDraw::None);
    plt->setAxisScaleEngine(QwtPlot::yLeft, catEng);
    plt->setAxisScaleDiv(QwtPlot::yLeft, catDiv);
    plt->updateLayout();
}

MultiLayer::PageUnit parsePageUnit(const QString &s)
{
    const QString u = s.trimmed().toLower();
    if (u.isEmpty() || u == QStringLiteral("px") || u == QStringLiteral("pixel")
        || u == QStringLiteral("pixels"))
        return MultiLayer::UnitPx;
    if (u == QStringLiteral("mm") || u == QStringLiteral("millimeter")
        || u == QStringLiteral("millimeters"))
        return MultiLayer::UnitMm;
    if (u == QStringLiteral("cm") || u == QStringLiteral("centimeter")
        || u == QStringLiteral("centimeters"))
        return MultiLayer::UnitCm;
    if (u == QStringLiteral("in") || u == QStringLiteral("inch") || u == QStringLiteral("inches"))
        return MultiLayer::UnitInch;
    throwInvalidParamsWithAvailable(
            QStringLiteral("Unknown page size unit '%1'.").arg(s),
            { QStringLiteral("px"), QStringLiteral("mm"), QStringLiteral("cm"),
              QStringLiteral("inch") });
}

// Headless format clipboard so graph.copy_format / graph.paste_format work even when window is hidden
struct HeadlessFormatClipboard
{
    struct CurveFmt
    {
        QPen pen;
        QBrush brush;
        int curveStyle = QwtPlotCurve::Lines;
        QwtSymbol symbol;
        bool symbolHollow = false;
    };

    bool valid = false;
    int srcLayerCount = 1;
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
    QColor legendTextColor = QColor(Qt::black);
    QColor legendBgColor = QColor(Qt::white);
    int legendFrameStyle = 0;
    int legendNumColumns = 1;
    bool hasLegendNormPos = false;
    double legendNormX = 0.0;
    double legendNormY = 0.0;
    bool axisEnabled[4] = { true, false, true, false };
    bool backboneVis[4] = { true, true, true, true };
    int majTicks[4] = { ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out };
    int minTicks[4] = { ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out, ScaleDraw::Out };
    bool framed = false;
    int frameWidth = 1;
    QColor frameColor = QColor(Qt::black);
    QVector<CurveFmt> curves;
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

QJsonArray rectToJsonArray(const QRect &r)
{
    QJsonArray a;
    a.append(r.x());
    a.append(r.y());
    a.append(r.width());
    a.append(r.height());
    return a;
}

QJsonArray rectFToJsonArray(const QRectF &r)
{
    QJsonArray a;
    a.append(safeJsonDouble(r.x()));
    a.append(safeJsonDouble(r.y()));
    a.append(safeJsonDouble(r.width()));
    a.append(safeJsonDouble(r.height()));
    return a;
}

void applyPlotLevelConvenienceParams(Graph *g, const QJsonObject &params)
{
    if (!g)
        return;
    Plot *plt = g->plotWidget();
    if (params.contains(QStringLiteral("title")) && !params.value(QStringLiteral("title")).isNull()) {
        const QString titleTxt = optionalJsonString(params, QStringLiteral("title"));
        g->setTitle(titleTxt);
        QFont tf(QStringLiteral("Arial"), 12, QFont::Normal, false);
        tf.setHintingPreference(QFont::PreferNoHinting);
        g->setTitleFont(tf);
    }
    const bool hasXLabel =
            params.contains(QStringLiteral("xlabel")) && !params.value(QStringLiteral("xlabel")).isNull();
    const bool hasXTitle =
            params.contains(QStringLiteral("x_title")) && !params.value(QStringLiteral("x_title")).isNull();
    if (hasXLabel || hasXTitle) {
        const QString xLbl = hasXLabel ? optionalJsonString(params, QStringLiteral("xlabel"))
                                       : optionalJsonString(params, QStringLiteral("x_title"));
        g->setAxisTitleAuto(QwtPlot::xBottom, false);
        g->setXAxisTitle(xLbl);
    }
    const bool hasYLabel =
            params.contains(QStringLiteral("ylabel")) && !params.value(QStringLiteral("ylabel")).isNull();
    const bool hasYTitle =
            params.contains(QStringLiteral("y_title")) && !params.value(QStringLiteral("y_title")).isNull();
    if (hasYLabel || hasYTitle) {
        const QString yLbl = hasYLabel ? optionalJsonString(params, QStringLiteral("ylabel"))
                                       : optionalJsonString(params, QStringLiteral("y_title"));
        g->setAxisTitleAuto(QwtPlot::yLeft, false);
        g->setYAxisTitle(yLbl);
    }
    auto applyLimArray = [&](int qwtAxis, const QString &key) {
        if (!params.contains(key) || params.value(key).isNull() || params.value(key).isUndefined())
            return;
        if (!params.value(key).isArray()) {
            throwInvalidParams(
                    QStringLiteral("Parameter '%1' must be a 2-element numeric [min, max] list.")
                            .arg(key));
        }
        const QJsonArray a = params.value(key).toArray();
        if (a.size() != 2 || !a[0].isDouble() || !a[1].isDouble())
            throwInvalidParams(
                    QStringLiteral("Parameter '%1' must be a 2-element numeric [min, max] list.")
                            .arg(key));
        const double lo = parseFiniteCoordDouble(a[0], key);
        const double hi = parseFiniteCoordDouble(a[1], key);
        validateFiniteCoordRange(lo, hi, QStringLiteral("Parameter '%1'").arg(key),
                                 g->axisStep(qwtAxis));
        const int maj = plt ? std::max(4, plt->axisMaxMajor(qwtAxis)) : 8;
        const int scType = (plt
                            && dynamic_cast<const QwtLog10ScaleEngine *>(
                                    plt->axisScaleEngine(qwtAxis)))
                ? 1
                : 0;
        if (scType == 1 && (lo <= 0.0 || hi <= 0.0))
            throwInvalidParams(
                    QStringLiteral("Parameter '%1' on log10 axis requires positive bounds (> 0).")
                            .arg(key));
        g->setScale(qwtAxis, std::min(lo, hi), std::max(lo, hi), g->axisStep(qwtAxis), maj,
                    (scType == 1) ? 8 : 1, scType, lo > hi);
        g->setAxisUserLocked(qwtAxis, true);
    };
    applyLimArray(QwtPlot::xBottom, QStringLiteral("xlim"));
    applyLimArray(QwtPlot::yLeft, QStringLiteral("ylim"));
    enforceArialGraphFonts(g);
    replotGraphOrDefer(g);
}

} // namespace

namespace rpc_internal {

void applyRequestedWindowName(ApplicationWindow *app, MyWidget *w, const QJsonObject &params,
                              const QString &explicitKey)
{
    if (!app || !w)
        return;
    QString req;
    if (!explicitKey.isEmpty()) {
        req = optionalJsonString(params, explicitKey).trimmed();
    } else {
        req = optionalJsonString(params, QStringLiteral("graph_name")).trimmed();
        if (req.isEmpty())
            req = optionalJsonString(params, QStringLiteral("name")).trimmed();
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

QJsonObject inspectGraphDetailed(MultiLayer *ml, bool includeTicks, bool diagnose)
{
    QJsonObject out;
    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("name")] = ml->name();
    out[QStringLiteral("label")] = ml->windowLabel();
    out[QStringLiteral("type")] = QStringLiteral("MultiLayer");

    const QList<Graph *> layers = ml->layersList();
    for (Graph *lg : layers) {
        if (lg && lg->plotWidget())
            lg->plotWidget()->updateLayout();
    }

    struct ParsedInsetMeta
    {
        int sourceIdx = -1;
        QRectF normRect;
        bool showConnectors = true;
        QRectF zoomDataRect;
    };
    QHash<int, ParsedInsetMeta> insetMap;
    {
        const QString savedMl = ml->saveToString(QString());
        const QStringList mlLines = savedMl.split(QLatin1Char('\n'));
        auto cleanTok = [](QString tok) -> QString {
            tok = tok.trimmed();
            if (tok.endsWith(QLatin1String("/>")))
                tok.chop(2);
            else if (tok.endsWith(QLatin1Char('>')))
                tok.chop(1);
            return tok.trimmed();
        };
        for (const QString &rawLn : mlLines) {
            const QString ln = rawLn.trimmed();
            if (ln.startsWith(QStringLiteral("<ToriInset\t"))) {
                const QStringList parts = ln.split(QLatin1Char('\t'));
                if (parts.size() >= 7) {
                    const int childIdx = cleanTok(parts.at(1)).toInt();
                    ParsedInsetMeta im;
                    im.sourceIdx = cleanTok(parts.at(2)).toInt();
                    im.normRect = QRectF(cleanTok(parts.at(3)).toDouble(),
                                         cleanTok(parts.at(4)).toDouble(),
                                         cleanTok(parts.at(5)).toDouble(),
                                         cleanTok(parts.at(6)).toDouble());
                    if (parts.size() >= 8)
                        im.showConnectors = (cleanTok(parts.at(7)).toInt() != 0);
                    if (parts.size() >= 12) {
                        im.zoomDataRect = QRectF(cleanTok(parts.at(8)).toDouble(),
                                                 cleanTok(parts.at(9)).toDouble(),
                                                 cleanTok(parts.at(10)).toDouble(),
                                                 cleanTok(parts.at(11)).toDouble());
                    }
                    if (childIdx >= 0)
                        insetMap.insert(childIdx, im);
                }
            }
        }
    }

    QJsonObject pageObj;
    pageObj[QStringLiteral("width")] = safeJsonDouble(ml->pageWidthPhysical());
    pageObj[QStringLiteral("height")] = safeJsonDouble(ml->pageHeightPhysical());
    pageObj[QStringLiteral("unit")] = pageUnitToString(ml->pageUnit());
    pageObj[QStringLiteral("dpi")] = ml->pageDpi();
    const QWidget *canvasContainer = ml->canvasWidget() ? ml->canvasWidget() : ml;
    pageObj[QStringLiteral("width_px")] = canvasContainer->width();
    pageObj[QStringLiteral("height_px")] = canvasContainer->height();
    pageObj[QStringLiteral("page_view_mode")] = ml->pageViewMode();
    pageObj[QStringLiteral("zoom")] = safeJsonDouble(ml->pageZoom());
    pageObj[QStringLiteral("keep_aspect_ratio")] = ml->keepAspectRatio();
    pageObj[QStringLiteral("aspect_ratio")] = safeJsonDouble(ml->targetAspectRatio());
    pageObj[QStringLiteral("background_color")] =
            ml->pageBackgroundColor().name(QColor::HexArgb);
    out[QStringLiteral("page")] = pageObj;

    QJsonObject gridObj;
    gridObj[QStringLiteral("rows")] = ml->getRows();
    gridObj[QStringLiteral("cols")] = ml->getCols();
    gridObj[QStringLiteral("row_spacing")] = ml->rowsSpacing();
    gridObj[QStringLiteral("col_spacing")] = ml->colsSpacing();
    QJsonObject marginsObj;
    marginsObj[QStringLiteral("left")] = ml->leftMargin();
    marginsObj[QStringLiteral("right")] = ml->rightMargin();
    marginsObj[QStringLiteral("top")] = ml->topMargin();
    marginsObj[QStringLiteral("bottom")] = ml->bottomMargin();
    gridObj[QStringLiteral("margins")] = marginsObj;
    QJsonObject plObj;
    plObj[QStringLiteral("enabled")] = ml->hasPanelLabels();
    plObj[QStringLiteral("placement")] =
            (ml->panelLabelPlacement() == MultiLayer::PanelInsideTopLeft)
            ? QStringLiteral("inside_top_left")
            : QStringLiteral("outside_top_left");
    plObj[QStringLiteral("pattern")] = ml->panelLabelPattern();
    plObj[QStringLiteral("font_pt")] = ml->panelLabelFontPt();
    gridObj[QStringLiteral("panel_labels")] = plObj;
    if (!layers.isEmpty() && layers.first()) {
        Graph *gFirst = layers.first();
        out[QStringLiteral("is_polar")] = invokeGraphIsPolarPlot(gFirst);
        out[QStringLiteral("is_ternary")] = gFirst->isTernaryPlot();
        out[QStringLiteral("is_pie")] = gFirst->isPiePlot();
        if (Grid *gr0 = gFirst->grid()) {
            gridObj[QStringLiteral("x_major")] = gr0->xEnabled();
            gridObj[QStringLiteral("y_major")] = gr0->yEnabled();
            gridObj[QStringLiteral("x_minor")] = gr0->xMinEnabled();
            gridObj[QStringLiteral("y_minor")] = gr0->yMinEnabled();
        } else {
            gridObj[QStringLiteral("x_major")] = false;
            gridObj[QStringLiteral("y_major")] = false;
            gridObj[QStringLiteral("x_minor")] = false;
            gridObj[QStringLiteral("y_minor")] = false;
        }
    } else {
        out[QStringLiteral("is_polar")] = false;
        out[QStringLiteral("is_ternary")] = false;
        out[QStringLiteral("is_pie")] = false;
    }
    out[QStringLiteral("grid")] = gridObj;

    int activeIdx = -1;
    for (int i = 0; i < layers.size(); ++i) {
        if (layers[i] == ml->activeGraph()) {
            activeIdx = i;
            break;
        }
    }
    out[QStringLiteral("active_layer")] = activeIdx;
    out[QStringLiteral("layer_count")] = layers.size();

    QJsonArray warningsArr;
    QStringList warningCodes;
    auto pushWarning = [&](const QString &code, const QString &severity, int layerIdx,
                           const QString &msg, const QJsonObject &details = QJsonObject()) {
        if (!diagnose)
            return;
        QJsonObject w;
        w[QStringLiteral("code")] = code;
        w[QStringLiteral("severity")] = severity;
        w[QStringLiteral("layer")] = layerIdx;
        w[QStringLiteral("message")] = msg;
        if (!details.isEmpty())
            w[QStringLiteral("details")] = details;
        warningsArr.append(w);
        if (!warningCodes.contains(code))
            warningCodes.append(code);
    };

    bool allFontsArial = true;
    QVector<QRect> primaryCanvasRects;
    QVector<int> primaryLayerIndices;
    QJsonArray layersArr;

    for (int li = 0; li < layers.size(); ++li) {
        Graph *g = layers[li];
        if (!g)
            continue;
        Plot *plt = g->plotWidget();
        if (plt)
            plt->updateLayout();

        const bool isInset = insetMap.contains(li);
        const bool isLinked = (!isInset && g->linkedParentLayer() != nullptr);
        const QString role = isInset
                ? QStringLiteral("inset")
                : (isLinked ? QStringLiteral("linked") : QStringLiteral("primary"));

        const QRect layerRectPx = g->geometry();
        QRect localCanvas(0, 0, g->width(), g->height());
        QPoint pltPos(0, 0);
        if (plt) {
            pltPos = plt->pos();
            if (plt->plotLayout() && plt->plotLayout()->canvasRect().isValid())
                localCanvas = plt->plotLayout()->canvasRect();
            else if (plt->canvas())
                localCanvas = plt->canvas()->geometry();
        }
        const QRect canvasInMl(layerRectPx.x() + pltPos.x() + localCanvas.x(),
                               layerRectPx.y() + pltPos.y() + localCanvas.y(),
                               localCanvas.width(), localCanvas.height());
        if (role == QStringLiteral("primary")) {
            primaryCanvasRects.append(canvasInMl);
            primaryLayerIndices.append(li);
        }

        const bool isPolarLayer = invokeGraphIsPolarPlot(g);
        const bool isTernaryLayer = g->isTernaryPlot();
        const bool isPieLayer = g->isPiePlot();
        const bool isNonCartesian = isPolarLayer || isTernaryLayer || isPieLayer;

        QJsonObject lObj;
        lObj[QStringLiteral("index")] = li;
        lObj[QStringLiteral("is_active")] = (li == activeIdx);
        lObj[QStringLiteral("role")] = role;
        lObj[QStringLiteral("is_polar")] = isPolarLayer;
        lObj[QStringLiteral("is_ternary")] = isTernaryLayer;
        lObj[QStringLiteral("is_pie")] = isPieLayer;
        {
            QJsonObject lGridObj;
            if (Grid *lgr = g->grid()) {
                lGridObj[QStringLiteral("x_major")] = lgr->xEnabled();
                lGridObj[QStringLiteral("y_major")] = lgr->yEnabled();
                lGridObj[QStringLiteral("x_minor")] = lgr->xMinEnabled();
                lGridObj[QStringLiteral("y_minor")] = lgr->yMinEnabled();
            } else {
                lGridObj[QStringLiteral("x_major")] = false;
                lGridObj[QStringLiteral("y_major")] = false;
                lGridObj[QStringLiteral("x_minor")] = false;
                lGridObj[QStringLiteral("y_minor")] = false;
            }
            lObj[QStringLiteral("grid")] = lGridObj;
        }
        lObj[QStringLiteral("layer_rect_px")] = rectToJsonArray(layerRectPx);
        lObj[QStringLiteral("canvas_rect_px")] = rectToJsonArray(canvasInMl);
        lObj[QStringLiteral("framed")] = g->framed();
        lObj[QStringLiteral("frame_width")] = g->canvasFrameWidth();

        if (isLinked && g->linkedParentLayer()) {
            QJsonObject linkObj;
            linkObj[QStringLiteral("parent_layer")] = layers.indexOf(g->linkedParentLayer());
            linkObj[QStringLiteral("x_mode")] = g->xLinkMode();
            linkObj[QStringLiteral("x_expr")] = g->xLinkExpr();
            linkObj[QStringLiteral("y_mode")] = g->yLinkMode();
            linkObj[QStringLiteral("y_expr")] = g->yLinkExpr();
            lObj[QStringLiteral("link_spec")] = linkObj;
        } else {
            lObj[QStringLiteral("link_spec")] = QJsonValue::Null;
        }

        if (isInset) {
            const ParsedInsetMeta &im = insetMap[li];
            QJsonObject insObj;
            insObj[QStringLiteral("source_layer")] = im.sourceIdx;
            insObj[QStringLiteral("norm_rect")] = rectFToJsonArray(im.normRect);
            insObj[QStringLiteral("zoom_data_rect")] = rectFToJsonArray(im.zoomDataRect);
            insObj[QStringLiteral("show_connectors")] = im.showConnectors;
            lObj[QStringLiteral("inset_spec")] = insObj;
        } else {
            lObj[QStringLiteral("inset_spec")] = QJsonValue::Null;
        }

        if (plt) {
            const QwtText titleTxt = plt->title();
            lObj[QStringLiteral("title")] = titleTxt.text();
            lObj[QStringLiteral("title_font_family")] = titleTxt.font().family();
            lObj[QStringLiteral("title_font_size")] = fontPtJson(titleTxt.font());
            if (!titleTxt.text().isEmpty()
                && titleTxt.font().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive)
                        != 0) {
                allFontsArial = false;
            }
        }

        const QString propUnitFmt = plt
                ? plt->property("_toriAxisUnitFormatStr").toString().trimmed()
                : QString();
        const QString layerUnitFmt = !propUnitFmt.isEmpty()
                ? propUnitFmt
                : ((g->axisUnitFormat() == Column::UnitFormat::ISIJSlash)
                           ? QStringLiteral("slash")
                           : QStringLiteral("paren"));

        auto inspectOneAxis = [&](int qwtAxis, const QString &axisKey) -> QJsonObject {
            QJsonObject ax = serializeAxisInfo(g, qwtAxis);
            QwtScaleDraw *qwtSd = plt ? plt->axisScaleDraw(qwtAxis) : nullptr;
            auto *sd = dynamic_cast<ScaleDraw *>(qwtSd);
            const bool enabled = plt ? plt->axisEnabled(qwtAxis) : false;
            const bool showLabels =
                    enabled && qwtSd && qwtSd->hasComponent(QwtAbstractScaleDraw::Labels);
            const bool showTicks = enabled && qwtSd
                    && (sd ? (sd->majorTicksStyle() != ScaleDraw::None
                              || sd->minorTicksStyle() != ScaleDraw::None)
                           : qwtSd->hasComponent(QwtAbstractScaleDraw::Ticks));
            ax[QStringLiteral("show_labels")] = showLabels;
            ax[QStringLiteral("show_ticks")] = showTicks;
            ax[QStringLiteral("is_weibull_probability")] = g->hasWeibullProbabilityAxis(qwtAxis);
            ax[QStringLiteral("unit_format")] = layerUnitFmt;
            const int fmtInt = plt ? plt->axisLabelFormat(qwtAxis) : 0;
            QString fmtStr = QStringLiteral("auto");
            if (fmtInt == 1)
                fmtStr = QStringLiteral("f");
            else if (fmtInt == 2)
                fmtStr = QStringLiteral("e");
            else if (fmtInt == 3)
                fmtStr = QStringLiteral("superscript");
            ax[QStringLiteral("tick_format")] = fmtStr;
            ax[QStringLiteral("precision")] = plt ? plt->axisLabelPrecision(qwtAxis) : 6;
            ax[QStringLiteral("label_rotation")] = g->labelsRotation(qwtAxis);

            if (enabled) {
                if (g->axisFont(qwtAxis).family().compare(QStringLiteral("Arial"),
                                                          Qt::CaseInsensitive)
                            != 0
                    || g->axisTitleFont(qwtAxis).family().compare(QStringLiteral("Arial"),
                                                                  Qt::CaseInsensitive)
                            != 0) {
                    allFontsArial = false;
                }
            } else if (!isNonCartesian) {
                const QString disabledTitle = g->axisTitle(qwtAxis).trimmed();
                if (!disabledTitle.isEmpty()) {
                    QJsonObject det;
                    det[QStringLiteral("axis")] = axisKey;
                    det[QStringLiteral("reason")] = QStringLiteral("disabled_axis_has_title");
                    det[QStringLiteral("title")] = disabledTitle;
                    pushWarning(
                            QStringLiteral("DISABLED_AXIS_HAS_TITLE_OR_CURVE"),
                            QStringLiteral("warning"), li,
                            QStringLiteral("Layer %1 disabled axis '%2' has a non-empty title '%3'.")
                                    .arg(li)
                                    .arg(axisKey, disabledTitle),
                            det);
                }
            }

            if (includeTicks || diagnose) {
                QJsonArray majArr;
                QJsonArray lblArr;
                struct TickBox
                {
                    double posPx;
                    double halfSpanPx;
                    QString text;
                };
                QVector<TickBox> tickBoxes;
                if (plt && plt->axisScaleDiv(qwtAxis)) {
                    const QwtScaleDiv *sdiv = plt->axisScaleDiv(qwtAxis);
                    const double lo = std::min(sdiv->lowerBound(), sdiv->upperBound());
                    const double hi = std::max(sdiv->lowerBound(), sdiv->upperBound());
                    const double spanEps = std::max(1e-12, 1e-7 * std::abs(hi - lo));
                    const QwtValueList majTicks = sdiv->ticks(QwtScaleDiv::MajorTick);
                    const QFontMetrics fm(g->axisFont(qwtAxis));
                    const int rotDeg = g->labelsRotation(qwtAxis);
                    const double rotRad = std::abs(double(rotDeg)) * 3.14159265358979323846 / 180.0;
                    const bool horizAxis =
                            (qwtAxis == QwtPlot::xBottom || qwtAxis == QwtPlot::xTop);
                    for (int ti = 0; ti < majTicks.size(); ++ti) {
                        const double tv = majTicks[ti];
                        if (!std::isfinite(tv) || tv < lo - spanEps || tv > hi + spanEps)
                            continue;
                        majArr.append(safeJsonDouble(tv));
                        QString lblText;
                        if (showLabels && qwtSd)
                            lblText = qwtSd->label(tv).text().trimmed();
                        lblArr.append(lblText);
                        if (showLabels && !lblText.isEmpty()) {
                            const double px = double(plt->transform(qwtAxis, tv));
                            const double tw = double(fm.horizontalAdvance(lblText));
                            const double th = double(fm.height());
                            const double spanAlongAxis = horizAxis
                                    ? (tw * std::cos(rotRad) + th * std::sin(rotRad))
                                    : (th * std::cos(rotRad) + tw * std::sin(rotRad));
                            tickBoxes.append({ px, 0.5 * spanAlongAxis, lblText });
                        }
                    }
                }
                if (includeTicks) {
                    ax[QStringLiteral("major_ticks")] = majArr;
                    ax[QStringLiteral("tick_labels")] = lblArr;
                }
                if (diagnose && !isNonCartesian && tickBoxes.size() >= 2) {
                    std::sort(tickBoxes.begin(), tickBoxes.end(),
                              [](const TickBox &a, const TickBox &b) { return a.posPx < b.posPx; });
                    for (int ti = 0; ti + 1 < tickBoxes.size(); ++ti) {
                        const double gap = (tickBoxes[ti + 1].posPx - tickBoxes[ti + 1].halfSpanPx)
                                - (tickBoxes[ti].posPx + tickBoxes[ti].halfSpanPx);
                        if (gap < -1.5) {
                            QJsonObject det;
                            det[QStringLiteral("axis")] = axisKey;
                            det[QStringLiteral("label_a")] = tickBoxes[ti].text;
                            det[QStringLiteral("label_b")] = tickBoxes[ti + 1].text;
                            det[QStringLiteral("overlap_px")] = safeJsonDouble(-gap);
                            pushWarning(
                                    QStringLiteral("TICK_LABEL_COLLISION"),
                                    QStringLiteral("warning"), li,
                                    QStringLiteral("Tick labels '%1' and '%2' collide on layer %3 axis '%4' (overlap %5 px).")
                                            .arg(tickBoxes[ti].text, tickBoxes[ti + 1].text)
                                            .arg(li)
                                            .arg(axisKey, QString::number(-gap, 'f', 1)),
                                    det);
                            break;
                        }
                    }
                }
            }
            return ax;
        };

        QJsonObject axesObj;
        axesObj[QStringLiteral("x")] = inspectOneAxis(QwtPlot::xBottom, QStringLiteral("x"));
        axesObj[QStringLiteral("y")] = inspectOneAxis(QwtPlot::yLeft, QStringLiteral("y"));
        axesObj[QStringLiteral("top")] = inspectOneAxis(QwtPlot::xTop, QStringLiteral("top"));
        axesObj[QStringLiteral("right")] = inspectOneAxis(QwtPlot::yRight, QStringLiteral("right"));
        lObj[QStringLiteral("axes")] = axesObj;

        Legend *leg = g->legend();
        const bool legVis = g->isLegendVisible() && leg && !leg->text().trimmed().isEmpty();
        const QRect legLocalRect = leg ? leg->rect() : QRect();
        const QRect legInMl(canvasInMl.x() + legLocalRect.x(), canvasInMl.y() + legLocalRect.y(),
                            legLocalRect.width(), legLocalRect.height());
        int legendOverlapSamples = 0;

        QJsonArray curvesArr;
        for (int ci = 0; ci < g->curves(); ++ci) {
            QwtPlotCurve *c = g->curve(ci);
            if (!c)
                continue;
            const int cType = g->curveType(ci);
            const bool isErrCurve = (cType == Graph::ErrorBars
                                     || dynamic_cast<QwtErrorPlotCurve *>(c) != nullptr);
            const bool vis = g->isCurveVisible(ci);
            const int xAx = (c->xAxis() == QwtPlot::xTop) ? QwtPlot::xTop : QwtPlot::xBottom;
            const int yAx = (c->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight : QwtPlot::yLeft;
            const QString xAxKey =
                    (xAx == QwtPlot::xTop) ? QStringLiteral("top") : QStringLiteral("x");
            const QString yAxKey =
                    (yAx == QwtPlot::yRight) ? QStringLiteral("right") : QStringLiteral("y");

            if (vis && !isErrCurve && !isNonCartesian && plt) {
                if (!plt->axisEnabled(xAx)) {
                    QJsonObject det;
                    det[QStringLiteral("axis")] = xAxKey;
                    det[QStringLiteral("curve")] = c->title().text();
                    det[QStringLiteral("reason")] = QStringLiteral("curve_on_disabled_axis");
                    pushWarning(
                            QStringLiteral("DISABLED_AXIS_HAS_TITLE_OR_CURVE"),
                            QStringLiteral("warning"), li,
                            QStringLiteral("Curve '%1' on layer %2 is mapped to disabled X axis '%3'.")
                                    .arg(c->title().text())
                                    .arg(li)
                                    .arg(xAxKey),
                            det);
                }
                if (!plt->axisEnabled(yAx)) {
                    QJsonObject det;
                    det[QStringLiteral("axis")] = yAxKey;
                    det[QStringLiteral("curve")] = c->title().text();
                    det[QStringLiteral("reason")] = QStringLiteral("curve_on_disabled_axis");
                    pushWarning(
                            QStringLiteral("DISABLED_AXIS_HAS_TITLE_OR_CURVE"),
                            QStringLiteral("warning"), li,
                            QStringLiteral("Curve '%1' on layer %2 is mapped to disabled Y axis '%3'.")
                                    .arg(c->title().text())
                                    .arg(li)
                                    .arg(yAxKey),
                            det);
                }
            }

            const bool xIsLog = plt
                    && (dynamic_cast<const QwtLog10ScaleEngine *>(plt->axisScaleEngine(xAx))
                        != nullptr);
            const bool yIsLog = plt
                    && (dynamic_cast<const QwtLog10ScaleEngine *>(plt->axisScaleEngine(yAx))
                        != nullptr);
            double axXLo = -std::numeric_limits<double>::infinity();
            double axXHi = std::numeric_limits<double>::infinity();
            double axYLo = -std::numeric_limits<double>::infinity();
            double axYHi = std::numeric_limits<double>::infinity();
            if (plt && plt->axisScaleDiv(xAx)) {
                axXLo = std::min(plt->axisScaleDiv(xAx)->lowerBound(),
                                 plt->axisScaleDiv(xAx)->upperBound());
                axXHi = std::max(plt->axisScaleDiv(xAx)->lowerBound(),
                                 plt->axisScaleDiv(xAx)->upperBound());
            }
            if (plt && plt->axisScaleDiv(yAx)) {
                axYLo = std::min(plt->axisScaleDiv(yAx)->lowerBound(),
                                 plt->axisScaleDiv(yAx)->upperBound());
                axYHi = std::max(plt->axisScaleDiv(yAx)->lowerBound(),
                                 plt->axisScaleDiv(yAx)->upperBound());
            }
            const double xEps = std::isfinite(axXHi - axXLo)
                    ? std::max(1e-12, 1e-6 * std::abs(axXHi - axXLo))
                    : 0.0;
            const double yEps = std::isfinite(axYHi - axYLo)
                    ? std::max(1e-12, 1e-6 * std::abs(axYHi - axYLo))
                    : 0.0;

            const int nPts = static_cast<int>(c->dataSize());
            auto *dc = dynamic_cast<DataCurve *>(c);
            auto *pc = dynamic_cast<PlotCurve *>(c);
            const bool checkMasks = (dc != nullptr) && dc->hasAnyMaskedPoints();
            Column *maskXCol = nullptr;
            Column *maskYCol = nullptr;
            if (checkMasks)
                dc->resolveColumns(maskXCol, maskYCol);
            const bool checkLegOverlap =
                    vis && !isErrCurve && legVis && plt && legLocalRect.isValid();
            const QwtScaleMap xMap = checkLegOverlap ? plt->canvasMap(xAx) : QwtScaleMap();
            const QwtScaleMap yMap = checkLegOverlap ? plt->canvasMap(yAx) : QwtScaleMap();
            const bool hasLineSegs = (c->style() != QwtPlotCurve::NoCurve);
            int validPts = 0;
            int clippedPts = 0;
            int nonPosX = 0;
            int nonPosY = 0;
            double xMin = std::numeric_limits<double>::infinity();
            double xMax = -std::numeric_limits<double>::infinity();
            double yMin = std::numeric_limits<double>::infinity();
            double yMax = -std::numeric_limits<double>::infinity();

            bool hasPrevPx = false;
            QPoint prevPx;
            for (int k = 0; k < nPts; ++k) {
                if (checkMasks && dc->isPointMaskedFast(k, maskXCol, maskYCol)) {
                    hasPrevPx = false;
                    continue;
                }
                const double xv = c->x(k);
                const double yv = c->y(k);
                if (!std::isfinite(xv) || !std::isfinite(yv)) {
                    hasPrevPx = false;
                    continue;
                }
                ++validPts;
                xMin = std::min(xMin, xv);
                xMax = std::max(xMax, xv);
                yMin = std::min(yMin, yv);
                yMax = std::max(yMax, yv);
                if (xv <= 0.0)
                    ++nonPosX;
                if (yv <= 0.0)
                    ++nonPosY;
                const bool outOfBounds = (xv < axXLo - xEps || xv > axXHi + xEps
                                          || yv < axYLo - yEps || yv > axYHi + yEps
                                          || (xIsLog && xv <= 0.0) || (yIsLog && yv <= 0.0));
                if (outOfBounds)
                    ++clippedPts;

                if (checkLegOverlap && !outOfBounds) {
                    const QPoint curPx(xMap.transform(xv), yMap.transform(yv));
                    if (legLocalRect.contains(curPx))
                        ++legendOverlapSamples;
                    if (hasPrevPx && hasLineSegs
                        && std::max(prevPx.x(), curPx.x()) >= legLocalRect.left()
                        && std::min(prevPx.x(), curPx.x()) <= legLocalRect.right()
                        && std::max(prevPx.y(), curPx.y()) >= legLocalRect.top()
                        && std::min(prevPx.y(), curPx.y()) <= legLocalRect.bottom()) {
                        for (int s = 1; s <= 7; ++s) {
                            const double t = double(s) / 8.0;
                            const QPoint midPx(
                                    qRound((1.0 - t) * double(prevPx.x()) + t * double(curPx.x())),
                                    qRound((1.0 - t) * double(prevPx.y()) + t * double(curPx.y())));
                            if (legLocalRect.contains(midPx)) {
                                ++legendOverlapSamples;
                                break;
                            }
                        }
                    }
                    prevPx = curPx;
                    hasPrevPx = true;
                } else {
                    hasPrevPx = false;
                }
            }

            const double clippedFrac =
                    (validPts > 0) ? (double(clippedPts) / double(validPts)) : 0.0;

            if (vis && !isErrCurve) {
                if (validPts == 0) {
                    QJsonObject det;
                    det[QStringLiteral("curve")] = c->title().text();
                    det[QStringLiteral("curve_index")] = ci;
                    pushWarning(QStringLiteral("EMPTY_CURVE"), QStringLiteral("warning"), li,
                                QStringLiteral("Visible curve '%1' on layer %2 has 0 finite points.")
                                        .arg(c->title().text())
                                        .arg(li),
                                det);
                } else if (!isNonCartesian
                           && ((role != QStringLiteral("inset") && clippedFrac > 0.02)
                               || (role == QStringLiteral("inset") && clippedFrac >= 1.0))) {
                    QJsonObject det;
                    det[QStringLiteral("curve")] = c->title().text();
                    det[QStringLiteral("clipped_fraction")] = safeJsonDouble(clippedFrac);
                    det[QStringLiteral("clipped_points")] = clippedPts;
                    det[QStringLiteral("valid_points")] = validPts;
                    pushWarning(
                            QStringLiteral("CURVE_CLIPPED_BY_AXIS"), QStringLiteral("warning"), li,
                            QStringLiteral("Curve '%1' on layer %2 has %3/%4 points (%5%) clipped by axis bounds.")
                                    .arg(c->title().text())
                                    .arg(li)
                                    .arg(clippedPts)
                                    .arg(validPts)
                                    .arg(QString::number(clippedFrac * 100.0, 'f', 1)),
                            det);
                }
                if ((xIsLog && nonPosX > 0) || (yIsLog && nonPosY > 0)) {
                    QJsonObject det;
                    det[QStringLiteral("curve")] = c->title().text();
                    det[QStringLiteral("nonpositive_x_points")] = xIsLog ? nonPosX : 0;
                    det[QStringLiteral("nonpositive_y_points")] = yIsLog ? nonPosY : 0;
                    pushWarning(
                            QStringLiteral("NONPOSITIVE_DATA_ON_LOG_AXIS"),
                            QStringLiteral("error"), li,
                            QStringLiteral("Curve '%1' on layer %2 has non-positive data on a log10 axis.")
                                    .arg(c->title().text())
                                    .arg(li),
                            det);
                }
            }

            QJsonObject cObj;
            cObj[QStringLiteral("index")] = ci;
            cObj[QStringLiteral("title")] = c->title().text();
            cObj[QStringLiteral("type")] = cType;
            cObj[QStringLiteral("visible")] = vis;
            cObj[QStringLiteral("x_axis")] = xAxKey;
            cObj[QStringLiteral("y_axis")] = yAxKey;
            cObj[QStringLiteral("points")] = nPts;
            cObj[QStringLiteral("valid_points")] = validPts;
            cObj[QStringLiteral("masked_points")] = dc ? dc->maskedPointCount() : 0;
            cObj[QStringLiteral("nonpositive_x_points")] = nonPosX;
            cObj[QStringLiteral("nonpositive_y_points")] = nonPosY;
            cObj[QStringLiteral("clipped_fraction")] = safeJsonDouble(clippedFrac);
            QJsonObject boundsObj;
            boundsObj[QStringLiteral("x_min")] = safeJsonDouble(xMin);
            boundsObj[QStringLiteral("x_max")] = safeJsonDouble(xMax);
            boundsObj[QStringLiteral("y_min")] = safeJsonDouble(yMin);
            boundsObj[QStringLiteral("y_max")] = safeJsonDouble(yMax);
            cObj[QStringLiteral("data_bounds")] = boundsObj;
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
            cObj[QStringLiteral("symbol_skip")] = pc ? std::max(1, pc->symbolSkip()) : 1;
            if (dc) {
                cObj[QStringLiteral("x_column")] = dc->xColumnName();
                cObj[QStringLiteral("y_column")] = dc->yColumnName();
                QJsonArray errArr;
                for (DataCurve *ebBase : dc->errorBarsList()) {
                    auto *ec = dynamic_cast<QwtErrorPlotCurve *>(ebBase);
                    if (!ec)
                        continue;
                    QJsonObject eObj;
                    eObj[QStringLiteral("direction")] = ec->xErrors() ? QStringLiteral("x")
                                                                      : QStringLiteral("y");
                    eObj[QStringLiteral("column")] = ec->yColumnName();
                    eObj[QStringLiteral("color")] = ec->color().name(QColor::HexRgb);
                    eObj[QStringLiteral("width")] = ec->width();
                    eObj[QStringLiteral("cap_length")] = ec->capLength();
                    eObj[QStringLiteral("plus")] = ec->plusSide();
                    eObj[QStringLiteral("minus")] = ec->minusSide();
                    eObj[QStringLiteral("through_symbol")] = ec->throughSymbol();
                    errArr.append(eObj);
                }
                cObj[QStringLiteral("error_bars")] = errArr;
            } else {
                cObj[QStringLiteral("error_bars")] = QJsonArray();
            }
            curvesArr.append(cObj);
        }
        lObj[QStringLiteral("curves")] = curvesArr;
        lObj[QStringLiteral("curve_count")] = curvesArr.size();

        QJsonObject legObj;
        legObj[QStringLiteral("visible")] = legVis;
        legObj[QStringLiteral("has_legend")] = (leg != nullptr);
        legObj[QStringLiteral("text")] = leg ? leg->text() : QString();
        legObj[QStringLiteral("bbox_px")] =
                (leg && legLocalRect.isValid()) ? rectToJsonArray(legInMl) : QJsonArray();
        legObj[QStringLiteral("font_family")] = leg ? leg->font().family() : QStringLiteral("Arial");
        legObj[QStringLiteral("font_size")] = leg ? fontPtJson(leg->font()) : QJsonValue(0);
        legObj[QStringLiteral("columns")] = leg ? leg->numColumns() : 1;
        legObj[QStringLiteral("curve_overlap_points")] = legendOverlapSamples;
        if (leg && leg->font().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive) != 0)
            allFontsArial = false;
        lObj[QStringLiteral("legend")] = legObj;

        if (legVis && legendOverlapSamples > 0) {
            QJsonObject det;
            det[QStringLiteral("curve_overlap_points")] = legendOverlapSamples;
            det[QStringLiteral("legend_bbox_px")] = rectToJsonArray(legInMl);
            pushWarning(
                    QStringLiteral("LEGEND_OVERLAPS_DATA"), QStringLiteral("warning"), li,
                    QStringLiteral("Legend on layer %1 overlaps plotted curve geometry (%2 sample hits).")
                            .arg(li)
                            .arg(legendOverlapSamples),
                    det);
        }

        const QVector<Graph::ReferenceLineSpec> refSpecs = g->referenceLines();
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
            rObj[QStringLiteral("width")] = safeJsonDouble(rs.width);
            rObj[QStringLiteral("style")] = int(rs.style);
            rObj[QStringLiteral("visible")] = rs.visible;
            refArr.append(rObj);
        }
        lObj[QStringLiteral("reference_lines")] = refArr;

        QJsonArray bandsArr;
        {
            const int directBandCount = invokeGraphShadedBandCount(g);
            const QString savedG = g->saveToString();
            const QStringList savedLines = savedG.split(QLatin1Char('\n'));
            static const QRegularExpression rxAttr(
                    QStringLiteral("([A-Za-z0-9_]+)=\"([^\"]*)\""));
            int bi = 0;
            for (const QString &rawLn : savedLines) {
                const QString ln = rawLn.trimmed();
                if (!ln.startsWith(QStringLiteral("<ToriRefLine")) || !ln.endsWith(QStringLiteral("/>")))
                    continue;
                if (!ln.contains(QStringLiteral("kind=\"band\"")))
                    continue;
                QMap<QString, QString> am;
                auto ait = rxAttr.globalMatch(ln);
                while (ait.hasNext()) {
                    const auto m = ait.next();
                    am.insert(m.captured(1), m.captured(2));
                }
                if (am.value(QStringLiteral("kind")) != QStringLiteral("band"))
                    continue;
                if (directBandCount >= 0 && bi >= directBandCount)
                    break;
                const QStringList xs = am.value(QStringLiteral("x")).split(
                        QLatin1Char(','), Qt::SkipEmptyParts);
                QJsonObject bObj;
                bObj[QStringLiteral("index")] = bi++;
                bObj[QStringLiteral("color")] = am.value(QStringLiteral("color"));
                bObj[QStringLiteral("label")] = am.value(QStringLiteral("title"));
                bObj[QStringLiteral("points")] = xs.size();
                bandsArr.append(bObj);
            }
            while (directBandCount > bandsArr.size()) {
                QJsonObject bObj;
                bObj[QStringLiteral("index")] = bandsArr.size();
                bObj[QStringLiteral("color")] = QString();
                bObj[QStringLiteral("label")] = QString();
                bObj[QStringLiteral("points")] = 0;
                bandsArr.append(bObj);
            }
        }
        lObj[QStringLiteral("shaded_bands")] = bandsArr;
        lObj[QStringLiteral("shaded_band_count")] = bandsArr.size();

        QJsonArray textArr;
        const QVector<int> txtKeys = g->textMarkerKeys();
        int tIdx = 0;
        for (int tid : txtKeys) {
            Legend *tm = g->textMarker(tid);
            if (!tm || tm == leg)
                continue;
            const QRect tmLocal = tm->rect();
            const QRect tmInMl(canvasInMl.x() + tmLocal.x(), canvasInMl.y() + tmLocal.y(),
                               tmLocal.width(), tmLocal.height());
            QJsonObject tObj;
            tObj[QStringLiteral("id")] = tid;
            tObj[QStringLiteral("index")] = tIdx;
            tObj[QStringLiteral("text")] = tm->text();
            tObj[QStringLiteral("x")] = safeJsonDouble(tm->xValue());
            tObj[QStringLiteral("y")] = safeJsonDouble(tm->yValue());
            tObj[QStringLiteral("bbox_px")] = rectToJsonArray(tmInMl);
            tObj[QStringLiteral("font_family")] = tm->font().family();
            tObj[QStringLiteral("font_size")] = fontPtJson(tm->font());
            tObj[QStringLiteral("color")] = tm->textColor().name(QColor::HexRgb);
            tObj[QStringLiteral("angle")] = tm->angle();
            if (tm->font().family().compare(QStringLiteral("Arial"), Qt::CaseInsensitive) != 0)
                allFontsArial = false;

            if (diagnose && tm->isVisible() && tmLocal.isValid() && !tm->text().trimmed().isEmpty()) {
                static const QRegularExpression kOutsidePanelBadgeRx(
                        QStringLiteral("^\\s*\\([A-Za-z0-9]{1,2}\\)\\s*$"));
                const int canvasW = (plt && plt->canvas() && plt->canvas()->width() > 4)
                        ? plt->canvas()->width()
                        : localCanvas.width();
                const int canvasH = (plt && plt->canvas() && plt->canvas()->height() > 4)
                        ? plt->canvas()->height()
                        : localCanvas.height();
                const QRect safeCanvasLocal(1, 1, std::max(1, canvasW - 2),
                                            std::max(1, canvasH - 2));
                const bool isOutsidePanelBadge =
                        kOutsidePanelBadgeRx.match(tm->text().trimmed()).hasMatch()
                        && (tmLocal.left() < 0 || tmLocal.top() < 0);
                if (!isNonCartesian && canvasW >= 20 && canvasH >= 20 && !isOutsidePanelBadge
                    && !safeCanvasLocal.contains(tmLocal)) {
                    QJsonObject det;
                    det[QStringLiteral("marker_id")] = tid;
                    det[QStringLiteral("marker_index")] = tIdx;
                    det[QStringLiteral("text")] = tm->text();
                    det[QStringLiteral("callout_bbox_px")] = rectToJsonArray(tmInMl);
                    det[QStringLiteral("canvas_bbox_px")] = rectToJsonArray(canvasInMl);
                    pushWarning(
                            QStringLiteral("CALLOUT_CLIPPED_BY_CANVAS"),
                            QStringLiteral("warning"), li,
                            QStringLiteral("Text callout '%1' on layer %2 extends outside the plot canvas bounds.")
                                    .arg(tm->text().simplified().left(40))
                                    .arg(li),
                            det);
                }
                if (legVis && legLocalRect.isValid() && tmLocal.intersects(legLocalRect)) {
                    QJsonObject det;
                    det[QStringLiteral("marker_id")] = tid;
                    det[QStringLiteral("marker_index")] = tIdx;
                    det[QStringLiteral("text")] = tm->text();
                    det[QStringLiteral("callout_bbox_px")] = rectToJsonArray(tmInMl);
                    det[QStringLiteral("legend_bbox_px")] = rectToJsonArray(legInMl);
                    pushWarning(
                            QStringLiteral("CALLOUT_OVERLAPS_LEGEND"), QStringLiteral("warning"),
                            li,
                            QStringLiteral("Text callout '%1' on layer %2 overlaps the legend box.")
                                    .arg(tm->text().simplified().left(40))
                                    .arg(li),
                            det);
                }
                if (plt) {
                    int calloutHits = 0;
                    for (int ci2 = 0; ci2 < g->curves(); ++ci2) {
                        QwtPlotCurve *c2 = g->curve(ci2);
                        if (!c2 || !g->isCurveVisible(ci2))
                            continue;
                        if (g->curveType(ci2) == Graph::ErrorBars
                            || dynamic_cast<QwtErrorPlotCurve *>(c2) != nullptr)
                            continue;
                        const int xAx2 =
                                (c2->xAxis() == QwtPlot::xTop) ? QwtPlot::xTop : QwtPlot::xBottom;
                        const int yAx2 =
                                (c2->yAxis() == QwtPlot::yRight) ? QwtPlot::yRight : QwtPlot::yLeft;
                        const QwtScaleMap xMap2 = plt->canvasMap(xAx2);
                        const QwtScaleMap yMap2 = plt->canvasMap(yAx2);
                        auto *dc2 = dynamic_cast<DataCurve *>(c2);
                        const bool checkMasks2 = (dc2 != nullptr) && dc2->hasAnyMaskedPoints();
                        Column *maskXCol2 = nullptr;
                        Column *maskYCol2 = nullptr;
                        if (checkMasks2)
                            dc2->resolveColumns(maskXCol2, maskYCol2);
                        const QList<DataCurve *> errList2 = dc2 ? dc2->errorBarsList()
                                                                : QList<DataCurve *>();
                        const bool hasErr2 = !errList2.isEmpty();
                        const bool hasLineSegs2 = (c2->style() != QwtPlotCurve::NoCurve);
                        const QRect padTm = tmLocal.adjusted(-3, -3, 3, 3);
                        const int n2 = static_cast<int>(c2->dataSize());
                        bool hasPrev2 = false;
                        QPoint prev2;
                        for (int k2 = 0; k2 < n2; ++k2) {
                            if (checkMasks2 && dc2->isPointMaskedFast(k2, maskXCol2, maskYCol2)) {
                                hasPrev2 = false;
                                continue;
                            }
                            const double xv2 = c2->x(k2);
                            const double yv2 = c2->y(k2);
                            if (!std::isfinite(xv2) || !std::isfinite(yv2)) {
                                hasPrev2 = false;
                                continue;
                            }
                            const QPoint cur2(xMap2.transform(xv2), yMap2.transform(yv2));
                            if (padTm.contains(cur2))
                                ++calloutHits;
                            if (hasErr2) {
                                for (DataCurve *ebBase2 : errList2) {
                                    auto *ec2 = dynamic_cast<QwtErrorPlotCurve *>(ebBase2);
                                    if (!ec2 || k2 >= ec2->dataSize())
                                        continue;
                                    const double ev2 = std::abs(ec2->errorValue(k2));
                                    if (!std::isfinite(ev2) || ev2 <= 0.0)
                                        continue;
                                    const int halfCap = std::max(3, ec2->capLength() / 2);
                                    if (ec2->xErrors()) {
                                        const int pxLo = xMap2.transform(
                                                ec2->minusSide() ? (xv2 - ev2) : xv2);
                                        const int pxHi = xMap2.transform(
                                                ec2->plusSide() ? (xv2 + ev2) : xv2);
                                        const QRect ebBox(std::min(pxLo, pxHi), cur2.y() - halfCap,
                                                          std::max(1, std::abs(pxHi - pxLo)),
                                                          2 * halfCap + 1);
                                        if (padTm.intersects(ebBox))
                                            ++calloutHits;
                                    } else {
                                        const int pyLo = yMap2.transform(
                                                ec2->minusSide() ? (yv2 - ev2) : yv2);
                                        const int pyHi = yMap2.transform(
                                                ec2->plusSide() ? (yv2 + ev2) : yv2);
                                        const QRect ebBox(cur2.x() - halfCap, std::min(pyLo, pyHi),
                                                          2 * halfCap + 1,
                                                          std::max(1, std::abs(pyHi - pyLo)));
                                        if (padTm.intersects(ebBox))
                                            ++calloutHits;
                                    }
                                }
                            }
                            if (hasPrev2 && hasLineSegs2
                                && std::max(prev2.x(), cur2.x()) >= tmLocal.left()
                                && std::min(prev2.x(), cur2.x()) <= tmLocal.right()
                                && std::max(prev2.y(), cur2.y()) >= tmLocal.top()
                                && std::min(prev2.y(), cur2.y()) <= tmLocal.bottom()) {
                                for (int s = 1; s <= 7; ++s) {
                                    const double t = double(s) / 8.0;
                                    const QPoint mid2(
                                            qRound((1.0 - t) * double(prev2.x())
                                                   + t * double(cur2.x())),
                                            qRound((1.0 - t) * double(prev2.y())
                                                   + t * double(cur2.y())));
                                    if (tmLocal.contains(mid2)) {
                                        ++calloutHits;
                                        break;
                                    }
                                }
                            }
                            prev2 = cur2;
                            hasPrev2 = true;
                        }
                    }
                    tObj[QStringLiteral("curve_overlap_points")] = calloutHits;
                    if (calloutHits > 0) {
                        QJsonObject det;
                        det[QStringLiteral("marker_id")] = tid;
                        det[QStringLiteral("marker_index")] = tIdx;
                        det[QStringLiteral("text")] = tm->text();
                        det[QStringLiteral("curve_overlap_points")] = calloutHits;
                        det[QStringLiteral("callout_bbox_px")] = rectToJsonArray(tmInMl);
                        pushWarning(
                                QStringLiteral("CALLOUT_OVERLAPS_DATA"),
                                QStringLiteral("warning"), li,
                                QStringLiteral("Text callout '%1' on layer %2 overlaps plotted curve geometry (%3 sample hits).")
                                        .arg(tm->text().simplified().left(40))
                                        .arg(li)
                                        .arg(calloutHits),
                                det);
                    }
                }
            }
            ++tIdx;
            textArr.append(tObj);
        }
        lObj[QStringLiteral("text_markers")] = textArr;

        QJsonArray arrowsArr;
        const QVector<int> lineKeys = g->lineMarkerKeys();
        for (int ai = 0; ai < lineKeys.size(); ++ai) {
            const int aid = lineKeys[ai];
            ArrowMarker *am = g->arrow(aid);
            if (!am)
                continue;
            const QwtDoublePoint p1 = am->startPointCoord();
            const QwtDoublePoint p2 = am->endPointCoord();
            const QPoint px1 = am->startPoint();
            const QPoint px2 = am->endPoint();
            const QRect arLocal = QRect(QPoint(std::min(px1.x(), px2.x()),
                                               std::min(px1.y(), px2.y())),
                                        QPoint(std::max(px1.x(), px2.x()),
                                               std::max(px1.y(), px2.y())))
                                          .adjusted(-2, -2, 2, 2);
            const QRect arInMl(canvasInMl.x() + arLocal.x(), canvasInMl.y() + arLocal.y(),
                               arLocal.width(), arLocal.height());
            QJsonObject aObj;
            aObj[QStringLiteral("id")] = aid;
            aObj[QStringLiteral("index")] = ai;
            aObj[QStringLiteral("x1")] = safeJsonDouble(p1.x());
            aObj[QStringLiteral("y1")] = safeJsonDouble(p1.y());
            aObj[QStringLiteral("x2")] = safeJsonDouble(p2.x());
            aObj[QStringLiteral("y2")] = safeJsonDouble(p2.y());
            aObj[QStringLiteral("bbox_px")] = rectToJsonArray(arInMl);
            aObj[QStringLiteral("start_arrow")] = am->hasStartArrow();
            aObj[QStringLiteral("end_arrow")] = am->hasEndArrow();
            aObj[QStringLiteral("color")] = am->color().name(QColor::HexRgb);
            aObj[QStringLiteral("width")] = am->width();
            aObj[QStringLiteral("style")] = int(am->style());
            arrowsArr.append(aObj);
        }
        lObj[QStringLiteral("arrows")] = arrowsArr;

        QJsonArray specsArr;
        if (plt) {
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
                        specsArr.append(sObj);
                    }
                }
            }
        }
        lObj[QStringLiteral("spectrograms")] = specsArr;

        QJsonArray gadgetsArr;
        if (PlotToolInterface *tool = g->activeTool()) {
            QJsonObject gObj;
            if (auto *roi = dynamic_cast<RoiGadgetTool *>(tool)) {
                gObj[QStringLiteral("type")] = QStringLiteral("roi");
                gObj[QStringLiteral("summary")] = roi->summaryText();
                gObj[QStringLiteral("roi_rect")] = rectFToJsonArray(roi->roiRect());
            } else if (auto *dcTool = dynamic_cast<DeltaCursorTool *>(tool)) {
                gObj[QStringLiteral("type")] = QStringLiteral("delta_cursor");
                gObj[QStringLiteral("readout")] = dcTool->readoutText();
            } else if (auto *vcTool = dynamic_cast<VerticalCursorTool *>(tool)) {
                gObj[QStringLiteral("type")] = QStringLiteral("vertical_cursor");
                gObj[QStringLiteral("readout")] = vcTool->readoutText();
            } else if (dynamic_cast<ImageProfileGadgetTool *>(tool)) {
                gObj[QStringLiteral("type")] = QStringLiteral("image_profile");
            }
            if (!gObj.isEmpty())
                gadgetsArr.append(gObj);
        }
        lObj[QStringLiteral("gadgets")] = gadgetsArr;

        layersArr.append(lObj);
    }

    if (diagnose && primaryCanvasRects.size() >= 2) {
        int minW = primaryCanvasRects[0].width(), maxW = primaryCanvasRects[0].width();
        int minH = primaryCanvasRects[0].height(), maxH = primaryCanvasRects[0].height();
        for (int i = 1; i < primaryCanvasRects.size(); ++i) {
            minW = std::min(minW, primaryCanvasRects[i].width());
            maxW = std::max(maxW, primaryCanvasRects[i].width());
            minH = std::min(minH, primaryCanvasRects[i].height());
            maxH = std::max(maxH, primaryCanvasRects[i].height());
        }
        if ((maxW - minW) > 4 || (maxH - minH) > 4) {
            QJsonObject det;
            det[QStringLiteral("min_canvas_width_px")] = minW;
            det[QStringLiteral("max_canvas_width_px")] = maxW;
            det[QStringLiteral("min_canvas_height_px")] = minH;
            det[QStringLiteral("max_canvas_height_px")] = maxH;
            pushWarning(
                    QStringLiteral("PANEL_CANVAS_SIZE_MISMATCH"), QStringLiteral("warning"),
                    primaryLayerIndices.first(),
                    QStringLiteral("Multi-panel primary canvas sizes differ across panels (W: %1..%2 px, H: %3..%4 px).")
                            .arg(minW)
                            .arg(maxW)
                            .arg(minH)
                            .arg(maxH),
                    det);
        }
    }

    out[QStringLiteral("layers")] = layersArr;
    out[QStringLiteral("all_fonts_arial")] = allFontsArial;
    out[QStringLiteral("warnings")] = warningsArr;
    out[QStringLiteral("warning_codes")] = toJsonArray(warningCodes);
    out[QStringLiteral("warning_count")] = warningsArr.size();
    out[QStringLiteral("healthy")] = warningsArr.isEmpty();
    return out;
}

void closeAndDeleteEmptyPlot(ApplicationWindow *app, MultiLayer *ml)
{
    if (!ml)
        return;
    ml->askOnCloseEvent(false);
    ml->hide();
    ml->close();
    if (app && ml->folder())
        app->closeWindow(ml);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

int countFiniteUnmaskedInTableColumn(Table *t, int colIdx, int optXIdx)
{
    if (!t || colIdx < 0 || colIdx >= t->numCols())
        return 0;
    Column *yc = t->column(colIdx);
    if (!yc || yc->columnMode() != SciDAVis::ColumnMode::Numeric)
        return 0;
    Column *xc = (optXIdx >= 0 && optXIdx < t->numCols()) ? t->column(optXIdx) : nullptr;
    int count = 0;
    for (int r = 0; r < t->numRows(); ++r) {
        if (yc->isInvalid(r) || yc->isMasked(r) || !std::isfinite(yc->valueAt(r)))
            continue;
        if (xc && xc->columnMode() == SciDAVis::ColumnMode::Numeric) {
            if (xc->isInvalid(r) || xc->isMasked(r) || !std::isfinite(xc->valueAt(r)))
                continue;
        }
        ++count;
    }
    return count;
}

void applyInlineCurveStyleParams(Graph *g, int cIdx, const QJsonObject &params,
                                 bool applyColor)
{
    if (!g || cIdx < 0 || cIdx >= g->curves())
        return;
    QwtPlotCurve *c = g->curve(cIdx);
    if (!c)
        return;

    const bool wasHollow = Graph::isSymbolWhiteHollow(c->symbol());
    QPen pen = c->pen();
    QColor col = pen.color();
    const bool hasColor = applyColor && params.contains(QStringLiteral("color"))
            && !params.value(QStringLiteral("color")).isNull()
            && !(params.value(QStringLiteral("color")).isString()
                 && params.value(QStringLiteral("color")).toString().trimmed().isEmpty());
    if (hasColor)
        col = parseColorValue(params.value(QStringLiteral("color")), col);
    if (params.contains(QStringLiteral("alpha")) && !params.value(QStringLiteral("alpha")).isNull()) {
        const double rawA =
                parseFiniteCoordDouble(params.value(QStringLiteral("alpha")),
                                       QStringLiteral("alpha"));
        const int a255 = (rawA <= 1.0 && rawA >= 0.0) ? int(std::round(rawA * 255.0))
                                                      : std::clamp(int(rawA), 0, 255);
        col.setAlpha(a255);
    }
    pen.setColor(col);

    QJsonValue lwVal;
    if (params.contains(QStringLiteral("line_width"))
        && !params.value(QStringLiteral("line_width")).isNull())
        lwVal = params.value(QStringLiteral("line_width"));
    else if (params.contains(QStringLiteral("linewidth"))
             && !params.value(QStringLiteral("linewidth")).isNull())
        lwVal = params.value(QStringLiteral("linewidth"));
    else if (params.contains(QStringLiteral("lw")) && !params.value(QStringLiteral("lw")).isNull())
        lwVal = params.value(QStringLiteral("lw"));
    if (!lwVal.isUndefined() && !lwVal.isNull())
        pen.setWidthF(std::max(0.0, parseFiniteCoordDouble(lwVal, QStringLiteral("line_width"))));

    QJsonValue lsVal;
    if (params.contains(QStringLiteral("line_style"))
        && !params.value(QStringLiteral("line_style")).isNull())
        lsVal = params.value(QStringLiteral("line_style"));
    else if (params.contains(QStringLiteral("linestyle"))
             && !params.value(QStringLiteral("linestyle")).isNull())
        lsVal = params.value(QStringLiteral("linestyle"));
    else if (params.contains(QStringLiteral("ls")) && !params.value(QStringLiteral("ls")).isNull())
        lsVal = params.value(QStringLiteral("ls"));
    if (!lsVal.isUndefined() && !lsVal.isNull())
        pen.setStyle(parsePenStyleValue(lsVal, pen.style()));

    if (hasColor || params.contains(QStringLiteral("alpha")) || !lwVal.isUndefined()
        || !lsVal.isUndefined()) {
        g->setCurvePen(cIdx, pen);
    }

    QJsonValue symTypeVal;
    if (params.contains(QStringLiteral("symbol_type"))
        && !params.value(QStringLiteral("symbol_type")).isNull())
        symTypeVal = params.value(QStringLiteral("symbol_type"));
    else if (params.contains(QStringLiteral("symbol_style"))
             && !params.value(QStringLiteral("symbol_style")).isNull())
        symTypeVal = params.value(QStringLiteral("symbol_style"));
    else if (params.contains(QStringLiteral("marker"))
             && !params.value(QStringLiteral("marker")).isNull())
        symTypeVal = params.value(QStringLiteral("marker"));

    QJsonValue symSizeVal;
    if (params.contains(QStringLiteral("symbol_size"))
        && !params.value(QStringLiteral("symbol_size")).isNull())
        symSizeVal = params.value(QStringLiteral("symbol_size"));
    else if (params.contains(QStringLiteral("markersize"))
             && !params.value(QStringLiteral("markersize")).isNull())
        symSizeVal = params.value(QStringLiteral("markersize"));
    else if (params.contains(QStringLiteral("ms")) && !params.value(QStringLiteral("ms")).isNull())
        symSizeVal = params.value(QStringLiteral("ms"));

    if (!symTypeVal.isUndefined() || !symSizeVal.isUndefined()
        || (hasColor && c->symbol().style() != QwtSymbol::NoSymbol)) {
        QwtSymbol sym = c->symbol();
        if (!symTypeVal.isUndefined() && !symTypeVal.isNull())
            sym.setStyle(parseQwtSymbolStyle(symTypeVal, sym.style()));
        if (!symSizeVal.isUndefined() && !symSizeVal.isNull()) {
            const int sz = std::clamp(
                    qRound(parseFiniteCoordDouble(symSizeVal, QStringLiteral("symbol_size"))), 1,
                    200);
            sym.setSize(sz, sz);
        } else if (!symTypeVal.isUndefined() && sym.style() != QwtSymbol::NoSymbol
                   && sym.size().width() <= 0) {
            sym.setSize(7, 7);
        }
        QPen sp = sym.pen();
        sp.setColor(col);
        sym.setPen(sp);
        QBrush sb = sym.brush();
        if (wasHollow) {
            sb = QBrush(QColor(255, 255, 255), Qt::SolidPattern);
        } else {
            sb.setColor(col);
            if (sb.style() == Qt::NoBrush && sym.style() != QwtSymbol::NoSymbol)
                sb.setStyle(Qt::SolidPattern);
        }
        sym.setBrush(sb);
        g->setCurveSymbol(cIdx, sym);
    }

    const bool hasSymbolHollow = params.contains(QStringLiteral("symbol_hollow"))
            && !params.value(QStringLiteral("symbol_hollow")).isNull();
    const bool hasSymbolFill = params.contains(QStringLiteral("symbol_fill"))
            && !params.value(QStringLiteral("symbol_fill")).isNull();
    if (hasSymbolHollow || hasSymbolFill) {
        bool wantHollow = false;
        if (hasSymbolHollow) {
            wantHollow = optionalJsonBool(params, QStringLiteral("symbol_hollow"), false);
        } else {
            const QJsonValue sf = params.value(QStringLiteral("symbol_fill"));
            if (sf.isBool()) {
                wantHollow = !sf.toBool(true);
            } else if (sf.isString()) {
                const QString sfs = sf.toString().trimmed().toLower();
                wantHollow = (sfs == QStringLiteral("hollow") || sfs == QStringLiteral("white")
                              || sfs == QStringLiteral("none") || sfs == QStringLiteral("false"));
            } else {
                throwInvalidParams(
                        QStringLiteral("Parameter 'symbol_fill' must be a boolean or string."));
            }
        }
        g->setCurveSymbolHollow(cIdx, wantHollow);
    }

    if (params.contains(QStringLiteral("symbol_skip"))
        && !params.value(QStringLiteral("symbol_skip")).isNull()) {
        const int skip = parseBoundedInt(params, QStringLiteral("symbol_skip"), 1, 0, 100000);
        if (auto *pc = dynamic_cast<PlotCurve *>(g->curve(cIdx)))
            pc->setSymbolSkip(skip == 0 ? 1 : skip);
    }

    if (params.contains(QStringLiteral("label")) && !params.value(QStringLiteral("label")).isNull()) {
        const QString customLbl = optionalJsonString(params, QStringLiteral("label")).trimmed();
        if (!customLbl.isEmpty()) {
            // 表に結び付いた曲線（DataCurve）のタイトルは Y 列の結び付き
            // （yColumnName() == title()）なので書き換えない。書き換えると保存で Y 列名の
            // 代わりにラベルが書かれ、開き直しで曲線が消え、表の編集にも追従しなくなる。
            // ラベルは凡例の文字列（下の \c{n} 行）だけで持つ。
            if (QwtPlotCurve *cur = g->curve(cIdx)) {
                if (!dynamic_cast<DataCurve *>(cur))
                    cur->setTitle(customLbl);
            }
            if (Legend *leg = g->legend()) {
                QStringList lines = leg->text().split(QLatin1Char('\n'));
                const QString prefix = QStringLiteral("\\c{%1}").arg(cIdx + 1);
                bool updated = false;
                for (int li = 0; li < lines.size(); ++li) {
                    if (lines[li].startsWith(prefix)) {
                        lines[li] = prefix + customLbl;
                        updated = true;
                        break;
                    }
                }
                if (updated)
                    leg->setText(lines.join(QLatin1Char('\n')));
            }
        }
    }
}

bool dispatchPlotAndLayerMethod(ApplicationWindow *d_app, const QString &method,
                                const QJsonObject &params, QJsonObject &outResult)
{
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

        const QString styleRaw = optionalJsonString(params, QStringLiteral("style")).trimmed().toLower();
        const bool isStackedBars = (method == QStringLiteral("plot.stacked_bars"))
                || styleRaw == QStringLiteral("stacked_bars")
                || styleRaw == QStringLiteral("stacked_bar")
                || styleRaw == QStringLiteral("stacked_column")
                || styleRaw == QStringLiteral("stack_bar")
                || styleRaw == QStringLiteral("stack_column")
                || styleRaw == QStringLiteral("stacked_horizontal_bars")
                || styleRaw == QStringLiteral("stacked_hbars");

        // Validate numeric column domain (|val| <= 1e300, non-overflowing span) before plotting
        auto checkColNumericDomain = [&](int cIdx, const QString &roleLbl) {
            if (cIdx < 0 || cIdx >= t->numCols())
                return;
            Column *col = t->column(cIdx);
            if (!col || col->columnMode() != SciDAVis::ColumnMode::Numeric)
                return;
            double vMin = std::numeric_limits<double>::infinity();
            double vMax = -std::numeric_limits<double>::infinity();
            for (int r = 0; r < t->numRows(); ++r) {
                if (col->isInvalid(r) || col->isMasked(r))
                    continue;
                const double v = col->valueAt(r);
                if (!std::isfinite(v))
                    continue;
                if (std::abs(v) > 1e300) {
                    throwInvalidParams(
                            QStringLiteral("%1 column '%2' contains extreme value (%3) exceeding |val| <= 1e300.")
                                    .arg(roleLbl, t->colLabel(cIdx))
                                    .arg(v, 0, 'e', 2));
                }
                vMin = std::min(vMin, v);
                vMax = std::max(vMax, v);
            }
            if (vMin <= vMax && !std::isfinite(vMax - vMin)) {
                throwInvalidParams(
                        QStringLiteral("%1 column '%2' span [min, max] overflows IEEE-754 double.")
                                .arg(roleLbl, t->colLabel(cIdx)));
            }
        };
        checkColNumericDomain(xIdx, QStringLiteral("X"));
        for (int yi : yIdxs)
            checkColNumericDomain(yi, QStringLiteral("Y"));

        if (isStackedBars) {
            const int nRows = t->numRows();
            if (nRows < 1)
                throwInvalidParams(QStringLiteral("Stacked bar plot requires at least 1 row."));
            const int barGap = parseBoundedInt(params, QStringLiteral("bar_gap"), 25, 0, 99);
            const int barOffset = parseBoundedInt(params, QStringLiteral("bar_offset"), 0, -500, 500);
            const bool horiz = styleRaw.contains(QStringLiteral("horiz"))
                    || styleRaw == QStringLiteral("stacked_hbars")
                    || optionalJsonBool(params, QStringLiteral("horizontal"), false);
            const bool percent100 = params.contains(QStringLiteral("percent_100"))
                    ? optionalJsonBool(params, QStringLiteral("percent_100"), false)
                    : (params.contains(QStringLiteral("percent"))
                               ? optionalJsonBool(params, QStringLiteral("percent"), false)
                               : optionalJsonBool(params, QStringLiteral("normalize_100"), false));

            Column *xColObj = (xIdx >= 0) ? t->column(xIdx) : nullptr;
            const bool isTextX = xColObj && xColObj->columnMode() != SciDAVis::ColumnMode::Numeric;
            QStringList catLabels;
            if (isTextX)
                catLabels.reserve(nRows);

            const int K = yIdxs.size();
            QList<Column *> stCols;
            auto *colX = new Column(QStringLiteral("X"), SciDAVis::ColumnMode::Numeric);
            colX->setPlotDesignation(SciDAVis::X);
            for (int r = 0; r < nRows; ++r) {
                double xv = double(r + 1);
                if (xColObj && !isTextX && !xColObj->isInvalid(r)) {
                    const double cand = xColObj->valueAt(r);
                    if (std::isfinite(cand))
                        xv = cand;
                } else if (isTextX) {
                    const QString txt = xColObj->textAt(r).trimmed();
                    catLabels.append(txt.isEmpty() ? QString::number(r + 1) : txt);
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
                cumCols[k]->setComment(lbl);
                stCols << cumCols[k];
            }

            double maxPosCum = 0.0;
            double minNegCum = 0.0;
            for (int r = 0; r < nRows; ++r) {
                QVector<double> rowVals(K, 0.0);
                double rowAbsSum = 0.0;
                for (int k = 0; k < K; ++k) {
                    if (t->column(yIdxs[k]) && !t->column(yIdxs[k])->isInvalid(r)
                        && !t->column(yIdxs[k])->isMasked(r)) {
                        const double cand = t->cell(r, yIdxs[k]);
                        if (std::isfinite(cand))
                            rowVals[k] = cand;
                    }
                    rowAbsSum += std::abs(rowVals[k]);
                }
                double posCum = 0.0;
                double negCum = 0.0;
                for (int k = 0; k < K; ++k) {
                    const double yv = percent100
                            ? ((rowAbsSum > 0.0) ? (rowVals[k] / rowAbsSum * 100.0) : 0.0)
                            : rowVals[k];
                    if (yv >= 0.0) {
                        posCum += yv;
                        cumCols[k]->setValueAt(r, posCum);
                    } else {
                        negCum += yv;
                        cumCols[k]->setValueAt(r, negCum);
                    }
                }
                maxPosCum = std::max(maxPosCum, posCum);
                minNegCum = std::min(minNegCum, negCum);
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

            const QList<QColor> &kOkabeIto = Graph::okabeItoColors();
            const int barCurveType = horiz ? Graph::HorizontalBars : Graph::VerticalBars;
            // Insert from top stack (K-1) down to base stack (0) so lower cumulative bars paint in front
            for (int k = K - 1; k >= 0; --k) {
                g->insertCurve(stTbl, stTbl->colName(0), stTbl->colName(k + 1), barCurveType, 0,
                               nRows - 1);
                const int ci = g->curves() - 1;
                if (ci >= 0) {
                    const QColor fillCol = kOkabeIto.at(k % kOkabeIto.size());
                    g->setCurvePen(ci, QPen(QColor(Qt::black), 0.8, Qt::SolidLine));
                    g->setCurveBrush(ci, QBrush(fillCol, Qt::SolidPattern));
                }
            }
            // Mark every bar segment as stacked and reset gap/offset across ALL segments AFTER insertion
            // (because each insertCurve auto-staggers previously inserted bar curves).
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (auto *bc = dynamic_cast<QwtBarCurve *>(g->curve(ci)))
                    invokeBarCurveSetStacked(bc, true);
                g->setBarsGap(ci, barGap, barOffset);
            }

            const int catAxis = horiz ? QwtPlot::yLeft : QwtPlot::xBottom;
            const int valAxis = horiz ? QwtPlot::xBottom : QwtPlot::yLeft;

            if (isTextX && !catLabels.isEmpty())
                invokeGraphSetCategoryLabels(d_app, g, catAxis, catLabels);

            if (xColObj) {
                const QString xt = xColObj->formattedAxisTitle().trimmed();
                if (!xt.isEmpty()) {
                    g->setAxisTitleAuto(catAxis, false);
                    g->setAxisTitle(catAxis, xt);
                }
            }
            if (percent100) {
                g->setAxisTitleAuto(valAxis, false);
                g->setAxisTitle(valAxis, QStringLiteral("Percentage (%)"));
            } else if (!yIdxs.isEmpty() && t->column(yIdxs.first())) {
                QString yt;
                if (K == 1) {
                    yt = t->column(yIdxs.first())->formattedAxisTitle().trimmed();
                } else {
                    const QString u0 = t->colUnits(yIdxs.first()).trimmed();
                    yt = u0.isEmpty() ? QStringLiteral("Value")
                                      : QStringLiteral("Value (%1)").arg(u0);
                }
                if (!yt.isEmpty()) {
                    g->setAxisTitleAuto(valAxis, false);
                    g->setAxisTitle(valAxis, yt);
                }
            }

            g->reconstructLegend();
            if (Legend *leg = g->legend()) {
                QStringList legLines;
                for (int k = K - 1; k >= 0; --k) {
                    const int curveOneBased = K - k; // curve index (K-1-k) + 1
                    QString sLbl = t->colLongName(yIdxs[k]).trimmed();
                    if (sLbl.isEmpty())
                        sLbl = t->colLabel(yIdxs[k]).trimmed();
                    legLines.append(QStringLiteral("\\c{%1}%2").arg(curveOneBased).arg(sLbl));
                }
                if (!horiz)
                    std::reverse(legLines.begin(), legLines.end());
                leg->setText(legLines.join(QLatin1Char('\n')));
            }

            g->setAutoScale();
            if (horiz) {
                if (Legend *leg = g->legend()) {
                    if (K >= 2)
                        leg->setNumColumns(2);
                }
                if (isTextX && nRows >= 1) {
                    applyHorizontalBarCategoryHeadroom(g, nRows, K);
                } else if (K >= 2 && g->plotWidget()) {
                    if (const QwtScaleDiv *ysd = g->plotWidget()->axisScaleDiv(QwtPlot::yLeft)) {
                        const double yLo = std::min(ysd->lowerBound(), ysd->upperBound());
                        const double yHi = std::max(ysd->lowerBound(), ysd->upperBound());
                        const int legRows = std::max(1, (K + 1) / 2);
                        const double topFrac = std::min(0.45, 0.13 * double(legRows) + 0.03);
                        const double span = std::max(1.0, yHi - yLo);
                        g->setScale(QwtPlot::yLeft, yLo, yHi + span * topFrac / (1.0 - topFrac));
                        g->setAxisUserLocked(QwtPlot::yLeft, true);
                    }
                }
            } else if (isTextX && nRows >= 1) {
                g->setScale(catAxis, 0.5, double(nRows) + 0.5, 1.0,
                            std::min(std::max(nRows + 2, 4), 32), 0, 0, false);
            }
            if (percent100) {
                const double lo = (minNegCum < 0.0) ? -100.0 : 0.0;
                const double hiHeadroom = horiz ? 108.0 : 136.0;
                g->setScale(valAxis, lo, hiHeadroom, 20.0, 7, 0, 0, false);
                g->setAxisUserLocked(valAxis, true);
                QVector<double> pctPos;
                QStringList pctLbls;
                for (int pv = (minNegCum < 0.0 ? -100 : 0); pv <= 100; pv += 20) {
                    pctPos.append(double(pv));
                    pctLbls.append(QString::number(pv));
                }
                g->setCustomTickLabels(valAxis, pctPos, pctLbls);
                if (!horiz && K >= 3) {
                    if (Legend *leg = g->legend())
                        leg->setNumColumns(2);
                }
                if (!params.contains(QStringLiteral("legend_pos"))
                    && !params.contains(QStringLiteral("legend"))) {
                    double legSc = 0.0;
                    g->placeLegendAnchor(Graph::LegendTopRight, &legSc, horiz ? 4 : 6);
                }
            } else {
                if (!horiz && K >= 3) {
                    if (Legend *leg = g->legend())
                        leg->setNumColumns(2);
                }
                if (!horiz && maxPosCum > 0.0) {
                    const double lo = (minNegCum < 0.0) ? (minNegCum * 1.12) : 0.0;
                    const double hi =
                            maxPosCum * (1.18 + 0.08 * double(std::min(K, 6)));
                    g->setScale(valAxis, lo, hi);
                    g->setAxisUserLocked(valAxis, true);
                } else if (horiz && maxPosCum > 0.0) {
                    const double lo = (minNegCum < 0.0) ? (minNegCum * 1.12) : 0.0;
                    const double hi = maxPosCum * 1.12;
                    g->setScale(valAxis, lo, hi);
                    g->setAxisUserLocked(valAxis, true);
                }
                if (!params.contains(QStringLiteral("legend_pos"))
                    && !params.contains(QStringLiteral("legend"))) {
                    g->autoPlaceLegend();
                }
            }
            applyPlotLevelConvenienceParams(g, params);
            applyRequestedWindowName(d_app, ml, params);

            outResult = serializeGraphInfo(ml);
            outResult[QStringLiteral("stacked")] = true;
            outResult[QStringLiteral("percent_100")] = percent100;
            outResult[QStringLiteral("bar_gap")] = barGap;
            outResult[QStringLiteral("ok")] = true;
            return true;
        }

        const int style = parsePlotStyle(params.value(QStringLiteral("style")).toString(),
                                         Graph::LineSymbols);
        int totalFinitePts = 0;
        for (int yi : yIdxs)
            totalFinitePts += countFiniteUnmaskedInTableColumn(t, yi, xIdx);
        if (totalFinitePts <= 0) {
            throwInvalidParams(
                    QStringLiteral("plot.xy requires at least 1 finite unmasked numeric point across the selected columns in table '%1'.")
                            .arg(t->name()));
        }
        MultiLayer *ml = d_app->multilayerPlot(t, yFullCols, style, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create 2D XY plot."));
        if (!ml->activeGraph() || ml->activeGraph()->curves() <= 0) {
            closeAndDeleteEmptyPlot(d_app, ml);
            throwInvalidParams(
                    QStringLiteral("plot.xy created 0 plottable curves from table '%1' (check for all-NaN or non-numeric columns).")
                            .arg(t->name()));
        }

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
                int nBarSeries = 0;
                for (int ci = 0; ci < g->curves(); ++ci) {
                    if (dynamic_cast<QwtBarCurve *>(g->curve(ci)))
                        ++nBarSeries;
                }
                if (hasGap && !hasOff && nBarSeries > 1) {
                    const int perBarGap = std::clamp(
                            qRound(100.0 - (100.0 - double(barGap)) / double(nBarSeries)), 10, 92);
                    int bIdx = 0;
                    for (int ci = 0; ci < g->curves(); ++ci) {
                        if (dynamic_cast<QwtBarCurve *>(g->curve(ci))) {
                            const int symOff =
                                    qRound((double(bIdx) - 0.5 * double(nBarSeries - 1)) * 106.0);
                            g->setBarsGap(ci, perBarGap, symOff);
                            ++bIdx;
                        }
                    }
                } else {
                    for (int ci = 0; ci < g->curves(); ++ci) {
                        if (auto *bc = dynamic_cast<QwtBarCurve *>(g->curve(ci))) {
                            g->setBarsGap(ci, hasGap ? barGap : bc->gap(),
                                          hasOff ? barOffset : bc->offset());
                        }
                    }
                }
            }
            const bool applySingleColor = (g->curves() == 1);
            for (int ci = 0; ci < g->curves(); ++ci) {
                if (g->curveType(ci) != Graph::ErrorBars)
                    applyInlineCurveStyleParams(g, ci, params, applySingleColor);
            }
            const QString xFull = (xIdx >= 0) ? fullColumnName(t, xIdx) : QString();
            const int valErrDir = (style == Graph::HorizontalBars) ? 0 : 1;
            if (params.value(QStringLiteral("y_err_cols")).isArray()) {
                const QJsonArray errArr = params.value(QStringLiteral("y_err_cols")).toArray();
                const int nErr = std::min(yFullCols.size(), errArr.size());
                for (int k = 0; k < nErr; ++k) {
                    if (errArr[k].isNull() || errArr[k].isUndefined())
                        continue;
                    const int yeIdx = resolveColumnIndex(t, errArr[k], true);
                    if (!xFull.isEmpty())
                        g->addErrorBars(xFull, yFullCols[k], t, fullColumnName(t, yeIdx), valErrDir);
                    else
                        g->addErrorBars(yFullCols[k], t, fullColumnName(t, yeIdx), valErrDir);
                }
            } else if (params.contains(QStringLiteral("y_err_col"))
                       && !params.value(QStringLiteral("y_err_col")).isNull()) {
                const QJsonValue yev = params.value(QStringLiteral("y_err_col"));
                if (yev.isArray()) {
                    const QJsonArray errArr = yev.toArray();
                    const int nErr = std::min(yFullCols.size(), errArr.size());
                    for (int k = 0; k < nErr; ++k) {
                        if (errArr[k].isNull() || errArr[k].isUndefined())
                            continue;
                        const int yeIdx = resolveColumnIndex(t, errArr[k], true);
                        if (!xFull.isEmpty())
                            g->addErrorBars(xFull, yFullCols[k], t, fullColumnName(t, yeIdx),
                                            valErrDir);
                        else
                            g->addErrorBars(yFullCols[k], t, fullColumnName(t, yeIdx), valErrDir);
                    }
                } else {
                    const int yeIdx = resolveColumnIndex(t, yev, true);
                    for (const QString &yFull : yFullCols) {
                        if (!xFull.isEmpty())
                            g->addErrorBars(xFull, yFull, t, fullColumnName(t, yeIdx), valErrDir);
                        else
                            g->addErrorBars(yFull, t, fullColumnName(t, yeIdx), valErrDir);
                    }
                }
            }
            if (params.contains(QStringLiteral("x_err_col"))
                && !params.value(QStringLiteral("x_err_col")).isNull()) {
                const int xeIdx = resolveColumnIndex(t, params.value(QStringLiteral("x_err_col")), true);
                if (!xFull.isEmpty())
                    g->addErrorBars(xFull, yFullCols.first(), t, fullColumnName(t, xeIdx), 0);
            }
            g->refreshAutoAxisTitlesFromColumns();
            if (xIdx >= 0 && t->column(xIdx)
                && t->column(xIdx)->columnMode() != SciDAVis::ColumnMode::Numeric) {
                QStringList cats;
                cats.reserve(t->numRows());
                for (int r = 0; r < t->numRows(); ++r)
                    cats.append(t->column(xIdx)->textAt(r).trimmed());
                if (style == Graph::HorizontalBars) {
                    invokeGraphSetCategoryLabels(d_app, g, QwtPlot::yLeft, cats);
                    applyHorizontalBarCategoryHeadroom(g, cats.size(), yIdxs.size());
                    if (yIdxs.size() > 1) {
                        if (Legend *leg = g->legend())
                            leg->setNumColumns(2);
                    }
                    g->setLabelsNumericFormat(QwtPlot::xBottom, 0, 6, QString());
                    double vMin = 0.0;
                    double vMax = 0.0;
                    for (int yi : yIdxs) {
                        Column *yc = t->column(yi);
                        if (!yc || yc->columnMode() != SciDAVis::ColumnMode::Numeric)
                            continue;
                        for (int r = 0; r < t->numRows(); ++r) {
                            if (yc->isInvalid(r) || yc->isMasked(r))
                                continue;
                            const double v = yc->valueAt(r);
                            if (std::isfinite(v)) {
                                vMin = std::min(vMin, v);
                                vMax = std::max(vMax, v);
                            }
                        }
                    }
                    for (int ci = 0; ci < g->curves(); ++ci) {
                        if (auto *eb = dynamic_cast<QwtErrorPlotCurve *>(g->curve(ci))) {
                            for (int p = 0; p < eb->dataSize(); ++p) {
                                const double bx = eb->x(p);
                                const double ev = std::abs(eb->errorValue(p));
                                if (std::isfinite(bx) && std::isfinite(ev)) {
                                    vMin = std::min(vMin, bx - ev);
                                    vMax = std::max(vMax, bx + ev);
                                }
                            }
                        }
                    }
                    const double span = (vMax > vMin) ? (vMax - vMin) : 1.0;
                    const double loX = (vMin < 0.0) ? (vMin - 0.08 * span) : 0.0;
                    const double hiX = (vMax > 0.0) ? (vMax + 0.14 * span) : (loX + 1.0);
                    g->setScale(QwtPlot::xBottom, loX, hiX);
                    g->setAxisUserLocked(QwtPlot::xBottom, true);
                    const QString catTitle = t->column(xIdx)->formattedAxisTitle().trimmed();
                    const QString valTitle = (!yIdxs.isEmpty() && t->column(yIdxs.first()))
                            ? t->column(yIdxs.first())->formattedAxisTitle().trimmed()
                            : QString();
                    g->setAxisTitleAuto(QwtPlot::yLeft, false);
                    g->setYAxisTitle(catTitle);
                    g->setAxisTitleAuto(QwtPlot::xBottom, false);
                    g->setXAxisTitle(valTitle);
                } else {
                    invokeGraphSetCategoryLabels(d_app, g, QwtPlot::xBottom, cats);
                }
            } else if (style == Graph::HorizontalBars && yIdxs.size() > 1) {
                if (Legend *leg = g->legend())
                    leg->setNumColumns(2);
                if (g->plotWidget()) {
                    if (const QwtScaleDiv *ysd = g->plotWidget()->axisScaleDiv(QwtPlot::yLeft)) {
                        const double yLo = std::min(ysd->lowerBound(), ysd->upperBound());
                        const double yHi = std::max(ysd->lowerBound(), ysd->upperBound());
                        const int legRows = std::max(1, (yIdxs.size() + 1) / 2);
                        const double topFrac = std::min(0.45, 0.13 * double(legRows) + 0.03);
                        const double span = std::max(1.0, yHi - yLo);
                        g->setScale(QwtPlot::yLeft, yLo, yHi + span * topFrac / (1.0 - topFrac));
                        g->setAxisUserLocked(QwtPlot::yLeft, true);
                    }
                }
                double vMin = 0.0;
                double vMax = 0.0;
                for (int yi : yIdxs) {
                    Column *yc = t->column(yi);
                    if (!yc || yc->columnMode() != SciDAVis::ColumnMode::Numeric)
                        continue;
                    for (int r = 0; r < t->numRows(); ++r) {
                        if (yc->isInvalid(r) || yc->isMasked(r))
                            continue;
                        const double v = yc->valueAt(r);
                        if (std::isfinite(v)) {
                            vMin = std::min(vMin, v);
                            vMax = std::max(vMax, v);
                        }
                    }
                }
                for (int ci = 0; ci < g->curves(); ++ci) {
                    if (auto *eb = dynamic_cast<QwtErrorPlotCurve *>(g->curve(ci))) {
                        for (int p = 0; p < eb->dataSize(); ++p) {
                            const double bx = eb->x(p);
                            const double ev = std::abs(eb->errorValue(p));
                            if (std::isfinite(bx) && std::isfinite(ev)) {
                                vMin = std::min(vMin, bx - ev);
                                vMax = std::max(vMax, bx + ev);
                            }
                        }
                    }
                }
                if (vMax > 0.0) {
                    const double loX = (vMin < 0.0) ? (vMin * 1.10) : 0.0;
                    g->setScale(QwtPlot::xBottom, loX, vMax * 1.14);
                    g->setAxisUserLocked(QwtPlot::xBottom, true);
                }
            }
            if ((style == Graph::HorizontalBars || style == Graph::VerticalBars)
                && yIdxs.size() > 1) {
                g->autoPlaceLegend();
            }
            applyPlotLevelConvenienceParams(g, params);
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
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Curve"));
        bool ok = false;
        if (xIdx >= 0) {
            ok = g->insertCurve(t, fullColumnName(t, xIdx), fullColumnName(t, yIdx), style, 0,
                                t->numRows() - 1);
        } else {
            ok = g->insertCurve(t, fullColumnName(t, yIdx), style);
        }
        if (!ok)
            throwRpcRuntime(QStringLiteral("Failed to add curve to graph '%1'.").arg(ml->name()));
        if (g->curves() > 0) {
            const int newIdx = g->curves() - 1;
            int dataOrder = 0;
            int inheritedLineW = d_app->defaultCurveLineWidth;
            int inheritedSymSz = d_app->defaultSymbolSize;
            bool foundRefStyle = false;
            for (int ci = 0; ci < newIdx; ++ci) {
                if (g->curveType(ci) != Graph::ErrorBars) {
                    ++dataOrder;
                    if (!foundRefStyle) {
                        if (const QwtPlotCurve *ec = g->curve(ci)) {
                            if (ec->pen().width() > 0)
                                inheritedLineW = ec->pen().width();
                            if (ec->symbol().size().width() > 0)
                                inheritedSymSz = ec->symbol().size().width();
                            foundRefStyle = true;
                        }
                    }
                }
            }
            if (!foundRefStyle && ml) {
                for (Graph *og : ml->layersList()) {
                    if (!og || og == g)
                        continue;
                    for (int ci = 0; ci < og->curves(); ++ci) {
                        if (og->curveType(ci) != Graph::ErrorBars) {
                            if (const QwtPlotCurve *ec = og->curve(ci)) {
                                if (ec->pen().width() > 0)
                                    inheritedLineW = ec->pen().width();
                                if (ec->symbol().size().width() > 0)
                                    inheritedSymSz = ec->symbol().size().width();
                                foundRefStyle = true;
                                break;
                            }
                        }
                    }
                    if (foundRefStyle)
                        break;
                }
            }
            CurveLayout cl = g->initCurveLayout(style, dataOrder);
            cl.lWidth = inheritedLineW;
            cl.sSize = inheritedSymSz;
            g->updateCurveLayout(newIdx, &cl);
            applyInlineCurveStyleParams(g, newIdx, params, true);
        }
        const bool addedRightY =
                params.value(QStringLiteral("right_axis")).toBool(false) && g->curves() > 0;
        QwtPlotCurve *addedCurve = (g->curves() > 0) ? g->curve(g->curves() - 1) : nullptr;
        if (addedRightY && addedCurve) {
            addedCurve->setYAxis(QwtPlot::yRight);
            g->enableAxis(QwtPlot::yRight, true);
            if (auto *sd = g->plotWidget() ? g->plotWidget()->axisScaleDraw(QwtPlot::yRight)
                                           : nullptr) {
                sd->enableComponent(QwtAbstractScaleDraw::Labels, true);
            }
            const QString curRightTitle = g->plotWidget()
                    ? g->plotWidget()->axisTitle(QwtPlot::yRight).text().trimmed()
                    : QString();
            if (curRightTitle.isEmpty() || curRightTitle == QStringLiteral("Y Axis Title")
                || curRightTitle == QStringLiteral("Y2 Axis Title")) {
                if (Column *yColObj = t->column(yIdx)) {
                    const QString base = !yColObj->longName().trimmed().isEmpty()
                            ? yColObj->longName().trimmed()
                            : yColObj->name().trimmed();
                    const QString unit = yColObj->units().trimmed();
                    QString rightTitle = base;
                    if (!base.isEmpty() && !unit.isEmpty()) {
                        rightTitle = (g->axisUnitFormat() == Column::UnitFormat::ISIJSlash)
                                ? QStringLiteral("%1 / %2").arg(base, unit)
                                : QStringLiteral("%1 (%2)").arg(base, unit);
                    }
                    if (!rightTitle.isEmpty()) {
                        g->setAxisTitleAuto(QwtPlot::yRight, false);
                        g->setAxisTitle(QwtPlot::yRight, rightTitle);
                    }
                }
            }
        }
        if (g->plotWidget()) {
            bool anyLocked = false;
            for (int a = 0; a < QwtPlot::axisCnt; ++a) {
                if (g->isAxisUserLocked(a))
                    anyLocked = true;
            }
            if (!anyLocked) {
                g->setAutoScale();
            } else {
                const int targetX = (addedCurve && addedCurve->xAxis() == QwtPlot::xTop)
                        ? QwtPlot::xTop
                        : QwtPlot::xBottom;
                const int targetY = addedRightY ? QwtPlot::yRight : QwtPlot::yLeft;
                if (!g->isAxisUserLocked(targetX))
                    g->setAutoScaleAxis(targetX);
                if (!g->isAxisUserLocked(targetY))
                    g->setAutoScaleAxis(targetY);
                g->updatePlot();
            }
        } else {
            g->setAutoScale();
        }
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.remove_curve")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString title = resolveCurveTitle(g, params, QStringLiteral("curve"), true);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Remove Curve"));
        if (d_app)
            d_app->invalidateModelessGraphDialogs(ml);
        g->removeCurve(title);
        g->replot();
        undoGuard.commit();
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
        const QString dirStr = optionalJsonString(params, QStringLiteral("direction")).trimmed().toLower();
        if (!dirStr.isEmpty() && dirStr != QStringLiteral("y")
            && dirStr != QStringLiteral("vertical") && dirStr != QStringLiteral("v")
            && dirStr != QStringLiteral("x") && dirStr != QStringLiteral("horizontal")
            && dirStr != QStringLiteral("h")) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Unknown error-bar direction '%1'.").arg(dirStr),
                    { QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("horizontal"),
                      QStringLiteral("vertical") });
        }
        const int type = (dirStr == QStringLiteral("x") || dirStr == QStringLiteral("horizontal")
                          || dirStr == QStringLiteral("h"))
                ? 0
                : 1;
        const QColor col = parseColorValue(params.value(QStringLiteral("color")), QColor(Qt::black));
        const int width = std::max(1, params.value(QStringLiteral("width")).toInt(1));
        const int cap = std::max(0, params.value(QStringLiteral("cap")).toInt(8));
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Error Bars"));
        bool ok = g->addErrorBars(curveTitle, t, fullColumnName(t, errIdx), type, width, cap, col);
        g->replot();
        if (ok)
            undoGuard.commit();
        outResult[QStringLiteral("ok")] = ok;
        outResult[QStringLiteral("graph")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("plot.double_y")) {
        Table *t = resolveTable(d_app, params);
        const QJsonValue xParam = params.contains(QStringLiteral("x_col"))
                ? params.value(QStringLiteral("x_col"))
                : params.value(QStringLiteral("x"));
        int xIdx = (!xParam.isUndefined() && !xParam.isNull())
                ? resolveColumnIndex(t, xParam, true)
                : t->firstXCol();
        if (xIdx < 0 && t->numCols() >= 3) {
            xIdx = 0;
            t->setColPlotDesignation(0, SciDAVis::X);
        } else if (xIdx >= 0) {
            t->setColPlotDesignation(xIdx, SciDAVis::X);
        }
        const QJsonValue y1Param = params.contains(QStringLiteral("y_left_col"))
                ? params.value(QStringLiteral("y_left_col"))
                : (params.contains(QStringLiteral("y1_col"))
                           ? params.value(QStringLiteral("y1_col"))
                           : params.value(QStringLiteral("y_left")));
        const QJsonValue y2Param = params.contains(QStringLiteral("y_right_col"))
                ? params.value(QStringLiteral("y_right_col"))
                : (params.contains(QStringLiteral("y2_col"))
                           ? params.value(QStringLiteral("y2_col"))
                           : params.value(QStringLiteral("y_right")));
        int y1Idx = (!y1Param.isUndefined() && !y1Param.isNull())
                ? resolveColumnIndex(t, y1Param, true)
                : -1;
        int y2Idx = (!y2Param.isUndefined() && !y2Param.isNull())
                ? resolveColumnIndex(t, y2Param, true)
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
        if (countFiniteUnmaskedInTableColumn(t, y1Idx, xIdx) <= 0
            || countFiniteUnmaskedInTableColumn(t, y2Idx, xIdx) <= 0) {
            throwInvalidParams(
                    QStringLiteral("plot.double_y requires at least 1 finite unmasked numeric point in both left and right Y columns."));
        }

        QStringList cols;
        cols << fullColumnName(t, y1Idx) << fullColumnName(t, y2Idx);
        MultiLayer *ml = d_app->multilayerPlot(t, cols, Graph::LineSymbols, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Double-Y plot."));
        if (!ml->activeGraph() || ml->activeGraph()->curves() < 2) {
            closeAndDeleteEmptyPlot(d_app, ml);
            throwInvalidParams(QStringLiteral("plot.double_y could not create two valid data curves."));
        }
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
            // 左軸も右軸と同じ規則（左の Y 列の題）で付ける。作成時は 2 本とも左軸に
            // 載った状態で自動の題を決めるため空のままで、開き直して初めて列名が入っていた。
            QString lt = t->column(y1Idx) ? t->column(y1Idx)->formattedAxisTitle().trimmed()
                                          : t->colLabel(y1Idx);
            if (lt.isEmpty())
                lt = t->colLabel(y1Idx);
            g->setYAxisTitle(lt);
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
            // 凡例はデータ座標（xBottom/yLeft）で位置を持つ。凡例は 2 本とも左軸に載った
            // 広い範囲で置かれたので、右の曲線を移して左軸の範囲が縮むと、同じ座標が
            // ページの外（上）に押し出される。尺度が決まった後に置き直す。
            if (g->legend()) {
                g->replot();
                g->placeLegendAnchor(Graph::LegendAutoAvoidData, nullptr, 12);
            }
            applyPlotLevelConvenienceParams(g, params);
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
        QVector<int> wfYIdxs;
        QStringList yCols;
        if (params.value(QStringLiteral("y_cols")).isArray()) {
            for (const QJsonValue &it : params.value(QStringLiteral("y_cols")).toArray()) {
                const int yi = resolveColumnIndex(t, it, true);
                t->setColPlotDesignation(yi, SciDAVis::Y);
                wfYIdxs.append(yi);
                yCols << fullColumnName(t, yi);
            }
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != xIdx && t->colPlotDesignation(c) == SciDAVis::Y) {
                    wfYIdxs.append(c);
                    yCols << fullColumnName(t, c);
                }
            }
        }
        if (yCols.isEmpty())
            throwInvalidParams(QStringLiteral("Waterfall plot requires Y columns."));
        int wfFinite = 0;
        for (int yi : wfYIdxs)
            wfFinite += countFiniteUnmaskedInTableColumn(t, yi, xIdx);
        if (wfFinite <= 0)
            throwInvalidParams(QStringLiteral("plot.waterfall requires at least 1 finite numeric point."));
        MultiLayer *ml = d_app->multilayerPlot(t, yCols, Graph::Line, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create waterfall plot."));
        if (!ml->activeGraph() || ml->activeGraph()->curves() <= 0) {
            closeAndDeleteEmptyPlot(d_app, ml);
            throwInvalidParams(QStringLiteral("plot.waterfall created 0 valid curves."));
        }
        Graph *g = ml->activeGraph();
        if (g) {
            const double yGap = params.contains(QStringLiteral("y_offset_pct"))
                    ? optionalJsonFiniteDouble(params, QStringLiteral("y_offset_pct"), 15.0, 1000.0)
                    : optionalJsonFiniteDouble(params, QStringLiteral("offset_y"), 15.0, 1000.0);
            const double xGap = params.contains(QStringLiteral("x_offset_pct"))
                    ? optionalJsonFiniteDouble(params, QStringLiteral("x_offset_pct"), 0.0, 1000.0)
                    : optionalJsonFiniteDouble(params, QStringLiteral("offset_x"), 0.0, 1000.0);
            const bool norm01 = optionalJsonBool(params, QStringLiteral("normalize"), false);
            g->setStackOffsets(1, yGap, xGap, norm01);
            if (Legend *leg = g->legend()) {
                QStringList lines = leg->text().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                if (lines.size() > 1) {
                    std::reverse(lines.begin(), lines.end());
                    leg->setText(lines.join(QLatin1Char('\n')));
                }
            }
            applyPlotLevelConvenienceParams(g, params);
        }
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.histogram") || method == QStringLiteral("plot.box")
        || method == QStringLiteral("plot.violin")) {
        Table *t = resolveTable(d_app, params);
        QVector<int> statColIdxs;
        QStringList cols;
        const QJsonValue colsArrVal = params.contains(QStringLiteral("cols"))
                ? params.value(QStringLiteral("cols"))
                : params.value(QStringLiteral("y_cols"));
        if (colsArrVal.isArray()) {
            for (const QJsonValue &it : colsArrVal.toArray()) {
                const int ci = resolveColumnIndex(t, it, true);
                statColIdxs.append(ci);
                cols << fullColumnName(t, ci);
            }
        } else if (params.contains(QStringLiteral("col"))
                   && !params.value(QStringLiteral("col")).isNull()) {
            const int ci = resolveColumnIndex(t, params.value(QStringLiteral("col")), true);
            statColIdxs.append(ci);
            cols << fullColumnName(t, ci);
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (t->colPlotDesignation(c) == SciDAVis::Y) {
                    statColIdxs.append(c);
                    cols << fullColumnName(t, c);
                }
            }
            if (cols.isEmpty() && t->numCols() > 0) {
                statColIdxs.append(0);
                cols << fullColumnName(t, 0);
            }
        }
        int totalStatFinite = 0;
        double globalMin = std::numeric_limits<double>::infinity();
        double globalMax = -std::numeric_limits<double>::infinity();
        for (int ci : statColIdxs) {
            totalStatFinite += countFiniteUnmaskedInTableColumn(t, ci);
            if (Column *cc = t->column(ci)) {
                if (cc->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    for (int r = 0; r < t->numRows(); ++r) {
                        if (cc->isInvalid(r) || cc->isMasked(r))
                            continue;
                        const double v = cc->valueAt(r);
                        if (!std::isfinite(v))
                            continue;
                        if (std::abs(v) > 1e300) {
                            throwInvalidParams(
                                    QStringLiteral("%1 column '%2' exceeds numeric domain (|val| <= 1e300).")
                                            .arg(method, t->colLabel(ci)));
                        }
                        globalMin = std::min(globalMin, v);
                        globalMax = std::max(globalMax, v);
                    }
                }
            }
        }
        if (totalStatFinite <= 0 || !std::isfinite(globalMax - globalMin)) {
            throwInvalidParams(
                    QStringLiteral("%1 requires at least 1 finite unmasked numeric value with bounded span in '%2'.")
                            .arg(method, t->name()));
        }
        const int plotType = (method == QStringLiteral("plot.histogram")) ? Graph::Histogram
                                                                          : Graph::Box;
        const bool wantStackedHist = (method == QStringLiteral("plot.histogram"))
                && optionalJsonBool(params, QStringLiteral("stacked"), false)
                && cols.size() > 1;

        const QString whiskerStr = (method == QStringLiteral("plot.box"))
                ? optionalJsonString(params, QStringLiteral("whisker_mode")).trimmed().toLower()
                : QString();
        if (method == QStringLiteral("plot.box") && !whiskerStr.isEmpty()
            && whiskerStr != QStringLiteral("1.5iqr") && whiskerStr != QStringLiteral("iqr")
            && whiskerStr != QStringLiteral("tukey") && whiskerStr != QStringLiteral("minmax")
            && whiskerStr != QStringLiteral("min_max") && whiskerStr != QStringLiteral("sd")
            && whiskerStr != QStringLiteral("se") && whiskerStr != QStringLiteral("5_95")
            && whiskerStr != QStringLiteral("p5_95") && whiskerStr != QStringLiteral("10_90")
            && whiskerStr != QStringLiteral("p10_90") && whiskerStr != QStringLiteral("1_99")
            && whiskerStr != QStringLiteral("p1_99")) {
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Unknown box plot whisker_mode '%1'.").arg(whiskerStr),
                    { QStringLiteral("1.5iqr"), QStringLiteral("sd"), QStringLiteral("se"),
                      QStringLiteral("minmax"), QStringLiteral("p5_95"), QStringLiteral("p10_90"),
                      QStringLiteral("p1_99") });
        }

        const bool attachToExisting = (method == QStringLiteral("plot.box")
                                       || method == QStringLiteral("plot.violin"))
                && !optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty();
        MultiLayer *ml = nullptr;
        Graph *targetBoxLayer = nullptr;
        int prevCurveCount = 0;
        if (attachToExisting) {
            ml = resolveMultiLayer(d_app, params);
            targetBoxLayer = resolveGraphLayer(ml, params);
            prevCurveCount = targetBoxLayer->curves();
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Box Plot"));
            targetBoxLayer->plotBoxDiagram(t, cols, 0, t->numRows() - 1);
            enforceArialGraphFonts(targetBoxLayer);
            undoGuard.commit();
        } else if (wantStackedHist) {
            ml = d_app->multilayerPlot(t, QStringList() << cols.first(), Graph::Histogram, 0,
                                       t->numRows() - 1);
            if (ml) {
                for (int k = 1; k < cols.size(); ++k) {
                    Graph *lg = ml->addLayer();
                    if (lg) {
                        lg->insertCurvesList(t, QStringList() << cols[k], Graph::Histogram, 1,
                                             Graph::Area, 0, t->numRows() - 1);
                        lg->setAutoScale();
                        enforceArialGraphFonts(lg);
                    }
                }
                ml->resize(560, std::min(1200, std::max(440, 260 * cols.size())));
                ml->setCols(1);
                ml->setRows(cols.size());
                ml->arrangeLayers(false, false);
            }
        } else {
            ml = d_app->multilayerPlot(t, cols, plotType, 0, t->numRows() - 1);
        }
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create statistical plot."));
        Graph *checkG = targetBoxLayer ? targetBoxLayer : ml->activeGraph();
        if (!checkG || checkG->curves() <= prevCurveCount) {
            if (!attachToExisting)
                closeAndDeleteEmptyPlot(d_app, ml);
            throwInvalidParams(
                    QStringLiteral("%1 created 0 curves from '%2' (check for all-NaN or non-numeric columns).")
                            .arg(method, t->name()));
        }

        if (method == QStringLiteral("plot.histogram")) {
            int totalHistPoints = 0;
            for (Graph *lg : ml->layersList()) {
                if (!lg)
                    continue;
                for (int ci = 0; ci < lg->curves(); ++ci) {
                    if (const QwtPlotCurve *c = lg->curve(ci))
                        totalHistPoints += c->dataSize();
                }
            }
            if (totalHistPoints <= 0) {
                closeAndDeleteEmptyPlot(d_app, ml);
                throwInvalidParams(
                        QStringLiteral("plot.histogram created 0 valid histogram bins from '%1' (empty or degenerate data).")
                                .arg(t->name()));
            }
        }

        if (method == QStringLiteral("plot.histogram")) {
            const bool hasBins = params.contains(QStringLiteral("bins"))
                    && !params.value(QStringLiteral("bins")).isNull();
            const bool hasBinSize = params.contains(QStringLiteral("bin_size"))
                    && !params.value(QStringLiteral("bin_size")).isNull();
            const bool hasBinMin = (params.contains(QStringLiteral("bin_min"))
                                    && !params.value(QStringLiteral("bin_min")).isNull())
                    || (params.contains(QStringLiteral("min"))
                        && !params.value(QStringLiteral("min")).isNull());
            const bool hasBinMax = (params.contains(QStringLiteral("bin_max"))
                                    && !params.value(QStringLiteral("bin_max")).isNull())
                    || (params.contains(QStringLiteral("max"))
                        && !params.value(QStringLiteral("max")).isNull());
            const int reqBins = hasBins ? parseBoundedInt(params, QStringLiteral("bins"), 10, 1, 2000)
                                        : 0;
            const double reqBinSize = hasBinSize
                    ? optionalJsonFiniteDouble(params, QStringLiteral("bin_size"), 0.0)
                    : 0.0;
            if (hasBinSize && reqBinSize <= 0.0)
                throwInvalidParams(QStringLiteral("plot.histogram 'bin_size' must be > 0."));

            QString pdfMode;
            if (params.contains(QStringLiteral("overlay_pdf"))
                && !params.value(QStringLiteral("overlay_pdf")).isNull()) {
                const QJsonValue pv = params.value(QStringLiteral("overlay_pdf"));
                if (pv.isBool()) {
                    pdfMode = pv.toBool() ? QStringLiteral("normal") : QString();
                } else if (pv.isString()) {
                    pdfMode = pv.toString().trimmed().toLower();
                } else {
                    throwInvalidParams(
                            QStringLiteral("Parameter 'overlay_pdf' must be a boolean or string ('normal'|'lognormal'|'none')."));
                }
            }

            const QList<QColor> &kHistColors = Graph::okabeItoColors();
            int histCounter = 0;
            for (Graph *lg : ml->layersList()) {
                if (!lg)
                    continue;
                const int origCurves = lg->curves();
                for (int ci = 0; ci < origCurves; ++ci) {
                    auto *hc = dynamic_cast<QwtHistogram *>(lg->curve(ci));
                    if (!hc)
                        continue;
                    if (hasBins || hasBinSize || hasBinMin || hasBinMax) {
                        const double bLo = params.contains(QStringLiteral("bin_min"))
                                ? optionalJsonFiniteDouble(params, QStringLiteral("bin_min"),
                                                           globalMin)
                                : (params.contains(QStringLiteral("min"))
                                           ? optionalJsonFiniteDouble(params, QStringLiteral("min"),
                                                                      globalMin)
                                           : globalMin);
                        double bHi = params.contains(QStringLiteral("bin_max"))
                                ? optionalJsonFiniteDouble(params, QStringLiteral("bin_max"),
                                                           globalMax)
                                : (params.contains(QStringLiteral("max"))
                                           ? optionalJsonFiniteDouble(params, QStringLiteral("max"),
                                                                      globalMax)
                                           : globalMax);
                        if (bHi <= bLo)
                            bHi = bLo + 1.0;
                        double bSz = reqBinSize;
                        if (bSz <= 0.0) {
                            const int effBins = (reqBins > 0) ? reqBins : 10;
                            bSz = (bHi - bLo) / double(effBins);
                        }
                        validateFiniteCoordRange(bLo, bHi, QStringLiteral("Histogram bin range"),
                                                 bSz, 4000);
                        hc->setBinning(false, bSz, bLo, bHi);
                        hc->loadData();
                    }
                    QColor fillC = kHistColors.at(histCounter % kHistColors.size());
                    fillC.setAlpha((origCurves > 1) ? 140 : 195);
                    lg->setCurvePen(ci, QPen(fillC.darker(130), 1.0));
                    lg->setCurveBrush(ci, QBrush(fillC, Qt::SolidPattern));

                    if (!pdfMode.isEmpty() && pdfMode != QStringLiteral("none")
                        && pdfMode != QStringLiteral("false")) {
                        const int srcColIdx =
                                (histCounter < statColIdxs.size()) ? statColIdxs[histCounter] : 0;
                        std::vector<double> sampleVals;
                        if (Column *sc = t->column(srcColIdx)) {
                            for (int r = 0; r < t->numRows(); ++r) {
                                if (!sc->isInvalid(r) && !sc->isMasked(r)
                                    && std::isfinite(sc->valueAt(r))) {
                                    sampleVals.push_back(sc->valueAt(r));
                                }
                            }
                        }
                        if (sampleVals.size() >= 2 && hc->binSize() > 0.0) {
                            const double nSamp = double(sampleVals.size());
                            const double bw = hc->binSize();
                            const double x0 = hc->begin() - 0.5 * bw;
                            const double x1 = hc->end() + 0.5 * bw;
                            const int nPdf = 200;
                            auto *pxCol = new Column(QStringLiteral("X"), SciDAVis::ColumnMode::Numeric);
                            pxCol->setPlotDesignation(SciDAVis::X);
                            if (Column *scMeta = t->column(srcColIdx)) {
                                pxCol->setLongName(scMeta->longName());
                                pxCol->setUnits(scMeta->units());
                            }
                            auto *pyCol =
                                    new Column(QStringLiteral("PDF"), SciDAVis::ColumnMode::Numeric);
                            pyCol->setPlotDesignation(SciDAVis::Y);

                            bool okPdf = false;
                            if (pdfMode == QStringLiteral("lognormal")
                                || pdfMode == QStringLiteral("log_normal")) {
                                std::vector<double> lnVals;
                                for (double v : sampleVals) {
                                    if (v > 0.0)
                                        lnVals.push_back(std::log(v));
                                }
                                if (lnVals.size() >= 2) {
                                    const double muLn =
                                            std::accumulate(lnVals.begin(), lnVals.end(), 0.0)
                                            / double(lnVals.size());
                                    double varLn = 0.0;
                                    for (double lv : lnVals)
                                        varLn += (lv - muLn) * (lv - muLn);
                                    const double sigLn =
                                            std::sqrt(varLn / double(lnVals.size() - 1));
                                    if (sigLn > 1e-15) {
                                        pyCol->setComment(QStringLiteral("Lognormal PDF"));
                                        const double invDen =
                                                1.0 / (sigLn * std::sqrt(2.0 * 3.14159265358979323846));
                                        for (int i = 0; i < nPdf; ++i) {
                                            const double xv =
                                                    x0 + (x1 - x0) * double(i) / double(nPdf - 1);
                                            double yv = 0.0;
                                            if (xv > 1e-300) {
                                                const double z = (std::log(xv) - muLn) / sigLn;
                                                yv = nSamp * bw * (invDen / xv)
                                                        * std::exp(-0.5 * z * z);
                                            }
                                            pxCol->setValueAt(i, xv);
                                            pyCol->setValueAt(i, yv);
                                        }
                                        okPdf = true;
                                    }
                                }
                            } else if (pdfMode == QStringLiteral("normal")
                                       || pdfMode == QStringLiteral("gaussian")
                                       || pdfMode == QStringLiteral("gauss")) {
                                const double mu =
                                        std::accumulate(sampleVals.begin(), sampleVals.end(), 0.0)
                                        / nSamp;
                                double varS = 0.0;
                                for (double sv : sampleVals)
                                    varS += (sv - mu) * (sv - mu);
                                const double sig = std::sqrt(varS / (nSamp - 1.0));
                                if (sig > 1e-15) {
                                    pyCol->setComment(QStringLiteral("Normal PDF"));
                                    const double invDen =
                                            1.0 / (sig * std::sqrt(2.0 * 3.14159265358979323846));
                                    for (int i = 0; i < nPdf; ++i) {
                                        const double xv =
                                                x0 + (x1 - x0) * double(i) / double(nPdf - 1);
                                        const double z = (xv - mu) / sig;
                                        pxCol->setValueAt(i, xv);
                                        pyCol->setValueAt(i,
                                                          nSamp * bw * invDen
                                                                  * std::exp(-0.5 * z * z));
                                    }
                                    okPdf = true;
                                }
                            } else {
                                delete pxCol;
                                delete pyCol;
                                throwInvalidParams(
                                        QStringLiteral("Unknown overlay_pdf '%1' (expected 'normal'|'lognormal'|'none').")
                                                .arg(pdfMode));
                            }

                            if (okPdf) {
                                const QString pdfTblName =
                                        d_app->generateUniqueName(QStringLiteral("HistPdf"));
                                Table *pdfTbl = d_app->newHiddenTable(
                                        pdfTblName, QStringLiteral("HistogramPDF"),
                                        QList<Column *>() << pxCol << pyCol);
                                if (pdfTbl) {
                                    lg->insertCurve(pdfTbl, pdfTbl->colName(0), pdfTbl->colName(1),
                                                    Graph::Line, 0, nPdf - 1);
                                    const int pdfIdx = lg->curves() - 1;
                                    if (pdfIdx >= 0) {
                                        lg->setCurvePen(
                                                pdfIdx,
                                                QPen(QColor(213, 94, 0), 2.0, Qt::SolidLine));
                                    }
                                } else {
                                    delete pxCol;
                                    delete pyCol;
                                }
                            } else {
                                delete pxCol;
                                delete pyCol;
                            }
                        }
                    }
                    ++histCounter;
                }
                lg->setAutoScale();
                if (wantStackedHist) {
                    const int panelIdx = std::clamp(histCounter - 1, 0,
                                                    std::max(0, statColIdxs.size() - 1));
                    const int srcColIdx = statColIdxs[panelIdx];
                    lg->setAxisTitleAuto(QwtPlot::xBottom, true);
                    lg->refreshAutoAxisTitlesFromColumns();
                    lg->setAxisTitleAuto(QwtPlot::yLeft, false);
                    lg->setYAxisTitle(QStringLiteral("Count"));
                    const bool hasPdfCurve = (lg->curves() >= 2);
                    if (Plot *lplt = lg->plotWidget()) {
                        if (const QwtScaleDiv *ysd = lplt->axisScaleDiv(QwtPlot::yLeft)) {
                            const double yHi = std::max(ysd->lowerBound(), ysd->upperBound());
                            if (std::isfinite(yHi) && yHi > 0.0) {
                                lg->setScale(QwtPlot::yLeft, 0.0,
                                             yHi * (hasPdfCurve ? 1.24 : 1.10));
                            }
                        }
                    }
                    if (hasPdfCurve) {
                        Legend *leg = lg->legend();
                        if (!leg)
                            leg = lg->newLegend();
                        if (leg) {
                            const QString lnTrim = t->colLongName(srcColIdx).trimmed();
                            const QString colTrim = t->colLabel(srcColIdx).trimmed();
                            QString shortLbl = (!lnTrim.isEmpty() && lnTrim.size() <= 18)
                                    ? lnTrim
                                    : colTrim;
                            if (shortLbl.size() > 18) {
                                const QStringList toks = shortLbl.split(
                                        QRegularExpression(QStringLiteral("\\s+")),
                                        Qt::SkipEmptyParts);
                                if (toks.size() >= 2) {
                                    const QString tail2 = toks[toks.size() - 2] + QLatin1Char(' ')
                                            + toks[toks.size() - 1];
                                    const QString tail1 = toks.last();
                                    if (tail2.size() <= 18)
                                        shortLbl = tail2;
                                    else if (tail1.size() <= 14)
                                        shortLbl = QStringLiteral("Hist ") + tail1;
                                    else
                                        shortLbl = QStringLiteral("Histogram");
                                } else {
                                    shortLbl = shortLbl.left(15) + QStringLiteral("...");
                                }
                            }
                            const QString pdfLbl = (pdfMode == QStringLiteral("lognormal")
                                                    || pdfMode == QStringLiteral("log_normal"))
                                    ? QStringLiteral("Lognormal PDF")
                                    : QStringLiteral("Normal PDF");
                            leg->setText(
                                    QStringLiteral("\\c{1}%1\n\\c{2}%2").arg(shortLbl, pdfLbl));
                        }
                        lg->autoPlaceLegend();
                    } else if (!params.contains(QStringLiteral("legend"))) {
                        lg->setLegendVisible(false);
                    } else {
                        lg->autoPlaceLegend();
                    }
                } else {
                    lg->autoPlaceLegend();
                }
                applyPlotLevelConvenienceParams(lg, params);
            }
        } else if (Graph *g = targetBoxLayer ? targetBoxLayer : ml->activeGraph()) {
            const bool notched = optionalJsonBool(params, QStringLiteral("notched"), false);
            for (int i = prevCurveCount; i < g->curves(); ++i) {
                if (auto *bc = dynamic_cast<BoxCurve *>(g->curve(i))) {
                    if (method == QStringLiteral("plot.violin")) {
                        bc->setViolinMode(true);
                        bc->setShowDataPoints(
                                optionalJsonBool(params, QStringLiteral("show_points"), true));
                    } else {
                        if (notched)
                            bc->setBoxStyle(BoxCurve::Notch);
                        if (params.contains(QStringLiteral("show_points")))
                            bc->setShowDataPoints(
                                    optionalJsonBool(params, QStringLiteral("show_points"), false));
                        if (whiskerStr.isEmpty() || whiskerStr == QStringLiteral("1.5iqr")
                            || whiskerStr == QStringLiteral("iqr")
                            || whiskerStr == QStringLiteral("tukey"))
                            bc->setWhiskersRange(BoxCurve::Tukey15IQR, 1.5);
                        else if (whiskerStr == QStringLiteral("minmax")
                                 || whiskerStr == QStringLiteral("min_max"))
                            bc->setWhiskersRange(BoxCurve::MinMax, 0.0);
                        else if (whiskerStr == QStringLiteral("sd"))
                            bc->setWhiskersRange(BoxCurve::SD, 1.0);
                        else if (whiskerStr == QStringLiteral("se"))
                            bc->setWhiskersRange(BoxCurve::SE, 1.0);
                        else if (whiskerStr == QStringLiteral("5_95")
                                 || whiskerStr == QStringLiteral("p5_95"))
                            bc->setWhiskersRange(BoxCurve::r5_95, 0.0);
                        else if (whiskerStr == QStringLiteral("10_90")
                                 || whiskerStr == QStringLiteral("p10_90"))
                            bc->setWhiskersRange(BoxCurve::r10_90, 0.0);
                        else if (whiskerStr == QStringLiteral("1_99")
                                 || whiskerStr == QStringLiteral("p1_99"))
                            bc->setWhiskersRange(BoxCurve::r1_99, 0.0);
                        bc->loadData();
                    }
                }
            }
            applyPlotLevelConvenienceParams(g, params);
        }
        if (!attachToExisting)
            applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.pareto")) {
        Table *srcTable = resolveTable(d_app, params);
        int catCol = -1;
        if (params.contains(QStringLiteral("category_col"))
            && !params.value(QStringLiteral("category_col")).isNull()) {
            catCol = resolveColumnIndex(srcTable, params.value(QStringLiteral("category_col")),
                                        true);
        } else if (params.contains(QStringLiteral("label_col"))
                   && !params.value(QStringLiteral("label_col")).isNull()) {
            catCol = resolveColumnIndex(srcTable, params.value(QStringLiteral("label_col")), true);
        } else if (params.contains(QStringLiteral("x_col"))
                   && !params.value(QStringLiteral("x_col")).isNull()) {
            catCol = resolveColumnIndex(srcTable, params.value(QStringLiteral("x_col")), true);
        } else {
            for (int c = 0; c < srcTable->numCols(); ++c) {
                if (srcTable->column(c)
                    && srcTable->column(c)->columnMode() == SciDAVis::ColumnMode::Text) {
                    catCol = c;
                    break;
                }
            }
        }

        int yCol = -1;
        if (params.contains(QStringLiteral("value_col"))
            && !params.value(QStringLiteral("value_col")).isNull()) {
            yCol = resolveColumnIndex(srcTable, params.value(QStringLiteral("value_col")), true);
        } else if (params.contains(QStringLiteral("col"))
                   && !params.value(QStringLiteral("col")).isNull()) {
            yCol = resolveColumnIndex(srcTable, params.value(QStringLiteral("col")), true);
        } else {
            for (int c = 0; c < srcTable->numCols(); ++c) {
                if (c != catCol && srcTable->column(c)
                    && srcTable->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    yCol = c;
                    break;
                }
            }
            if (yCol < 0)
                yCol = (srcTable->numCols() >= 2) ? 1 : 0;
        }

        struct ParetoItem
        {
            double val;
            QString label;
        };
        QVector<ParetoItem> items;
        double totalSum = 0.0;
        for (int r = 0; r < srcTable->numRows(); ++r) {
            if (srcTable->column(yCol) && !srcTable->column(yCol)->isInvalid(r)
                && !srcTable->column(yCol)->isMasked(r)) {
                const double v = srcTable->cell(r, yCol);
                if (std::isfinite(v) && v >= 0.0 && v <= 1e300) {
                    QString lbl;
                    if (catCol >= 0 && srcTable->column(catCol))
                        lbl = srcTable->text(r, catCol).trimmed();
                    if (lbl.isEmpty())
                        lbl = QString::number(r + 1);
                    items.append({ v, lbl });
                    totalSum += v;
                }
            }
        }
        if (items.size() < 2 || totalSum <= 0.0 || !std::isfinite(totalSum))
            throwInvalidParams(QStringLiteral(
                    "Pareto plot requires >= 2 non-negative numeric values with positive finite sum."));
        std::stable_sort(items.begin(), items.end(),
                         [](const ParetoItem &a, const ParetoItem &b) { return a.val > b.val; });

        Table *pt = createUniqueTableHeadless(d_app, items.size(), 4,
                                              srcTable->name() + QStringLiteral("Pareto"));
        pt->setColName(0, QStringLiteral("Rank"));
        pt->setColPlotDesignation(0, SciDAVis::X);
        pt->setColName(1, srcTable->colLabel(yCol));
        pt->setColPlotDesignation(1, SciDAVis::Y);
        pt->setColName(2, QStringLiteral("Cumulative_Pct"));
        pt->setColPlotDesignation(2, SciDAVis::Y);
        pt->setColName(3, QStringLiteral("Category"));
        if (pt->column(3))
            pt->column(3)->setColumnMode(SciDAVis::ColumnMode::Text);
        pt->setColPlotDesignation(3, SciDAVis::noDesignation);

        QStringList sortedLabels;
        sortedLabels.reserve(items.size());
        double runSum = 0.0;
        for (int i = 0; i < items.size(); ++i) {
            runSum += items[i].val;
            pt->setCell(i, 0, i + 1);
            pt->setCell(i, 1, items[i].val);
            pt->setCell(i, 2, (runSum / totalSum) * 100.0);
            if (pt->column(3))
                pt->column(3)->setTextAt(i, items[i].label);
            sortedLabels.append(items[i].label);
        }
        MultiLayer *ml = d_app->multilayerPlot(pt, QStringList() << pt->colName(1),
                                               Graph::VerticalBars, 0, items.size() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Pareto plot."));
        if (Graph *g = ml->activeGraph()) {
            if (g->curves() >= 1) {
                g->setCurvePen(0, QPen(QColor(0, 77, 128), 1.0));
                g->setCurveBrush(0, QBrush(QColor(0, 114, 178, 210), Qt::SolidPattern));
                g->setBarsGap(0, 22, 0);
            }
            g->insertCurve(pt, pt->colName(0), pt->colName(2), Graph::LineSymbols, 0,
                           items.size() - 1);
            if (g->curves() >= 2 && g->curve(g->curves() - 1)) {
                QwtPlotCurve *cumC = g->curve(g->curves() - 1);
                cumC->setYAxis(QwtPlot::yRight);
                cumC->setPen(QPen(QColor(213, 94, 0), 2.0));
                QwtSymbol sym(QwtSymbol::Ellipse, QBrush(QColor(213, 94, 0)),
                              QPen(QColor(213, 94, 0), 1.2), QSize(6, 6));
                g->setCurveSymbol(g->curves() - 1, sym);
            }
            g->enableAxis(QwtPlot::yRight, true);
            if (!sortedLabels.isEmpty()) {
                invokeGraphSetCategoryLabels(d_app, g, QwtPlot::xBottom, sortedLabels);
                g->setAxisLabelRotation(QwtPlot::xBottom, -35);
            }
            if (catCol >= 0) {
                const QString xTitle = srcTable->column(catCol)
                        ? srcTable->column(catCol)->formattedAxisTitle().trimmed()
                        : srcTable->colLabel(catCol);
                g->setAxisTitleAuto(QwtPlot::xBottom, false);
                g->setXAxisTitle(xTitle.isEmpty() ? QStringLiteral("Category") : xTitle);
            } else {
                g->setAxisTitleAuto(QwtPlot::xBottom, false);
                g->setXAxisTitle(QStringLiteral("Category"));
            }
            const QString yTitle = srcTable->column(yCol)
                    ? srcTable->column(yCol)->formattedAxisTitle().trimmed()
                    : srcTable->colLabel(yCol);
            g->setAxisTitleAuto(QwtPlot::yLeft, false);
            g->setYAxisTitle(yTitle.isEmpty() ? QStringLiteral("Count") : yTitle);
            g->setAxisTitleAuto(QwtPlot::yRight, false);
            g->setRightAxisTitle(QStringLiteral("Cumulative (%)"));

            const double maxBar = items.first().val;
            const double yLeftTop = std::max(totalSum * 1.12, maxBar * 1.24);
            const double yRightTop = 100.0 * (yLeftTop / totalSum);
            g->setScale(QwtPlot::yLeft, 0.0, yLeftTop);
            g->setScale(QwtPlot::yRight, 0.0, yRightTop, 20.0);
            if (optionalJsonBool(params, QStringLiteral("show_80_line"), true)) {
                Graph::ReferenceLineSpec r80;
                r80.orientation = 0;
                r80.value = 0.80 * totalSum;
                r80.color = QColor(120, 120, 120);
                r80.style = Qt::DashLine;
                r80.width = 1.0;
                r80.label = QStringLiteral("80%");
                g->addReferenceLine(r80);
            }
            if (Legend *leg = g->legend()) {
                const QString barLbl = srcTable->colLongName(yCol).trimmed().isEmpty()
                        ? srcTable->colLabel(yCol)
                        : srcTable->colLongName(yCol).trimmed();
                leg->setText(QStringLiteral("\\c{1}%1\n\\c{2}Cumulative (%)").arg(barLbl));
            }
            g->autoPlaceLegend();
            applyPlotLevelConvenienceParams(g, params);
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
                if (std::isfinite(v) && std::abs(v) <= 1e300)
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
            const QString colDesc = srcTable->column(yCol)
                    ? srcTable->column(yCol)->formattedAxisTitle().trimmed()
                    : srcTable->colLabel(yCol);
            g->setAxisTitleAuto(QwtPlot::xBottom, false);
            g->setXAxisTitle(QStringLiteral("Theoretical Normal Quantiles"));
            g->setAxisTitleAuto(QwtPlot::yLeft, false);
            g->setYAxisTitle(
                    colDesc.isEmpty()
                            ? QStringLiteral("Sample Quantiles")
                            : ((colDesc.size() > 20)
                                       ? colDesc
                                       : QStringLiteral("Sample Quantiles (%1)").arg(colDesc)));
            if (Legend *leg = g->legend())
                leg->setText(QStringLiteral("\\c{1}Sample Quantiles\n\\c{2}Normal Reference"));
            g->placeLegendAnchor(Graph::LegendTopLeft);
            g->setAutoScale();
            applyPlotLevelConvenienceParams(g, params);
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
                : (params.contains(QStringLiteral("value_col"))
                           ? resolveColumnIndex(t, params.value(QStringLiteral("value_col")), true)
                           : ((t->numCols() >= 2) ? 1 : 0));
        int lblColIdx = -1;
        if (params.contains(QStringLiteral("label_col"))
            && !params.value(QStringLiteral("label_col")).isNull()) {
            lblColIdx = resolveColumnIndex(t, params.value(QStringLiteral("label_col")), true);
        } else if (params.contains(QStringLiteral("category_col"))
                   && !params.value(QStringLiteral("category_col")).isNull()) {
            lblColIdx = resolveColumnIndex(t, params.value(QStringLiteral("category_col")), true);
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != colIdx && t->column(c)
                    && t->column(c)->columnMode() == SciDAVis::ColumnMode::Text) {
                    lblColIdx = c;
                    break;
                }
            }
        }
        int posFinite = 0;
        double pieSum = 0.0;
        QStringList sliceLegendLines;
        if (Column *pc = t->column(colIdx)) {
            if (pc->columnMode() == SciDAVis::ColumnMode::Numeric) {
                for (int r = 0; r < t->numRows(); ++r) {
                    if (!pc->isInvalid(r) && !pc->isMasked(r)) {
                        const double v = pc->valueAt(r);
                        if (std::isfinite(v) && v >= 0.0 && v <= 1e300) {
                            ++posFinite;
                            pieSum += v;
                        }
                    }
                }
                if (pieSum > 0.0) {
                    int pIdx = 1;
                    int fbLblCol = lblColIdx;
                    if (fbLblCol < 0) {
                        for (int c = 0; c < t->numCols(); ++c) {
                            if (c != colIdx && t->column(c)) {
                                fbLblCol = c;
                                break;
                            }
                        }
                    }
                    for (int r = 0; r < t->numRows(); ++r) {
                        if (!pc->isInvalid(r) && !pc->isMasked(r)) {
                            const double v = pc->valueAt(r);
                            if (std::isfinite(v) && v >= 0.0) {
                                QString sl = (fbLblCol >= 0) ? t->text(r, fbLblCol).trimmed()
                                                             : QString();
                                if (sl.isEmpty())
                                    sl = QStringLiteral("Slice %1").arg(pIdx);
                                const double pct = (v / pieSum) * 100.0;
                                sliceLegendLines.append(
                                        QStringLiteral("\\p{%1}%2 (%3%)")
                                                .arg(pIdx++)
                                                .arg(sl, QString::number(pct, 'f', 1)));
                            }
                        }
                    }
                }
            }
        }
        if (posFinite <= 0 || pieSum <= 0.0 || !std::isfinite(pieSum)) {
            throwInvalidParams(
                    QStringLiteral("plot.pie requires non-negative numeric values with positive sum in column '%1'.")
                            .arg(t->colLabel(colIdx)));
        }
        MultiLayer *ml = d_app->multilayerPlot(t, QStringList() << fullColumnName(t, colIdx),
                                               Graph::Pie, 0, t->numRows() - 1);
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Pie plot."));
        if (!ml->activeGraph() || ml->activeGraph()->curves() <= 0) {
            closeAndDeleteEmptyPlot(d_app, ml);
            throwInvalidParams(QStringLiteral("plot.pie created 0 pie slices."));
        }
        if (Graph *g = ml->activeGraph()) {
            const bool explicitHideLeg = params.contains(QStringLiteral("legend"))
                    && params.value(QStringLiteral("legend")).isBool()
                    && !params.value(QStringLiteral("legend")).toBool();
            if (!sliceLegendLines.isEmpty() && g->legend())
                g->legend()->setText(sliceLegendLines.join(QLatin1Char('\n')));
            if (!explicitHideLeg && g->legend() && g->isLegendVisible()) {
                Legend *mainLeg = g->legend();
                const QVector<int> tKeys = g->textMarkerKeys();
                for (int tid : tKeys) {
                    Legend *tm = g->textMarker(tid);
                    if (tm && tm != mainLeg) {
                        g->setSelectedMarker(tid, false);
                        g->removeMarker();
                    }
                }
                g->deselectMarker();
                g->placeLegendAnchor(Graph::LegendTopCenter);
                g->replot();
            }
            applyPlotLevelConvenienceParams(g, params);
        }
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
        } else if (params.contains(QStringLiteral("x_col"))
                   || params.contains(QStringLiteral("y_col"))
                   || params.contains(QStringLiteral("u_col"))
                   || params.contains(QStringLiteral("v_col"))
                   || params.contains(QStringLiteral("dx_col"))
                   || params.contains(QStringLiteral("dy_col"))) {
            const QJsonValue xv = params.contains(QStringLiteral("x_col"))
                    ? params.value(QStringLiteral("x_col"))
                    : QJsonValue(0);
            const QJsonValue yv = params.contains(QStringLiteral("y_col"))
                    ? params.value(QStringLiteral("y_col"))
                    : QJsonValue(1);
            const QJsonValue uv = params.contains(QStringLiteral("u_col"))
                    ? params.value(QStringLiteral("u_col"))
                    : (params.contains(QStringLiteral("dx_col"))
                               ? params.value(QStringLiteral("dx_col"))
                               : QJsonValue(2));
            const QJsonValue vv = params.contains(QStringLiteral("v_col"))
                    ? params.value(QStringLiteral("v_col"))
                    : (params.contains(QStringLiteral("dy_col"))
                               ? params.value(QStringLiteral("dy_col"))
                               : QJsonValue(3));
            colIdxs << resolveColumnIndex(t, xv, true) << resolveColumnIndex(t, yv, true)
                    << resolveColumnIndex(t, uv, true) << resolveColumnIndex(t, vv, true);
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

        const QString mode = optionalJsonString(params, QStringLiteral("mode")).trimmed().toLower();
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
            if (std::isfinite(v0) && std::isfinite(v1) && std::isfinite(v2) && std::isfinite(v3)
                && std::abs(v0) <= 1e300 && std::abs(v1) <= 1e300 && std::abs(v2) <= 1e300
                && std::abs(v3) <= 1e300)
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
        if (auto *vc = dynamic_cast<VectorCurve *>(g->curve(g->curves() - 1))) {
            if (params.contains(QStringLiteral("color"))
                && !params.value(QStringLiteral("color")).isNull()) {
                vc->setColor(parseColorValue(params.value(QStringLiteral("color")), vc->color()));
            }
            const QJsonValue lwV = params.contains(QStringLiteral("line_width"))
                    ? params.value(QStringLiteral("line_width"))
                    : (params.contains(QStringLiteral("lw"))
                               ? params.value(QStringLiteral("lw"))
                               : params.value(QStringLiteral("width")));
            if (!lwV.isUndefined() && !lwV.isNull())
                vc->setWidth(std::clamp(qRound(lwV.toDouble(1.0)), 1, 20));
            if (params.contains(QStringLiteral("head_length")))
                vc->setHeadLength(parseBoundedInt(params, QStringLiteral("head_length"), 8, 1, 64));
            if (params.contains(QStringLiteral("head_angle")))
                vc->setHeadAngle(parseBoundedInt(params, QStringLiteral("head_angle"), 45, 5, 89));
        }
        applyPlotLevelConvenienceParams(g, params);
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

    if (method == QStringLiteral("plot.polar")) {
        Table *t = nullptr;
        if ((!params.contains(QStringLiteral("table"))
             || params.value(QStringLiteral("table")).isNull()
             || params.value(QStringLiteral("table")).toString().trimmed().isEmpty())
            && params.value(QStringLiteral("theta")).isArray()
            && params.value(QStringLiteral("r")).isArray()) {
            const QJsonArray thArr = params.value(QStringLiteral("theta")).toArray();
            const QJsonArray rArr = params.value(QStringLiteral("r")).toArray();
            const int nPts = std::min(thArr.size(), rArr.size());
            if (nPts < 1 || nPts > 1000000)
                throwInvalidParams(QStringLiteral("plot.polar inline 'theta'/'r' arrays must have 1..1,000,000 points."));
            t = createUniqueTableHeadless(d_app, nPts, 2, QStringLiteral("PolarData"));
            t->setColName(0, QStringLiteral("Theta"));
            t->setColPlotDesignation(0, SciDAVis::X);
            t->setColName(1, QStringLiteral("R"));
            t->setColPlotDesignation(1, SciDAVis::Y);
            for (int i = 0; i < nPts; ++i) {
                t->setCell(i, 0, parseFiniteCoordDouble(thArr[i], QStringLiteral("theta")));
                t->setCell(i, 1, parseFiniteCoordDouble(rArr[i], QStringLiteral("r")));
            }
        } else {
            t = resolveTable(d_app, params);
        }
        int thetaIdx = -1;
        if (params.contains(QStringLiteral("theta_col"))
            && !params.value(QStringLiteral("theta_col")).isNull()) {
            thetaIdx = resolveColumnIndex(t, params.value(QStringLiteral("theta_col")), true);
        } else if (params.contains(QStringLiteral("x_col"))
                   && !params.value(QStringLiteral("x_col")).isNull()) {
            thetaIdx = resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true);
        } else {
            thetaIdx = t->firstXCol();
            if (thetaIdx < 0)
                thetaIdx = 0;
        }

        QVector<int> rIdxs;
        const QJsonValue rSpec = params.contains(QStringLiteral("r_cols"))
                ? params.value(QStringLiteral("r_cols"))
                : (params.contains(QStringLiteral("r_col"))
                           ? params.value(QStringLiteral("r_col"))
                           : (params.contains(QStringLiteral("y_cols"))
                                      ? params.value(QStringLiteral("y_cols"))
                                      : params.value(QStringLiteral("y_col"))));
        if (rSpec.isArray()) {
            for (const QJsonValue &it : rSpec.toArray())
                rIdxs.append(resolveColumnIndex(t, it, true));
        } else if (!rSpec.isUndefined() && !rSpec.isNull()) {
            rIdxs.append(resolveColumnIndex(t, rSpec, true));
        } else {
            for (int c = 0; c < t->numCols(); ++c) {
                if (c != thetaIdx && t->column(c)
                    && t->column(c)->columnMode() == SciDAVis::ColumnMode::Numeric) {
                    rIdxs.append(c);
                }
            }
        }
        if (rIdxs.isEmpty())
            throwInvalidParams(QStringLiteral("plot.polar requires at least one radial column."));

        bool thetaInRad = false;
        if (params.contains(QStringLiteral("theta_in_radians"))
            && !params.value(QStringLiteral("theta_in_radians")).isNull()) {
            thetaInRad = optionalJsonBool(params, QStringLiteral("theta_in_radians"), false);
        } else if (params.contains(QStringLiteral("radians"))
                   && !params.value(QStringLiteral("radians")).isNull()) {
            thetaInRad = optionalJsonBool(params, QStringLiteral("radians"), false);
        } else if (params.contains(QStringLiteral("degrees"))
                   && !params.value(QStringLiteral("degrees")).isNull()) {
            thetaInRad = !optionalJsonBool(params, QStringLiteral("degrees"), true);
        } else if (params.contains(QStringLiteral("units"))
                   && !params.value(QStringLiteral("units")).isNull()) {
            const QString uStr = optionalJsonString(params, QStringLiteral("units")).trimmed().toLower();
            thetaInRad = uStr.startsWith(QStringLiteral("rad"));
        }

        MultiLayer *ml = d_app->newGraph(d_app->generateUniqueName(QStringLiteral("PolarGraph")));
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create Polar graph."));
        Graph *g = ml->activeGraph() ? ml->activeGraph() : ml->addLayer();
        g->setProperty("_toriIsPolarPlot", true);

        auto plotPolarSeries = [&](auto *graphPtr) {
            using GType = std::remove_pointer_t<decltype(graphPtr)>;
            double rMaxAll = 0.0;
            int validSeries = 0;
            const QList<QColor> &kPolarPalette = Graph::okabeItoColors();
            for (int si = 0; si < rIdxs.size(); ++si) {
                const int rColIdx = rIdxs[si];
                Column *thCol = t->column(thetaIdx);
                Column *raCol = t->column(rColIdx);
                if (!thCol || !raCol || thCol->columnMode() != SciDAVis::ColumnMode::Numeric
                    || raCol->columnMode() != SciDAVis::ColumnMode::Numeric) {
                    continue;
                }
                QVector<double> thVec;
                QVector<double> rVec;
                for (int r = 0; r < t->numRows(); ++r) {
                    if (thCol->isInvalid(r) || thCol->isMasked(r) || raCol->isInvalid(r)
                        || raCol->isMasked(r))
                        continue;
                    const double tv = thCol->valueAt(r);
                    const double rv = raCol->valueAt(r);
                    if (std::isfinite(tv) && std::isfinite(rv) && std::abs(tv) <= 1e300
                        && std::abs(rv) <= 1e300) {
                        thVec.append(tv);
                        rVec.append(rv);
                        rMaxAll = std::max(rMaxAll, std::abs(rv));
                    }
                }
                if (thVec.isEmpty())
                    continue;
                const QString sLabel = t->colLongName(rColIdx).trimmed().isEmpty()
                        ? t->colLabel(rColIdx)
                        : t->colLongName(rColIdx).trimmed();

                if constexpr (HasGraphSetupPolarPlot<GType>::value) {
                    graphPtr->setupPolarPlot(thVec, rVec, sLabel, thetaInRad);
                    if (graphPtr->curves() > 0)
                        applyInlineCurveStyleParams(graphPtr, graphPtr->curves() - 1, params,
                                                    rIdxs.size() == 1);
                    ++validSeries;
                } else {
                    auto *cx = new Column(QStringLiteral("Polar_X"), SciDAVis::ColumnMode::Numeric);
                    cx->setPlotDesignation(SciDAVis::X);
                    auto *cy = new Column(t->colLabel(rColIdx), SciDAVis::ColumnMode::Numeric);
                    cy->setPlotDesignation(SciDAVis::Y);
                    cy->setComment(sLabel);
                    const double degToRad = 3.14159265358979323846 / 180.0;
                    for (int k = 0; k < thVec.size(); ++k) {
                        const double ang = thetaInRad ? thVec[k] : (thVec[k] * degToRad);
                        cx->setValueAt(k, rVec[k] * std::cos(ang));
                        cy->setValueAt(k, rVec[k] * std::sin(ang));
                    }
                    const QString pTblName =
                            d_app->generateUniqueName(QStringLiteral("PolarCartesian"));
                    Table *pTbl = d_app->newHiddenTable(pTblName, sLabel,
                                                        QList<Column *>() << cx << cy);
                    if (!pTbl) {
                        delete cx;
                        delete cy;
                        continue;
                    }
                    const int cStyle = parsePlotStyle(
                            optionalJsonString(params, QStringLiteral("style")), Graph::Line);
                    graphPtr->insertCurve(pTbl, pTbl->colName(0), pTbl->colName(1), cStyle, 0,
                                          thVec.size() - 1);
                    const int ci = graphPtr->curves() - 1;
                    if (ci >= 0) {
                        const QColor sc = kPolarPalette.at(si % kPolarPalette.size());
                        graphPtr->setCurvePen(ci, QPen(sc, 2.0));
                        applyInlineCurveStyleParams(graphPtr, ci, params, rIdxs.size() == 1);
                    }
                    ++validSeries;
                }
            }
            if (validSeries <= 0) {
                closeAndDeleteEmptyPlot(d_app, ml);
                throwInvalidParams(
                        QStringLiteral("plot.polar found 0 valid finite (theta, r) points."));
            }
            if constexpr (!HasGraphSetupPolarPlot<GType>::value) {
                const double spanR = (rMaxAll > 0.0) ? (rMaxAll * 1.18) : 1.0;
                const int nRings = 4;
                const int ringPts = 97;
                for (int ri = 1; ri <= nRings; ++ri) {
                    const double rr = (rMaxAll > 0.0 ? rMaxAll : 1.0) * double(ri) / double(nRings);
                    auto *rxC = new Column(QStringLiteral("RX"), SciDAVis::ColumnMode::Numeric);
                    rxC->setPlotDesignation(SciDAVis::X);
                    auto *ryC = new Column(QStringLiteral("RY"), SciDAVis::ColumnMode::Numeric);
                    ryC->setPlotDesignation(SciDAVis::Y);
                    for (int p = 0; p < ringPts; ++p) {
                        const double a = 2.0 * 3.14159265358979323846 * double(p)
                                / double(ringPts - 1);
                        rxC->setValueAt(p, rr * std::cos(a));
                        ryC->setValueAt(p, rr * std::sin(a));
                    }
                    Table *rt = d_app->newHiddenTable(
                            d_app->generateUniqueName(QStringLiteral("PolarRing")),
                            QStringLiteral("PolarRing"), QList<Column *>() << rxC << ryC);
                    if (rt) {
                        graphPtr->insertCurve(rt, rt->colName(0), rt->colName(1), Graph::Line, 0,
                                              ringPts - 1);
                        const int rci = graphPtr->curves() - 1;
                        graphPtr->setCurvePen(
                                rci, QPen(QColor(180, 185, 195), 0.75, Qt::DotLine));
                    } else {
                        delete rxC;
                        delete ryC;
                    }
                }
                for (int deg = 0; deg < 360; deg += 30) {
                    const double a = double(deg) * 3.14159265358979323846 / 180.0;
                    const double rEdge = (rMaxAll > 0.0 ? rMaxAll : 1.0);
                    auto *am = new ArrowMarker();
                    am->attach(graphPtr->plotWidget());
                    am->setStartPoint(0.0, 0.0);
                    am->setEndPoint(rEdge * std::cos(a), rEdge * std::sin(a));
                    am->setColor(QColor(190, 195, 205));
                    am->setWidth(1);
                    am->setStyle(Qt::DotLine);
                    am->drawStartArrow(false);
                    am->drawEndArrow(false);
                    graphPtr->addArrow(am);

                    Legend *lbl = graphPtr->newLegend(QStringLiteral("%1\u00B0").arg(deg));
                    if (lbl) {
                        lbl->setFrameStyle(0);
                        lbl->setBackgroundColor(QColor(255, 255, 255, 0));
                        lbl->setOriginCoord(1.09 * rEdge * std::cos(a),
                                            1.09 * rEdge * std::sin(a));
                    }
                }
                graphPtr->setScale(QwtPlot::xBottom, -spanR, spanR);
                graphPtr->setScale(QwtPlot::yLeft, -spanR, spanR);
                graphPtr->setAxisUserLocked(QwtPlot::xBottom, true);
                graphPtr->setAxisUserLocked(QwtPlot::yLeft, true);
                if (Legend *leg = graphPtr->legend()) {
                    QStringList keepLines;
                    for (int s = 0; s < validSeries; ++s) {
                        const QString sLbl = t->colLongName(rIdxs[s]).trimmed().isEmpty()
                                ? t->colLabel(rIdxs[s])
                                : t->colLongName(rIdxs[s]).trimmed();
                        keepLines.append(QStringLiteral("\\c{%1}%2").arg(s + 1).arg(sLbl));
                    }
                    leg->setText(keepLines.join(QLatin1Char('\n')));
                }
            }
        };
        plotPolarSeries(g);
        ml->setKeepAspectRatio(true);
        ml->setTargetAspectRatio(1.0);
        applyPlotLevelConvenienceParams(g, params);
        applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("is_polar")] = true;
        outResult[QStringLiteral("theta_in_radians")] = thetaInRad;
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.function")) {
        const QString typeStr =
                optionalJsonString(params, QStringLiteral("type"), QStringLiteral("normal"))
                        .trimmed()
                        .toLower();
        FunctionCurve::FunctionType fnType = FunctionCurve::Normal;
        if (typeStr.isEmpty() || typeStr == QStringLiteral("normal")
            || typeStr == QStringLiteral("cartesian") || typeStr == QStringLiteral("explicit")) {
            fnType = FunctionCurve::Normal;
        } else if (typeStr == QStringLiteral("parametric")) {
            fnType = FunctionCurve::Parametric;
        } else if (typeStr == QStringLiteral("polar")) {
            fnType = FunctionCurve::Polar;
        } else {
            throwInvalidParams(
                    QStringLiteral("Unknown function type '%1' (expected 'normal'|'parametric'|'polar').")
                            .arg(typeStr));
        }

        const bool explicitVar = params.contains(QStringLiteral("variable"))
                && !params.value(QStringLiteral("variable")).isNull()
                && !optionalJsonString(params, QStringLiteral("variable")).trimmed().isEmpty();
        const QString defaultVar =
                (fnType == FunctionCurve::Normal) ? QStringLiteral("x") : QStringLiteral("t");
        QString var =
                optionalJsonString(params, QStringLiteral("variable"), defaultVar).trimmed();
        if (var.isEmpty())
            throwInvalidParams(QStringLiteral("Parameter 'variable' must be a non-empty identifier."));

        QStringList formulas;
        const QJsonValue fVal = params.contains(QStringLiteral("formula"))
                ? params.value(QStringLiteral("formula"))
                : params.value(QStringLiteral("formulas"));
        if (fVal.isArray()) {
            for (const QJsonValue &fv : fVal.toArray()) {
                if (!fv.isString())
                    throwInvalidParams(QStringLiteral("Each entry in 'formula' list must be a string."));
                formulas << sanitizeUtf16Surrogates(fv.toString()).trimmed();
            }
        } else if (fVal.isString()) {
            const QString single = sanitizeUtf16Surrogates(fVal.toString()).trimmed();
            if (!single.isEmpty())
                formulas << single;
        }

        if (!explicitVar && fnType != FunctionCurve::Normal && !formulas.isEmpty()
            && !formulas[0].isEmpty()) {
            if (!checkMuParserExprValid(formulas[0], { var })) {
                if (checkMuParserExprValid(formulas[0], { QStringLiteral("theta") }))
                    var = QStringLiteral("theta");
                else if (checkMuParserExprValid(formulas[0], { QStringLiteral("x") }))
                    var = QStringLiteral("x");
            }
        }

        if (fnType == FunctionCurve::Normal) {
            if (formulas.isEmpty() || formulas[0].isEmpty())
                throwInvalidParams(QStringLiteral("'formula' is required for plot.function."));
            validateMuParserExprOrThrow(formulas[0], { var }, QStringLiteral("plot.function"));
        } else if (fnType == FunctionCurve::Parametric) {
            if (formulas.size() < 2 && params.contains(QStringLiteral("y_formula"))) {
                formulas << optionalJsonString(params, QStringLiteral("y_formula")).trimmed();
            }
            if (formulas.size() < 2 || formulas[0].isEmpty() || formulas[1].isEmpty()) {
                throwInvalidParams(
                        QStringLiteral("Parametric plot.function requires 2-element 'formula': [x_expr, y_expr]."));
            }
            validateMuParserExprOrThrow(formulas[0], { var }, QStringLiteral("plot.function"));
            validateMuParserExprOrThrow(formulas[1], { var }, QStringLiteral("plot.function"));
        } else {
            // FunctionCurve::Polar expects [r(var), theta(var)] in radians
            if (formulas.size() == 1 && !formulas[0].isEmpty())
                formulas << var;
            if (formulas.size() < 2 || formulas[0].isEmpty() || formulas[1].isEmpty()) {
                throwInvalidParams(
                        QStringLiteral("Polar plot.function requires 'formula': r_expr or [r_expr, theta_expr]."));
            }
            validateMuParserExprOrThrow(formulas[0], { var }, QStringLiteral("plot.function"));
            validateMuParserExprOrThrow(formulas[1], { var }, QStringLiteral("plot.function"));
        }

        const double defaultEnd =
                (fnType == FunctionCurve::Normal) ? 10.0 : (2.0 * 3.14159265358979323846);
        const double xMin = params.contains(QStringLiteral("x_min"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("x_min"), 0.0)
                : optionalJsonFiniteDouble(params, QStringLiteral("from"), 0.0);
        const double xMax = params.contains(QStringLiteral("x_max"))
                ? optionalJsonFiniteDouble(params, QStringLiteral("x_max"), defaultEnd)
                : optionalJsonFiniteDouble(params, QStringLiteral("to"), defaultEnd);
        validateFiniteCoordRange(xMin, xMax, QStringLiteral("plot.function range"));
        const int points = parseBoundedInt(params, QStringLiteral("points"), 200, 2, 1000000);
        QList<double> ranges;
        ranges << xMin << xMax;

        MultiLayer *ml = nullptr;
        const bool createdNewGraph = !params.contains(QStringLiteral("graph"))
                || params.value(QStringLiteral("graph")).isNull()
                || optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty();
        if (!createdNewGraph)
            ml = resolveMultiLayer(d_app, params, QStringLiteral("graph"), true);
        else
            ml = d_app->newGraph(d_app->generateUniqueName(QStringLiteral("Graph")));
        Graph *g = createdNewGraph
                ? (ml->activeGraph() ? ml->activeGraph() : ml->addLayer())
                : resolveGraphLayer(ml, params);
        ScopedGraphUndoGuard undoGuard(createdNewGraph ? nullptr : ml,
                                       QObject::tr("Plot Function"));
        if (!g->addFunctionCurve(d_app, fnType, formulas, var, ranges, points,
                                 optionalJsonString(params, QStringLiteral("title")))) {
            if (createdNewGraph)
                closeAndDeleteEmptyPlot(d_app, ml);
            throwRpcRuntime(
                    QStringLiteral("Failed to plot function '%1'.").arg(formulas.join(QStringLiteral(", "))));
        }
        if (g->curves() > 0)
            applyInlineCurveStyleParams(g, g->curves() - 1, params, true);
        if (fnType == FunctionCurve::Polar) {
            g->setProperty("_toriIsPolarPlot", true);
            ml->setKeepAspectRatio(true);
            ml->setTargetAspectRatio(1.0);
        }
        applyPlotLevelConvenienceParams(g, params);
        if (!createdNewGraph)
            undoGuard.commit();
        if (createdNewGraph)
            applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("function_type")] = typeStr;
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.matrix")) {
        Matrix *m = resolveMatrix(d_app, params);
        const QString style = optionalJsonString(params, QStringLiteral("style")).trimmed().toLower();
        Graph::CurveType cType = Graph::ColorMap;
        if (style.isEmpty() || style == QStringLiteral("colormap")
            || style == QStringLiteral("color_map") || style == QStringLiteral("heatmap")
            || style == QStringLiteral("image")) {
            cType = Graph::ColorMap;
        } else if (style == QStringLiteral("contour") || style == QStringLiteral("contour_map")) {
            cType = Graph::ContourMap;
        } else if (style == QStringLiteral("grayscale") || style == QStringLiteral("gray")
                   || style == QStringLiteral("grey") || style == QStringLiteral("gray_map")) {
            cType = Graph::GrayMap;
        } else {
            throwInvalidParams(
                    QStringLiteral("Unknown plot.matrix style '%1' (expected 'colormap'|'contour'|'grayscale').")
                            .arg(style));
        }
        const bool attachToExisting =
                !optionalJsonString(params, QStringLiteral("graph")).trimmed().isEmpty();
        MultiLayer *ml = nullptr;
        Graph *targetG = nullptr;
        if (attachToExisting) {
            ml = resolveMultiLayer(d_app, params);
            targetG = resolveGraphLayer(ml, params);
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Plot Matrix"));
            targetG->plotSpectrogram(m, cType);
            enforceArialGraphFonts(targetG);
            undoGuard.commit();
        } else {
            ml = d_app->plotSpectrogram(m, cType);
            targetG = ml ? ml->activeGraph() : nullptr;
        }
        if (!ml)
            throwRpcRuntime(QStringLiteral("Failed to create matrix spectrogram plot."));
        if (Graph *g = targetG) {
            const QString cmapParam = !optionalJsonString(params, QStringLiteral("palette")).trimmed().isEmpty()
                    ? optionalJsonString(params, QStringLiteral("palette")).trimmed()
                    : (!optionalJsonString(params, QStringLiteral("colormap")).trimmed().isEmpty()
                               ? optionalJsonString(params, QStringLiteral("colormap")).trimmed()
                               : optionalJsonString(params, QStringLiteral("cmap")).trimmed());
            const bool hasPalette = !cmapParam.isEmpty();
            const bool hasReverse = params.contains(QStringLiteral("reverse"))
                    && !params.value(QStringLiteral("reverse")).isNull();
            const bool hasLevels = params.contains(QStringLiteral("levels"))
                    && !params.value(QStringLiteral("levels")).isNull();
            const int reqLevels =
                    hasLevels ? parseBoundedInt(params, QStringLiteral("levels"), 12, 2, 256) : 12;
            const bool hasShowCb = params.contains(QStringLiteral("show_colorbar"))
                    && !params.value(QStringLiteral("show_colorbar")).isNull();
            const bool hasUseCmPen = params.contains(QStringLiteral("use_colormap_pen"))
                    && !params.value(QStringLiteral("use_colormap_pen")).isNull();
            const QString cbTitle = optionalJsonString(params, QStringLiteral("colorbar_title"));
            if (hasPalette || hasReverse || hasLevels || hasShowCb || hasUseCmPen
                || params.contains(QStringLiteral("colorbar_title"))) {
                bool isGray = false;
                const auto preset = hasPalette
                        ? parseColorMapPreset(cmapParam, &isGray, true)
                        : Spectrogram::PresetViridis;
                const bool rev = optionalJsonBool(params, QStringLiteral("reverse"), false);
                for (QwtPlotItem *it : g->plotWidget()->itemList()) {
                    if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                        if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
                            if (hasPalette) {
                                sp->setColorMapPreset(
                                        isGray ? Spectrogram::PresetGrayScale : preset, rev);
                            } else if (hasReverse) {
                                sp->setReversePalette(rev);
                            }
                            if (hasLevels)
                                sp->setLevelsNumber(reqLevels);
                            if (hasUseCmPen) {
                                const bool useCmPen = optionalJsonBool(
                                        params, QStringLiteral("use_colormap_pen"), true);
                                sp->setDefaultContourPen(useCmPen ? QPen(Qt::NoPen)
                                                                  : QPen(Qt::black, 1.2));
                            }
                            if (hasShowCb)
                                sp->showColorScale(sp->colorScaleAxis(),
                                                   optionalJsonBool(params,
                                                                    QStringLiteral("show_colorbar"),
                                                                    true));
                            if (params.contains(QStringLiteral("colorbar_title")))
                                sp->setColorBarTitle(cbTitle);
                            sp->enforceColorBarArialFont();
                            sp->syncColorScale();
                        }
                    }
                }
            }
            applyPlotLevelConvenienceParams(g, params);
        }
        if (!attachToExisting)
            applyRequestedWindowName(d_app, ml, params);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("plot.3d_matrix")) {
        Matrix *m = resolveMatrix(d_app, params);
        const QString style = optionalJsonString(params, QStringLiteral("style")).trimmed().toLower();
        if (!style.isEmpty() && style != QStringLiteral("surface")
            && style != QStringLiteral("filled") && style != QStringLiteral("filled_mesh")
            && style != QStringLiteral("wireframe") && style != QStringLiteral("mesh")
            && style != QStringLiteral("line_grid") && style != QStringLiteral("hidden_line")
            && style != QStringLiteral("bars") && style != QStringLiteral("scatter")
            && style != QStringLiteral("dots") && style != QStringLiteral("points")) {
            throwInvalidParams(
                    QStringLiteral("Unknown plot.3d_matrix style '%1' (expected 'surface'|'wireframe'|'hidden_line'|'bars'|'scatter').")
                            .arg(style));
        }
        const QString caption = d_app->generateUniqueName(QStringLiteral("Graph3D"), true);

        auto *plot = new Graph3D(QString(), &d_app->d_workspace, nullptr);
        plot->setAttribute(Qt::WA_DeleteOnClose);
        plot->addMatrixData(m);
        if (style == QStringLiteral("wireframe") || style == QStringLiteral("mesh")
            || style == QStringLiteral("line_grid"))
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
        const QString style = optionalJsonString(params, QStringLiteral("style")).trimmed().toLower();
        if (!style.isEmpty() && style != QStringLiteral("scatter")
            && style != QStringLiteral("dots") && style != QStringLiteral("points")
            && style != QStringLiteral("trajectory") && style != QStringLiteral("line")
            && style != QStringLiteral("bars") && style != QStringLiteral("ribbon")) {
            throwInvalidParams(
                    QStringLiteral("Unknown plot.3d_xyz style '%1' (expected 'scatter'|'trajectory'|'bars'|'ribbon').")
                            .arg(style));
        }
        if (style == QStringLiteral("ribbon")) {
            const int xIdx = params.contains(QStringLiteral("x_col"))
                    ? resolveColumnIndex(t, params.value(QStringLiteral("x_col")), true)
                    : 0;
            const int yIdx = params.contains(QStringLiteral("z_col"))
                    ? resolveColumnIndex(t, params.value(QStringLiteral("z_col")), true)
                    : (params.contains(QStringLiteral("y_col"))
                               ? resolveColumnIndex(t, params.value(QStringLiteral("y_col")), true)
                               : std::min(1, t->numCols() - 1));
            t->setColPlotDesignation(xIdx, SciDAVis::X);
            t->setColPlotDesignation(yIdx, SciDAVis::Y);
            Graph3D *plot = d_app->dataPlot3D(t, fullColumnName(t, yIdx));
            if (!plot)
                throwRpcRuntime(QStringLiteral("Failed to create 3D Ribbon plot."));
            enforceArialGraph3DFonts(plot);
            applyRequestedWindowName(d_app, plot, params);
            outResult = serializeGraph3DInfo(plot);
            outResult[QStringLiteral("style")] = QStringLiteral("ribbon");
            outResult[QStringLiteral("ok")] = true;
            return true;
        }

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
        const QString formula = optionalJsonString(params, QStringLiteral("formula")).trimmed();
        if (formula.isEmpty())
            throwInvalidParams(QStringLiteral("'formula' is required for plot.3d_function."));
        const double xl = optionalJsonFiniteDouble(params, QStringLiteral("x_min"), -5.0);
        const double xr = optionalJsonFiniteDouble(params, QStringLiteral("x_max"), 5.0);
        const double yl = optionalJsonFiniteDouble(params, QStringLiteral("y_min"), -5.0);
        const double yr = optionalJsonFiniteDouble(params, QStringLiteral("y_max"), 5.0);
        const double zl = optionalJsonFiniteDouble(params, QStringLiteral("z_min"), -5.0);
        const double zr = optionalJsonFiniteDouble(params, QStringLiteral("z_max"), 5.0);
        validateFiniteCoordRange(xl, xr, QStringLiteral("plot.3d_function X range"));
        validateFiniteCoordRange(yl, yr, QStringLiteral("plot.3d_function Y range"));
        validateFiniteCoordRange(zl, zr, QStringLiteral("plot.3d_function Z range"));
        if (xl >= xr || yl >= yr || zl >= zr) {
            throwInvalidParams(QStringLiteral("plot.3d_function requires min < max on X, Y, and Z."));
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

    if (method == QStringLiteral("graph.inspect")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const bool includeTicks = params.contains(QStringLiteral("include_ticks"))
                ? params.value(QStringLiteral("include_ticks")).toBool(true)
                : true;
        const bool diagnose = params.contains(QStringLiteral("diagnose"))
                ? params.value(QStringLiteral("diagnose")).toBool(true)
                : true;
        outResult = inspectGraphDetailed(ml, includeTicks, diagnose);
        return true;
    }

    if (method == QStringLiteral("graph.remove_layer")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        if (!params.contains(QStringLiteral("layer"))
            || params.value(QStringLiteral("layer")).isNull()) {
            throwInvalidParams(QStringLiteral("graph.remove_layer requires 0-based 'layer' index."));
        }
        if (ml->layers() <= 1) {
            throwInvalidParams(
                    QStringLiteral("Cannot remove the last remaining layer of graph '%1'.")
                            .arg(ml->name()));
        }
        Graph *targetG = resolveGraphLayer(ml, params, true);
        const int removedIdx = ml->layersList().indexOf(targetG);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Remove Layer"));
        ml->setActiveGraph(targetG);
        ml->removeLayer();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("removed_layer")] = removedIdx;
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.add_layer")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        if (ml->layers() >= 128)
            throwInvalidParams(QStringLiteral("Maximum layer count (128) reached on graph '%1'.")
                                       .arg(ml->name()));
        const int x = parseBoundedInt(params, QStringLiteral("x"), 0, 0, 16384);
        const int y = parseBoundedInt(params, QStringLiteral("y"), 0, 0, 16384);
        const int w = parseBoundedInt(params, QStringLiteral("width"), 0, 0, 16384);
        const int h = parseBoundedInt(params, QStringLiteral("height"), 0, 0, 16384);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Layer"));
        Graph *g = ml->addLayer(x, y, w, h);
        enforceArialGraphFonts(g);
        if (optionalJsonBool(params, QStringLiteral("arrange"), true) && w == 0 && h == 0)
            ml->arrangeLayers(true, false);
        if (g)
            undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        const int newIdx = std::max(0, ml->layers() - 1);
        outResult[QStringLiteral("layer")] = newIdx;
        outResult[QStringLiteral("layer_index")] = newIdx;
        outResult[QStringLiteral("ok")] = (g != nullptr);
        return true;
    }

    if (method == QStringLiteral("graph.add_linked_layer")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        if (ml->layers() >= 128)
            throwInvalidParams(QStringLiteral("Maximum layer count (128) reached on graph '%1'.")
                                       .arg(ml->name()));
        QJsonObject parentParams = params;
        if (params.contains(QStringLiteral("parent_layer"))
            && !params.value(QStringLiteral("parent_layer")).isNull()) {
            parentParams[QStringLiteral("layer")] = params.value(QStringLiteral("parent_layer"));
        }
        Graph *parentG = resolveGraphLayer(ml, parentParams, true);
        const bool linkX = optionalJsonBool(params, QStringLiteral("link_x"), true);
        const bool linkY = params.contains(QStringLiteral("link_y"))
                ? optionalJsonBool(params, QStringLiteral("link_y"), false)
                : optionalJsonBool(params, QStringLiteral("right_y"), false);
        const QString xExpr = params.contains(QStringLiteral("x_formula"))
                ? optionalJsonString(params, QStringLiteral("x_formula"), QStringLiteral("x")).trimmed()
                : optionalJsonString(params, QStringLiteral("x_expr"), QStringLiteral("x")).trimmed();
        const QString yExpr = params.contains(QStringLiteral("y_formula"))
                ? optionalJsonString(params, QStringLiteral("y_formula"), QStringLiteral("y")).trimmed()
                : optionalJsonString(params, QStringLiteral("y_expr"), QStringLiteral("y")).trimmed();
        if (linkX)
            validateMuParserExprOrThrow(xExpr, { QStringLiteral("x") },
                                        QStringLiteral("graph.add_linked_layer"));
        if (linkY)
            validateMuParserExprOrThrow(yExpr, { QStringLiteral("y") },
                                        QStringLiteral("graph.add_linked_layer"));
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Linked Layer"));
        Graph *childG = ml->addLinkedLayer(parentG, linkX, linkY, xExpr, yExpr);
        enforceArialGraphFonts(childG);
        if (childG)
            undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        const int newIdx = std::max(0, ml->layers() - 1);
        outResult[QStringLiteral("layer")] = newIdx;
        outResult[QStringLiteral("layer_index")] = newIdx;
        outResult[QStringLiteral("link_x")] = linkX;
        outResult[QStringLiteral("link_y")] = linkY;
        outResult[QStringLiteral("ok")] = (childG != nullptr);
        return true;
    }

    if (method == QStringLiteral("graph.add_inset_layer")
        || method == QStringLiteral("graph.add_inset")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        if (ml->layers() >= 128)
            throwInvalidParams(QStringLiteral("Maximum layer count (128) reached on graph '%1'.")
                                       .arg(ml->name()));
        QJsonObject srcParams = params;
        if (params.contains(QStringLiteral("source_layer"))
            && !params.value(QStringLiteral("source_layer")).isNull()) {
            srcParams[QStringLiteral("layer")] = params.value(QStringLiteral("source_layer"));
        }
        Graph *srcG = resolveGraphLayer(ml, srcParams, true);
        if (ml->isInsetLayer(srcG)) {
            throwInvalidParams(
                    QStringLiteral("Cannot create a nested inset inside an existing inset layer."));
        }
        QRectF roi;
        const bool hasXBounds = params.contains(QStringLiteral("x_min"))
                && params.contains(QStringLiteral("x_max"))
                && !params.value(QStringLiteral("x_min")).isNull()
                && !params.value(QStringLiteral("x_max")).isNull();
        const bool hasYBounds = params.contains(QStringLiteral("y_min"))
                && params.contains(QStringLiteral("y_max"))
                && !params.value(QStringLiteral("y_min")).isNull()
                && !params.value(QStringLiteral("y_max")).isNull();
        if (hasXBounds && hasYBounds) {
            const double xMin = parseFiniteCoordDouble(params.value(QStringLiteral("x_min")),
                                                       QStringLiteral("x_min"));
            const double xMax = parseFiniteCoordDouble(params.value(QStringLiteral("x_max")),
                                                       QStringLiteral("x_max"));
            const double yMin = parseFiniteCoordDouble(params.value(QStringLiteral("y_min")),
                                                       QStringLiteral("y_min"));
            const double yMax = parseFiniteCoordDouble(params.value(QStringLiteral("y_max")),
                                                       QStringLiteral("y_max"));
            validateFiniteCoordRange(xMin, xMax, QStringLiteral("Inset ROI X"));
            validateFiniteCoordRange(yMin, yMax, QStringLiteral("Inset ROI Y"));
            roi = QRectF(QPointF(std::min(xMin, xMax), std::min(yMin, yMax)),
                         QPointF(std::max(xMin, xMax), std::max(yMin, yMax)));
        } else if (hasXBounds && srcG) {
            const double xMin = parseFiniteCoordDouble(params.value(QStringLiteral("x_min")),
                                                       QStringLiteral("x_min"));
            const double xMax = parseFiniteCoordDouble(params.value(QStringLiteral("x_max")),
                                                       QStringLiteral("x_max"));
            validateFiniteCoordRange(xMin, xMax, QStringLiteral("Inset ROI X"));
            const double xLo = std::min(xMin, xMax);
            const double xHi = std::max(xMin, xMax);
            double yLo = std::numeric_limits<double>::infinity();
            double yHi = -std::numeric_limits<double>::infinity();
            for (int ci = 0; ci < srcG->curves(); ++ci) {
                QwtPlotCurve *c = srcG->curve(ci);
                if (!c || !srcG->isCurveVisible(ci) || srcG->curveType(ci) == Graph::ErrorBars)
                    continue;
                for (int k = 0; k < int(c->dataSize()); ++k) {
                    const double xv = c->x(k);
                    const double yv = c->y(k);
                    if (std::isfinite(xv) && std::isfinite(yv) && xv >= xLo && xv <= xHi) {
                        yLo = std::min(yLo, yv);
                        yHi = std::max(yHi, yv);
                    }
                }
            }
            if (yLo <= yHi && std::isfinite(yHi - yLo)) {
                const double pad = (yHi > yLo) ? (0.08 * (yHi - yLo)) : 0.5;
                roi = QRectF(QPointF(xLo, yLo - pad), QPointF(xHi, yHi + pad));
            }
        } else if (params.value(QStringLiteral("zoom_rect")).isArray()
                   && params.value(QStringLiteral("zoom_rect")).toArray().size() == 4) {
            const QJsonArray za = params.value(QStringLiteral("zoom_rect")).toArray();
            const double zx = parseFiniteCoordDouble(za[0], QStringLiteral("zoom_rect[0]"));
            const double zy = parseFiniteCoordDouble(za[1], QStringLiteral("zoom_rect[1]"));
            const double zw = parseFiniteCoordDouble(za[2], QStringLiteral("zoom_rect[2]"));
            const double zh = parseFiniteCoordDouble(za[3], QStringLiteral("zoom_rect[3]"));
            if (zw != 0.0 && zh != 0.0)
                roi = QRectF(zx, zy, zw, zh).normalized();
        }
        const bool showConnectors = optionalJsonBool(params, QStringLiteral("show_connectors"), true);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Add Inset Layer"));
        Graph *insetG = ml->addInsetLayer(srcG, roi, showConnectors);
        if (insetG)
            ml->setInsetShowConnectors(insetG, showConnectors);
        QJsonArray rectArr;
        if (params.value(QStringLiteral("rect")).isArray()) {
            rectArr = params.value(QStringLiteral("rect")).toArray();
        } else if (params.value(QStringLiteral("norm_rect")).isArray()) {
            rectArr = params.value(QStringLiteral("norm_rect")).toArray();
        } else if (params.contains(QStringLiteral("rel_x"))
                   || params.contains(QStringLiteral("rel_y"))
                   || params.contains(QStringLiteral("rel_w"))
                   || params.contains(QStringLiteral("rel_h"))) {
            rectArr.append(params.value(QStringLiteral("rel_x")).toDouble(0.54));
            rectArr.append(params.value(QStringLiteral("rel_y")).toDouble(0.08));
            rectArr.append(params.value(QStringLiteral("rel_w")).toDouble(0.38));
            rectArr.append(params.value(QStringLiteral("rel_h")).toDouble(0.38));
        }
        if (insetG && srcG && rectArr.size() >= 4) {
            const double rx = rectArr[0].toDouble();
            const double ry = rectArr[1].toDouble();
            const double rw = rectArr[2].toDouble();
            const double rh = rectArr[3].toDouble();
            if (rw > 0.0 && rh > 0.0 && rw <= 1.5 && rh <= 1.5) {
                QRect cr = srcG->geometry();
                if (srcG->plotWidget()) {
                    srcG->plotWidget()->updateLayout();
                    const QRect layoutCr =
                            srcG->plotWidget()->plotLayout()->canvasRect().translated(srcG->pos());
                    if (layoutCr.width() > 40 && layoutCr.height() > 40) {
                        cr = layoutCr;
                    } else if (srcG->plotWidget()->canvas() && srcG->parentWidget()) {
                        const QWidget *cv = srcG->plotWidget()->canvas();
                        const QPoint tl = cv->mapTo(srcG->parentWidget(), QPoint(0, 0));
                        if (cv->width() > 40 && cv->height() > 40)
                            cr = QRect(tl, cv->size());
                    }
                }
                if (cr.width() > 40 && cr.height() > 40) {
                    int iw = std::max(60, qRound(rw * double(cr.width())));
                    int ih = std::max(50, qRound(rh * double(cr.height())));
                    iw = std::min(iw, std::max(60, cr.width() - 12));
                    ih = std::min(ih, std::max(50, cr.height() - 12));
                    int ix = cr.left() + qRound(rx * double(cr.width()));
                    int iy = cr.top() + qRound(ry * double(cr.height()));
                    ix = qBound(cr.left() + 6, ix, std::max(cr.left() + 6, cr.right() - iw - 6));
                    iy = qBound(cr.top() + 6, iy, std::max(cr.top() + 6, cr.bottom() - ih - 6));
                    insetG->setGeometry(ix, iy, iw, ih);
                    if (insetG->plotWidget()) {
                        insetG->plotWidget()->resize(QSize(iw, ih));
                        insetG->plotWidget()->updateLayout();
                        insetG->plotWidget()->replot();
                    }
                    if (!insetG->isVisible()) {
                        QResizeEvent syncEv(insetG->size(), insetG->size());
                        QCoreApplication::sendEvent(insetG, &syncEv);
                    }
                    insetG->raise();
                }
            } else if (rw > 1.5 && rh > 1.5) {
                int ix = qRound(rx);
                int iy = qRound(ry);
                int iw = std::max(60, qRound(rw));
                int ih = std::max(50, qRound(rh));
                if (srcG->plotWidget()) {
                    srcG->plotWidget()->updateLayout();
                    const QRect cr =
                            srcG->plotWidget()->plotLayout()->canvasRect().translated(srcG->pos());
                    if (cr.width() > 60 && cr.height() > 60) {
                        iw = std::min(iw, std::max(60, cr.width() - 12));
                        ih = std::min(ih, std::max(50, cr.height() - 12));
                        ix = qBound(cr.left() + 6, ix, std::max(cr.left() + 6, cr.right() - iw - 6));
                        iy = qBound(cr.top() + 6, iy, std::max(cr.top() + 6, cr.bottom() - ih - 6));
                    }
                }
                insetG->setGeometry(ix, iy, iw, ih);
                if (insetG->plotWidget()) {
                    insetG->plotWidget()->resize(QSize(iw, ih));
                    insetG->plotWidget()->updateLayout();
                    insetG->plotWidget()->replot();
                }
                if (!insetG->isVisible()) {
                    QResizeEvent syncEv(insetG->size(), insetG->size());
                    QCoreApplication::sendEvent(insetG, &syncEv);
                }
                insetG->raise();
            }
        }
        const QList<Graph *> curLayers = ml->layersList();
        const int newIdx = insetG ? curLayers.indexOf(insetG) : std::max(0, ml->layers() - 1);
        const int srcIdx = srcG ? curLayers.indexOf(srcG) : 0;
        if (insetG && (!showConnectors || (rectArr.size() >= 4 && rectArr[2].toDouble() <= 1.5))) {
            // Sync MultiLayer's internal InsetSpec (showConnectors + normGeo) via restorePhase5Tag
            const QString savedMl = ml->saveToString(QString());
            for (const QString &rawLn : savedMl.split(QLatin1Char('\n'))) {
                const QString ln = rawLn.trimmed();
                if (ln.startsWith(QStringLiteral("<ToriInset\t"))) {
                    QStringList parts = ln.split(QLatin1Char('\t'));
                    if (parts.size() >= 8 && parts[1].trimmed().toInt() == newIdx) {
                        parts[2] = QString::number(std::max(0, srcIdx));
                        if (rectArr.size() >= 4 && rectArr[2].toDouble() > 0.0
                            && rectArr[2].toDouble() <= 1.5) {
                            parts[3] = QString::number(rectArr[0].toDouble(), 'g', 8);
                            parts[4] = QString::number(rectArr[1].toDouble(), 'g', 8);
                            parts[5] = QString::number(rectArr[2].toDouble(), 'g', 8);
                            parts[6] = QString::number(rectArr[3].toDouble(), 'g', 8);
                        }
                        QString p7 = parts[7].trimmed();
                        const bool hasClosing = p7.endsWith(QLatin1String("/>"))
                                || p7.endsWith(QLatin1Char('>'));
                        parts[7] = QString::number(showConnectors ? 1 : 0)
                                + ((parts.size() == 8 && hasClosing) ? QStringLiteral("/>")
                                                                     : QString());
                        ml->restorePhase5Tag(parts.join(QLatin1Char('\t')));
                        ml->finalizePhase5Restore();
                        break;
                    }
                }
            }
        }
        // Keep activeGraph() on the source parent layer so subsequent add_reference_line / fit_* target layer 0
        if (srcG)
            ml->setActiveGraph(srcG);
        enforceArialGraphFonts(insetG);
        if (insetG)
            undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("inset_layer")] = newIdx;
        outResult[QStringLiteral("layer")] = newIdx;
        outResult[QStringLiteral("layer_index")] = newIdx;
        outResult[QStringLiteral("show_connectors")] = showConnectors;
        outResult[QStringLiteral("ok")] = (insetG != nullptr);
        return true;
    }

    if (method == QStringLiteral("graph.sync_axis_color")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params, true);
        const int ax = parseAxisId(params.contains(QStringLiteral("axis"))
                                           ? params.value(QStringLiteral("axis")).toString()
                                           : QStringLiteral("right"));
        if (optionalJsonBool(params, QStringLiteral("reset"), false)) {
            ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Reset Axis Color"));
            const QColor blk(Qt::black);
            if (ax == QwtPlot::yRight)
                g->setRightAxisTitleColor(blk);
            else if (ax == QwtPlot::yLeft)
                g->setYAxisTitleColor(blk);
            else if (ax == QwtPlot::xTop)
                g->setTopAxisTitleColor(blk);
            else
                g->setXAxisTitleColor(blk);
            QStringList axCols = g->axesColors();
            while (axCols.size() < 4)
                axCols.append(QStringLiteral("#000000"));
            axCols[ax] = QStringLiteral("#000000");
            g->setAxesColors(axCols);
            QStringList numCols = g->axesNumColors();
            while (numCols.size() < 4)
                numCols.append(QStringLiteral("#000000"));
            numCols[ax] = QStringLiteral("#000000");
            g->setAxesNumColors(numCols);
            replotGraphOrDefer(g);
            undoGuard.commit();
            outResult = serializeAxisInfo(g, ax);
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("color")] = QStringLiteral("#000000");
            return true;
        }
        int cIdx = -1;
        if (params.contains(QStringLiteral("curve")) && !params.value(QStringLiteral("curve")).isNull()) {
            const QJsonValue cv = params.value(QStringLiteral("curve"));
            if (cv.isDouble()) {
                cIdx = parseStrictJsonInt(cv, QStringLiteral("curve"));
            } else {
                const QString ct = resolveCurveTitle(g, params, QStringLiteral("curve"), true);
                cIdx = g->curveIndex(ct);
            }
        } else {
            for (int ci = 0; ci < g->curves(); ++ci) {
                QwtPlotCurve *cand = g->curve(ci);
                if (!cand || g->curveType(ci) == Graph::ErrorBars)
                    continue;
                if ((ax == QwtPlot::yRight && cand->yAxis() == QwtPlot::yRight)
                    || (ax == QwtPlot::yLeft && cand->yAxis() == QwtPlot::yLeft)
                    || (ax == QwtPlot::xTop && cand->xAxis() == QwtPlot::xTop)
                    || (ax == QwtPlot::xBottom && cand->xAxis() == QwtPlot::xBottom)) {
                    cIdx = ci;
                    break;
                }
            }
            if (cIdx < 0 && g->curves() > 0)
                cIdx = 0;
        }
        if (cIdx < 0 || cIdx >= g->curves() || !g->curve(cIdx))
            throwInvalidParams(QStringLiteral("Valid curve index required for graph.sync_axis_color."));

        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Sync Axis Color to Curve"));
        g->syncAxisColorToCurve(ax, cIdx);
        for (Graph *cand : ml->layersList()) {
            if (cand && cand->linkedParentLayer() == g && cand->plotWidget()
                && cand->plotWidget()->axisEnabled(ax)) {
                QColor col = g->axisColor(ax);
                if (col.isValid()) {
                    if (ax == QwtPlot::yRight)
                        cand->setRightAxisTitleColor(col);
                    else if (ax == QwtPlot::yLeft)
                        cand->setYAxisTitleColor(col);
                    else if (ax == QwtPlot::xTop)
                        cand->setTopAxisTitleColor(col);
                    else
                        cand->setXAxisTitleColor(col);
                    QStringList axCols = cand->axesColors();
                    while (axCols.size() < 4)
                        axCols.append(QStringLiteral("#000000"));
                    axCols[ax] = col.name();
                    cand->setAxesColors(axCols);
                    QStringList numCols = cand->axesNumColors();
                    while (numCols.size() < 4)
                        numCols.append(QStringLiteral("#000000"));
                    numCols[ax] = col.name();
                    cand->setAxesNumColors(numCols);
                    cand->replot();
                }
            }
        }
        g->replot();
        undoGuard.commit();
        outResult = serializeAxisInfo(g, ax);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("color")] = g->axisColor(ax).name(QColor::HexRgb);
        return true;
    }

    if (method == QStringLiteral("graph.step_zoom_history")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params, true);
        int delta = -1;
        if (params.contains(QStringLiteral("delta")) && !params.value(QStringLiteral("delta")).isNull()) {
            delta = parseBoundedInt(params, QStringLiteral("delta"), -1, -100, 100);
        } else if (params.contains(QStringLiteral("direction"))
                   && !params.value(QStringLiteral("direction")).isNull()) {
            const QJsonValue dv = params.value(QStringLiteral("direction"));
            if (dv.isDouble()) {
                delta = parseBoundedInt(params, QStringLiteral("direction"), -1, -100, 100);
            } else {
                const QString ds = dv.toString().trimmed().toLower();
                if (ds == QStringLiteral("forward") || ds == QStringLiteral("redo")
                    || ds == QStringLiteral("next") || ds == QStringLiteral("+1")) {
                    delta = 1;
                } else if (ds == QStringLiteral("back") || ds == QStringLiteral("backward")
                           || ds == QStringLiteral("undo") || ds == QStringLiteral("prev")
                           || ds == QStringLiteral("-1")) {
                    delta = -1;
                } else {
                    throwInvalidParams(
                            QStringLiteral("Unknown zoom history direction '%1' (expected 'back'|'forward' or integer delta).")
                                    .arg(ds));
                }
            }
        }
        const bool stepped = g->stepZoomHistory(delta);
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = stepped;
        outResult[QStringLiteral("stepped")] = stepped;
        return true;
    }

    if (method == QStringLiteral("graph.merge_graphs")) {
        if (!params.value(QStringLiteral("graphs")).isArray())
            throwInvalidParams(QStringLiteral("'graphs' array of graph window names is required."));
        QList<MultiLayer *> chosenGraphs;
        for (const QJsonValue &gv : params.value(QStringLiteral("graphs")).toArray()) {
            QJsonObject subP;
            subP[QStringLiteral("graph")] = gv;
            MultiLayer *srcMl = resolveMultiLayer(d_app, subP, QStringLiteral("graph"), true);
            if (srcMl && !srcMl->isEmpty())
                chosenGraphs.append(srcMl);
        }
        if (chosenGraphs.size() < 2)
            throwInvalidParams(QStringLiteral("At least two non-empty graph windows are required."));
        if (chosenGraphs.size() > 64)
            throwInvalidParams(QStringLiteral("graph.merge_graphs supports at most 64 source graphs."));

        int cols = parseBoundedInt(params, QStringLiteral("cols"), 0, 0, 32);
        int rows = parseBoundedInt(params, QStringLiteral("rows"), 0, 0, 32);
        if (cols <= 0 || rows <= 0) {
            const int n = chosenGraphs.size();
            if (n == 2) {
                rows = 1;
                cols = 2;
            } else if (n == 3) {
                rows = 1;
                cols = 3;
            } else if (n == 4) {
                rows = 2;
                cols = 2;
            } else {
                cols = std::max(2, int(std::ceil(std::sqrt(double(n)))));
                rows = (n + cols - 1) / cols;
            }
        }

        const int savedFileVer = d_app->d_file_version;
        d_app->d_file_version = SciDAVis::version();
        MultiLayer *merged =
                d_app->multilayerPlot(d_app->generateUniqueName(QStringLiteral("MergedGraph")));
        if (!merged) {
            d_app->d_file_version = savedFileVer;
            throwRpcRuntime(QStringLiteral("Failed to allocate merged MultiLayer window."));
        }

        merged->blockSignals(true);
        int primaryPanelCount = 0;
        for (MultiLayer *srcMl : chosenGraphs) {
            const int baseLayerIdx = merged->layers();
            const QString mlSerialized = srcMl->saveToString("");
            QTextStream ts(const_cast<QString *>(&mlSerialized), QIODevice::ReadOnly);
            QString s;
            while (!ts.atEnd()) {
                s = ts.readLine();
                if (s == QStringLiteral("</multiLayer>"))
                    break;
                if (s.startsWith(QStringLiteral("<graph>"))) {
                    QStringList gLines;
                    bool isChildLinked = false;
                    while (!ts.atEnd()) {
                        s = ts.readLine();
                        const QString trimmedTag = s.trimmed();
                        if (trimmedTag.startsWith(QStringLiteral("<ToriLinkedLayer"))) {
                            isChildLinked = true;
                            static const QRegularExpression rxParentAttr(
                                    QStringLiteral("parentIdx=\"(-?\\d+)\""));
                            const QRegularExpressionMatch pm = rxParentAttr.match(s);
                            if (pm.hasMatch()) {
                                const int oldP = pm.captured(1).toInt();
                                const int newP = (oldP >= 0) ? (oldP + baseLayerIdx) : oldP;
                                s.replace(pm.capturedStart(0), pm.capturedLength(0),
                                          QStringLiteral("parentIdx=\"%1\"").arg(newP));
                            }
                        } else if (trimmedTag.startsWith(QStringLiteral("<ToriLinkedParent\t"))) {
                            isChildLinked = true;
                            QStringList parts = trimmedTag.split(QLatin1Char('\t'));
                            if (parts.size() >= 2) {
                                parts[1] = QString::number(parts[1].toInt() + baseLayerIdx);
                                s = parts.join(QLatin1Char('\t'));
                            }
                        }
                        gLines << s;
                        if (s == QStringLiteral("</graph>"))
                            break;
                    }
                    d_app->openGraph(d_app, merged, gLines);
                    if (!isChildLinked)
                        ++primaryPanelCount;
                } else if (s.trimmed().startsWith(QStringLiteral("<ToriInset\t"))) {
                    QStringList parts = s.trimmed().split(QLatin1Char('\t'));
                    if (parts.size() >= 3) {
                        parts[1] = QString::number(parts[1].toInt() + baseLayerIdx);
                        parts[2] = QString::number(parts[2].toInt() + baseLayerIdx);
                        merged->restorePhase5Tag(parts.join(QLatin1Char('\t')));
                        if (primaryPanelCount > 0)
                            --primaryPanelCount;
                    }
                }
            }
        }
        merged->finalizePhase5Restore();
        merged->blockSignals(false);
        d_app->d_file_version = savedFileVer;

        if (rows * cols < primaryPanelCount)
            rows = (primaryPanelCount + cols - 1) / cols;
        merged->setCols(cols);
        merged->setRows(rows);
        merged->setMargins(10, 10, 10, 10);
        merged->setSpacing(12, 14);

        const QString rawPreset = params.value(QStringLiteral("preset")).toString().trimmed();
        const QString preset = rawPreset.toLower();
        if (preset == QStringLiteral("isij_175mm")) {
            merged->applyStylePreset(MultiLayer::StyleISIJJournal85mm);
            const double hMm = std::clamp(175.0 * double(rows) / double(cols), 60.0, 260.0);
            merged->setPageSizePhysical(175.0, hMm, MultiLayer::UnitMm, 600);
            merged->setPageViewMode(true);
        } else if (!rawPreset.isEmpty()) {
            MultiLayer::GraphStylePresetSpec spec;
            if (resolveStylePresetForRpc(rawPreset, &spec, true))
                applyResolvedStylePreset(merged, spec);
        }

        const double wMm = optionalJsonFiniteDouble(params, QStringLiteral("width_mm"), 0.0, 5000.0);
        const double hMm = optionalJsonFiniteDouble(params, QStringLiteral("height_mm"), 0.0, 5000.0);
        const int dpi = parseBoundedInt(params, QStringLiteral("dpi"), 300, 36, 2400);
        if (wMm > 0.0 || hMm > 0.0) {
            const double effW = (wMm > 0.0) ? wMm : 175.0;
            const double effH = (hMm > 0.0)
                    ? hMm
                    : std::clamp(effW * double(rows) / double(cols), 60.0, 260.0);
            merged->setPageSizePhysical(effW, effH, MultiLayer::UnitMm, dpi);
            merged->setPageViewMode(true);
        } else if (!merged->pageViewMode()) {
            merged->resize(std::clamp(420 * cols, 560, 1280), std::clamp(360 * rows, 420, 960));
        }

        if (params.value(QStringLiteral("hide_inner_axes")).toBool(false)) {
            QList<Graph *> primaryPanels;
            for (Graph *lg : merged->layersList()) {
                if (lg && !lg->linkedParentLayer())
                    primaryPanels.append(lg);
            }
            for (int idx = 0; idx < primaryPanels.size(); ++idx) {
                Graph *lg = primaryPanels[idx];
                const int c = idx % cols;
                if (c > 0 && !lg->hasColorScaleOnAxis(QwtPlot::yLeft)) {
                    QStringList tl = lg->enabledTickLabels();
                    if (tl.size() == 4) {
                        tl[QwtPlot::yLeft] = QStringLiteral("0");
                        lg->setEnabledTickLabels(tl);
                    }
                    lg->setAxisTitleAuto(QwtPlot::yLeft, false);
                    lg->setAxisTitle(QwtPlot::yLeft, QString());
                }
                if ((idx + cols) < primaryPanels.size()) {
                    QStringList tl = lg->enabledTickLabels();
                    if (tl.size() == 4) {
                        tl[QwtPlot::xBottom] = QStringLiteral("0");
                        lg->setEnabledTickLabels(tl);
                    }
                    lg->setAxisTitleAuto(QwtPlot::xBottom, false);
                    lg->setAxisTitle(QwtPlot::xBottom, QString());
                }
            }
        }

        const bool panelLabels = params.value(QStringLiteral("panel_labels")).toBool(true);
        const QString plPlaceStr =
                params.value(QStringLiteral("panel_placement")).toString().trimmed().toLower();
        const MultiLayer::PanelLabelPlacement plPlace =
                (plPlaceStr == QStringLiteral("inside")
                 || plPlaceStr == QStringLiteral("inside_top_left"))
                ? MultiLayer::PanelInsideTopLeft
                : MultiLayer::PanelOutsideTopLeft;
        merged->setPanelLabels(panelLabels, plPlace, QStringLiteral("(%a)"), 0);

        for (Graph *lg : merged->layersList()) {
            if (!lg)
                continue;
            enforceArialGraphFonts(lg);
            lg->updateStatReferenceLines();
            lg->syncChildLinkedLayers();
            lg->replot();
        }
        if (merged->pageViewMode())
            merged->fitLayersToPage();
        else
            merged->arrangeLayers(false, false);

        applyRequestedWindowName(d_app, merged, params);
        if (optionalJsonBool(params, QStringLiteral("close_sources"), false)) {
            for (MultiLayer *srcMl : chosenGraphs) {
                if (srcMl && srcMl != merged) {
                    srcMl->askOnCloseEvent(false);
                    srcMl->hide();
                    srcMl->close();
                    if (srcMl->folder())
                        d_app->closeWindow(srcMl);
                }
            }
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        outResult = serializeGraphInfo(merged);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("primary_panels")] = primaryPanelCount;
        return true;
    }

    if (method == QStringLiteral("graph.arrange_layers")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Arrange Layers"));
        const bool hasExplicitRows = params.contains(QStringLiteral("rows"))
                && !params.value(QStringLiteral("rows")).isNull();
        const bool hasExplicitCols = params.contains(QStringLiteral("cols"))
                && !params.value(QStringLiteral("cols")).isNull();
        if (hasExplicitRows) {
            const int r = parseBoundedInt(params, QStringLiteral("rows"), ml->getRows(), 1, 32);
            ml->setRows(r);
        }
        if (hasExplicitCols) {
            const int c = parseBoundedInt(params, QStringLiteral("cols"), ml->getCols(), 1, 32);
            ml->setCols(c);
        }
        const bool hasRowSpace = params.contains(QStringLiteral("row_spacing"))
                || params.contains(QStringLiteral("row_gap"));
        const bool hasColSpace = params.contains(QStringLiteral("col_spacing"))
                || params.contains(QStringLiteral("col_gap"));
        if (hasRowSpace || hasColSpace) {
            const int rSp = params.contains(QStringLiteral("row_spacing"))
                    ? parseBoundedInt(params, QStringLiteral("row_spacing"), ml->rowsSpacing(), 0, 500)
                    : parseBoundedInt(params, QStringLiteral("row_gap"), ml->rowsSpacing(), 0, 500);
            const int cSp = params.contains(QStringLiteral("col_spacing"))
                    ? parseBoundedInt(params, QStringLiteral("col_spacing"), ml->colsSpacing(), 0, 500)
                    : parseBoundedInt(params, QStringLiteral("col_gap"), ml->colsSpacing(), 0, 500);
            ml->setSpacing(rSp, cSp);
        }
        if (params.contains(QStringLiteral("margin"))
            || params.contains(QStringLiteral("left_margin"))
            || params.contains(QStringLiteral("right_margin"))
            || params.contains(QStringLiteral("top_margin"))
            || params.contains(QStringLiteral("bottom_margin"))) {
            const int uniM = params.contains(QStringLiteral("margin"))
                    ? parseBoundedInt(params, QStringLiteral("margin"), 5, 0, 500)
                    : -1;
            const int lm = params.contains(QStringLiteral("left_margin"))
                    ? parseBoundedInt(params, QStringLiteral("left_margin"), ml->leftMargin(), 0, 500)
                    : (uniM >= 0 ? uniM : ml->leftMargin());
            const int rm = params.contains(QStringLiteral("right_margin"))
                    ? parseBoundedInt(params, QStringLiteral("right_margin"), ml->rightMargin(), 0, 500)
                    : (uniM >= 0 ? uniM : ml->rightMargin());
            const int tm = params.contains(QStringLiteral("top_margin"))
                    ? parseBoundedInt(params, QStringLiteral("top_margin"), ml->topMargin(), 0, 500)
                    : (uniM >= 0 ? uniM : ml->topMargin());
            const int bm = params.contains(QStringLiteral("bottom_margin"))
                    ? parseBoundedInt(params, QStringLiteral("bottom_margin"), ml->bottomMargin(), 0, 500)
                    : (uniM >= 0 ? uniM : ml->bottomMargin());
            ml->setMargins(lm, rm, tm, bm);
        }
        const bool shareX = optionalJsonBool(params, QStringLiteral("share_x"), false)
                || optionalJsonBool(params, QStringLiteral("hide_inner_axes"), false);
        const bool shareY = optionalJsonBool(params, QStringLiteral("share_y"), false)
                || optionalJsonBool(params, QStringLiteral("hide_inner_axes"), false);
        if (shareX || shareY) {
            const int cols = std::max(1, ml->getCols());
            QList<Graph *> primaryPanels;
            for (Graph *lg : ml->layersList()) {
                if (lg && !lg->linkedParentLayer() && !ml->isInsetLayer(lg))
                    primaryPanels.append(lg);
            }
            for (int idx = 0; idx < primaryPanels.size(); ++idx) {
                Graph *lg = primaryPanels[idx];
                const int c = idx % cols;
                if (shareY && c > 0 && !lg->hasColorScaleOnAxis(QwtPlot::yLeft)) {
                    lg->setTickLabelsOnAxis(QwtPlot::yLeft, false);
                    lg->setAxisTitleAuto(QwtPlot::yLeft, false);
                    lg->setAxisTitle(QwtPlot::yLeft, QString());
                }
                if (shareX && (idx + cols) < primaryPanels.size()) {
                    lg->setTickLabelsOnAxis(QwtPlot::xBottom, false);
                    lg->setAxisTitleAuto(QwtPlot::xBottom, false);
                    lg->setAxisTitle(QwtPlot::xBottom, QString());
                }
            }
        }
        // Default fit=false when explicit rows/cols are passed so MultiLayer::arrangeLayers preserves them
        const bool defaultFit = !(hasExplicitRows || hasExplicitCols);
        const bool fitGrid = params.contains(QStringLiteral("fit"))
                ? optionalJsonBool(params, QStringLiteral("fit"), false)
                : (defaultFit ? false : false);
        ml->arrangeLayers(fitGrid && !(hasExplicitRows || hasExplicitCols), false);
        if (ml->pageViewMode() && !fitGrid)
            ml->fitLayersToPage();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_page_size")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const double w = optionalJsonFiniteDouble(params, QStringLiteral("width"),
                                                  ml->pageWidthPhysical(), 10000.0);
        const double h = optionalJsonFiniteDouble(params, QStringLiteral("height"),
                                                  ml->pageHeightPhysical(), 10000.0);
        if (w <= 0.0 || h <= 0.0)
            throwInvalidParams(QStringLiteral("Page 'width' and 'height' must be > 0."));
        const MultiLayer::PageUnit unit = params.contains(QStringLiteral("unit"))
                ? parsePageUnit(optionalJsonString(params, QStringLiteral("unit")))
                : ml->pageUnit();
        const int dpi = parseBoundedInt(params, QStringLiteral("dpi"),
                                        std::clamp(ml->pageDpi(), 36, 2400), 36, 2400);
        const double pxPerUnit = (unit == MultiLayer::UnitInch)
                ? double(dpi)
                : ((unit == MultiLayer::UnitMm)
                           ? (double(dpi) / 25.4)
                           : ((unit == MultiLayer::UnitCm) ? (double(dpi) / 2.54) : 1.0));
        if (w * pxPerUnit > 16384.0 || h * pxPerUnit > 16384.0) {
            throwInvalidParams(
                    QStringLiteral("Page pixel dimensions exceed maximum 16,384 x 16,384 px at %1 DPI.")
                            .arg(dpi));
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Page Setup"));
        ml->setPageSizePhysical(w, h, unit, dpi);
        if (params.contains(QStringLiteral("background_color"))
            || params.contains(QStringLiteral("bg_color"))) {
            const QJsonValue bgV = params.contains(QStringLiteral("background_color"))
                    ? params.value(QStringLiteral("background_color"))
                    : params.value(QStringLiteral("bg_color"));
            ml->setPageBackgroundColor(parseColorValue(bgV, ml->pageBackgroundColor()));
        }
        if (params.contains(QStringLiteral("page_view")))
            ml->setPageViewMode(optionalJsonBool(params, QStringLiteral("page_view"), true));
        if (params.contains(QStringLiteral("zoom")))
            ml->setPageZoom(std::clamp(optionalJsonFiniteDouble(params, QStringLiteral("zoom"), 1.0, 20.0),
                                       0.05, 10.0));
        if (optionalJsonBool(params, QStringLiteral("fit_layers"), true))
            ml->fitLayersToPage();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.fit_page_to_layers")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const int margin = std::max(0, params.value(QStringLiteral("margin_px")).toInt(14));
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Fit Page to Layers"));
        ml->fitPageToLayers(margin);
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.fit_layers_to_page")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Fit Layers to Page"));
        ml->fitLayersToPage();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("app.list_style_presets")) {
        const QVector<MultiLayer::GraphStylePresetSpec> presets = MultiLayer::listStylePresets();
        QJsonArray arr;
        for (const auto &s : presets) {
            QJsonObject o;
            o[QStringLiteral("id")] = s.id;
            o[QStringLiteral("name")] = s.name;
            o[QStringLiteral("built_in")] = s.builtIn;
            o[QStringLiteral("page_width_mm")] = s.pageWidthMm;
            o[QStringLiteral("page_height_mm")] = s.pageHeightMm;
            o[QStringLiteral("keep_aspect_ratio")] = s.keepAspectRatio;
            o[QStringLiteral("target_dpi")] = s.targetDpi;
            o[QStringLiteral("margin_px")] = s.marginPx;
            o[QStringLiteral("frame_preset")] = s.framePreset;
            o[QStringLiteral("axis_line_width")] = s.axisLineWidth;
            o[QStringLiteral("minor_tick_len")] = s.minorTickLen;
            o[QStringLiteral("major_tick_len")] = s.majorTickLen;
            o[QStringLiteral("font_family")] = s.fontFamily;
            o[QStringLiteral("axis_title_pt")] = s.axisTitlePt;
            o[QStringLiteral("tick_label_pt")] = s.tickLabelPt;
            o[QStringLiteral("legend_pt")] = s.legendPt;
            o[QStringLiteral("bold_title")] = s.boldTitle;
            o[QStringLiteral("bold_tick")] = s.boldTick;
            o[QStringLiteral("bold_legend")] = s.boldLegend;
            const double effCw = (s.curveWidthF > 0.0) ? s.curveWidthF : double(s.minCurveWidth);
            o[QStringLiteral("curve_width")] = effCw;
            o[QStringLiteral("min_curve_width")] = qBound(1, qRound(effCw), 20);
            o[QStringLiteral("symbol_size")] = s.minSymbolSize;
            o[QStringLiteral("min_symbol_size")] = s.minSymbolSize;
            o[QStringLiteral("legend_frame")] = s.legendFrame;
            o[QStringLiteral("format_isij_unit_slash")] = s.formatIsijUnitSlash;
            o[QStringLiteral("large_slide_layout")] = s.largeSlideLayout;
            if (s.slidePastePercent > 0.0)
                o[QStringLiteral("slide_paste_percent")] = s.slidePastePercent;
            arr.append(o);
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("count")] = arr.size();
        outResult[QStringLiteral("file")] = MultiLayer::userStylePresetsFilePath();
        outResult[QStringLiteral("presets")] = arr;
        return true;
    }

    if (method == QStringLiteral("app.save_style_preset")) {
        MultiLayer::GraphStylePresetSpec spec;
        const QString reqId = optionalJsonString(params, QStringLiteral("id")).trimmed();
        if (!reqId.isEmpty())
            MultiLayer::findStylePreset(reqId, &spec);
        if (!reqId.isEmpty())
            spec.id = reqId;
        const QString reqName = optionalJsonString(params, QStringLiteral("name")).trimmed();
        if (!reqName.isEmpty())
            spec.name = reqName;
        if (params.contains(QStringLiteral("page_width_mm")))
            spec.pageWidthMm = params.value(QStringLiteral("page_width_mm")).toDouble(spec.pageWidthMm);
        if (params.contains(QStringLiteral("page_height_mm")))
            spec.pageHeightMm = params.value(QStringLiteral("page_height_mm")).toDouble(spec.pageHeightMm);
        if (params.contains(QStringLiteral("keep_aspect_ratio")))
            spec.keepAspectRatio = params.value(QStringLiteral("keep_aspect_ratio")).toBool(spec.keepAspectRatio);
        if (params.contains(QStringLiteral("target_dpi")))
            spec.targetDpi = params.value(QStringLiteral("target_dpi")).toInt(spec.targetDpi);
        if (params.contains(QStringLiteral("margin_px")))
            spec.marginPx = params.value(QStringLiteral("margin_px")).toInt(spec.marginPx);
        if (params.contains(QStringLiteral("frame_preset")))
            spec.framePreset = params.value(QStringLiteral("frame_preset")).toInt(spec.framePreset);
        if (params.contains(QStringLiteral("axis_line_width")))
            spec.axisLineWidth = params.value(QStringLiteral("axis_line_width")).toInt(spec.axisLineWidth);
        if (params.contains(QStringLiteral("minor_tick_len")))
            spec.minorTickLen = params.value(QStringLiteral("minor_tick_len")).toInt(spec.minorTickLen);
        if (params.contains(QStringLiteral("major_tick_len")))
            spec.majorTickLen = params.value(QStringLiteral("major_tick_len")).toInt(spec.majorTickLen);
        if (params.contains(QStringLiteral("font_family")))
            spec.fontFamily = params.value(QStringLiteral("font_family")).toString(spec.fontFamily);
        if (params.contains(QStringLiteral("axis_title_pt")))
            spec.axisTitlePt = params.value(QStringLiteral("axis_title_pt")).toDouble(spec.axisTitlePt);
        if (params.contains(QStringLiteral("tick_label_pt")))
            spec.tickLabelPt = params.value(QStringLiteral("tick_label_pt")).toDouble(spec.tickLabelPt);
        if (params.contains(QStringLiteral("legend_pt")))
            spec.legendPt = params.value(QStringLiteral("legend_pt")).toDouble(spec.legendPt);
        if (params.contains(QStringLiteral("bold_title")))
            spec.boldTitle = params.value(QStringLiteral("bold_title")).toBool(spec.boldTitle);
        if (params.contains(QStringLiteral("bold_tick")))
            spec.boldTick = params.value(QStringLiteral("bold_tick")).toBool(spec.boldTick);
        if (params.contains(QStringLiteral("bold_legend")))
            spec.boldLegend = params.value(QStringLiteral("bold_legend")).toBool(spec.boldLegend);
        if (params.contains(QStringLiteral("curve_width"))) {
            const double defCw = (spec.curveWidthF > 0.0) ? spec.curveWidthF
                                                          : double(spec.minCurveWidth);
            const double cw = qBound(
                    0.5, params.value(QStringLiteral("curve_width")).toDouble(defCw), 20.0);
            spec.curveWidthF = cw;
            spec.minCurveWidth = qBound(1, qRound(cw), 20);
        } else if (params.contains(QStringLiteral("min_curve_width"))) {
            const double defCw = (spec.curveWidthF > 0.0) ? spec.curveWidthF
                                                          : double(spec.minCurveWidth);
            const double cw = qBound(
                    0.5, params.value(QStringLiteral("min_curve_width")).toDouble(defCw), 20.0);
            spec.curveWidthF = cw;
            spec.minCurveWidth = qBound(1, qRound(cw), 20);
        }
        if (params.contains(QStringLiteral("symbol_size"))) {
            spec.minSymbolSize = qBound(
                    0, params.value(QStringLiteral("symbol_size")).toInt(spec.minSymbolSize), 100);
        } else if (params.contains(QStringLiteral("min_symbol_size"))) {
            spec.minSymbolSize = qBound(
                    0, params.value(QStringLiteral("min_symbol_size")).toInt(spec.minSymbolSize),
                    100);
        }
        if (params.contains(QStringLiteral("legend_frame")))
            spec.legendFrame = params.value(QStringLiteral("legend_frame")).toInt(spec.legendFrame);
        if (params.contains(QStringLiteral("format_isij_unit_slash")))
            spec.formatIsijUnitSlash = params.value(QStringLiteral("format_isij_unit_slash")).toBool(spec.formatIsijUnitSlash);
        if (params.contains(QStringLiteral("large_slide_layout")))
            spec.largeSlideLayout = params.value(QStringLiteral("large_slide_layout")).toBool(spec.largeSlideLayout);
        if (params.contains(QStringLiteral("slide_paste_percent"))) {
            spec.slidePastePercent = qBound(
                    10.0,
                    params.value(QStringLiteral("slide_paste_percent"))
                            .toDouble(spec.slidePastePercent > 0.0 ? spec.slidePastePercent : 100.0),
                    400.0);
        }

        QString err;
        if (!MultiLayer::saveStylePreset(spec, &err))
            throwInvalidParams(err.isEmpty() ? QStringLiteral("Failed to save style preset.") : err);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("id")] = spec.id;
        outResult[QStringLiteral("name")] = spec.name;
        return true;
    }

    if (method == QStringLiteral("app.delete_style_preset")) {
        const QString reqId = optionalJsonString(params, QStringLiteral("id")).trimmed();
        if (reqId.isEmpty())
            throwInvalidParams(QStringLiteral("Missing 'id' parameter."));
        const bool resetOnly = optionalJsonBool(params, QStringLiteral("reset"), false);
        QString err;
        const bool ok = resetOnly ? MultiLayer::resetBuiltInStylePreset(reqId, &err)
                                  : MultiLayer::deleteStylePreset(reqId, &err);
        if (!ok)
            throwInvalidParams(err.isEmpty() ? QStringLiteral("Style preset '%1' not found.").arg(reqId) : err);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("id")] = reqId;
        return true;
    }

    if (method == QStringLiteral("graph.apply_style_preset")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        const QString rawP = params.value(QStringLiteral("preset")).toString().trimmed();
        MultiLayer::GraphStylePresetSpec spec;
        if (!resolveStylePresetForRpc(rawP, &spec, true)) {
            QStringList avail;
            for (const auto &s : MultiLayer::listStylePresets())
                avail << s.id;
            throwInvalidParamsWithAvailable(
                    QStringLiteral("Style preset 'preset' is required."), avail);
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Apply Style Preset"));
        applyResolvedStylePreset(ml, spec);
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("preset")] = spec.id;
        return true;
    }

    if (method == QStringLiteral("graph.apply_frame_preset")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        Graph *g = resolveGraphLayer(ml, params);
        const QString p = optionalJsonString(params, QStringLiteral("preset")).trimmed().toLower();
        Graph::FramePreset framePreset = Graph::FrameBoxInwardTicks;
        if (p == QStringLiteral("box_inward") || p == QStringLiteral("inward")
            || p == QStringLiteral("box"))
            framePreset = Graph::FrameBoxInwardTicks;
        else if (p == QStringLiteral("box_plain") || p == QStringLiteral("plain"))
            framePreset = Graph::FrameBoxPlainOpposite;
        else if (p == QStringLiteral("open_l") || p == QStringLiteral("l")
                 || p == QStringLiteral("top_right_off"))
            framePreset = Graph::FrameOpenL;
        else if (p == QStringLiteral("zero_crossed") || p == QStringLiteral("origin"))
            framePreset = Graph::FrameZeroCrossed;
        else
            throwInvalidParams(
                    QStringLiteral("Unknown frame preset '%1' (expected 'box_inward'|'box'|'box_plain'|'open_l'|'top_right_off'|'zero_crossed').")
                            .arg(p));
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Apply Frame Preset"));
        g->applyFramePreset(framePreset);
        enforceArialGraphFonts(g);
        g->replot();
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.set_aspect_ratio")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        double ratio = 1.0;
        if (params.contains(QStringLiteral("ratio"))
            && !params.value(QStringLiteral("ratio")).isNull()) {
            const QJsonValue rv = params.value(QStringLiteral("ratio"));
            if (!rv.isDouble()) {
                throwInvalidParams(
                        QStringLiteral("Parameter 'ratio' must be a positive finite number."));
            }
            ratio = rv.toDouble();
            if (!std::isfinite(ratio) || ratio <= 0.0) {
                throwInvalidParams(
                        QStringLiteral("Parameter 'ratio' must be finite and > 0, got %1.")
                                .arg(ratio));
            }
        }
        const bool keep = params.contains(QStringLiteral("keep"))
                ? params.value(QStringLiteral("keep")).toBool(true)
                : (params.contains(QStringLiteral("enabled"))
                           ? params.value(QStringLiteral("enabled")).toBool(true)
                           : true);
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Aspect Ratio"));
        if (keep) {
            if (ratio > 0.0)
                ml->applyAspectRatioPreset(ratio, true);
            ml->setKeepAspectRatio(true);
        } else {
            if (ratio > 0.0)
                ml->setTargetAspectRatio(ratio);
            ml->setKeepAspectRatio(false);
        }
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.copy_format")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        d_app->activateSubWindow(ml);
        d_app->copyGraphFormat();

        HeadlessFormatClipboard &clip = rpcFormatClipboard();
        clip = HeadlessFormatClipboard();
        clip.valid = true;
        clip.srcLayerCount = ml->layers();
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
                if (clip.legendFont.family().isEmpty())
                    clip.legendFont.setFamily(QStringLiteral("Arial"));
                clip.legendTextColor = leg->textColor();
                clip.legendBgColor = leg->backgroundColor();
                clip.legendFrameStyle = leg->frameStyle();
                clip.legendNumColumns = leg->numColumns();
                if (g->plotWidget() && g->plotWidget()->canvas()) {
                    const QRect cr = g->plotWidget()->canvas()->contentsRect();
                    if (cr.width() > 4 && cr.height() > 4) {
                        const QPoint tl = leg->rect().topLeft();
                        clip.hasLegendNormPos = true;
                        clip.legendNormX = double(tl.x() - cr.left()) / double(cr.width());
                        clip.legendNormY = double(tl.y() - cr.top()) / double(cr.height());
                    }
                }
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
            for (int ci = 0; ci < g->curves(); ++ci) {
                QwtPlotCurve *c = g->curve(ci);
                if (!c || c->rtti() == QwtPlotItem::Rtti_PlotSpectrogram
                    || g->curveType(ci) == Graph::ErrorBars)
                    continue;
                HeadlessFormatClipboard::CurveFmt cf;
                cf.pen = c->pen();
                cf.brush = c->brush();
                cf.curveStyle = c->style();
                cf.symbol = c->symbol();
                cf.symbolHollow = Graph::isSymbolWhiteHollow(cf.symbol);
                clip.curves.append(cf);
            }
        }
        outResult[QStringLiteral("ok")] = true;
        outResult[QStringLiteral("source")] = ml->name();
        return true;
    }

    if (method == QStringLiteral("graph.paste_format")) {
        MultiLayer *ml = resolveMultiLayer(d_app, params);
        int scopeFlags = ApplicationWindow::FormatDefaultSafe;
        if (params.value(QStringLiteral("scopes")).isArray()) {
            scopeFlags = 0;
            for (const QJsonValue &sv : params.value(QStringLiteral("scopes")).toArray()) {
                const QString s = sv.toString().trimmed().toLower();
                if (s == QStringLiteral("fonts") || s == QStringLiteral("typography"))
                    scopeFlags |= ApplicationWindow::FormatFonts;
                else if (s == QStringLiteral("colors") || s == QStringLiteral("symbols")
                         || s == QStringLiteral("colors_and_symbols"))
                    scopeFlags |= ApplicationWindow::FormatColorsAndSymbols;
                else if (s == QStringLiteral("frame") || s == QStringLiteral("ticks")
                         || s == QStringLiteral("frame_and_ticks"))
                    scopeFlags |= ApplicationWindow::FormatFrameAndTicks;
                else if (s == QStringLiteral("scales") || s == QStringLiteral("axis_scales"))
                    scopeFlags |= ApplicationWindow::FormatAxisScales;
                else if (s == QStringLiteral("dimensions") || s == QStringLiteral("page"))
                    scopeFlags |= ApplicationWindow::FormatPageDimensions;
                else if (s == QStringLiteral("breaks") || s == QStringLiteral("reflines")
                         || s == QStringLiteral("breaks_and_reflines"))
                    scopeFlags |= ApplicationWindow::FormatBreaksAndRefLines;
                else if (s == QStringLiteral("all"))
                    scopeFlags |= (ApplicationWindow::FormatDefaultSafe
                                   | ApplicationWindow::FormatAxisScales
                                   | ApplicationWindow::FormatBreaksAndRefLines);
            }
            if (scopeFlags == 0)
                scopeFlags = ApplicationWindow::FormatDefaultSafe;
        }
        ScopedGraphUndoGuard undoGuard(ml, QObject::tr("Paste Graph Format"));
        struct SuppressInnerUndoScope
        {
            MultiLayer *target = nullptr;
            ~SuppressInnerUndoScope()
            {
                if (target)
                    target->setProperty("_suppressInnerUndoPush", QVariant());
            }
        } suppressInner{ ml };
        ml->setProperty("_suppressInnerUndoPush", true);
        d_app->activateSubWindow(ml);
        d_app->pasteGraphFormatWithScope(scopeFlags);

        const HeadlessFormatClipboard &clip = rpcFormatClipboard();
        // Fallback when headless QMdiArea::activeSubWindow() returns nullptr
        if (clip.valid && d_app->d_workspace.activeSubWindow() != ml) {
            const bool doFonts = (scopeFlags & ApplicationWindow::FormatFonts) != 0;
            const bool doColors = (scopeFlags & ApplicationWindow::FormatColorsAndSymbols) != 0;
            const bool doFrame = (scopeFlags & ApplicationWindow::FormatFrameAndTicks) != 0;
            const bool singleToSingle = (clip.srcLayerCount <= 1 && ml->layers() <= 1);
            const bool doDims = (scopeFlags & ApplicationWindow::FormatPageDimensions) != 0
                    && (singleToSingle || scopeFlags == ApplicationWindow::FormatPageDimensions);
            if (doDims) {
                ml->setMargins(clip.lm, clip.rm, clip.tm, clip.bm);
                ml->setSpacing(clip.rSpace, clip.cSpace);
                ml->setPageBackgroundColor(clip.pageBg);
                if (clip.pageW > 0.0 && clip.pageH > 0.0)
                    ml->setPageSizePhysical(clip.pageW, clip.pageH, clip.pageUnit, clip.pageDpi);
                ml->setPageViewMode(clip.pageViewMode);
                ml->setTargetAspectRatio(clip.aspect);
                ml->setKeepAspectRatio(clip.keepAspect);
            } else if (doFrame) {
                ml->setPageBackgroundColor(clip.pageBg);
            }
            QList<Graph *> pasteLayers = ml->layersList();
            if (params.contains(QStringLiteral("layer"))
                && !params.value(QStringLiteral("layer")).isNull()) {
                pasteLayers.clear();
                pasteLayers.append(resolveGraphLayer(ml, params, true));
            }
            for (Graph *lg : pasteLayers) {
                if (doFrame)
                    lg->drawCanvasFrame(clip.framed, clip.frameWidth, clip.frameColor);
                if (doFonts && clip.hasPlotTitleFont && lg->plotWidget()) {
                    QwtText t = lg->plotWidget()->title();
                    t.setFont(clip.plotTitleFont);
                    lg->plotWidget()->setTitle(t);
                }
                if ((doFonts || doFrame || doColors) && clip.hasLegendFont && lg->legend()) {
                    Legend *leg = lg->legend();
                    if (doFonts)
                        leg->setFont(clip.legendFont);
                    if (doColors)
                        leg->setTextColor(clip.legendTextColor);
                    if (doFrame) {
                        leg->setBackgroundColor(clip.legendBgColor);
                        leg->setFrameStyle(clip.legendFrameStyle);
                        leg->setNumColumns(clip.legendNumColumns);
                    }
                }
                for (int ax = 0; ax < 4; ++ax) {
                    if (doFrame) {
                        lg->enableAxis(ax, clip.axisEnabled[ax]);
                        lg->setAxisBackboneVisible(ax, clip.backboneVis[ax]);
                    }
                    if (doFonts) {
                        lg->setAxisTitleFont(ax, clip.axisTitleFonts[ax]);
                        lg->setAxisFont(ax, clip.axisNumFonts[ax]);
                    }
                    if (doFrame && lg->plotWidget()) {
                        if (auto *sd =
                                    dynamic_cast<ScaleDraw *>(lg->plotWidget()->axisScaleDraw(ax))) {
                            sd->setMajorTicksStyle(
                                    static_cast<ScaleDraw::TicksStyle>(clip.majTicks[ax]));
                            sd->setMinorTicksStyle(
                                    static_cast<ScaleDraw::TicksStyle>(clip.minTicks[ax]));
                        }
                    }
                }
                if (doColors && !clip.curves.isEmpty()) {
                    int visIdx = 0;
                    for (int ci = 0; ci < lg->curves(); ++ci) {
                        QwtPlotCurve *c = lg->curve(ci);
                        if (!c || c->rtti() == QwtPlotItem::Rtti_PlotSpectrogram
                            || lg->curveType(ci) == Graph::ErrorBars)
                            continue;
                        const bool directMatch = (visIdx < clip.curves.size());
                        const auto &cf =
                                clip.curves[directMatch ? visIdx : (clip.curves.size() - 1)];
                        if (directMatch) {
                            lg->setCurveStyle(ci, cf.curveStyle);
                            lg->setCurvePen(ci, cf.pen);
                            lg->setCurveBrush(ci, cf.brush);
                            lg->setCurveSymbol(ci, cf.symbol);
                            if (cf.symbolHollow)
                                lg->setCurveSymbolHollow(ci, true);
                        } else {
                            QPen p = c->pen();
                            p.setWidthF(cf.pen.widthF());
                            p.setStyle(cf.pen.style());
                            p.setCapStyle(cf.pen.capStyle());
                            p.setJoinStyle(cf.pen.joinStyle());
                            lg->setCurvePen(ci, p);
                            QwtSymbol s = c->symbol();
                            if (cf.symbol.style() != QwtSymbol::NoSymbol
                                && s.style() == QwtSymbol::NoSymbol)
                                s.setStyle(cf.symbol.style());
                            s.setSize(cf.symbol.size());
                            QPen sp = s.pen();
                            sp.setWidthF(cf.symbol.pen().widthF());
                            s.setPen(sp);
                            lg->setCurveSymbol(ci, s);
                            if (cf.symbolHollow || Graph::isSymbolWhiteHollow(s))
                                lg->setCurveSymbolHollow(ci, cf.symbolHollow);
                        }
                        ++visIdx;
                    }
                }
                lg->replot();
            }
            if (doFrame || doFonts || doDims) {
                ml->arrangeLayers(false, false);
                if (doFrame && clip.hasLegendFont && clip.hasLegendNormPos) {
                    for (Graph *lg : ml->layersList()) {
                        if (Legend *leg = lg->legend()) {
                            if (lg->plotWidget() && lg->plotWidget()->canvas()) {
                                lg->plotWidget()->updateLayout();
                                const QRect cr = lg->plotWidget()->canvas()->contentsRect();
                                if (cr.width() > 4 && cr.height() > 4) {
                                    const int nx = cr.left()
                                            + qRound(clip.legendNormX * double(cr.width()));
                                    const int ny = cr.top()
                                            + qRound(clip.legendNormY * double(cr.height()));
                                    leg->setOrigin(QPoint(nx, ny));
                                    lg->replot();
                                }
                            }
                        }
                    }
                }
            }
        }
        ml->setProperty("_suppressInnerUndoPush", QVariant());
        suppressInner.target = nullptr;
        undoGuard.commit();
        outResult = serializeGraphInfo(ml);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    if (method == QStringLiteral("graph.batch_clone")) {
        MultiLayer *srcMl = resolveMultiLayer(d_app, params, QStringLiteral("graph"), true);
        const bool hasXCol = params.contains(QStringLiteral("x_col"))
                && !params.value(QStringLiteral("x_col")).isNull();
        const QJsonValue xColVal = params.value(QStringLiteral("x_col"));
        const QJsonArray yColsArr = params.value(QStringLiteral("y_cols")).isArray()
                ? params.value(QStringLiteral("y_cols")).toArray()
                : QJsonArray();
        if (params.contains(QStringLiteral("tables"))) {
            if (!params.value(QStringLiteral("tables")).isArray())
                throwInvalidParams(QStringLiteral("Parameter 'tables' must be a list of table names."));
            const QJsonArray tArr = params.value(QStringLiteral("tables")).toArray();
            if (tArr.isEmpty())
                throwInvalidParams(QStringLiteral("Parameter 'tables' must be a non-empty list."));
            if (tArr.size() > 128)
                throwInvalidParams(QStringLiteral("Parameter 'tables' supports at most 128 tables per call."));
            QJsonArray clonedArr;
            for (const QJsonValue &tv : tArr) {
                QJsonObject subP;
                subP[QStringLiteral("table")] = tv;
                Table *dstT = resolveTable(d_app, subP, QStringLiteral("table"), true);
                if (hasXCol)
                    dstT->setColPlotDesignation(resolveColumnIndex(dstT, xColVal, true), SciDAVis::X);
                for (const QJsonValue &yv : yColsArr)
                    dstT->setColPlotDesignation(resolveColumnIndex(dstT, yv, true), SciDAVis::Y);
                if (MultiLayer *cl = d_app->batchCloneGraphToTable(srcMl, dstT))
                    clonedArr.append(cl->name());
            }
            outResult[QStringLiteral("ok")] = true;
            outResult[QStringLiteral("cloned_graphs")] = clonedArr;
            return true;
        }
        Table *dstT = resolveTable(d_app, params, QStringLiteral("table"), true);
        if (hasXCol)
            dstT->setColPlotDesignation(resolveColumnIndex(dstT, xColVal, true), SciDAVis::X);
        for (const QJsonValue &yv : yColsArr)
            dstT->setColPlotDesignation(resolveColumnIndex(dstT, yv, true), SciDAVis::Y);
        MultiLayer *cloned = d_app->batchCloneGraphToTable(srcMl, dstT);
        if (!cloned)
            throwRpcRuntime(QStringLiteral("Failed to batch-clone graph '%1' to table '%2'.")
                                    .arg(srcMl->name(), dstT->name()));
        outResult = serializeGraphInfo(cloned);
        outResult[QStringLiteral("ok")] = true;
        return true;
    }

    return false;
}

} // namespace rpc_internal
