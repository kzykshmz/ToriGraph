/***************************************************************************
    File                 : PythonAutomationServer_Internal.h
    Project              : ToriGraph (SciDAVis)
    --------------------------------------------------------------------
    Description          : Internal shared helpers and sub-dispatchers for
                           split PythonAutomationServer translation units
 ***************************************************************************/
#ifndef PYTHON_AUTOMATION_SERVER_INTERNAL_H
#define PYTHON_AUTOMATION_SERVER_INTERNAL_H

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "PythonAutomationServer.h"

#include "ApplicationWindow.h"
#include "Graph.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "Spectrogram.h"
#include "Table.h"
#include "core/column/Column.h"

#include <qwt_plot_curve.h>

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

class ApplicationWindow;
class Graph3D;
class MyWidget;

namespace rpc_internal {

// Custom domain exception carrying JSON-RPC error code and optional structured error.data
struct RpcDispatchException : public std::runtime_error
{
    int code;
    QJsonValue data;
    RpcDispatchException(int c, const QString &msg, const QJsonValue &errData = QJsonValue())
        : std::runtime_error(msg.toUtf8().constData()), code(c), data(errData)
    {
    }
};

[[noreturn]] inline void throwInvalidParams(const QString &msg, const QJsonValue &errData = QJsonValue())
{
    throw RpcDispatchException(-32602, msg, errData);
}

[[noreturn]] void throwInvalidParamsWithAvailable(const QString &msg,
                                                  const QStringList &available,
                                                  const QJsonObject &extraData = QJsonObject());

[[noreturn]] inline void throwRpcRuntime(const QString &msg)
{
    throw RpcDispatchException(-32000, msg);
}

inline QJsonValue safeJsonDouble(double v)
{
    if (!std::isfinite(v))
        return QJsonValue::Null;
    return QJsonValue(v);
}

inline QJsonArray toJsonArray(const std::vector<double> &vec)
{
    QJsonArray arr;
    for (double v : vec)
        arr.append(safeJsonDouble(v));
    return arr;
}

inline QJsonArray toJsonArray(const QVector<double> &vec)
{
    QJsonArray arr;
    for (double v : vec)
        arr.append(safeJsonDouble(v));
    return arr;
}

inline QJsonArray toJsonArray(const QStringList &lst)
{
    QJsonArray arr;
    for (const QString &s : lst)
        arr.append(s);
    return arr;
}

inline double jsonToDoubleOrNaN(const QJsonValue &val)
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

inline int parseStrictJsonInt(const QJsonValue &val, const QString &paramName)
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

QString sanitizeUtf16Surrogates(const QString &in);

inline QString optionalJsonString(const QJsonObject &params, const QString &key,
                           const QString &defaultVal = QString())
{
    if (!params.contains(key) || params.value(key).isUndefined() || params.value(key).isNull())
        return defaultVal;
    const QJsonValue v = params.value(key);
    if (!v.isString()) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' must be a string.").arg(key));
    }
    return sanitizeUtf16Surrogates(v.toString());
}

inline bool optionalJsonBool(const QJsonObject &params, const QString &key, bool defaultVal)
{
    if (!params.contains(key) || params.value(key).isUndefined() || params.value(key).isNull())
        return defaultVal;
    const QJsonValue v = params.value(key);
    if (v.isBool())
        return v.toBool();
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (d == 0.0)
            return false;
        if (d == 1.0)
            return true;
    }
    if (v.isString()) {
        const QString s = v.toString().trimmed().toLower();
        if (s == QStringLiteral("true") || s == QStringLiteral("1") || s == QStringLiteral("yes")
            || s == QStringLiteral("on"))
            return true;
        if (s == QStringLiteral("false") || s == QStringLiteral("0") || s == QStringLiteral("no")
            || s == QStringLiteral("off"))
            return false;
    }
    throwInvalidParams(QStringLiteral("Parameter '%1' must be a boolean.").arg(key));
}

inline int parseBoundedInt(const QJsonObject &params, const QString &key, int defaultVal, int minVal,
                    int maxVal)
{
    if (!params.contains(key) || params.value(key).isUndefined() || params.value(key).isNull())
        return defaultVal;
    const int v = parseStrictJsonInt(params.value(key), key);
    if (v < minVal || v > maxVal) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' must be in [%2, %3], got %4.")
                        .arg(key)
                        .arg(minVal)
                        .arg(maxVal)
                        .arg(v));
    }
    return v;
}

