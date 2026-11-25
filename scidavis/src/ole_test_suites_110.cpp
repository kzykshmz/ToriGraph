#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTemporaryDir>

#include <cmath>

// =============================================================================
// Suite 110: curve maths and table-driven analysis must not change the graph or the worksheet
// behind the caller's back (JSON-RPC path).
//   110a curvemath.integrate with plot_curve=false (default, x range, cubic; a double-Y graph
//        on both curves) returns the area without changing the axis ranges, the curve count
//        or the windows (it used to add the integral curve for a moment, which autoscaled the
//        axes: Y 500..600 became 0..600, and save -> reopen showed 500..600 again); the ranges
//        survive save -> reopen. plot_curve=true still adds the integral curve.
//   110b curvemath.subtract_line on a graph with automatic axis titles ("温度 / K", "信号 / A")
//        keeps the titles, also after save -> reopen (the result columns "X" / "SubtractedY"
//        had no Long Name or units, so the titles became "X / K" and "Y / A"); the legend
//        still names the result "SubtractedY"
//   110c analysis.fit_linear(table, x_col, y_col) fits y against x_col: when x_col is right of
//        y_col, and when y_col is already plotted against another X (the curve was reused
//        whatever its X); x_col's plot designation is left as it was; a curve that has the
//        same X is still reused, and without x_col the existing reuse is unchanged
//   110d curvemath.integrate(plot_curve=true) on the right-axis curve of a double-Y graph puts
//        the integral on the right axis (it went on the left axis and widened its range, which
//        squashed the left curve); the left axis range is kept; the integral of a left-axis
//        curve stays on the left; the axis survives save -> reopen
//   110e fits of the right-axis curve of a double-Y graph: analysis.fit_nonlinear with the
//        default "uniform X" output (a function curve) and confidence / prediction bands, and
//        analysis.analyze_peaks with component curves, put every result curve on the right axis
//        (the fit curve, the bands and the peak curves went on the left axis and changed its
//        range; only the "same X as the data" output followed the source curve); the left axis
//        range is kept; the axes survive save -> reopen
//   110f curve maths and signal processing of a right-axis curve of a double-Y graph
//        (curvemath.normalize / subtract_line / eval / average_curves with the SD band,
//        signal.smooth gaussian / iir_filter / envelope (Hilbert and peak) / decimate /
//        savgol_derivative / wavelet_denoise / fsd) put their result curves on the right axis
//        (they went on the left axis: the left axis 0..0.1 became 0..2000); the left axis range
//        and title are kept; the result of a left-axis curve stays on the left; the axes
//        survive save -> reopen
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g110Failures = 0;

// rtti of Graph's shaded band item (the fill of a fit's confidence / prediction band)
constexpr int kShadedBandRtti110 = QwtPlotItem::Rtti_PlotUserItem + 43;

#define CHECK110(cond, msg)                                                                     \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g110Failures;                                                                     \
        }                                                                                       \
    } while (0)

struct NonInteractiveGuard110
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard110()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard110()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard110
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard110(ApplicationWindow *w = nullptr) : win(w)
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
    ~AppWinGuard110()
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
    AppWinGuard110(const AppWinGuard110 &) = delete;
    AppWinGuard110 &operator=(const AppWinGuard110 &) = delete;
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

struct Rpc110
{
    PythonAutomationServer *srv = nullptr;
    int id = 11000;
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
            std::cerr << "[INFO] 110: " << method.toStdString() << " error: "
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
    QJsonObject layer0(const QString &graph)
    {
        const QJsonObject info =
                result(call(QStringLiteral("graph.get_info"), QJsonObject{ { "graph", graph } }));
        return info.value(QStringLiteral("layers")).toArray().first().toObject();
    }
    QJsonObject axis(const QString &graph, const char *key)
    {
        return layer0(graph)
                .value(QStringLiteral("axes"))
                .toObject()
                .value(QString::fromLatin1(key))
                .toObject();
    }
    int curveCount(const QString &graph)
    {
        return layer0(graph).value(QStringLiteral("curves")).toArray().size();
    }
};

struct Range110
{
    double lo = std::nan("");
    double hi = std::nan("");
    bool operator==(const Range110 &o) const
    {
        auto same = [](double a, double b) {
            return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
        };
        return same(lo, o.lo) && same(hi, o.hi);
    }
    std::string str() const { return std::to_string(lo) + ".." + std::to_string(hi); }
};

Range110 range110(Rpc110 &rpc, const QString &graph, const char *key)
{
    const QJsonObject a = rpc.axis(graph, key);
    Range110 r;
    if (a.value(QStringLiteral("min")).isDouble())
        r.lo = a.value(QStringLiteral("min")).toDouble();
    if (a.value(QStringLiteral("max")).isDouble())
        r.hi = a.value(QStringLiteral("max")).toDouble();
    return r;
}

int countWindowsWithPrefix110(ApplicationWindow *app, const QString &prefix)
{
    int n = 0;
    for (MyWidget *w : app->windowsList())
        if (w && w->name().startsWith(prefix))
            ++n;
    return n;
}

