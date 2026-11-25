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
//   97h a curve label is kept by the curve itself: legend rebuilds (mini toolbar, fit,
//       set_legend reconstruct), curve add / remove, hiding and showing the legend, undo,
//       duplicating the graph and save -> reopen; curve="<label>" resolves the curve in RPC;
//       a label given while there is no legend shows up when the legend is shown
//   97i 2-row layout: outside panel labels lie inside the page, above their canvas and clear of
//       the axis band of the panel above (panel heights / axis ranges: 97f)
//   97j a labelled curve switched vertical bars -> line (a new curve object) keeps its label,
//       also across save -> reopen
//   97k batch plotting a labelled graph onto another table does not carry the label over: the
//       copy's legend shows the new table's column and curve="<label>" does not resolve there
//   97l 2x2 layout with the top-left panel deleted: outside panel labels (set twice) do not push
//       the remaining panels down by a panel height nor grow the page again
//   97m two curves of one Y column (different X): a label on the 2nd curve resolves to the 2nd
//       curve in RPC (style, visibility, mask, remove); a method that selects curves by column
//       (fit) reports the ambiguity instead of using the 1st curve
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

bool legendHas97(Graph *g, const QString &line)
{
    return g && g->legend() && g->legend()->text().split(QLatin1Char('\n')).contains(line);
}

std::string legendText97(Graph *g)
{
    return (g && g->legend()) ? g->legend()->text().toStdString() : std::string("<none>");
}