// Font sizes may be fractional (the lab slide preset uses 12.9 pt). Whole sizes are still
// returned as JSON integers so existing scripts see the same values as before.
inline QJsonValue fontPtJson(const QFont &f)
{
    const double pt = f.pointSizeF();
    if (!(pt > 0.0))
        return f.pointSize();
    const double r = std::round(pt * 100.0) / 100.0;
    if (std::fabs(r - std::round(r)) < 1e-9)
        return int(std::lround(r));
    return r;
}

inline double parseFiniteCoordDouble(const QJsonValue &val, const QString &paramName,
                              double absCap = 1e300)
{
    if (!val.isDouble() || val.isBool()) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' must be a finite number.").arg(paramName));
    }
    const double d = val.toDouble();
    if (!std::isfinite(d) || std::abs(d) > absCap) {
        throwInvalidParams(
                QStringLiteral("Parameter '%1' is non-finite or exceeds numeric domain (|val| <= %2).")
                        .arg(paramName)
                        .arg(absCap, 0, 'e', 1));
    }
    return d;
}

inline double optionalJsonFiniteDouble(const QJsonObject &params, const QString &key, double defaultVal,
                                double absCap = 1e300)
{
    if (!params.contains(key) || params.value(key).isUndefined() || params.value(key).isNull())
        return defaultVal;
    return parseFiniteCoordDouble(params.value(key), key, absCap);
}

inline void validateFiniteCoordRange(double lo, double hi, const QString &label, double step = 0.0,
                              int maxTicks = 2000, double absCap = 1e300)
{
    if (!std::isfinite(lo) || !std::isfinite(hi) || std::abs(lo) > absCap
        || std::abs(hi) > absCap) {
        throwInvalidParams(
                QStringLiteral("%1 bounds must be finite and within [-%2, +%2].")
                        .arg(label)
                        .arg(absCap, 0, 'e', 1));
    }
    const double span = hi - lo;
    if (!std::isfinite(span) || lo == hi) {
        throwInvalidParams(
                QStringLiteral("%1 requires distinct, non-overflowing bounds [min, max].")
                        .arg(label));
    }
    if (step < 0.0 || !std::isfinite(step)) {
        throwInvalidParams(
                QStringLiteral("%1 step must be >= 0 and finite.").arg(label));
    }
    if (step > 0.0) {
        const double ticks = std::abs(span) / step;
        if (!std::isfinite(ticks) || ticks > double(maxTicks)) {
            throwInvalidParams(
                    QStringLiteral("%1 step (%2) produces > %3 major ticks across span %4.")
                            .arg(label)
                            .arg(step, 0, 'g', 6)
                            .arg(maxTicks)
                            .arg(std::abs(span), 0, 'g', 6));
        }
    }
}

// Batch execution state for app.batch coalesced undo snapshots & deferred replot
struct BatchExecutionContext
{
    bool active = false;
    bool deferReplot = false;
    int recursionDepth = 0;
    QVector<QPointer<Graph>> dirtyGraphs;
    QVector<QPointer<MultiLayer>> dirtyMultiLayers;
    QVector<QPointer<MultiLayer>> snapshottedMultiLayers;
    QVector<QPair<QPointer<MultiLayer>, QString>> preBatchGraphSnaps;

    void markDirtyGraph(Graph *g)
    {
        if (!g)
            return;
        for (const QPointer<Graph> &p : dirtyGraphs) {
            if (p.data() == g)
                return;
        }
        dirtyGraphs.append(QPointer<Graph>(g));
    }

    void markDirtyMultiLayer(MultiLayer *ml)
    {
        if (!ml)
            return;
        for (const QPointer<MultiLayer> &p : dirtyMultiLayers) {
            if (p.data() == ml)
                return;
        }
        dirtyMultiLayers.append(QPointer<MultiLayer>(ml));
    }

    bool hasSnapshotted(MultiLayer *ml) const
    {
        if (!ml)
            return false;
        for (const QPointer<MultiLayer> &p : snapshottedMultiLayers) {
            if (p.data() == ml)
                return true;
        }
        return false;
    }

