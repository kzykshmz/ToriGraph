#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "Graph3D.h"

#include <QImage>
#include <iomanip>
#include <QJsonArray>
#include <QJsonObject>

// =============================================================================
// Suite 97: plot data fidelity of graphs made through JSON-RPC, at creation, on export and
// across save -> reopen.
//   97a a curve label (graph.set_curve_style label=, plot.xy label=, plot.add_curve label=)
//       only changes the legend text: the curve title (its Y column binding) stays the table
//       column, so the curve survives save -> reopen and still points at the column
//   97b plot.color_scatter draws points only (NoCurve) right after creation, as after reopen,
//       its legend is inside the layer, and the X range is the same after reopen
//   97c horizontal bars: no placeholder title on the right axis, a title on the bottom axis
//   97d graph.export of a 3D graph honours dpi (PNG size proportional to dpi)
//   97e plot.double_y: left axis title set at creation (same rule as the right axis), legend
//       inside the layer right after creation
//   97f 2-row layout: set_panel_labels keeps panel heights and axis ranges, and save -> reopen
//       keeps the ranges and major ticks
//   97g 3D x/y/z scale keeps full precision across save -> reopen
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g97Failures = 0;

#define CHECK97(cond, msg)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g97Failures;                                                                      \
        }                                                                                       \
    } while (0)

struct NonInteractiveGuard97
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard97()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard97()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard97
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard97(ApplicationWindow *w = nullptr, bool headless = false) : win(w)
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
    ~AppWinGuard97() { reset(); }
    AppWinGuard97(const AppWinGuard97 &) = delete;
    AppWinGuard97 &operator=(const AppWinGuard97 &) = delete;
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

MyWidget *findWindow97(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            return w;
    return nullptr;
}

Graph *firstLayer97(ApplicationWindow *app, const QString &name)
{
    auto *ml = qobject_cast<MultiLayer *>(findWindow97(app, name));
    if (!ml || ml->layersList().isEmpty())
        return nullptr;
    return ml->layersList().first();
}

// The legend rectangle (canvas pixels) lies inside the layer's plot widget.
bool legendInsideLayer97(Graph *g, QString *why)
{
    Legend *leg = g ? g->legend() : nullptr;
    if (!leg) {
        *why = QStringLiteral("no legend");
        return false;
    }
    Plot *p = g->plotWidget();
    const QRect lr = leg->rect();
    const QPoint cpos = p->canvas()->geometry().topLeft();
    const QRect plotInCanvas(-cpos.x(), -cpos.y(), p->width(), p->height());
    *why = QStringLiteral("legend (%1,%2 %3x%4) plot (%5,%6 %7x%8)")
                   .arg(lr.x())
                   .arg(lr.y())
                   .arg(lr.width())
                   .arg(lr.height())
                   .arg(plotInCanvas.x())
                   .arg(plotInCanvas.y())
                   .arg(plotInCanvas.width())
                   .arg(plotInCanvas.height());
    return lr.isValid() && plotInCanvas.contains(lr);
}

QString plainTitle97(Graph *g, int axis)
{
    QString t = g ? g->axisTitle(axis) : QString();
    t.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return t.trimmed();
}

struct AxisState97
{
    int canvasH = 0;
    double lo = 0.0, hi = 0.0;
    QList<double> majors;
};

AxisState97 axisState97(Graph *g, int axis)
{
    AxisState97 s;
    if (!g)
        return s;
    Plot *p = g->plotWidget();
    p->updateLayout();
    s.canvasH = p->plotLayout()->canvasRect().height();
    if (const QwtScaleDiv *sd = p->axisScaleDiv(axis)) {
        s.lo = sd->lowerBound();
        s.hi = sd->upperBound();
        for (double v : sd->ticks(QwtScaleDiv::MajorTick))
            s.majors << v;
    }
    return s;
}

