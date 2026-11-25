#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "Graph3D.h"
#include "ScaleDraw.h"

#include <qwt3d_surfaceplot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_widget.h>

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>

// =============================================================================
// Suite 108: plot geometry that must survive clone_window and save -> reopen.
//   108a a 3D matrix surface (Z 0..14 and a narrow Z range) and an XYZ scatter (Z 0..100) keep
//        the drawn size (raw qwtplot3d zoom) and report zoom 1 after app.clone_window and
//        after save -> reopen (the stored zoom is relative to the base zoom chosen at creation,
//        which the reopened / cloned plot now recomputes instead of leaving it at 1)
//   108b app.clone_window of a 2D graph (default, isij_85mm, slide_28pt) keeps the layer
//        rectangle, the plot area, the canvas size and the exported PNG of the source
//   108c a heatmap has the same plot area (colour bar margin) right after creation, after
//        app.clone_window and after save -> reopen, and exports the same PNG
//   108d a 3D surface with a base zoom above 10 (X, Y 0..10, Z 0..14: 11.5) zoomed in 4.5 times
//        keeps its zoom after clone_window and save -> reopen (the saved relative 4.5 was read
//        as an absolute zoom, the reopened plot shrank to about 1/11.5 of it)
// Every check is evaluated (failures are counted) so one run reports all regressions.
// =============================================================================
namespace {

int g108Failures = 0;

#define CHECK108(cond, msg)                                                                     \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;         \
            ++g108Failures;                                                                     \
        }                                                                                       \
    } while (0)

struct NonInteractiveGuard108
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard108()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard108()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard108
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard108(ApplicationWindow *w = nullptr) : win(w)
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
    ~AppWinGuard108() { reset(); }
    AppWinGuard108(const AppWinGuard108 &) = delete;
    AppWinGuard108 &operator=(const AppWinGuard108 &) = delete;
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

struct Rpc108
{
    PythonAutomationServer *srv = nullptr;
    int id = 10800;
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
            std::cerr << "[INFO] 108: " << method.toStdString() << " error: "
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
    bool makeMatrix(const QString &name, const QVector<QVector<double>> &cells)
    {
        const int rows = cells.size();
        const int cols = rows ? cells.first().size() : 0;
        if (!ok(call(QStringLiteral("matrix.create"),
                     QJsonObject{ { "rows", rows },
                                  { "cols", cols },
                                  { "name", name },
                                  { "x_min", 0.0 },
                                  { "x_max", 1.0 },
                                  { "y_min", 0.0 },
                                  { "y_max", 1.0 } })))
            return false;
        QJsonArray data;
        for (const QVector<double> &row : cells) {
            QJsonArray r;
            for (double v : row)
                r.append(v);
            data.append(r);
        }
        return ok(call(QStringLiteral("matrix.set_data"),
                       QJsonObject{ { "matrix", name }, { "data", data } }));
    }
};

MyWidget *findWindow108(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            return w;
    return nullptr;
}

Graph *firstLayer108(ApplicationWindow *app, const QString &name)
{
    auto *ml = qobject_cast<MultiLayer *>(findWindow108(app, name));
    if (!ml || ml->layersList().isEmpty())
        return nullptr;
    return ml->layersList().first();
}

// What graph.inspect reports as canvas_rect_px (plot area inside the MultiLayer canvas).
QRect plotArea108(Graph *g)
{
    Plot *p = g ? g->plotWidget() : nullptr;
    if (!p)
        return QRect();
    p->updateLayout();
    QRect local = (p->plotLayout() && p->plotLayout()->canvasRect().isValid())
            ? p->plotLayout()->canvasRect()
            : (p->canvas() ? p->canvas()->geometry() : QRect());
    return local.translated(g->geometry().topLeft() + p->pos());
}

std::string rectStr108(const QRect &r)
{
    return QStringLiteral("%1,%2 %3x%4")
            .arg(r.x())
            .arg(r.y())
            .arg(r.width())
            .arg(r.height())
            .toStdString();
}

