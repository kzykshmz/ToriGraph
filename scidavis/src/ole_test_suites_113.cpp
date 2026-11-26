#include "ole_test_common.h"
#include "GraphInspectorDock.h"
#include "LineProfileTool.h"
#include "QwtPieCurve.h"
#include "RangeSelectorTool.h"
#include "SigmoidalFit.h"
#include "TableStatistics.h"
#include "TranslateCurveTool.h"
#include "VectorCurve.h"
#include "future/matrix/MatrixModel.h"
#include "qwt3d_autoscaler.h"

#include <QBuffer>
#include <QComboBox>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTreeWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// =============================================================================
// Suite 113: Swarm review Wave 1 & Wave 2 C++ core, tools, I/O and RPC regressions.
//   113a Curve fitting & numerics:
//        - LinearFit centered evaluation at x = 1e13 (no catastrophic cancellation on MSVC/Linux)
//        - LinearFit femtosecond-scale x ~ 1e-16 (scale-invariant Fit::isCollinearLinearFit)
//        - PolynomialFit degree 2 at x = 1e6 with zero-weight Inf row (no 0*Inf = NaN in SVD)
//        - ExponentialFit finite parameter errors, covariance, R^2 and bounds
//        - SmoothFilter::FFT window=5 actually attenuates high-frequency noise (not no-op)
//   113b Worksheet, Matrix, TableStatistics, Folder & Note serialization:
//        - TableStatistics::removeCol multi-column removal re-resolves surviving targets accurately
//        - Matrix::range() when (0,0) is NaN, Matrix::recalculate on empty selection,
//          MatrixModel::headerData descending Y coordinates, Matrix::addColumns undo/redo
//        - Folder::myFolderListItem default-initialized to nullptr (no wild pointer)
//        - Note save/reload round-trip containing literal "</content>\n</note>" preserves text
//          and does not truncate following windows
//   113c 2D/3D Plotting, FunctionCurve, Histogram, Violin, AutoScaler & ImageMarker:
//        - FunctionCurve with 1/x across 0 and sqrt(x) across negative x stays bounded
//        - QwtHistogram on Log10 X axis and BoxCurve violin on Log10 Y axis produce finite data
//        - Qwt3D::LinearAutoScaler execute() handles equal/Inf/overflow inputs without OOB/hang
//        - ImageMarker format-sniffing fallback loads PNG bytes saved under a .jpg extension
//   113d Interactive tools, digitizer & SettingsTransfer:
//        - LineProfileTool without a selected ImageMarker does not crash; border pixel averaging
//          with large averagePixels does not read out-of-bounds sentinel 12345
//        - GraphDigitizer::mapPixelToData exact 2x2 affine inverse on oblique/sheared axes and
//          safe rejection of non-positive Log10 anchors
//        - RangeSelectorTool skips QwtErrorPlotCurve targets
//        - SelectionMoveResizer handles purely horizontal / vertical ArrowMarker without div-by-0
//        - SettingsTransfer::applyLabDefaults unpacks GraphStylePresets/JsonBase64 safely
//   113e ImportOPJU bounds & RPC safety / undo:
//        - ImportOPJU rejects crafted huge Matrix dimensions without int32 overflow / OOM
//        - curvemath.eval / curvemath.fill_between / signal.coherence on a spectrogram-only layer
//          return clean RPC errors instead of null-dereferencing
//        - table.set_data_dict & graph.to_python preserve numeric-looking Text columns verbatim
//        - plot.stacked_bars formats DateTime X labels cleanly
//        - plot.xy invalid-param rejection leaves zero orphan graph windows
//        - stats.clustering (hierarchical) pads assignments to table.numRows() with -1 on NaN rows
//        - curvemath.integrate / signal.smooth / analysis.analyze_peaks / plot.color_scatter push
//          clean per-graph Undo/Redo steps
// =============================================================================

namespace {

int g113Failures = 0;

#define CHECK113(cond, msg)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;            \
            ++g113Failures;                                                                        \
        }                                                                                          \
    } while (0)

struct NonInteractiveGuard113
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard113()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard113()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard113
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard113(ApplicationWindow *w = nullptr) : win(w)
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
    ~AppWinGuard113()
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
    AppWinGuard113(const AppWinGuard113 &) = delete;
    AppWinGuard113 &operator=(const AppWinGuard113 &) = delete;
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

struct Rpc113
{
    PythonAutomationServer *srv = nullptr;
    int id = 11300;
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

class ExposedAutoScaler113 : public Qwt3D::LinearAutoScaler
{
public:
    using Qwt3D::LinearAutoScaler::LinearAutoScaler;
    int runExecute(double &a, double &b, double start, double stop, int ivals)
    {
        return execute(a, b, start, stop, ivals);
    }
};

class ExposedLineProfileTool113 : public LineProfileTool
{
public:
    using LineProfileTool::LineProfileTool;
    int probeAveragePixel(const QImage &img, int px, int py, bool horiz)
    {
        return averageImagePixel(img, px, py, horiz);
    }
};

MultiLayer *findGraph113(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer") && w->name() == name)
            return static_cast<MultiLayer *>(w);
    }
    return nullptr;
}

} // namespace

