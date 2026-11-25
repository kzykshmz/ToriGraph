#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "Graph3D.h"
#include "ScaleDraw.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

// =============================================================================
// Suite 102: graph creation, export and save -> reopen, round 2 of the data-driven tests.
//   102a plot.xy with a date/time X column keeps the date/time axis (no empty category axis)
//        and its range covers the data, also after reopen; month and weekday X columns plot
//        as month / weekday numbers on a month-name / weekday-name axis; a date/time Y column
//        can be plotted
//   102b the major ticks chosen by the lab_slide_83mm and slide_28pt presets stay the same
//        after clone_window, graph.export_ole and save -> reopen (isij_85mm as well), and
//        the user step stays unset
//   102c arrange_layers(1, 3) / (1, 4) keeps every panel on the page with a usable plot
//        area (the page is widened, with a warning); 1 x 2 and 2 x 2 keep the page size
//   102d plot.xy with many Y columns (automatic legend placement) stays fast
//   102e a colour-mapped scatter exports the same tick labels before saving and after
//        reopening, and while a zoomed page is rendered at 100 % its autoscaled axes keep the
//        ranges shown on screen
//   102f plot.3d_xyz accepts one column for X and Z
//   102g a function curve keeps its Function type across save -> reopen
//   102h appending a project whose table is renamed keeps the custom curve label
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g102Failures = 0;

#define CHECK102(cond, msg)                                                                     \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g102Failures;                                                                     \
        }                                                                                       \
    } while (0)

struct NonInteractiveGuard102
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard102()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard102()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard102
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard102(ApplicationWindow *w = nullptr, bool headless = false) : win(w)
    {
        if (win) {
            if (headless)
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
    ~AppWinGuard102() { reset(); }
    AppWinGuard102(const AppWinGuard102 &) = delete;
    AppWinGuard102 &operator=(const AppWinGuard102 &) = delete;
    void reset()
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
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

// JSON-RPC 2.0 calls on one application window (as Suite 82 / 97 do).
struct Rpc102
{
    PythonAutomationServer *srv = nullptr;
    int id = 10200;
    QJsonObject call(const QString &method, const QJsonObject &params)
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = ++id;
        req[QStringLiteral("method")] = method;
        req[QStringLiteral("params")] = params;
        const QJsonObject resp = srv->handleRpcRequest(req);
        QCoreApplication::processEvents();
        if (resp.contains(QStringLiteral("error")))
            std::cerr << "[INFO] 102: " << method.toStdString() << " error: "
                      << QJsonDocument(resp.value(QStringLiteral("error")).toObject())
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString()
                      << "\n";
        return resp;
    }
    static bool ok(const QJsonObject &resp) { return !resp.contains(QStringLiteral("error")); }
    static QJsonObject result(const QJsonObject &resp)
    {
        return resp.value(QStringLiteral("result")).toObject();
    }
    bool makeTable(const QString &name, const QStringList &cols, const QList<QJsonArray> &data)
    {
        const int rows = data.isEmpty() ? 0 : data.first().size();
        QJsonArray colNames;
        QJsonObject dict;
        for (int i = 0; i < cols.size(); ++i) {
            colNames.append(cols[i]);
            dict.insert(cols[i], data.value(i));
        }
        if (!ok(call(QStringLiteral("table.create"),
                     QJsonObject{ { "rows", rows },
                                  { "cols", cols.size() },
                                  { "name", name },
                                  { "col_names", colNames } })))
            return false;
        return ok(call(QStringLiteral("table.set_data_dict"),
                       QJsonObject{ { "table", name },
                                    { "data", dict },
                                    { "column_order", colNames } }));
    }
};

MyWidget *findWindow102(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            return w;
    return nullptr;
}

Graph *firstLayer102(ApplicationWindow *app, const QString &name)
{
    auto *ml = qobject_cast<MultiLayer *>(findWindow102(app, name));
    if (!ml || ml->layersList().isEmpty())
        return nullptr;
    return ml->layersList().first();
}

QList<double> majors102(Graph *g, int axis)
{
    QList<double> out;
    if (!g || !g->plotWidget())
        return out;
    if (const QwtScaleDiv *sd = g->plotWidget()->axisScaleDiv(axis))
        for (double v : sd->ticks(QwtScaleDiv::MajorTick))
            out << v;
    return out;
}

std::string str102(const QList<double> &v)
{
    QStringList t;
    for (double x : v)
        t << QString::number(x, 'g', 10);
    return ("{" + t.join(',') + "}").toStdString();
}

bool sameList102(const QList<double> &a, const QList<double> &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i)
        if (std::fabs(a[i] - b[i]) > 1e-9 * std::max(1.0, std::fabs(a[i])))
            return false;
    return true;
}