// Outside panel labels: every label lies inside the page, above its own canvas, and clear of
// the bottom axis band (tick labels / axis title) of the panel above it.
bool panelLabelsClear97(MultiLayer *ml, QString *why)
{
    const QVector<MultiLayer::PanelLabelBox> boxes =
            ml ? ml->panelLabelBoxes() : QVector<MultiLayer::PanelLabelBox>();
    QWidget *page = ml ? ml->canvasWidget() : nullptr;
    if (boxes.size() < 2 || !page) {
        *why = QStringLiteral("%1 label boxes").arg(boxes.size());
        return false;
    }
    const QRect pageRect = page->rect();
    QStringList bad;
    for (int i = 0; i < boxes.size(); ++i) {
        const MultiLayer::PanelLabelBox &pb = boxes[i];
        Plot *p = pb.graph->plotWidget();
        p->updateLayout();
        const QRect canvasRect = p->plotLayout()->canvasRect().translated(pb.graph->pos());
        // The 4 px padding around the glyphs may touch the page edge; the glyphs may not.
        const QRect glyphs = pb.box.adjusted(4, 4, -4, -4);
        if (!pageRect.contains(glyphs))
            bad << QStringLiteral("label %1 (%2,%3 %4x%5) outside page %6x%7")
                            .arg(i)
                            .arg(glyphs.x())
                            .arg(glyphs.y())
                            .arg(glyphs.width())
                            .arg(glyphs.height())
                            .arg(pageRect.width())
                            .arg(pageRect.height());
        if (glyphs.bottom() >= canvasRect.top())
            bad << QStringLiteral("label %1 bottom %2 not above its canvas top %3")
                            .arg(i)
                            .arg(glyphs.bottom())
                            .arg(canvasRect.top());
        for (int j = 0; j < boxes.size(); ++j) {
            if (boxes[j].row != pb.row - 1)
                continue;
            Graph *up = boxes[j].graph;
            Plot *up_p = up->plotWidget();
            up_p->updateLayout();
            const QRect upCanvas = up_p->plotLayout()->canvasRect().translated(up->pos());
            const QRect band(up->x(), upCanvas.bottom() + 1, up->width(),
                             up->geometry().bottom() - upCanvas.bottom());
            if (band.intersects(pb.box))
                bad << QStringLiteral("label %1 (top %2) overlaps the axis band of panel %3 "
                                      "(%4..%5)")
                                .arg(i)
                                .arg(pb.box.top())
                                .arg(j)
                                .arg(band.top())
                                .arg(band.bottom());
        }
    }
    *why = bad.join(QStringLiteral("; "));
    return bad.isEmpty();
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
    QString batchCloneName;

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

        // ---- 97h: curve labels survive legend rebuilds, curve edits, undo and copies ----------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A", "B" } },
                                        { "graph_name", "G97LR" } })),
                    "97h: plot.xy failed");
            CHECK97(ok(rpc(QStringLiteral("graph.set_curve_style"),
                           QJsonObject{ { "graph", "G97LR" },
                                        { "curve", 0 },
                                        { "label", "Sample A" } })),
                    "97h: label on curve 0 failed");
            auto *ml = qobject_cast<MultiLayer *>(findWindow97(w, QStringLiteral("G97LR")));
            Graph *g = firstLayer97(w, QStringLiteral("G97LR"));
            CHECK97(ml && g && g->curves() == 2, "97h: G97LR should have 2 curves");
            if (ml && g && g->curves() == 2) {
                CHECK97(g->curveCustomLabel(0) == QLatin1String("Sample A")
                                && g->curve(0)->title().text() == QLatin1String("T97_A"),
                        "97h: label not kept on the curve (custom '"
                                << g->curveCustomLabel(0).toStdString() << "', title '"
                                << g->curve(0)->title().text().toStdString() << "')");
                // Mini toolbar colour / width / symbol changes rebuild the legend.
                CHECK97(ok(rpc(QStringLiteral("graph.set_curve_style"),
                               QJsonObject{ { "graph", "G97LR" },
                                            { "curve", 0 },
                                            { "color", "#ff0000" } })),
                        "97h: colour change failed");
                g->reconstructLegend();
                CHECK97(legendHas97(g, QStringLiteral("\\c{1}Sample A")),
                        "97h: label lost by reconstructLegend: " << legendText97(g));
                // Adding a fit curve rewrites the legend from legendText().
                CHECK97(g->legendText().contains(QStringLiteral("\\c{1}Sample A")),
                        "97h: legendText() ignores the label: " << g->legendText().toStdString());
                CHECK97(ok(rpc(QStringLiteral("graph.set_legend"),
                               QJsonObject{ { "graph", "G97LR" }, { "reconstruct", true } })),
                        "97h: set_legend reconstruct failed");
                g = firstLayer97(w, QStringLiteral("G97LR"));
                CHECK97(legendHas97(g, QStringLiteral("\\c{1}Sample A")),
                        "97h: label lost by set_legend(reconstruct): " << legendText97(g));
                // Curve add / remove (CurvesDialog): the label follows its curve.
                CHECK97(ok(rpc(QStringLiteral("plot.add_curve"),
                               QJsonObject{ { "graph", "G97LR" },
                                            { "table", "T97" },
                                            { "x_col", "X" },
                                            { "y_col", "C" },
                                            { "label", "Sample C" } })),
                        "97h: add_curve C failed");
                CHECK97(ok(rpc(QStringLiteral("plot.remove_curve"),
                               QJsonObject{ { "graph", "G97LR" }, { "curve", 1 } })),
                        "97h: remove_curve B failed");
                g = firstLayer97(w, QStringLiteral("G97LR"));
                CHECK97(g && g->curves() == 2, "97h: G97LR should have 2 curves after remove");
                if (g && g->curves() == 2) {
                    g->reconstructLegend();
                    CHECK97(legendHas97(g, QStringLiteral("\\c{1}Sample A"))
                                    && legendHas97(g, QStringLiteral("\\c{2}Sample C")),
                            "97h: labels not renumbered after removing a curve: "
                                    << legendText97(g));
                    CHECK97(g->curve(1)->title().text() == QLatin1String("T97_C"),
                            "97h: curve 1 should be T97_C");
                }
                // curve="label" resolves the curve in RPC calls.
                CHECK97(ok(rpc(QStringLiteral("graph.set_curve_style"),
                               QJsonObject{ { "graph", "G97LR" },
                                            { "curve", "Sample C" },
                                            { "color", "#00ff00" } })),
                        "97h: curve=\"Sample C\" not resolved");
                g = firstLayer97(w, QStringLiteral("G97LR"));
                CHECK97(g && g->curves() == 2
                                && g->curve(1)->pen().color() == QColor(QStringLiteral("#00ff00")),
                        "97h: curve=\"Sample C\" styled the wrong curve");
                // Hide -> show the legend.
                CHECK97(ok(rpc(QStringLiteral("graph.set_legend"),
                               QJsonObject{ { "graph", "G97LR" }, { "visible", false } })),
                        "97h: hiding the legend failed");
                CHECK97(ok(rpc(QStringLiteral("graph.set_legend"),
                               QJsonObject{ { "graph", "G97LR" },
                                            { "visible", true },
                                            { "reconstruct", true } })),
                        "97h: showing the legend failed");
                g = firstLayer97(w, QStringLiteral("G97LR"));
                CHECK97(legendHas97(g, QStringLiteral("\\c{1}Sample A"))
                                && legendHas97(g, QStringLiteral("\\c{2}Sample C")),
                        "97h: labels lost by hiding / showing the legend: " << legendText97(g));
                // Undo snapshot restore keeps the labels.
                CHECK97(ok(rpc(QStringLiteral("graph.set_curve_style"),
                               QJsonObject{ { "graph", "G97LR" },
                                            { "curve", 0 },
                                            { "line_width", 3 } })),
                        "97h: width change failed");
                CHECK97(ml->undoStack() && ml->undoStack()->canUndo(), "97h: nothing to undo");
                if (ml->undoStack() && ml->undoStack()->canUndo())
                    ml->undoStack()->undo();
                QCoreApplication::processEvents();
                g = firstLayer97(w, QStringLiteral("G97LR"));
                CHECK97(g && g->curves() == 2 && g->curveCustomLabel(0) == QLatin1String("Sample A")
                                && g->curveCustomLabel(1) == QLatin1String("Sample C"),
                        "97h: labels lost by undo");
                if (g) {
                    g->reconstructLegend();
                    CHECK97(legendHas97(g, QStringLiteral("\\c{1}Sample A")),
                            "97h: label lost after undo + reconstruct: " << legendText97(g));
                }
                // Duplicating the graph window (Graph::copy).
                auto *dup = qobject_cast<MultiLayer *>(w->clone(ml));
                QCoreApplication::processEvents();
                Graph *dg = (dup && !dup->layersList().isEmpty()) ? dup->layersList().first()
                                                                  : nullptr;
                CHECK97(dg && dg->curves() == 2
                                && dg->curveCustomLabel(0) == QLatin1String("Sample A")
                                && dg->curveCustomLabel(1) == QLatin1String("Sample C"),
                        "97h: labels lost by duplicating the graph");
                if (dg) {
                    dg->reconstructLegend();
                    CHECK97(legendHas97(dg, QStringLiteral("\\c{1}Sample A")),
                            "97h: duplicate legend lost the label: " << legendText97(dg));
                }
                if (dup) {
                    dup->askOnCloseEvent(false);
                    dup->close();
                    QCoreApplication::processEvents();
                }
            }
            // A label given while the graph has no legend is kept for when it is shown.
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "graph_name", "G97NL" },
                                        { "legend", false },
                                        { "label", "NoLeg" } })),
                    "97h: plot.xy legend=false label= failed");
            CHECK97(ok(rpc(QStringLiteral("graph.set_legend"),
                           QJsonObject{ { "graph", "G97NL" },
                                        { "visible", true },
                                        { "reconstruct", true } })),
                    "97h: showing the G97NL legend failed");
            Graph *gn = firstLayer97(w, QStringLiteral("G97NL"));
            CHECK97(legendHas97(gn, QStringLiteral("\\c{1}NoLeg")),
                    "97h: label given without a legend is lost: " << legendText97(gn));
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
                // 97i: the outside labels have room: inside the page, above their canvas, clear
                // of the upper panel's bottom axis band.
                QString why;
                CHECK97(panelLabelsClear97(ml, &why), "97i: panel labels: " << why.toStdString());
                for (const MultiLayer::PanelLabelBox &pb : ml->panelLabelBoxes())
                    std::cout << "[INFO] 97i: label row " << pb.row << " box (" << pb.box.x()
                              << "," << pb.box.y() << " " << pb.box.width() << "x"
                              << pb.box.height() << "), panel y " << pb.graph->y() << " h "
                              << pb.graph->height() << "\n";
                // Setting them again (or with the same options) does not move anything further.
                QList<QRect> geo;
                for (Graph *lg : ml->layersList())
                    geo << lg->geometry();
                CHECK97(ok(rpc(QStringLiteral("graph.set_panel_labels"),
                               QJsonObject{ { "graph", "G97M" }, { "enabled", true } })),
                        "97i: second set_panel_labels failed");
                for (int k = 0; k < ml->layersList().size() && k < geo.size(); ++k)
                    CHECK97(ml->layersList().at(k)->geometry() == geo[k],
                            "97i: layer " << k << " moved by a repeated set_panel_labels");
            }
        }

        // ---- 97j: label kept when switching bars <-> line ------------------------------------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "style", "vertical_bars" },
                                        { "graph_name", "G97BL" } })),
                    "97j: vertical bars failed");
            CHECK97(ok(rpc(QStringLiteral("graph.set_curve_style"),
                           QJsonObject{ { "graph", "G97BL" },
                                        { "curve", 0 },
                                        { "label", "BarLbl" } })),
                    "97j: label on the bar curve failed");
            Graph *g = firstLayer97(w, QStringLiteral("G97BL"));
            CHECK97(g && g->curves() == 1 && g->curveType(0) == Graph::VerticalBars,
                    "97j: G97BL should hold one vertical bar curve");
            if (g && g->curves() == 1) {
                g->setCurveType(0, Graph::Line);
                CHECK97(g->curveType(0) == Graph::Line, "97j: curve type not switched to line");
                CHECK97(g->curveCustomLabel(0) == QLatin1String("BarLbl"),
                        "97j: label lost by bars -> line ('"
                                << g->curveCustomLabel(0).toStdString() << "')");
                g->setCurveType(0, Graph::HorizontalBars);
                g->setCurveType(0, Graph::Scatter);
                CHECK97(g->curveCustomLabel(0) == QLatin1String("BarLbl"),
                        "97j: label lost by line -> horizontal bars -> scatter");
                g->setCurveType(0, Graph::VerticalBars);
                g->setCurveType(0, Graph::Line);
                g->reconstructLegend();
                CHECK97(legendHas97(g, QStringLiteral("\\c{1}BarLbl")),
                        "97j: legend lost the label after the type changes: " << legendText97(g));
            }
        }

        // ---- 97k: batch plotting does not carry a label to another table ---------------------
        {
            QJsonArray b2x, b2a, b2b, b2c;
            for (int i = 0; i <= 10; ++i) {
                b2x.append(double(i));
                b2a.append(2.0 * i);
                b2b.append(3.0 * i);
                b2c.append(4.0 * i);
            }
            CHECK_COND(makeTable(QStringLiteral("T97B2"), { "X", "A", "B", "C" },
                                 { b2x, b2a, b2b, b2c }),
                       "97k: creating table T97B2 failed");
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "graph_name", "G97BC" },
                                        { "label", "Sample1" } })),
                    "97k: plot.xy label= failed");
            const QJsonObject r = rpc(QStringLiteral("graph.batch_clone"),
                                      QJsonObject{ { "graph", "G97BC" }, { "table", "T97B2" } });
            CHECK97(ok(r), "97k: graph.batch_clone failed");
            batchCloneName =
                    r.value(QStringLiteral("result")).toObject().value(QStringLiteral("name")).toString();
            Graph *src = firstLayer97(w, QStringLiteral("G97BC"));
            Graph *cg = firstLayer97(w, batchCloneName);
            CHECK97(src && src->curveCustomLabel(0) == QLatin1String("Sample1"),
                    "97k: batch plotting removed the source label");
            CHECK97(cg && cg->curves() == 1, "97k: batch copy '" << batchCloneName.toStdString()
                                                                 << "' missing or empty");
            if (cg && cg->curves() == 1) {
                CHECK97(cg->curve(0)->title().text() == QLatin1String("T97B2_A"),
                        "97k: batch copy curve bound to '"
                                << cg->curve(0)->title().text().toStdString() << "'");
                CHECK97(cg->curveCustomLabel(0).isEmpty(),
                        "97k: batch copy carries the label '"
                                << cg->curveCustomLabel(0).toStdString() << "'");
                CHECK97(cg->legend() && !cg->legend()->text().contains(QStringLiteral("Sample1"))
                                && cg->legend()->text().contains(QStringLiteral("\\c{1}")),
                        "97k: batch copy legend: " << legendText97(cg));
                const QJsonObject rs = rpc(QStringLiteral("graph.set_curve_style"),
                                           QJsonObject{ { "graph", batchCloneName },
                                                        { "curve", "Sample1" },
                                                        { "color", "#123456" } });
                CHECK97(!ok(rs), "97k: curve=\"Sample1\" resolved on the batch copy");
            }
        }

        // ---- 97l: 2x2 with a deleted panel + outside panel labels -----------------------------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97P" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "A" } },
                                        { "graph_name", "G97Q" } })),
                    "97l: plot.xy failed");
            for (int k = 1; k <= 3; ++k) {
                CHECK97(ok(rpc(QStringLiteral("graph.add_layer"), QJsonObject{ { "graph", "G97Q" } })),
                        "97l: add_layer " << k << " failed");
                CHECK97(ok(rpc(QStringLiteral("plot.add_curve"),
                               QJsonObject{ { "graph", "G97Q" },
                                            { "table", "T97P" },
                                            { "x_col", "X" },
                                            { "y_col", (k % 2) ? "B" : "A" },
                                            { "layer", k } })),
                        "97l: add_curve to layer " << k << " failed");
            }
            CHECK97(ok(rpc(QStringLiteral("graph.arrange_layers"),
                           QJsonObject{ { "graph", "G97Q" }, { "rows", 2 }, { "cols", 2 } })),
                    "97l: arrange_layers 2x2 failed");
            CHECK97(ok(rpc(QStringLiteral("graph.remove_layer"),
                           QJsonObject{ { "graph", "G97Q" }, { "layer", 0 } })),
                    "97l: removing the top-left layer failed");
            auto *ml = qobject_cast<MultiLayer *>(findWindow97(w, QStringLiteral("G97Q")));
            CHECK97(ml && ml->layersList().size() == 3, "97l: G97Q should have 3 layers");
            if (ml && ml->layersList().size() == 3) {
                QCoreApplication::processEvents();
                QList<QRect> geo0;
                for (Graph *lg : ml->layersList())
                    geo0 << lg->geometry();
                const int pageH0 = ml->canvasWidget() ? ml->canvasWidget()->height() : -1;
                CHECK97(ok(rpc(QStringLiteral("graph.set_panel_labels"),
                               QJsonObject{ { "graph", "G97Q" }, { "enabled", true } })),
                        "97l: set_panel_labels failed");
                QList<QRect> geo1;
                for (Graph *lg : ml->layersList())
                    geo1 << lg->geometry();
                const int pageH1 = ml->canvasWidget() ? ml->canvasWidget()->height() : -1;
                int maxPanelH = 0;
                for (int k = 0; k < 3; ++k) {
                    maxPanelH = qMax(maxPanelH, geo0[k].height());
                    std::cout << "[INFO] 97l: layer " << k << " y " << geo0[k].y() << " -> "
                              << geo1[k].y() << " (h " << geo0[k].height() << ")\n";
                    CHECK97(std::abs(geo1[k].y() - geo0[k].y()) <= geo0[k].height() / 4
                                    && geo1[k].x() == geo0[k].x()
                                    && geo1[k].size() == geo0[k].size(),
                            "97l: layer " << k << " moved from y " << geo0[k].y() << " to "
                                          << geo1[k].y() << " by the panel labels");
                }
                std::cout << "[INFO] 97l: page height " << pageH0 << " -> " << pageH1 << "\n";
                // Panels that shared a row on the page still do, and every label sits just
                // above its own panel (not at the height of another row's labels).
                for (int a = 0; a < 3; ++a)
                    for (int c = a + 1; c < 3; ++c)
                        if (geo0[a].y() == geo0[c].y())
                            CHECK97(geo1[a].y() == geo1[c].y(),
                                    "97l: layers " << a << " and " << c << " of one row now at y "
                                                   << geo1[a].y() << " / " << geo1[c].y());
                QString whyL;
                CHECK97(panelLabelsClear97(ml, &whyL), "97l: panel labels: " << whyL.toStdString());
                for (const MultiLayer::PanelLabelBox &pb : ml->panelLabelBoxes()) {
                    Plot *pp = pb.graph->plotWidget();
                    pp->updateLayout();
                    const int canvasTop =
                            pp->plotLayout()->canvasRect().translated(pb.graph->pos()).top();
                    CHECK97(canvasTop - pb.box.bottom() <= pb.graph->height() / 4,
                            "97l: label of layer " << ml->layersList().indexOf(pb.graph)
                                                   << " (bottom " << pb.box.bottom()
                                                   << ") far above its canvas top " << canvasTop);
                }
                CHECK97(pageH1 - pageH0 <= maxPanelH / 4,
                        "97l: page grew from " << pageH0 << " to " << pageH1);
                CHECK97(ok(rpc(QStringLiteral("graph.set_panel_labels"),
                               QJsonObject{ { "graph", "G97Q" }, { "enabled", true } })),
                        "97l: second set_panel_labels failed");
                for (int k = 0; k < 3; ++k)
                    CHECK97(ml->layersList().at(k)->geometry() == geo1[k],
                            "97l: layer " << k << " moved by a repeated set_panel_labels (y "
                                          << geo1[k].y() << " -> "
                                          << ml->layersList().at(k)->geometry().y() << ")");
                CHECK97(!ml->canvasWidget() || ml->canvasWidget()->height() == pageH1,
                        "97l: page grew again on a repeated set_panel_labels ("
                                << pageH1 << " -> " << ml->canvasWidget()->height() << ")");
            }
        }

        // ---- 97m: label on the 2nd of two curves of one Y column -----------------------------
        {
            CHECK97(ok(rpc(QStringLiteral("plot.xy"),
                           QJsonObject{ { "table", "T97" },
                                        { "x_col", "X" },
                                        { "y_cols", QJsonArray{ "B" } },
                                        { "graph_name", "G97DUP" } })),
                    "97m: plot.xy failed");
            CHECK97(ok(rpc(QStringLiteral("plot.add_curve"),
                           QJsonObject{ { "graph", "G97DUP" },
                                        { "table", "T97" },
                                        { "x_col", "C" },
                                        { "y_col", "B" },
                                        { "label", "run2" } })),
                    "97m: add_curve X=C Y=B label=run2 failed");
            Graph *g = firstLayer97(w, QStringLiteral("G97DUP"));
            auto xColOf = [](Graph *gr, int i) {
                auto *dc = gr ? dynamic_cast<DataCurve *>(gr->curve(i)) : nullptr;
                return dc ? dc->xColumnName() : QString();
            };
            CHECK97(g && g->curves() == 2 && g->curve(0)->title().text() == QLatin1String("T97_B")
                            && g->curve(1)->title().text() == QLatin1String("T97_B")
                            && xColOf(g, 1) == QLatin1String("T97_C")
                            && g->curveCustomLabel(1) == QLatin1String("run2"),
                    "97m: G97DUP should hold B(X) and B(C) with the label on the 2nd");
            if (g && g->curves() == 2) {
                const QColor before0 = g->curve(0)->pen().color();
                CHECK97(ok(rpc(QStringLiteral("graph.set_curve_style"),
                               QJsonObject{ { "graph", "G97DUP" },
                                            { "curve", "run2" },
                                            { "color", "#0000ff" } })),
                        "97m: set_curve_style curve=run2 failed");
                g = firstLayer97(w, QStringLiteral("G97DUP"));
                CHECK97(g->curve(1)->pen().color() == QColor(QStringLiteral("#0000ff"))
                                && g->curve(0)->pen().color() == before0,
                        "97m: curve=run2 styled the 1st curve");
                CHECK97(ok(rpc(QStringLiteral("graph.set_curve_visible"),
                               QJsonObject{ { "graph", "G97DUP" },
                                            { "curve", "run2" },
                                            { "visible", false } })),
                        "97m: set_curve_visible curve=run2 failed");
                g = firstLayer97(w, QStringLiteral("G97DUP"));
                CHECK97(!g->isCurveVisible(1) && g->isCurveVisible(0),
                        "97m: curve=run2 hid the wrong curve");
                rpc(QStringLiteral("graph.set_curve_visible"),
                    QJsonObject{ { "graph", "G97DUP" }, { "curve", 1 }, { "visible", true } });
                // X of the 2nd curve is exp(x): only its last two points lie in [5, 8]; the 1st
                // curve (X in [-2, 2]) has none there.
                const QJsonObject rm = rpc(QStringLiteral("graph.mask_region"),
                                           QJsonObject{ { "graph", "G97DUP" },
                                                        { "curve", "run2" },
                                                        { "x_min", 5.0 },
                                                        { "x_max", 8.0 } });
                CHECK97(ok(rm)
                                && rm.value(QStringLiteral("result"))
                                                .toObject()
                                                .value(QStringLiteral("masked_points"))
                                                .toInt()
                                        == 2,
                        "97m: mask_region curve=run2 masked "
                                << rm.value(QStringLiteral("result"))
                                           .toObject()
                                           .value(QStringLiteral("masked_points"))
                                           .toInt()
                                << " points (expected 2)");
                CHECK97(ok(rpc(QStringLiteral("graph.clear_masks"),
                               QJsonObject{ { "graph", "G97DUP" }, { "curve", "run2" } })),
                        "97m: clear_masks curve=run2 failed");
                // A fit picks its data by column title: it must not silently fit the 1st curve.
                const QJsonObject rf = rpc(QStringLiteral("analysis.fit_linear"),
                                           QJsonObject{ { "graph", "G97DUP" },
                                                        { "curve", "run2" },
                                                        { "plot_curve", false } });
                CHECK97(!ok(rf), "97m: fit_linear curve=run2 used the 1st curve silently");
                CHECK97(ok(rpc(QStringLiteral("plot.remove_curve"),
                               QJsonObject{ { "graph", "G97DUP" }, { "curve", "run2" } })),
                        "97m: remove_curve curve=run2 failed");
                g = firstLayer97(w, QStringLiteral("G97DUP"));
                CHECK97(g && g->curves() == 1 && xColOf(g, 0) == QLatin1String("T97_X"),
                        "97m: remove_curve curve=run2 removed the 1st curve (left X '"
                                << (g ? xColOf(g, 0).toStdString() : std::string("?")) << "')");
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
            if (g && g->curves() == 2) {
                CHECK97(g->curveCustomLabel(0) == QLatin1String("Sample A"),
                        "97a-reopen: curve label not restored ('"
                                << g->curveCustomLabel(0).toStdString() << "')");
                g->reconstructLegend();
                CHECK97(legendHas97(g, QStringLiteral("\\c{1}Sample A")),
                        "97a-reopen: label lost by reconstructLegend: " << legendText97(g));
            }
        }
        // 97h
        {
            Graph *g = firstLayer97(rd, QStringLiteral("G97LR"));
            CHECK97(g && g->curves() == 2 && g->curveCustomLabel(0) == QLatin1String("Sample A")
                            && g->curveCustomLabel(1) == QLatin1String("Sample C"),
                    "97h-reopen: curve labels not restored");
            if (g) {
                g->reconstructLegend();
                CHECK97(legendHas97(g, QStringLiteral("\\c{1}Sample A"))
                                && legendHas97(g, QStringLiteral("\\c{2}Sample C")),
                        "97h-reopen: labels lost by reconstructLegend: " << legendText97(g));
            }
            Graph *gn = firstLayer97(rd, QStringLiteral("G97NL"));
            CHECK97(gn && gn->curveCustomLabel(0) == QLatin1String("NoLeg"),
                    "97h-reopen: G97NL label not restored");
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
                QString why;
                CHECK97(panelLabelsClear97(ml, &why),
                        "97i-reopen: panel labels: " << why.toStdString());
            }
        }
        // 97j
        {
            Graph *g = firstLayer97(rd, QStringLiteral("G97BL"));
            CHECK97(g && g->curves() == 1 && g->curveType(0) == Graph::Line
                            && g->curveCustomLabel(0) == QLatin1String("BarLbl"),
                    "97j-reopen: bars -> line curve lost its type or label ('"
                            << (g ? g->curveCustomLabel(0).toStdString() : std::string("?"))
                            << "')");
        }
        // 97k
        {
            Graph *src = firstLayer97(rd, QStringLiteral("G97BC"));
            CHECK97(src && src->curveCustomLabel(0) == QLatin1String("Sample1"),
                    "97k-reopen: source label not restored");
            Graph *cg = firstLayer97(rd, batchCloneName);
            CHECK97(cg && cg->curves() == 1 && cg->curveCustomLabel(0).isEmpty()
                            && cg->legend()
                            && !cg->legend()->text().contains(QStringLiteral("Sample1")),
                    "97k-reopen: batch copy carries the source label: " << legendText97(cg));
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
