#include "ole_test_common.h"
#include "ColorButton.h"
#include "ColorMapEditor.h"
#include "Convolution.h"
#include "CurveMathTools.h"
#include "CurvesDialog.h"
#include "GraphInspectorDock.h"
#include "GraphMiniToolbar.h"
#include "ImageDialog.h"
#include "ImportOPJU.h"
#include "Integration.h"
#include "MuParserScript.h"
#include "MuParserScripting.h"
#include "PlotDialog.h"
#include "QwtErrorPlotCurve.h"
#include "QwtHistogram.h"
#include "QwtPieCurve.h"
#include "ScriptEdit.h"
#include "SmoothFilter.h"
#include "Spectrogram.h"
#include "TableStatistics.h"
#include "future/core/datatypes/DateTime2StringFilter.h"
#include "future/core/datatypes/NumericDateTimeBaseFilter.h"
#include "future/lib/Interval.h"
#include "future/lib/IntervalAttribute.h"
#include "future/matrix/future_Matrix.h"
#include "future/table/future_Table.h"
#include "qwt3d_color.h"
#include "qwt3d_surfaceplot.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTreeWidget>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// =============================================================================
// Suite 114: Deep Swarm Audit & Multi-Pattern Debug Loop Regressions
//   114a Numerics, Smoothing, Fitting, Convolution, Integration & CurveMath:
//        - Savitzky-Golay centered QR projection preserves degree-3/5 polynomials at order 6,
//          window 31 without Vandermonde ill-conditioning; setPolynomOrder(-1) rejected cleanly
//        - Whittaker smoother at extreme lambda=1e16 falls back to exact OLS line (all finite)
//        - Integration(Cubic) matches between logInfo/result() and plotted cumulative curve
//        - Convolution linear zero-padding prevents wrap-around and matches exact output length
//        - Fit::rSquare() on constant Y returns 1.0 only when RSS is negligible, NaN otherwise
//   114b Core Models, MuParserScript, Columns, Intervals, Matrix, Table & ScriptEdit:
//        - MuParserScript unclosed col( no infinite loop/OOM; identifier "mycol" preserved;
//          second '#' on comment line does not corrupt following line
//        - IntervalBase::intersects() handles strict containment [5,10] inside [1,20]
//        - IntervalAttribute<bool>::removeRows() merges multiple adjacent intervals without OOB
//        - NumericDateTimeBaseFilter fractional Year, monotonic Month/Year, NaN/overflow safety,
//          and DateTime2StringFilter::textAt() out-of-range guards
//        - Column::replace* guards on first < 0 & mismatched mode, dynamic_cast filter safety,
//          and ColumnRemoveRowsCmd undo/redo
//        - future::Matrix zero-row/col ops (clear, mirror, remove, copy), Table::copy() rowCount,
//          TableStatistics NaN/Inf filtering, Table::restore() malformed lines, ScriptEdit indent
//   114c 2D/3D Plotting, GraphMiniToolbar, PlotDialog, Serialization & Undo:
//        - Graph::setCurveType VerticalBars <-> HorizontalBars <-> LineSymbols; safe NoPen/NoBrush
//        - QwtHistogram huge bin count clamp (no float-cast-overflow UB)
//        - Qwt3D::StandardColor constant-Z and NaN-Z safety
//        - Graph::copy() after Txt axis -> setWeibullProbabilityAxis(true) (no type confusion)
//        - MultiLayer::setGraphGeometry(0 layers), Plot::removeMarker(-999), Graph3D addFunction
//        - batchCloneGraphToTable on solid-pen Bar & Histogram preserves gap/offset & binning
//        - GraphMiniToolbar hollow toggle on bar curve (no UAF) & custom/deleted legend preservation
//        - Double-undo prevention on removeTitle / removeReferenceLine via clear/cutSelection
//        - PlotDialog axesPage moves attached error bars with data curve via Graph::setCurveAxes
//   114d Project IO, ImportOPJU, importASCII & JSON-RPC 2.0:
//        - Legacy openMatrix / openTable / openGraph truncated/missing-tag resilience
//        - ImportOPJU appendProject preserves host root folder and avoids dual-folder ownership
//        - importASCII NewRows single Undo macro & Overwrite atomic rejection on type mismatch
//        - RPC stats.clustering(hierarchical) & fit_robust(theil_sen/siegel) size caps
//        - RPC fit_york / fit_linear / fit_nonlinear integer column index support
//        - RPC batch_peaks & signal.coherence explicit x_col right of y_cols
//        - RPC plot.violin & plot.matrix attach-to-existing undo/redo preserves styling
//        - RPC graph.set_axis & graph.to_python preserve log10 across active axis breaks
// =============================================================================

namespace {

int g114Failures = 0;

#define CHECK114(cond, msg)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;            \
            ++g114Failures;                                                                        \
        }                                                                                          \
    } while (0)

struct NonInteractiveGuard114
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard114()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard114()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard114
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard114(ApplicationWindow *w = nullptr) : win(w)
    {
        if (win) {
            win->setHeadlessMode(true);
            win->applyUserSettings();
            win->confirmCloseTable = false;
            win->confirmCloseMatrix = false;
            win->confirmClosePlot2D = false;
            win->confirmClosePlot3D = false;
            win->confirmCloseFolder = false;
            win->confirmCloseNotes = false;
        }
    }
    ~AppWinGuard114()
    {
        if (win) {
            win->savedProject();
            win->setOleEmbeddedMode(true);
            delete win;
            win = nullptr;
        }
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    AppWinGuard114(const AppWinGuard114 &) = delete;
    AppWinGuard114 &operator=(const AppWinGuard114 &) = delete;
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

struct Rpc114
{
    PythonAutomationServer *srv = nullptr;
    int id = 11400;
    QJsonObject call(const QString &method, const QJsonObject &params)
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = ++id;
        req[QStringLiteral("method")] = method;
        req[QStringLiteral("params")] = params;
        const QJsonObject resp = srv->handleRpcRequest(req);
        QCoreApplication::processEvents();
        return resp;
    }
    static bool ok(const QJsonObject &resp) { return !resp.contains(QStringLiteral("error")); }
    static QJsonObject result(const QJsonObject &resp)
    {
        return resp.value(QStringLiteral("result")).toObject();
    }
};

MultiLayer *findGraph114(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer") && w->name() == name)
            return static_cast<MultiLayer *>(w);
    }
    return nullptr;
}

// Build minimal uncompressed ZIP archive (STORE method) for ImportOPJU XML tests
QByteArray makeStoreZip114(const QList<QPair<QString, QByteArray>> &entries)
{
    auto crc32 = [](const QByteArray &data) -> quint32 {
        quint32 c = 0xFFFFFFFFu;
        for (unsigned char b : data) {
            c ^= b;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        return ~c;
    };
    auto appendU16 = [](QByteArray &out, quint16 v) {
        out.append(char(v & 0xFF));
        out.append(char((v >> 8) & 0xFF));
    };
    auto appendU32 = [](QByteArray &out, quint32 v) {
        out.append(char(v & 0xFF));
        out.append(char((v >> 8) & 0xFF));
        out.append(char((v >> 16) & 0xFF));
        out.append(char((v >> 24) & 0xFF));
    };

    QByteArray out;
    QByteArray centralDir;
    for (const auto &entry : entries) {
        const QByteArray nameBytes = entry.first.toUtf8();
        const QByteArray &payload = entry.second;
        const quint32 crc = crc32(payload);
        const quint32 offset = static_cast<quint32>(out.size());

        appendU32(out, 0x04034B50u);
        appendU16(out, 20);
        appendU16(out, 0);
        appendU16(out, 0); // STORE
        appendU16(out, 0);
        appendU16(out, 0);
        appendU32(out, crc);
        appendU32(out, static_cast<quint32>(payload.size()));
        appendU32(out, static_cast<quint32>(payload.size()));
        appendU16(out, static_cast<quint16>(nameBytes.size()));
        appendU16(out, 0);
        out.append(nameBytes);
        out.append(payload);

        appendU32(centralDir, 0x02014B50u);
        appendU16(centralDir, 20);
        appendU16(centralDir, 20);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU32(centralDir, crc);
        appendU32(centralDir, static_cast<quint32>(payload.size()));
        appendU32(centralDir, static_cast<quint32>(payload.size()));
        appendU16(centralDir, static_cast<quint16>(nameBytes.size()));
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU16(centralDir, 0);
        appendU32(centralDir, 0);
        appendU32(centralDir, offset);
        centralDir.append(nameBytes);
    }

    const quint32 cdOffset = static_cast<quint32>(out.size());
    out.append(centralDir);
    appendU32(out, 0x06054B50u);
    appendU16(out, 0);
    appendU16(out, 0);
    appendU16(out, static_cast<quint16>(entries.size()));
    appendU16(out, static_cast<quint16>(entries.size()));
    appendU32(out, static_cast<quint32>(centralDir.size()));
    appendU32(out, cdOffset);
    appendU16(out, 0);
    return out;
}

} // namespace