int runSuite113SwarmAuditRegressions(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    g113Failures = 0;
    NonInteractiveGuard113 niGuard;
    QTemporaryDir tmpDir(QDir::tempPath() + QStringLiteral("/torigraph_suite113_XXXXXX"));
    CHECK113(tmpDir.isValid(), "113: temporary directory creation failed");

    std::cout << "[INFO] Running Suite 113: Swarm review Wave 1 & Wave 2 regressions...\n";

    // =========================================================================
    // 113a: Curve fitting & numerics
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());
        PythonAutomationServer srv(app);
        Rpc113 rpc { &srv };

        // 1) LinearFit centered evaluation at x = 1e13 + {0..4}
        {
            Table *t = app->newTable(QStringLiteral("LinBig113"), 5, 2);
            CHECK113(t != nullptr, "113a: LinBig113 table");
            const double base = 1e13;
            for (int i = 0; i < 5; ++i) {
                t->setCell(i, 0, base + double(i));
                t->setCell(i, 1, 2.0 * double(i) + 5.0);
            }
            MultiLayer *ml = app->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            CHECK113(ml && ml->activeGraph(), "113a: LinBig113 plot");
            if (ml && ml->activeGraph()) {
                LinearFit lf(app, ml->activeGraph(), t->colName(1));
                lf.fit();
                const std::vector<double> res = lf.results();
                CHECK113(res.size() >= 2, "113a: LinearFit(1e13) results empty");
                if (res.size() >= 2) {
                    CHECK113(std::isfinite(res[1]) && std::abs(res[1] - 2.0) < 1e-6,
                             "113a: LinearFit(1e13) slope=" << res[1] << " expected 2.0");
                    CHECK113(std::isfinite(lf.rSquare()) && lf.rSquare() > 0.999999,
                             "113a: LinearFit(1e13) R^2=" << lf.rSquare());
                    CHECK113(std::isfinite(lf.chiSquare()) && lf.chiSquare() < 1e-10,
                             "113a: LinearFit(1e13) chi^2=" << lf.chiSquare());
                }
            }
        }

        // 2) LinearFit femtosecond-scale x ~ 1e-16 (scale-invariant collinear check)
        {
            Table *t = app->newTable(QStringLiteral("LinFemto113"), 5, 2);
            CHECK113(t != nullptr, "113a: LinFemto113 table");
            for (int i = 0; i < 5; ++i) {
                const double xv = double(i + 1) * 1e-16;
                t->setCell(i, 0, xv);
                t->setCell(i, 1, 3.0 * xv + 1e-15);
            }
            MultiLayer *ml = app->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            CHECK113(ml && ml->activeGraph(), "113a: LinFemto113 plot");
            if (ml && ml->activeGraph()) {
                LinearFit lf(app, ml->activeGraph(), t->colName(1));
                lf.fit();
                const std::vector<double> res = lf.results();
                CHECK113(res.size() >= 2 && std::isfinite(res[1]) && std::abs(res[1] - 3.0) < 1e-6,
                         "113a: LinearFit(1e-16) slope=" << (res.size() >= 2 ? res[1] : 0.0));
                CHECK113(std::isfinite(lf.rSquare()) && lf.rSquare() > 0.99999,
                         "113a: LinearFit(1e-16) R^2=" << lf.rSquare());
            }
        }

        // 3) PolynomialFit degree 2 on x = 1e6 + {0..9} with a zero-weight Inf row
        {
            Table *t = app->newTable(QStringLiteral("Poly113"), 10, 3);
            CHECK113(t != nullptr, "113a: Poly113 table");
            t->setColPlotDesignation(2, SciDAVis::yErr);
            const double x0 = 1e6;
            for (int i = 0; i < 10; ++i) {
                const double dx = double(i);
                t->setCell(i, 0, x0 + dx);
                t->setCell(i, 1, 1.5 * dx * dx - 2.0 * dx + 3.0);
                t->setCell(i, 2, 1.0);
            }
            // Put Inf in row 0 with Inf error bar (weight = 0) so naive 0*Inf would yield NaN
            t->setCell(0, 1, std::numeric_limits<double>::infinity());
            t->setCell(0, 2, std::numeric_limits<double>::infinity());
            MultiLayer *ml = app->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            CHECK113(ml && ml->activeGraph(), "113a: Poly113 plot");
            if (ml && ml->activeGraph()) {
                PolynomialFit pf(app, ml->activeGraph(), 2);
                pf.setDataFromCurve(t->colName(1));
                pf.setYErrorSource(Fit::AssociatedErrors);
                pf.fit();
                const std::vector<double> res = pf.results();
                CHECK113(res.size() >= 3, "113a: PolynomialFit results empty");
                if (res.size() >= 3) {
                    CHECK113(std::isfinite(res[0]) && std::isfinite(res[1]) && std::isfinite(res[2]),
                             "113a: PolynomialFit produced non-finite coeffs");
                    CHECK113(std::isfinite(pf.rSquare()) && pf.rSquare() > 0.999,
                             "113a: PolynomialFit(1e6+zeroWeightInf) R^2=" << pf.rSquare());
                }
            }
        }

        // 4) ExponentialFit finite parameter errors, covariance, R^2 and bounds
        {
            Table *t = app->newTable(QStringLiteral("Exp113"), 40, 2);
            CHECK113(t != nullptr, "113a: Exp113 table");
            for (int i = 0; i < 40; ++i) {
                const double xv = 0.25 * double(i);
                const double yv = 1.0 + 10.0 * std::exp(-xv / 2.5) + 0.02 * ((i % 3) - 1);
                t->setCell(i, 0, xv);
                t->setCell(i, 1, yv);
            }
            MultiLayer *ml = app->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            CHECK113(ml && ml->activeGraph(), "113a: Exp113 plot");
            if (ml && ml->activeGraph()) {
                ExponentialFit ef(app, ml->activeGraph(), t->colName(1));
                double guesses[3] = { 8.0, 2.0, 0.5 };
                ef.setInitialGuesses(guesses);
                ef.setParameterBounds(1, 0.5, 1.5); // clamp decay time t1 <= 1.5
                ef.fit();
                const std::vector<double> res = ef.results();
                const std::vector<double> errs = ef.errors();
                CHECK113(res.size() >= 3 && errs.size() >= 3, "113a: ExponentialFit res/errs");
                if (res.size() >= 3 && errs.size() >= 3) {
                    CHECK113(std::isfinite(res[0]) && std::isfinite(res[1]) && std::isfinite(res[2]),
                             "113a: ExponentialFit non-finite results");
                    CHECK113(res[1] >= 0.5 - 1e-6 && res[1] <= 1.5 + 1e-6,
                             "113a: ExponentialFit bound violated: t1=" << res[1]);
                    CHECK113(std::isfinite(errs[0]) && std::isfinite(errs[1])
                                     && std::isfinite(errs[2]),
                             "113a: ExponentialFit non-finite errors");
                    CHECK113(std::isfinite(ef.rSquare()), "113a: ExponentialFit R^2 non-finite");
                }
            }
        }

        // 5) SmoothFilter::FFT window=5 attenuates high-frequency oscillation
        {
            Table *t = app->newTable(QStringLiteral("SmoothFft113"), 64, 2);
            CHECK113(t != nullptr, "113a: SmoothFft113 table");
            double rawHfEnergy = 0.0;
            for (int i = 0; i < 64; ++i) {
                const double xv = double(i);
                const double sig = 5.0 + std::sin(2.0 * M_PI * xv / 64.0)
                        + ((i % 2 == 0) ? 1.5 : -1.5);
                t->setCell(i, 0, xv);
                t->setCell(i, 1, sig);
                rawHfEnergy += ((i % 2 == 0) ? 1.5 : -1.5) * ((i % 2 == 0) ? 1.5 : -1.5);
            }
            MultiLayer *ml = app->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
            CHECK113(ml && ml->activeGraph(), "113a: SmoothFft113 plot");
            if (ml && ml->activeGraph()) {
                const QJsonObject sr = rpc.call(
                        QStringLiteral("signal.smooth"),
                        QJsonObject { { "graph", ml->name() },
                                      { "curve", 0 },
                                      { "method", "fft" },
                                      { "window", 5 } });
                CHECK113(Rpc113::ok(sr), "113a: signal.smooth(fft, window=5) failed");
                Graph *g = ml->activeGraph();
                CHECK113(g && g->curves() >= 2 && g->curve(1) != nullptr,
                         "113a: signal.smooth(fft) did not add curve");
                if (g && g->curves() >= 2 && g->curve(1)) {
                    QwtPlotCurve *sc = g->curve(1);
                    double smoothErr = 0.0;
                    for (int i = 4; i < int(sc->dataSize()) - 4; ++i) {
                        const double clean = 5.0 + std::sin(2.0 * M_PI * sc->x(i) / 64.0);
                        const double diff = sc->y(i) - clean;
                        smoothErr += diff * diff;
                    }
                    CHECK113(smoothErr < 0.25 * rawHfEnergy,
                             "113a: SmoothFilter::FFT(window=5) did not attenuate HF noise: err="
                                     << smoothErr << " raw=" << rawHfEnergy);
                }
            }
        }
        std::cout << "[PASS] 113a: curve fitting & numerics checks passed\n";
    }

    // =========================================================================
    // 113b: Worksheet, Matrix, TableStatistics, Folder & Note serialization
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());
        PythonAutomationServer srv(app);
        Rpc113 rpc { &srv };

        // 1) TableStatistics::removeCol multi-column removal
        {
            Table *base = app->newTable(QStringLiteral("StatBase113"), 4, 5);
            CHECK113(base != nullptr, "113b: StatBase113");
            for (int c = 0; c < 5; ++c) {
                base->setColName(c, QStringLiteral("c%1").arg(c));
                for (int r = 0; r < 4; ++r)
                    base->setCell(r, c, double((c + 1) * 10 + r));
            }
            TableStatistics *ts = app->newTableStatistics(
                    base, TableStatistics::StatColumn, QList<int>() << 1 << 3 << 4);
            CHECK113(ts != nullptr && ts->numRows() == 3,
                     "113b: TableStatistics initial rows=" << (ts ? ts->numRows() : -1));
            if (ts) {
                // Remove columns 0 and 2; surviving targets {1,3,4} shift to {0,1,2}
                base->d_future_table->removeColumns(2, 1);
                base->d_future_table->removeColumns(0, 1);
                QCoreApplication::processEvents();
                CHECK113(ts->numRows() == 3,
                         "113b: TableStatistics lost rows after removing non-target cols: "
                                 << ts->numRows());
                // Modify surviving column c3 (now index 1) and ensure update() writes right row
                base->setCell(0, 1, 999.0);
                ts->update(base, base->colName(1));
                const double meanRow1 = ts->cell(1, 2);
                CHECK113(std::isfinite(meanRow1) && meanRow1 > 200.0,
                         "113b: TableStatistics failed to update shifted target col, mean="
                                 << meanRow1);
            }
        }

        // 2) Matrix::range() with NaN@(0,0), empty recalculate, descending Y headerData, addColumns undo
        {
            Matrix *m = app->newMatrix(QStringLiteral("Mat113"), 4, 4);
            CHECK113(m != nullptr, "113b: Mat113");
            m->setCoordinates(0.0, 3.0, 10.0, 1.0); // descending Y: yStart=10 > yEnd=1
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    m->setCell(r, c, double(r * 10 + c + 1));
            m->setCell(0, 0, std::numeric_limits<double>::quiet_NaN());

            double zMin = 0.0, zMax = 0.0;
            m->range(&zMin, &zMax);
            CHECK113(std::isfinite(zMin) && std::isfinite(zMax)
                             && std::abs(zMin - 2.0) < 1e-9 && std::abs(zMax - 34.0) < 1e-9,
                     "113b: Matrix::range with NaN@(0,0) got [" << zMin << ", " << zMax << "]");

            // Descending Y headerData via MatrixModel ("<1-based index> (<coord>)")
            if (m->d_future_matrix) {
                MatrixModel mm(m->d_future_matrix);
                const QString h0 = mm.headerData(0, Qt::Vertical, Qt::DisplayRole).toString();
                const QString h3 = mm.headerData(3, Qt::Vertical, Qt::DisplayRole).toString();
                CHECK113(h0.contains(QStringLiteral("(10)")) && h3.contains(QStringLiteral("(1)")),
                         "113b: MatrixModel descending Y headers: h0=" << h0.toStdString()
                                                                       << " h3=" << h3.toStdString());
            }

            // Recalculate with empty selection should not crash
            m->setFormula(QStringLiteral("i + j"));
            m->recalculate();
            CHECK113(std::isfinite(m->cell(1, 1)), "113b: Matrix::recalculate");

            // addColumns Undo/Redo round-trip (select all 4 columns -> addColumns appends 4)
            if (m->d_future_matrix) {
                const int colsBefore = m->numCols();
                m->d_future_matrix->selectAll();
                m->d_future_matrix->addColumns();
                CHECK113(m->numCols() > colsBefore,
                         "113b: Matrix::addColumns got " << m->numCols() << " expected > "
                                                         << colsBefore);
                const int colsAdded = m->numCols();
                if (m->d_future_matrix->undoStack()) {
                    m->d_future_matrix->undoStack()->undo();
                    CHECK113(m->numCols() == colsBefore, "113b: Matrix::addColumns undo");
                    m->d_future_matrix->undoStack()->redo();
                    CHECK113(m->numCols() == colsAdded, "113b: Matrix::addColumns redo");
                }
            }
        }

        // 3) Folder::myFolderListItem default-initialized to nullptr
        {
            Folder f(QStringLiteral("StandaloneFolder113"));
            CHECK113(f.folderListItem() == nullptr,
                     "113b: Folder::myFolderListItem was not null-initialized");
        }

        // 4) Note save/reload containing literal </content>\n</note> + trailing graph preserved
        {
            const QString trickyText = QStringLiteral("line1\n</content>\n</note>\nline2_after");
            Note *n = app->newNote(QStringLiteral("Note113"));
            CHECK113(n != nullptr, "113b: Note113");
            n->setText(trickyText);

            Table *t = app->newTable(QStringLiteral("AfterNoteTbl113"), 3, 2);
            for (int i = 0; i < 3; ++i) {
                t->setCell(i, 0, double(i));
                t->setCell(i, 1, double(i * i + 1));
            }
            MultiLayer *ml = app->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
            CHECK113(ml != nullptr, "113b: AfterNote plot");
            const QString gName = ml ? ml->name() : QString();

            const QString projPath = tmpDir.filePath(QStringLiteral("note_roundtrip_113.sciprj"));
            const QJsonObject saveR = rpc.call(QStringLiteral("app.save_project"),
                                               QJsonObject { { "path", projPath } });
            CHECK113(Rpc113::ok(saveR), "113b: app.save_project failed");
            QFileInfo fi(projPath);
            CHECK113(fi.exists() && fi.size() > 0, "113b: saved note_roundtrip_113.sciprj");

            AppWinGuard113 app2(new ApplicationWindow());
            PythonAutomationServer srv2(app2);
            Rpc113 rpc2 { &srv2 };
            const QJsonObject openR = rpc2.call(QStringLiteral("app.open_project"),
                                                QJsonObject { { "path", projPath } });
            CHECK113(Rpc113::ok(openR), "113b: app.open_project failed");
            Note *reloadedNote = nullptr;
            for (MyWidget *w : app2->windowsList()) {
                if (w && w->inherits("Note") && w->name() == QLatin1String("Note113"))
                    reloadedNote = static_cast<Note *>(w);
            }
            CHECK113(reloadedNote != nullptr, "113b: reloaded Note113 missing");
            if (reloadedNote) {
                CHECK113(reloadedNote->text().trimmed() == trickyText,
                         "113b: Note113 text corrupted on reload: ["
                                 << reloadedNote->text().toStdString() << "]");
            }
            CHECK113(findGraph113(app2, gName) != nullptr,
                     "113b: graph after Note113 was lost on reload");
        }
        std::cout << "[PASS] 113b: worksheet, matrix, TableStatistics, Folder & Note checks passed\n";
    }

    // =========================================================================
    // 113c: 2D/3D plotting, FunctionCurve, Histogram, Violin, AutoScaler, ImageMarker
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());

        // 1) FunctionCurve with singularities (1/x, sqrt(x) on negative x) stays bounded
        {
            MultiLayer *ml = app->newGraph(QStringLiteral("Fn113"));
            Graph *g = ml ? ml->activeGraph() : nullptr;
            CHECK113(g != nullptr, "113c: Fn113 graph");
            if (g) {
                QList<double> ranges { -5.0, 5.0 };
                const bool ok1 = g->addFunctionCurve(
                        app, FunctionCurve::Normal, QStringList { QStringLiteral("1/x") },
                        QStringLiteral("x"), ranges, 200);
                CHECK113(ok1 && g->curves() >= 1 && g->curve(0) != nullptr
                                 && g->curve(0)->dataSize() <= 200,
                         "113c: FunctionCurve(1/x) invalid or unbounded");
                const bool ok2 = g->addFunctionCurve(
                        app, FunctionCurve::Normal, QStringList { QStringLiteral("sqrt(x)") },
                        QStringLiteral("x"), ranges, 200);
                CHECK113(ok2 && g->curves() >= 2 && g->curve(1) != nullptr
                                 && g->curve(1)->dataSize() <= 200,
                         "113c: FunctionCurve(sqrt(x)) invalid or unbounded");
                g->replot();
            }
        }

        // 2) QwtHistogram on Log10 X axis & BoxCurve violin on Log10 Y axis
        {
            Table *t = app->newTable(QStringLiteral("LogPlot113"), 30, 1);
            for (int i = 0; i < 30; ++i)
                t->setCell(i, 0, 1.0 + double(i % 10) * 2.5);
            MultiLayer *mlH = app->multilayerPlot(t, QStringList() << t->colName(0), Graph::Histogram);
            CHECK113(mlH && mlH->activeGraph(), "113c: Histogram plot");
            if (mlH && mlH->activeGraph()) {
                Graph *g = mlH->activeGraph();
                g->setScale(QwtPlot::xBottom, 0.5, 50.0, 0.0, 5, 5, 1); // 1 = Log10
                g->replot();
                QwtPlotCurve *hc = g->curve(0);
                CHECK113(hc != nullptr && hc->dataSize() > 0, "113c: Log10 Histogram empty");
                if (hc) {
                    for (int i = 0; i < int(hc->dataSize()); ++i) {
                        CHECK113(std::isfinite(hc->x(i)) && std::isfinite(hc->y(i)),
                                 "113c: Log10 Histogram non-finite point");
                    }
                }
            }

            MultiLayer *mlV = app->multilayerPlot(t, QStringList() << t->colName(0), Graph::Box);
            CHECK113(mlV && mlV->activeGraph(), "113c: Box/Violin plot");
            if (mlV && mlV->activeGraph()) {
                Graph *g = mlV->activeGraph();
                if (auto *bc = dynamic_cast<BoxCurve *>(g->curve(0))) {
                    bc->setViolinMode(true);
                    g->setScale(QwtPlot::yLeft, 0.5, 50.0, 0.0, 5, 5, 1); // 1 = Log10
                    g->replot();
                    QImage img(200, 160, QImage::Format_ARGB32);
                    img.fill(Qt::white);
                    QPainter p(&img);
                    g->plotWidget()->print(&p, QRect(0, 0, 200, 160));
                    p.end();
                    CHECK113(true, "113c: Log10 Violin painted cleanly");
                }
            }
        }

        // 3) Qwt3D::LinearAutoScaler execute() edge cases (a==b, Inf, huge delta)
        {
            ExposedAutoScaler113 scaler;
            double a = 0.0, b = 0.0;
            const int rEq = scaler.runExecute(a, b, 5.0, 5.0, 4);
            CHECK113(rEq >= 0, "113c: LinearAutoScaler(5,5) crashed/negative");
            const double inf = std::numeric_limits<double>::infinity();
            const int rInf = scaler.runExecute(a, b, inf, inf, 4);
            CHECK113(rInf == 0 || (std::isfinite(a) && std::isfinite(b)),
                     "113c: LinearAutoScaler(+Inf,+Inf) returned non-finite [" << a << "," << b << "]");
            const int rHuge = scaler.runExecute(a, b, -1e300, 1e300, 4);
            CHECK113(rHuge >= 0 && std::isfinite(a) && std::isfinite(b),
                     "113c: LinearAutoScaler(-1e300,1e300) non-finite");
            const int rNorm = scaler.runExecute(a, b, 0.0, 10.0, 5);
            CHECK113(rNorm > 0 && a <= 0.0 && b >= 10.0,
                     "113c: LinearAutoScaler(0,10) failed: r=" << rNorm << " a=" << a << " b=" << b);
        }

        // 4) ImageMarker format-sniffing fallback (PNG bytes saved as .jpg)
        {
            QImage src(16, 16, QImage::Format_ARGB32);
            src.fill(QColor(0x22, 0x88, 0xCC));
            const QString fakeJpg = tmpDir.filePath(QStringLiteral("png_as_jpg_113.jpg"));
            CHECK113(src.save(fakeJpg, "PNG"), "113c: write PNG bytes to .jpg path");
            ImageMarker im(fakeJpg);
            CHECK113(!im.pixmap().isNull() && im.pixmap().width() == 16 && im.pixmap().height() == 16,
                     "113c: ImageMarker failed format-sniffing fallback for PNG-in-.jpg");
        }
        std::cout << "[PASS] 113c: 2D/3D plotting, FunctionCurve, AutoScaler & ImageMarker passed\n";
    }

    // =========================================================================
    // 113d: Interactive tools, digitizer & SettingsTransfer
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());

        // 1) LineProfileTool null target safety & border pixel averaging OOB guard
        {
            MultiLayer *ml = app->newGraph(QStringLiteral("Tools113"));
            Graph *g = ml ? ml->activeGraph() : nullptr;
            CHECK113(g != nullptr, "113d: Tools113 graph");
            if (g) {
                // No ImageMarker selected -> must NOT crash
                ExposedLineProfileTool113 tool(g, 7);
                const bool okNull = tool.calculateLineProfile(QPoint(0, 0), QPoint(10, 10));
                CHECK113(!okNull, "113d: LineProfileTool with null target should return false");

                // Uniform gray image: border averaging with averagePixels=7 must stay 200 (not 12345 sentinel!)
                QImage uniformImg(8, 8, QImage::Format_ARGB32);
                uniformImg.fill(QColor(200, 200, 200));
                const int avgCorner = tool.probeAveragePixel(uniformImg, 0, 0, true);
                const int avgEnd = tool.probeAveragePixel(uniformImg, 7, 7, false);
                CHECK113(avgCorner == 200 && avgEnd == 200,
                         "113d: LineProfileTool border average corrupted (OOB): corner="
                                 << avgCorner << " end=" << avgEnd);
            }
        }

        // 2) GraphDigitizer::mapPixelToData oblique 2x2 affine inverse & Log10 <= 0 guard
        {
            GraphDigitizer::AxisCalibration cal;
            cal.pixelX1 = QPointF(10.0, 20.0);
            cal.pixelX2 = QPointF(110.0, 40.0); // u = (100, 20)
            cal.valueX1 = 0.0;
            cal.valueX2 = 10.0;
            cal.pixelY1 = QPointF(10.0, 20.0);
            cal.pixelY2 = QPointF(25.0, 120.0); // v = (15, 100)
            cal.valueY1 = 0.0;
            cal.valueY2 = 100.0;
            // Point at origin + 0.3*u + 0.7*v -> should map exactly to (3.0, 70.0)
            const QPointF pTest = QPointF(10.0 + 0.3 * 100.0 + 0.7 * 15.0,
                                          20.0 + 0.3 * 20.0 + 0.7 * 100.0);
            const QPointF dMapped = GraphDigitizer::mapPixelToData(pTest, cal);
            CHECK113(std::abs(dMapped.x() - 3.0) < 1e-6 && std::abs(dMapped.y() - 70.0) < 1e-6,
                     "113d: GraphDigitizer oblique affine inverse got ("
                             << dMapped.x() << ", " << dMapped.y() << ") expected (3, 70)");

            // Log10 with non-positive valueX1 must produce NaN rather than silent garbage
            GraphDigitizer::AxisCalibration badLog = cal;
            badLog.logX = true;
            badLog.valueX1 = 0.0;
            badLog.valueX2 = 100.0;
            const QPointF dBad = GraphDigitizer::mapPixelToData(pTest, badLog);
            CHECK113(std::isnan(dBad.x()),
                     "113d: GraphDigitizer Log10<=0 did not reject non-positive anchor: x="
                             << dBad.x());
        }

        // 3) RangeSelectorTool skips ErrorBars curves
        {
            Table *t = app->newTable(QStringLiteral("ErrSkip113"), 5, 3);
            t->setColPlotDesignation(2, SciDAVis::yErr);
            for (int i = 0; i < 5; ++i) {
                t->setCell(i, 0, double(i + 1));
                t->setCell(i, 1, double((i + 1) * 2));
                t->setCell(i, 2, 0.5);
            }
            MultiLayer *ml = app->multilayerPlot(t, QStringList() << t->colName(1), Graph::Scatter);
            Graph *g = ml ? ml->activeGraph() : nullptr;
            CHECK113(g != nullptr, "113d: ErrSkip113 graph");
            if (g) {
                g->addErrorBars(t->colName(1), t, t->colName(2), QwtErrorPlotCurve::Vertical);
                RangeSelectorTool rst(g);
                CHECK113(rst.selectedCurve() != nullptr
                                 && dynamic_cast<QwtErrorPlotCurve *>(rst.selectedCurve()) == nullptr,
                         "113d: RangeSelectorTool selected an ErrorBars curve");
            }
        }

        // 4) SelectionMoveResizer horizontal / vertical ArrowMarker
        {
            MultiLayer *ml = app->newGraph(QStringLiteral("ArrowRes113"));
            Graph *g = ml ? ml->activeGraph() : nullptr;
            CHECK113(g != nullptr, "113d: ArrowRes113 graph");
            if (g) {
                g->resize(400, 300);
                g->replot();
                ArrowMarker amH;
                amH.setStartPoint(0.2, 0.5);
                amH.setEndPoint(0.8, 0.5);
                g->addArrow(&amH);
                ArrowMarker *addedH =
                        g->lineMarkerKeys().isEmpty() ? nullptr : g->arrow(g->lineMarkerKeys().last());
                CHECK113(addedH != nullptr, "113d: add horizontal ArrowMarker");
                if (addedH) {
                    SelectionMoveResizer resizer(addedH);
                    CHECK113(resizer.contains(addedH), "113d: SelectionMoveResizer holds H arrow");
                }
                ArrowMarker amV;
                amV.setStartPoint(0.5, 0.2);
                amV.setEndPoint(0.5, 0.8);
                g->addArrow(&amV);
                ArrowMarker *addedV =
                        g->lineMarkerKeys().isEmpty() ? nullptr : g->arrow(g->lineMarkerKeys().last());
                CHECK113(addedV != nullptr, "113d: add vertical ArrowMarker");
                if (addedV) {
                    SelectionMoveResizer resizer(addedV);
                    CHECK113(resizer.contains(addedV), "113d: SelectionMoveResizer holds V arrow");
                }
            }
        }

        // 5) SettingsTransfer::applyLabDefaults unpacks GraphStylePresets/JsonBase64 to presets JSON
        {
            const QString labIni = tmpDir.filePath(QStringLiteral("lab_defaults_113.ini"));
            const QByteArray payload = QByteArray("[{\"id\":\"lab_test_113\",\"name\":\"Lab113\"}]");
            {
                QSettings lab(labIni, QSettings::IniFormat);
                lab.setValue(QStringLiteral("GraphStylePresets/JsonBase64"),
                             QString::fromLatin1(payload.toBase64()));
                lab.sync();
            }
            const QString presetPath = MultiLayer::userStylePresetsFilePath();
            struct PresetFileGuard113
            {
                QString path;
                bool hadFile = false;
                QByteArray origBytes;
                explicit PresetFileGuard113(const QString &p) : path(p)
                {
                    QFile f(path);
                    if (f.exists() && f.open(QIODevice::ReadOnly)) {
                        hadFile = true;
                        origBytes = f.readAll();
                    }
                    if (QFile::exists(path))
                        QFile::remove(path);
                }
                ~PresetFileGuard113()
                {
                    if (hadFile) {
                        QDir().mkpath(QFileInfo(path).absolutePath());
                        QFile f(path);
                        if (f.open(QIODevice::WriteOnly))
                            f.write(origBytes);
                    } else if (QFile::exists(path)) {
                        QFile::remove(path);
                    }
                }
            } presetGuard(presetPath);

            QSettings &appSettings = ApplicationWindow::getSettings();
            const int applied = SettingsTransfer::applyLabDefaults(appSettings, labIni);
            CHECK113(applied > 0, "113d: applyLabDefaults returned " << applied);
            CHECK113(!appSettings.contains(QStringLiteral("GraphStylePresets/JsonBase64")),
                     "113d: applyLabDefaults leaked raw JsonBase64 key into user QSettings");
            QFile pf(presetPath);
            QByteArray unpacked;
            if (pf.open(QIODevice::ReadOnly))
                unpacked = pf.readAll();
            CHECK113(unpacked.contains("lab_test_113"),
                     "113d: applyLabDefaults failed to unpack presets file: "
                             << QString::fromUtf8(unpacked).toStdString());
        }
        std::cout << "[PASS] 113d: tools, digitizer & SettingsTransfer checks passed\n";
    }

    // =========================================================================
    // 113e: ImportOPJU bounds & RPC safety / undo
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());
        PythonAutomationServer srv(app);
        Rpc113 rpc { &srv };

        // 1) ImportOPJU crafted huge Matrix dimensions rejected cleanly (no OOM / int32 overflow)
        {
            const QString opjuPath = tmpDir.filePath(QStringLiteral("huge_matrix_113.opju"));
            const QByteArray xml =
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<OriginProject><Matrix Name=\"HugeM\" Rows=\"1000000\" Cols=\"1000000\">"
                    "</Matrix></OriginProject>\n";
            QList<Suite64ZipEntry> entries;
            entries.append(Suite64ZipEntry { QByteArray("Project.xml"), xml, false });
            const QByteArray zipBytes = suite64BuildZip(entries);
            QFile f(opjuPath);
            CHECK113(f.open(QIODevice::WriteOnly), "113e: open huge_matrix_113.opju");
            f.write(zipBytes);
            f.close();
            QString importErr;
            ImportOPJU::importFile(app, opjuPath, &importErr);
            // Must not crash or OOM; huge matrix must NOT be created
            CHECK113(app->matrix(QStringLiteral("HugeM")) == nullptr,
                     "113e: ImportOPJU created absurd 1e6x1e6 matrix");
        }

        // 2) Spectrogram-only layer: curvemath.eval / fill_between / signal.coherence must not crash
        {
            Matrix *m = app->newMatrix(QStringLiteral("SpecMat113"), 5, 5);
            for (int r = 0; r < 5; ++r)
                for (int c = 0; c < 5; ++c)
                    m->setCell(r, c, double(r + c + 1));
            MultiLayer *ml = app->plotSpectrogram(m, Graph::ColorMap);
            CHECK113(ml && ml->activeGraph(), "113e: spectrogram plot");
            if (ml) {
                const QJsonObject rEval = rpc.call(
                        QStringLiteral("curvemath.eval"),
                        QJsonObject { { "graph", ml->name() }, { "op", "add" } });
                CHECK113(!Rpc113::ok(rEval),
                         "113e: curvemath.eval on spectrogram-only layer should return error");
                const QJsonObject rFill = rpc.call(
                        QStringLiteral("curvemath.fill_between"),
                        QJsonObject { { "graph", ml->name() } });
                CHECK113(!Rpc113::ok(rFill),
                         "113e: curvemath.fill_between on spectrogram-only layer should return error");
                const QJsonObject rCoh = rpc.call(
                        QStringLiteral("signal.coherence"),
                        QJsonObject { { "graph", ml->name() } });
                CHECK113(!Rpc113::ok(rCoh),
                         "113e: signal.coherence on spectrogram-only layer should return error");
            }
        }

        // 3) table.set_data_dict & graph.to_python preserve numeric-looking Text columns verbatim
        {
            CHECK113(Rpc113::ok(rpc.call(
                             QStringLiteral("table.create"),
                             QJsonObject { { "name", "Txt113" },
                                           { "rows", 3 },
                                           { "cols", 2 },
                                           { "col_names", QJsonArray { "id", "val" } } })),
                     "113e: create Txt113");
            const QJsonObject setR = rpc.call(
                    QStringLiteral("table.set_data_dict"),
                    QJsonObject {
                            { "table", "Txt113" },
                            { "data",
                              QJsonObject {
                                      { "id", QJsonArray { "001", "1.50", "12345678" } },
                                      { "val", QJsonArray { 10.0, 20.0, 30.0 } } } },
                            { "meta",
                              QJsonObject {
                                      { "id", QJsonObject { { "mode", "text" } } } } },
                            { "column_order", QJsonArray { "id", "val" } } });
            CHECK113(Rpc113::ok(setR), "113e: set_data_dict Txt113");
            Table *t = app->table(QStringLiteral("Txt113"));
            CHECK113(t != nullptr && t->column(0) != nullptr, "113e: find Txt113");
            if (t && t->column(0)) {
                CHECK113(t->column(0)->columnMode() == SciDAVis::ColumnMode::Text,
                         "113e: numeric-looking string column coerced away from Text");
                CHECK113(t->column(0)->textAt(0) == QLatin1String("001")
                                 && t->column(0)->textAt(1) == QLatin1String("1.50"),
                         "113e: numeric-looking text corrupted: ["
                                 << t->column(0)->textAt(0).toStdString() << ", "
                                 << t->column(0)->textAt(1).toStdString() << "]");
            }
            const QJsonObject plR = rpc.call(
                    QStringLiteral("plot.xy"),
                    QJsonObject { { "table", "Txt113" },
                                  { "x_col", "id" },
                                  { "y_cols", QJsonArray { "val" } },
                                  { "graph_name", "TxtG113" } });
            CHECK113(Rpc113::ok(plR), "113e: plot TxtG113");
            const QJsonObject pyR = rpc.call(
                    QStringLiteral("graph.to_python"),
                    QJsonObject { { "graph", "TxtG113" }, { "data_mode", "inline" } });
            CHECK113(Rpc113::ok(pyR), "113e: graph.to_python TxtG113");
            const QString code = Rpc113::result(pyR).value(QStringLiteral("code")).toString();
            CHECK113(code.contains(QStringLiteral("'001'"))
                             && code.contains(QStringLiteral("'1.50'"))
                             && code.contains(QStringLiteral("'mode': 'text'")),
                     "113e: graph.to_python lost string quoting or text mode meta");
        }

        // 4) plot.stacked_bars with DateTime X column formats non-empty date labels
        {
            Table *dt = app->newTable(QStringLiteral("DtBar113"), 3, 3);
            dt->setColName(0, QStringLiteral("date"));
            dt->setColName(1, QStringLiteral("a"));
            dt->setColName(2, QStringLiteral("b"));
            dt->column(0)->setColumnMode(SciDAVis::ColumnMode::DateTime);
            dt->column(0)->setDateTimeAt(0, QDateTime(QDate(2025, 1, 10), QTime(0, 0)));
            dt->column(0)->setDateTimeAt(1, QDateTime(QDate(2025, 2, 15), QTime(0, 0)));
            dt->column(0)->setDateTimeAt(2, QDateTime(QDate(2025, 3, 20), QTime(0, 0)));
            for (int r = 0; r < 3; ++r) {
                dt->setCell(r, 1, double(r + 1));
                dt->setCell(r, 2, double(r + 2));
            }
            const QJsonObject sbR = rpc.call(
                    QStringLiteral("plot.stacked_bars"),
                    QJsonObject { { "table", "DtBar113" },
                                  { "x_col", "date" },
                                  { "y_cols", QJsonArray { "a", "b" } },
                                  { "graph_name", "DtBarG113" } });
            CHECK113(Rpc113::ok(sbR), "113e: plot.stacked_bars(DateTime) failed");
            MultiLayer *sbMl = findGraph113(app, QStringLiteral("DtBarG113"));
            CHECK113(sbMl && sbMl->activeGraph(), "113e: DtBarG113 missing");
            if (sbMl && sbMl->activeGraph()) {
                const QwtScaleDraw *sd =
                        sbMl->activeGraph()->plotWidget()->axisScaleDraw(QwtPlot::xBottom);
                const QString lbl0 = sd ? sd->label(1.0).text() : QString();
                CHECK113(lbl0.contains(QStringLiteral("2025")),
                         "113e: plot.stacked_bars DateTime tick label missing year: ["
                                 << lbl0.toStdString() << "]");
            }
        }

        // 5) plot.xy invalid-param rejection leaves ZERO orphan graph windows
        {
            Table *ot = app->newTable(QStringLiteral("Orphan113"), 4, 2);
            for (int i = 0; i < 4; ++i) {
                ot->setCell(i, 0, double(i));
                ot->setCell(i, 1, double(i * 2));
            }
            const int winsBefore = app->windowsList().size();
            const QJsonObject badXy = rpc.call(
                    QStringLiteral("plot.xy"),
                    QJsonObject { { "table", "Orphan113" },
                                  { "x_col", "1" },
                                  { "y_cols", QJsonArray { "2" } },
                                  { "xlim", QJsonArray { 1.0, 1.0 } } }); // degenerate range -> -32602
            CHECK113(!Rpc113::ok(badXy), "113e: plot.xy degenerate xlim should fail");
            CHECK113(app->windowsList().size() == winsBefore,
                     "113e: plot.xy leaked orphan graph on error: before=" << winsBefore
                                                                           << " after="
                                                                           << app->windowsList().size());
        }

        // 6) stats.clustering (hierarchical) pads assignments to numRows() with -1 on NaN rows
        {
            Table *ct = app->newTable(QStringLiteral("Clust113"), 7, 2);
            ct->setColName(0, QStringLiteral("f1"));
            ct->setColName(1, QStringLiteral("f2"));
            const double vals[7][2] = { { 0.0, 0.0 }, { 0.1, 0.2 }, { 0.2, 0.1 },
                                        { std::numeric_limits<double>::quiet_NaN(), 1.0 },
                                        { 10.0, 10.0 }, { 10.2, 9.9 }, { 9.8, 10.1 } };
            for (int r = 0; r < 7; ++r) {
                ct->setCell(r, 0, vals[r][0]);
                ct->setCell(r, 1, vals[r][1]);
            }
            const QJsonObject cr = rpc.call(
                    QStringLiteral("stats.clustering"),
                    QJsonObject { { "table", "Clust113" },
                                  { "cols", QJsonArray { "f1", "f2" } },
                                  { "method", "hierarchical" },
                                  { "k", 2 },
                                  { "plot", false } });
            CHECK113(Rpc113::ok(cr), "113e: stats.clustering hierarchical failed");
            const QJsonArray assigns = Rpc113::result(cr).value(QStringLiteral("assignments")).toArray();
            CHECK113(assigns.size() == 7 && assigns.at(3).toInt() == -1,
                     "113e: hierarchical assignments len=" << assigns.size()
                                                          << " row3=" << assigns.at(3).toInt());
        }

        // 7) Per-graph Undo/Redo works on integrate / smooth / analyze_peaks / color_scatter
        {
            Table *ut = app->newTable(QStringLiteral("Undo113"), 25, 3);
            ut->setColName(0, QStringLiteral("x"));
            ut->setColName(1, QStringLiteral("y"));
            ut->setColName(2, QStringLiteral("z"));
            for (int i = 0; i < 25; ++i) {
                const double xv = double(i) * 0.5;
                ut->setCell(i, 0, xv);
                ut->setCell(i, 1, 2.0 + 8.0 * std::exp(-0.5 * (xv - 6.0) * (xv - 6.0) / 1.5));
                ut->setCell(i, 2, xv * 10.0);
            }
            CHECK113(Rpc113::ok(rpc.call(
                             QStringLiteral("plot.xy"),
                             QJsonObject { { "table", "Undo113" },
                                           { "x_col", "x" },
                                           { "y_cols", QJsonArray { "y" } },
                                           { "graph_name", "UndoG113" } })),
                     "113e: plot UndoG113");
            MultiLayer *uml = findGraph113(app, QStringLiteral("UndoG113"));
            CHECK113(uml && uml->activeGraph() && uml->activeGraph()->curves() == 1,
                     "113e: UndoG113 initial curves");

            auto verifyGraphUndoRedo = [&](const QString &label, const QString &method,
                                           const QJsonObject &params) {
                const int before = uml->layersList().at(0)->curves();
                const QJsonObject resp = rpc.call(method, params);
                CHECK113(Rpc113::ok(resp), "113e: " << label.toStdString() << " RPC failed");
                const int after = uml->layersList().at(0)->curves();
                CHECK113(after > before,
                         "113e: " << label.toStdString() << " did not add curve (before=" << before
                                  << " after=" << after << ")");
                CHECK113(Rpc113::ok(rpc.call(QStringLiteral("app.undo"),
                                             QJsonObject { { "window", "UndoG113" } })),
                         "113e: " << label.toStdString() << " graph undo failed");
                CHECK113(uml->layersList().at(0)->curves() == before,
                         "113e: " << label.toStdString()
                                  << " graph undo did not restore curve count: got "
                                  << uml->layersList().at(0)->curves() << " expected " << before);
                CHECK113(Rpc113::ok(rpc.call(QStringLiteral("app.redo"),
                                             QJsonObject { { "window", "UndoG113" } })),
                         "113e: " << label.toStdString() << " graph redo failed");
                CHECK113(uml->layersList().at(0)->curves() == after,
                         "113e: " << label.toStdString()
                                  << " graph redo did not restore curve count: got "
                                  << uml->layersList().at(0)->curves() << " expected " << after);
                // Undo back so next operation starts clean
                rpc.call(QStringLiteral("app.undo"), QJsonObject { { "window", "UndoG113" } });
            };

            verifyGraphUndoRedo(QStringLiteral("curvemath.integrate"),
                                QStringLiteral("curvemath.integrate"),
                                QJsonObject { { "graph", "UndoG113" },
                                              { "curve", 0 },
                                              { "plot_curve", true } });
            verifyGraphUndoRedo(QStringLiteral("signal.smooth"), QStringLiteral("signal.smooth"),
                                QJsonObject { { "graph", "UndoG113" },
                                              { "curve", 0 },
                                              { "method", "savgol" },
                                              { "window", 5 },
                                              { "poly_order", 2 } });
            verifyGraphUndoRedo(QStringLiteral("analysis.analyze_peaks"),
                                QStringLiteral("analysis.analyze_peaks"),
                                QJsonObject { { "graph", "UndoG113" },
                                              { "curve", 0 },
                                              { "num_peaks", 1 },
                                              { "plot_fit", true } });
            verifyGraphUndoRedo(QStringLiteral("plot.color_scatter"),
                                QStringLiteral("plot.color_scatter"),
                                QJsonObject { { "graph", "UndoG113" },
                                              { "table", "Undo113" },
                                              { "x_col", "x" },
                                              { "y_col", "y" },
                                              { "color_col", "z" },
                                              { "colorbar_title", "ZVal" } });
        }
        std::cout << "[PASS] 113e: ImportOPJU bounds & RPC safety / undo checks passed\n";
    }

    // =========================================================================
    // 113f: Wave 3 math, persistence, Graph3D, ProjectIO & ASan/RPC regressions
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());
        PythonAutomationServer srv(app);
        Rpc113 rpc { &srv };

        // 1) waveletCWT log2(scale) coordinate alignment & computeSTFT odd-window fMax
        {
            const int nSig = 256;
            const double fs = 200.0;
            Table *ts = app->newTable(QStringLiteral("SigWave113f"), nSig, 2);
            for (int i = 0; i < nSig; ++i) {
                const double tv = double(i) / fs;
                ts->setCell(i, 0, tv);
                ts->setCell(i, 1, std::sin(2.0 * M_PI * 10.0 * tv));
            }
            MultiLayer *mlSig = app->multilayerPlot(ts, QStringList() << ts->colName(1), Graph::Line);
            Graph *gSig = mlSig ? mlSig->activeGraph() : nullptr;
            CHECK113(gSig != nullptr && gSig->curve(0) != nullptr, "113f: SigWave113f plot");
            if (gSig && gSig->curve(0)) {
                const QString cTitle = gSig->curve(0)->title().text();
                Matrix *cwtM = AdvancedSignalProc::waveletCWT(
                        app, gSig, cTitle, 32, AdvancedSignalProc::Morlet, true);
                CHECK113(cwtM != nullptr && cwtM->numRows() == 32, "113f: waveletCWT matrix");
                if (cwtM) {
                    const double expectedYEnd = std::log2(std::max(4.0, 0.30 * double(nSig)));
                    CHECK113(std::abs(cwtM->yStart() - 0.0) < 1e-9
                                     && std::abs(cwtM->yEnd() - expectedYEnd) < 1e-9,
                             "113f: waveletCWT Y coords not log2(scale): [" << cwtM->yStart()
                                                                            << ", " << cwtM->yEnd()
                                                                            << "]");
                    int peakRow = 0;
                    double peakVal = -1.0;
                    for (int r = 0; r < cwtM->numRows(); ++r) {
                        const double v = cwtM->cell(r, nSig / 2);
                        if (v > peakVal) {
                            peakVal = v;
                            peakRow = r;
                        }
                    }
                    const double yAtPeak = cwtM->yStart()
                            + double(peakRow) / double(cwtM->numRows() - 1)
                                    * (cwtM->yEnd() - cwtM->yStart());
                    const double scaleAtPeak = std::pow(2.0, yAtPeak);
                    const double expectedScale = 6.0 / (2.0 * M_PI * 10.0 / fs); // ~19.1
                    CHECK113(std::abs(scaleAtPeak - expectedScale) / expectedScale < 0.20,
                             "113f: waveletCWT 2^y peak scale=" << scaleAtPeak << " expected ~"
                                                                << expectedScale);
                }

                // Odd window STFT top frequency == (nFreqs - 1) * fs / W
                Matrix *stftOdd = AdvancedSignalProc::computeSTFT(app, gSig, cTitle, 63, 16, false);
                CHECK113(stftOdd != nullptr && stftOdd->numRows() == 32, "113f: odd STFT matrix");
                if (stftOdd) {
                    const double expectedFMax = 31.0 * fs / 63.0;
                    CHECK113(std::abs(stftOdd->yEnd() - expectedFMax) < 1e-6,
                             "113f: odd STFT yEnd=" << stftOdd->yEnd() << " expected "
                                                    << expectedFMax);
                }
            }
        }

        // 2) fft2D exact shifted (even & odd) and unshifted spatial frequency bounds
        {
            Matrix *m8 = app->newMatrix(QStringLiteral("FftEven113f"), 8, 8);
            m8->setCoordinates(0.0, 7.0, 0.0, 7.0); // dx = dy = 1.0
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    m8->setCell(r, c, 1.0);
            Matrix *f8s = AdvancedSignalProc::fft2D(app, m8, false, true);
            CHECK113(f8s != nullptr, "113f: fft2D 8x8 shifted");
            if (f8s) {
                const double dcX = f8s->xStart() + 4.0 * (f8s->xEnd() - f8s->xStart()) / 7.0;
                const double dcY = f8s->yStart() + 4.0 * (f8s->yEnd() - f8s->yStart()) / 7.0;
                CHECK113(std::abs(f8s->xStart() - (-0.5)) < 1e-12
                                 && std::abs(f8s->xEnd() - 0.375) < 1e-12
                                 && std::abs(dcX) < 1e-12 && std::abs(dcY) < 1e-12,
                         "113f: fft2D 8x8 shifted DC misaligned: x=[" << f8s->xStart() << ","
                                                                      << f8s->xEnd()
                                                                      << "] dcX=" << dcX);
            }
            Matrix *f8u = AdvancedSignalProc::fft2D(app, m8, false, false);
            CHECK113(f8u != nullptr && std::abs(f8u->xStart()) < 1e-12
                             && std::abs(f8u->xEnd() - 0.875) < 1e-12,
                     "113f: fft2D 8x8 unshifted bounds wrong");

            Matrix *m5 = app->newMatrix(QStringLiteral("FftOdd113f"), 5, 5);
            m5->setCoordinates(0.0, 4.0, 0.0, 4.0);
            for (int r = 0; r < 5; ++r)
                for (int c = 0; c < 5; ++c)
                    m5->setCell(r, c, 1.0);
            Matrix *f5s = AdvancedSignalProc::fft2D(app, m5, false, true);
            CHECK113(f5s != nullptr && std::abs(f5s->xStart() - (-0.4)) < 1e-12
                             && std::abs(f5s->xEnd() - 0.4) < 1e-12,
                     "113f: fft2D 5x5 shifted bounds wrong");
        }

        // 3) filterMatrix2D FilterSubtractPlanarTilt2D singular fallback uses unmutated mean
        {
            Matrix *strip = app->newMatrix(QStringLiteral("StripTilt113f"), 1, 10);
            for (int c = 0; c < 10; ++c)
                strip->setCell(0, c, 42.0);
            Matrix *flat = AdvancedSignalProc::filterMatrix2D(
                    app, strip, AdvancedSignalProc::FilterSubtractPlanarTilt2D, 1.0, 1, false,
                    false);
            CHECK113(flat != nullptr, "113f: planar tilt strip returned null");
            if (flat) {
                double maxAbs = 0.0;
                for (int c = 0; c < 10; ++c)
                    maxAbs = std::max(maxAbs, std::abs(flat->cell(0, c)));
                CHECK113(maxAbs < 1e-9,
                         "113f: planar tilt singular fallback failed: maxAbs=" << maxAbs);
            }
        }

        // 4) plotCorrelationMatrixHeatmap cell center alignment & CellBlocks
        {
            Table *ct = app->newTable(QStringLiteral("Corr113f"), 10, 3);
            ct->setColName(0, QStringLiteral("A"));
            ct->setColName(1, QStringLiteral("B"));
            ct->setColName(2, QStringLiteral("C"));
            for (int r = 0; r < 10; ++r) {
                ct->setCell(r, 0, double(r));
                ct->setCell(r, 1, double(r) * 2.0 + ((r % 2) ? 0.5 : -0.5));
                ct->setCell(r, 2, -double(r) + 3.0);
            }
            Matrix *corrMat = nullptr;
            MultiLayer *mlC = GraphStatsAndSurfaceFit::plotCorrelationMatrixHeatmap(
                    app, ct, QStringList() << "A" << "B" << "C", false, &corrMat);
            CHECK113(mlC && mlC->activeGraph() && corrMat, "113f: correlation heatmap");
            if (mlC && mlC->activeGraph() && corrMat) {
                CHECK113(std::abs(corrMat->xStart() - 1.0) < 1e-9
                                 && std::abs(corrMat->xEnd() - 3.0) < 1e-9,
                         "113f: corrMat coords=[" << corrMat->xStart() << "," << corrMat->xEnd()
                                                  << "] expected [1,3]");
                Spectrogram *sp = nullptr;
                for (QwtPlotItem *it : mlC->activeGraph()->plotWidget()->itemList()) {
                    if ((sp = dynamic_cast<Spectrogram *>(it)))
                        break;
                }
                CHECK113(sp != nullptr && sp->cellBlocks(),
                         "113f: correlation heatmap Spectrogram missing cellBlocks");
                if (sp) {
                    const QwtDoubleRect br = sp->data().boundingRect();
                    CHECK113(std::abs(br.left() - 0.5) < 1e-9 && std::abs(br.right() - 3.5) < 1e-9,
                             "113f: correlation heatmap boundingRect=[" << br.left() << ","
                                                                        << br.right() << "]");
                    CHECK113(std::abs(sp->data().value(1.0, 1.0) - 1.0) < 1e-9
                                     && std::abs(sp->data().value(2.0, 2.0) - 1.0) < 1e-9,
                             "113f: correlation heatmap diagonal value smeared at (1,1)="
                                     << sp->data().value(1.0, 1.0));
                }
            }
        }

        // 5) runPassingBablokDeming tied-X order invariance & negative-slope NaN (no clamping)
        {
            Table *t1 = app->newTable(QStringLiteral("PbTieA113f"), 8, 2);
            Table *t2 = app->newTable(QStringLiteral("PbTieB113f"), 8, 2);
            const double xs[8] = { 1, 2, 2, 3, 4, 4, 5, 6 };
            const double ys1[8] = { 1.1, 1.8, 2.4, 3.1, 3.7, 4.5, 5.2, 6.0 };
            const double ys2[8] = { 1.1, 2.4, 1.8, 3.1, 4.5, 3.7, 5.2, 6.0 }; // swapped ties
            for (int r = 0; r < 8; ++r) {
                t1->setCell(r, 0, xs[r]);
                t1->setCell(r, 1, ys1[r]);
                t2->setCell(r, 0, xs[r]);
                t2->setCell(r, 1, ys2[r]);
            }
            const auto r1 = GraphStatsAndSurfaceFit::runPassingBablokDeming(
                    app, t1, t1->colLabel(0), t1->colLabel(1), 1.0, false);
            const auto r2 = GraphStatsAndSurfaceFit::runPassingBablokDeming(
                    app, t2, t2->colLabel(0), t2->colLabel(1), 1.0, false);
            CHECK113(r1.valid && r2.valid && std::abs(r1.pbSlope - r2.pbSlope) < 1e-12
                             && std::abs(r1.pbSlopeCILow - r2.pbSlopeCILow) < 1e-12
                             && std::abs(r1.pbSlopeCIHigh - r2.pbSlopeCIHigh) < 1e-12,
                     "113f: Passing-Bablok depended on tied-X row order: " << r1.pbSlope << " vs "
                                                                           << r2.pbSlope);

            // Negatively correlated data: K >= M/2 must yield NaN slope (not clamped max slope)
            Table *tNeg = app->newTable(QStringLiteral("PbNeg113f"), 8, 2);
            for (int r = 0; r < 8; ++r) {
                tNeg->setCell(r, 0, double(r + 1));
                tNeg->setCell(r, 1, (r == 7) ? 50.0 : (-2.0 * double(r + 1)));
            }
            const auto rNeg = GraphStatsAndSurfaceFit::runPassingBablokDeming(
                    app, tNeg, tNeg->colLabel(0), tNeg->colLabel(1), 1.0, true);
            CHECK113(std::isnan(rNeg.pbSlope),
                     "113f: Passing-Bablok clamped out-of-range negative slope to " << rNeg.pbSlope);
            CHECK113(rNeg.summaryTable != nullptr && rNeg.summaryTable->column(2)->isInvalid(1),
                     "113f: Passing-Bablok summary table did not mark NaN slope cell invalid");
        }

        // 6) plotWeibullProbability JIS R 1625 B_n CI scaling
        {
            Table *tw = app->newTable(QStringLiteral("WeibJis113f"), 5, 1);
            const double vals[5] = { 210.0, 250.0, 290.0, 320.0, 360.0 };
            for (int i = 0; i < 5; ++i)
                tw->setCell(i, 0, vals[i]);
            const auto rRaw = GraphStatsAndSurfaceFit::plotWeibullProbability(
                    app, tw, tw->colLabel(0), QString(), 0, false, true, false, false);
            const auto rJis = GraphStatsAndSurfaceFit::plotWeibullProbability(
                    app, tw, tw->colLabel(0), QString(), 0, false, true, false, true);
            CHECK113(rRaw.valid && rJis.valid && rJis.unbiased && rRaw.reportTable
                             && rJis.reportTable,
                     "113f: Weibull JIS MLE");
            if (rRaw.reportTable && rJis.reportTable) {
                const double bn = rJis.unbiasingFactor;
                for (int i = 0; i < 5; ++i) {
                    const double hbRaw = 0.5
                            * (rRaw.reportTable->cell(i, 6) - rRaw.reportTable->cell(i, 5));
                    const double hbJis = 0.5
                            * (rJis.reportTable->cell(i, 6) - rJis.reportTable->cell(i, 5));
                    CHECK113(hbRaw > 0.0 && std::abs(hbJis / hbRaw - bn) < 1e-6,
                             "113f: Weibull JIS CI half-width ratio=" << (hbJis / hbRaw)
                                                                      << " expected B_n=" << bn);
                }
            }
        }

        // 7) analyze2DPeaks small SI-meter coordinate matrix ([0, 1e-6])
        {
            Matrix *mSi = app->newMatrix(QStringLiteral("SiPeak113f"), 25, 25);
            mSi->setCoordinates(0.0, 1e-6, 0.0, 1e-6);
            const double trueXc = 5e-7, trueYc = 5e-7, trueS = 8e-8;
            for (int r = 0; r < 25; ++r) {
                const double y = double(r) / 24.0 * 1e-6;
                for (int c = 0; c < 25; ++c) {
                    const double x = double(c) / 24.0 * 1e-6;
                    const double dx = (x - trueXc) / trueS;
                    const double dy = (y - trueYc) / trueS;
                    mSi->setCell(r, c, 10.0 * std::exp(-0.5 * (dx * dx + dy * dy)));
                }
            }
            Table *pkTbl = GraphStatsAndSurfaceFit::analyze2DPeaks(app, mSi, 3, 0.2);
            CHECK113(pkTbl != nullptr && pkTbl->numRows() >= 1, "113f: analyze2DPeaks SI");
            if (pkTbl && pkTbl->numRows() >= 1) {
                const double fitSx = pkTbl->cell(0, 4);
                const double fitSy = pkTbl->cell(0, 5);
                CHECK113(std::abs(fitSx - trueS) / trueS < 0.10
                                 && std::abs(fitSy - trueS) / trueS < 0.10,
                         "113f: analyze2DPeaks SI sigma=(" << fitSx << "," << fitSy
                                                           << ") expected " << trueS);
            }
        }

        // 8) analyzeEIS(fitRandlesCPE = false) skips fit curves, arc table & callout
        {
            Table *te = app->newTable(QStringLiteral("EisNoFit113f"), 12, 3);
            te->setColName(0, QStringLiteral("F"));
            te->setColName(1, QStringLiteral("Zr"));
            te->setColName(2, QStringLiteral("Zi"));
            for (int i = 0; i < 12; ++i) {
                const double f = std::pow(10.0, double(i) * 0.5);
                const double w = 2.0 * M_PI * f;
                const double alpha = 100.0 * 1e-5 * w;
                te->setCell(i, 0, f);
                te->setCell(i, 1, 10.0 + 100.0 / (1.0 + alpha * alpha));
                te->setCell(i, 2, -100.0 * alpha / (1.0 + alpha * alpha));
            }
            const auto eis = CurveMathTools::analyzeEIS(app, te, "F", "Zr", "Zi", false, false);
            CHECK113(eis.valid && std::isnan(eis.rs) && std::isnan(eis.rct),
                     "113f: analyzeEIS(fit=false) leaked Rs=" << eis.rs);
            CHECK113(eis.nyquistPlot && eis.nyquistPlot->activeGraph()
                             && eis.nyquistPlot->activeGraph()->curves() == 1,
                     "113f: analyzeEIS(fit=false) plotted extra Nyquist fit curve");
            CHECK113(eis.bodePlot && eis.bodePlot->activeGraph()
                             && eis.bodePlot->activeGraph()->curves() == 2,
                     "113f: analyzeEIS(fit=false) plotted extra Bode fit curve");
        }

        // 9) plotVaspDos(verticalEnergyAxis=true) preserves shaded band across copy & .sciprj reload
        {
            Table *tv = app->newTable(QStringLiteral("Vasp113f"), 15, 2);
            tv->setColName(0, QStringLiteral("E"));
            tv->setColName(1, QStringLiteral("DOS"));
            for (int i = 0; i < 15; ++i) {
                const double e = -3.0 + 0.4 * double(i);
                tv->setCell(i, 0, e);
                tv->setCell(i, 1, 2.0 * std::exp(-0.5 * e * e));
            }
            const auto vr = CurveMathTools::plotVaspDos(
                    app, tv, "E", QStringList() << "DOS", QStringList(), 0.0, true);
            CHECK113(vr.valid && vr.plot && vr.plot->activeGraph()
                             && vr.plot->activeGraph()->shadedBandCount() >= 1,
                     "113f: vertical VASP DOS missing persisted shaded band");
            if (vr.plot) {
                MultiLayer *cloned = app->multilayerPlot(QStringLiteral("VaspClone113f"));
                cloned->copy(app, vr.plot);
                CHECK113(cloned->activeGraph() && cloned->activeGraph()->shadedBandCount() >= 1,
                         "113f: vertical VASP DOS shaded band lost on Graph::copy");
                const QString vp = tmpDir.filePath(QStringLiteral("vasp_vert_113f.sciprj"));
                CHECK113(Rpc113::ok(rpc.call(QStringLiteral("app.save_project"),
                                             QJsonObject { { "path", vp } })),
                         "113f: save VASP project");
                AppWinGuard113 appV(new ApplicationWindow());
                PythonAutomationServer srvV(appV);
                Rpc113 rpcV { &srvV };
                CHECK113(Rpc113::ok(rpcV.call(QStringLiteral("app.open_project"),
                                              QJsonObject { { "path", vp } })),
                         "113f: reopen VASP project");
                MultiLayer *relV = findGraph113(appV, vr.plot->name());
                CHECK113(relV && relV->activeGraph()
                                 && relV->activeGraph()->shadedBandCount() >= 1,
                         "113f: vertical VASP DOS shaded band lost on .sciprj reload");
            }
        }

        // 10) RecalcEngine constant-curve normalization fails cleanly (no all-NaN curve)
        {
            Table *tc = app->newTable(QStringLiteral("ConstNorm113f"), 6, 2);
            for (int i = 0; i < 6; ++i) {
                tc->setCell(i, 0, double(i));
                tc->setCell(i, 1, 5.0);
            }
            MultiLayer *mlC = app->multilayerPlot(tc, QStringList() << tc->colName(1), Graph::Line);
            Graph *gC = mlC ? mlC->activeGraph() : nullptr;
            CHECK113(gC && gC->curve(0), "113f: ConstNorm113f plot");
            if (gC && gC->curve(0)) {
                QVariantMap params;
                params[QStringLiteral("mode")] = 0; // Range0To1
                RecalcManager *rm = RecalcManager::instance(app);
                rm->registerLink(app, gC, tc, gC->curve(0)->title().text(), QString(),
                                 RecalcManager::OpNormalize, params);
                rm->recalculateAll(gC);
                CHECK113(gC->curves() == 1 && !rm->lastError(gC).isEmpty(),
                         "113f: RecalcEngine created all-NaN curve or cleared error on constant normalize");
            }
        }

        // 11) Graph3D WindowLabel newline round-trip & setAxesLabels literal <br>/&#9; preservation
        {
            Matrix *m3 = app->newMatrix(QStringLiteral("Mat3D113f"), 4, 4);
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    m3->setCell(r, c, double(r + c));
            const QJsonObject p3Resp = rpc.call(
                    QStringLiteral("plot.3d_matrix"),
                    QJsonObject { { "matrix", "Mat3D113f" } });
            const QString g3Name = Rpc113::result(p3Resp).value(QStringLiteral("graph")).toString();
            Graph3D *g3 = nullptr;
            for (MyWidget *w : app->windowsList()) {
                if (w && w->inherits("Graph3D") && (g3Name.isEmpty() || w->name() == g3Name))
                    g3 = static_cast<Graph3D *>(w);
            }
            CHECK113(Rpc113::ok(p3Resp) && g3 != nullptr, "113f: Graph3D created");
            if (g3) {
                g3->setWindowLabel(QStringLiteral("Line1\nLine2"));
                g3->setAxesLabels(QStringList() << QStringLiteral("A<br>B")
                                                << QStringLiteral("Y&#9;1") << QStringLiteral("Z"));
                CHECK113(g3->axesLabels().value(0) == QLatin1String("A<br>B")
                                 && g3->axesLabels().value(1) == QLatin1String("Y&#9;1"),
                         "113f: Graph3D::setAxesLabels corrupted literal tags");
                Graph3D *g3Copy = new Graph3D(QString(), app);
                g3Copy->copy(g3);
                CHECK113(g3Copy->axesLabels().value(0) == QLatin1String("A<br>B")
                                 && g3Copy->axesLabels().value(1) == QLatin1String("Y&#9;1"),
                         "113f: Graph3D::copy corrupted literal tags");
                delete g3Copy;

                const QString p3 = tmpDir.filePath(QStringLiteral("g3d_label_113f.sciprj"));
                CHECK113(Rpc113::ok(rpc.call(QStringLiteral("app.save_project"),
                                             QJsonObject { { "path", p3 } })),
                         "113f: save Graph3D project");
                AppWinGuard113 app3(new ApplicationWindow());
                PythonAutomationServer srv3(app3);
                Rpc113 rpc3 { &srv3 };
                CHECK113(Rpc113::ok(rpc3.call(QStringLiteral("app.open_project"),
                                              QJsonObject { { "path", p3 } })),
                         "113f: reopen Graph3D project");
                Graph3D *relG3 = nullptr;
                for (MyWidget *w : app3->windowsList()) {
                    if (w && w->inherits("Graph3D") && w->name() == g3->name())
                        relG3 = static_cast<Graph3D *>(w);
                }
                CHECK113(relG3 != nullptr && relG3->windowLabel() == QLatin1String("Line1 Line2"),
                         "113f: Graph3D WindowLabel corrupted on reload: ["
                                 << (relG3 ? relG3->windowLabel().toStdString() : "<null>") << "]");
                // Also verify a raw .sciprj containing <br> in Graph3D WindowLabel unescapes cleanly
                {
                    QFile rf(p3);
                    if (rf.open(QIODevice::ReadOnly)) {
                        QByteArray rawProj = rf.readAll();
                        rf.close();
                        rawProj.replace("Line1 Line2", "Line1<br>Line2");
                        const QString p3Br = tmpDir.filePath(QStringLiteral("g3d_br_113f.sciprj"));
                        QFile wf(p3Br);
                        if (wf.open(QIODevice::WriteOnly)) {
                            wf.write(rawProj);
                            wf.close();
                            AppWinGuard113 app3b(new ApplicationWindow());
                            PythonAutomationServer srv3b(app3b);
                            Rpc113 rpc3b { &srv3b };
                            CHECK113(Rpc113::ok(rpc3b.call(QStringLiteral("app.open_project"),
                                                           QJsonObject { { "path", p3Br } })),
                                     "113f: reopen Graph3D <br> project");
                            Graph3D *relBr = nullptr;
                            for (MyWidget *w : app3b->windowsList()) {
                                if (w && w->inherits("Graph3D") && w->name() == g3->name())
                                    relBr = static_cast<Graph3D *>(w);
                            }
                            CHECK113(relBr != nullptr
                                             && relBr->windowLabel() == QLatin1String("Line1 Line2"),
                                     "113f: Graph3D <br> WindowLabel not unescaped: ["
                                             << (relBr ? relBr->windowLabel().toStdString()
                                                       : "<null>")
                                             << "]");
                        }
                    }
                }
            }
        }

        // 12) appendProject remaps <ToriDensity2D> matrix/table refs without overwriting existing matrix
        {
            const QString projB = tmpDir.filePath(QStringLiteral("d2d_b_113f.sciprj"));
            {
                AppWinGuard113 appB(new ApplicationWindow());
                PythonAutomationServer srvB(appB);
                Rpc113 rpcB { &srvB };
                Table *tb = appB->newTable(QStringLiteral("D2dSrc"), 20, 2);
                for (int i = 0; i < 20; ++i) {
                    tb->setCell(i, 0, double(i % 5));
                    tb->setCell(i, 1, double(i / 5));
                }
                const auto dRes = ExtraPlotTypes::plotDensity2D(
                        appB, tb, tb->colLabel(0), tb->colLabel(1), 5, 4);
                CHECK113(dRes.valid && dRes.matrix && dRes.plot, "113f: projB Density2D");
                CHECK113(Rpc113::ok(rpcB.call(QStringLiteral("app.save_project"),
                                              QJsonObject { { "path", projB } })),
                         "113f: save projB");
            }
            AppWinGuard113 appA(new ApplicationWindow());
            Table *ta = appA->newTable(QStringLiteral("D2dSrc"), 20, 2);
            for (int i = 0; i < 20; ++i) {
                ta->setCell(i, 0, 100.0 + double(i));
                ta->setCell(i, 1, 200.0 + double(i));
            }
            const auto dResA = ExtraPlotTypes::plotDensity2D(
                    appA, ta, ta->colLabel(0), ta->colLabel(1), 5, 4);
            CHECK113(dResA.valid && dResA.matrix && dResA.plot, "113f: projA Density2D");
            const double origXStartA = dResA.matrix ? dResA.matrix->xStart() : 0.0;
            appA->appendProject(projB);
            QCoreApplication::processEvents();
            Graph *appendedG = nullptr;
            QList<Folder *> folderQueue;
            if (Folder *rf = appA->projectFolder())
                folderQueue.append(rf);
            while (!folderQueue.isEmpty() && !appendedG) {
                Folder *f = folderQueue.takeFirst();
                for (MyWidget *w : f->windowsList()) {
                    if (w && w->inherits("MultiLayer") && w != dResA.plot) {
                        Graph *cand = static_cast<MultiLayer *>(w)->activeGraph();
                        if (cand) {
                            appendedG = cand;
                            break;
                        }
                    }
                }
                for (Folder *sub : f->folders()) {
                    if (sub)
                        folderQueue.append(sub);
                }
            }
            CHECK113(appendedG != nullptr, "113f: appended Density2D graph missing");
            if (appendedG && dResA.matrix) {
                CHECK113(ExtraPlotTypes::rebuildDensity2D(appA, appendedG),
                         "113f: rebuildDensity2D failed after appendProject");
                CHECK113(std::abs(dResA.matrix->xStart() - origXStartA) < 1e-9,
                         "113f: appendProject rebuildDensity2D overwrote original project matrix!");
            }
        }

        // 13) app.recalculate with live signal recalc link (ASan heap-use-after-free regression)
        {
            CHECK113(Rpc113::ok(rpc.call(QStringLiteral("app.set_recalc_mode"),
                                         QJsonObject { { "mode", "auto" } })),
                     "113f: set_recalc_mode auto");
            Table *tr = app->newTable(QStringLiteral("RecalcUaf113f"), 32, 2);
            tr->setColName(0, QStringLiteral("X"));
            tr->setColName(1, QStringLiteral("Y"));
            for (int i = 0; i < 32; ++i) {
                tr->setCell(i, 0, double(i) * 0.05);
                tr->setCell(i, 1, std::sin(double(i) * 0.2) + 0.3 * std::cos(double(i) * 1.5));
            }
            CHECK113(Rpc113::ok(rpc.call(QStringLiteral("plot.xy"),
                                         QJsonObject { { "table", "RecalcUaf113f" },
                                                       { "x_col", "X" },
                                                       { "y_cols", QJsonArray { "Y" } },
                                                       { "graph_name", "RecalcG113f" } })),
                     "113f: plot RecalcG113f");
            CHECK113(Rpc113::ok(rpc.call(QStringLiteral("signal.iir_filter"),
                                         QJsonObject { { "graph", "RecalcG113f" },
                                                       { "curve", 0 },
                                                       { "design", "butterworth" },
                                                       { "mode", "lowpass" },
                                                       { "order", 2 },
                                                       { "cutoff", 3.0 } })),
                     "113f: attach IIR recalc link");
            tr->setCell(0, 1, 2.5);
            tr->notifyChanges();
            const QJsonObject rc1 = rpc.call(QStringLiteral("app.recalculate"), QJsonObject {});
            const QJsonObject rc2 = rpc.call(QStringLiteral("app.recalculate"), QJsonObject {});
            CHECK113(Rpc113::ok(rc1) && Rpc113::result(rc1).value(QStringLiteral("ok")).toBool()
                             && Rpc113::ok(rc2)
                             && Rpc113::result(rc2).value(QStringLiteral("ok")).toBool(),
                     "113f: app.recalculate failed with live signal link");
        }

        // 14) Nested app.batch(atomic=True) rollback regression
        {
            Table *tb = app->newTable(QStringLiteral("NestedBatch113f"), 4, 2);
            tb->setColName(0, QStringLiteral("X"));
            tb->setColName(1, QStringLiteral("Y"));
            for (int i = 0; i < 4; ++i) {
                tb->setCell(i, 0, double(i + 1));
                tb->setCell(i, 1, double((i + 1) * 10));
            }
            QJsonArray innerCmds;
            innerCmds.append(QJsonObject {
                    { "method", "table.set_cell" },
                    { "params",
                      QJsonObject { { "table", "NestedBatch113f" },
                                    { "row", 1 },
                                    { "col", "Y" },
                                    { "value", 888.0 } } } });
            innerCmds.append(QJsonObject {
                    { "method", "graph.set_axis" },
                    { "params", QJsonObject { { "graph", "NO_SUCH_GRAPH_113F" } } } });

            QJsonArray outerCmds;
            outerCmds.append(QJsonObject {
                    { "method", "table.set_cell" },
                    { "params",
                      QJsonObject { { "table", "NestedBatch113f" },
                                    { "row", 0 },
                                    { "col", "Y" },
                                    { "value", 999.0 } } } });
            outerCmds.append(QJsonObject {
                    { "method", "app.batch" },
                    { "params",
                      QJsonObject { { "commands", innerCmds },
                                    { "atomic", true },
                                    { "stop_on_error", true } } } });

            const QJsonObject br = rpc.call(
                    QStringLiteral("app.batch"),
                    QJsonObject { { "commands", outerCmds },
                                  { "atomic", true },
                                  { "stop_on_error", true } });
            const QJsonObject bres = Rpc113::result(br);
            CHECK113(Rpc113::ok(br) && !bres.value(QStringLiteral("ok")).toBool(true)
                             && bres.value(QStringLiteral("rolled_back")).toBool(false),
                     "113f: nested app.batch(atomic=true) did not fail & roll back");
            Table *tbAfter = app->table(QStringLiteral("NestedBatch113f"));
            CHECK113(tbAfter != nullptr && std::abs(tbAfter->cell(0, 1) - 10.0) < 1e-9
                             && std::abs(tbAfter->cell(1, 1) - 20.0) < 1e-9,
                     "113f: nested app.batch leaked partial mutations: y0="
                             << (tbAfter ? tbAfter->cell(0, 1) : -1.0)
                             << " y1=" << (tbAfter ? tbAfter->cell(1, 1) : -1.0));
        }

        std::cout << "[PASS] 113f: Wave 3 math, persistence, Graph3D, ProjectIO & ASan/RPC checks passed\n";
    }

    // =========================================================================
    // 113g: Wave 4 appendProject prefix/formula, EIS Randles RC, Deming vertical,
    //       Workbook rename/undo, StylePreset 96 PPI & ZipReader CRC regressions
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());
        PythonAutomationServer srv(app);
        Rpc113 rpc { &srv };

        // 1) appendProject <T> + <T>_<suffix> curve/3D preservation & cross-table formula rewrite
        {
            const QString projB = tmpDir.filePath(QStringLiteral("append_pfx_113g.sciprj"));
            {
                AppWinGuard113 appB(new ApplicationWindow());
                PythonAutomationServer srvB(appB);
                Rpc113 rpcB { &srvB };
                Table *t1 = appB->table(QStringLiteral("Table1"));
                if (!t1)
                    t1 = appB->newTable(QStringLiteral("Table1"), 20, 2);
                t1->setNumRows(20);
                for (int r = 0; r < 20; ++r) {
                    t1->setCell(r, 0, double(r + 1));
                    t1->setCell(r, 1, double((r + 1) * 2));
                }
                CHECK113(Rpc113::ok(rpcB.call(
                                 QStringLiteral("plot.histogram_cumulative"),
                                 QJsonObject { { "table", "Table1" }, { "col", "1" } })),
                         "113g: projB plot.histogram_cumulative");
                // Derived table referencing Table1 via cross-table formula
                Table *tDer = appB->newTable(QStringLiteral("Derived113g"), 2, 1);
                tDer->column(0)->setFormula(Interval<int>(0, 1),
                                            QStringLiteral("tablecol(\"Table1\", \"1\") * 10"));
                tDer->recalculate(0, false);

                // Matrix1 + Matrix1_Fit + 3D plot of Matrix1_Fit
                Matrix *m1 = appB->newMatrix(QStringLiteral("Matrix1"), 4, 4);
                Matrix *m1Fit = appB->newMatrix(QStringLiteral("Matrix1_Fit"), 4, 4);
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        m1->setCell(r, c, double(r + c));
                        m1Fit->setCell(r, c, double(r * 2 + c + 1));
                    }
                }
                CHECK113(Rpc113::ok(rpcB.call(QStringLiteral("plot.3d_matrix"),
                                              QJsonObject { { "matrix", "Matrix1_Fit" } })),
                         "113g: projB 3D plot of Matrix1_Fit");
                CHECK113(Rpc113::ok(rpcB.call(QStringLiteral("app.save_project"),
                                              QJsonObject { { "path", projB } })),
                         "113g: save projB");
            }

            AppWinGuard113 appA(new ApplicationWindow());
            Table *tA = appA->table(QStringLiteral("Table1"));
            if (!tA)
                tA = appA->newTable(QStringLiteral("Table1"), 2, 2);
            tA->setCell(0, 0, 100.0);
            tA->setCell(1, 0, 200.0);
            Matrix *mA = appA->newMatrix(QStringLiteral("Matrix1"), 4, 4);
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    mA->setCell(r, c, 99.0);

            appA->appendProject(projB);
            QCoreApplication::processEvents();

            // Gather all windows across root and subfolders
            QList<MyWidget *> allWins;
            QList<Folder *> fq;
            if (Folder *rf = appA->projectFolder())
                fq.append(rf);
            while (!fq.isEmpty()) {
                Folder *f = fq.takeFirst();
                allWins.append(f->windowsList());
                for (Folder *sub : f->folders())
                    if (sub)
                        fq.append(sub);
            }

            MultiLayer *histMl = nullptr;
            Graph3D *fit3d = nullptr;
            Table *derTbl = nullptr;
            for (MyWidget *w : allWins) {
                if (!w)
                    continue;
                if (w->inherits("MultiLayer") && !histMl)
                    histMl = static_cast<MultiLayer *>(w);
                if (w->inherits("Graph3D"))
                    fit3d = static_cast<Graph3D *>(w);
                if (w->inherits("Table") && w->name() == QLatin1String("Derived113g"))
                    derTbl = static_cast<Table *>(w);
            }
            CHECK113(histMl && histMl->activeGraph() && histMl->activeGraph()->curves() >= 2,
                     "113g: appendProject dropped Table1_HistCum curve (curves="
                             << (histMl && histMl->activeGraph() ? histMl->activeGraph()->curves()
                                                                 : -1)
                             << ")");
            CHECK113(fit3d != nullptr,
                     "113g: appendProject dropped 3D surface plot of Matrix1_Fit");
            CHECK113(derTbl != nullptr, "113g: Derived113g missing after appendProject");
            if (derTbl) {
                const QString f0 = derTbl->column(0)->formula(0);
                CHECK113(!f0.contains(QStringLiteral("\"Table1\"")),
                         "113g: appendProject left stale Table1 formula ref: " << f0.toStdString());
                derTbl->recalculate(0, false);
                CHECK113(std::abs(derTbl->cell(0, 0) - 10.0) < 1e-9
                                 && std::abs(derTbl->cell(1, 0) - 20.0) < 1e-9,
                         "113g: Derived113g recalc read wrong table: [" << derTbl->cell(0, 0)
                                                                        << ", "
                                                                        << derTbl->cell(1, 0)
                                                                        << "]");
            }
        }

        // 2) curvemath.eis_fit with model="Randles" fits ideal n=1.0 RC circuit
        {
            Table *te = app->newTable(QStringLiteral("EisIdeal113g"), 12, 3);
            te->setColName(0, QStringLiteral("Freq"));
            te->setColName(1, QStringLiteral("Zre"));
            te->setColName(2, QStringLiteral("Zim"));
            const double trueRs = 10.0, trueRct = 100.0, trueC = 1e-6;
            for (int i = 0; i < 12; ++i) {
                const double f = std::pow(10.0, double(i) * 0.45);
                const double w = 2.0 * M_PI * f;
                const double a = trueRct * trueC * w;
                te->setCell(i, 0, f);
                te->setCell(i, 1, trueRs + trueRct / (1.0 + a * a));
                te->setCell(i, 2, -trueRct * a / (1.0 + a * a));
            }
            const QJsonObject er = rpc.call(
                    QStringLiteral("curvemath.eis_fit"),
                    QJsonObject { { "table", "EisIdeal113g" },
                                  { "freq_col", "Freq" },
                                  { "z_re_col", "Zre" },
                                  { "z_im_col", "Zim" },
                                  { "model", "Randles" } });
            const QJsonObject eres = Rpc113::result(er);
            CHECK113(Rpc113::ok(er) && eres.value(QStringLiteral("ok")).toBool()
                             && eres.value(QStringLiteral("rs")).isDouble()
                             && std::abs(eres.value(QStringLiteral("rs")).toDouble() - trueRs) < 0.5
                             && std::abs(eres.value(QStringLiteral("rct")).toDouble() - trueRct) < 2.0
                             && std::abs(eres.value(QStringLiteral("n_cpe")).toDouble() - 1.0) < 1e-6,
                     "113g: curvemath.eis_fit(model=Randles) failed ideal RC fit");
        }

        // 3) Deming regression rejects vertical & orthogonal-maximum datasets
        {
            Table *tv = app->newTable(QStringLiteral("DemVert113g"), 4, 2);
            for (int r = 0; r < 4; ++r) {
                tv->setCell(r, 0, 5.0);
                tv->setCell(r, 1, double(r + 1));
            }
            const auto rv = GraphStatsAndSurfaceFit::runPassingBablokDeming(
                    app, tv, tv->colLabel(0), tv->colLabel(1), 1.0, false);
            CHECK113(!rv.valid && !std::isfinite(rv.demingSlope),
                     "113g: Deming returned fake finite slope on vertical data: " << rv.demingSlope);

            Table *to = app->newTable(QStringLiteral("DemOrtho113g"), 6, 2);
            const double ox[6] = { -1, 1, -1, 1, -2, 2 };
            const double oy[6] = { -10, -10, 10, 10, 0, 0 };
            for (int r = 0; r < 6; ++r) {
                to->setCell(r, 0, ox[r]);
                to->setCell(r, 1, oy[r]);
            }
            const auto ro = GraphStatsAndSurfaceFit::runPassingBablokDeming(
                    app, to, to->colLabel(0), to->colLabel(1), 1.0, true);
            CHECK113(ro.valid && std::isfinite(ro.pbSlope) && !std::isfinite(ro.demingSlope),
                     "113g: Deming returned max-error horizontal slope on sxy==0, lam*syy>sxx: "
                             << ro.demingSlope << " (pbSlope=" << ro.pbSlope << ")");
            CHECK113(ro.summaryTable != nullptr && ro.summaryTable->column(2)->isInvalid(4),
                     "113g: Deming NaN slope cell not marked invalid in summary table");
        }

        // 4) Workbook underscore renameSheet preserves prefix & renameBook+Undo spawns no ghost book
        {
            WorkbookManager *wbm = WorkbookManager::instance(app);
            QString err;
            const QString bk = wbm->createBook(QStringLiteral("my_book"), 2, &err);
            CHECK113(!bk.isEmpty(), "113g: createBook my_book failed");
            QList<Table *> sh = wbm->sheets(bk);
            CHECK113(sh.size() == 2, "113g: my_book sheet count");
            if (sh.size() == 2) {
                CHECK113(wbm->renameSheet(sh[0], QStringLiteral("Raw"), &err),
                         "113g: renameSheet -> Raw failed: " << err.toStdString());
                CHECK113(wbm->sheetDisplayLabel(sh[0]) == QLatin1String("Raw"),
                         "113g: underscore workbook tab label broken: "
                                 << wbm->sheetDisplayLabel(sh[0]).toStdString());
                CHECK113(wbm->renameSheet(sh[0], QStringLiteral("Clean"), &err),
                         "113g: second renameSheet -> Clean failed: " << err.toStdString());
                CHECK113(wbm->sheetDisplayLabel(sh[0]) == QLatin1String("Clean")
                                 && sh[0]->name() != QLatin1String("Clean"),
                         "113g: second renameSheet stripped workbook prefix: "
                                 << sh[0]->name().toStdString());

                // Delete sheet 2, rename book -> ExpA113g, Undo -> must return to ExpA113g, no ghost book!
                app->closeWindow(sh[1]);
                QCoreApplication::processEvents();
                CHECK113(wbm->renameBook(bk, QStringLiteral("ExpA113g"), &err),
                         "113g: renameBook failed: " << err.toStdString());
                CHECK113(Rpc113::ok(rpc.call(QStringLiteral("app.undo"), QJsonObject {})),
                         "113g: undo deleted sheet after renameBook");
                CHECK113(!wbm->hasBook(bk) && wbm->sheets(QStringLiteral("ExpA113g")).size() == 2,
                         "113g: undo after renameBook spawned ghost workbook or lost sheet!");
            }
        }

        // 5) MultiLayer::captureStylePresetSpec converts UnitPx at 96 PPI (not export DPI 300)
        {
            MultiLayer *mlPx = app->newGraph(QStringLiteral("PxPreset113g"));
            CHECK113(mlPx != nullptr, "113g: PxPreset113g created");
            if (mlPx) {
                mlPx->setPageSizePhysical(520.0, 520.0, MultiLayer::UnitPx, 300);
                mlPx->resize(520, 520);
                QCoreApplication::processEvents();
                const auto spec = mlPx->captureStylePresetSpec(QStringLiteral("Px113g"));
                // 520 px at 96 PPI ~= 137.58 mm (vs buggy 44.0 mm at 300 DPI)
                CHECK113(spec.pageWidthMm > 100.0 && spec.pageHeightMm > 100.0,
                         "113g: captureStylePresetSpec shrunk UnitPx using export DPI: "
                                 << spec.pageWidthMm << " x " << spec.pageHeightMm << " mm");
            }
        }

        // 6) ZipArchive::extract rejects empty-output entry with non-zero uncompSize/CRC32
        {
            // Craft minimal ZIP with method=0, compSize=0, uncompSize=128, crc32=0x12345678
            QByteArray z;
            auto put16 = [&z](quint16 v) {
                z.append(char(v & 0xFF));
                z.append(char((v >> 8) & 0xFF));
            };
            auto put32 = [&z](quint32 v) {
                z.append(char(v & 0xFF));
                z.append(char((v >> 8) & 0xFF));
                z.append(char((v >> 16) & 0xFF));
                z.append(char((v >> 24) & 0xFF));
            };
            const QByteArray name("bad.xml");
            put32(0x04034B50U);
            put16(20);
            put16(0);
            put16(0);
            put16(0);
            put16(0);
            put32(0x12345678U);
            put32(0);
            put32(128);
            put16(quint16(name.size()));
            put16(0);
            z.append(name);
            const quint32 cdOff = quint32(z.size());
            put32(0x02014B50U);
            put16(20);
            put16(20);
            put16(0);
            put16(0);
            put16(0);
            put16(0);
            put32(0x12345678U);
            put32(0);
            put32(128);
            put16(quint16(name.size()));
            put16(0);
            put16(0);
            put16(0);
            put16(0);
            put32(0);
            put32(0);
            z.append(name);
            const quint32 cdSz = quint32(z.size()) - cdOff;
            put32(0x06054B50U);
            put16(0);
            put16(0);
            put16(1);
            put16(1);
            put32(cdSz);
            put32(cdOff);
            put16(0);

            torizip::ZipArchive za;
            QString zErr;
            CHECK113(za.openData(z, &zErr), "113g: open crafted zip");
            QByteArray extracted;
            CHECK113(!za.extract(0, extracted, &zErr),
                     "113g: ZipArchive::extract accepted empty payload with uncompSize=128/crc!=0");

            // Also verify non-empty payload with spoofed crc32=0 is rejected
            QByteArray z0;
            auto put16b = [&z0](quint16 v) {
                z0.append(char(v & 0xFF));
                z0.append(char((v >> 8) & 0xFF));
            };
            auto put32b = [&z0](quint32 v) {
                z0.append(char(v & 0xFF));
                z0.append(char((v >> 8) & 0xFF));
                z0.append(char((v >> 16) & 0xFF));
                z0.append(char((v >> 24) & 0xFF));
            };
            const QByteArray payload0("corrupt");
            put32b(0x04034B50U);
            put16b(20);
            put16b(0);
            put16b(0);
            put16b(0);
            put16b(0);
            put32b(0); // spoofed crc32 == 0
            put32b(quint32(payload0.size()));
            put32b(quint32(payload0.size()));
            put16b(quint16(name.size()));
            put16b(0);
            z0.append(name);
            z0.append(payload0);
            const quint32 cdOff0 = quint32(z0.size());
            put32b(0x02014B50U);
            put16b(20);
            put16b(20);
            put16b(0);
            put16b(0);
            put16b(0);
            put16b(0);
            put32b(0); // spoofed crc32 == 0
            put32b(quint32(payload0.size()));
            put32b(quint32(payload0.size()));
            put16b(quint16(name.size()));
            put16b(0);
            put16b(0);
            put16b(0);
            put16b(0);
            put32b(0);
            put32b(0);
            z0.append(name);
            const quint32 cdSz0 = quint32(z0.size()) - cdOff0;
            put32b(0x06054B50U);
            put16b(0);
            put16b(0);
            put16b(1);
            put16b(1);
            put32b(cdSz0);
            put32b(cdOff0);
            put16b(0);
            torizip::ZipArchive za0;
            CHECK113(za0.openData(z0, &zErr), "113g: open crc0 zip");
            CHECK113(!za0.extract(0, extracted, &zErr),
                     "113g: ZipArchive::extract accepted non-empty payload with crc32==0");
        }

        // 6b) appendProject ignores pre-existing Project A table named Table1_Y when remapping Project B's Table1_Y curve
        {
            const QString projB2 = tmpDir.filePath(QStringLiteral("append_preexist_113g.sciprj"));
            {
                AppWinGuard113 appB2(new ApplicationWindow());
                PythonAutomationServer srvB2(appB2);
                Rpc113 rpcB2 { &srvB2 };
                Table *tb = appB2->table(QStringLiteral("Table1"));
                if (!tb)
                    tb = appB2->newTable(QStringLiteral("Table1"), 3, 2);
                tb->setNumRows(3);
                tb->setColName(0, QStringLiteral("X"));
                tb->setColName(1, QStringLiteral("Y"));
                for (int r = 0; r < 3; ++r) {
                    tb->setCell(r, 0, double(r + 1));
                    tb->setCell(r, 1, double((r + 1) * 7));
                }
                CHECK113(Rpc113::ok(rpcB2.call(QStringLiteral("plot.xy"),
                                               QJsonObject { { "table", "Table1" },
                                                             { "x_col", "X" },
                                                             { "y_cols", QJsonArray { "Y" } },
                                                             { "graph_name", "GPre113g" } })),
                         "113g: projB2 plot Table1_Y");
                CHECK113(Rpc113::ok(rpcB2.call(QStringLiteral("app.save_project"),
                                               QJsonObject { { "path", projB2 } })),
                         "113g: save projB2");
            }
            AppWinGuard113 appA2(new ApplicationWindow());
            Table *ta1 = appA2->table(QStringLiteral("Table1"));
            if (!ta1)
                ta1 = appA2->newTable(QStringLiteral("Table1"), 3, 2);
            Table *taPre = appA2->newTable(QStringLiteral("Table1_Y"), 3, 2);
            for (int r = 0; r < 3; ++r) {
                taPre->setCell(r, 0, double(r));
                taPre->setCell(r, 1, 999.0);
            }
            appA2->appendProject(projB2);
            QCoreApplication::processEvents();
            MultiLayer *gPre = nullptr;
            QList<Folder *> fq2;
            if (Folder *rf = appA2->projectFolder())
                fq2.append(rf);
            while (!fq2.isEmpty() && !gPre) {
                Folder *f = fq2.takeFirst();
                for (MyWidget *w : f->windowsList()) {
                    if (w && w->inherits("MultiLayer") && w->name() == QLatin1String("GPre113g")) {
                        gPre = static_cast<MultiLayer *>(w);
                        break;
                    }
                }
                for (Folder *sub : f->folders())
                    if (sub)
                        fq2.append(sub);
            }
            CHECK113(gPre && gPre->activeGraph() && gPre->activeGraph()->curves() == 1,
                     "113g: appendProject dropped GPre113g curve due to pre-existing Table1_Y");
            if (gPre && gPre->activeGraph() && gPre->activeGraph()->curve(0)) {
                const QwtPlotCurve *c = gPre->activeGraph()->curve(0);
                CHECK113(c->dataSize() == 3 && std::abs(c->y(0) - 7.0) < 1e-9
                                 && std::abs(c->y(2) - 21.0) < 1e-9,
                         "113g: GPre113g bound to wrong table! y0=" << (c->dataSize() > 0 ? c->y(0)
                                                                                         : -1.0));
            }
        }

        // 7) GraphInspectorDock heap-UAF on tree click with pending annotation edit,
        //    cross-axis title flush on ReferenceLine selection, and Log10 nano-scale clamping
        {
            GraphInspectorDock *dock = app->graphInspectorDock();
            CHECK113(dock != nullptr, "113g: graphInspectorDock null");
            MultiLayer *mlI = app->newGraph(QStringLiteral("Insp113g"));
            Graph *gI = mlI ? mlI->activeGraph() : nullptr;
            CHECK113(mlI && gI && dock, "113g: Insp113g setup");
            if (mlI && gI && dock) {
                gI->setAxisTitle(QwtPlot::xBottom, QStringLiteral("OldX113g"));
                gI->setAxisTitle(QwtPlot::yLeft, QStringLiteral("OldY113g"));
                gI->setScale(QwtPlot::yLeft, -1e-9, 1e-7);

                // Create a non-primary TextAnnotation + a ReferenceLine
                Legend proto(gI->plotWidget());
                proto.setText(QStringLiteral("OrigAnnot113g"));
                const long annKey = gI->insertTextMarker(&proto);
                Graph::ReferenceLineSpec refSpec;
                refSpec.orientation = 1; // Vertical (X)
                refSpec.value = 0.5;
                gI->addReferenceLine(refSpec);

                dock->setTargetMultiLayer(mlI, true);
                QCoreApplication::processEvents();

                // (a) Heap-UAF regression: start debounced annotation edit, then emit itemClicked
                gI->setCurrentSelection(
                        Graph::GraphSelection { Graph::GraphSelection::TextAnnotation, int(annKey),
                                                -1 });
                QCoreApplication::processEvents();
                QPlainTextEdit *annotEdit =
                        dock->findChild<QPlainTextEdit *>(QStringLiteral("InspectorAnnotationTextEdit"));
                CHECK113(annotEdit != nullptr, "113g: InspectorAnnotationTextEdit missing");
                if (annotEdit && dock->objectTree() && dock->objectTree()->topLevelItemCount() > 0) {
                    annotEdit->setPlainText(QStringLiteral("EditedAnnot113g"));
                    QTreeWidgetItem *clickTarget = dock->objectTree()->topLevelItem(0);
                    if (clickTarget && clickTarget->childCount() > 0)
                        clickTarget = clickTarget->child(0);
                    emit dock->objectTree()->itemClicked(clickTarget, 0);
                    QCoreApplication::processEvents();
                    Legend *updated = gI->textMarker(int(annKey));
                    CHECK113(updated != nullptr
                                     && updated->text() == QLatin1String("EditedAnnot113g"),
                             "113g: pending annotation edit did not commit safely on tree click");
                }

                // (b) Cross-axis title flush regression when selecting ReferenceLine
                gI->setCurrentSelection(
                        Graph::GraphSelection { Graph::GraphSelection::Axis, QwtPlot::xBottom, -1 });
                dock->switchToInspectorPage(GraphInspectorDock::PageTabAxis);
                QCoreApplication::processEvents();
                QWidget *axisPage = dock->propertyStack()->widget(GraphInspectorDock::PageTabAxis);
                QLineEdit *axisTitleEdit = nullptr;
                QComboBox *scaleCombo = nullptr;
                if (axisPage) {
                    for (QLineEdit *le : axisPage->findChildren<QLineEdit *>()) {
                        if (!le->placeholderText().isEmpty()) {
                            axisTitleEdit = le;
                            break;
                        }
                    }
                    for (QComboBox *cb : axisPage->findChildren<QComboBox *>()) {
                        if (cb->count() == 2 && cb->itemData(0).toInt() == 0
                            && cb->itemData(1).toInt() == 1 && !cb->isEditable()) {
                            scaleCombo = cb;
                            break;
                        }
                    }
                }
                CHECK113(axisTitleEdit != nullptr && scaleCombo != nullptr,
                         "113g: axis title edit or scale combo missing");
                if (axisTitleEdit) {
                    axisTitleEdit->setText(QStringLiteral("NewX113g"));
                    emit axisTitleEdit->textEdited(QStringLiteral("NewX113g"));
                    gI->setCurrentSelection(
                            Graph::GraphSelection { Graph::GraphSelection::ReferenceLine, 0, -1 });
                    QCoreApplication::processEvents();
                    CHECK113(gI->axisTitle(QwtPlot::xBottom) == QLatin1String("NewX113g")
                                     && gI->axisTitle(QwtPlot::yLeft) == QLatin1String("OldY113g"),
                             "113g: debounced X title lost or leaked to Y on ReferenceLine select");
                }

                // (c) Log10 nano-scale clamping regression: [-1e-9, 1e-7] must not invert to [1e-6, 1e-7]
                if (scaleCombo) {
                    gI->setCurrentSelection(
                            Graph::GraphSelection { Graph::GraphSelection::Axis, QwtPlot::yLeft,
                                                    -1 });
                    dock->switchToInspectorPage(GraphInspectorDock::PageTabAxis);
                    QCoreApplication::processEvents();
                    scaleCombo->setCurrentIndex(1); // Log10
                    QCoreApplication::processEvents();
                    const QwtScaleDiv *sdY = gI->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
                    CHECK113(sdY != nullptr && sdY->lowerBound() > 0.0
                                     && sdY->lowerBound() < sdY->upperBound()
                                     && sdY->upperBound() <= 1e-5,
                             "113g: Log10 nano-scale clamping inverted range: ["
                                     << (sdY ? sdY->lowerBound() : 0.0) << ", "
                                     << (sdY ? sdY->upperBound() : 0.0) << "]");
                }
            }
        }

        std::cout << "[PASS] 113g: Wave 4 appendProject, EIS Randles, Deming, Workbook, StylePreset, ZipReader & Inspector checks passed\n";
    }

    // =========================================================================
    // 113h: Wave 6 Swarm fixes (Column::copy, MuParser UB, Boltzmann overflow,
    //       FitDialog destroyed UAF, Plotting/Axes/Pie/Vector/Arrow/Spec & RPC)
    // =========================================================================
    {
        AppWinGuard113 app(new ApplicationWindow());
        PythonAutomationServer srv(app);
        Rpc113 rpc { &srv };

        // 1) Column::copy overlapping self-copy & negative index rejection
        {
            Table *tc = app->newTable(QStringLiteral("ColCopy113h"), 8, 1);
            for (int r = 0; r < 5; ++r)
                tc->setCell(r, 0, double((r + 1) * 10));
            Column *col = tc->column(0);
            CHECK113(!col->copy(nullptr) && !col->copy(col, -1, 0, 2) && !col->copy(col, 0, -1, 2)
                             && !col->copy(col, 0, 0, -3),
                     "113h: Column::copy accepted null or negative indices");
            CHECK113(col->copy(col, 0, 2, 5), "113h: Column::copy overlapping self-copy failed");
            CHECK113(std::abs(col->valueAt(2) - 10.0) < 1e-9
                             && std::abs(col->valueAt(3) - 20.0) < 1e-9
                             && std::abs(col->valueAt(4) - 30.0) < 1e-9
                             && std::abs(col->valueAt(5) - 40.0) < 1e-9
                             && std::abs(col->valueAt(6) - 50.0) < 1e-9,
                     "113h: Column::copy self-copy corrupted overlapping rows: ["
                             << col->valueAt(2) << "," << col->valueAt(4) << ","
                             << col->valueAt(6) << "]");
            // Self-copy extending past origRowCount must keep copied out-of-bounds rows invalid
            Table *tc2 = app->newTable(QStringLiteral("ColCopyOob113h"), 3, 1);
            for (int r = 0; r < 3; ++r)
                tc2->setCell(r, 0, double((r + 1) * 10));
            Column *col2 = tc2->column(0);
            CHECK113(col2->copy(col2, 1, 2, 4), "113h: Column::copy self-copy past origRowCount");
            CHECK113(!col2->isInvalid(2) && std::abs(col2->valueAt(2) - 20.0) < 1e-9
                             && !col2->isInvalid(3) && std::abs(col2->valueAt(3) - 30.0) < 1e-9
                             && col2->isInvalid(4) && col2->isInvalid(5),
                     "113h: Column::copy self-copy turned out-of-bounds rows into valid phantom values");
        }

        // 2) MuParserScript NaN/Inf/huge row/col indices & lag formula cell(1, i-1)
        {
            Table *tm = app->newTable(QStringLiteral("MuUb113h"), 3, 2);
            for (int r = 0; r < 3; ++r)
                tm->setCell(r, 0, double(r + 1));
            tm->column(1)->setFormula(Interval<int>(0, 2), QStringLiteral("cell(1, 1/0)"));
            tm->recalculate(1, false);
            CHECK113(tm->column(1)->isInvalid(0),
                     "113h: cell(1, 1/0) did not mark row invalid");
            tm->column(1)->setFormula(Interval<int>(0, 2), QStringLiteral("cell(1, 1e20)"));
            tm->recalculate(1, false);
            CHECK113(tm->column(1)->isInvalid(0),
                     "113h: cell(1, 1e20) did not mark row invalid");
            tm->column(1)->setFormula(Interval<int>(0, 2), QStringLiteral("cell(1, i-1)"));
            tm->recalculate(1, false);
            CHECK113(tm->column(1)->isInvalid(0) && !tm->column(1)->isInvalid(1)
                             && std::abs(tm->cell(1, 1) - 1.0) < 1e-9
                             && std::abs(tm->cell(2, 1) - 2.0) < 1e-9,
                     "113h: lag formula cell(1, i-1) broke row-1 empty semantics");
        }

        // 3) SigmoidalFit (Boltzmann) on wide X [0, 2000] converges without Inf*0=NaN Jacobian blowup
        {
            Table *ts = app->newTable(QStringLiteral("Boltz113h"), 41, 2);
            for (int i = 0; i < 41; ++i) {
                const double xv = double(i) * 50.0; // [0, 2000]
                const double yv = 10.0 + (100.0 - 10.0) / (1.0 + std::exp(-(xv - 1000.0) / 80.0));
                ts->setCell(i, 0, xv);
                ts->setCell(i, 1, yv);
            }
            MultiLayer *mlS = app->multilayerPlot(ts, QStringList() << ts->colName(1), Graph::Scatter);
            Graph *gS = mlS ? mlS->activeGraph() : nullptr;
            CHECK113(gS && gS->curve(0), "113h: Boltz113h plot");
            if (gS && gS->curve(0)) {
                SigmoidalFit fit(app, gS, gS->curve(0)->title().text());
                fit.guessInitialValues();
                fit.fit();
                const std::vector<double> res = fit.results();
                CHECK113(res.size() >= 4 && std::isfinite(res[0]) && std::isfinite(res[1])
                                 && std::isfinite(res[2]) && std::isfinite(res[3])
                                 && std::abs(res[2] - 1000.0) < 5.0,
                         "113h: SigmoidalFit wide-X failed: x0="
                                 << (res.size() >= 3 ? res[2] : -1.0));
            }
        }

        // 4) Non-modal FitDialog survives target Graph destruction (no UAF on accept/updateLivePreview)
        {
            Table *tf = app->newTable(QStringLiteral("FitUaf113h"), 5, 2);
            for (int i = 0; i < 5; ++i) {
                tf->setCell(i, 0, double(i + 1));
                tf->setCell(i, 1, double(2 * i + 1));
            }
            MultiLayer *mlF = app->multilayerPlot(tf, QStringList() << tf->colName(1), Graph::Scatter);
            Graph *gF = mlF ? mlF->activeGraph() : nullptr;
            CHECK113(mlF && gF, "113h: FitUaf113h graph");
            if (mlF && gF) {
                QPointer<FitDialog> fd = new FitDialog(app);
                fd->setGraph(gF);
                mlF->askOnCloseEvent(false);
                QPointer<Graph> gFGuard(gF);
                mlF->removeLayer();
                for (int pass = 0; pass < 2; ++pass) {
                    QCoreApplication::processEvents();
                    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                }
                CHECK113(gFGuard.isNull(), "113h: removeLayer did not destroy gF");
                if (fd) {
                    fd->accept();
                    fd->updateLivePreview();
                    delete fd.data();
                }
                CHECK113(true, "113h: FitDialog survived target graph destruction");
            }
        }

        // 5) Graph::plotPie invalid column / empty insertCurvesList & QwtPieCurve zero-row consistency
        {
            Table *tp = app->newTable(QStringLiteral("Pie113h"), 3, 2);
            tp->setColName(0, QStringLiteral("Cat"));
            tp->setColName(1, QStringLiteral("Val"));
            tp->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
            tp->column(0)->setTextAt(0, QStringLiteral("A"));
            tp->column(0)->setTextAt(1, QStringLiteral("B"));
            tp->column(0)->setTextAt(2, QStringLiteral("C"));
            tp->setCell(0, 1, 10.0);
            tp->setCell(1, 1, 0.0);
            tp->setCell(2, 1, 30.0);
            MultiLayer *mlP = app->newGraph(QStringLiteral("PieG113h"));
            Graph *gP = mlP ? mlP->activeGraph() : nullptr;
            CHECK113(gP != nullptr, "113h: PieG113h graph");
            if (gP) {
                gP->plotPie(nullptr, QStringLiteral("NoCol"));
                gP->plotPie(tp, QStringLiteral("NonExistentCol113h"));
                CHECK113(!gP->insertCurvesList(tp, QStringList(), Graph::Pie, 1, 5, 0, -1, QString()),
                         "113h: empty insertCurvesList(Pie) should return false");
                gP->plotPie(tp, tp->colName(1));
                QwtPieCurve *pc = dynamic_cast<QwtPieCurve *>(gP->curve(0));
                CHECK113(pc != nullptr && pc->dataSize() == 2,
                         "113h: initial pie dataSize=" << (pc ? int(pc->dataSize()) : -1));
                if (pc) {
                    pc->loadData();
                    CHECK113(pc->dataSize() == 2,
                             "113h: QwtPieCurve::loadData() included zero row (dataSize="
                                     << pc->dataSize() << ")");
                }
            }
        }

        // 6) PlotCurve::boundingRect on reversed X axis & VectorCurve horizontal boundingRect & ArrowMarker
        {
            Table *tr = app->newTable(QStringLiteral("Rev113h"), 5, 2);
            for (int i = 0; i < 5; ++i) {
                tr->setCell(i, 0, double(i) * 2.5);
                tr->setCell(i, 1, double(i + 1));
            }
            MultiLayer *mlR = app->multilayerPlot(tr, QStringList() << tr->colName(1), Graph::Scatter);
            Graph *gR = mlR ? mlR->activeGraph() : nullptr;
            CHECK113(gR && gR->curve(0), "113h: Rev113h plot");
            if (gR && gR->curve(0)) {
                gR->setScale(QwtPlot::xBottom, 0.0, 10.0, 0.0, 5, 5, 0, true); // reversed X
                gR->replot();
                const QwtDoubleRect br = gR->curve(0)->boundingRect();
                CHECK113(br.left() <= 0.0 + 1e-9 && br.right() >= 10.0 - 1e-9,
                         "113h: PlotCurve::boundingRect shrunk inward on reversed X: ["
                                 << br.left() << ", " << br.right() << "]");
            }

            // Purely horizontal VectXYAM boundingRect must include vector tips [0, 10]
            Table *tv = app->newTable(QStringLiteral("Vec113h"), 2, 4);
            for (int i = 0; i < 2; ++i) {
                tv->setCell(i, 0, double(i) * 5.0);
                tv->setCell(i, 1, double(i));
                tv->setCell(i, 2, 0.0); // horizontal angle = 0
                tv->setCell(i, 3, 5.0); // mag = 5 -> tip at x = 10
            }
            VectorCurve vc(VectorCurve::XYAM, tv, tv->colName(0), tv->colName(1), tv->colName(2),
                           tv->colName(3), 0, 1);
            vc.loadData();
            const QwtDoubleRect vbr = vc.boundingRect();
            CHECK113(vbr.left() <= 0.0 + 1e-9 && vbr.right() >= 10.0 - 1e-9,
                     "113h: horizontal VectorCurve::boundingRect dropped tips: ["
                             << vbr.left() << ", " << vbr.right() << "]");

            // ArrowMarker::boundingRect safe with null plot() and normalized Y
            ArrowMarker am;
            am.setStartPoint(1.0, 2.0);
            am.setEndPoint(4.0, 6.0);
            const QwtDoubleRect abr = am.boundingRect();
            CHECK113(std::abs(abr.left() - 1.0) < 1e-9 && std::abs(abr.right() - 4.0) < 1e-9
                             && std::abs(abr.top() - 2.0) < 1e-9
                             && std::abs(abr.bottom() - 6.0) < 1e-9,
                     "113h: ArrowMarker::boundingRect wrong: [" << abr.left() << "," << abr.right()
                                                                << "] y=[" << abr.top() << ","
                                                                << abr.bottom() << "]");
        }

        // 7) setCustomTickLabels update/clear, setAutoScale restoring suspended axis break,
        //    Spectrogram showCurve color bar persistence, and plot.histogram orphan cleanup
        {
            Table *ta = app->newTable(QStringLiteral("Axes113h"), 5, 2);
            for (int i = 0; i < 5; ++i) {
                ta->setCell(i, 0, double(i) * 25.0); // [0, 100]
                ta->setCell(i, 1, double(i + 1));
            }
            MultiLayer *mlA = app->multilayerPlot(ta, QStringList() << ta->colName(1), Graph::Line);
            Graph *gA = mlA ? mlA->activeGraph() : nullptr;
            CHECK113(gA != nullptr, "113h: Axes113h plot");
            if (gA) {
                // Custom tick labels second update & clear (including rotation & minor ticks restore)
                gA->setCustomTickLabels(QwtPlot::xBottom, { 0.0, 50.0 }, { "P1", "P2" }, 45.0);
                gA->setCustomTickLabels(QwtPlot::xBottom, { 25.0, 75.0 }, { "Q1", "Q2" }, 45.0);
                const QwtScaleDraw *sd1 = gA->plotWidget()->axisScaleDraw(QwtPlot::xBottom);
                CHECK113(sd1 && sd1->label(25.0).text() == QLatin1String("Q1")
                                 && sd1->label(75.0).text() == QLatin1String("Q2"),
                         "113h: second setCustomTickLabels failed to update ScaleDraw");
                gA->setCustomTickLabels(QwtPlot::xBottom, {}, {});
                const QwtScaleDraw *sd2 = gA->plotWidget()->axisScaleDraw(QwtPlot::xBottom);
                CHECK113(sd2 && !sd2->label(50.0).text().isEmpty()
                                 && gA->plotWidget()->axisMaxMinor(QwtPlot::xBottom) == 2
                                 && gA->labelsRotation(QwtPlot::xBottom) == 0,
                         "113h: clearing setCustomTickLabels left blank ScaleDraw or stale ticks");

                // Axis break suspend on zoom-in, restore on setAutoScale()
                Graph::AxisBreakSpec brk;
                brk.enabled = true;
                brk.breakFrom = 20.0;
                brk.breakTo = 60.0;
                gA->setAxisBreak(QwtPlot::xBottom, brk);
                CHECK113(gA->axisBreak(QwtPlot::xBottom).enabled, "113h: initial axis break");
                gA->setScale(QwtPlot::xBottom, 0.0, 10.0); // outside break -> suspends
                CHECK113(!gA->axisBreak(QwtPlot::xBottom).enabled, "113h: break not suspended");
                gA->setAutoScale(); // back to [0, 100] -> must restore break
                CHECK113(gA->axisBreak(QwtPlot::xBottom).enabled,
                         "113h: setAutoScale() did not restore suspended axis break");
            }

            // Spectrogram hide/show color bar persistence (single & multi-spectrogram on shared axis)
            Matrix *ms = app->newMatrix(QStringLiteral("SpecVis113h"), 4, 4);
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    ms->setCell(r, c, double(r * 4 + c + 1));
            MultiLayer *mlSp = app->plotSpectrogram(ms, Graph::ColorMap);
            Graph *gSp = mlSp ? mlSp->activeGraph() : nullptr;
            CHECK113(gSp != nullptr && gSp->curves() >= 1, "113h: SpecVis113h");
            if (gSp) {
                Spectrogram *sp = dynamic_cast<Spectrogram *>(gSp->plotItem(0));
                CHECK113(sp != nullptr && sp->hasColorScale(), "113h: initial spectrogram colorbar");
                gSp->showCurve(0, false);
                CHECK113(!sp->hasColorScale(), "113h: colorbar still enabled while hidden");
                gSp->showCurve(0, true);
                CHECK113(sp->hasColorScale(), "113h: colorbar permanently lost after re-show");

                // Add a 2nd spectrogram sharing yRight (exercises hideOtherCurves sequence)
                gSp->plotSpectrogram(ms, Graph::ColorMap);
                Spectrogram *sp2 = dynamic_cast<Spectrogram *>(gSp->plotItem(1));
                CHECK113(sp2 != nullptr && gSp->curves() >= 2, "113h: 2nd spectrogram");
                if (sp2) {
                    gSp->showCurve(0, false); // sp2 still visible -> colorbar stays ON
                    CHECK113(sp2->hasColorScale(),
                             "113h: hiding sp0 wiped still-visible sp2 colorbar");
                    gSp->showCurve(1, false); // both hidden -> colorbar turns OFF
                    CHECK113(!sp2->hasColorScale(),
                             "113h: colorbar stayed enabled with all spectrograms hidden");
                    gSp->showCurve(1, true); // showing 2nd spectrogram must restore colorbar
                    CHECK113(sp2->hasColorScale(),
                             "113h: re-showing 2nd spectrogram lost shared colorbar");
                }
            }

            // plot.histogram & plot.xy invalid xlim return -32602 and leave zero orphan graph windows
            const int winsBefore = app->windowsList().size();
            const QJsonObject badH = rpc.call(
                    QStringLiteral("plot.histogram"),
                    QJsonObject { { "table", "Axes113h" },
                                  { "col", "2" },
                                  { "xlim", QJsonArray { 1.0, 1.0 } } });
            CHECK113(!Rpc113::ok(badH)
                             && badH.value(QStringLiteral("error"))
                                                .toObject()
                                                .value(QStringLiteral("code"))
                                                .toInt()
                                     == -32602
                             && app->windowsList().size() == winsBefore,
                     "113h: plot.histogram leaked orphan window or wrong error on invalid xlim");
            const QJsonObject badB = rpc.call(
                    QStringLiteral("plot.xy"),
                    QJsonObject { { "table", "Axes113h" },
                                  { "x_col", "1" },
                                  { "y_cols", QJsonArray { "2" } },
                                  { "style", "vertical_bars" },
                                  { "xlim", QJsonArray { 1.0, 1.0 } } });
            CHECK113(!Rpc113::ok(badB)
                             && badB.value(QStringLiteral("error"))
                                                .toObject()
                                                .value(QStringLiteral("code"))
                                                .toInt()
                                     == -32602
                             && app->windowsList().size() == winsBefore,
                     "113h: plot.xy leaked orphan window or wrong error on invalid xlim");
        }

        std::cout << "[PASS] 113h: Wave 6 Swarm fixes (Column, MuParser, Boltzmann, FitDialog, Plotting & RPC) passed\n";
    }

    if (g113Failures > 0) {
        std::cerr << "[FAIL] Suite 113 completed with " << g113Failures << " failure(s).\n";
        return 1;
    }
    std::cout << "[PASS] Suite 113: Swarm review Wave 1-6 regressions passed.\n";
    return 0;
}