std::string sizeStr108(const QSize &s)
{
    return QStringLiteral("%1x%2").arg(s.width()).arg(s.height()).toStdString();
}

// Axis widget metrics that decide where Qwt puts the plot area (printed when a check fails).
std::string axisMetrics108(Graph *g)
{
    Plot *p = g ? g->plotWidget() : nullptr;
    if (!p)
        return "<no plot>";
    QStringList t;
    for (int a = 0; a < QwtPlot::axisCnt; ++a) {
        const QwtScaleWidget *sw = p->axisWidget(a);
        if (!sw)
            continue;
        int sb = 0, eb = 0;
        sw->getMinBorderDist(sb, eb);
        const QwtScaleDraw *qsd = sw->scaleDraw();
        const auto *sd = dynamic_cast<const ScaleDraw *>(qsd);
        const QwtScaleDiv *div = p->axisScaleDiv(a);
        t << QStringLiteral("a%1[en %2 sp %3 mg %4 bd %5/%6 tf %7 hint %8x%9 cb %10/%11 brd %12 "
                            "tick %13 lbl %14 rng %15..%16 fnt %17 sty %18/%19]")
                     .arg(a)
                     .arg(p->axisEnabled(a) ? 1 : 0)
                     .arg(sw->spacing())
                     .arg(sw->margin())
                     .arg(sb)
                     .arg(eb)
                     .arg(sw->title().font().pointSizeF())
                     .arg(sw->sizeHint().width())
                     .arg(sw->sizeHint().height())
                     .arg(sw->isColorBarEnabled() ? 1 : 0)
                     .arg(sw->colorBarWidth())
                     .arg(sd ? (sd->hasColorBarBorder() ? sd->colorBarBorderWidth() : -1) : -2)
                     .arg(qsd ? qsd->majTickLength() : -1)
                     .arg(qsd ? (qsd->hasComponent(QwtAbstractScaleDraw::Labels) ? 1 : 0) : -1)
                     .arg(div ? div->lowerBound() : 0.0)
                     .arg(div ? div->upperBound() : 0.0)
                     .arg(p->axisFont(a).pointSizeF())
                     .arg(sd ? sd->majorTicksStyle() : -1)
                     .arg(sd ? sd->minorTicksStyle() : -1);
    }
    if (p->plotLayout())
        t << QStringLiteral("layout sp %1 cm %2 align %3")
                     .arg(p->plotLayout()->spacing())
                     .arg(p->plotLayout()->canvasMargin(QwtPlot::yLeft))
                     .arg(p->plotLayout()->alignCanvasToScales() ? 1 : 0);
    t << QStringLiteral("plot margin %1").arg(p->margin());
    return t.join(' ').toStdString();
}

bool sameImage108(const QString &a, const QString &b)
{
    const QImage ia(a), ib(b);
    return !ia.isNull() && ia.size() == ib.size()
            && ia.convertToFormat(QImage::Format_ARGB32) == ib.convertToFormat(QImage::Format_ARGB32);
}

bool exportPng108(Rpc108 &rpc, const QString &graph, const QString &path)
{
    return Rpc108::ok(rpc.call(QStringLiteral("graph.export"),
                               QJsonObject{ { "graph", graph }, { "path", path }, { "dpi", 96 } }));
}

bool clone108(Rpc108 &rpc, const QString &src, const QString &dst)
{
    return Rpc108::ok(rpc.call(QStringLiteral("app.clone_window"),
                               QJsonObject{ { "name", src }, { "new_name", dst } }));
}

struct Zoom3D108
{
    QString graph;
    double raw = 0.0; // qwtplot3d zoom (what is drawn)
    double rel = 0.0; // Graph3D::zoom() (what is saved and reported)
};