bool axisRange102(Graph *g, int axis, double *lo, double *hi)
{
    const QwtScaleDiv *sd = (g && g->plotWidget()) ? g->plotWidget()->axisScaleDiv(axis) : nullptr;
    if (!sd)
        return false;
    *lo = qMin(sd->lowerBound(), sd->upperBound());
    *hi = qMax(sd->lowerBound(), sd->upperBound());
    return true;
}

// Curve X (or Y) data range.
bool curveRange102(Graph *g, int idx, bool xData, double *lo, double *hi)
{
    QwtPlotCurve *c = (g && idx < g->curves()) ? g->curve(idx) : nullptr;
    if (!c || c->dataSize() <= 0)
        return false;
    *lo = 1e300;
    *hi = -1e300;
    for (int i = 0; i < c->dataSize(); ++i) {
        const double v = xData ? c->x(i) : c->y(i);
        *lo = qMin(*lo, v);
        *hi = qMax(*hi, v);
    }
    return true;
}

QString tickLabel102(Graph *g, int axis, double v)
{
    const QwtScaleDraw *sd = (g && g->plotWidget()) ? g->plotWidget()->axisScaleDraw(axis) : nullptr;
    return sd ? sd->label(v).text() : QString();
}

// Numbers drawn as text in an SVG (tick labels, colour-bar labels).
QStringList svgNumbers102(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return { QStringLiteral("<unreadable>") };
    const QString txt = QString::fromUtf8(f.readAll());
    QStringList out;
    QRegularExpression re(QStringLiteral(">\\s*([-\\x{2212}]?\\d+(?:\\.\\d+)?)\\s*</text>"));
    auto it = re.globalMatch(txt);
    while (it.hasNext())
        out << it.next().captured(1);
    return out;
}

struct PresetTicks102
{
    QString graph;
    QList<double> x, y;
    double xStep = 0.0, yStep = 0.0; // user steps (the preset sets none)
};

} // namespace