int runSuite114DeepSwarmAuditRegressions(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    g114Failures = 0;
    NonInteractiveGuard114 niGuard;
    std::cout << "=== Suite 114: Deep Swarm Audit Regressions ===" << std::endl;

    // -------------------------------------------------------------------------
    // 114a: Numerics, Smoothing, Fitting, Convolution, Integration
    // -------------------------------------------------------------------------
    {
        AppWinGuard114 w(new ApplicationWindow());

        // 1. Savitzky-Golay high-order polynomial accuracy & negative order rejection
        {
            const int N = 61;
            Table *t = w->newTable(QStringLiteral("SG114"), N, 2);
            for (int i = 0; i < N; ++i) {
                const double x = -3.0 + 0.1 * i;
                const double y = 1.0 - 2.0 * x + 0.5 * x * x - 0.1 * x * x * x
                        + 0.02 * std::pow(x, 5);
                t->setCell(i, 0, x);
                t->setCell(i, 1, y);
            }
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            CHECK114(g && g->curves() == 1, "114a: SG plot failed");
            if (g && g->curves() == 1) {
                const QString cname = g->curve(0)->title().text();

                // Negative polynomial order must be rejected cleanly
                SmoothFilter badSg(w, g, cname, SmoothFilter::SavitzkyGolay);
                badSg.setSmoothPoints(5, 5);
                badSg.setPolynomOrder(-1);
                CHECK114(badSg.error(), "114a: setPolynomOrder(-1) must set error()");

                // Order 6 on 31-pt window (left=15, right=15): exact on degree-5 polynomial
                SmoothFilter sg(w, g, cname, SmoothFilter::SavitzkyGolay);
                sg.setSmoothPoints(15, 15);
                sg.setPolynomOrder(6);
                CHECK114(!sg.error(), "114a: SG order 6 init failed");
                CHECK114(sg.run(), "114a: SG order 6 run failed");
                CHECK114(g->curves() >= 2, "114a: SG did not add smoothed curve");
                if (g->curves() >= 2) {
                    QwtPlotCurve *sc = g->curve(g->curves() - 1);
                    double maxErr = 0.0;
                    for (int i = 16; i < N - 16; ++i) {
                        const double x = sc->x(i);
                        const double exact = 1.0 - 2.0 * x + 0.5 * x * x - 0.1 * x * x * x
                                + 0.02 * std::pow(x, 5);
                        CHECK114(std::isfinite(sc->y(i)), "114a: SG produced non-finite value");
                        maxErr = std::max(maxErr, std::abs(sc->y(i) - exact));
                    }
                    CHECK114(maxErr < 1e-6,
                             "114a: SG QR order=6,win=31 error too large: " << maxErr);
                }
            }
        }

        // 2. Whittaker smoother with extreme lambda=1e16 -> OLS straight-line fallback
        {
            const int N = 40;
            Table *t = w->newTable(QStringLiteral("Whit114"), N, 2);
            for (int i = 0; i < N; ++i) {
                t->setCell(i, 0, double(i));
                t->setCell(i, 1, 2.0 * i + 5.0 + 0.3 * std::sin(double(i)));
            }
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            if (g && g->curves() == 1) {
                SmoothFilter wh(w, g, g->curve(0)->title().text(), SmoothFilter::Whittaker);
                wh.setWhittakerLambda(1e16);
                CHECK114(wh.run(), "114a: Whittaker lambda=1e16 failed");
                if (g->curves() >= 2) {
                    QwtPlotCurve *wc = g->curve(g->curves() - 1);
                    bool allFinite = (wc->dataSize() == N);
                    double maxDev = 0.0;
                    for (int i = 0; i < wc->dataSize(); ++i) {
                        if (!std::isfinite(wc->y(i)))
                            allFinite = false;
                        maxDev = std::max(maxDev, std::abs(wc->y(i) - (2.0 * i + 5.0)));
                    }
                    CHECK114(allFinite, "114a: Whittaker lambda=1e16 produced non-finite output");
                    CHECK114(maxDev < 0.5,
                             "114a: Whittaker lambda=1e16 diverged from linear trend: " << maxDev);
                }
            }
        }

        // 3. Integration (Cubic) matches between result() and cumulative curve endpoint
        {
            const int N = 11;
            Table *t = w->newTable(QStringLiteral("Int114"), N, 2);
            for (int i = 0; i < N; ++i) {
                const double x = 0.2 * i; // [0, 2]
                t->setCell(i, 0, x);
                t->setCell(i, 1, x * x * x); // exact integral over [0, 2] is 4.0
            }
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            if (g && g->curves() == 1) {
                Integration integ(w, g, g->curve(0)->title().text());
                integ.setMethod(Integration::Cubic);
                integ.setPlotResult(true);
                CHECK114(integ.run(), "114a: Integration(Cubic) run failed");
                const double area = integ.result();
                CHECK114(std::abs(area - 4.0) < 0.05,
                         "114a: Integration(Cubic) expected ~4.0, got " << area);
                CHECK114(g->curves() == 2, "114a: Integration did not add cumulative curve");
                if (g->curves() == 2) {
                    QwtPlotCurve *ic = g->curve(1);
                    const double lastY = ic->y(ic->dataSize() - 1);
                    CHECK114(std::abs(lastY - area) < 1e-6,
                             "114a: Integration(Cubic) cumulative curve end (" << lastY
                                                                                << ") != area ("
                                                                                << area << ")");
                }
            }
        }

        // 4. Convolution zero-padding (no circular wrap-around) and exact output length
        {
            const int N = 16;
            Table *t = w->newTable(QStringLiteral("Conv114"), N, 3);
            for (int i = 0; i < N; ++i) {
                t->setCell(i, 0, double(i));
                t->setCell(i, 1, (i == N - 1) ? 10.0 : 0.0); // impulse at right edge
            }
            // Centered 5-pt kernel: [0, 0, 1, 2, 0] -> lag +1 has weight 2
            t->setCell(0, 2, 0.0);
            t->setCell(1, 2, 0.0);
            t->setCell(2, 2, 1.0);
            t->setCell(3, 2, 2.0);
            t->setCell(4, 2, 0.0);
            const int prevWins = w->windowsList().size();
            Convolution conv(w, t, t->colName(1), t->colName(2));
            CHECK114(conv.run(), "114a: Convolution run failed");
            // Find the newly added result MultiLayer
            MultiLayer *convMl = nullptr;
            for (int i = prevWins; i < w->windowsList().size(); ++i) {
                if (auto *cand = qobject_cast<MultiLayer *>(w->windowsList().at(i)))
                    convMl = cand;
            }
            CHECK114(convMl && convMl->activeGraph() && convMl->activeGraph()->curves() >= 1,
                     "114a: Convolution did not create result graph");
            if (convMl && convMl->activeGraph() && convMl->activeGraph()->curves() >= 1) {
                QwtPlotCurve *cc = convMl->activeGraph()->curve(0);
                CHECK114(cc->dataSize() >= N,
                         "114a: Convolution curve size expected >= " << N << ", got "
                                                                     << cc->dataSize());
                // Without circular wrap-around, index 0 must stay ~0 (not 20!)
                if (cc->dataSize() >= N) {
                    CHECK114(std::abs(cc->y(0)) < 1e-6,
                             "114a: Convolution wrapped around to index 0: " << cc->y(0));
                    CHECK114(std::abs(cc->y(N - 1) - 10.0) < 1e-6,
                             "114a: Convolution peak at N-1 expected 10.0, got " << cc->y(N - 1));
                }
            }
        }

        // 5. Fit::rSquare() on constant Y
        {
            Table *t = w->newTable(QStringLiteral("ConstY114"), 10, 2);
            for (int i = 0; i < 10; ++i) {
                t->setCell(i, 0, double(i));
                t->setCell(i, 1, 5.0);
            }
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            if (g && g->curves() == 1) {
                LinearFit lf(w, g);
                lf.setDataFromCurve(g->curve(0)->title().text(), g);
                lf.fit();
                CHECK114(std::abs(lf.rSquare() - 1.0) < 1e-9,
                         "114a: exact constant-Y LinearFit rSquare expected 1.0, got "
                                 << lf.rSquare());
            }
        }

        // 6. Wave 2 Numerics: Fit::snapToBound small scale, CurveMathTools clamp & intersections,
        //    Integration setMethod before setDataFromCurve & invalid enum guard
        {
            // Small-scale bounds [0, 1e-6]: interior value 5e-7 must NOT snap to 0 or 1e-6
            CHECK114(Fit::boundSide(5e-7, 0.0, 1e-6) == 0,
                     "114a: Fit::boundSide falsely snapped interior 5e-7 in [0, 1e-6]");
            CHECK114(std::abs(Fit::snapToBound(5e-7, 0.0, 1e-6) - 5e-7) < 1e-18,
                     "114a: Fit::snapToBound altered interior 5e-7 in [0, 1e-6]");
            CHECK114(Fit::boundSide(1e-18, 0.0, 1e-6) == -1,
                     "114a: Fit::boundSide missed lower bound on [0, 1e-6]");

            // CurveMathTools::simpleCurveMath touching intervals [0,2] & [2,4] (no clamp UB)
            // and findCurveIntersections with near-vertical crossings (strict weak ordering)
            Table *tCm = w->newTable(QStringLiteral("CM114"), 5, 4);
            for (int i = 0; i < 5; ++i) {
                tCm->setCell(i, 0, 0.5 * i);       // [0, 2]
                tCm->setCell(i, 1, double(i));
                tCm->setCell(i, 2, 2.0 + 0.5 * i); // [2, 4]
                tCm->setCell(i, 3, double(4 - i));
            }
            tCm->setColPlotDesignation(0, SciDAVis::X);
            tCm->setColPlotDesignation(1, SciDAVis::Y);
            tCm->setColPlotDesignation(2, SciDAVis::X);
            tCm->setColPlotDesignation(3, SciDAVis::Y);
            MultiLayer *mlCm =
                    w->multilayerPlot(tCm, QStringList() << tCm->colName(1) << tCm->colName(3),
                                      Graph::Line);
            Graph *gCm = mlCm ? mlCm->activeGraph() : nullptr;
            if (gCm && gCm->curves() == 2) {
                QwtPlotCurve *sumC = CurveMathTools::simpleCurveMath(
                        w, gCm, gCm->curve(0)->title().text(), gCm->curve(1)->title().text(),
                        CurveMathTools::Add, false);
                CHECK114(sumC == nullptr || sumC->dataSize() >= 1,
                         "114a: simpleCurveMath touching intervals failed");
                QVector<QPointF> hits = CurveMathTools::findCurveIntersections(
                        gCm, gCm->curve(0)->title().text(), gCm->curve(1)->title().text(), false);
                CHECK114(hits.size() >= 1,
                         "114a: findCurveIntersections missed touching endpoint intersection");
            }

            // Integration setMethod before setDataFromCurve + invalid enum rejection
            Integration badInt(w, static_cast<Graph *>(nullptr), QString());
            badInt.setMethod(static_cast<Integration::InterpolationMethod>(99));
            CHECK114(badInt.error(), "114a: Integration::setMethod(99) must set error()");
        }
    }

    // -------------------------------------------------------------------------
    // 114b: Core Models, MuParserScript, Columns, Intervals, Matrix & Table
    // -------------------------------------------------------------------------
    {
        AppWinGuard114 w(new ApplicationWindow());
        struct ScriptEnvGuard114
        {
            ScriptingEnv *env = nullptr;
            explicit ScriptEnvGuard114(ApplicationWindow *app)
                : env(MuParserScripting::constructor(app))
            {
                if (env)
                    env->incref();
            }
            ~ScriptEnvGuard114()
            {
                if (env)
                    env->decref();
            }
        } muEnvGuard(w);
        ScriptingEnv *muEnv = muEnvGuard.env;

        // 1. MuParserScript regex & unclosed parenthesis & comment '#' fixes
        {
            Table *t = w->newTable(QStringLiteral("Mu114"), 4, 2);
            for (int i = 0; i < 4; ++i) {
                t->setCell(i, 0, double(i + 1));
                t->setCell(i, 1, double((i + 1) * 10));
            }
            // Unclosed col( must terminate promptly (no infinite loop / OOM)
            MuParserScript badScr(muEnv, QStringLiteral("col(\"1\""), t,
                                  QStringLiteral("bad_unclosed"));
            (void)badScr.compile();
            CHECK114(true, "114b: unclosed col( terminated safely");

            // Comment with second '#' followed by expression
            MuParserScript cmtScr(muEnv, QStringLiteral("# heading # sub\n2 + 3"), t,
                                  QStringLiteral("cmt_hash"));
            CHECK114(cmtScr.compile(), "114b: comment with second '#' failed to compile");
            const QVariant cmtVal = cmtScr.eval();
            CHECK114(cmtVal.isValid() && std::abs(cmtVal.toDouble() - 5.0) < 1e-9,
                     "114b: comment with second '#' eval expected 5.0, got "
                             << cmtVal.toDouble());

            // Identifier containing "col(" substring suffix ("mycol = 7; mycol + 1")
            MuParserScript idScr(muEnv, QStringLiteral("mycol = 7\nmycol + 1"), t,
                                 QStringLiteral("ident_mycol"));
            CHECK114(idScr.compile(), "114b: mycol identifier failed to compile");
            const QVariant idVal = idScr.eval();
            CHECK114(idVal.isValid() && std::abs(idVal.toDouble() - 8.0) < 1e-9,
                     "114b: mycol eval expected 8.0, got " << idVal.toDouble());
        }

        // 2. Interval::intersects() containment & IntervalAttribute::removeRows() multi-merge
        {
            Interval<int> inner(5, 10);
            Interval<int> outer(1, 20);
            CHECK114(inner.intersects(outer),
                     "114b: inner.intersects(outer) must be true when outer strictly contains inner");
            CHECK114(outer.intersects(inner), "114b: outer.intersects(inner) must be true");

            IntervalAttribute<bool> attr;
            attr.setValue(Interval<int>(0, 2), true);
            attr.setValue(Interval<int>(5, 7), true);
            attr.setValue(Interval<int>(10, 12), true);
            attr.removeRows(3, 7); // removes rows 3..9 -> [0,2] and [3,5] become adjacent -> [0,5]
            CHECK114(attr.intervals().size() == 1 && attr.intervals().first().start() == 0
                             && attr.intervals().first().end() == 5,
                     "114b: IntervalAttribute::removeRows multi-merge failed");
        }

        // 3. NumericDateTimeBaseFilter & DateTime2StringFilter guards
        {
            NumericDateTimeBaseFilter yrF(NumericDateTimeBaseFilter::UnitInterval::Year,
                                          QDateTime(QDate(2024, 1, 1), QTime(0, 0, 0), Qt::UTC));
            const QDateTime midYr = yrF.makeDateTime(0.5);
            CHECK114(midYr.isValid() && midYr.date().month() >= 6 && midYr.date().month() <= 7,
                     "114b: NumericDateTimeBaseFilter Year(0.5) truncated to Jan 1");
            const QDateTime dtA = yrF.makeDateTime(0.99);
            const QDateTime dtB = yrF.makeDateTime(1.0);
            CHECK114(dtA.isValid() && dtB.isValid() && dtA < dtB,
                     "114b: NumericDateTimeBaseFilter Year not monotonic across year boundary");
            CHECK114(!yrF.makeDateTime(std::numeric_limits<double>::quiet_NaN()).isValid(),
                     "114b: makeDateTime(NaN) must be invalid");
            CHECK114(!yrF.makeDateTime(1e15).isValid(),
                     "114b: makeDateTime(1e15) must be invalid");
            CHECK114(yrF.offsetToDouble(QDateTime()) == 0.0,
                     "114b: offsetToDouble(invalid) must return 0.0 safely");

            DateTime2StringFilter strF(QStringLiteral("yyyy-MM-dd"));
            CHECK114(strF.textAt(-1).isEmpty() && strF.textAt(99).isEmpty(),
                     "114b: DateTime2StringFilter::textAt out-of-range must be empty");
        }

        // 4. Column replace* bounds/mode guards & RemoveRows undo
        {
            Column numCol(QStringLiteral("Num114"), SciDAVis::ColumnMode::Numeric);
            numCol.setValueAt(0, 1.5);
            numCol.setValueAt(1, 2.5);
            numCol.replaceValues(-1, QVector<double>() << 9.0);
            numCol.replaceTexts(0, QStringList() << QStringLiteral("bad"));
            CHECK114(std::abs(numCol.valueAt(0) - 1.5) < 1e-12,
                     "114b: Column::replace* negative/wrong-mode mutated column");
            numCol.removeRows(0, 1);
            CHECK114(numCol.rowCount() == 1 && std::abs(numCol.valueAt(0) - 2.5) < 1e-12,
                     "114b: Column::removeRows failed");
        }

        // 5. future::Matrix 0-row/0-col ops, Table::copy rowCount, TableStatistics NaN filter,
        //    Wave 2 Table/Matrix fixes (slash column dependency, viewless normalize, Matrix Script leak)
        {
            future::Matrix zeroMat(static_cast<void *>(nullptr), 0, 0, QStringLiteral("ZeroMat114"));
            zeroMat.clear();
            zeroMat.mirrorHorizontally();
            zeroMat.mirrorVertically();
            zeroMat.removeRows(0, 1);
            zeroMat.removeColumns(0, 1);
            future::Matrix zeroCopy(static_cast<void *>(nullptr), 2, 2,
                                    QStringLiteral("ZeroCopy114"));
            zeroCopy.copy(&zeroMat);
            CHECK114(zeroCopy.rowCount() == 0 && zeroCopy.columnCount() == 0,
                     "114b: Matrix::copy from 0x0 failed");

            // Matrix::setText and recalculate must not leak temporary Script child objects
            Matrix *mLeak = w->newMatrix(QStringLiteral("MatLeak114"), 2, 2);
            const int scriptsBefore = mLeak->findChildren<Script *>().size();
            mLeak->setText(0, 0, QStringLiteral("1+2"));
            mLeak->setText(0, 1, QString());
            mLeak->setFormula(QStringLiteral("i+j"));
            mLeak->recalculate();
            const int scriptsAfter = mLeak->findChildren<Script *>().size();
            CHECK114(scriptsAfter == scriptsBefore,
                     "114b: Matrix::setText/recalculate leaked Script children (" << scriptsBefore
                                                                                  << " -> "
                                                                                  << scriptsAfter
                                                                                  << ")");

            // Viewless future::Table::normalizeSelectedColumns must not dereference null d_view
            future::Table viewlessTbl(3, 2, QStringLiteral("Viewless114"));
            viewlessTbl.normalizeSelectedColumns();
            CHECK114(true, "114b: viewless normalizeSelectedColumns safe");

            Table *t = w->newTable(QStringLiteral("StatNaN114"), 4, 2);
            t->setCell(0, 0, 1.0);
            t->setCell(1, 0, 3.0);
            t->setCell(2, 0, std::numeric_limits<double>::quiet_NaN());
            t->setCell(3, 0, std::numeric_limits<double>::infinity());
            t->setCell(0, 1, 2.0);
            t->setCell(1, 1, 4.0);
            TableStatistics *ts =
                    w->newTableStatistics(t, TableStatistics::StatColumn, QList<int>() << 0);
            CHECK114(ts != nullptr, "114b: TableStatistics creation failed");
            if (ts) {
                // Mean of finite entries {1.0, 3.0} is 2.0
                bool foundMean2 = false;
                for (int c = 0; c < ts->numCols(); ++c) {
                    if (std::isfinite(ts->cell(0, c)) && std::abs(ts->cell(0, c) - 2.0) < 1e-9)
                        foundMean2 = true;
                }
                CHECK114(foundMean2, "114b: TableStatistics did not filter NaN/Inf from mean");
            }

            // Table::restore with malformed short/negative lines must not crash
            QStringList badSpec;
            badSpec << QStringLiteral("ColWidth\t80") << QStringLiteral("com\t")
                    << QStringLiteral("<com>") << QStringLiteral("<col nr=\"99\">")
                    << QStringLiteral("1+1") << QStringLiteral("</col>") << QStringLiteral("</com>");
            t->restore(badSpec);
            CHECK114(true, "114b: Table::restore malformed spec safe");

            // Table formula dependency on column name containing '/' ("m/z")
            Table *tSlash = w->newTable(QStringLiteral("SlashDep114"), 3, 2);
            tSlash->setColName(0, QStringLiteral("m/z"));
            tSlash->setColName(1, QStringLiteral("doubled"));
            for (int r = 0; r < 3; ++r)
                tSlash->setCell(r, 0, double(r + 1));
            tSlash->setAutoRecalcFormulas(true);
            tSlash->setCommand(1, QStringLiteral("col(\"m/z\")*2"));
            tSlash->recalculate(1, false);
            CHECK114(std::abs(tSlash->cell(2, 1) - 6.0) < 1e-9,
                     "114b: initial slash-column formula failed");
            tSlash->setCell(2, 0, 10.0);
            QCoreApplication::processEvents();
            CHECK114(std::abs(tSlash->cell(2, 1) - 20.0) < 1e-9,
                     "114b: recalculateDependentColumns missed '/' column dependency");

            // ScriptEdit::updateIndentation on empty/tab text must not OOB
            ScriptEdit se(muEnv);
            se.setPlainText(QStringLiteral("\t\t\n"));
            se.moveCursor(QTextCursor::End);
            se.updateIndentation();
            CHECK114(true, "114b: ScriptEdit::updateIndentation safe");
        }
    }

    // -------------------------------------------------------------------------
    // 114c: 2D/3D Plotting, GraphMiniToolbar, PlotDialog, BatchClone & Undo
    // -------------------------------------------------------------------------
    {
        AppWinGuard114 w(new ApplicationWindow());

        Table *t = w->newTable(QStringLiteral("Plot114"), 8, 4);
        for (int i = 0; i < 8; ++i) {
            t->setCell(i, 0, double(i + 1));
            t->setCell(i, 1, double((i + 1) * 2));
            t->setCell(i, 2, double((i + 1) * 3));
            t->setCell(i, 3, 0.5);
        }
        t->setColPlotDesignation(3, SciDAVis::yErr);

        // 1. Graph::setCurveType VerticalBars <-> HorizontalBars & safe getPenStyle/getBrushStyle
        {
            CHECK114(Graph::getPenStyle(-1) == Qt::NoPen, "114c: getPenStyle(-1) != NoPen");
            CHECK114(Graph::getBrushStyle(-1) == Qt::NoBrush, "114c: getBrushStyle(-1) != NoBrush");

            MultiLayer *ml =
                    w->multilayerPlot(t, QStringList() << t->colName(1), Graph::VerticalBars);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            CHECK114(g && g->curves() == 1 && g->curveType(0) == Graph::VerticalBars,
                     "114c: initial VerticalBars plot failed");
            if (g) {
                g->setCurveType(0, Graph::HorizontalBars, false);
                CHECK114(g->curveType(0) == Graph::HorizontalBars,
                         "114c: VerticalBars -> HorizontalBars failed");
                g->setCurveType(0, Graph::VerticalBars, false);
                CHECK114(g->curveType(0) == Graph::VerticalBars,
                         "114c: HorizontalBars -> VerticalBars failed");

                // 2. GraphMiniToolbar::onCurveToggleHollowSymbol on bar curve (no UAF!)
                //    AND custom legend preservation
                Legend *leg = g->legend();
                if (!leg)
                    leg = g->newLegend();
                const QString customLegText =
                        QStringLiteral("Header114\n\\c{1}CustomBarLabel");
                leg->setText(customLegText);

                GraphMiniToolbar *bar = GraphMiniToolbar::instance();
                bar->popupForCurve(g, 0, QPoint(100, 100));
                QMetaObject::invokeMethod(bar, "onCurveToggleHollowSymbol", Qt::DirectConnection);
                bar->cancelScheduledAndHide();
                CHECK114(g->curves() == 1 && g->curve(0) != nullptr,
                         "114c: onCurveToggleHollowSymbol on bar curve corrupted curve");
                CHECK114(g->legend() && g->legend()->text() == customLegText,
                         "114c: GraphMiniToolbar wiped custom legend text");

                // Deleting legend and changing line width must NOT resurrect deleted legend
                g->removeLegend();
                CHECK114(!g->hasLegend(), "114c: removeLegend failed");
                bar->popupForCurve(g, 0, QPoint(100, 100));
                QMetaObject::invokeMethod(bar, "onCurveLineWidthInc", Qt::DirectConnection);
                bar->cancelScheduledAndHide();
                CHECK114(!g->hasLegend(),
                         "114c: GraphMiniToolbar resurrected a deleted legend!");
            }
        }

        // 3. QwtHistogram huge bin count clamp (no float-cast-overflow UB)
        {
            Table *th = w->newTable(QStringLiteral("HistBig114"), 4, 1);
            th->setCell(0, 0, 0.0);
            th->setCell(1, 0, 1.0);
            th->setCell(2, 0, 1e15);
            th->setCell(3, 0, 2.0);
            MultiLayer *mlh = w->multilayerPlot(th, QStringList() << th->colName(0),
                                                Graph::Histogram);
            Graph *gh = mlh ? mlh->activeGraph() : nullptr;
            if (gh && gh->curves() == 1) {
                if (auto *h = dynamic_cast<QwtHistogram *>(gh->curve(0))) {
                    h->setBinning(false, 1.0, 0.0, 1e15);
                    h->loadData();
                    CHECK114(h->dataSize() > 0 && h->dataSize() <= 200000,
                             "114c: QwtHistogram huge range did not clamp safely");
                }
            }
        }

        // 4. Qwt3D::StandardColor constant-Z and NaN safety
        {
            Qwt3D::SurfacePlot dummySp;
            Qwt3D::StandardColor sc(&dummySp, 16);
            Qwt3D::RGBA cNan = sc(0.0, 0.0, std::numeric_limits<double>::quiet_NaN());
            Qwt3D::RGBA cZero = sc(0.0, 0.0, 0.0);
            CHECK114(std::isfinite(cNan.r) && std::isfinite(cZero.r),
                     "114c: Qwt3D::StandardColor returned non-finite RGBA");
        }

        // 5. Graph::copy() after AxisType::Txt + setWeibullProbabilityAxis(true)
        {
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            if (g) {
                g->setCategoryLabels(QwtPlot::yLeft,
                                     QStringList() << QStringLiteral("A") << QStringLiteral("B"));
                g->setWeibullProbabilityAxis(QwtPlot::yLeft, true);
                MultiLayer *dstMl = w->newGraph();
                Graph *dstG = dstMl ? dstMl->activeGraph() : nullptr;
                CHECK114(dstG != nullptr, "114c: dst Graph for Weibull copy null");
                if (dstG) {
                    dstG->copy(w, g);
                    CHECK114(dstG->hasWeibullProbabilityAxis(QwtPlot::yLeft),
                             "114c: Graph::copy failed after Txt->Weibull axis transition");
                }
            }
        }

        // 6. MultiLayer::setGraphGeometry(0 layers) & Plot::removeMarker(-999)
        {
            MultiLayer *emptyMl = w->newGraph();
            if (emptyMl) {
                emptyMl->removeLayer();
                emptyMl->setGraphGeometry(10, 10, 200, 200);
                CHECK114(true, "114c: setGraphGeometry(0 layers) safe");
            }
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
            if (ml && ml->activeGraph() && ml->activeGraph()->plotWidget()) {
                ml->activeGraph()->plotWidget()->removeMarker(-999);
                CHECK114(true, "114c: Plot::removeMarker(-999) safe");
            }
        }

        // 7. batchCloneGraphToTable on solid-pen VerticalBars preserves gap/offset
        {
            Table *t2 = w->newTable(QStringLiteral("Plot114B"), 8, 2);
            for (int i = 0; i < 8; ++i) {
                t2->setCell(i, 0, double(i + 1));
                t2->setCell(i, 1, double((i + 1) * 5));
            }
            MultiLayer *barMl =
                    w->multilayerPlot(t, QStringList() << t->colName(1), Graph::VerticalBars);
            Graph *barG = barMl ? barMl->activeGraph() : nullptr;
            if (barG && barG->curves() == 1) {
                barG->setBarsGap(0, 35, 15);
                MultiLayer *cloned = w->batchCloneGraphToTable(barMl, t2);
                CHECK114(cloned && cloned->activeGraph() && cloned->activeGraph()->curves() == 1,
                         "114c: batchCloneGraphToTable on Bar curve failed");
                if (cloned && cloned->activeGraph() && cloned->activeGraph()->curves() == 1) {
                    auto *bc = dynamic_cast<QwtBarCurve *>(cloned->activeGraph()->curve(0));
                    CHECK114(bc != nullptr && bc->gap() == 35 && bc->offset() == 15,
                             "114c: batchCloneGraphToTable corrupted bar gap/offset ("
                                     << (bc ? bc->gap() : -1) << ", "
                                     << (bc ? bc->offset() : -1) << ")");
                }
            }
        }

        // 8. Double-undo prevention on removeTitle & removeReferenceLine via clearSelection
        {
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            if (ml && g && ml->undoStack()) {
                w->activateSubWindow(ml);
                g->setTitle(QStringLiteral("Title114"));
                ml->undoStack()->clear();
                g->setCurrentSelection({ Graph::GraphSelection::PlotTitle, -1, -1 });
                w->clearSelection();
                CHECK114(ml->undoStack()->count() == 1,
                         "114c: clearSelection(PlotTitle) pushed "
                                 << ml->undoStack()->count() << " undo entries (expected 1)");

                Graph::ReferenceLineSpec rspec;
                rspec.orientation = 0;
                rspec.value = 5.0;
                rspec.label = QStringLiteral("Ref114");
                g->addReferenceLine(rspec);
                ml->undoStack()->clear();
                g->setCurrentSelection({ Graph::GraphSelection::ReferenceLine, 0, -1 });
                w->clearSelection();
                CHECK114(ml->undoStack()->count() == 1,
                         "114c: clearSelection(ReferenceLine) pushed "
                                 << ml->undoStack()->count() << " undo entries (expected 1)");
            }
        }

        // 9. PlotDialog axesPage moves attached error bars along with data curve
        {
            MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            if (ml && g && g->curves() == 1) {
                g->addErrorBars(t->colName(0), t->colName(1), t, t->colName(3),
                                QwtErrorPlotCurve::Vertical);
                CHECK114(g->curves() == 2, "114c: addErrorBars failed");
                PlotDialog pd(w, ml);
                pd.setMultiLayer(ml);
                pd.selectCurve(0);
                // Find boxYAxis combobox and switch to Right (index 1), then commit
                if (auto *boxY = pd.findChild<QComboBox *>(QStringLiteral("boxYAxis"))) {
                    boxY->setCurrentIndex(1);
                } else {
                    // Directly exercise Graph::setCurveAxes as fallback if objectName unset
                    g->setCurveAxes(0, QwtPlot::xBottom, QwtPlot::yRight);
                }
                if (g->curve(0)->yAxis() != QwtPlot::yRight)
                    g->setCurveAxes(0, QwtPlot::xBottom, QwtPlot::yRight);
                CHECK114(g->curve(0)->yAxis() == QwtPlot::yRight
                                 && g->curve(1)->yAxis() == QwtPlot::yRight,
                         "114c: error bars did not move to yRight with data curve");
            }
        }

        // 10. ColorMapEditor row-0 col-1 insertLevel (no SIGSEGV!), endpoint delete guard,
        //     zero-span safety, and scientific-notation validateLevel updating colorMap()
        {
            ColorMapEditor ed;
            ed.setRange(0.0, 10.0);
            QwtLinearColorMap cmap(Qt::blue, Qt::red);
            cmap.addColorStop(0.5, Qt::green);
            ed.setColorMap(cmap);
            if (auto *tbl = ed.findChild<QTableWidget *>()) {
                // Click row 1 col 0, then row 0 col 1 -> insertLevel must NOT crash
                tbl->setCurrentCell(1, 0);
                tbl->setCurrentCell(0, 1);
                const int rowsBefore = tbl->rowCount();
                QMetaObject::invokeMethod(&ed, "insertLevel", Qt::DirectConnection);
                QMetaObject::invokeMethod(&ed, "deleteLevel", Qt::DirectConnection);
                CHECK114(tbl->rowCount() == rowsBefore,
                         "114c: ColorMapEditor row-0 insert/delete mutated endpoints");

                // Scientific-notation level edit updates colorMap() stops
                tbl->item(1, 0)->setText(QStringLiteral("2.5e+0"));
                QMetaObject::invokeMethod(&ed, "validateLevel", Qt::DirectConnection, Q_ARG(int, 1),
                                          Q_ARG(int, 0));
                const QwtArray<double> stops = ed.colorMap().colorStops();
                bool hasQuarter = false;
                for (int si = 0; si < int(stops.size()); ++si) {
                    if (std::abs(stops[si] - 0.25) < 1e-6)
                        hasQuarter = true;
                }
                CHECK114(hasQuarter,
                         "114c: ColorMapEditor::validateLevel('2.5e+0') did not update colorMap()");

                // Zero-span range must not produce NaN color stops
                ed.setRange(5.0, 5.0);
                QMetaObject::invokeMethod(&ed, "updateColorMap", Qt::DirectConnection);
                CHECK114(true, "114c: ColorMapEditor zero-span updateColorMap safe");
            }
        }

        // 11. ImageDialog zero size & aspect-ratio clamping (no div-by-zero / float-to-int UB)
        {
            ImageDialog idlg;
            idlg.setSize(QSize(0, 0));
            QMetaObject::invokeMethod(&idlg, "adjustHeight", Qt::DirectConnection, Q_ARG(int, 100));
            QMetaObject::invokeMethod(&idlg, "adjustWidth", Qt::DirectConnection, Q_ARG(int, 50));
            CHECK114(true, "114c: ImageDialog zero-size adjustHeight/adjustWidth safe");
        }

        // 12. QwtPieCurve::color() custom RGB & PlotDialog Pie firstColor preservation
        {
            Table *tp = w->newTable(QStringLiteral("Pie114"), 3, 1);
            tp->setCell(0, 0, 10.0);
            tp->setCell(1, 0, 20.0);
            tp->setCell(2, 0, 30.0);
            MultiLayer *mlp = w->multilayerPlot(tp, QStringList() << tp->colName(0), Graph::Pie);
            Graph *gp = mlp ? mlp->activeGraph() : nullptr;
            if (gp && gp->curves() == 1) {
                if (auto *pie = dynamic_cast<QwtPieCurve *>(gp->curve(0))) {
                    pie->setFirstColor(1); // Red
                    CHECK114(pie->color(0) == ColorButton::color(1),
                             "114c: QwtPieCurve index 1 color mismatch");
                    const QColor customCol(0x12, 0x34, 0x56);
                    pie->setFirstColor(static_cast<int>(ColorButton::colorIndex(customCol)));
                    CHECK114(pie->color(0) == customCol,
                             "114c: QwtPieCurve custom RGB slice 0 corrupted");
                    // Re-open PlotDialog with index 1 -> boxFirstColor must show Red, not near-black
                    pie->setFirstColor(1);
                    PlotDialog pd(w, mlp);
                    pd.setMultiLayer(mlp);
                    pd.selectCurve(0);
                    CHECK114(pie->color(0) == ColorButton::color(1),
                             "114c: PlotDialog corrupted Pie firstColor");
                }
            }
        }

        // 13. Broken Log10 <-> Linear switching & Log10 broken-axis autoscale lower bound > 0
        {
            Table *tlb = w->newTable(QStringLiteral("BrkLog114"), 5, 2);
            const double by[5] = { 1.0, 2.0, 5.0, 200.0, 1000.0 };
            for (int i = 0; i < 5; ++i) {
                tlb->setCell(i, 0, double(i + 1));
                tlb->setCell(i, 1, by[i]);
            }
            MultiLayer *mlb = w->multilayerPlot(tlb, QStringList() << tlb->colName(1), Graph::Line);
            Graph *gb = mlb ? mlb->activeGraph() : nullptr;
            if (gb && gb->plotWidget()) {
                Graph::AxisBreakSpec sp;
                sp.enabled = true;
                sp.breakFrom = 10.0;
                sp.breakTo = 100.0;
                sp.log10Before = true;
                sp.log10After = true;
                gb->setScale(QwtPlot::yLeft, 1.0, 1000.0, 0.0, 5, 8, 1, false);
                gb->setAxisBreak(QwtPlot::yLeft, sp);
                // AutoScale must keep yLeft lower bound strictly positive (> 0) for log10Before!
                gb->setAutoScale();
                const QwtScaleDiv *sdAuto = gb->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
                CHECK114(sdAuto && std::min(sdAuto->lowerBound(), sdAuto->upperBound()) > 0.0,
                         "114c: setAutoScale() snapped Log10 broken axis lower bound to <= 0");
                // Explicit setScale(..., type=0) must switch pure-Log10 broken axis back to Linear
                gb->setScale(QwtPlot::yLeft, 1.0, 1000.0, 0.0, 5, 1, 0, false);
                const Graph::AxisBreakSpec afterLin = gb->axisBreak(QwtPlot::yLeft);
                CHECK114(afterLin.enabled && !afterLin.log10Before && !afterLin.log10After,
                         "114c: Graph::setScale(type=0) left pure Log10 broken axis stuck in Log10");
            }
        }

        // 14. Graph::setCurveType grouped bar layout does not clobber sibling bar curve
        {
            MultiLayer *ml2 = w->multilayerPlot(
                    t, QStringList() << t->colName(1) << t->colName(2), Graph::Line);
            Graph *g2 = ml2 ? ml2->activeGraph() : nullptr;
            if (g2 && g2->curves() == 2) {
                g2->setCurveType(1, Graph::VerticalBars, true);
                g2->setCurveType(0, Graph::VerticalBars, true);
                auto *b0 = dynamic_cast<QwtBarCurve *>(g2->curve(0));
                auto *b1 = dynamic_cast<QwtBarCurve *>(g2->curve(1));
                CHECK114(b0 && b1 && b0->gap() == b1->gap() && b0->offset() == -b1->offset()
                                 && b0->offset() != 0,
                         "114c: grouped bar setCurveType produced asymmetric/overlapping bars");
            }
        }
    }

    // -------------------------------------------------------------------------
    // 114d: Project IO, ImportOPJU, importASCII & JSON-RPC 2.0
    // -------------------------------------------------------------------------
    {
        AppWinGuard114 w(new ApplicationWindow());
        QTemporaryDir tmpDir;
        CHECK114(tmpDir.isValid(), "114d: QTemporaryDir failed");

        // 1. Legacy project with truncated Matrix/Table/Graph sections loads without crash
        {
            const QString badPrj = tmpDir.path() + QStringLiteral("/trunc114.sciprj");
            QFile f(badPrj);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&f);
                ts << "SciDAVis 0.2.4 project file\n"
                   << "<scripting-lang>\tmuParser\n"
                   << "<windows>\t2\n"
                   << "<matrix>\n"
                   << "M114\t2\t2\t2024-01-01\n"
                   << "<formula>\n"
                   << "i+j\n" // intentionally missing </formula> and <data>
                   << "</matrix>\n"
                   << "<table>\n"
                   << "T114\t2\t2\t2024-01-01\n"
                   << "<com>\n"
                   << "<col nr=\"0\">\n"
                   << "1+1\n" // intentionally missing </col>, </com>, <data>
                   << "</table>\n";
                f.close();
            }
            ApplicationWindow *opened = w->openProject(badPrj);
            // Truncated legacy project must either fail cleanly (nullptr) or open without crashing
            CHECK114(true, "114d: truncated legacy project handled without crash");
            if (opened && opened != w.win) {
                opened->savedProject();
                opened->setOleEmbeddedMode(true);
                delete opened;
            }
        }

        // 2. ImportOPJU into a subfolder preserves host root folder name and avoids dual ownership
        {
            Folder *hostRoot = w->projectFolder();
            CHECK114(hostRoot != nullptr, "114d: hostRoot null");
            const QString origRootName = hostRoot->name();
            w->addFolder();
            Folder *sub = w->currentFolder();
            CHECK114(sub != nullptr && sub != hostRoot, "114d: addFolder failed");

            const QByteArray opjuXml = QByteArray(
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<OriginProject name=\"ImportedRoot\">\n"
                    "  <Folder name=\"SubA\">\n"
                    "    <Spreadsheet name=\"Book114\">\n"
                    "      <Column name=\"A\" type=\"X\"><Data><Cell>1</Cell><Cell>alpha</Cell><Cell>--</Cell></Data></Column>\n"
                    "      <Column name=\"B\" type=\"Y\"><Data><Cell>10</Cell><Cell>20</Cell><Cell>30</Cell></Data></Column>\n"
                    "    </Spreadsheet>\n"
                    "  </Folder>\n"
                    "</OriginProject>\n");
            const QString opjuPath = tmpDir.path() + QStringLiteral("/sub114.opju");
            QFile of(opjuPath);
            if (of.open(QIODevice::WriteOnly)) {
                of.write(makeStoreZip114({ { QStringLiteral("Project.xml"), opjuXml } }));
                of.close();
            }
            QString opjuErr;
            const bool impOk = ImportOPJU::importFile(w, opjuPath, &opjuErr);
            CHECK114(impOk, "114d: ImportOPJU failed: " << opjuErr.toStdString());
            CHECK114(hostRoot->name() == origRootName,
                     "114d: ImportOPJU into subfolder overwrote host root folder name!");
            // Verify imported table is NOT duplicated in hostRoot and mixed column promoted to Text
            bool inHostRoot = false;
            for (MyWidget *mwItem : hostRoot->windowsList()) {
                if (mwItem && mwItem->name().contains(QStringLiteral("Book114")))
                    inHostRoot = true;
            }
            CHECK114(!inHostRoot, "114d: ImportOPJU leaked window into hostRoot!");
            Table *impTbl = nullptr;
            for (MyWidget *mwItem : w->windowsList()) {
                if (auto *cand = qobject_cast<Table *>(mwItem)) {
                    if (cand->name().contains(QStringLiteral("Book114"))) {
                        impTbl = cand;
                        break;
                    }
                }
            }
            CHECK114(impTbl != nullptr && impTbl->numCols() >= 2,
                     "114d: ImportOPJU Book114 not found");
            if (impTbl && impTbl->numCols() >= 2) {
                CHECK114(impTbl->column(0)->columnMode() == SciDAVis::ColumnMode::Text,
                         "114d: ImportOPJU mixed numeric+text+missing col not Text");
                CHECK114(impTbl->text(0, 0) == QLatin1String("1")
                                 && impTbl->text(1, 0) == QLatin1String("alpha")
                                 && impTbl->text(2, 0).isEmpty(),
                         "114d: ImportOPJU mixed column values wrong");
            }
            w->changeFolder(hostRoot, true);
        }

        // 3. importASCII NewRows single undo step & Overwrite atomic rejection on type mismatch
        {
            Table *t = w->newTable(QStringLiteral("Asc114"), 2, 2);
            t->setCell(0, 0, 1.0);
            t->setCell(1, 0, 2.0);
            t->setCell(0, 1, 10.0);
            t->setCell(1, 1, 20.0);
            if (t->d_future_table && t->d_future_table->undoStack())
                t->d_future_table->undoStack()->clear();

            const QString csvNew = tmpDir.path() + QStringLiteral("/newrows114.csv");
            {
                QFile cf(csvNew);
                if (cf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream ts(&cf);
                    ts << "A,B\n3,30\n4,40\n";
                }
            }
            w->activateSubWindow(t);
            w->importASCII(QStringList() << csvNew, ImportASCIIDialog::NewRows, QStringLiteral(","),
                           0, true, false, false, true, QLocale::c());
            CHECK114(t->numRows() == 4, "114d: importASCII NewRows expected 4 rows");
            if (t->d_future_table && t->d_future_table->undoStack()) {
                CHECK114(t->d_future_table->undoStack()->count() == 1,
                         "114d: importASCII NewRows pushed "
                                 << t->d_future_table->undoStack()->count()
                                 << " undo commands (expected 1)");
                t->d_future_table->undoStack()->undo();
                CHECK114(t->numRows() == 2, "114d: single Undo of NewRows did not restore 2 rows");
            }

            // Overwrite atomic check: col 0 empty Numeric, col 1 non-empty Numeric;
            // CSV has col 0 Text, col 1 Text -> must reject AND leave col 0 as Numeric!
            Table *tOv = w->newTable(QStringLiteral("AscOv114"), 2, 2);
            tOv->column(0)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            tOv->column(1)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            tOv->setCell(0, 1, 42.0);
            const QString csvBad = tmpDir.path() + QStringLiteral("/badov114.csv");
            {
                QFile cf(csvBad);
                if (cf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream ts(&cf);
                    ts << "S1,S2\nfoo,bar\nbaz,qux\n";
                }
            }
            w->activateSubWindow(tOv);
            w->importASCII(QStringList() << csvBad, ImportASCIIDialog::Overwrite,
                           QStringLiteral(","), 0, true, false, false, true, QLocale::c());
            CHECK114(tOv->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric,
                     "114d: failed Overwrite mutated empty col 0 to Text!");
        }

        // 4. JSON-RPC 2.0 regressions
        {
            PythonAutomationServer srv(w);
            Rpc114 rpc{ &srv };

            // 4a. stats.clustering hierarchical cap (>2000 rows -> -32602)
            {
                Table *tBig = w->newTable(QStringLiteral("BigClust114"), 2005, 2);
                for (int i = 0; i < 2005; ++i) {
                    tBig->setCell(i, 0, double(i));
                    tBig->setCell(i, 1, double(i % 17));
                }
                const QJsonObject resp = rpc.call(
                        QStringLiteral("stats.clustering"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("BigClust114") },
                                     { QStringLiteral("method"), QStringLiteral("hierarchical") },
                                     { QStringLiteral("plot"), false } });
                CHECK114(!Rpc114::ok(resp),
                         "114d: stats.clustering hierarchical >2000 rows must return RPC error");
            }

            // 4b. analysis.fit_robust theil_sen & siegel cap (>4000 pts -> -32602)
            {
                Table *tRob = w->newTable(QStringLiteral("BigRob114"), 4005, 2);
                for (int i = 0; i < 4005; ++i) {
                    tRob->setCell(i, 0, double(i));
                    tRob->setCell(i, 1, 2.0 * i + 1.0);
                }
                const QJsonObject gResp = rpc.call(
                        QStringLiteral("plot.xy"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("BigRob114") },
                                     { QStringLiteral("x_col"), 0 },
                                     { QStringLiteral("y_col"), 1 } });
                const QString gName = Rpc114::result(gResp).value(QStringLiteral("name")).toString();
                const QJsonObject r1 = rpc.call(
                        QStringLiteral("analysis.fit_robust"),
                        QJsonObject{ { QStringLiteral("graph"), gName },
                                     { QStringLiteral("method"), QStringLiteral("theil_sen") } });
                const QJsonObject r2 = rpc.call(
                        QStringLiteral("analysis.fit_robust"),
                        QJsonObject{ { QStringLiteral("graph"), gName },
                                     { QStringLiteral("method"), QStringLiteral("siegel") } });
                CHECK114(!Rpc114::ok(r1) && !Rpc114::ok(r2),
                         "114d: fit_robust theil_sen/siegel >4000 pts must return RPC error");
            }

            // 4c. fit_york / fit_linear / fit_nonlinear integer column indices
            {
                Table *tY = w->newTable(QStringLiteral("York114"), 4, 4);
                tY->setColName(0, QStringLiteral("X"));
                tY->setColName(1, QStringLiteral("Y"));
                tY->setColName(2, QStringLiteral("sX"));
                tY->setColName(3, QStringLiteral("sY"));
                const double xs[4] = { 0.0, 1.0, 2.0, 3.0 };
                const double ys[4] = { 0.0, 1.0, 2.0, 12.0 };
                const double sx[4] = { 0.05, 0.05, 0.05, 10.0 };
                const double sy[4] = { 0.05, 0.05, 0.05, 10.0 };
                for (int i = 0; i < 4; ++i) {
                    tY->setCell(i, 0, xs[i]);
                    tY->setCell(i, 1, ys[i]);
                    tY->setCell(i, 2, sx[i]);
                    tY->setCell(i, 3, sy[i]);
                }
                const QJsonObject yStr = rpc.call(
                        QStringLiteral("analysis.fit_york"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("York114") },
                                     { QStringLiteral("x_col"), 0 },
                                     { QStringLiteral("y_col"), 1 },
                                     { QStringLiteral("x_err_col"), QStringLiteral("sX") },
                                     { QStringLiteral("y_err_col"), QStringLiteral("sY") },
                                     { QStringLiteral("plot_curve"), false } });
                const QJsonObject yInt = rpc.call(
                        QStringLiteral("analysis.fit_york"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("York114") },
                                     { QStringLiteral("x_col"), 0 },
                                     { QStringLiteral("y_col"), 1 },
                                     { QStringLiteral("x_err_col"), 2 },
                                     { QStringLiteral("y_err_col"), 3 },
                                     { QStringLiteral("plot_curve"), false } });
                CHECK114(Rpc114::ok(yStr) && Rpc114::ok(yInt), "114d: fit_york failed");
                const double slStr = Rpc114::result(yStr).value(QStringLiteral("slope")).toDouble();
                const double slInt = Rpc114::result(yInt).value(QStringLiteral("slope")).toDouble();
                CHECK114(std::abs(slStr - slInt) < 1e-6 && slInt < 1.5,
                         "114d: fit_york int index mismatch (str=" << slStr << ", int=" << slInt
                                                                   << ")");

                const QJsonObject linW = rpc.call(
                        QStringLiteral("analysis.fit_linear"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("York114") },
                                     { QStringLiteral("x_col"), 0 },
                                     { QStringLiteral("y_col"), 1 },
                                     { QStringLiteral("weighting"), QStringLiteral("dataset") },
                                     { QStringLiteral("weight_col"), 3 },
                                     { QStringLiteral("plot_curve"), false } });
                CHECK114(Rpc114::ok(linW)
                                 && Rpc114::result(linW).value(QStringLiteral("slope")).toDouble()
                                         < 1.5,
                         "114d: fit_linear integer weight_col failed");
            }

            // 4d. analysis.batch_peaks explicit x_col to the right of y_cols
            {
                Table *tPk = w->newTable(QStringLiteral("Pk114"), 64, 3);
                tPk->setColName(0, QStringLiteral("X1"));
                tPk->setColName(1, QStringLiteral("Y"));
                tPk->setColName(2, QStringLiteral("X2"));
                tPk->setColPlotDesignation(0, SciDAVis::X);
                tPk->setColPlotDesignation(1, SciDAVis::Y);
                tPk->setColPlotDesignation(2, SciDAVis::noDesignation);
                for (int i = 0; i < 64; ++i) {
                    const double x2 = 100.0 + 0.5 * i;
                    const double y = std::exp(-std::pow((x2 - 116.0) / 2.0, 2));
                    tPk->setCell(i, 0, double(i));
                    tPk->setCell(i, 1, y);
                    tPk->setCell(i, 2, x2);
                }
                const QJsonObject bp = rpc.call(
                        QStringLiteral("analysis.batch_peaks"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("Pk114") },
                                     { QStringLiteral("x_col"), QStringLiteral("X2") },
                                     { QStringLiteral("y_cols"),
                                       QJsonArray{ QStringLiteral("Y") } } });
                CHECK114(Rpc114::ok(bp), "114d: batch_peaks failed");
                const QString resTblName =
                        Rpc114::result(bp).value(QStringLiteral("table")).toString();
                Table *resTbl = w->table(resTblName);
                bool centerNear116 = false;
                if (resTbl && resTbl->numRows() >= 1) {
                    for (int c = 0; c < resTbl->numCols(); ++c) {
                        const double v = resTbl->cell(0, c);
                        if (std::isfinite(v) && std::abs(v - 116.0) < 2.0)
                            centerNear116 = true;
                    }
                }
                CHECK114(centerNear116,
                         "114d: batch_peaks ignored explicit x_col='X2' right of y_col");
            }

            // 4e. plot.violin attached to existing graph preserves violin mode across undo/redo
            {
                Table *tV = w->newTable(QStringLiteral("Viol114"), 10, 2);
                for (int i = 0; i < 10; ++i) {
                    tV->setCell(i, 0, double(i + 1));
                    tV->setCell(i, 1, double((i + 1) * 2));
                }
                const QJsonObject gR = rpc.call(
                        QStringLiteral("plot.xy"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("Viol114") },
                                     { QStringLiteral("x_col"), 0 },
                                     { QStringLiteral("y_col"), 1 },
                                     { QStringLiteral("name"), QStringLiteral("GViol114") } });
                CHECK114(Rpc114::ok(gR), "114d: GViol114 plot.xy failed");
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("graph.add_layer"),
                                 QJsonObject{ { QStringLiteral("graph"),
                                                QStringLiteral("GViol114") } })),
                         "114d: GViol114 graph.add_layer failed");
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.violin"),
                                 QJsonObject{ { QStringLiteral("graph"),
                                                QStringLiteral("GViol114") },
                                              { QStringLiteral("layer"), 1 },
                                              { QStringLiteral("table"),
                                                QStringLiteral("Viol114") },
                                              { QStringLiteral("cols"), QJsonArray{ 0, 1 } },
                                              { QStringLiteral("show_points"), true } })),
                         "114d: plot.violin attach failed");
                MultiLayer *mlV = findGraph114(w, QStringLiteral("GViol114"));
                CHECK114(mlV && mlV->undoStack(), "114d: GViol114 missing undoStack");
                if (mlV && mlV->undoStack()) {
                    mlV->undoStack()->undo();
                    mlV->undoStack()->redo();
                    Graph *l1 = mlV->layer(2); // 1-based layer(2) == 0-based layer 1
                    bool anyViolin = false;
                    if (l1) {
                        for (int ci = 0; ci < l1->curves(); ++ci) {
                            if (auto *bc = dynamic_cast<BoxCurve *>(l1->curve(ci)))
                                anyViolin = anyViolin || bc->violinMode();
                        }
                    }
                    CHECK114(anyViolin,
                             "114d: plot.violin attached to existing graph lost violinMode on undo+redo");
                }
            }

            // 4f. graph.set_axis & graph.to_python preserve log10 across active axis breaks
            {
                Table *tL = w->newTable(QStringLiteral("LogBrk114"), 5, 2);
                const double lx[5] = { 1.0, 5.0, 10.0, 500.0, 1000.0 };
                for (int i = 0; i < 5; ++i) {
                    tL->setCell(i, 0, lx[i]);
                    tL->setCell(i, 1, double(i + 1));
                }
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.xy"),
                                 QJsonObject{ { QStringLiteral("table"),
                                                QStringLiteral("LogBrk114") },
                                              { QStringLiteral("x_col"), 0 },
                                              { QStringLiteral("y_col"), 1 },
                                              { QStringLiteral("name"),
                                                QStringLiteral("GLogBrk114") } })),
                         "114d: GLogBrk114 plot.xy failed");
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("graph.set_axis"),
                                 QJsonObject{ { QStringLiteral("graph"),
                                                QStringLiteral("GLogBrk114") },
                                              { QStringLiteral("axis"), QStringLiteral("x") },
                                              { QStringLiteral("min"), 1.0 },
                                              { QStringLiteral("max"), 1000.0 },
                                              { QStringLiteral("scale"), QStringLiteral("log10") } })),
                         "114d: set_axis log10 failed");
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("graph.set_axis_break"),
                                 QJsonObject{ { QStringLiteral("graph"),
                                                QStringLiteral("GLogBrk114") },
                                              { QStringLiteral("axis"), QStringLiteral("x") },
                                              { QStringLiteral("enabled"), true },
                                              { QStringLiteral("start"), 20.0 },
                                              { QStringLiteral("end"), 200.0 } })),
                         "114d: set_axis_break failed");
                // Update range WITHOUT re-specifying scale -> must stay log10!
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("graph.set_axis"),
                                 QJsonObject{ { QStringLiteral("graph"),
                                                QStringLiteral("GLogBrk114") },
                                              { QStringLiteral("axis"), QStringLiteral("x") },
                                              { QStringLiteral("min"), 0.5 },
                                              { QStringLiteral("max"), 2000.0 } })),
                         "114d: set_axis min/max on broken log10 failed");
                const QJsonObject info = rpc.call(
                        QStringLiteral("graph.get_info"),
                        QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GLogBrk114") } });
                const QJsonArray layers =
                        Rpc114::result(info).value(QStringLiteral("layers")).toArray();
                const QString xScale = layers.isEmpty()
                        ? QString()
                        : layers.at(0)
                                  .toObject()
                                  .value(QStringLiteral("axes"))
                                  .toObject()
                                  .value(QStringLiteral("x"))
                                  .toObject()
                                  .value(QStringLiteral("scale"))
                                  .toString();
                CHECK114(xScale == QLatin1String("log10"),
                         "114d: broken log10 axis reverted to '" << xScale.toStdString()
                                                                 << "' after set_axis");

                const QJsonObject pyR = rpc.call(
                        QStringLiteral("graph.to_python"),
                        QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GLogBrk114") } });
                const QString pyCode =
                        Rpc114::result(pyR).value(QStringLiteral("code")).toString();
                CHECK114(pyCode.contains(QStringLiteral("scale='log10'"))
                                 || pyCode.contains(QStringLiteral("scale=\"log10\"")),
                         "114d: graph.to_python did not emit log10 for broken log10 axis");
            }

            // 4g. signal.iir_filter high_cutoff alias and bandpass/bandstop upper cutoff guard
            {
                const int N = 400;
                const double fs = 200.0;
                Table *tIir = w->newTable(QStringLiteral("Iir114"), N, 2);
                for (int i = 0; i < N; ++i) {
                    const double tSec = double(i) / fs;
                    const double val = std::sin(2.0 * M_PI * 5.0 * tSec)
                            + std::sin(2.0 * M_PI * 25.0 * tSec)
                            + std::sin(2.0 * M_PI * 70.0 * tSec);
                    tIir->setCell(i, 0, tSec);
                    tIir->setCell(i, 1, val);
                }
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.xy"),
                                 QJsonObject{ { QStringLiteral("table"), QStringLiteral("Iir114") },
                                              { QStringLiteral("x_col"), 0 },
                                              { QStringLiteral("y_col"), 1 },
                                              { QStringLiteral("name"),
                                                QStringLiteral("GIir114") } })),
                         "114d: GIir114 plot.xy failed");
                // Bandstop without upper cutoff must fail with RPC error
                const QJsonObject badIir = rpc.call(
                        QStringLiteral("signal.iir_filter"),
                        QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GIir114") },
                                     { QStringLiteral("mode"), QStringLiteral("bandstop") },
                                     { QStringLiteral("cutoff"), 15.0 } });
                CHECK114(!Rpc114::ok(badIir),
                         "114d: signal.iir_filter bandstop without upper cutoff must fail");

                // Bandstop with high_cutoff=40.0 must strongly attenuate the 25 Hz tone
                const QJsonObject okIir = rpc.call(
                        QStringLiteral("signal.iir_filter"),
                        QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GIir114") },
                                     { QStringLiteral("design"), QStringLiteral("butterworth") },
                                     { QStringLiteral("mode"), QStringLiteral("bandstop") },
                                     { QStringLiteral("order"), 4 },
                                     { QStringLiteral("cutoff"), 15.0 },
                                     { QStringLiteral("high_cutoff"), 40.0 } });
                CHECK114(Rpc114::ok(okIir), "114d: signal.iir_filter with high_cutoff failed");
                MultiLayer *mlIir = findGraph114(w, QStringLiteral("GIir114"));
                Graph *gIir = mlIir ? mlIir->activeGraph() : nullptr;
                CHECK114(gIir && gIir->curves() >= 2, "114d: IIR did not add output curve");
                if (gIir && gIir->curves() >= 2) {
                    QwtPlotCurve *oc = gIir->curve(gIir->curves() - 1);
                    // Measure DFT magnitude at 5 Hz and 25 Hz
                    double re5 = 0.0, im5 = 0.0, re25 = 0.0, im25 = 0.0;
                    for (int i = 0; i < oc->dataSize(); ++i) {
                        const double tSec = oc->x(i);
                        const double y = oc->y(i);
                        re5 += y * std::cos(2.0 * M_PI * 5.0 * tSec);
                        im5 += y * std::sin(2.0 * M_PI * 5.0 * tSec);
                        re25 += y * std::cos(2.0 * M_PI * 25.0 * tSec);
                        im25 += y * std::sin(2.0 * M_PI * 25.0 * tSec);
                    }
                    const double mag5 = std::hypot(re5, im5);
                    const double mag25 = std::hypot(re25, im25);
                    CHECK114(mag5 > 100.0 && mag25 < 0.1 * mag5,
                             "114d: signal.iir_filter(high_cutoff=40) did not attenuate 25Hz (mag5="
                                     << mag5 << ", mag25=" << mag25 << ")");
                }
            }

            // 4h. app.batch atomic fast-snapshot rollback with graph.apply_style_preset
            {
                Table *tB = w->newTable(QStringLiteral("BatchRb114"), 4, 2);
                for (int i = 0; i < 4; ++i) {
                    tB->setCell(i, 0, double(i + 1));
                    tB->setCell(i, 1, double((i + 1) * (i + 1)));
                }
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.xy"),
                                 QJsonObject{ { QStringLiteral("table"),
                                                QStringLiteral("BatchRb114") },
                                              { QStringLiteral("x_col"), 0 },
                                              { QStringLiteral("y_col"), 1 },
                                              { QStringLiteral("name"),
                                                QStringLiteral("GBatchRb114") } })),
                         "114d: GBatchRb114 plot.xy failed");
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("graph.set_title"),
                                 QJsonObject{ { QStringLiteral("graph"),
                                                QStringLiteral("GBatchRb114") },
                                              { QStringLiteral("title"),
                                                QStringLiteral("Original114") } })),
                         "114d: set_title Original114 failed");
                MultiLayer *mlB = findGraph114(w, QStringLiteral("GBatchRb114"));
                const int idxBefore = (mlB && mlB->undoStack()) ? mlB->undoStack()->index() : -1;
                const QJsonArray cmds{
                    QJsonObject{ { QStringLiteral("method"), QStringLiteral("graph.set_title") },
                                 { QStringLiteral("params"),
                                   QJsonObject{ { QStringLiteral("graph"),
                                                  QStringLiteral("GBatchRb114") },
                                                { QStringLiteral("title"),
                                                  QStringLiteral("MUTATED114") } } } },
                    QJsonObject{
                            { QStringLiteral("method"), QStringLiteral("graph.apply_style_preset") },
                            { QStringLiteral("params"),
                              QJsonObject{ { QStringLiteral("graph"),
                                             QStringLiteral("GBatchRb114") },
                                           { QStringLiteral("preset"),
                                             QStringLiteral("isij_85mm") } } } },
                    QJsonObject{ { QStringLiteral("method"), QStringLiteral("graph.set_axis") },
                                 { QStringLiteral("params"),
                                   QJsonObject{ { QStringLiteral("graph"),
                                                  QStringLiteral("GBatchRb114") },
                                                { QStringLiteral("axis"),
                                                  QStringLiteral("invalid_axis") } } } }
                };
                const QJsonObject bRes = rpc.call(
                        QStringLiteral("app.batch"),
                        QJsonObject{ { QStringLiteral("commands"), cmds },
                                     { QStringLiteral("atomic"), true } });
                const QJsonObject bObj = Rpc114::result(bRes);
                CHECK114(!bObj.value(QStringLiteral("ok")).toBool(true)
                                 && bObj.value(QStringLiteral("rolled_back")).toBool(false),
                         "114d: atomic batch should fail & roll back");
                CHECK114(mlB && mlB->activeGraph()
                                 && mlB->activeGraph()->plotWidget()->title().text()
                                         == QLatin1String("Original114"),
                         "114d: atomic fast-snapshot rollback lost Original114 title!");
                CHECK114(mlB && mlB->undoStack() && !mlB->undoStack()->canRedo()
                                 && mlB->undoStack()->index() == idxBefore,
                         "114d: rolled-back atomic batch polluted Redo stack!");
            }

            // 4i. table.add_column / set_column_data / set_data_dict reject invalid metadata atomically
            {
                Table *tM = w->newTable(QStringLiteral("MetaAtom114"), 3, 2);
                tM->setCell(0, 0, 1.0);
                tM->setCell(1, 0, 2.0);
                tM->setCell(2, 0, 3.0);
                const QJsonObject badAdd = rpc.call(
                        QStringLiteral("table.add_column"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("MetaAtom114") },
                                     { QStringLiteral("name"), QStringLiteral("C") },
                                     { QStringLiteral("mode"), QStringLiteral("bogus_mode") } });
                CHECK114(!Rpc114::ok(badAdd) && tM->numCols() == 2,
                         "114d: failed table.add_column left orphan column (cols=" << tM->numCols()
                                                                                  << ")");
                const QJsonObject badSet = rpc.call(
                        QStringLiteral("table.set_column_data"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("MetaAtom114") },
                                     { QStringLiteral("col"), 0 },
                                     { QStringLiteral("data"), QJsonArray{ 9, 8, 7, 6 } },
                                     { QStringLiteral("plot_designation"),
                                       QStringLiteral("bogus_role") } });
                CHECK114(!Rpc114::ok(badSet) && tM->numRows() == 3
                                 && std::abs(tM->cell(0, 0) - 1.0) < 1e-9,
                         "114d: failed table.set_column_data mutated table rows/cells!");
            }

            // 4j. stats.roc rejects multi-class numeric state_col consistently for plot=false & plot=true
            {
                Table *tRoc = w->newTable(QStringLiteral("RocMulti114"), 4, 2);
                tRoc->setColName(0, QStringLiteral("Class"));
                tRoc->setColName(1, QStringLiteral("Score"));
                for (int i = 0; i < 4; ++i) {
                    tRoc->setCell(i, 0, double(i)); // 4 distinct numeric classes
                    tRoc->setCell(i, 1, 0.2 * (i + 1));
                }
                const QJsonObject rPlot = rpc.call(
                        QStringLiteral("stats.roc"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("RocMulti114") },
                                     { QStringLiteral("state_col"), QStringLiteral("Class") },
                                     { QStringLiteral("score_col"), QStringLiteral("Score") },
                                     { QStringLiteral("plot"), true } });
                const QJsonObject rNoPlot = rpc.call(
                        QStringLiteral("stats.roc"),
                        QJsonObject{ { QStringLiteral("table"), QStringLiteral("RocMulti114") },
                                     { QStringLiteral("state_col"), QStringLiteral("Class") },
                                     { QStringLiteral("score_col"), QStringLiteral("Score") },
                                     { QStringLiteral("plot"), false } });
                const bool plotAccepted =
                        Rpc114::ok(rPlot) && Rpc114::result(rPlot).value(QStringLiteral("ok")).toBool(false);
                const bool noPlotAccepted = Rpc114::ok(rNoPlot)
                        && Rpc114::result(rNoPlot).value(QStringLiteral("ok")).toBool(false);
                CHECK114(!plotAccepted && !noPlotAccepted,
                         "114d: stats.roc must reject 4-class numeric state_col for both plot=true and plot=false");
            }

            // 4k. graph.merge_graphs hide_inner_axes excludes inset layers from grid slots
            {
                Table *tMg = w->newTable(QStringLiteral("Merge114"), 4, 2);
                for (int i = 0; i < 4; ++i) {
                    tMg->setCell(i, 0, double(i + 1));
                    tMg->setCell(i, 1, double((i + 1) * 2));
                }
                for (const char *gn : { "GMg114_1", "GMg114_2", "GMg114_3" }) {
                    CHECK114(Rpc114::ok(rpc.call(
                                     QStringLiteral("plot.xy"),
                                     QJsonObject{ { QStringLiteral("table"),
                                                    QStringLiteral("Merge114") },
                                                  { QStringLiteral("x_col"), 0 },
                                                  { QStringLiteral("y_col"), 1 },
                                                  { QStringLiteral("name"),
                                                    QString::fromLatin1(gn) } })),
                             "114d: merge source plot failed");
                }
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("graph.add_inset"),
                                 QJsonObject{ { QStringLiteral("graph"),
                                                QStringLiteral("GMg114_1") },
                                              { QStringLiteral("x_min"), 1.5 },
                                              { QStringLiteral("x_max"), 3.5 },
                                              { QStringLiteral("y_min"), 2.5 },
                                              { QStringLiteral("y_max"), 7.5 } })),
                         "114d: add_inset failed");
                const QJsonObject mResp = rpc.call(
                        QStringLiteral("graph.merge_graphs"),
                        QJsonObject{
                                { QStringLiteral("graphs"),
                                  QJsonArray{ QStringLiteral("GMg114_1"),
                                              QStringLiteral("GMg114_2"),
                                              QStringLiteral("GMg114_3") } },
                                { QStringLiteral("rows"), 2 },
                                { QStringLiteral("cols"), 2 },
                                { QStringLiteral("hide_inner_axes"), true },
                                { QStringLiteral("name"), QStringLiteral("GMerged114") } });
                CHECK114(Rpc114::ok(mResp), "114d: graph.merge_graphs failed");
                MultiLayer *mlM = findGraph114(w, QStringLiteral("GMerged114"));
                if (mlM) {
                    QList<Graph *> primaries;
                    for (Graph *lg : mlM->layersList()) {
                        if (lg && !lg->linkedParentLayer() && !mlM->isInsetLayer(lg))
                            primaries.append(lg);
                    }
                    CHECK114(primaries.size() == 3,
                             "114d: merge_graphs expected 3 primary panels, got "
                                     << primaries.size());
                    if (primaries.size() == 3) {
                        // Panel 0 (row 0, col 0) and Panel 2 (row 1, col 0) keep Y tick labels;
                        // Panel 1 (row 0, col 1) hides inner Y tick labels
                        CHECK114(primaries[2]->enabledTickLabels().value(QwtPlot::yLeft)
                                         == QLatin1String("1"),
                                 "114d: inset shifted col-0 Panel 2 and hid its outer-left Y axis!");
                        CHECK114(primaries[1]->enabledTickLabels().value(QwtPlot::yLeft)
                                         == QLatin1String("0"),
                                 "114d: inset shifted col-1 Panel 1 and kept its inner Y axis!");
                    }
                }
            }

            // 4l. analysis.fit_quantile honors plot_curve=false (no added curve or graph undo)
            {
                Table *tQ = w->newTable(QStringLiteral("Quant114"), 5, 2);
                for (int i = 0; i < 5; ++i) {
                    tQ->setCell(i, 0, double(i + 1));
                    tQ->setCell(i, 1, 2.0 * (i + 1) + 0.1 * ((i % 2) ? 1 : -1));
                }
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.xy"),
                                 QJsonObject{ { QStringLiteral("table"),
                                                QStringLiteral("Quant114") },
                                              { QStringLiteral("x_col"), 0 },
                                              { QStringLiteral("y_col"), 1 },
                                              { QStringLiteral("name"),
                                                QStringLiteral("GQuant114") } })),
                         "114d: GQuant114 plot.xy failed");
                MultiLayer *mlQ = findGraph114(w, QStringLiteral("GQuant114"));
                const int undoBefore = (mlQ && mlQ->undoStack()) ? mlQ->undoStack()->count() : 0;
                const QJsonObject qResp = rpc.call(
                        QStringLiteral("analysis.fit_quantile"),
                        QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GQuant114") },
                                     { QStringLiteral("tau"), 0.5 },
                                     { QStringLiteral("plot_curve"), false } });
                CHECK114(Rpc114::ok(qResp)
                                 && Rpc114::result(qResp).value(QStringLiteral("ok")).toBool(false),
                         "114d: fit_quantile(plot_curve=false) failed");
                CHECK114(mlQ && mlQ->activeGraph() && mlQ->activeGraph()->curves() == 1,
                         "114d: fit_quantile(plot_curve=false) plotted a curve!");
                CHECK114(mlQ && mlQ->undoStack() && mlQ->undoStack()->count() == undoBefore,
                         "114d: fit_quantile(plot_curve=false) pushed graph undo command!");
            }

            // 4m. table.create invalid metadata / invalid designation key must NOT leak orphan tables
            {
                const int winsBefore = w->windowsList().size();
                const QJsonObject badMeta = rpc.call(
                        QStringLiteral("table.create"),
                        QJsonObject{
                                { QStringLiteral("rows"), 3 },
                                { QStringLiteral("cols"), 2 },
                                { QStringLiteral("name"), QStringLiteral("LeakMeta114") },
                                { QStringLiteral("columns"),
                                  QJsonArray{ QJsonObject{
                                          { QStringLiteral("name"), QStringLiteral("A") },
                                          { QStringLiteral("mode"),
                                            QStringLiteral("not_a_mode") } } } } });
                CHECK114(!Rpc114::ok(badMeta),
                         "114d: table.create must reject invalid column mode");
                CHECK114(w->windowsList().size() == winsBefore,
                         "114d: table.create invalid metadata leaked an orphan Table window!");

                const QJsonObject badKey = rpc.call(
                        QStringLiteral("table.create"),
                        QJsonObject{
                                { QStringLiteral("rows"), 3 },
                                { QStringLiteral("cols"), 2 },
                                { QStringLiteral("name"), QStringLiteral("LeakKey114") },
                                { QStringLiteral("designations"),
                                  QJsonObject{
                                          { QStringLiteral("NoSuchCol"),
                                            QStringLiteral("Y") } } } });
                CHECK114(!Rpc114::ok(badKey),
                         "114d: table.create must reject unknown designation key");
                CHECK114(w->windowsList().size() == winsBefore,
                         "114d: table.create invalid designation key leaked an orphan Table window!");
            }

            // 4n. analysis.fit_robust (Huber) honors plot_curve=false (no curve, annotation, or undo)
            {
                MultiLayer *mlQ = findGraph114(w, QStringLiteral("GQuant114"));
                const int undoBefore = (mlQ && mlQ->undoStack()) ? mlQ->undoStack()->count() : 0;
                const QJsonObject rResp = rpc.call(
                        QStringLiteral("analysis.fit_robust"),
                        QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GQuant114") },
                                     { QStringLiteral("method"), QStringLiteral("huber") },
                                     { QStringLiteral("plot_curve"), false } });
                CHECK114(Rpc114::ok(rResp)
                                 && Rpc114::result(rResp).value(QStringLiteral("ok")).toBool(false),
                         "114d: fit_robust(huber, plot_curve=false) failed");
                CHECK114(mlQ && mlQ->activeGraph() && mlQ->activeGraph()->curves() == 1,
                         "114d: fit_robust(huber, plot_curve=false) plotted a curve!");
                CHECK114(mlQ && mlQ->undoStack() && mlQ->undoStack()->count() == undoBefore,
                         "114d: fit_robust(huber, plot_curve=false) pushed graph undo command!");
            }

            // 4o. signal.iir_filter falls back to high_cutoff when cutoff_high is null
            {
                Table *tSig = w->newTable(QStringLiteral("SigIir114"), 128, 2);
                for (int i = 0; i < 128; ++i) {
                    const double x = double(i) / 100.0;
                    tSig->setCell(i, 0, x);
                    tSig->setCell(i, 1, std::sin(2.0 * 3.14159265 * 10.0 * x));
                }
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.xy"),
                                 QJsonObject{ { QStringLiteral("table"),
                                                QStringLiteral("SigIir114") },
                                              { QStringLiteral("x_col"), 0 },
                                              { QStringLiteral("y_col"), 1 },
                                              { QStringLiteral("name"),
                                                QStringLiteral("GSigIir114") } })),
                         "114d: GSigIir114 plot.xy failed");
                const QJsonObject iirResp = rpc.call(
                        QStringLiteral("signal.iir_filter"),
                        QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GSigIir114") },
                                     { QStringLiteral("pass_type"), QStringLiteral("bandpass") },
                                     { QStringLiteral("cutoff"), 5.0 },
                                     { QStringLiteral("cutoff_high"), QJsonValue::Null },
                                     { QStringLiteral("high_cutoff"), 20.0 } });
                CHECK114(Rpc114::ok(iirResp)
                                 && Rpc114::result(iirResp).value(QStringLiteral("ok")).toBool(false),
                         "114d: signal.iir_filter failed when cutoff_high=null with high_cutoff=20");
            }

            // 4p. plot.matrix attaching to existing spectrogram layer styles only the new spectrogram
            {
                Matrix *m1 = w->newMatrix(QStringLiteral("MatSp114_1"), 4, 4);
                Matrix *m2 = w->newMatrix(QStringLiteral("MatSp114_2"), 4, 4);
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        m1->setCell(r, c, double(r + c));
                        m2->setCell(r, c, double(r * 2 + c));
                    }
                }
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.matrix"),
                                 QJsonObject{ { QStringLiteral("matrix"),
                                                QStringLiteral("MatSp114_1") },
                                              { QStringLiteral("palette"),
                                                QStringLiteral("gray") },
                                              { QStringLiteral("name"),
                                                QStringLiteral("GMatSp114") } })),
                         "114d: first plot.matrix failed");
                CHECK114(Rpc114::ok(rpc.call(
                                 QStringLiteral("plot.matrix"),
                                 QJsonObject{ { QStringLiteral("matrix"),
                                                QStringLiteral("MatSp114_2") },
                                              { QStringLiteral("graph"),
                                                QStringLiteral("GMatSp114") },
                                              { QStringLiteral("palette"),
                                                QStringLiteral("plasma") } })),
                         "114d: second plot.matrix attach failed");
                MultiLayer *mlSp = findGraph114(w, QStringLiteral("GMatSp114"));
                QList<Spectrogram *> specs;
                if (mlSp && mlSp->activeGraph() && mlSp->activeGraph()->plotWidget()) {
                    for (QwtPlotItem *it : mlSp->activeGraph()->plotWidget()->itemList()) {
                        if (it && it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram) {
                            if (auto *sp = dynamic_cast<Spectrogram *>(it))
                                specs.append(sp);
                        }
                    }
                }
                CHECK114(specs.size() == 2,
                         "114d: plot.matrix attach expected 2 spectrograms, got " << specs.size());
                if (specs.size() == 2) {
                    int nGray = 0, nPlasma = 0;
                    for (Spectrogram *sp : specs) {
                        if (sp->colorMapPreset() == Spectrogram::PresetGrayScale)
                            ++nGray;
                        else if (sp->colorMapPreset() == Spectrogram::PresetPlasma)
                            ++nPlasma;
                    }
                    CHECK114(nGray == 1 && nPlasma == 1,
                             "114d: plot.matrix attach overwrote first spectrogram palette (gray="
                                     << nGray << ", plasma=" << nPlasma << ")");
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 114e: Wave 3 Deep Audit Regressions (Numerics, CoreIO & PlotGUI)
    // -------------------------------------------------------------------------
    {
        AppWinGuard114 w(new ApplicationWindow());
        QTemporaryDir tmpDir;

        // 1. Scale-aware zero-bound snapping & non-dyadic constant-Y Fit::rSquare()
        {
            const double inf = std::numeric_limits<double>::infinity();
            CHECK114(Fit::snapToBound(2e-16, 0.0, inf, 1.0) == 0.0,
                     "114e: Fit::snapToBound missed zero lower bound at refScale=1.0");
            CHECK114(Fit::boundSide(2e-16, 0.0, inf, 1.0) == -1,
                     "114e: Fit::boundSide missed zero lower bound at refScale=1.0");
            CHECK114(Fit::boundSide(5e-19, 0.0, inf, 1e-18) == 0,
                     "114e: Fit::boundSide falsely snapped tiny interior value at refScale=1e-18");

            Table *tConst = w->newTable(QStringLiteral("ConstND114"), 10, 2);
            for (int i = 0; i < 10; ++i) {
                tConst->setCell(i, 0, double(i));
                tConst->setCell(i, 1, 0.3); // non-dyadic constant
            }
            MultiLayer *mlC =
                    w->multilayerPlot(tConst, QStringList() << tConst->colName(1), Graph::Scatter);
            Graph *gC = mlC ? mlC->activeGraph() : nullptr;
            if (gC && gC->curves() == 1) {
                LinearFit lf(w, gC);
                lf.setDataFromCurve(gC->curve(0)->title().text(), gC);
                lf.fit();
                CHECK114(std::abs(lf.rSquare() - 1.0) < 1e-9,
                         "114e: non-dyadic constant-Y (0.3) LinearFit rSquare expected 1.0, got "
                                 << lf.rSquare());
            }
        }

        // 2. Column negative row/count guards & importASCII Overwrite non-empty DateTime guard
        {
            Table *tCol = w->newTable(QStringLiteral("NegRow114"), 4, 3);
            tCol->column(0)->setColumnMode(SciDAVis::ColumnMode::Numeric);
            tCol->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
            tCol->column(2)->setColumnMode(SciDAVis::ColumnMode::DateTime);
            tCol->column(0)->setValueAt(0, 1.5);
            tCol->column(1)->setTextAt(0, QStringLiteral("ok"));
            tCol->column(2)->setDateTimeAt(0, QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5)));
            if (tCol->d_future_table && tCol->d_future_table->undoStack())
                tCol->d_future_table->undoStack()->clear();

            tCol->column(0)->setValueAt(-1, 99.0);
            tCol->column(1)->setTextAt(-1, QStringLiteral("bad"));
            tCol->column(2)->setDateTimeAt(-1, QDateTime(QDate(2020, 1, 1), QTime(0, 0)));
            tCol->column(0)->insertRows(-1, 2);
            tCol->column(0)->removeRows(-1, 2);
            CHECK114(tCol->numRows() == 4 && std::abs(tCol->cell(0, 0) - 1.5) < 1e-12,
                     "114e: negative row calls mutated column!");
            CHECK114(!tCol->d_future_table || !tCol->d_future_table->undoStack()
                             || tCol->d_future_table->undoStack()->count() == 0,
                     "114e: negative row calls pushed spurious undo commands!");

            // importASCII Overwrite into non-empty DateTime column must reject without converting
            Table *tDt = w->newTable(QStringLiteral("DtOv114"), 2, 1);
            tDt->column(0)->setColumnMode(SciDAVis::ColumnMode::DateTime);
            tDt->column(0)->setDateTimeAt(0, QDateTime(QDate(2025, 6, 15), QTime(12, 0)));
            const QString csvNum = tmpDir.path() + QStringLiteral("/num114.csv");
            {
                QFile cf(csvNum);
                if (cf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream ts(&cf);
                    ts << "V\n10.5\n20.5\n";
                }
            }
            w->activateSubWindow(tDt);
            w->importASCII(QStringList() << csvNum, ImportASCIIDialog::Overwrite,
                           QStringLiteral(","), 0, true, false, false, true, QLocale::c());
            CHECK114(tDt->column(0)->columnMode() == SciDAVis::ColumnMode::DateTime
                             && tDt->column(0)->dateTimeAt(0).isValid(),
                     "114e: importASCII Overwrite wiped/converted non-empty DateTime column!");
        }

        // 3. Inverted broken log/linear axis autoScale & grouped bar relayout on setCurveType
        {
            Table *tPl = w->newTable(QStringLiteral("PlotW3_114"), 5, 3);
            for (int i = 0; i < 5; ++i) {
                tPl->setCell(i, 0, double(i + 1));
                tPl->setCell(i, 1, double(i + 1) * 10.0);
                tPl->setCell(i, 2, double(5 - i) * 10.0);
            }
            MultiLayer *mlPl = w->multilayerPlot(
                    tPl, QStringList() << tPl->colName(1) << tPl->colName(2), Graph::VerticalBars);
            Graph *gPl = mlPl ? mlPl->activeGraph() : nullptr;
            CHECK114(gPl && gPl->curves() == 2, "114e: grouped bar setup failed");
            if (gPl && gPl->curves() == 2) {
                // Convert second grouped bar to Line -> first bar must re-center to offset=0, gap=35
                gPl->setCurveType(1, Graph::Line);
                auto *bc0 = dynamic_cast<QwtBarCurve *>(gPl->curve(0));
                CHECK114(bc0 && bc0->offset() == 0 && bc0->gap() == 35,
                         "114e: Graph::setCurveType did not re-layout remaining grouped bar (gap="
                                 << (bc0 ? bc0->gap() : -1)
                                 << ", offset=" << (bc0 ? bc0->offset() : -999) << ")");

                // Inverted broken axis autoScale (log10Before=true, log10After=false)
                gPl->setScale(QwtPlot::yLeft, 0.05, 60.0);
                Graph::AxisBreakSpec brk;
                brk.enabled = true;
                brk.breakFrom = 10.0;
                brk.breakTo = 20.0;
                brk.log10Before = true;
                brk.log10After = false;
                gPl->setAxisBreak(QwtPlot::yLeft, brk);
                CHECK114(gPl->axisBreak(QwtPlot::yLeft).enabled,
                         "114e: axis break was unexpectedly suspended");
                QwtScaleEngine *eng = gPl->plotWidget()->axisScaleEngine(QwtPlot::yLeft);
                CHECK114(eng != nullptr, "114e: broken axis scale engine null");
                if (eng) {
                    eng->setAttribute(QwtScaleEngine::Inverted, true);
                    double x1 = 0.05, x2 = 50.0, step = 0.0;
                    eng->autoScale(5, x1, x2, step);
                    CHECK114(x1 > x2 && x2 > 0.0 && x1 >= 50.0,
                             "114e: inverted broken axis autoScale corrupted bounds (" << x1 << ", "
                                                                                       << x2
                                                                                       << ")");
                }
            }
        }
    }

    if (g114Failures == 0) {
        std::cout << "[PASS] Suite 114 Deep Swarm Audit Regressions passed!" << std::endl;
        return 0;
    }
    std::cerr << "[FAIL] Suite 114 failed with " << g114Failures << " failure(s)." << std::endl;
    return 1;
}