std::string describe97(const AxisState97 &s)
{
    QStringList t;
    for (double v : s.majors)
        t << QString::number(v, 'g', 6);
    return QStringLiteral("h=%1 [%2, %3] ticks{%4}")
            .arg(s.canvasH)
            .arg(s.lo, 0, 'g', 8)
            .arg(s.hi, 0, 'g', 8)
            .arg(t.join(','))
            .toStdString();
}

bool sameRangeAndTicks97(const AxisState97 &a, const AxisState97 &b)
{
    auto approx = [](double x, double y) {
        return std::fabs(x - y) <= 1e-9 * std::max(1.0, std::max(std::fabs(x), std::fabs(y)));
    };
    if (!approx(a.lo, b.lo) || !approx(a.hi, b.hi) || a.majors.size() != b.majors.size())
        return false;
    for (int i = 0; i < a.majors.size(); ++i)
        if (!approx(a.majors[i], b.majors[i]))
            return false;
    return true;
}

} // namespace

int runSuite97PlotDataFidelity(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 97: plot data fidelity (labels, scatter, axes, export, "
                 "panels, 3D scale)...\n";
    g97Failures = 0;
    NonInteractiveGuard97 niGuard;

    QTemporaryDir tmp97;
    CHECK_COND(tmp97.isValid(), "97: temp dir creation failed");
    const QString prj = tmp97.filePath(QStringLiteral("fidelity97.sciprj"));

    const double sx = 1.0192977, sy = 0.98765432109876, sz = 1.2345678901234;

    // Values recorded in the source session and compared after reopening.
    QList<AxisState97> panelAfterLabels;
    double cscatXLo = 0.0, cscatXHi = 0.0;

    {
        AppWinGuard97 w(new ApplicationWindow());
        w->resize(1280, 800);
        w->show();
        QCoreApplication::processEvents();
        PythonAutomationServer *srv = w->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "97: pythonAutomationServer null");

        int rpcId = 9700;
        auto rpc = [&](const QString &method, const QJsonObject &params) -> QJsonObject {
            QJsonObject req;
            req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            req[QStringLiteral("id")] = ++rpcId;
            req[QStringLiteral("method")] = method;
            req[QStringLiteral("params")] = params;
            const QJsonObject resp = srv->handleRpcRequest(req);
            QCoreApplication::processEvents();
            if (resp.contains(QStringLiteral("error")))
                std::cerr << "[INFO] 97: " << method.toStdString() << " error: "
                          << QJsonDocument(resp.value(QStringLiteral("error")).toObject())
                                     .toJson(QJsonDocument::Compact)
                                     .toStdString()
                          << "\n";
            return resp;
        };
        auto ok = [](const QJsonObject &resp) { return !resp.contains(QStringLiteral("error")); };
        auto makeTable = [&](const QString &name, const QStringList &cols,
                             const QList<QJsonArray> &data) -> bool {
            const int rows = data.isEmpty() ? 0 : data.first().size();
            QJsonArray colNames;
            QJsonObject dict;
            for (int i = 0; i < cols.size(); ++i) {
                colNames.append(cols[i]);
                dict.insert(cols[i], data.value(i));
            }
            if (!ok(rpc(QStringLiteral("table.create"),
                        QJsonObject{ { "rows", rows },
                                     { "cols", cols.size() },
                                     { "name", name },
                                     { "col_names", colNames } })))
                return false;
            return ok(rpc(QStringLiteral("table.set_data_dict"),
                          QJsonObject{ { "table", name },
                                       { "data", dict },
                                       { "column_order", colNames } }));
        };

        QJsonArray xs, a2, b1, ex, sinx, cosx;
        for (int i = 0; i <= 20; ++i) {
            const double x = -2.0 + 0.2 * i;
            xs.append(x);
            a2.append(x * x);
            b1.append(-x);
            ex.append(std::exp(x));
        }
        CHECK_COND(makeTable(QStringLiteral("T97"), { "X", "A", "B", "C" }, { xs, a2, b1, ex }),
                   "97: creating table T97 failed");

        // ---- 97a: curve labels ---------------------------------------------------------------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A", "B" } },
                                        { "graph_name", "G97Lbl" } })),
                    "97a: plot.xy failed");
            CHECK97(ok(rpc(QStringLiteral("graph.set_curve_style"),
                           QJsonObject{ { "graph", "G97Lbl" },
                                        { "curve", 0 },
                                        { "label", "Sample A" } })),
                    "97a: graph.set_curve_style label failed");
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "graph_name", "G97Lbl1" },
                                        { "label", "LabelA" } })),
                    "97a: plot.xy label= failed");
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "graph_name", "G97Lbl2" } })),
                    "97a: plot.xy for add_curve failed");
            CHECK97(ok(rpc(QStringLiteral("plot.add_curve"),
                           QJsonObject{ { "graph", "G97Lbl2" },
                                        { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_col", "B" },
                                        { "label", "LabelB" } })),
                    "97a: plot.add_curve label= failed");

            Graph *g = firstLayer97(w, QStringLiteral("G97Lbl"));
            CHECK97(g && g->curves() == 2, "97a: G97Lbl should have 2 curves");
            if (g && g->curves() == 2) {
                CHECK97(g->curve(0)->title().text() == QLatin1String("T97_A"),
                        "97a: relabelled curve title changed to '"
                                << g->curve(0)->title().text().toStdString() << "'");
                CHECK97(g->legend() && g->legend()->text().contains(QStringLiteral("\\c{1}Sample A")),
                        "97a: legend does not show the label: "
                                << (g->legend() ? g->legend()->text().toStdString() : "<none>"));
            }
            Graph *g1 = firstLayer97(w, QStringLiteral("G97Lbl1"));
            CHECK97(g1 && g1->curves() == 1 && g1->curve(0)->title().text() == QLatin1String("T97_A"),
                    "97a: plot.xy label= changed the curve title");
            CHECK97(g1 && g1->legend() && g1->legend()->text().contains(QStringLiteral("LabelA")),
                    "97a: plot.xy label= missing from the legend");
            Graph *g2 = firstLayer97(w, QStringLiteral("G97Lbl2"));
            CHECK97(g2 && g2->curves() == 2 && g2->curve(1)->title().text() == QLatin1String("T97_B"),
                    "97a: plot.add_curve label= changed the curve title");
            CHECK97(g2 && g2->legend() && g2->legend()->text().contains(QStringLiteral("LabelB")),
                    "97a: plot.add_curve label= missing from the legend");
        }

        // ---- 97b: colour-mapped scatter ------------------------------------------------------
        {
            QJsonArray cx, cy, cz;
            for (int i = 0; i < 30; ++i) {
                cx.append(-2.7111624789659685 + 4.005226293364176 * double((i * 7) % 30) / 29.0);
                cy.append(std::sin(0.7 * i) * 2.0);
                cz.append(double((i * 11) % 30) / 29.0);
            }
            CHECK_COND(makeTable(QStringLiteral("T97S"), { "x", "y", "z" }, { cx, cy, cz }),
                       "97b: creating table T97S failed");
            CHECK97(ok(rpc(QStringLiteral("plot.color_scatter"),
                           QJsonObject{ { "table", "T97S" },
                                        { "x", "x" },
                                        { "y", "y" },
                                        { "z", "z" },
                                        { "graph_name", "G97CS" } })),
                    "97b: plot.color_scatter failed");
            Graph *g = firstLayer97(w, QStringLiteral("G97CS"));
            CHECK97(g && g->curves() >= 1, "97b: colour scatter has no curve");
            if (g && g->curves() >= 1) {
                CHECK97(g->curve(0)->style() == QwtPlotCurve::NoCurve,
                        "97b: colour scatter connects its points (style " << g->curve(0)->style()
                                                                         << ")");
                QString why;
                CHECK97(legendInsideLayer97(g, &why),
                        "97b: colour scatter legend outside the layer: " << why.toStdString());
                const AxisState97 xs0 = axisState97(g, QwtPlot::xBottom);
                cscatXLo = xs0.lo;
                cscatXHi = xs0.hi;
            }
            // Adding a colour-mapped scatter to an existing graph uses the same layout.
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97S" },
                                        { "x_col", "x" },
                                        { "y_cols", QJsonArray{ "y" } },
                                        { "style", "scatter" },
                                        { "graph_name", "G97CS2" } })),
                    "97b: plot.xy scatter failed");
            CHECK97(ok(rpc(QStringLiteral("plot.color_scatter"),
                           QJsonObject{ { "table", "T97S" },
                                        { "x", "x" },
                                        { "y", "y" },
                                        { "z", "z" },
                                        { "graph", "G97CS2" } })),
                    "97b: plot.color_scatter onto G97CS2 failed");
            Graph *g2 = firstLayer97(w, QStringLiteral("G97CS2"));
            CHECK97(g2 && g2->curves() >= 2
                            && g2->curve(g2->curves() - 1)->style() == QwtPlotCurve::NoCurve,
                    "97b: colour scatter added to a graph connects its points");
        }

        // ---- 97c: horizontal bars ------------------------------------------------------------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "style", "horizontal_bars" },
                                        { "graph_name", "G97HB" } })),
                    "97c: horizontal bars failed");
            Graph *g = firstLayer97(w, QStringLiteral("G97HB"));
            CHECK97(g != nullptr, "97c: horizontal bar graph missing");
            if (g) {
                CHECK97(plainTitle97(g, QwtPlot::yRight).isEmpty(),
                        "97c: right axis carries a title: '"
                                << plainTitle97(g, QwtPlot::yRight).toStdString() << "'");
                CHECK97(!plainTitle97(g, QwtPlot::xBottom).isEmpty(),
                        "97c: bottom axis has no title");
                CHECK97(!plainTitle97(g, QwtPlot::yLeft).isEmpty(), "97c: left axis has no title");
            }
        }

        // ---- 97d: 3D export dpi --------------------------------------------------------------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.3d_function"),
                           QJsonObject{ { "formula", "sin(x)*cos(y)" },
                                        { "x_min", -3 },
                                        { "x_max", 3 },
                                        { "y_min", -3 },
                                        { "y_max", 3 },
                                        { "graph_name", "S97F" } })),
                    "97d: plot.3d_function failed");
            QSize sz[2];
            const int dpis[2] = { 72, 150 };
            for (int k = 0; k < 2; ++k) {
                const QString png = tmp97.filePath(QStringLiteral("f3d_%1.png").arg(dpis[k]));
                const QJsonObject r = rpc(QStringLiteral("graph.export"),
                                          QJsonObject{ { "graph", "S97F" },
                                                       { "path", png },
                                                       { "dpi", dpis[k] } });
                CHECK97(ok(r), "97d: 3D export at " << dpis[k] << " dpi failed");
                sz[k] = QImage(png).size();
                const QJsonObject res = r.value(QStringLiteral("result")).toObject();
                CHECK97(res.value(QStringLiteral("width_px")).toInt() == sz[k].width(),
                        "97d: width_px " << res.value(QStringLiteral("width_px")).toInt()
                                         << " != PNG width " << sz[k].width());
            }
            std::cout << "[INFO] 97d: 3D PNG 72 dpi " << sz[0].width() << "x" << sz[0].height()
                      << ", 150 dpi " << sz[1].width() << "x" << sz[1].height() << "\n";
            const double ratio = sz[0].width() > 0 ? double(sz[1].width()) / sz[0].width() : 0.0;
            CHECK97(std::fabs(ratio - 150.0 / 72.0) < 0.06,
                    "97d: 3D PNG width ratio 150/72 dpi is " << ratio);
        }

        // ---- 97e: double Y -------------------------------------------------------------------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.double_y"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y1_col", "A" },
                                        { "y2_col", "C" },
                                        { "graph_name", "G97DY" } })),
                    "97e: plot.double_y failed");
            Graph *g = firstLayer97(w, QStringLiteral("G97DY"));
            CHECK97(g != nullptr, "97e: double-Y graph missing");
            if (g) {
                CHECK97(plainTitle97(g, QwtPlot::yLeft) == QLatin1String("A"),
                        "97e: left axis title right after creation is '"
                                << plainTitle97(g, QwtPlot::yLeft).toStdString() << "'");
                CHECK97(plainTitle97(g, QwtPlot::yRight) == QLatin1String("C"),
                        "97e: right axis title is '"
                                << plainTitle97(g, QwtPlot::yRight).toStdString() << "'");
                QString why;
                CHECK97(legendInsideLayer97(g, &why),
                        "97e: double-Y legend outside the layer: " << why.toStdString());
            }
        }

        // ---- 97f: 2-row layout + panel labels ------------------------------------------------
        {
            QJsonArray px, ps, pc;
            for (int i = 0; i <= 100; ++i) {
                const double x = 0.1 * i;
                px.append(x);
                ps.append(std::sin(x));
                pc.append(std::cos(x) + 2.0);
            }
            CHECK_COND(makeTable(QStringLiteral("T97P"), { "X", "A", "B" }, { px, ps, pc }),
                       "97f: creating table T97P failed");
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97P" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "graph_name", "G97M" } })),
                    "97f: plot.xy failed");
            CHECK97(ok(rpc(QStringLiteral("graph.add_layer"), QJsonObject{ { "graph", "G97M" } })),
                    "97f: add_layer failed");
            CHECK97(ok(rpc(QStringLiteral("plot.add_curve"),
                           QJsonObject{ { "graph", "G97M" },
                                        { "table", "T97P" },
                                        { "x_col", "X" },
                                        { "y_col", "B" },
                                        { "layer", 1 } })),
                    "97f: add_curve to layer 1 failed");
            CHECK97(ok(rpc(QStringLiteral("graph.arrange_layers"),
                           QJsonObject{ { "graph", "G97M" }, { "rows", 2 }, { "cols", 1 } })),
                    "97f: arrange_layers failed");
            auto *ml = qobject_cast<MultiLayer *>(findWindow97(w, QStringLiteral("G97M")));
            CHECK97(ml && ml->layersList().size() == 2, "97f: G97M should have 2 layers");
            if (ml && ml->layersList().size() == 2) {
                std::cout << "[INFO] 97f: keep aspect ratio " << ml->keepAspectRatio()
                          << " (the 1:1 lock took the single-panel path before the fix)\n";
                QList<AxisState97> before;
                for (Graph *lg : ml->layersList())
                    before << axisState97(lg, QwtPlot::yLeft);
                CHECK97(ok(rpc(QStringLiteral("graph.set_panel_labels"),
                               QJsonObject{ { "graph", "G97M" }, { "enabled", true } })),
                        "97f: set_panel_labels failed");
                CHECK97(ml->hasPanelLabels(), "97f: panel labels not enabled");
                for (int k = 0; k < 2; ++k) {
                    const AxisState97 after = axisState97(ml->layersList().at(k), QwtPlot::yLeft);
                    panelAfterLabels << after;
                    CHECK97(std::abs(after.canvasH - before[k].canvasH) <= 1,
                            "97f: layer " << k << " panel height changed by panel labels: "
                                          << describe97(before[k]) << " -> " << describe97(after));
                    CHECK97(sameRangeAndTicks97(before[k], after),
                            "97f: layer " << k << " axis changed by panel labels: "
                                          << describe97(before[k]) << " -> " << describe97(after));
                }
            }
        }

        // ---- 97g: 3D scale -------------------------------------------------------------------
        {
            auto *g3 = qobject_cast<Graph3D *>(findWindow97(w, QStringLiteral("S97F")));
            CHECK97(g3 != nullptr, "97g: 3D graph missing");
            if (g3)
                g3->setScale(sx, sy, sz);
        }

        CHECK_COND(w->saveFolder(w->projectFolder(), prj), "97: saving the project failed");
    }

    // ---- reopen ----------------------------------------------------------------------------
    {
        AppWinGuard97 rd(new ApplicationWindow(), true);
        CHECK_COND(rd->loadProject(prj), "97: reopening the project failed");
        QCoreApplication::processEvents();

        // 97a
        {
            Graph *g = firstLayer97(rd, QStringLiteral("G97Lbl"));
            CHECK97(g && g->curves() == 2,
                    "97a-reopen: relabelled graph has " << (g ? g->curves() : -1) << " curves");
            if (g && g->curves() == 2) {
                auto *dc = dynamic_cast<DataCurve *>(g->curve(0));
                CHECK97(dc && dc->table() && dc->yColumnName() == QLatin1String("T97_A")
                                && dc->table()->colIndex(dc->yColumnName()) >= 0,
                        "97a-reopen: curve 0 is not bound to T97_A");
                CHECK97(g->legend() && g->legend()->text().contains(QStringLiteral("Sample A")),
                        "97a-reopen: label missing from the legend");
            }
            Graph *g1 = firstLayer97(rd, QStringLiteral("G97Lbl1"));
            CHECK97(g1 && g1->curves() == 1, "97a-reopen: plot.xy label= curve lost");
            Graph *g2 = firstLayer97(rd, QStringLiteral("G97Lbl2"));
            CHECK97(g2 && g2->curves() == 2, "97a-reopen: plot.add_curve label= curve lost");
        }
        // 97b
        {
            Graph *g = firstLayer97(rd, QStringLiteral("G97CS"));
            CHECK97(g && g->curves() >= 1 && g->curve(0)->style() == QwtPlotCurve::NoCurve,
                    "97b-reopen: colour scatter style differs");
            if (g) {
                const AxisState97 xs1 = axisState97(g, QwtPlot::xBottom);
                CHECK97(std::fabs(xs1.lo - cscatXLo) < 1e-9 && std::fabs(xs1.hi - cscatXHi) < 1e-9,
                        "97b-reopen: X range " << cscatXLo << ".." << cscatXHi << " -> " << xs1.lo
                                               << ".." << xs1.hi);
            }
        }
        // 97c
        {
            Graph *g = firstLayer97(rd, QStringLiteral("G97HB"));
            CHECK97(g && plainTitle97(g, QwtPlot::yRight).isEmpty()
                            && !plainTitle97(g, QwtPlot::xBottom).isEmpty(),
                    "97c-reopen: horizontal bar axis titles differ");
        }
        // 97e
        {
            Graph *g = firstLayer97(rd, QStringLiteral("G97DY"));
            CHECK97(g && plainTitle97(g, QwtPlot::yLeft) == QLatin1String("A"),
                    "97e-reopen: left axis title differs");
        }
        // 97f
        {
            auto *ml = qobject_cast<MultiLayer *>(findWindow97(rd, QStringLiteral("G97M")));
            CHECK97(ml && ml->layersList().size() == 2 && panelAfterLabels.size() == 2,
                    "97f-reopen: G97M layers missing");
            if (ml && ml->layersList().size() == 2 && panelAfterLabels.size() == 2) {
                for (int k = 0; k < 2; ++k) {
                    const AxisState97 re = axisState97(ml->layersList().at(k), QwtPlot::yLeft);
                    CHECK97(sameRangeAndTicks97(panelAfterLabels[k], re),
                            "97f-reopen: layer " << k << " axis changed on reopen: "
                                                 << describe97(panelAfterLabels[k]) << " -> "
                                                 << describe97(re));
                }
            }
        }
        // 97g
        {
            auto *g3 = qobject_cast<Graph3D *>(findWindow97(rd, QStringLiteral("S97F")));
            CHECK97(g3 != nullptr, "97g-reopen: 3D graph missing");
            if (g3) {
                CHECK97(g3->xScale() == sx && g3->yScale() == sy && g3->zScale() == sz,
                        "97g-reopen: 3D scale " << std::setprecision(17) << g3->xScale() << ", "
                                                << g3->yScale() << ", " << g3->zScale());
            }
        }
    }

    if (g97Failures > 0) {
        std::cerr << "[FAIL] Suite 97: " << g97Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 97 passed.\n";
    return 0;
}
