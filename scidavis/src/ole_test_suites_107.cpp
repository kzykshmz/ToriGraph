#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "Graph3D.h"
#include "SymbolBox.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <cmath>

// =============================================================================
// Suite 107: graph fixes from the data-driven plot review (JSON-RPC path).
//   107a a polar plot keeps the look of its curves (colour, width, dash, symbol) and a view
//        range widened by a style preset across save -> reopen; files written before the
//        look was saved still load as solid lines with the derived range
//   107b graph.set_axis_break on a log10 axis keeps both sides log10 unless log_before /
//        log_after say otherwise; the axis reports scale "log10", also after reopening
//   107c plot.3d_function without z_min / z_max takes the Z range from the function (50*x*y
//        is not cut at 5); given bounds are kept; describe_api lists z_min / z_max
//   107d plot.add_error_bars without table= takes err_col from the curve's own table, not from
//        the active / last table that happens to have a column of the same name; an explicit
//        table is kept; a curve whose table lacks err_col is an error
//   107e plot.histogram with both bin_min and bin_max given and bin_min >= bin_max is an
//        invalid-params error and leaves no window (it used to bin [bin_min, bin_min+1]); one
//        bound only, and a constant column, still plot as before
//   107f graph.copy_format(layer=L) copies the format of layer L (it used to copy layer 0 when
//        the main window was shown); paste_format(layer=L) changes only layer L; with the main
//        window shown and headless; an out-of-range layer is an error
//   107g curvemath.fill_between and stats.confidence_ellipse on a curve without points return
//        ok false with the reason in "error" (it used to be a bare ok false); describe_api of
//        graph.set_stack_offsets gives the defaults the RPC uses (auto_waterfall true: leaving
//        it out stacks as a waterfall, like auto_waterfall=true and unlike false) and says
//        that y_offset / x_offset are percentages
//   107h the global fit curves of an older file (formula without the parameter values, drawn
//        from the saved points) stay function curves when the project is saved through RPC:
//        they used to become data curves named "RpcFitCurve1_GlobalFit_..." of new hidden
//        tables RpcFitCurve1..4; titles and points are kept on reopen
// =============================================================================

namespace {

int g107Failures = 0;

#define CHECK107(cond, msg)                                                                     \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g107Failures;                                                                     \
        }                                                                                       \
    } while (0)

struct NonInteractiveGuard107
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard107()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard107()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard107
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard107(ApplicationWindow *w = nullptr) : win(w)
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
    ~AppWinGuard107() { reset(); }
    AppWinGuard107(const AppWinGuard107 &) = delete;
    AppWinGuard107 &operator=(const AppWinGuard107 &) = delete;
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

struct Rpc107
{
    PythonAutomationServer *srv = nullptr;
    int id = 10700;
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
            std::cerr << "[INFO] 107: " << method.toStdString() << " error: "
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
    QJsonObject yAxisInfo(const QString &graph)
    {
        const QJsonObject info = result(call(QStringLiteral("graph.get_info"),
                                             QJsonObject{ { "graph", graph } }));
        return info.value(QStringLiteral("layers"))
                .toArray()
                .first()
                .toObject()
                .value(QStringLiteral("axes"))
                .toObject()
                .value(QStringLiteral("y"))
                .toObject();
    }
};

Graph *layer107(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList()) {
        auto *ml = qobject_cast<MultiLayer *>(w);
        if (ml && ml->name() == name && !ml->layersList().isEmpty())
            return ml->layersList().first();
    }
    return nullptr;
}

Graph3D *graph3d107(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            return qobject_cast<Graph3D *>(w);
    return nullptr;
}

struct PolarLook107
{
    bool valid = false;
    QString color;
    double width = 0.0;
    int penStyle = -1;
    int curveStyle = -1;
    int symbol = -1;
    int symbolSize = -1;
    double xLo = 0.0, xHi = 0.0, yLo = 0.0, yHi = 0.0;
};

PolarLook107 polarLook107(Graph *g)
{
    PolarLook107 l;
    QwtPlotCurve *c = (g && g->curves() > 0) ? g->curve(0) : nullptr;
    if (!c || !g->plotWidget())
        return l;
    l.valid = true;
    l.color = c->pen().color().name(QColor::HexArgb);
    l.width = c->pen().widthF();
    l.penStyle = int(c->pen().style());
    l.curveStyle = int(c->style());
    l.symbol = SymbolBox::symbolIndex(c->symbol().style());
    l.symbolSize = c->symbol().size().width();
    const QwtScaleDiv *xd = g->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
    const QwtScaleDiv *yd = g->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
    l.xLo = qMin(xd->lowerBound(), xd->upperBound());
    l.xHi = qMax(xd->lowerBound(), xd->upperBound());
    l.yLo = qMin(yd->lowerBound(), yd->upperBound());
    l.yHi = qMax(yd->lowerBound(), yd->upperBound());
    return l;
}

std::string lookStr107(const PolarLook107 &l)
{
    return QStringLiteral("color %1 width %2 pen %3 curve %4 symbol %5/%6 x %7..%8 y %9..%10")
            .arg(l.color)
            .arg(l.width)
            .arg(l.penStyle)
            .arg(l.curveStyle)
            .arg(l.symbol)
            .arg(l.symbolSize)
            .arg(l.xLo)
            .arg(l.xHi)
            .arg(l.yLo)
            .arg(l.yHi)
            .toStdString();
}

bool brkFlag107(const QJsonObject &axis, const char *key)
{
    return axis.value(QStringLiteral("axis_break")).toObject().value(QLatin1String(key)).toBool();
}

QString scale107(const QJsonObject &axis)
{
    return axis.value(QStringLiteral("scale")).toString();
}

QJsonObject breakAt107(const char *graph, double start, double end)
{
    return QJsonObject{ { "graph", graph }, { "axis", "y" }, { "start", start }, { "end", end } };
}

bool near107(double a, double b)
{
    return std::fabs(a - b) <= 1e-9 * qMax(1.0, std::fabs(b));
}