bool zoom3D108(ApplicationWindow *app, const QString &name, Zoom3D108 *z)
{
    auto *g = qobject_cast<Graph3D *>(findWindow108(app, name));
    if (!g || !g->sp)
        return false;
    z->raw = g->sp->zoom();
    z->rel = g->zoom();
    return true;
}

bool near108(double a, double b, double rel = 1e-6)
{
    return std::fabs(a - b) <= rel * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

struct Geom2D108
{
    QString graph;
    QRect layer;
    QRect area;
    QSize canvas;
    QString png;
};

Geom2D108 geom2D108(ApplicationWindow *app, const QString &name)
{
    Geom2D108 r;
    r.graph = name;
    auto *ml = qobject_cast<MultiLayer *>(findWindow108(app, name));
    Graph *g = firstLayer108(app, name);
    if (!ml || !g)
        return r;
    r.layer = g->geometry();
    r.area = plotArea108(g);
    r.canvas = ml->canvasWidget() ? ml->canvasWidget()->size() : QSize();
    return r;
}

} // namespace

int runSuite108PlotGeometryCloneRoundTrip(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 108: plot geometry across clone_window and save -> reopen "
                 "(3D zoom, 2D layer rectangle, heatmap colour bar margin)...\n";
    g108Failures = 0;
    NonInteractiveGuard108 niGuard;

    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "108: temp dir creation failed");
    const QString prj = tmp.filePath(QStringLiteral("geom_108.sciprj"));

    QList<Zoom3D108> zooms;
    QRect heatArea;
    QString heatPng = tmp.filePath(QStringLiteral("GH108_before.png"));

    {
        AppWinGuard108 w(new ApplicationWindow());
        Rpc108 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "108: pythonAutomationServer null");

        // ---- 108a: 3D plots ------------------------------------------------------------------
        {
            QVector<QVector<double>> big(8, QVector<double>(8));
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    big[r][c] = r + c; // Z 0..14
            QVector<QVector<double>> narrow(6, QVector<double>(6, 0.5));
            for (int i = 0; i < 6; ++i)
                narrow[i][i] = 0.7; // Z 0.5..0.7
            CHECK_COND(rpc.makeMatrix(QStringLiteral("M108A"), big), "108a: matrix M108A");
            CHECK_COND(rpc.makeMatrix(QStringLiteral("M108B"), narrow), "108a: matrix M108B");
            QJsonArray x, y, z;
            for (int i = 0; i < 20; ++i) {
                const double t = i / 19.0;
                x.append(t);
                y.append(t);
                z.append(100.0 * t * t); // Z 0..100
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T108X"), { "X", "Y", "Z" }, { x, y, z }),
                       "108a: table T108X");
            CHECK108(Rpc108::ok(rpc.call(QStringLiteral("plot.3d_matrix"),
                                         QJsonObject{ { "matrix", "M108A" },
                                                      { "style", "surface" },
                                                      { "graph_name", "G108S" } })),
                     "108a: plot.3d_matrix M108A");
            CHECK108(Rpc108::ok(rpc.call(QStringLiteral("plot.3d_matrix"),
                                         QJsonObject{ { "matrix", "M108B" },
                                                      { "style", "surface" },
                                                      { "graph_name", "G108N" } })),
                     "108a: plot.3d_matrix M108B");
            CHECK108(Rpc108::ok(rpc.call(QStringLiteral("plot.3d_xyz"),
                                         QJsonObject{ { "table", "T108X" },
                                                      { "x_col", "X" },
                                                      { "y_col", "Y" },
                                                      { "z_col", "Z" },
                                                      { "style", "scatter" },
                                                      { "graph_name", "G108X" } })),
                     "108a: plot.3d_xyz T108X");
            for (const char *n : { "G108S", "G108N", "G108X" }) {
                Zoom3D108 z0;
                z0.graph = QString::fromLatin1(n);
                CHECK108(zoom3D108(w, z0.graph, &z0), "108a: " << n << " missing");
                std::cout << "[INFO] 108a: " << n << " raw zoom " << z0.raw << ", zoom " << z0.rel
                          << "\n";
                CHECK108(near108(z0.rel, 1.0), "108a: " << n << " zoom after creation " << z0.rel);
                zooms << z0;

                const QString cn = z0.graph + QStringLiteral("C");
                CHECK108(clone108(rpc, z0.graph, cn), "108a: clone_window " << n);
                Zoom3D108 zc;
                CHECK108(zoom3D108(w, cn, &zc), "108a: clone of " << n << " missing");
                CHECK108(near108(zc.raw, z0.raw) && near108(zc.rel, z0.rel),
                         "108a: " << n << " clone raw zoom " << z0.raw << " -> " << zc.raw
                                  << ", zoom " << z0.rel << " -> " << zc.rel);
            }
        }

        // ---- 108d: a manual zoom on a plot whose base zoom is above 10 -----------------------
        {
            // X, Y 0..10 and Z 0..14: base zoom = |diagonal| / sqrt(3) = 11.5
            QVector<QVector<double>> cells(8, QVector<double>(8));
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    cells[r][c] = r + c;
            QJsonArray data;
            for (const QVector<double> &row : cells) {
                QJsonArray jr;
                for (double v : row)
                    jr.append(v);
                data.append(jr);
            }
            CHECK_COND(Rpc108::ok(rpc.call(QStringLiteral("matrix.create"),
                                           QJsonObject{ { "rows", 8 },
                                                        { "cols", 8 },
                                                        { "name", "M108Z" },
                                                        { "x_min", 0.0 },
                                                        { "x_max", 10.0 },
                                                        { "y_min", 0.0 },
                                                        { "y_max", 10.0 } }))
                               && Rpc108::ok(rpc.call(QStringLiteral("matrix.set_data"),
                                                      QJsonObject{ { "matrix", "M108Z" },
                                                                   { "data", data } })),
                       "108d: matrix M108Z");
            CHECK108(Rpc108::ok(rpc.call(QStringLiteral("plot.3d_matrix"),
                                         QJsonObject{ { "matrix", "M108Z" },
                                                      { "style", "surface" },
                                                      { "graph_name", "G108Z" } })),
                     "108d: plot.3d_matrix M108Z");
            auto *g = qobject_cast<Graph3D *>(findWindow108(w, QStringLiteral("G108Z")));
            CHECK_COND(g && g->sp, "108d: G108Z missing");
            const double base = g->sp->zoom();
            // zoomed in 4.5 times with the mouse wheel (the drawn zoom, relative 4.5)
            g->sp->setZoom(4.5 * base);
            Zoom3D108 z0;
            z0.graph = QStringLiteral("G108Z");
            CHECK108(zoom3D108(w, z0.graph, &z0), "108d: G108Z missing");
            std::cout << "[INFO] 108d: base zoom " << base << ", raw zoom " << z0.raw
                      << ", zoom " << z0.rel << "\n";
            CHECK108(base > 10.0 && near108(z0.rel, 4.5),
                     "108d: base zoom " << base << " (> 10 expected), zoom " << z0.rel);
            zooms << z0;
            CHECK108(clone108(rpc, z0.graph, QStringLiteral("G108ZC")), "108d: clone_window");
            Zoom3D108 zc;
            CHECK108(zoom3D108(w, QStringLiteral("G108ZC"), &zc), "108d: clone missing");
            CHECK108(near108(zc.raw, z0.raw) && near108(zc.rel, z0.rel),
                     "108d: clone raw zoom " << z0.raw << " -> " << zc.raw << ", zoom " << z0.rel
                                             << " -> " << zc.rel);
        }

        // ---- 108b: 2D clone geometry ---------------------------------------------------------
        {
            QJsonArray x, a;
            for (int i = 0; i < 30; ++i) {
                const double t = 1.0 + 9.0 * i / 29.0;
                x.append(t);
                a.append(t * t);
            }
            CHECK_COND(rpc.makeTable(QStringLiteral("T108P"), { "X", "A" }, { x, a }),
                       "108b: table T108P");
            for (const char *preset : { "", "isij_85mm", "slide_28pt" }) {
                const QString gname = QStringLiteral("G108P%1").arg(
                        QString::fromLatin1(preset).isEmpty() ? QStringLiteral("def")
                                                              : QString::fromLatin1(preset).left(3));
                CHECK108(Rpc108::ok(rpc.call(QStringLiteral("plot.xy"),
                                             QJsonObject{ { "table", "T108P" },
                                                          { "x_col", "X" },
                                                          { "y_cols", QJsonArray{ "A" } },
                                                          { "style", "scatter" },
                                                          { "graph_name", gname } })),
                         "108b: plot.xy " << gname.toStdString());
                if (*preset)
                    CHECK108(Rpc108::ok(rpc.call(QStringLiteral("graph.apply_style_preset"),
                                                 QJsonObject{ { "graph", gname },
                                                              { "preset", preset } })),
                             "108b: apply_style_preset " << preset);
                const QString cname = gname + QStringLiteral("C");
                CHECK108(clone108(rpc, gname, cname), "108b: clone_window " << gname.toStdString());
                Geom2D108 s = geom2D108(w, gname);
                Geom2D108 c = geom2D108(w, cname);
                std::cout << "[INFO] 108b: " << gname.toStdString() << " layer "
                          << rectStr108(s.layer) << " area " << rectStr108(s.area) << " canvas "
                          << sizeStr108(s.canvas) << "; clone layer " << rectStr108(c.layer)
                          << " area " << rectStr108(c.area) << " canvas "
                          << sizeStr108(c.canvas) << "\n";
                CHECK108(s.layer.isValid() && s.layer == c.layer,
                         "108b: " << gname.toStdString() << " clone layer rect "
                                  << rectStr108(s.layer) << " -> " << rectStr108(c.layer));
                CHECK108(s.area.isValid() && s.area == c.area,
                         "108b: " << gname.toStdString() << " clone plot area "
                                  << rectStr108(s.area) << " -> " << rectStr108(c.area)
                                  << "\n  source " << axisMetrics108(firstLayer108(w, gname))
                                  << "\n  clone  " << axisMetrics108(firstLayer108(w, cname)));
                CHECK108(s.canvas == c.canvas,
                         "108b: " << gname.toStdString() << " clone canvas "
                                  << sizeStr108(s.canvas) << " -> " << sizeStr108(c.canvas));
                auto *sml = qobject_cast<MultiLayer *>(findWindow108(w, gname));
                auto *cml = qobject_cast<MultiLayer *>(findWindow108(w, cname));
                CHECK108(sml && cml && near108(sml->pageWidthPhysical(), cml->pageWidthPhysical())
                                 && near108(sml->pageHeightPhysical(), cml->pageHeightPhysical())
                                 && near108(sml->pageZoom(), cml->pageZoom()),
                         "108b: " << gname.toStdString() << " clone page size / zoom changed");
                const QString ps = tmp.filePath(gname + QStringLiteral(".png"));
                const QString pc = tmp.filePath(cname + QStringLiteral(".png"));
                CHECK108(exportPng108(rpc, gname, ps) && exportPng108(rpc, cname, pc),
                         "108b: PNG export " << gname.toStdString());
                CHECK108(sameImage108(ps, pc),
                         "108b: " << gname.toStdString() << " clone PNG differs ("
                                  << sizeStr108(QImage(ps).size()) << " vs "
                                  << sizeStr108(QImage(pc).size()) << ")");
            }
        }

        // ---- 108c: heatmap colour bar margin -------------------------------------------------
        {
            QVector<QVector<double>> cells(6, QVector<double>(6));
            for (int r = 0; r < 6; ++r)
                for (int c = 0; c < 6; ++c)
                    cells[r][c] = r * 6 + c;
            CHECK_COND(rpc.makeMatrix(QStringLiteral("M108H"), cells), "108c: matrix M108H");
            CHECK108(Rpc108::ok(rpc.call(QStringLiteral("plot.matrix"),
                                         QJsonObject{ { "matrix", "M108H" },
                                                      { "style", "colormap" },
                                                      { "graph_name", "GH108" } })),
                     "108c: plot.matrix colormap");
            heatArea = plotArea108(firstLayer108(w, QStringLiteral("GH108")));
            std::cout << "[INFO] 108c: heatmap plot area " << rectStr108(heatArea) << "\n";
            CHECK108(exportPng108(rpc, QStringLiteral("GH108"), heatPng), "108c: PNG export");
            CHECK108(clone108(rpc, QStringLiteral("GH108"), QStringLiteral("GH108C")),
                     "108c: clone_window");
            const QRect ca = plotArea108(firstLayer108(w, QStringLiteral("GH108C")));
            CHECK108(heatArea.isValid() && ca == heatArea,
                     "108c: heatmap clone plot area "
                             << rectStr108(heatArea) << " -> " << rectStr108(ca) << "\n  source "
                             << axisMetrics108(firstLayer108(w, QStringLiteral("GH108")))
                             << "\n  clone  "
                             << axisMetrics108(firstLayer108(w, QStringLiteral("GH108C"))));
            const QString pc = tmp.filePath(QStringLiteral("GH108C.png"));
            CHECK108(exportPng108(rpc, QStringLiteral("GH108C"), pc) && sameImage108(heatPng, pc),
                     "108c: heatmap clone PNG differs");
            const QRect after = plotArea108(firstLayer108(w, QStringLiteral("GH108")));
            CHECK108(after == heatArea,
                     "108c: heatmap plot area changed by export / clone " << rectStr108(heatArea)
                                                                          << " -> "
                                                                          << rectStr108(after));
        }

        CHECK_COND(w->saveFolder(w->projectFolder(), prj), "108: saving the project failed");
    }

    // ---- reopen ------------------------------------------------------------------------------
    {
        AppWinGuard108 rd(new ApplicationWindow());
        CHECK_COND(rd->loadProject(prj), "108: reopening the project failed");
        QCoreApplication::processEvents();
        Rpc108 rpc;
        rpc.srv = rd->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "108: reopened pythonAutomationServer null");

        for (const Zoom3D108 &z0 : zooms) {
            Zoom3D108 z1;
            CHECK108(zoom3D108(rd, z0.graph, &z1), "108a-reopen: " << z0.graph.toStdString()
                                                                   << " missing");
            std::cout << "[INFO] 108a-reopen: " << z0.graph.toStdString() << " raw zoom " << z1.raw
                      << ", zoom " << z1.rel << "\n";
            CHECK108(near108(z1.raw, z0.raw) && near108(z1.rel, z0.rel),
                     "108a-reopen: " << z0.graph.toStdString() << " raw zoom " << z0.raw << " -> "
                                     << z1.raw << ", zoom " << z0.rel << " -> " << z1.rel);
        }

        const QRect ra = plotArea108(firstLayer108(rd, QStringLiteral("GH108")));
        CHECK108(ra == heatArea,
                 "108c-reopen: heatmap plot area "
                         << rectStr108(heatArea) << " -> " << rectStr108(ra) << "\n  reopened "
                         << axisMetrics108(firstLayer108(rd, QStringLiteral("GH108"))));
        const QString pr = tmp.filePath(QStringLiteral("GH108_after.png"));
        CHECK108(exportPng108(rpc, QStringLiteral("GH108"), pr) && sameImage108(heatPng, pr),
                 "108c-reopen: heatmap PNG differs");
    }

    if (g108Failures > 0) {
        std::cerr << "[FAIL] Suite 108: " << g108Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 108 passed.\n";
    return 0;
}