    void markSnapshotted(MultiLayer *ml, const QString &initialSnap = QString())
    {
        if (!ml || hasSnapshotted(ml))
            return;
        snapshottedMultiLayers.append(QPointer<MultiLayer>(ml));
        if (!initialSnap.isEmpty())
            preBatchGraphSnaps.append(qMakePair(QPointer<MultiLayer>(ml), initialSnap));
    }
};

BatchExecutionContext &rpcBatchContext();

template <typename G, typename = void>
struct HasGraphSetCategoryLabels : std::false_type {};
template <typename G>
struct HasGraphSetCategoryLabels<
        G, std::void_t<decltype(std::declval<G *>()->setCategoryLabels(0, QStringList()))>>
    : std::true_type {};

template <typename G, typename = void>
struct HasGraphCategoryLabels : std::false_type {};
template <typename G>
struct HasGraphCategoryLabels<G, std::void_t<decltype(std::declval<const G *>()->categoryLabels(0))>>
    : std::true_type {};

template <typename G, typename = void>
struct HasGraphIsPolarPlot : std::false_type {};
template <typename G>
struct HasGraphIsPolarPlot<G, std::void_t<decltype(std::declval<const G *>()->isPolarPlot())>>
    : std::true_type {};

template <typename G = Graph>
void invokeGraphSetCategoryLabels(ApplicationWindow *app, G *g, int qwtAxis,
                                  const QStringList &labels)
{
    if (!g)
        return;
    g->setProperty(QStringLiteral("_toriCatLabels_%1").arg(qwtAxis).toLatin1().constData(), labels);
    if constexpr (HasGraphSetCategoryLabels<G>::value) {
        g->setCategoryLabels(qwtAxis, labels);
    } else {
        if (!app || labels.isEmpty())
            return;
        auto *colTxt = new Column(QStringLiteral("Cat"), SciDAVis::ColumnMode::Text);
        colTxt->setPlotDesignation(SciDAVis::X);
        colTxt->replaceTexts(0, labels);
        const QString tblName = app->generateUniqueName(QStringLiteral("RpcCategoryAxis"));
        Table *backing = app->newHiddenTable(tblName, QStringLiteral("CategoryLabels"),
                                             QList<Column *>() << colTxt);
        if (!backing) {
            delete colTxt;
            return;
        }
        g->setLabelsTextFormat(qwtAxis, backing, tblName + QStringLiteral("_") + colTxt->name());
        const int n = labels.size();
        g->setScale(qwtAxis, 0.5, double(n) + 0.5, 1.0, std::min(std::max(n + 2, 4), 32), 0, 0,
                    false);
    }
}

template <typename G = Graph>
QStringList invokeGraphCategoryLabels(const G *g, int qwtAxis)
{
    if (!g)
        return QStringList();
    if constexpr (HasGraphCategoryLabels<G>::value) {
        const QStringList direct = g->categoryLabels(qwtAxis);
        if (!direct.isEmpty())
            return direct;
    }
    const QVariant prop =
            g->property(QStringLiteral("_toriCatLabels_%1").arg(qwtAxis).toLatin1().constData());
    if (prop.isValid())
        return prop.toStringList();
    return QStringList();
}

template <typename G = Graph>
bool invokeGraphIsPolarPlot(const G *g)
{
    if (!g)
        return false;
    if constexpr (HasGraphIsPolarPlot<G>::value) {
        if (g->isPolarPlot())
            return true;
    }
    return g->property("_toriIsPolarPlot").toBool();
}

int countValidCurvePoints(QwtPlotCurve *c, int *distinctXOut = nullptr);
void extractValidUnmaskedCurveXY(QwtPlotCurve *c, std::vector<double> &xs, std::vector<double> &ys);
bool checkMuParserExprValid(const QString &expr, const QStringList &varNames,
                            QString *errMsgOut = nullptr);
void validateMuParserExprOrThrow(const QString &expr, const QStringList &varNames,
                                 const QString &opName);
int requireCurveMinPoints(Graph *g, const QString &curveTitle, int minPts, const QString &opName,
                          int *distinctXOut = nullptr);