bool sameLook107(const PolarLook107 &a, const PolarLook107 &b)
{
    return a.valid && b.valid && a.color == b.color && near107(a.width, b.width)
            && a.penStyle == b.penStyle && a.curveStyle == b.curveStyle && a.symbol == b.symbol
            && a.symbolSize == b.symbolSize && near107(a.xLo, b.xLo) && near107(a.xHi, b.xHi)
            && near107(a.yLo, b.yLo) && near107(a.yHi, b.yHi);
}

} // namespace

int runSuite107PlotFixesRound3(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 107: polar curve look / view round-trip, log10 axis "
                 "break, 3D function Z range...\n";
    g107Failures = 0;
    NonInteractiveGuard107 niGuard;

    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "107: temp dir creation failed");
    const QString prj = tmp.filePath(QStringLiteral("fixes_107.sciprj"));
    const QString prjLegacy = tmp.filePath(QStringLiteral("legacy_107.sciprj"));

    PolarLook107 styledBefore, presetBefore;
    double presetRMax = 0.0;

    {
        AppWinGuard107 w(new ApplicationWindow());
        Rpc107 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "107: pythonAutomationServer null");

        // ---- 107a: polar curve look and preset view -------------------------------------------
        {
            QJsonArray th, r;
            for (int i = 0; i <= 36; ++i) {
                const double deg = 10.0 * i;
                th.append(deg);
                r.append(1.0 + std::cos(deg * 3.14159265358979323846 / 180.0));
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T107P"), { "th", "r" }, { th, r }),
                       "107a: polar table");
            for (const char *gname : { "P107A", "P107S" }) {
                CHECK107(Rpc107::ok(rpc.call(QStringLiteral("plot.polar"),
                                             QJsonObject{ { "table", "T107P" },
                                                          { "theta_col", "th" },
                                                          { "r_cols", QJsonArray{ "r" } },
                                                          { "theta_in_radians", false },
                                                          { "graph_name", gname } })),
                         "107a: plot.polar " << gname);
            }
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.set_curve_style"),
                                         QJsonObject{ { "graph", "P107A" },
                                                      { "curve_index", 0 },
                                                      { "color", "#ff0000" },
                                                      { "lw", 4 },
                                                      { "ls", "dash" },
                                                      { "marker", "o" },
                                                      { "ms", 9 } })),
                     "107a: set_curve_style");
            styledBefore = polarLook107(layer107(w, QStringLiteral("P107A")));
            std::cout << "[INFO] 107a: styled polar curve " << lookStr107(styledBefore) << "\n";
            CHECK107(styledBefore.valid && styledBefore.color == QLatin1String("#ffff0000")
                             && near107(styledBefore.width, 4.0)
                             && styledBefore.penStyle == int(Qt::DashLine),
                     "107a: set_curve_style did not take: " << lookStr107(styledBefore));

            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.apply_style_preset"),
                                         QJsonObject{ { "graph", "P107S" },
                                                      { "preset", "slide_28pt" } })),
                     "107a: slide_28pt preset");
            Graph *gs = layer107(w, QStringLiteral("P107S"));
            presetBefore = polarLook107(gs);
            presetRMax = gs ? gs->polarRadiusMax() : 0.0;
            std::cout << "[INFO] 107a: preset polar " << lookStr107(presetBefore) << ", r max "
                      << presetRMax << "\n";
            CHECK107(presetBefore.valid && presetBefore.yHi > 1.64 * presetRMax + 1e-6,
                     "107a: the preset did not widen the polar view (y max " << presetBefore.yHi
                                                                           << ")");
            // A later replot keeps the widened view.
            if (gs) {
                gs->updatePlot();
                const PolarLook107 after = polarLook107(gs);
                CHECK107(near107(after.yHi, presetBefore.yHi)
                                 && near107(after.xHi, presetBefore.xHi),
                         "107a: updatePlot reset the preset view: " << lookStr107(after));
            }
        }

        // ---- 107b: axis break on a log10 axis -------------------------------------------------
        {
            QJsonArray xs, ys;
            for (int i = 1; i <= 10; ++i) {
                xs.append(double(i));
                ys.append(std::pow(10.0, double(i)) / 1e3);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T107B"), { "X", "Y" }, { xs, ys }),
                       "107b: table");
            for (const char *gname : { "G107B", "G107C", "G107D" }) {
                CHECK107(Rpc107::ok(rpc.call(QStringLiteral("plot.xy"),
                                             QJsonObject{ { "table", "T107B" },
                                                          { "x_col", "X" },
                                                          { "y_cols", QJsonArray{ "Y" } },
                                                          { "graph_name", gname } })),
                         "107b: plot.xy " << gname);
            }
            for (const char *gname : { "G107B", "G107D" }) {
                CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.set_axis"),
                                             QJsonObject{ { "graph", gname },
                                                          { "axis", "y" },
                                                          { "scale", "log10" },
                                                          { "min", 1e-3 },
                                                          { "max", 1e8 } })),
                         "107b: log10 axis " << gname);
            }
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.set_axis"),
                                         QJsonObject{ { "graph", "G107C" },
                                                      { "axis", "y" },
                                                      { "min", 0.0 },
                                                      { "max", 1e7 } })),
                     "107b: linear axis G107C");

            // Default flags on a log10 axis: both sides log10.
            const QString brk = QStringLiteral("graph.set_axis_break");
            const QJsonObject rb = Rpc107::result(rpc.call(brk, breakAt107("G107B", 10, 1e4)));
            std::cout << "[INFO] 107b: log axis break -> scale " << scale107(rb).toStdString()
                      << ", log " << brkFlag107(rb, "log_before") << "/"
                      << brkFlag107(rb, "log_after") << "\n";
            CHECK107(scale107(rb) == QLatin1String("log10"),
                     "107b: set_axis_break on a log10 axis returned scale '"
                             << scale107(rb).toStdString() << "'");
            CHECK107(brkFlag107(rb, "log_before") && brkFlag107(rb, "log_after"),
                     "107b: log10 axis broken into linear sides");
            CHECK107(scale107(rpc.yAxisInfo(QStringLiteral("G107B"))) == QLatin1String("log10"),
                     "107b: get_info scale of the broken log10 axis is not log10");
            Graph *gb = layer107(w, QStringLiteral("G107B"));
            CHECK107(gb && gb->axisBreak(QwtPlot::yLeft).enabled
                             && gb->axisBreak(QwtPlot::yLeft).log10Before
                             && gb->axisBreak(QwtPlot::yLeft).log10After,
                     "107b: graph break spec not log10 on both sides");

            // Moving the break keeps the sides' scales.
            const QJsonObject rb2 = Rpc107::result(rpc.call(brk, breakAt107("G107B", 100, 1e5)));
            CHECK107(brkFlag107(rb2, "log_before") && brkFlag107(rb2, "log_after"),
                     "107b: moving the break lost the log10 sides");

            // A linear axis stays linear.
            const QJsonObject rc = Rpc107::result(rpc.call(brk, breakAt107("G107C", 1e5, 1e6)));
            CHECK107(scale107(rc) == QLatin1String("linear") && !brkFlag107(rc, "log_before")
                             && !brkFlag107(rc, "log_after"),
                     "107b: break on a linear axis is not linear");

            // Explicit flags win over the axis scale.
            QJsonObject explicitFlags = breakAt107("G107D", 10, 1e4);
            explicitFlags[QStringLiteral("log_before")] = true;
            explicitFlags[QStringLiteral("log_after")] = false;
            const QJsonObject rd = Rpc107::result(rpc.call(brk, explicitFlags));
            CHECK107(brkFlag107(rd, "log_before") && !brkFlag107(rd, "log_after")
                             && scale107(rd) == QLatin1String("linear"),
                     "107b: explicit log_before=true / log_after=false not kept");
        }

        // ---- 107c: 3D function Z range --------------------------------------------------------
        {
            auto zRange = [&](const QJsonObject &params, double *lo, double *hi) {
                const QJsonObject resp = rpc.call(QStringLiteral("plot.3d_function"), params);
                if (!Rpc107::ok(resp))
                    return false;
                const QJsonObject res = Rpc107::result(resp);
                *lo = res.value(QStringLiteral("z_min")).toDouble(NAN);
                *hi = res.value(QStringLiteral("z_max")).toDouble(NAN);
                Graph3D *g3 = graph3d107(w, res.value(QStringLiteral("name")).toString());
                return g3 && near107(g3->zStart(), *lo) && near107(g3->zStop(), *hi);
            };
            double lo = 0.0, hi = 0.0;
            CHECK107(zRange(QJsonObject{ { "formula", "50*x*y" },
                                         { "x_min", 0 },
                                         { "x_max", 1 },
                                         { "y_min", 0 },
                                         { "y_max", 1 },
                                         { "graph_name", "F107A" } },
                            &lo, &hi),
                     "107c: plot.3d_function 50*x*y");
            std::cout << "[INFO] 107c: 50*x*y on [0,1]^2 -> Z " << lo << ".." << hi << "\n";
            CHECK107(near107(lo, 0.0) && near107(hi, 50.0),
                     "107c: 50*x*y Z range " << lo << ".." << hi << " (expected 0..50)");

            CHECK107(zRange(QJsonObject{ { "formula", "50*x*y" },
                                         { "x_min", 0 },
                                         { "x_max", 1 },
                                         { "y_min", 0 },
                                         { "y_max", 1 },
                                         { "z_min", -1 },
                                         { "z_max", 2 },
                                         { "graph_name", "F107B" } },
                            &lo, &hi),
                     "107c: plot.3d_function with z range");
            CHECK107(near107(lo, -1.0) && near107(hi, 2.0),
                     "107c: given Z range not kept: " << lo << ".." << hi);

            CHECK107(zRange(QJsonObject{ { "formula", "50*x*y" },
                                         { "x_min", 0 },
                                         { "x_max", 1 },
                                         { "y_min", 0 },
                                         { "y_max", 1 },
                                         { "z_max", 10 },
                                         { "graph_name", "F107C" } },
                            &lo, &hi),
                     "107c: plot.3d_function with z_max only");
            CHECK107(near107(lo, 0.0) && near107(hi, 10.0),
                     "107c: z_max only gives Z " << lo << ".." << hi << " (expected 0..10)");

            CHECK107(zRange(QJsonObject{ { "formula", "3" }, { "graph_name", "F107D" } }, &lo, &hi),
                     "107c: plot.3d_function constant");
            CHECK107(lo < 3.0 && hi > 3.0 && hi - lo < 10.0,
                     "107c: constant surface Z " << lo << ".." << hi);

            const QJsonObject api =
                    Rpc107::result(rpc.call(QStringLiteral("app.describe_api"),
                                            QJsonObject{ { "method", "plot.3d_function" } }));
            const QString apiText =
                    QString::fromUtf8(QJsonDocument(api).toJson(QJsonDocument::Compact));
            CHECK107(apiText.contains(QStringLiteral("z_min"))
                             && apiText.contains(QStringLiteral("z_max")),
                     "107c: describe_api does not list z_min / z_max");
        }

        // ---- 107d: add_error_bars without table= reads the curve's own table ---------------
        {
            QJsonArray xs, ys, es, others;
            for (int i = 0; i < 5; ++i) {
                xs.append(double(i));
                ys.append(10.0 + i);
                es.append(0.1);
                others.append(3.0);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T107E"), { "x", "y", "e" }, { xs, ys, es }),
                       "107d: table T107E");
            CHECK_COND(rpc.makeTable(QStringLiteral("N107E"), { "x", "y" }, { xs, ys }),
                       "107d: table N107E");
            // 後から作った表（同名の列 e を持つ）。table を省くと以前はこちらが使われた。
            CHECK_COND(rpc.makeTable(QStringLiteral("O107E"), { "x", "e" }, { xs, others }),
                       "107d: table O107E");
            for (const char *gname : { "G107E", "G107F" }) {
                CHECK107(Rpc107::ok(rpc.call(QStringLiteral("plot.xy"),
                                             QJsonObject{ { "table", "T107E" },
                                                          { "x_col", "x" },
                                                          { "y_cols", QJsonArray{ "y" } },
                                                          { "graph_name", gname } })),
                         "107d: plot.xy " << gname);
            }
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("plot.xy"),
                                         QJsonObject{ { "table", "N107E" },
                                                      { "x_col", "x" },
                                                      { "y_cols", QJsonArray{ "y" } },
                                                      { "graph_name", "G107N" } })),
                     "107d: plot.xy G107N");
            // エラーバーの曲線のタイトルは誤差の列のフル名（表名_列名）。
            auto errTitle = [&](const char *gname) {
                Graph *g = layer107(w, QString::fromLatin1(gname));
                if (!g)
                    return QString();
                for (int ci = 0; ci < g->curves(); ++ci)
                    if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(ci)))
                        return er->title().text();
                return QString();
            };
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("plot.add_error_bars"),
                                         QJsonObject{ { "graph", "G107E" },
                                                      { "curve", 0 },
                                                      { "err_col", "e" } })),
                     "107d: add_error_bars without table");
            std::cout << "[INFO] 107d: error bars without table use '"
                      << errTitle("G107E").toStdString() << "'\n";
            CHECK107(errTitle("G107E") == QLatin1String("T107E_e"),
                     "107d: add_error_bars without table used '"
                             << errTitle("G107E").toStdString()
                             << "' instead of the curve's table T107E_e");
            // 明示した table はそのまま使う。
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("plot.add_error_bars"),
                                         QJsonObject{ { "graph", "G107F" },
                                                      { "table", "O107E" },
                                                      { "curve", 0 },
                                                      { "err_col", "e" } })),
                     "107d: add_error_bars with table=O107E");
            CHECK107(errTitle("G107F") == QLatin1String("O107E_e"),
                     "107d: add_error_bars table=O107E used '" << errTitle("G107F").toStdString()
                                                               << "'");
            // 曲線の表に誤差の列が無ければ、別の表の同名の列を黙って使わずにエラーにする。
            CHECK107(!Rpc107::ok(rpc.call(QStringLiteral("plot.add_error_bars"),
                                          QJsonObject{ { "graph", "G107N" },
                                                       { "curve", 0 },
                                                       { "err_col", "e" } })),
                     "107d: add_error_bars took column e from another table for N107E's curve");
            CHECK107(errTitle("G107N").isEmpty(),
                     "107d: G107N got error bars '" << errTitle("G107N").toStdString() << "'");
        }

        // ---- 107e: plot.histogram with bin_min >= bin_max is an error ------------------------
        {
            QJsonArray vs, cs;
            for (int i = 0; i < 40; ++i) {
                vs.append(std::sin(0.37 * i) * 2.0);
                cs.append(5.0);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T107H"), { "v", "c" }, { vs, cs }),
                       "107e: table T107H");
            const int nWin = w->windowsList().size();
            struct Bad
            {
                const char *label;
                QJsonObject extra;
            };
            const QList<Bad> bad = {
                { "bin_min 3 > bin_max -3", QJsonObject{ { "bin_min", 3 }, { "bin_max", -3 } } },
                { "bin_min = bin_max", QJsonObject{ { "bin_min", 1 }, { "bin_max", 1 } } },
                { "min 2 > max 0 with bin_size",
                  QJsonObject{ { "min", 2 }, { "max", 0 }, { "bin_size", 0.5 } } },
            };
            for (const Bad &b : bad) {
                QJsonObject p = b.extra;
                p.insert(QStringLiteral("table"), QStringLiteral("T107H"));
                p.insert(QStringLiteral("col"), QStringLiteral("v"));
                const QJsonObject r = rpc.call(QStringLiteral("plot.histogram"), p);
                const int code = r.value(QStringLiteral("error"))
                                         .toObject()
                                         .value(QStringLiteral("code"))
                                         .toInt();
                CHECK107(!Rpc107::ok(r) && code == -32602,
                         "107e: histogram " << b.label << " was not an invalid-params error"
                                            << " (code " << code << ")");
                CHECK107(w->windowsList().size() == nWin,
                         "107e: histogram " << b.label << " left a window behind");
            }
            // 片方だけ・自動の場合は今までどおり（bin_min がデータの最大より上でも作図する）。
            const QJsonObject r1 = rpc.call(QStringLiteral("plot.histogram"),
                                            QJsonObject{ { "table", "T107H" },
                                                         { "col", "v" },
                                                         { "bin_min", 10 } });
            CHECK107(Rpc107::ok(r1), "107e: histogram with bin_min only failed");
            const QJsonObject r2 = rpc.call(QStringLiteral("plot.histogram"),
                                            QJsonObject{ { "table", "T107H" },
                                                         { "col", "v" },
                                                         { "bin_min", -1 },
                                                         { "bin_max", 1 } });
            CHECK107(Rpc107::ok(r2), "107e: histogram with bin_min < bin_max failed");
            // 全部同じ値の列（自動範囲が潰れる）も今までどおり作図する。
            const QJsonObject r3 = rpc.call(QStringLiteral("plot.histogram"),
                                            QJsonObject{ { "table", "T107H" },
                                                         { "col", "c" },
                                                         { "bins", 10 } });
            CHECK107(Rpc107::ok(r3), "107e: histogram of a constant column failed");
        }

        CHECK_COND(w->saveFolder(w->projectFolder(), prj), "107: saving the project failed");
    }

    // ---- reopen ------------------------------------------------------------------------------
    {
        AppWinGuard107 rd(new ApplicationWindow());
        CHECK_COND(rd->loadProject(prj), "107: reopening the project failed");
        QCoreApplication::processEvents();
        Rpc107 rpc;
        rpc.srv = rd->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "107: reopened pythonAutomationServer null");

        const PolarLook107 styledAfter = polarLook107(layer107(rd, QStringLiteral("P107A")));
        std::cout << "[INFO] 107a-reopen: styled polar curve " << lookStr107(styledAfter) << "\n";
        CHECK107(sameLook107(styledBefore, styledAfter),
                 "107a-reopen: polar curve look changed: " << lookStr107(styledBefore) << " -> "
                                                            << lookStr107(styledAfter));
        const PolarLook107 presetAfter = polarLook107(layer107(rd, QStringLiteral("P107S")));
        std::cout << "[INFO] 107a-reopen: preset polar " << lookStr107(presetAfter) << "\n";
        CHECK107(sameLook107(presetBefore, presetAfter),
                 "107a-reopen: preset polar changed: " << lookStr107(presetBefore) << " -> "
                                                        << lookStr107(presetAfter));

        const QJsonObject yb = rpc.yAxisInfo(QStringLiteral("G107B"));
        CHECK107(scale107(yb) == QLatin1String("log10") && brkFlag107(yb, "log_before")
                         && brkFlag107(yb, "log_after"),
                 "107b-reopen: broken log10 axis reads scale '" << scale107(yb).toStdString()
                                                                << "'");

        Graph3D *f3 = graph3d107(rd, QStringLiteral("F107A"));
        CHECK107(f3 && near107(f3->zStart(), 0.0) && near107(f3->zStop(), 50.0),
                 "107c-reopen: 50*x*y Z range " << (f3 ? f3->zStart() : NAN) << ".."
                                                << (f3 ? f3->zStop() : NAN));
    }

    // ---- 107a: a project written before the polar look was saved -----------------------------
    {
        QFile f(prj);
        CHECK_COND(f.open(QIODevice::ReadOnly), "107: reading the saved project");
        QString text = QString::fromUtf8(f.readAll());
        f.close();
        CHECK107(text.contains(QStringLiteral("<ToriPolarView")),
                 "107a: the widened preset view is not saved");
        // Drop what older versions did not write: <ToriPolarView> and the look attributes of
        // <ToriPolarSeries>.
        const QRegularExpression lookAttr(QStringLiteral(
                " (style|curve|dash|sym|symsize|sympen|symw|symbrush|visible)=\"[^\"]*\""));
        QStringList lines = text.split(QLatin1Char('\n'));
        QStringList legacy;
        for (QString ln : lines) {
            const QString t = ln.trimmed();
            if (t.startsWith(QLatin1String("<ToriPolarView")))
                continue;
            if (t.startsWith(QLatin1String("<ToriPolarSeries")))
                ln.remove(lookAttr);
            legacy << ln;
        }
        QFile lf(prjLegacy);
        CHECK_COND(lf.open(QIODevice::WriteOnly | QIODevice::Truncate),
                   "107: writing legacy project");
        lf.write(legacy.join(QLatin1Char('\n')).toUtf8());
        lf.close();

        AppWinGuard107 lg(new ApplicationWindow());
        CHECK_COND(lg->loadProject(prjLegacy), "107: opening the legacy project failed");
        QCoreApplication::processEvents();
        Graph *g = layer107(lg, QStringLiteral("P107A"));
        const PolarLook107 l = polarLook107(g);
        std::cout << "[INFO] 107a-legacy: polar curve " << lookStr107(l) << "\n";
        // One series: the view derived from the radius is -1.44 r .. 1.64 r.
        const double rMax = g ? g->polarRadiusMax() : 0.0;
        const double dy0 = -1.44 * rMax, dy1 = 1.64 * rMax;
        CHECK107(l.valid && l.color == QLatin1String("#ffff0000") && near107(l.width, 4.0)
                         && l.penStyle == int(Qt::SolidLine) && near107(l.yHi, dy1)
                         && near107(l.yLo, dy0),
                 "107a-legacy: old-format polar series " << lookStr107(l));
        Graph *gs = layer107(lg, QStringLiteral("P107S"));
        const PolarLook107 ls = polarLook107(gs);
        CHECK107(ls.valid && near107(ls.yHi, 1.64 * presetRMax),
                 "107a-legacy: old-format view y max " << ls.yHi << " (expected "
                                                       << 1.64 * presetRMax << ")");
    }

    // ---- 107f: graph.copy_format / paste_format honour layer= ---------------------------------
    // With the main window shown (graph.copy_format activates the graph and uses the
    // application's format clipboard) and headless (RPC clipboard).
    for (const bool headless : { false, true }) {
        const char *mode = headless ? "headless" : "shown";
        AppWinGuard107 w(new ApplicationWindow());
        if (!headless) {
            w->setHeadlessMode(false);
            w->resize(1280, 800);
            w->show();
            QCoreApplication::processEvents();
        }
        Rpc107 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "107f: pythonAutomationServer null");
        QJsonArray xs, as, cs;
        for (int i = 1; i <= 10; ++i) {
            xs.append(i);
            as.append(i);
            cs.append(3 * i);
        }
        CHECK_COND(rpc.makeTable(QStringLiteral("T107F"), { "x", "a", "c" }, { xs, as, cs }),
                   "107f: table");
        auto plot = [&](const char *name) {
            return Rpc107::ok(rpc.call(QStringLiteral("plot.xy"),
                                       QJsonObject{ { "table", "T107F" },
                                                    { "x_col", "x" },
                                                    { "y_cols", QJsonArray{ "a" } },
                                                    { "graph_name", name } }));
        };
        auto addLayerWithCurve = [&](const char *name) {
            return Rpc107::ok(rpc.call(QStringLiteral("graph.add_layer"),
                                       QJsonObject{ { "graph", name } }))
                    && Rpc107::ok(rpc.call(QStringLiteral("plot.add_curve"),
                                           QJsonObject{ { "graph", name },
                                                        { "layer", 1 },
                                                        { "table", "T107F" },
                                                        { "x_col", "x" },
                                                        { "y_col", "c" } }));
        };
        // "color/width/y-numbers-pt" of the first curve of a layer
        auto look = [&](const char *name, int layer) {
            const QJsonObject info = Rpc107::result(
                    rpc.call(QStringLiteral("graph.get_info"), QJsonObject{ { "graph", name } }));
            const QJsonObject l = info.value(QStringLiteral("layers")).toArray().at(layer).toObject();
            const QJsonObject c = l.value(QStringLiteral("curves")).toArray().at(0).toObject();
            return QStringLiteral("%1/%2/%3")
                    .arg(c.value(QStringLiteral("color")).toString())
                    .arg(c.value(QStringLiteral("line_width")).toDouble())
                    .arg(l.value(QStringLiteral("axes"))
                                 .toObject()
                                 .value(QStringLiteral("y"))
                                 .toObject()
                                 .value(QStringLiteral("numbers_font_size"))
                                 .toDouble())
                    .toStdString();
        };
        CHECK_COND(plot("ML107F") && addLayerWithCurve("ML107F"), "107f: two-layer source graph");
        CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.set_curve_style"),
                                     QJsonObject{ { "graph", "ML107F" }, { "layer", 0 }, { "curve", 0 },
                                                  { "color", "#ff0000" }, { "lw", 4 } })),
                 "107f: style layer 0");
        CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.set_curve_style"),
                                     QJsonObject{ { "graph", "ML107F" }, { "layer", 1 }, { "curve", 0 },
                                                  { "color", "#0000ff" }, { "lw", 6 } })),
                 "107f: style layer 1");
        CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.set_axis"),
                                     QJsonObject{ { "graph", "ML107F" }, { "layer", 1 }, { "axis", "y" },
                                                  { "numbers_font_size", 20 } })),
                 "107f: axis font of layer 1");
        const std::string src0 = look("ML107F", 0), src1 = look("ML107F", 1);
        std::cout << "[INFO] 107f (" << mode << "): source layer 0 " << src0 << ", layer 1 " << src1
                  << "\n";
        CHECK_COND(src0.rfind("#ff0000/4/", 0) == 0 && src1 == "#0000ff/6/20",
                   "107f: source styles not set: " << src0 << " / " << src1);

        // copy_format(layer=L) -> a one-layer graph takes layer L's format
        for (int L = 0; L < 2; ++L) {
            const QString one = QStringLiteral("One107F%1").arg(L);
            CHECK_COND(plot(one.toUtf8().constData()), "107f: one-layer graph " << L);
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.copy_format"),
                                         QJsonObject{ { "graph", "ML107F" }, { "layer", L } })),
                     "107f: copy_format layer " << L);
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.paste_format"),
                                         QJsonObject{ { "graph", one } })),
                     "107f: paste_format to " << one.toStdString());
            const std::string got = look(one.toUtf8().constData(), 0);
            const std::string want = L == 0 ? src0 : src1;
            std::cout << "[INFO] 107f (" << mode << "): copy layer " << L << " -> " << got << "\n";
            CHECK107(got == want, "107f (" << mode << "): copy_format(layer=" << L
                                           << ") pasted " << got << ", expected " << want);
        }

        // paste_format(layer=1) changes only layer 1 of a two-layer graph
        CHECK_COND(plot("Two107F") && addLayerWithCurve("Two107F"), "107f: two-layer target graph");
        const std::string two0 = look("Two107F", 0);
        CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.copy_format"),
                                     QJsonObject{ { "graph", "ML107F" }, { "layer", 1 } })),
                 "107f: copy_format layer 1 for the layer paste");
        CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.paste_format"),
                                     QJsonObject{ { "graph", "Two107F" }, { "layer", 1 } })),
                 "107f: paste_format layer 1");
        const std::string t0 = look("Two107F", 0), t1 = look("Two107F", 1);
        std::cout << "[INFO] 107f (" << mode << "): paste to layer 1 -> layer 0 " << t0
                  << ", layer 1 " << t1 << "\n";
        CHECK107(t1 == src1, "107f (" << mode << "): paste_format(layer=1) gave layer 1 " << t1
                                      << ", expected " << src1);
        CHECK107(t0 == two0, "107f (" << mode << "): paste_format(layer=1) changed layer 0: "
                                      << two0 << " -> " << t0);
        // an out-of-range layer is an error, not a silent copy of nothing
        CHECK107(!Rpc107::ok(rpc.call(QStringLiteral("graph.copy_format"),
                                      QJsonObject{ { "graph", "ML107F" }, { "layer", 5 } })),
                 "107f: copy_format(layer=5) on a two-layer graph did not fail");
    }

    // ---- 107g: ok false with a reason; set_stack_offsets as described --------------------------
    {
        AppWinGuard107 w(new ApplicationWindow());
        Rpc107 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "107g: pythonAutomationServer null");
        auto compact = [](const QJsonObject &o) {
            return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)).toStdString();
        };
        QJsonArray xs, as, bs, cs, empties;
        for (int i = 1; i <= 10; ++i) {
            xs.append(i);
            as.append(double(i) * i);
            bs.append(2.0 * i);
            cs.append(30.0 + i);
            empties.append(QJsonValue::Null);
        }
        CHECK_COND(rpc.makeTable(QStringLiteral("T107G"), { "x", "a", "b", "c" }, { xs, as, bs, cs }),
                   "107g: table");
        CHECK_COND(Rpc107::ok(rpc.call(QStringLiteral("plot.xy"),
                                       QJsonObject{ { "table", "T107G" },
                                                    { "x_col", "x" },
                                                    { "y_cols", QJsonArray{ "a", "b", "c" } },
                                                    { "graph_name", "G107G" } })),
                   "107g: plot");
        // emptying column b leaves its curve on the graph without points
        CHECK_COND(Rpc107::ok(rpc.call(QStringLiteral("table.set_column_data"),
                                       QJsonObject{ { "table", "T107G" },
                                                    { "col", "b" },
                                                    { "data", empties } })),
                   "107g: emptying column b");
        Graph *g = layer107(w, QStringLiteral("G107G"));
        CHECK_COND(g && g->curves() == 3 && g->curve(1) && g->curve(1)->dataSize() == 0,
                   "107g: the curve of the emptied column is not an empty curve");

        auto explained = [](const QJsonObject &r) {
            return !r.value(QStringLiteral("ok")).toBool(true)
                    && r.value(QStringLiteral("error")).toString().contains(QLatin1String("T107G_b"));
        };
        for (const QJsonObject &p :
             { QJsonObject{ { "graph", "G107G" }, { "curve_a", 0 }, { "curve_b", 1 } },
               QJsonObject{ { "graph", "G107G" }, { "curve_a", 1 }, { "curve_b", 2 } } }) {
            const QJsonObject r = rpc.call(QStringLiteral("curvemath.fill_between"), p);
            std::cout << "[INFO] 107g: fill_between " << compact(p) << " -> "
                      << compact(Rpc107::result(r)) << "\n";
            CHECK107(Rpc107::ok(r) && explained(Rpc107::result(r)),
                     "107g: fill_between with the empty curve gave no reason: "
                             << compact(Rpc107::result(r)));
        }
        {
            const QJsonObject r = rpc.call(QStringLiteral("stats.confidence_ellipse"),
                                           QJsonObject{ { "graph", "G107G" }, { "curve", 1 } });
            std::cout << "[INFO] 107g: confidence_ellipse of the empty curve -> "
                      << compact(Rpc107::result(r)) << "\n";
            CHECK107(Rpc107::ok(r) && explained(Rpc107::result(r)),
                     "107g: confidence_ellipse of the empty curve gave no reason: "
                             << compact(Rpc107::result(r)));
        }
        // curves with points still work and carry no error
        {
            const QJsonObject r = Rpc107::result(rpc.call(
                    QStringLiteral("curvemath.fill_between"),
                    QJsonObject{ { "graph", "G107G" }, { "curve_a", 0 }, { "curve_b", 2 } }));
            CHECK107(r.value(QStringLiteral("ok")).toBool() && !r.contains(QStringLiteral("error")),
                     "107g: fill_between of two curves with points: " << compact(r));
            const QJsonObject e = Rpc107::result(rpc.call(
                    QStringLiteral("stats.confidence_ellipse"),
                    QJsonObject{ { "graph", "G107G" }, { "curve", 0 } }));
            CHECK107(e.value(QStringLiteral("ok")).toBool() && !e.contains(QStringLiteral("error")),
                     "107g: confidence_ellipse of a curve with points: " << compact(e));
        }

        // set_stack_offsets: describe_api tells the defaults the RPC uses
        const QJsonObject api =
                Rpc107::result(rpc.call(QStringLiteral("app.describe_api"),
                                        QJsonObject{ { "method", "graph.set_stack_offsets" } }));
        QJsonObject awParam;
        for (const QJsonValue &v : api.value(QStringLiteral("params")).toArray())
            if (v.toObject().value(QStringLiteral("name")).toString() == QLatin1String("auto_waterfall"))
                awParam = v.toObject();
        std::cout << "[INFO] 107g: describe_api set_stack_offsets: " << compact(api) << "\n";
        CHECK107(awParam.value(QStringLiteral("default")).isBool()
                         && awParam.value(QStringLiteral("default")).toBool(),
                 "107g: describe_api gives auto_waterfall the default " << compact(awParam)
                                                                        << " (the RPC uses true)");
        const QString apiText = api.value(QStringLiteral("summary")).toString()
                + api.value(QStringLiteral("sdk_method")).toString();
        CHECK107(apiText.contains(QLatin1Char('%')),
                 "107g: describe_api does not say that the offsets are percentages");

        CHECK_COND(Rpc107::ok(rpc.call(QStringLiteral("plot.xy"),
                                       QJsonObject{ { "table", "T107G" },
                                                    { "x_col", "x" },
                                                    { "y_cols", QJsonArray{ "a", "c" } },
                                                    { "graph_name", "S107G" } })),
                   "107g: stack graph");
        auto stacked = [&](QJsonObject p) {
            p[QStringLiteral("graph")] = QStringLiteral("S107G");
            rpc.call(QStringLiteral("graph.set_stack_offsets"),
                     QJsonObject{ { "graph", "S107G" }, { "reset", true } });
            CHECK107(Rpc107::ok(rpc.call(QStringLiteral("graph.set_stack_offsets"), p)),
                     "107g: set_stack_offsets " << compact(p));
            const QJsonObject c = Rpc107::result(rpc.call(QStringLiteral("graph.get_info"),
                                                          QJsonObject{ { "graph", "S107G" } }))
                                          .value(QStringLiteral("layers"))
                                          .toArray()
                                          .first()
                                          .toObject()
                                          .value(QStringLiteral("curves"))
                                          .toArray()
                                          .at(1)
                                          .toObject();
            return QStringLiteral("%1..%2")
                    .arg(c.value(QStringLiteral("y_min")).toDouble(), 0, 'g', 12)
                    .arg(c.value(QStringLiteral("y_max")).toDouble(), 0, 'g', 12)
                    .toStdString();
        };
        const std::string byDefault = stacked(QJsonObject{ { "y_offset", 10 } });
        const std::string waterfall = stacked(QJsonObject{ { "y_offset", 10 }, { "auto_waterfall", true } });
        const std::string uniform = stacked(QJsonObject{ { "y_offset", 10 }, { "auto_waterfall", false } });
        std::cout << "[INFO] 107g: second curve stacked by default " << byDefault << ", waterfall "
                  << waterfall << ", uniform " << uniform << "\n";
        CHECK107(byDefault == waterfall && byDefault != uniform,
                 "107g: set_stack_offsets without auto_waterfall " << byDefault
                         << " is not the waterfall " << waterfall << " (uniform " << uniform << ")");
    }

    // ---- 107h: global fit curves of an older file stay function curves when saved via RPC ----
    {
        const QString livePath = tmp.filePath(QStringLiteral("global_107h.sciprj"));
        const QString oldPath = tmp.filePath(QStringLiteral("global_107h_old.sciprj"));
        const QString resavedPath = tmp.filePath(QStringLiteral("global_107h_resaved.sciprj"));
        struct Fc107
        {
            QString title;
            QVector<double> x, y;
        };
        auto functionCurves = [](ApplicationWindow *app) {
            QList<Fc107> out;
            if (Graph *g = layer107(app, QStringLiteral("G107H")))
                for (int i = 0; i < g->curves(); ++i)
                    if (auto *fc = dynamic_cast<FunctionCurve *>(g->curve(i))) {
                        Fc107 f;
                        f.title = fc->title().text();
                        for (int k = 0; k < fc->dataSize(); ++k) {
                            f.x << fc->x(k);
                            f.y << fc->y(k);
                        }
                        out << f;
                    }
            return out;
        };
        auto curveTitles = [](ApplicationWindow *app) {
            QStringList out;
            if (Graph *g = layer107(app, QStringLiteral("G107H")))
                for (int i = 0; i < g->curves(); ++i)
                    out << g->curve(i)->title().text();
            return out.join(QStringLiteral(", ")).toStdString();
        };
        auto rpcFitTables = [](ApplicationWindow *app) {
            QStringList out;
            for (MyWidget *mw2 : app->windowsList())
                if (mw2 && mw2->name().startsWith(QLatin1String("RpcFitCurve")))
                    out << mw2->name();
            return out.join(QStringLiteral(", ")).toStdString();
        };
        auto samePoints = [](const QList<Fc107> &a, const QList<Fc107> &b) {
            if (a.size() != b.size())
                return false;
            for (int i = 0; i < a.size(); ++i) {
                if (a[i].title != b[i].title || a[i].x.size() != b[i].x.size())
                    return false;
                for (int k = 0; k < a[i].x.size(); ++k)
                    if (!(std::fabs(a[i].x[k] - b[i].x[k]) <= 1e-9 * qMax(1.0, std::fabs(b[i].x[k])))
                        || !(std::fabs(a[i].y[k] - b[i].y[k]) <= 1e-9 * qMax(1.0, std::fabs(b[i].y[k]))))
                        return false;
            }
            return true;
        };

        QList<Fc107> live;
        {
            AppWinGuard107 w(new ApplicationWindow());
            Rpc107 rpc;
            rpc.srv = w->pythonAutomationServer();
            CHECK_COND(rpc.srv != nullptr, "107h: pythonAutomationServer null");
            QJsonArray xs, as, bs;
            for (int i = 0; i <= 20; ++i) {
                const double x = 0.5 * i;
                xs.append(x);
                as.append(1.0 + 5.0 * std::exp(-x / 3.0));
                bs.append(2.0 + 3.0 * std::exp(-x / 3.0));
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T107H"), { "x", "a", "b" }, { xs, as, bs }),
                       "107h: table");
            CHECK_COND(Rpc107::ok(rpc.call(QStringLiteral("plot.xy"),
                                           QJsonObject{ { "table", "T107H" },
                                                        { "x_col", "x" },
                                                        { "y_cols", QJsonArray{ "a", "b" } },
                                                        { "graph_name", "G107H" } })),
                       "107h: plot");
            CHECK_COND(Rpc107::ok(rpc.call(QStringLiteral("analysis.fit_global"),
                                           QJsonObject{ { "graph", "G107H" },
                                                        { "curves", QJsonArray{ 0, 1 } },
                                                        { "formula", "y0+A*exp(-x/t)" },
                                                        { "param_names", QJsonArray{ "y0", "A", "t" } },
                                                        { "shared_params", QJsonArray{ "t" } } })),
                       "107h: fit_global");
            live = functionCurves(w);
            std::cout << "[INFO] 107h: live curves " << curveTitles(w) << "\n";
            CHECK_COND(live.size() == 2 && live[0].x.size() > 1 && std::isfinite(live[0].y[0]),
                       "107h: the global fit made " << live.size() << " function curves");
            CHECK_COND(Rpc107::ok(rpc.call(QStringLiteral("app.save_project"),
                                           QJsonObject{ { "path", livePath } })),
                       "107h: saving the project");
        }

        // an older file: the formulas of the global fit curves carry no "name=value" lines
        {
            QFile in(livePath);
            CHECK_COND(in.open(QIODevice::ReadOnly), "107h: reading the saved project");
            QString text = QString::fromUtf8(in.readAll());
            in.close();
            const QRegularExpression paramLines(QStringLiteral(
                    "(<formula>\\r?\\n)(?:[A-Za-z_]\\w*=[^\\r\\n]*\\r?\\n)+\\r?\\n"));
            int n = 0;
            for (auto it = paramLines.globalMatch(text); it.hasNext(); it.next())
                ++n;
            CHECK_COND(n == 2, "107h: " << n << " formulas with parameter lines (expected 2)");
            text.replace(paramLines, QStringLiteral("\\1"));
            QFile out(oldPath);
            CHECK_COND(out.open(QIODevice::WriteOnly | QIODevice::Truncate),
                       "107h: writing the older file");
            out.write(text.toUtf8());
        }

        {
            AppWinGuard107 rd(new ApplicationWindow());
            CHECK_COND(rd->loadProject(oldPath), "107h: opening the older file failed");
            QCoreApplication::processEvents();
            const QList<Fc107> opened = functionCurves(rd);
            CHECK107(samePoints(opened, live),
                     "107h: the older file shows other fit curves: " << curveTitles(rd));
            Rpc107 rpc;
            rpc.srv = rd->pythonAutomationServer();
            CHECK_COND(rpc.srv != nullptr, "107h: reopened pythonAutomationServer null");
            CHECK_COND(Rpc107::ok(rpc.call(QStringLiteral("app.save_project"),
                                           QJsonObject{ { "path", resavedPath } })),
                       "107h: saving the older file through RPC");
            std::cout << "[INFO] 107h: after the RPC save: curves " << curveTitles(rd)
                      << "; RpcFitCurve tables '" << rpcFitTables(rd) << "'\n";
            CHECK107(samePoints(functionCurves(rd), live),
                     "107h: the RPC save turned the fit curves into " << curveTitles(rd));
            CHECK107(rpcFitTables(rd).empty(),
                     "107h: the RPC save made the tables " << rpcFitTables(rd));
        }

        {
            AppWinGuard107 rd(new ApplicationWindow());
            CHECK_COND(rd->loadProject(resavedPath), "107h: reopening the resaved file failed");
            QCoreApplication::processEvents();
            std::cout << "[INFO] 107h: resaved and reopened: curves " << curveTitles(rd) << "\n";
            CHECK107(samePoints(functionCurves(rd), live),
                     "107h: resaved and reopened, the fit curves are " << curveTitles(rd));
            CHECK107(rpcFitTables(rd).empty(),
                     "107h: resaved and reopened, there are the tables " << rpcFitTables(rd));
        }
    }

    if (g107Failures > 0) {
        std::cerr << "[FAIL] Suite 107: " << g107Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 107 passed.\n";
    return 0;
}