int runSuite102PlotRound2(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 102: graph creation/export/round-trip round 2 (date/time "
                 "axes, preset ticks, 1xN panels, many curves, colour scatter export, 3D XYZ, "
                 "function type, appended labels)...\n";
    g102Failures = 0;
    NonInteractiveGuard102 niGuard;

    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "102: temp dir creation failed");
    const QString prj = tmp.filePath(QStringLiteral("round2_102.sciprj"));
    const QString prjLabel = tmp.filePath(QStringLiteral("label_102.sciprj"));

    // Values recorded in the source session and compared after reopening.
    QList<PresetTicks102> presetTicks;
    QStringList cscatSvgBefore;
    QString cscatSvgAfterPath = tmp.filePath(QStringLiteral("cscat_after.svg"));

    {
        AppWinGuard102 w(new ApplicationWindow(), true);
        Rpc102 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "102: pythonAutomationServer null");

        // ---- 102a: date/time, month and weekday columns ---------------------------------------
        {
            CHECK_COND(rpc.makeTable(QStringLiteral("T102D"), { "T", "Y" },
                                     { QJsonArray{ 0, 0, 0, 0 }, QJsonArray{ 1.0, 2.0, 3.0, 5.0 } }),
                       "102a: creating T102D failed");
            CHECK102(Rpc102::ok(rpc.call(
                             QStringLiteral("table.set_column_data"),
                             QJsonObject{ { "table", "T102D" },
                                          { "col", "T" },
                                          { "mode", "datetime" },
                                          { "data",
                                            QJsonArray{ "2026-01-01 06:00:00", "2026-01-02 06:00:00",
                                                        "2026-01-03 06:00:00",
                                                        "2026-01-05 12:00:00" } } })),
                     "102a: datetime column data");
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.xy"),
                                         QJsonObject{ { "table", "T102D" },
                                                      { "x_col", "T" },
                                                      { "y_cols", QJsonArray{ "Y" } },
                                                      { "graph_name", "G102DT" } })),
                     "102a: plot.xy with a datetime X column failed");
            Graph *g = firstLayer102(w, QStringLiteral("G102DT"));
            CHECK102(g && g->curves() == 1, "102a: datetime graph has no curve");
            if (g && g->curves() == 1) {
                CHECK102(g->axesType()[QwtPlot::xBottom] == Graph::AxisType::DateTime,
                         "102a: X axis type " << int(g->axesType()[QwtPlot::xBottom])
                                              << " (expected DateTime)");
                double lo = 0, hi = 0, cLo = 0, cHi = 0;
                axisRange102(g, QwtPlot::xBottom, &lo, &hi);
                curveRange102(g, 0, true, &cLo, &cHi);
                CHECK102(cLo > 2.4e6 && lo <= cLo && hi >= cHi,
                         "102a: X axis " << lo << ".." << hi << " does not cover the data " << cLo
                                         << ".." << cHi);
                const QList<double> mj = majors102(g, QwtPlot::xBottom);
                CHECK102(!mj.isEmpty() && !tickLabel102(g, QwtPlot::xBottom, mj.first()).isEmpty(),
                         "102a: datetime tick labels are blank");
            }
            // a datetime column as Y
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.xy"),
                                         QJsonObject{ { "table", "T102D" },
                                                      { "x_col", "Y" },
                                                      { "y_cols", QJsonArray{ "T" } },
                                                      { "graph_name", "G102DTY" } })),
                     "102a: plot.xy with a datetime Y column was refused");
            Graph *gy = firstLayer102(w, QStringLiteral("G102DTY"));
            CHECK102(gy && gy->curves() == 1
                             && gy->axesType()[QwtPlot::yLeft] == Graph::AxisType::DateTime,
                     "102a: datetime Y graph lacks a date/time Y axis");

            // month and weekday columns (type conversion rule: January / Monday -> 1)
            CHECK_COND(rpc.makeTable(QStringLiteral("T102M"), { "M", "Y" },
                                     { QJsonArray{ 1, 2, 3, 4 }, QJsonArray{ 5.0, 6.0, 7.0, 8.0 } }),
                       "102a: creating T102M failed");
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("table.set_column_data"),
                                         QJsonObject{ { "table", "T102M" },
                                                      { "col", "M" },
                                                      { "mode", "month" },
                                                      { "data", QJsonArray{ 1, 2, 3, 4 } } })),
                     "102a: month column data");
            CHECK_COND(rpc.makeTable(QStringLiteral("T102W"), { "W", "Y" },
                                     { QJsonArray{ 1, 2, 3, 4 }, QJsonArray{ 5.0, 6.0, 7.0, 8.0 } }),
                       "102a: creating T102W failed");
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("table.set_column_data"),
                                         QJsonObject{ { "table", "T102W" },
                                                      { "col", "W" },
                                                      { "mode", "day" },
                                                      { "data", QJsonArray{ 1, 2, 3, 4 } } })),
                     "102a: weekday column data");
            const struct
            {
                const char *table, *xcol, *graph;
                Graph::AxisType type;
            } kinds[] = { { "T102M", "M", "G102MO", Graph::AxisType::Month },
                          { "T102W", "W", "G102WD", Graph::AxisType::Day } };
            for (const auto &k : kinds) {
                CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.xy"),
                                             QJsonObject{ { "table", k.table },
                                                          { "x_col", k.xcol },
                                                          { "y_cols", QJsonArray{ "Y" } },
                                                          { "graph_name", k.graph } })),
                         "102a: plot.xy " << k.graph);
                Graph *gm = firstLayer102(w, QString::fromLatin1(k.graph));
                CHECK102(gm && gm->curves() == 1, "102a: " << k.graph << " has no curve");
                if (!gm || gm->curves() != 1)
                    continue;
                CHECK102(gm->axesType()[QwtPlot::xBottom] == k.type,
                         "102a: " << k.graph << " X axis type "
                                  << int(gm->axesType()[QwtPlot::xBottom]));
                QwtPlotCurve *c = gm->curve(0);
                bool seq = c->dataSize() == 4;
                for (int i = 0; seq && i < 4; ++i)
                    seq = std::fabs(c->x(i) - double(i + 1)) < 1e-12;
                CHECK102(seq, "102a: " << k.graph << " X values are not 1..4 (first "
                                       << (c->dataSize() > 0 ? c->x(0) : -1.0) << ")");
                const QString want = (k.type == Graph::AxisType::Month)
                        ? QLocale().monthName(1, QLocale::ShortFormat)
                        : QLocale().dayName(1, QLocale::ShortFormat);
                CHECK102(tickLabel102(gm, QwtPlot::xBottom, 1.0) == want,
                         "102a: " << k.graph << " label of 1 is '"
                                  << tickLabel102(gm, QwtPlot::xBottom, 1.0).toStdString()
                                  << "', expected '" << want.toStdString() << "'");
            }
        }

        // ---- 102b: preset tick density -------------------------------------------------------
        {
            QJsonArray px, pa, pb;
            for (int i = 0; i < 50; ++i) {
                const double x = 0.1 + (10.0 - 0.1) * i / 49.0;
                px.append(x);
                pa.append(std::sin(x));
                pb.append(std::cos(x));
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T102P"), { "X", "A", "B" }, { px, pa, pb }),
                       "102b: creating T102P failed");
            for (const char *preset : { "isij_85mm", "lab_slide_83mm", "slide_28pt" }) {
                const QString gname = QStringLiteral("G102P_%1").arg(QString::fromLatin1(preset).left(3));
                CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.xy"),
                                             QJsonObject{ { "table", "T102P" },
                                                          { "x_col", "X" },
                                                          { "y_cols", QJsonArray{ "A", "B" } },
                                                          { "graph_name", gname } })),
                         "102b: plot.xy " << preset);
                CHECK102(Rpc102::ok(rpc.call(QStringLiteral("graph.apply_style_preset"),
                                             QJsonObject{ { "graph", gname }, { "preset", preset } })),
                         "102b: apply_style_preset " << preset);
                Graph *g = firstLayer102(w, gname);
                if (!g)
                    continue;
                PresetTicks102 pt{ gname, majors102(g, QwtPlot::xBottom),
                                   majors102(g, QwtPlot::yLeft), g->axisStep(QwtPlot::xBottom),
                                   g->axisStep(QwtPlot::yLeft) };
                presetTicks << pt;
                std::cout << "[INFO] 102b: " << preset << " x " << str102(pt.x) << " y "
                          << str102(pt.y) << "\n";

                const QJsonObject rc = rpc.call(QStringLiteral("app.clone_window"),
                                                QJsonObject{ { "name", gname },
                                                             { "new_name", gname + "C" } });
                CHECK102(Rpc102::ok(rc), "102b: clone_window " << preset);
                Graph *gc = firstLayer102(w, gname + "C");
                CHECK102(gc && sameList102(majors102(gc, QwtPlot::xBottom), pt.x)
                                 && sameList102(majors102(gc, QwtPlot::yLeft), pt.y),
                         "102b: " << preset << " clone ticks x "
                                  << str102(majors102(gc, QwtPlot::xBottom)) << " y "
                                  << str102(majors102(gc, QwtPlot::yLeft)));

                const QString emf = tmp.filePath(gname + QStringLiteral(".emf"));
                CHECK102(Rpc102::ok(rpc.call(QStringLiteral("graph.export_ole"),
                                             QJsonObject{ { "graph", gname },
                                                          { "emf_path", emf },
                                                          { "dpi", 150 },
                                                          { "include_base64", false } })),
                         "102b: export_ole " << preset);
                g = firstLayer102(w, gname);
                CHECK102(g && sameList102(majors102(g, QwtPlot::xBottom), pt.x)
                                 && sameList102(majors102(g, QwtPlot::yLeft), pt.y),
                         "102b: " << preset << " ticks changed by export_ole: x "
                                  << str102(majors102(g, QwtPlot::xBottom)) << " y "
                                  << str102(majors102(g, QwtPlot::yLeft)));
            }
        }

        // ---- 102c: 1 x N panels --------------------------------------------------------------
        {
            QJsonArray ax, a1, a2, a3, a4;
            for (int i = 0; i < 50; ++i) {
                const double x = 0.1 + (10.0 - 0.1) * i / 49.0;
                ax.append(x);
                a1.append(std::sin(x));
                a2.append(std::cos(x));
                a3.append(x * x);
                a4.append(x);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T102L"), { "X", "A", "B", "C", "D" },
                                     { ax, a1, a2, a3, a4 }),
                       "102c: creating T102L failed");
            const struct
            {
                int rows, cols;
                bool mayGrow;
            } grids[] = { { 1, 2, false }, { 2, 2, false }, { 1, 3, true }, { 1, 4, true } };
            for (const auto &gr : grids) {
                const QString gname = QStringLiteral("G102A%1x%2").arg(gr.rows).arg(gr.cols);
                CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.xy"),
                                             QJsonObject{ { "table", "T102L" },
                                                          { "x_col", "X" },
                                                          { "y_cols", QJsonArray{ "A" } },
                                                          { "graph_name", gname } })),
                         "102c: plot.xy " << gname.toStdString());
                const char *ys[] = { "B", "C", "D" };
                for (int i = 1; i < gr.rows * gr.cols; ++i) {
                    rpc.call(QStringLiteral("graph.add_layer"), QJsonObject{ { "graph", gname } });
                    rpc.call(QStringLiteral("plot.add_curve"),
                             QJsonObject{ { "graph", gname },
                                          { "table", "T102L" },
                                          { "x_col", "X" },
                                          { "y_col", ys[(i - 1) % 3] },
                                          { "layer", i } });
                }
                auto *ml = qobject_cast<MultiLayer *>(findWindow102(w, gname));
                CHECK102(ml && ml->layers() == gr.rows * gr.cols,
                         "102c: " << gname.toStdString() << " layers "
                                  << (ml ? ml->layers() : -1));
                if (!ml)
                    continue;
                const double pageW0 = ml->pageWidthPhysical();
                const double pageH0 = ml->pageHeightPhysical();
                const QJsonObject r = rpc.call(QStringLiteral("graph.arrange_layers"),
                                               QJsonObject{ { "graph", gname },
                                                            { "rows", gr.rows },
                                                            { "cols", gr.cols } });
                CHECK102(Rpc102::ok(r), "102c: arrange_layers " << gname.toStdString());
                const QJsonArray warns =
                        Rpc102::result(r).value(QStringLiteral("warnings")).toArray();
                const bool grown = ml->pageWidthPhysical() > pageW0 + 1e-6
                        || ml->pageHeightPhysical() > pageH0 + 1e-6;
                if (!gr.mayGrow) {
                    CHECK102(!grown && warns.isEmpty(),
                             "102c: " << gname.toStdString() << " page changed "
                                      << pageW0 << "x" << pageH0 << " -> "
                                      << ml->pageWidthPhysical() << "x"
                                      << ml->pageHeightPhysical());
                } else {
                    CHECK102(grown && !warns.isEmpty(),
                             "102c: " << gname.toStdString()
                                      << " page not widened / no warning (warnings "
                                      << warns.size() << ")");
                }
                const QRect page = ml->canvasWidget() ? ml->canvasWidget()->rect() : QRect();
                int minCanvas = 1 << 30;
                bool inside = true;
                for (Graph *lg : ml->layersList()) {
                    Plot *p = lg->plotWidget();
                    p->updateLayout();
                    const QRect cr = p->plotLayout()->canvasRect();
                    minCanvas = qMin(minCanvas, qMin(cr.width(), cr.height()));
                    if (!page.contains(lg->geometry()))
                        inside = false;
                }
                std::cout << "[INFO] 102c: " << gname.toStdString() << " page "
                          << ml->pageWidthPhysical() << "x" << ml->pageHeightPhysical()
                          << ", smallest plot area " << minCanvas << " px\n";
                CHECK102(inside, "102c: " << gname.toStdString() << " a panel lies off the page");
                CHECK102(minCanvas >= 60, "102c: " << gname.toStdString()
                                                   << " plot area squashed to " << minCanvas
                                                   << " px");
            }
        }

        // ---- 102d: many Y columns ------------------------------------------------------------
        {
            const int nCurves = 60;
            QStringList cols{ QStringLiteral("X") };
            QList<QJsonArray> data;
            QJsonArray xs;
            for (int r = 0; r < 200; ++r)
                xs.append(10.0 * r / 199.0);
            data << xs;
            QJsonArray yNames;
            for (int k = 0; k < nCurves; ++k) {
                QJsonArray ys;
                for (int r = 0; r < 200; ++r)
                    ys.append(std::sin(10.0 * r / 199.0 + 0.1 * k) + 0.05 * k);
                cols << QStringLiteral("Y%1").arg(k);
                yNames.append(QStringLiteral("Y%1").arg(k));
                data << ys;
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T102N"), cols, data),
                       "102d: creating T102N failed");
            QElapsedTimer t;
            t.start();
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.xy"),
                                         QJsonObject{ { "table", "T102N" },
                                                      { "x_col", "X" },
                                                      { "y_cols", yNames },
                                                      { "style", "line" },
                                                      { "graph_name", "G102N" } })),
                     "102d: plot.xy with " << nCurves << " curves failed");
            const qint64 ms = t.elapsed();
            std::cout << "[INFO] 102d: plot.xy with " << nCurves << " curves took " << ms
                      << " ms\n";
            // Before the fix this took about 100 s (and 20 curves ~10 s).
            CHECK102(ms < 20000, "102d: plot.xy with " << nCurves << " curves took " << ms
                                                       << " ms");
            Graph *g = firstLayer102(w, QStringLiteral("G102N"));
            CHECK102(g && g->curves() == nCurves && g->legend(), "102d: curves/legend missing");
        }

        // ---- 102e: colour-mapped scatter export --------------------------------------------------
        {
            // The data of the data-driven test that found the export range change
            // (numpy default_rng(1): normal(0, 1), normal(0, 2), random()).
            static const double kx[30] = { 0.345584192064786, 0.8216181435011584, 0.33043707618338714, -1.303157231604361, 0.9053558666731177, 0.4463745723640113, -0.5369532353602852, 0.5811181041963531, 0.36457239618607573, 0.294132496655526, 0.02842224131579679, 0.5467129866124469, -0.7364540870016669, -0.16290994799305278, -0.48211931267997826, 0.5988462126346276, 0.03972210748165899, -0.2924567509650886, -0.7819084623568421, -0.2571922406188707, 0.008142180518343508, -0.2756029052993704, 1.2940638143982073, 1.0067243153057943, -2.7111624789659685, -1.8890132459676727, -0.17477209205516195, -0.42219041157635356, 0.2136429974986111, 0.21732193102256359 };
            static const double ky[30] = { 4.2356775101020965, -2.2240415253845627, -0.7552100142539961, 4.085543214984661, 1.2934059924036938, 1.3261267447525233, -1.0280127433749258, -3.2961503417113054, 0.33492948844548226, 0.21802817564309507, -2.4547041084891483, -1.3664533235611245, -0.14408735945445486, -1.8895032461215548, -0.19653993570443454, 0.19096605493890867, 0.07117247411097143, -1.0125833166286295, 1.1874961435716456, 1.7823339085646568, 0.6416966091331274, -1.636460454780614, 1.4633045675708816, -1.0028800369341047, 1.7583212365759706, -2.1435748337548883, 1.8289344062575623, -0.04012690923096084, -2.497497780668831, -0.6277989439336955 };
            static const double kz[30] = { 0.007091828603166261, 0.6457208955749478, 0.719909383508693, 0.8355692165002742, 0.28187782736454214, 0.2152181671629736, 0.6393313800665879, 0.8050548331450097, 0.9636708728449709, 0.15052483042117748, 0.48221238819933654, 0.8947158621961735, 0.4227169069454373, 0.5895020620840481, 0.0244906774933632, 0.6734598871529389, 0.9190886196338225, 0.8268253295567211, 0.8855202667099468, 0.6603553805205233, 0.24555226724317758, 0.7685169988962544, 0.2116747426075105, 0.8312748346644612, 0.06271792257076825, 0.8254878133935558, 0.1645072664741013, 0.37514699649664185, 0.3167381665569643, 0.6913370352777413 };
            QJsonArray cx, cy, cz;
            for (int i = 0; i < 30; ++i) {
                cx.append(kx[i]);
                cy.append(ky[i]);
                cz.append(kz[i]);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T102S"), { "x", "y", "z" }, { cx, cy, cz }),
                       "102e: creating T102S failed");
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.color_scatter"),
                                         QJsonObject{ { "table", "T102S" },
                                                      { "x", "x" },
                                                      { "y", "y" },
                                                      { "z", "z" },
                                                      { "graph_name", "G102CS" } })),
                     "102e: plot.color_scatter failed");
            // The graph window shows the page zoomed (as in the GUI / a launched ToriGraph);
            // exporting renders it at 100 %.
            if (auto *mlz = qobject_cast<MultiLayer *>(findWindow102(w, QStringLiteral("G102CS"))))
                mlz->setPageZoom(1.4545454545454548);
            const QString svg = tmp.filePath(QStringLiteral("cscat_before.svg"));
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("graph.export"),
                                         QJsonObject{ { "graph", "G102CS" }, { "path", svg } })),
                     "102e: SVG export failed");
            cscatSvgBefore = svgNumbers102(svg);
        }

        // ---- 102f: plot.3d_xyz with one column for X and Z ------------------------------------
        {
            QJsonArray x3, y3, z3;
            for (int i = 0; i < 50; ++i) {
                x3.append(std::sin(0.3 * i));
                y3.append(std::cos(0.2 * i) * 2.0);
                z3.append(0.02 * i);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T102X"), { "x", "y", "z" }, { x3, y3, z3 }),
                       "102f: creating T102X failed");
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.3d_xyz"),
                                         QJsonObject{ { "table", "T102X" },
                                                      { "x_col", "x" },
                                                      { "y_col", "y" },
                                                      { "z_col", "x" },
                                                      { "graph_name", "G102XYZ" } })),
                     "102f: plot.3d_xyz with x as X and Z failed");
            CHECK102(qobject_cast<Graph3D *>(findWindow102(w, QStringLiteral("G102XYZ"))) != nullptr,
                     "102f: 3D graph missing");
            Table *t = w->table(QStringLiteral("T102X"));
            CHECK102(t && t->colPlotDesignation(0) == SciDAVis::X
                             && t->colPlotDesignation(1) == SciDAVis::Y,
                     "102f: designations x/y changed to "
                             << (t ? int(t->colPlotDesignation(0)) : -1) << "/"
                             << (t ? int(t->colPlotDesignation(1)) : -1));
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.3d_xyz"),
                                         QJsonObject{ { "table", "T102X" },
                                                      { "x_col", "x" },
                                                      { "y_col", "y" },
                                                      { "z_col", "z" },
                                                      { "graph_name", "G102XYZ2" } })),
                     "102f: plot.3d_xyz x/y/z after the shared-column plot failed");
        }

        // ---- 102g: function curve type -------------------------------------------------------
        {
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.function"),
                                         QJsonObject{ { "formula", "sin(x)" },
                                                      { "x_min", 0 },
                                                      { "x_max", 10 },
                                                      { "points", 100 },
                                                      { "graph_name", "G102FN" } })),
                     "102g: plot.function failed");
            Graph *g = firstLayer102(w, QStringLiteral("G102FN"));
            CHECK102(g && g->curves() == 1 && g->curveType(0) == Graph::Function,
                     "102g: new function curve type " << (g && g->curves() ? g->curveType(0) : -1));
        }

        CHECK_COND(w->saveFolder(w->projectFolder(), prj), "102: saving the project failed");
    }

    // ---- reopen ----------------------------------------------------------------------------
    {
        AppWinGuard102 rd(new ApplicationWindow(), true);
        CHECK_COND(rd->loadProject(prj), "102: reopening the project failed");
        QCoreApplication::processEvents();
        Rpc102 rpc;
        rpc.srv = rd->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "102: reopened pythonAutomationServer null");

        // 102a
        {
            Graph *g = firstLayer102(rd, QStringLiteral("G102DT"));
            CHECK102(g && g->curves() == 1
                             && g->axesType()[QwtPlot::xBottom] == Graph::AxisType::DateTime,
                     "102a-reopen: datetime X axis lost");
            if (g && g->curves() == 1) {
                double lo = 0, hi = 0, cLo = 0, cHi = 0;
                axisRange102(g, QwtPlot::xBottom, &lo, &hi);
                curveRange102(g, 0, true, &cLo, &cHi);
                CHECK102(lo <= cLo && hi >= cHi,
                         "102a-reopen: X axis " << lo << ".." << hi << " misses " << cLo << ".."
                                                << cHi);
            }
            Graph *gm = firstLayer102(rd, QStringLiteral("G102MO"));
            CHECK102(gm && gm->curves() == 1
                             && gm->axesType()[QwtPlot::xBottom] == Graph::AxisType::Month
                             && gm->curve(0)->dataSize() == 4
                             && std::fabs(gm->curve(0)->x(3) - 4.0) < 1e-12,
                     "102a-reopen: month graph changed");
            Graph *gy = firstLayer102(rd, QStringLiteral("G102DTY"));
            CHECK102(gy && gy->curves() == 1
                             && gy->axesType()[QwtPlot::yLeft] == Graph::AxisType::DateTime,
                     "102a-reopen: datetime Y graph changed");
        }
        // 102b
        for (const PresetTicks102 &pt : presetTicks) {
            Graph *g = firstLayer102(rd, pt.graph);
            CHECK102(g && sameList102(majors102(g, QwtPlot::xBottom), pt.x)
                             && sameList102(majors102(g, QwtPlot::yLeft), pt.y),
                     "102b-reopen: " << pt.graph.toStdString() << " ticks x " << str102(pt.x)
                                     << " -> " << str102(majors102(g, QwtPlot::xBottom)) << ", y "
                                     << str102(pt.y) << " -> "
                                     << str102(majors102(g, QwtPlot::yLeft)));
            // the ticks come back without turning into a user step
            CHECK102(g && g->axisStep(QwtPlot::xBottom) == pt.xStep
                             && g->axisStep(QwtPlot::yLeft) == pt.yStep,
                     "102b-reopen: " << pt.graph.toStdString() << " user step x " << pt.xStep
                                     << " -> " << (g ? g->axisStep(QwtPlot::xBottom) : -1.0)
                                     << ", y " << pt.yStep << " -> "
                                     << (g ? g->axisStep(QwtPlot::yLeft) : -1.0));
        }
        // 102e
        {
            if (auto *mlz = qobject_cast<MultiLayer *>(findWindow102(rd, QStringLiteral("G102CS"))))
                mlz->setPageZoom(1.4545454545454548);
            // While a zoomed page is rendered at 100 %, the autoscaled axes keep the ranges
            // shown on screen (the symbol margin is in pixels, so autoscaling the smaller
            // canvas could add a tick step).
            Graph *g = firstLayer102(rd, QStringLiteral("G102CS"));
            CHECK102(g != nullptr, "102e-reopen: colour scatter missing");
            if (g) {
                double xLo0 = 0, xHi0 = 0, yLo0 = 0, yHi0 = 0;
                axisRange102(g, QwtPlot::xBottom, &xLo0, &xHi0);
                axisRange102(g, QwtPlot::yLeft, &yLo0, &yHi0);
                double xLo1 = 0, xHi1 = 0, yLo1 = 0, yHi1 = 0;
                bool guardActive = false;
                {
                    ScopedUnzoomForExport guard(g->multiLayerParent());
                    guardActive = guard.isActive();
                    g->plotWidget()->resize(g->plotWidget()->size() * 0.5);
                    g->plotWidget()->updateLayout();
                    g->plotWidget()->replot();
                    axisRange102(g, QwtPlot::xBottom, &xLo1, &xHi1);
                    axisRange102(g, QwtPlot::yLeft, &yLo1, &yHi1);
                }
                CHECK102(guardActive, "102e-reopen: page not zoomed, export guard inactive");
                CHECK102(xLo1 == xLo0 && xHi1 == xHi0 && yLo1 == yLo0 && yHi1 == yHi0,
                         "102e-reopen: ranges at 100 % x " << xLo0 << ".." << xHi0 << " -> " << xLo1
                                                           << ".." << xHi1 << ", y " << yLo0
                                                           << ".." << yHi0 << " -> " << yLo1
                                                           << ".." << yHi1);
                double xLo2 = 0, xHi2 = 0;
                g->plotWidget()->replot();
                axisRange102(g, QwtPlot::xBottom, &xLo2, &xHi2);
                CHECK102(xLo2 == xLo0 && xHi2 == xHi0 && g->plotWidget()->axisAutoScale(QwtPlot::xBottom),
                         "102e-reopen: after the export X " << xLo2 << ".." << xHi2 << ", autoscale "
                                                            << g->plotWidget()->axisAutoScale(QwtPlot::xBottom));
            }
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("graph.export"),
                                         QJsonObject{ { "graph", "G102CS" },
                                                      { "path", cscatSvgAfterPath } })),
                     "102e-reopen: SVG export failed");
            const QStringList after = svgNumbers102(cscatSvgAfterPath);
            CHECK102(after == cscatSvgBefore,
                     "102e-reopen: exported labels " << cscatSvgBefore.join(' ').toStdString()
                                                     << " -> " << after.join(' ').toStdString());
        }
        // 102g
        {
            Graph *g = firstLayer102(rd, QStringLiteral("G102FN"));
            CHECK102(g && g->curves() == 1 && g->curveType(0) == Graph::Function,
                     "102g-reopen: function curve type "
                             << (g && g->curves() ? g->curveType(0) : -1));
        }
    }

    // ---- 102h: appendProject with a renamed table keeps the curve label -------------------
    {
        {
            AppWinGuard102 w(new ApplicationWindow(), true);
            Rpc102 rpc;
            rpc.srv = w->pythonAutomationServer();
            CHECK_COND(rpc.srv != nullptr, "102h: pythonAutomationServer null");
            CHECK_COND(rpc.makeTable(QStringLiteral("T102H"), { "x", "y" },
                                     { QJsonArray{ 1, 2, 3 }, QJsonArray{ 2, 4, 9 } }),
                       "102h: creating T102H failed");
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("plot.xy"),
                                         QJsonObject{ { "table", "T102H" },
                                                      { "x_col", "x" },
                                                      { "y_cols", QJsonArray{ "y" } },
                                                      { "graph_name", "G102H" } })),
                     "102h: plot.xy failed");
            CHECK102(Rpc102::ok(rpc.call(QStringLiteral("graph.set_curve_style"),
                                         QJsonObject{ { "graph", "G102H" },
                                                      { "curve", 0 },
                                                      { "label", "Sample H" } })),
                     "102h: label failed");
            CHECK_COND(w->saveFolder(w->projectFolder(), prjLabel), "102h: saving failed");
        }
        AppWinGuard102 ap(new ApplicationWindow(), true);
        Table *ex = ap->newTable(QStringLiteral("T102H"), 3, 2);
        CHECK_COND(ex != nullptr, "102h: existing table");
        ap->setProperty("torigraph_last_append_ok", QVariant());
        ap->appendProject(prjLabel);
        CHECK102(ap->property("torigraph_last_append_ok").toBool(), "102h: appendProject");
        Graph *found = nullptr;
        for (MyWidget *mw2 : ap->windowsList()) {
            auto *ml = qobject_cast<MultiLayer *>(mw2);
            if (!ml || ml->layersList().isEmpty())
                continue;
            Graph *g = ml->layersList().first();
            auto *dc = (g && g->curves() == 1) ? dynamic_cast<DataCurve *>(g->curve(0)) : nullptr;
            if (dc && dc->table() && dc->table() != ex)
                found = g;
        }
        CHECK102(found != nullptr, "102h: appended graph not bound to the renamed table");
        if (found) {
            auto *dc = dynamic_cast<DataCurve *>(found->curve(0));
            std::cout << "[INFO] 102h: appended curve " << dc->title().text().toStdString()
                      << ", label '" << found->curveCustomLabel(0).toStdString() << "'\n";
            CHECK102(found->curveCustomLabel(0) == QLatin1String("Sample H"),
                     "102h: label after append '" << found->curveCustomLabel(0).toStdString()
                                                  << "'");
            CHECK102(found->legend() && found->legend()->text().contains(QStringLiteral("Sample H")),
                     "102h: legend after append lacks the label");
        }
    }

    if (g102Failures > 0) {
        std::cerr << "[FAIL] Suite 102: " << g102Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 102 passed.\n";
    return 0;
}