QColor parseColorValue(const QJsonValue &val, const QColor &fallback = QColor(Qt::black));
Qt::PenStyle parsePenStyleValue(const QJsonValue &val, Qt::PenStyle fallback = Qt::SolidLine);
QString windowTypeString(MyWidget *w);
QStringList collectWindowNames(ApplicationWindow *app, const char *inheritsClass = nullptr);
MyWidget *findWindowByName(ApplicationWindow *app, const QString &name);
Table *resolveTable(ApplicationWindow *app, const QJsonObject &params,
                    const QString &key = QStringLiteral("table"), bool required = true);
Matrix *resolveMatrix(ApplicationWindow *app, const QJsonObject &params,
                      const QString &key = QStringLiteral("matrix"), bool required = true);
MultiLayer *resolveMultiLayer(ApplicationWindow *app, const QJsonObject &params,
                              const QString &key = QStringLiteral("graph"), bool required = true);
Graph *resolveGraphLayer(MultiLayer *ml, const QJsonObject &params, bool required = true);
int resolveColumnIndex(Table *t, const QJsonValue &colSpec, bool required = true);

inline QString fullColumnName(Table *t, int colIdx)
{
    if (!t || colIdx < 0 || colIdx >= t->numCols())
        return QString();
    return t->colName(colIdx);
}

void enforceArialGraphFonts(Graph *g);
bool resolveStylePresetForRpc(const QString &rawPreset, MultiLayer::GraphStylePresetSpec *outSpec,
                              bool throwOnUnknown = true);
void applyResolvedStylePreset(MultiLayer *ml, const MultiLayer::GraphStylePresetSpec &spec);
void replotGraphOrDefer(Graph *g);
void enforceArialGraph3DFonts(Graph3D *g3d);
QString resolveCurveTitle(Graph *g, const QJsonObject &params,
                          const QString &key = QStringLiteral("curve"), bool required = true);
//! Same lookup as resolveCurveTitle(), but returns the curve index, so a curve picked by its
//! label (or position) is not confused with an earlier curve plotting the same Y column.
//! Returns -1 when nothing matched and \a required is false.
int resolveCurveIndex(Graph *g, const QJsonObject &params,
                      const QString &key = QStringLiteral("curve"), bool required = true,
                      bool *matchedByLabel = nullptr);
QString sanitizeWindowNameForSciDAVis(const QString &raw,
                                      const QString &fallbackPrefix = QStringLiteral("Win"),
                                      bool preserveUnderscores = false);
QString makeUniqueValidWindowName(ApplicationWindow *app, const QString &sanitizedBase,
                                  MyWidget *targetWidget = nullptr);
void applyDefaultNumericFormat6g(Table *t, int colIdx);
Table *createUniqueTableHeadless(ApplicationWindow *app, int rows, int cols, const QString &reqName);
void resolveGraphAndCurveOrAutoPlot(ApplicationWindow *app, const QJsonObject &params,
                                    MultiLayer *&outMl, Graph *&outGraph, QString &outCurveTitle);
QJsonObject extractTableAsJson(Table *t, int maxRows = 5000);

QVariant &rpcPrevNonInteractiveSlot();

//! While alive, restores the pre-RPC value of the application-global "torigraph_noninteractive"
//! property so that the user's own GUI actions inside a nested event loop (network waits, ...)
//! keep their normal save-confirmation behaviour. The RPC value is reinstated afterwards.
struct RpcInteractiveWindow
{
    QCoreApplication *qa;
    QVariant rpcValue;
    RpcInteractiveWindow() : qa(QCoreApplication::instance())
    {
        if (qa) {
            rpcValue = qa->property("torigraph_noninteractive");
            qa->setProperty("torigraph_noninteractive", rpcPrevNonInteractiveSlot());
        }
    }
    ~RpcInteractiveWindow()
    {
        if (qa)
            qa->setProperty("torigraph_noninteractive", rpcValue);
    }
};

QJsonObject serializeTableInfo(Table *t);
QJsonObject serializeMatrixInfo(Matrix *m);

// Shared Plot/Graph helpers
QString pageUnitToString(MultiLayer::PageUnit u);
int parseAxisId(const QString &axisStr);

struct ScopedGraphUndoGuard
{
    MultiLayer *ml = nullptr;
    QString label;
    QString beforeSnap;
    int countBefore = 0;
    int indexBefore = 0;
    bool macroOpened = false;