Graph *layer110(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            if (auto *ml = qobject_cast<MultiLayer *>(w))
                return ml->layersList().value(0);
    return nullptr;
}

Table *findTable110(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            return qobject_cast<Table *>(w);
    return nullptr;
}

double olsSlope110(const QVector<double> &x, const QVector<double> &y)
{
    const int n = x.size();
    double mx = 0, my = 0;
    for (int i = 0; i < n; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= n;
    my /= n;
    double sxy = 0, sxx = 0;
    for (int i = 0; i < n; ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
    }
    return sxy / sxx;
}

bool nearRel110(double a, double b, double rel)
{
    return std::isfinite(a) && std::fabs(a - b) <= rel * std::max(1e-300, std::fabs(b));
}

QJsonArray toJson110(const QVector<double> &v)
{
    QJsonArray a;
    for (double d : v)
        a.append(d);
    return a;
}

} // namespace

int runSuite110CurveMathKeepsGraphAndColumns(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 110: integrate / subtract_line / fit_linear(x_col) keep the "
                 "graph and the worksheet as they were...\n";
    g110Failures = 0;
    NonInteractiveGuard110 niGuard;

    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "110: temp dir creation failed");
    const QString prj = tmp.filePath(QStringLiteral("curvemath_110.sciprj"));

    const QString kTemp = QStringLiteral("温度"); // 温度
    const QString kSig = QStringLiteral("信号"); // 信号

    Range110 aX, aY, dY, dR, fLeft;
    int fCurves = 0, fBands = 0;
    QString bTitleX, bTitleY;
    QMap<QString, int> gCurves110f;
    QMap<QString, Range110> gLeft110f;

    {
        AppWinGuard110 w(new ApplicationWindow());
        Rpc110 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "110: pythonAutomationServer null");

        // ---- 110a: integrate without plotting ------------------------------------------------
        {
            QVector<double> xs, ys, y2s;
            double trap = 0.0;
            for (int i = 0; i <= 200; ++i) {
                const double x = 10.0 * i / 200.0;
                xs << x;
                ys << 500.0 + 100.0 * std::exp(-(x - 5.0) * (x - 5.0));
                y2s << 10.0 + x;
                if (i > 0)
                    trap += 0.5 * (ys[i] + ys[i - 1]) * (xs[i] - xs[i - 1]);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T110A"), { "x", "y", "y2" },
                                     { toJson110(xs), toJson110(ys), toJson110(y2s) }),
                       "110a: table T110A");
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.xy"),
                                           QJsonObject{ { "table", "T110A" },
                                                        { "x_col", "x" },
                                                        { "y_cols", QJsonArray{ "y" } },
                                                        { "style", "line" },
                                                        { "graph_name", "G110A" } })),
                       "110a: plot.xy G110A");
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.double_y"),
                                           QJsonObject{ { "table", "T110A" },
                                                        { "x_col", "x" },
                                                        { "y_left_col", "y" },
                                                        { "y_right_col", "y2" },
                                                        { "graph_name", "G110D" } })),
                       "110a: plot.double_y G110D");
            const QString gA = QStringLiteral("G110A");
            const QString gD = QStringLiteral("G110D");
            aX = range110(rpc, gA, "x");
            aY = range110(rpc, gA, "y");
            dY = range110(rpc, gD, "y");
            dR = range110(rpc, gD, "right");
            std::cout << "[INFO] 110a: G110A x " << aX.str() << " y " << aY.str() << ", G110D y "
                      << dY.str() << " right " << dR.str() << "\n";
            const int nA = rpc.curveCount(gA);
            const int nD = rpc.curveCount(gD);
            const int integWin = countWindowsWithPrefix110(w, QStringLiteral("Integration"));
            const int nWin = w->windowsList().size();

            struct Call
            {
                const char *label;
                QString graph;
                QJsonObject extra;
            };
            const QList<Call> calls = {
                { "default", gA, QJsonObject{} },
                { "x range", gA, QJsonObject{ { "x_min", 2.0 }, { "x_max", 8.0 } } },
                { "cubic", gA, QJsonObject{ { "method", "cubic" } } },
                { "double-Y left", gD, QJsonObject{ { "curve", 0 } } },
                { "double-Y right", gD, QJsonObject{ { "curve", 1 } } },
            };
            for (const Call &c : calls) {
                QJsonObject p = c.extra;
                p.insert(QStringLiteral("graph"), c.graph);
                const QJsonObject r = rpc.call(QStringLiteral("curvemath.integrate"), p);
                CHECK110(Rpc110::ok(r), "110a: integrate " << c.label << " failed");
                const double area = Rpc110::result(r).value(QStringLiteral("area")).toDouble();
                if (QLatin1String(c.label) == QLatin1String("default")) {
                    CHECK110(nearRel110(area, trap, 1e-9),
                             "110a: integrate area " << area << " != trapezoid " << trap);
                }
                CHECK110(!Rpc110::result(r).contains(QStringLiteral("output_curve")),
                         "110a: integrate " << c.label << " reported an output curve");
                const Range110 x1 = range110(rpc, gA, "x");
                const Range110 y1 = range110(rpc, gA, "y");
                const Range110 dy1 = range110(rpc, gD, "y");
                const Range110 dr1 = range110(rpc, gD, "right");
                CHECK110(x1 == aX && y1 == aY,
                         "110a: integrate " << c.label << " changed G110A axes: x " << aX.str()
                                            << " -> " << x1.str() << ", y " << aY.str() << " -> "
                                            << y1.str());
                CHECK110(dy1 == dY && dr1 == dR,
                         "110a: integrate " << c.label << " changed G110D axes: y " << dY.str()
                                            << " -> " << dy1.str() << ", right " << dR.str()
                                            << " -> " << dr1.str());
                CHECK110(rpc.curveCount(gA) == nA && rpc.curveCount(gD) == nD,
                         "110a: integrate " << c.label << " changed the curve count");
                CHECK110(w->windowsList().size() == nWin
                                 && countWindowsWithPrefix110(w, QStringLiteral("Integration"))
                                         == integWin,
                         "110a: integrate " << c.label << " left a window behind ("
                                            << nWin << " -> " << w->windowsList().size() << ")");
            }

            // plot_curve=true still draws the integral curve (on its own graph).
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.xy"),
                                           QJsonObject{ { "table", "T110A" },
                                                        { "x_col", "x" },
                                                        { "y_cols", QJsonArray{ "y" } },
                                                        { "style", "line" },
                                                        { "graph_name", "G110P" } })),
                       "110a: plot.xy G110P");
            const QJsonObject rp = rpc.call(QStringLiteral("curvemath.integrate"),
                                            QJsonObject{ { "graph", "G110P" },
                                                         { "plot_curve", true } });
            CHECK110(Rpc110::ok(rp)
                             && !Rpc110::result(rp).value(QStringLiteral("output_curve"))
                                         .toString()
                                         .isEmpty()
                             && rpc.curveCount(QStringLiteral("G110P")) == 2,
                     "110a: integrate plot_curve=true did not add the integral curve ("
                             << rpc.curveCount(QStringLiteral("G110P")) << " curves)");
        }

        // ---- 110d: the integral of a right-axis curve goes on the right axis -----------------
        {
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.double_y"),
                                           QJsonObject{ { "table", "T110A" },
                                                        { "x_col", "x" },
                                                        { "y_left_col", "y" },
                                                        { "y_right_col", "y2" },
                                                        { "graph_name", "G110R" } })),
                       "110d: plot.double_y G110R");
            const QString gR = QStringLiteral("G110R");
            const Range110 ly0 = range110(rpc, gR, "y");
            Graph *g = layer110(w, gR);
            CHECK_COND(g && g->curves() == 2 && g->curve(1)
                               && g->curve(1)->yAxis() == QwtPlot::yRight,
                       "110d: G110R curve 1 is not on the right axis");
            const QJsonObject r = rpc.call(QStringLiteral("curvemath.integrate"),
                                           QJsonObject{ { "graph", gR },
                                                        { "curve", 1 },
                                                        { "plot_curve", true } });
            CHECK110(Rpc110::ok(r) && g->curves() == 3,
                     "110d: integrate plot_curve=true on the right curve did not add a curve");
            QwtPlotCurve *ic = g->curves() == 3 ? g->curve(2) : nullptr;
            CHECK110(ic && ic->yAxis() == QwtPlot::yRight && ic->xAxis() == QwtPlot::xBottom,
                     "110d: the integral of the right-axis curve is on axis "
                             << (ic ? ic->yAxis() : -1) << " (want yRight "
                             << int(QwtPlot::yRight) << ")");
            const Range110 ly1 = range110(rpc, gR, "y");
            std::cout << "[INFO] 110d: G110R left y " << ly0.str() << " -> " << ly1.str()
                      << ", right " << range110(rpc, gR, "right").str() << "\n";
            CHECK110(ly1 == ly0,
                     "110d: integrating the right curve changed the left axis " << ly0.str()
                                                                               << " -> "
                                                                               << ly1.str());
            // 左軸の曲線の積分は左軸のまま。
            const QJsonObject rl = rpc.call(QStringLiteral("curvemath.integrate"),
                                            QJsonObject{ { "graph", gR },
                                                         { "curve", 0 },
                                                         { "plot_curve", true } });
            QwtPlotCurve *il = g->curves() == 4 ? g->curve(3) : nullptr;
            CHECK110(Rpc110::ok(rl) && il && il->yAxis() == QwtPlot::yLeft,
                     "110d: the integral of the left-axis curve is not on the left axis");
        }

        // ---- 110e: fit / band / peak curves of a right-axis curve go on the right axis -----------
        {
            QVector<double> xs, yl, yr;
            for (int i = 0; i <= 300; ++i) {
                const double x = 10.0 * i / 300.0;
                xs << x;
                yl << 500.0 + 20.0 * std::sin(x);
                // two peaks plus a little deterministic ripple, so the fit residual is not zero
                yr << 10.0 + 40.0 * std::exp(-std::pow(x - 3.5, 2) / (2.0 * 0.4 * 0.4))
                                + 25.0 * std::exp(-std::pow(x - 6.5, 2) / (2.0 * 0.5 * 0.5))
                                + 0.3 * std::sin(7.0 * x);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T110F"), { "x", "yl", "yr" },
                                     { toJson110(xs), toJson110(yl), toJson110(yr) }),
                       "110e: table T110F");
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.double_y"),
                                           QJsonObject{ { "table", "T110F" },
                                                        { "x_col", "x" },
                                                        { "y_left_col", "yl" },
                                                        { "y_right_col", "yr" },
                                                        { "graph_name", "G110F" } })),
                       "110e: plot.double_y G110F");
            const QString gF = QStringLiteral("G110F");
            Graph *g = layer110(w, gF);
            CHECK_COND(g && g->curves() == 2 && g->curve(1)
                               && g->curve(1)->yAxis() == QwtPlot::yRight,
                       "110e: G110F curve 1 is not on the right axis");
            const Range110 ly0 = range110(rpc, gF, "y");
            auto checkAddedOnRight = [&](int from, const char *what) {
                for (int i = from; i < g->curves(); ++i) {
                    QwtPlotCurve *c = g->curve(i);
                    CHECK110(c && c->yAxis() == QwtPlot::yRight && c->xAxis() == QwtPlot::xBottom,
                             "110e: " << what << ": curve " << i << " '"
                                      << (c ? c->title().text().toStdString() : std::string())
                                      << "' is on axes " << (c ? c->xAxis() : -1) << "/"
                                      << (c ? c->yAxis() : -1) << " (want xBottom/yRight)");
                }
                int bands = 0;
                for (QwtPlotItem *it : g->plotWidget()->itemList()) {
                    if (it->rtti() != kShadedBandRtti110)
                        continue;
                    ++bands;
                    CHECK110(it->yAxis() == QwtPlot::yRight && it->xAxis() == QwtPlot::xBottom,
                             "110e: " << what << ": shaded band '"
                                      << it->title().text().toStdString() << "' is on axes "
                                      << it->xAxis() << "/" << it->yAxis()
                                      << " (want xBottom/yRight)");
                }
                fBands = bands;
                const Range110 ly = range110(rpc, gF, "y");
                std::cout << "[INFO] 110e: after " << what << " (" << bands << " shaded bands)" << ": " << g->curves()
                          << " curves, left y " << ly0.str() << " -> " << ly.str() << ", right "
                          << range110(rpc, gF, "right").str() << "\n";
                CHECK110(ly == ly0, "110e: " << what << " changed the left axis " << ly0.str()
                                             << " -> " << ly.str());
            };

            const int before = g->curves();
            const QJsonObject rf = rpc.call(
                    QStringLiteral("analysis.fit_nonlinear"),
                    QJsonObject{ { "graph", gF },
                                 { "curve", 1 },
                                 { "formula", "y0+A*exp(-(x-xc)^2/(2*w^2))" },
                                 { "param_names", QJsonArray{ "y0", "A", "xc", "w" } },
                                 { "init_params",
                                   QJsonObject{ { "y0", 10.0 }, { "A", 35.0 }, { "xc", 3.4 }, { "w", 0.5 } } },
                                 { "x_min", 2.0 },
                                 { "x_max", 5.0 },
                                 { "conf_band", true },
                                 { "pred_band", true } });
            // the fit curve and two curves for each band
            CHECK110(Rpc110::ok(rf) && g->curves() >= before + 5,
                     "110e: fit_nonlinear with bands added " << (g->curves() - before)
                                                             << " curves (want >= 5)");
            checkAddedOnRight(before, "fit_nonlinear + bands");
            CHECK110(fBands == 2, "110e: fit_nonlinear made " << fBands
                                                          << " shaded bands (want 2)");

            const int beforePk = g->curves();
            const QJsonObject rp = rpc.call(QStringLiteral("analysis.analyze_peaks"),
                                            QJsonObject{ { "graph", gF },
                                                         { "curve", 1 },
                                                         { "profile", "Gaussian" },
                                                         { "num_peaks", 2 },
                                                         { "baseline", "constant" },
                                                         { "plot_components", true } });
            // the sum curve and one curve per peak
            CHECK110(Rpc110::ok(rp) && g->curves() >= beforePk + 3,
                     "110e: analyze_peaks added " << (g->curves() - beforePk)
                                                  << " curves (want >= 3)");
            checkAddedOnRight(beforePk, "analyze_peaks");
            fLeft = ly0;
            fCurves = g->curves();
        }

        // ---- 110f: curvemath / signal results of a right-axis curve go on the right axis ------
        {
            QVector<double> xs, yl, yr, yr2;
            for (int i = 0; i <= 50; ++i) {
                const double x = 10.0 * i / 50.0;
                xs << x;
                yl << 0.01 * x;
                yr << 1000.0 + 100.0 * x + 5.0 * std::sin(3.0 * x);
                yr2 << 1500.0 + 50.0 * std::sin(x);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T110G"), { "x", "L", "R", "R2" },
                                     { toJson110(xs), toJson110(yl), toJson110(yr),
                                       toJson110(yr2) }),
                       "110f: table T110G");
            struct OpCase
            {
                const char *graph;
                const char *method;
                QJsonObject params;
            };
            const QList<OpCase> cases = {
                { "G110Fnorm", "curvemath.normalize", QJsonObject{ { "curve", 1 }, { "mode", "minmax" } } },
                { "G110Fsubl", "curvemath.subtract_line",
                  QJsonObject{ { "curve", 1 }, { "x1", 0.0 }, { "y1", 1000.0 }, { "x2", 10.0 }, { "y2", 2000.0 } } },
                { "G110Feval", "curvemath.eval",
                  QJsonObject{ { "curve1", 2 }, { "curve2", 1 }, { "op", "sub" } } },
                { "G110Favg", "curvemath.average_curves",
                  QJsonObject{ { "curves", QJsonArray{ 1, 2 } }, { "sd_band", true } } },
                { "G110Fgauss", "signal.smooth",
                  QJsonObject{ { "curve", 1 }, { "method", "gaussian" }, { "window", 5 } } },
                { "G110Fiir", "signal.iir_filter",
                  QJsonObject{ { "curve", 1 }, { "filter_type", "lowpass" }, { "cutoff", 1.0 } } },
                { "G110Fenv", "signal.envelope", QJsonObject{ { "curve", 1 }, { "plot_lower", true } } },
                { "G110Fpenv", "signal.envelope", QJsonObject{ { "curve", 1 }, { "method", "peak" } } },
                { "G110Fdec", "signal.decimate", QJsonObject{ { "curve", 1 }, { "factor", 2 } } },
                { "G110Fsgd", "signal.savgol_derivative", QJsonObject{ { "curve", 1 }, { "window", 7 } } },
                { "G110Fwav", "signal.wavelet_denoise", QJsonObject{ { "curve", 1 } } },
                { "G110Ffsd", "signal.fsd", QJsonObject{ { "curve", 1 }, { "fwhm", 1.0 } } },
            };
            for (const OpCase &oc : cases) {
                const QString gn = QString::fromLatin1(oc.graph);
                CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.double_y"),
                                               QJsonObject{ { "table", "T110G" },
                                                            { "x_col", "x" },
                                                            { "y_left_col", "L" },
                                                            { "y_right_col", "R" },
                                                            { "graph_name", gn } }))
                                   && Rpc110::ok(rpc.call(QStringLiteral("plot.add_curve"),
                                                          QJsonObject{ { "graph", gn },
                                                                       { "table", "T110G" },
                                                                       { "x_col", "x" },
                                                                       { "y_col", "R2" },
                                                                       { "right_axis", true } })),
                           "110f: graph " << oc.graph);
                Graph *g = layer110(w, gn);
                CHECK_COND(g && g->curves() == 3 && g->curve(1) && g->curve(2)
                                   && g->curve(1)->yAxis() == QwtPlot::yRight
                                   && g->curve(2)->yAxis() == QwtPlot::yRight,
                           "110f: " << oc.graph << ": curves 1 and 2 are not on the right axis");
                const Range110 ly0 = range110(rpc, gn, "y");
                const QString lt0 = rpc.axis(gn, "y").value(QStringLiteral("title")).toString();
                QJsonObject p = oc.params;
                p.insert(QStringLiteral("graph"), gn);
                const QJsonObject r = rpc.call(QString::fromLatin1(oc.method), p);
                CHECK110(Rpc110::ok(r) && g->curves() > 3,
                         "110f: " << oc.method << " on " << oc.graph << " added "
                                  << (g->curves() - 3) << " curves");
                for (int i = 3; i < g->curves(); ++i) {
                    QwtPlotCurve *c = g->curve(i);
                    CHECK110(c && c->yAxis() == QwtPlot::yRight && c->xAxis() == QwtPlot::xBottom,
                             "110f: " << oc.method << " on " << oc.graph << ": result curve '"
                                      << (c ? c->title().text().toStdString() : std::string())
                                      << "' is on axes " << (c ? c->xAxis() : -1) << "/"
                                      << (c ? c->yAxis() : -1) << " (want xBottom/yRight)");
                }
                const Range110 ly = range110(rpc, gn, "y");
                const QString lt = rpc.axis(gn, "y").value(QStringLiteral("title")).toString();
                std::cout << "[INFO] 110f: " << oc.method << " on " << oc.graph << ": "
                          << g->curves() << " curves, left y " << ly0.str() << " -> " << ly.str()
                          << ", right " << range110(rpc, gn, "right").str() << "\n";
                CHECK110(ly == ly0, "110f: " << oc.method << " on " << oc.graph
                                             << " changed the left axis " << ly0.str() << " -> "
                                             << ly.str());
                CHECK110(lt == lt0, "110f: " << oc.method << " on " << oc.graph
                                             << " changed the left axis title '"
                                             << lt0.toStdString() << "' -> '" << lt.toStdString()
                                             << "'");
                gCurves110f.insert(gn, g->curves());
                gLeft110f.insert(gn, ly0);
            }
            // 左軸の曲線の結果は左軸のまま。
            const QString gn = QStringLiteral("G110Fnorm");
            Graph *g = layer110(w, gn);
            const int before = g ? g->curves() : 0;
            const QJsonObject r = rpc.call(QStringLiteral("curvemath.normalize"),
                                           QJsonObject{ { "graph", gn }, { "curve", 0 } });
            QwtPlotCurve *c = (g && g->curves() == before + 1) ? g->curve(before) : nullptr;
            CHECK110(Rpc110::ok(r) && c && c->yAxis() == QwtPlot::yLeft,
                     "110f: normalize of the left-axis curve is not on the left axis");
            if (g) {
                gCurves110f.insert(gn, g->curves());
                gLeft110f.remove(gn);
            }
        }

        // ---- 110b: subtract_line keeps the automatic axis titles -----------------------------
        {
            QVector<double> xs, ys;
            for (int i = 0; i <= 400; ++i) {
                const double x = 300.0 + 200.0 * i / 400.0;
                xs << x;
                ys << 1e-11 * std::exp(-std::pow((x - 400.0) / 20.0, 2)) + 1e-13;
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T110B"), { kTemp, kSig },
                                     { toJson110(xs), toJson110(ys) }),
                       "110b: table T110B");
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("table.set_column_meta"),
                                           QJsonObject{ { "table", "T110B" },
                                                        { "col", kTemp },
                                                        { "units", "K" },
                                                        { "plot_designation", "X" } }))
                               && Rpc110::ok(rpc.call(QStringLiteral("table.set_column_meta"),
                                                      QJsonObject{ { "table", "T110B" },
                                                                   { "col", kSig },
                                                                   { "units", "A" } })),
                       "110b: set_column_meta");
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.xy"),
                                           QJsonObject{ { "table", "T110B" },
                                                        { "x_col", kTemp },
                                                        { "y_cols", QJsonArray{ kSig } },
                                                        { "style", "line" },
                                                        { "graph_name", "G110B" } })),
                       "110b: plot.xy G110B");
            const QString gB = QStringLiteral("G110B");
            bTitleX = rpc.axis(gB, "x").value(QStringLiteral("title")).toString();
            bTitleY = rpc.axis(gB, "y").value(QStringLiteral("title")).toString();
            std::cout << "[INFO] 110b: titles before '" << bTitleX.toStdString() << "', '"
                      << bTitleY.toStdString() << "'\n";
            CHECK110(bTitleX.contains(kTemp) && bTitleX.contains(QLatin1Char('K'))
                             && bTitleY.contains(kSig),
                     "110b: unexpected automatic titles '" << bTitleX.toStdString() << "', '"
                                                           << bTitleY.toStdString() << "'");
            const QJsonObject r = rpc.call(QStringLiteral("curvemath.subtract_line"),
                                           QJsonObject{ { "graph", gB },
                                                        { "curve", 0 },
                                                        { "x1", 300.0 },
                                                        { "y1", 1e-13 },
                                                        { "x2", 500.0 },
                                                        { "y2", 1e-13 } });
            CHECK110(Rpc110::ok(r) && rpc.curveCount(gB) == 2,
                     "110b: subtract_line did not add the result curve");
            const QString tx = rpc.axis(gB, "x").value(QStringLiteral("title")).toString();
            const QString ty = rpc.axis(gB, "y").value(QStringLiteral("title")).toString();
            CHECK110(tx == bTitleX && ty == bTitleY,
                     "110b: subtract_line changed the axis titles '"
                             << bTitleX.toStdString() << "' -> '" << tx.toStdString() << "', '"
                             << bTitleY.toStdString() << "' -> '" << ty.toStdString() << "'");
            Graph *g = nullptr;
            for (MyWidget *mwid : w->windowsList())
                if (mwid && mwid->name() == gB)
                    if (auto *ml = qobject_cast<MultiLayer *>(mwid))
                        g = ml->layersList().value(0);
            const QString leg = (g && g->legend()) ? g->legend()->text() : QString();
            CHECK110(leg.contains(QStringLiteral("SubtractedY")),
                     "110b: legend no longer names the result curve: '" << leg.toStdString()
                                                                         << "'");
        }

        // ---- 110c: fit_linear(table, x_col, y_col) uses x_col --------------------------------
        {
            QVector<double> u, y, x2;
            for (int i = 1; i <= 7; ++i) {
                u << double(i);
                y << 4.05 + 0.002 * std::cos(double(i));
                x2 << 1.0 / double(i);
            }
            const double slopeX2 = olsSlope110(x2, y);
            const double slopeU = olsSlope110(u, y);
            std::cout << "[INFO] 110c: slope vs x2 " << slopeX2 << ", vs u " << slopeU << "\n";

            // case 1: x_col to the right of y_col, nothing plotted yet
            CHECK_COND(rpc.makeTable(QStringLiteral("T110C"), { "u", "y", "x2" },
                                     { toJson110(u), toJson110(y), toJson110(x2) }),
                       "110c: table T110C");
            Table *tc = findTable110(w, QStringLiteral("T110C"));
            CHECK_COND(tc, "110c: T110C missing");
            const int desX2Before = tc->colPlotDesignation(2);
            const QJsonObject r1 = rpc.call(QStringLiteral("analysis.fit_linear"),
                                            QJsonObject{ { "table", "T110C" },
                                                         { "x_col", "x2" },
                                                         { "y_col", "y" },
                                                         { "plot_curve", false } });
            const double s1 = Rpc110::result(r1).value(QStringLiteral("slope")).toDouble();
            CHECK110(Rpc110::ok(r1) && nearRel110(s1, slopeX2, 1e-6),
                     "110c: fit_linear(T110C, x2, y) slope " << s1 << " (want " << slopeX2
                                                             << ")");
            CHECK110(tc->colPlotDesignation(2) == desX2Before,
                     "110c: x2 plot designation changed " << desX2Before << " -> "
                                                          << tc->colPlotDesignation(2));
            const QString g1 = Rpc110::result(r1).value(QStringLiteral("graph")).toString();
            const QJsonArray c1 =
                    rpc.layer0(g1).value(QStringLiteral("curves")).toArray();
            CHECK110(c1.size() == 1
                             && c1.first().toObject().value(QStringLiteral("x_column")).toString()
                                     == QLatin1String("T110C_x2"),
                     "110c: fit_linear plotted y against '"
                             << (c1.isEmpty() ? std::string("<none>")
                                              : c1.first()
                                                        .toObject()
                                                        .value(QStringLiteral("x_column"))
                                                        .toString()
                                                        .toStdString())
                             << "'");

            // case 2: y already plotted against u
            CHECK_COND(rpc.makeTable(QStringLiteral("T110Q"), { "u", "x2", "y" },
                                     { toJson110(u), toJson110(x2), toJson110(y) }),
                       "110c: table T110Q");
            Table *tq = findTable110(w, QStringLiteral("T110Q"));
            CHECK_COND(tq, "110c: T110Q missing");
            CHECK_COND(Rpc110::ok(rpc.call(QStringLiteral("plot.xy"),
                                           QJsonObject{ { "table", "T110Q" },
                                                        { "x_col", "u" },
                                                        { "y_cols", QJsonArray{ "y" } },
                                                        { "graph_name", "G110Q" } })),
                       "110c: plot.xy G110Q");
            const int desQ = tq->colPlotDesignation(1);
            const QJsonObject r2 = rpc.call(QStringLiteral("analysis.fit_linear"),
                                            QJsonObject{ { "table", "T110Q" },
                                                         { "x_col", "x2" },
                                                         { "y_col", "y" },
                                                         { "plot_curve", false } });
            const double s2 = Rpc110::result(r2).value(QStringLiteral("slope")).toDouble();
            CHECK110(Rpc110::ok(r2) && nearRel110(s2, slopeX2, 1e-6),
                     "110c: fit_linear(T110Q, x2, y) with y plotted vs u: slope "
                             << s2 << " (want " << slopeX2 << ")");
            CHECK110(tq->colPlotDesignation(1) == desQ,
                     "110c: T110Q x2 plot designation changed " << desQ << " -> "
                                                                << tq->colPlotDesignation(1));
            CHECK110(Rpc110::result(r2).value(QStringLiteral("graph")).toString()
                             != QLatin1String("G110Q"),
                     "110c: fit_linear(x2) reused G110Q, which plots y against u");

            // the curve plotted against u is still reused for x_col=u, and without x_col
            for (const bool withX : { true, false }) {
                QJsonObject p{ { "table", "T110Q" }, { "y_col", "y" }, { "plot_curve", false } };
                if (withX)
                    p.insert(QStringLiteral("x_col"), QStringLiteral("u"));
                const QJsonObject r3 = rpc.call(QStringLiteral("analysis.fit_linear"), p);
                const double s3 = Rpc110::result(r3).value(QStringLiteral("slope")).toDouble();
                CHECK110(Rpc110::ok(r3) && nearRel110(s3, slopeU, 1e-6)
                                 && Rpc110::result(r3).value(QStringLiteral("graph")).toString()
                                         == QLatin1String("G110Q"),
                         "110c: fit_linear(T110Q, " << (withX ? "u" : "<no x_col>")
                                                    << ", y) slope " << s3 << " graph '"
                                                    << Rpc110::result(r3)
                                                               .value(QStringLiteral("graph"))
                                                               .toString()
                                                               .toStdString()
                                                    << "' (want " << slopeU << " on G110Q)");
            }
        }

        CHECK_COND(w->saveFolder(w->projectFolder(), prj), "110: saving the project failed");
    }

    // ---- reopen ------------------------------------------------------------------------------
    {
        AppWinGuard110 rd(new ApplicationWindow());
        CHECK_COND(rd->loadProject(prj), "110: reopening the project failed");
        QCoreApplication::processEvents();
        Rpc110 rpc;
        rpc.srv = rd->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "110: reopened pythonAutomationServer null");

        const Range110 x1 = range110(rpc, QStringLiteral("G110A"), "x");
        const Range110 y1 = range110(rpc, QStringLiteral("G110A"), "y");
        const Range110 dy1 = range110(rpc, QStringLiteral("G110D"), "y");
        const Range110 dr1 = range110(rpc, QStringLiteral("G110D"), "right");
        CHECK110(x1 == aX && y1 == aY,
                 "110a-reopen: G110A axes x " << aX.str() << " -> " << x1.str() << ", y "
                                              << aY.str() << " -> " << y1.str());
        CHECK110(dy1 == dY && dr1 == dR,
                 "110a-reopen: G110D axes y " << dY.str() << " -> " << dy1.str() << ", right "
                                              << dR.str() << " -> " << dr1.str());

        Graph *gr = layer110(rd, QStringLiteral("G110R"));
        CHECK110(gr && gr->curves() >= 3 && gr->curve(2)
                         && gr->curve(2)->yAxis() == QwtPlot::yRight,
                 "110d-reopen: the integral of the right-axis curve left the right axis");

        Graph *gf = layer110(rd, QStringLiteral("G110F"));
        CHECK110(gf && gf->curves() == fCurves,
                 "110e-reopen: G110F has " << (gf ? gf->curves() : -1) << " curves (want "
                                           << fCurves << ")");
        for (int i = 1; gf && i < gf->curves(); ++i) {
            QwtPlotCurve *c = gf->curve(i);
            CHECK110(c && c->yAxis() == QwtPlot::yRight,
                     "110e-reopen: curve " << i << " '"
                                           << (c ? c->title().text().toStdString() : std::string())
                                           << "' left the right axis");
        }
        int reBands = 0;
        for (QwtPlotItem *it : gf ? gf->plotWidget()->itemList() : QwtPlotItemList()) {
            if (it->rtti() != kShadedBandRtti110)
                continue;
            ++reBands;
            CHECK110(it->yAxis() == QwtPlot::yRight,
                     "110e-reopen: shaded band '" << it->title().text().toStdString()
                                                  << "' left the right axis");
        }
        CHECK110(reBands == fBands,
                 "110e-reopen: " << reBands << " shaded bands (want " << fBands << ")");
        const Range110 fl1 = range110(rpc, QStringLiteral("G110F"), "y");
        CHECK110(fl1 == fLeft,
                 "110e-reopen: G110F left axis " << fLeft.str() << " -> " << fl1.str());

        for (auto it = gCurves110f.constBegin(); it != gCurves110f.constEnd(); ++it) {
            Graph *gg = layer110(rd, it.key());
            CHECK110(gg && gg->curves() == it.value(),
                     "110f-reopen: " << it.key().toStdString() << " has "
                                     << (gg ? gg->curves() : -1) << " curves (want " << it.value()
                                     << ")");
            // G110Fnorm の最後の曲線は左軸の曲線の正規化（左軸のまま）。
            const bool lastOnLeft = it.key() == QLatin1String("G110Fnorm");
            for (int i = 1; gg && i < gg->curves(); ++i) {
                QwtPlotCurve *c = gg->curve(i);
                const int want = (lastOnLeft && i == gg->curves() - 1) ? QwtPlot::yLeft
                                                                       : QwtPlot::yRight;
                CHECK110(c && c->yAxis() == want && c->xAxis() == QwtPlot::xBottom,
                         "110f-reopen: " << it.key().toStdString() << " curve " << i << " '"
                                         << (c ? c->title().text().toStdString() : std::string())
                                         << "' is on axes " << (c ? c->xAxis() : -1) << "/"
                                         << (c ? c->yAxis() : -1) << " (want y axis " << want
                                         << ")");
            }
            if (gLeft110f.contains(it.key())) {
                const Range110 l1 = range110(rpc, it.key(), "y");
                CHECK110(l1 == gLeft110f.value(it.key()),
                         "110f-reopen: " << it.key().toStdString() << " left axis "
                                         << gLeft110f.value(it.key()).str() << " -> "
                                         << l1.str());
            }
        }

        const QString tx =
                rpc.axis(QStringLiteral("G110B"), "x").value(QStringLiteral("title")).toString();
        const QString ty =
                rpc.axis(QStringLiteral("G110B"), "y").value(QStringLiteral("title")).toString();
        CHECK110(tx == bTitleX && ty == bTitleY,
                 "110b-reopen: axis titles '" << bTitleX.toStdString() << "' -> '"
                                              << tx.toStdString() << "', '"
                                              << bTitleY.toStdString() << "' -> '"
                                              << ty.toStdString() << "'");
    }

    if (g110Failures > 0) {
        std::cerr << "[FAIL] Suite 110: " << g110Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 110: curve maths and table-driven analysis keep the graph and the "
                 "worksheet as they were\n";
    return 0;
}