    ScopedGraphUndoGuard(MultiLayer *m, const QString &lbl, bool wrapMacro = false);
    ~ScopedGraphUndoGuard();
    void checkpointBeforeSubPush();
    void checkpointAfterSubPush();
    void commit();
};

Spectrogram::ColorMapPreset parseColorMapPreset(const QString &name, bool *outGrayScale = nullptr,
                                                bool strict = false);
void applyRequestedWindowName(ApplicationWindow *app, MyWidget *w, const QJsonObject &params,
                              const QString &explicitKey = QString());
QJsonObject serializeAxisInfo(Graph *g, int qwtAxis);
QJsonObject serializeGraphInfo(MultiLayer *ml);
QJsonObject serializeGraph3DInfo(Graph3D *g3d);

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
                                           const std::vector<double> &scores);
void extractBinaryStateRows(Table *t, int stIdx, const QVector<int> &numericCheckCols,
                            std::vector<int> &yOut, std::vector<int> &validRowsOut);

void sanitizeGraphFunctionCurves(ApplicationWindow *app, Graph *g);
bool graphHasJapaneseContext(Graph *g, int upToCurveCount);
void harmonizeFitCurveLabelsOnGraph(Graph *g, int curvesBefore, bool hadJapaneseBefore,
                                    const QJsonObject &params);
QJsonObject inspectGraphDetailed(MultiLayer *ml, bool includeTicks, bool diagnose);
void closeAndDeleteEmptyPlot(ApplicationWindow *app, MultiLayer *ml);
int countFiniteUnmaskedInTableColumn(Table *t, int colIdx, int optXIdx = -1);
void applyInlineCurveStyleParams(Graph *g, int cIdx, const QJsonObject &params,
                                 bool applyColor = true);
void sanitizeAllProjectFunctionCurves(ApplicationWindow *app);

struct ApiMethodCatalogRow
{
    const char *method;
    const char *domain;
    const char *sdkMethod;
    const char *summary;
    const char *paramsSig;
    const char *example;
};
extern const ApiMethodCatalogRow kApiCatalogSpecs[];
extern const int kApiCatalogCount;
QJsonObject serializeCatalogRow(const ApiMethodCatalogRow &row);
void validateParamsAgainstCatalogSchema(const QString &method, const QJsonObject &params);

QString generatePythonScriptInternal(ApplicationWindow *app, MyWidget *window, bool standalone,
                                     const QString &embedDataRaw, const QString &csvSidecarDir,
                                     QStringList *outTables, QStringList *outMatrices);

// Sub-unit RPC dispatchers
bool dispatchAppTableMatrixMethod(PythonAutomationServer *server, ApplicationWindow *d_app,
                                  bool &focusHudFallback, QString &workspaceModeFallback,
                                  QString &lastUndoDomain, QString &lastUndoGraph,
                                  const QString &method, const QJsonObject &params,
                                  QJsonObject &outResult);
bool dispatchPlotAndLayerMethod(ApplicationWindow *d_app, const QString &method,
                                const QJsonObject &params, QJsonObject &outResult);
bool dispatchGraphDetailAndExportMethod(ApplicationWindow *d_app, const QString &method,
                                        const QJsonObject &params, QJsonObject &outResult);
bool dispatchCoreAndGraphMethod(PythonAutomationServer *server, ApplicationWindow *d_app,
                                bool &focusHudFallback, QString &workspaceModeFallback,
                                QString &lastUndoDomain, QString &lastUndoGraph,
                                const QString &method, const QJsonObject &params,
                                QJsonObject &outResult);
bool dispatchAnalysisAndSignalMethod(ApplicationWindow *d_app, const QString &method,
                                     const QJsonObject &params, QJsonObject &outResult);
bool dispatchCurveMathAndStatsMethod(ApplicationWindow *d_app, const QString &method,
                                     const QJsonObject &params, QJsonObject &outResult);
bool dispatchAnalysisAndScienceMethod(ApplicationWindow *d_app, const QString &method,
                                      const QJsonObject &params, QJsonObject &outResult);

} // namespace rpc_internal

#endif // PYTHON_AUTOMATION_SERVER_INTERNAL_H
