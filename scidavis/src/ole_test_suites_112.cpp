#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "QwtHistogram.h"

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>

#include <gsl/gsl_cdf.h>

#include <cmath>
#include <limits>

// =============================================================================
// Suite 112: heatmap / contour with blocks of missing cells, histogram clone and bin centres.
//   112a a block of 3x3 or more NaN cells in a matrix (values 10..20) is not drawn as 0: the
//        colour range stays 10..20, the cells without a finite neighbour are transparent and
//        the contour lines do not draw a false ring around the hole; a single NaN cell is still
//        filled from its neighbours (default look unchanged)
//   112b app.clone_window of a histogram keeps a histogram (not a bar curve of the bins): same
//        bins (manual and automatic), follows edits of the source table, and the clone's bins
//        survive save -> reopen (they used to be read from a bar curve as garbage)
//   112c the histogram points are the bin centres: a Gauss fit of a normal sample (mean 50)
//        finds xc = 50 for bin widths 1 and 4 (it was half a bin to the left), a LogNormal fit
//        of bins starting at 0 works, graph.get_info reports the centres, the cumulative %
//        table keeps the bin edges, and the bars are still drawn from edge to edge
//   112d the X / Y axes of a heatmap and a contour map follow the matrix: after a transpose,
//        new coordinates and a rotation they show the matrix's new coordinate range (they used
//        to stay at the first range, also after save -> reopen); an axis range the user fixed
//        stays, and the axes still follow the matrix after the project is reopened
//   112e a histogram with a function curve and / or a fit curve moved before it keeps the
//        automatic Y title "Counts" ("度数") after undo -> redo and save -> reopen (the kind
//        of the title was taken from the first curve, the function curve, so it became the
//        column name "y")
//   112f closing a matrix (app.close_window) and app.undo without a window brings back its
//        heatmap and contour map with their look (palette, levels, contour pen, colour bar and
//        its title and range) at their place in the layer; app.redo takes them again, a second
//        app.undo brings them back; once the Undo steps are dropped the held spectrograms are
//        deleted with the matrix (they used to be deleted on closing, so Undo brought only the
//        matrix)
// =============================================================================

namespace {

int g112Failures = 0;

#define CHECK112(cond, msg)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;            \
            ++g112Failures;                                                                        \
        }                                                                                          \
    } while (0)

struct NonInteractiveGuard112
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard112()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard112()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard112
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard112(ApplicationWindow *w = nullptr) : win(w)
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
    ~AppWinGuard112()
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
    AppWinGuard112(const AppWinGuard112 &) = delete;
    AppWinGuard112 &operator=(const AppWinGuard112 &) = delete;
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

struct Rpc112
{
    PythonAutomationServer *srv = nullptr;
    int id = 11200;
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
            std::cerr << "[INFO] 112: " << method.toStdString() << " error: "
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
    bool makeTable(const QString &name, const QString &col, const std::vector<double> &v)
    {
        QJsonArray data;
        for (double x : v)
            data.append(x);
        if (!ok(call(QStringLiteral("table.create"),
                     QJsonObject { { "rows", int(v.size()) },
                                   { "cols", 1 },
                                   { "name", name },
                                   { "col_names", QJsonArray { col } } })))
            return false;
        return ok(call(QStringLiteral("table.set_data_dict"),
                       QJsonObject { { "table", name },
                                     { "data", QJsonObject { { col, data } } },
                                     { "column_order", QJsonArray { col } } }));
    }
    QString histogram(const QString &table, const QString &col, const QString &graph,
                      double binSize = 0.0, double binMin = 0.0, double binMax = 0.0)
    {
        QJsonObject p { { "table", table }, { "col", col }, { "graph_name", graph } };
        if (binSize > 0.0) {
            p[QStringLiteral("bin_size")] = binSize;
            p[QStringLiteral("bin_min")] = binMin;
            p[QStringLiteral("bin_max")] = binMax;
        }
        const QJsonObject r = call(QStringLiteral("plot.histogram"), p);
        return ok(r) ? result(r).value(QStringLiteral("name")).toString() : QString();
    }
    QJsonObject firstCurveInfo(const QString &graph)
    {
        const QJsonObject r =
                result(call(QStringLiteral("graph.get_info"), QJsonObject { { "graph", graph } }));
        const QJsonArray layers = r.value(QStringLiteral("layers")).toArray();
        if (layers.isEmpty())
            return QJsonObject();
        const QJsonArray curves =
                layers.first().toObject().value(QStringLiteral("curves")).toArray();
        return curves.isEmpty() ? QJsonObject() : curves.first().toObject();
    }
};

MyWidget *findWindow112(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList())
        if (w && w->name() == name)
            return w;
    return nullptr;
}

QwtHistogram *histogramOf112(ApplicationWindow *app, const QString &graph)
{
    auto *ml = qobject_cast<MultiLayer *>(findWindow112(app, graph));
    if (!ml || ml->layersList().isEmpty() || !ml->layersList().first())
        return nullptr;
    Graph *g = ml->layersList().first();
    return g->curves() > 0 ? dynamic_cast<QwtHistogram *>(g->curve(0)) : nullptr;
}

int curveTypeOf112(ApplicationWindow *app, const QString &graph)
{
    auto *ml = qobject_cast<MultiLayer *>(findWindow112(app, graph));
    if (!ml || ml->layersList().isEmpty() || !ml->layersList().first())
        return -1;
    Graph *g = ml->layersList().first();
    return g->curves() > 0 ? g->curveType(0) : -1;
}

QString countsStr112(QwtPlotCurve *c)
{
    QStringList s;
    for (int i = 0; c && i < c->dataSize(); ++i)
        s << QString::number(c->y(i));
    return s.join(QLatin1Char(' '));
}

bool sameCounts112(QwtPlotCurve *a, QwtPlotCurve *b)
{
    if (!a || !b || a->dataSize() != b->dataSize())
        return false;
    for (int i = 0; i < a->dataSize(); ++i)
        if (a->x(i) != b->x(i) || a->y(i) != b->y(i))
            return false;
    return true;
}

//! Gives the test access to the raster image and the contour lines that Spectrogram draws.
class SpectrogramProbe112 : public Spectrogram
{
public:
    explicit SpectrogramProbe112(Matrix *m) : Spectrogram(m) { }
    using Spectrogram::renderContourLines;
    using Spectrogram::renderImage;
};

struct HoleResult112
{
    double zMin = 0.0, zMax = 0.0;
    bool centreFinite = true;
    int centreAlpha = -1;
    int cornerAlpha = -1;
    double lowestLevel = 0.0;
    double worstRingDist = 0.0; // farthest lowest-level contour point from its true position
    int lowestSegments = 0;
};

// 20x20 matrix z = 10 + 10 * col / 19 over x, y = 0..19, with a size x size block of NaN
// starting at row/col 8.
HoleResult112 probeHole112(ApplicationWindow *app, int size, const QString &name)
{
    HoleResult112 r;
    Matrix *m = app->newMatrix(name, 20, 20);
    if (!m)
        return r;
    m->setCoordinates(0.0, 19.0, 0.0, 19.0);
    for (int i = 0; i < 20; ++i)
        for (int j = 0; j < 20; ++j) {
            const bool hole = i >= 8 && i < 8 + size && j >= 8 && j < 8 + size;
            m->setCell(i, j,
                       hole ? std::numeric_limits<double>::quiet_NaN()
                            : 10.0 + 10.0 * double(j) / 19.0);
        }

    SpectrogramProbe112 sp(m);
    sp.setLevelsNumber(6);
    const QwtDoubleInterval zr = sp.effectiveValueRange();
    r.zMin = zr.minValue();
    r.zMax = zr.maxValue();
    const double hc = 8.0 + 0.5 * double(size - 1); // centre of the hole
    r.centreFinite = std::isfinite(sp.data().value(hc, hc));

    QwtScaleMap xMap, yMap;
    xMap.setScaleInterval(0.0, 19.0);
    xMap.setPaintInterval(0, 190);
    yMap.setScaleInterval(0.0, 19.0);
    yMap.setPaintInterval(190, 0);
    const QImage img = sp.renderImage(xMap, yMap, QwtDoubleRect(0.0, 0.0, 19.0, 19.0));
    if (!img.isNull()) {
        auto alphaAt = [&](double x, double y) {
            const int px =
                    qBound(0, int(std::lround(x / 19.0 * (img.width() - 1))), img.width() - 1);
            const int py = qBound(0, int(std::lround((19.0 - y) / 19.0 * (img.height() - 1))),
                                  img.height() - 1);
            return qAlpha(img.pixel(px, py));
        };
        r.centreAlpha = alphaAt(hc, hc);
        r.cornerAlpha = alphaAt(2.0, 2.0);
    }

    const QwtValueList levels = sp.contourLevels();
    if (!levels.isEmpty()) {
        r.lowestLevel = *std::min_element(levels.begin(), levels.end());
        const QwtRasterData::ContourLines lines =
                sp.renderContourLines(QwtDoubleRect(0.0, 0.0, 19.0, 19.0), QSize(80, 80));
        const QPolygonF seg = lines.value(r.lowestLevel);
        r.lowestSegments = seg.size() / 2;
        const double expectedX = 19.0 * (r.lowestLevel - 10.0) / 10.0;
        for (const QPointF &p : seg)
            r.worstRingDist = std::max(r.worstRingDist, std::fabs(p.x() - expectedX));
    }
    return r;
}

std::vector<double> normalSample112(int n, double mean, double sd)
{
    std::vector<double> v(static_cast<size_t>(n));
    for (int k = 0; k < n; ++k)
        v[size_t(k)] = mean + sd * gsl_cdf_ugaussian_Pinv((double(k) + 0.5) / double(n));
    return v;
}

std::vector<double> lognormalSample112(int n, double median, double sigma)
{
    std::vector<double> v(static_cast<size_t>(n));
    for (int k = 0; k < n; ++k)
        v[size_t(k)] =
                median * std::exp(sigma * gsl_cdf_ugaussian_Pinv((double(k) + 0.5) / double(n)));
    return v;
}

} // namespace

int runSuite112HeatmapHolesAndHistogramBins(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 112: heatmap NaN blocks, histogram clone and bin "
                 "centres...\n";
    g112Failures = 0;
    NonInteractiveGuard112 niGuard;

    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "112: temp dir creation failed");
    const QString prj = tmp.filePath(QStringLiteral("hist_112.sciprj"));

    {
        AppWinGuard112 w(new ApplicationWindow());
        Rpc112 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "112: pythonAutomationServer null");

        // ---- 112a: blocks of missing cells ---------------------------------------------------
        {
            const HoleResult112 one = probeHole112(w, 1, QStringLiteral("M112One"));
            std::cout << "[INFO] 112a 1x1: z " << one.zMin << ".." << one.zMax << ", centre finite "
                      << one.centreFinite << "\n";
            CHECK112(one.zMin >= 10.0 - 1e-9 && one.zMax <= 20.0 + 1e-9,
                     "112a: 1x1 hole colour range " << one.zMin << ".." << one.zMax);
            CHECK112(one.centreFinite && one.centreAlpha == 255,
                     "112a: a single NaN cell must still be filled from its neighbours (alpha "
                             << one.centreAlpha << ")");

            for (int size : { 3, 4 }) {
                const HoleResult112 h =
                        probeHole112(w, size, QStringLiteral("M112Hole%1").arg(size));
                std::cout << "[INFO] 112a " << size << "x" << size << ": z " << h.zMin << ".."
                          << h.zMax << ", centre finite " << h.centreFinite << ", alpha centre "
                          << h.centreAlpha << " corner " << h.cornerAlpha << ", level "
                          << h.lowestLevel << " segments " << h.lowestSegments << " worst distance "
                          << h.worstRingDist << "\n";
                CHECK112(h.zMin >= 10.0 - 1e-9 && h.zMax <= 20.0 + 1e-9,
                         "112a: " << size << "x" << size << " hole stretches the colour range to "
                                  << h.zMin << ".." << h.zMax);
                CHECK112(!h.centreFinite,
                         "112a: " << size << "x" << size << " hole centre is drawn with a value");
                CHECK112(h.centreAlpha == 0 && h.cornerAlpha == 255,
                         "112a: " << size << "x" << size << " hole centre alpha " << h.centreAlpha
                                  << " (want 0), corner alpha " << h.cornerAlpha);
                CHECK112(h.lowestSegments > 0 && h.worstRingDist < 0.75,
                         "112a: " << size << "x" << size << " lowest contour level "
                                  << h.lowestLevel << " has points " << h.worstRingDist
                                  << " away from its line (false ring around the hole)");
            }

            // colour bar of a plotted heatmap (RPC)
            const QString plotted = QStringLiteral("M112Hole4");
            const QJsonObject pr = rpc.call(QStringLiteral("plot.matrix"),
                                            QJsonObject { { "matrix", plotted },
                                                          { "style", "colormap" },
                                                          { "graph_name", "G112Heat" } });
            CHECK112(Rpc112::ok(pr), "112a: plot.matrix");
            const QJsonObject info = Rpc112::result(rpc.call(
                    QStringLiteral("graph.get_info"), QJsonObject { { "graph", "G112Heat" } }));
            const QJsonArray layers = info.value(QStringLiteral("layers")).toArray();
            const QJsonArray specs = layers.isEmpty()
                    ? QJsonArray()
                    : layers.first().toObject().value(QStringLiteral("spectrograms")).toArray();
            const double zMin = specs.isEmpty()
                    ? -1.0
                    : specs.first().toObject().value(QStringLiteral("z_min")).toDouble(-1.0);
            std::cout << "[INFO] 112a plotted 4x4 hole colour bar z_min " << zMin << "\n";
            CHECK112(std::fabs(zMin - 10.0) < 1e-9, "112a: plotted colour bar starts at " << zMin);
        }

        // ---- 112b: histogram clone ----------------------------------------------------------
        {
            const std::vector<double> v = lognormalSample112(3000, 10.0, 0.5);
            CHECK_COND(rpc.makeTable(QStringLiteral("T112H"), QStringLiteral("d"), v),
                       "112b: table T112H");
            const QString gm = rpc.histogram(QStringLiteral("T112H"), QStringLiteral("d"),
                                             QStringLiteral("G112Man"), 2.0, 0.0, 60.0);
            const QString ga = rpc.histogram(QStringLiteral("T112H"), QStringLiteral("d"),
                                             QStringLiteral("G112Auto"));
            CHECK_COND(!gm.isEmpty() && !ga.isEmpty(), "112b: plot.histogram");
            for (const QString &src : { gm, ga }) {
                const QString dst = src + QStringLiteral("C");
                CHECK112(Rpc112::ok(rpc.call(QStringLiteral("app.clone_window"),
                                             QJsonObject { { "name", src }, { "new_name", dst } })),
                         "112b: clone_window " << src.toStdString());
                QwtHistogram *hs = histogramOf112(w, src);
                QwtHistogram *hc = histogramOf112(w, dst);
                std::cout << "[INFO] 112b " << dst.toStdString() << ": histogram object "
                          << (hc != nullptr) << ", curve type " << curveTypeOf112(w, dst)
                          << ", points " << (hc ? hc->dataSize() : -1) << "\n";
                CHECK112(hs && hc,
                         "112b: the clone of " << src.toStdString()
                                               << " is not a histogram object");
                if (!hs || !hc)
                    continue;
                CHECK112(hs->type() == Graph::Histogram && hc->type() == Graph::Histogram,
                         "112b: histogram curve type " << hs->type() << " / " << hc->type());
                CHECK112(curveTypeOf112(w, dst) == Graph::Histogram, "112b: clone curve type");
                CHECK112(hs->autoBinning() == hc->autoBinning() && hs->binSize() == hc->binSize()
                                 && hs->begin() == hc->begin() && hs->end() == hc->end(),
                         "112b: clone bins " << hc->autoBinning() << "/" << hc->binSize() << "/"
                                             << hc->begin() << "/" << hc->end() << " vs "
                                             << hs->autoBinning() << "/" << hs->binSize() << "/"
                                             << hs->begin() << "/" << hs->end());
                CHECK112(sameCounts112(hs, hc), "112b: clone counts differ");
            }
            QwtHistogram *hm = histogramOf112(w, gm);
            CHECK112(hm && !hm->autoBinning() && hm->dataSize() == 30,
                     "112b: manual histogram bins " << (hm ? hm->dataSize() : -1));

            // edit the source table: the clones follow it like the originals
            CHECK112(Rpc112::ok(rpc.call(QStringLiteral("table.set_cell"),
                                         QJsonObject { { "table", "T112H" },
                                                       { "row", 0 },
                                                       { "col", "d" },
                                                       { "value", 30.0 } })),
                     "112b: table.set_cell");
            for (const QString &src : { gm, ga }) {
                QwtHistogram *hs = histogramOf112(w, src);
                QwtHistogram *hc = histogramOf112(w, src + QStringLiteral("C"));
                std::cout << "[INFO] 112b after edit " << src.toStdString() << ": "
                          << (hs ? hs->dataSize() : -1) << " bins, clone "
                          << (hc ? hc->dataSize() : -1) << " bins\n";
                CHECK112(hs && hc && sameCounts112(hs, hc),
                         "112b: after the edit the clone of "
                                 << src.toStdString() << " shows " << countsStr112(hc).toStdString()
                                 << "\n  source " << countsStr112(hs).toStdString());
            }

            CHECK_COND(w->saveFolder(w->projectFolder(), prj), "112b: saving the project failed");
        }

        // ---- 112c: bin centres ---------------------------------------------------------------
        {
            const std::vector<double> v = normalSample112(5000, 50.0, 5.0);
            CHECK_COND(rpc.makeTable(QStringLiteral("T112N"), QStringLiteral("d"), v),
                       "112c: table T112N");
            for (double bw : { 1.0, 4.0 }) {
                const QString g =
                        rpc.histogram(QStringLiteral("T112N"), QStringLiteral("d"),
                                      QStringLiteral("G112N%1").arg(int(bw)), bw, 20.0, 80.0);
                CHECK112(!g.isEmpty(), "112c: plot.histogram bin " << bw);
                if (g.isEmpty())
                    continue;
                const QJsonObject ci = rpc.firstCurveInfo(g);
                const double xMin = ci.value(QStringLiteral("x_min")).toDouble(-1.0);
                const double xMax = ci.value(QStringLiteral("x_max")).toDouble(-1.0);
                const QJsonObject fr = rpc.call(QStringLiteral("analysis.fit_nonlinear"),
                                                QJsonObject { { "graph", g },
                                                              { "curve", 0 },
                                                              { "model", "Gauss" },
                                                              { "plot_curve", false } });
                const double xc = Rpc112::result(fr)
                                          .value(QStringLiteral("params"))
                                          .toObject()
                                          .value(QStringLiteral("xc"))
                                          .toDouble(-1.0);
                std::cout << "[INFO] 112c bin " << bw << ": curve x " << xMin << ".." << xMax
                          << ", Gauss xc " << xc << "\n";
                CHECK112(std::fabs(xMin - (20.0 + 0.5 * bw)) < 1e-9
                                 && std::fabs(xMax - (80.0 - 0.5 * bw)) < 1e-9,
                         "112c: bin " << bw << " curve points " << xMin << ".." << xMax
                                      << " are not the bin centres");
                CHECK112(Rpc112::ok(fr) && std::fabs(xc - 50.0) < 0.05,
                         "112c: bin " << bw << " Gauss fit centre " << xc << " (sample mean 50)");
            }

            const std::vector<double> lv = lognormalSample112(3000, 10.0, 0.5);
            CHECK_COND(rpc.makeTable(QStringLiteral("T112L"), QStringLiteral("d"), lv),
                       "112c: table T112L");
            const QString gl = rpc.histogram(QStringLiteral("T112L"), QStringLiteral("d"),
                                             QStringLiteral("G112Log"), 2.0, 0.0, 60.0);
            const QJsonObject lr = rpc.call(QStringLiteral("analysis.fit_nonlinear"),
                                            QJsonObject { { "graph", gl },
                                                          { "curve", 0 },
                                                          { "model", "LogNormal" },
                                                          { "plot_curve", false } });
            std::cout << "[INFO] 112c LogNormal fit of bins from 0: "
                      << QJsonDocument(
                                 Rpc112::result(lr).value(QStringLiteral("params")).toObject())
                                 .toJson(QJsonDocument::Compact)
                                 .toStdString()
                      << "\n";
            const QJsonValue lxc = Rpc112::result(lr)
                                           .value(QStringLiteral("params"))
                                           .toObject()
                                           .value(QStringLiteral("xc"));
            CHECK112(!gl.isEmpty() && Rpc112::ok(lr) && lxc.isDouble()
                             && std::isfinite(lxc.toDouble()),
                     "112c: LogNormal fit of a histogram with bins from 0 failed");

            // cumulative % keeps the bin edges
            Table *th = w->newTable(QStringLiteral("T112Cum"), 20, 1);
            CHECK_COND(th != nullptr, "112c: table T112Cum");
            for (int r = 0; r < 20; ++r)
                th->setCell(r, 0, r + 1);
            const ExtraPlotTypes::HistogramCumulativeResult hr =
                    ExtraPlotTypes::plotHistogramCumulative(w, th, th->colName(0), 5.0, 0.0);
            CHECK112(hr.valid && hr.upperEdges.size() == 6, "112c: histogram + cumulative failed");
            for (int i = 0; hr.valid && i < hr.upperEdges.size(); ++i)
                CHECK112(std::fabs(hr.upperEdges[i] - 5.0 * i) < 1e-9,
                         "112c: cumulative upper edge " << i << " = " << hr.upperEdges[i]);

            // the bars still span the bins: values 1.5, 2.5, 2.5, 3.5 in bins [1,2) [2,3) [3,4)
            CHECK_COND(rpc.makeTable(QStringLiteral("T112D"), QStringLiteral("d"),
                                     { 1.5, 2.5, 2.5, 3.5 }),
                       "112c: table T112D");
            const QString gd = rpc.histogram(QStringLiteral("T112D"), QStringLiteral("d"),
                                             QStringLiteral("G112Draw"), 1.0, 1.0, 4.0);
            QwtHistogram *hd = histogramOf112(w, gd);
            CHECK112(hd && hd->dataSize() == 3, "112c: draw probe histogram");
            if (hd && hd->dataSize() == 3) {
                CHECK112(std::fabs(hd->x(0) - 1.5) < 1e-12 && std::fabs(hd->x(2) - 3.5) < 1e-12,
                         "112c: bin centres " << hd->x(0) << " .. " << hd->x(2));
                const QwtDoubleRect br = hd->boundingRect();
                CHECK112(std::fabs(br.left() - 0.85) < 1e-9 && std::fabs(br.right() - 4.15) < 1e-9,
                         "112c: histogram bounding box " << br.left() << ".." << br.right()
                                                         << " (want 0.85..4.15)");
                QImage img(500, 100, QImage::Format_ARGB32);
                img.fill(Qt::white);
                QwtScaleMap xMap, yMap;
                xMap.setScaleInterval(0.0, 5.0);
                xMap.setPaintInterval(0, 500);
                yMap.setScaleInterval(0.0, 2.0);
                yMap.setPaintInterval(100, 0);
                hd->setPen(QPen(Qt::NoPen));
                hd->setBrush(QBrush(Qt::black));
                {
                    QPainter p(&img);
                    static_cast<QwtPlotCurve *>(hd)->draw(&p, xMap, yMap, 0, -1);
                }
                auto dark = [&](int px) { return qGray(img.pixel(px, 98)) < 128; };
                std::cout << "[INFO] 112c bars at x 0.95/1.05/3.95/4.05: " << dark(95) << dark(105)
                          << dark(395) << dark(405) << "\n";
                CHECK112(!dark(95) && dark(105) && dark(395) && !dark(405),
                         "112c: histogram bars no longer span [1, 4]");
            }
        }
    }

    // ---- reopen: the clone's bins ------------------------------------------------------------
    {
        AppWinGuard112 rd(new ApplicationWindow());
        CHECK_COND(rd->loadProject(prj), "112b: reopening the project failed");
        QCoreApplication::processEvents();
        for (const QString &name : { QStringLiteral("G112Man"), QStringLiteral("G112ManC"),
                                     QStringLiteral("G112Auto"), QStringLiteral("G112AutoC") }) {
            QwtHistogram *h = histogramOf112(rd, name);
            std::cout << "[INFO] 112b reopened " << name.toStdString() << ": "
                      << (h ? h->dataSize() : -1) << " bins, binning "
                      << (h ? h->autoBinning() : -1) << "/" << (h ? h->binSize() : 0.0) << "/"
                      << (h ? h->begin() : 0.0) << "/" << (h ? h->end() : 0.0) << "\n";
            CHECK112(h != nullptr, "112b: reopened " << name.toStdString() << " has no histogram");
        }
        QwtHistogram *a = histogramOf112(rd, QStringLiteral("G112Man"));
        QwtHistogram *b = histogramOf112(rd, QStringLiteral("G112ManC"));
        CHECK112(b && !b->autoBinning() && b->binSize() == 2.0 && b->begin() == 0.0
                         && b->end() == 60.0 && b->dataSize() == 30,
                 "112b: reopened clone lost its bins");
        CHECK112(a && b && sameCounts112(a, b), "112b: reopened clone counts differ");
        QwtHistogram *c = histogramOf112(rd, QStringLiteral("G112Auto"));
        QwtHistogram *d = histogramOf112(rd, QStringLiteral("G112AutoC"));
        CHECK112(c && d && d->autoBinning() && sameCounts112(c, d),
                 "112b: reopened automatic-bin clone differs");
    }

    // ---- 112d: heatmap / contour X/Y axes follow the matrix coordinates ----------------------
    {
        const QString prjD = tmp.filePath(QStringLiteral("axes_112d.sciprj"));
        auto axisRange = [](ApplicationWindow *app, const QString &graph, int axis) {
            auto *ml = qobject_cast<MultiLayer *>(findWindow112(app, graph));
            Graph *g = (ml && !ml->layersList().isEmpty()) ? ml->layersList().first() : nullptr;
            const QwtScaleDiv *div = g ? g->plotWidget()->axisScaleDiv(axis) : nullptr;
            if (!div)
                return std::make_pair(std::nan(""), std::nan(""));
            return std::make_pair(qMin(div->lowerBound(), div->upperBound()),
                                  qMax(div->lowerBound(), div->upperBound()));
        };
        auto closeTo = [](double a, double b) {
            return std::fabs(a - b) <= 1e-9 * (1.0 + std::fabs(b));
        };
        // checks that the graph's X / Y axes show x0..x1 / y0..y1
        auto expectAxes = [&](ApplicationWindow *app, const QString &graph, double x0, double x1,
                              double y0, double y1, const char *when) {
            const auto x = axisRange(app, graph, QwtPlot::xBottom);
            const auto y = axisRange(app, graph, QwtPlot::yLeft);
            std::cout << "[INFO] 112d " << when << " " << graph.toStdString() << ": x " << x.first
                      << ".." << x.second << ", y " << y.first << ".." << y.second << "\n";
            CHECK112(closeTo(x.first, x0) && closeTo(x.second, x1),
                     "112d " << when << ": " << graph.toStdString() << " X axis " << x.first
                             << ".." << x.second << ", expected " << x0 << ".." << x1);
            CHECK112(closeTo(y.first, y0) && closeTo(y.second, y1),
                     "112d " << when << ": " << graph.toStdString() << " Y axis " << y.first
                             << ".." << y.second << ", expected " << y0 << ".." << y1);
        };
        auto matrixExtent = [](const QJsonObject &info) {
            return std::vector<double> { info.value(QStringLiteral("x_min")).toDouble(),
                                         info.value(QStringLiteral("x_max")).toDouble(),
                                         info.value(QStringLiteral("y_min")).toDouble(),
                                         info.value(QStringLiteral("y_max")).toDouble() };
        };
        auto setCoords = [](Rpc112 &rpc, double x0, double x1, double y0, double y1) {
            return Rpc112::ok(rpc.call(QStringLiteral("matrix.set_coordinates"),
                                       QJsonObject { { "matrix", "M112D" },
                                                     { "x_min", x0 },
                                                     { "x_max", x1 },
                                                     { "y_min", y0 },
                                                     { "y_max", y1 } }));
        };
        {
            AppWinGuard112 w(new ApplicationWindow());
            Rpc112 rpc;
            rpc.srv = w->pythonAutomationServer();
            CHECK_COND(rpc.srv != nullptr, "112d: pythonAutomationServer null");
            CHECK112(Rpc112::ok(rpc.call(QStringLiteral("matrix.create"),
                                         QJsonObject { { "rows", 20 },
                                                       { "cols", 30 },
                                                       { "name", "M112D" },
                                                       { "x_min", -3.0 },
                                                       { "x_max", 3.0 },
                                                       { "y_min", -2.0 },
                                                       { "y_max", 2.0 } })),
                     "112d: matrix.create");
            CHECK112(Rpc112::ok(rpc.call(QStringLiteral("matrix.set_formula"),
                                         QJsonObject { { "matrix", "M112D" },
                                                       { "formula", "exp(-((x-1)^2+y^2))" } })),
                     "112d: matrix.set_formula");
            // a heatmap, a contour map and a heatmap whose X axis the user fixed at 10..20
            const std::pair<const char *, const char *> plots[] = { { "G112D", "colormap" },
                                                                    { "G112DC", "contour" },
                                                                    { "G112DL", "colormap" } };
            for (const auto &gs : plots)
                CHECK112(Rpc112::ok(rpc.call(QStringLiteral("plot.matrix"),
                                             QJsonObject { { "matrix", "M112D" },
                                                           { "style", gs.second },
                                                           { "graph_name", gs.first } })),
                         "112d: plot.matrix " << gs.first);
            CHECK112(Rpc112::ok(rpc.call(QStringLiteral("graph.set_axis"),
                                         QJsonObject { { "graph", "G112DL" },
                                                       { "axis", "x" },
                                                       { "min", 10.0 },
                                                       { "max", 20.0 } })),
                     "112d: graph.set_axis");
            expectAxes(w, QStringLiteral("G112D"), -3, 3, -2, 2, "initial");
            expectAxes(w, QStringLiteral("G112DC"), -3, 3, -2, 2, "initial");
            expectAxes(w, QStringLiteral("G112DL"), 10, 20, -2, 2, "initial");

            // transpose: the X and Y coordinates swap
            const std::vector<double> t = matrixExtent(Rpc112::result(rpc.call(
                    QStringLiteral("matrix.transpose"), QJsonObject { { "matrix", "M112D" } })));
            std::cout << "[INFO] 112d transposed matrix: x " << t[0] << ".." << t[1] << ", y "
                      << t[2] << ".." << t[3] << "\n";
            expectAxes(w, QStringLiteral("G112D"), t[0], t[1], t[2], t[3], "after transpose");
            expectAxes(w, QStringLiteral("G112DC"), t[0], t[1], t[2], t[3], "after transpose");
            expectAxes(w, QStringLiteral("G112DL"), 10, 20, t[2], t[3], "after transpose");

            // new coordinates
            CHECK112(setCoords(rpc, 0, 100, 0, 50), "112d: matrix.set_coordinates");
            expectAxes(w, QStringLiteral("G112D"), 0, 100, 0, 50, "after set_coordinates");
            expectAxes(w, QStringLiteral("G112DC"), 0, 100, 0, 50, "after set_coordinates");
            expectAxes(w, QStringLiteral("G112DL"), 10, 20, 0, 50, "after set_coordinates");

            // a rotation changes the number of rows and columns
            CHECK112(Rpc112::ok(rpc.call(QStringLiteral("matrix.flip"),
                                         QJsonObject { { "matrix", "M112D" },
                                                       { "mode", "rot90_cw" } })),
                     "112d: matrix.flip rot90_cw");
            const std::vector<double> r = matrixExtent(Rpc112::result(rpc.call(
                    QStringLiteral("matrix.get_info"), QJsonObject { { "matrix", "M112D" } })));
            expectAxes(w, QStringLiteral("G112D"), r[0], r[1], r[2], r[3], "after rot90_cw");
            CHECK112(setCoords(rpc, 0, 100, 0, 50), "112d: matrix.set_coordinates again");
            expectAxes(w, QStringLiteral("G112D"), 0, 100, 0, 50, "before saving");
            CHECK_COND(w->saveFolder(w->projectFolder(), prjD), "112d: saving the project failed");
        }
        {
            AppWinGuard112 rd(new ApplicationWindow());
            CHECK_COND(rd->loadProject(prjD), "112d: reopening the project failed");
            QCoreApplication::processEvents();
            expectAxes(rd, QStringLiteral("G112D"), 0, 100, 0, 50, "reopened");
            expectAxes(rd, QStringLiteral("G112DC"), 0, 100, 0, 50, "reopened");
            expectAxes(rd, QStringLiteral("G112DL"), 10, 20, 0, 50, "reopened");
            // after reopening the axes still follow the matrix and the fixed X axis stays
            Rpc112 rpc;
            rpc.srv = rd->pythonAutomationServer();
            CHECK112(setCoords(rpc, 5, 6, -1, 1), "112d: matrix.set_coordinates after reopening");
            expectAxes(rd, QStringLiteral("G112D"), 5, 6, -1, 1, "reopened, new coordinates");
            expectAxes(rd, QStringLiteral("G112DC"), 5, 6, -1, 1, "reopened, new coordinates");
            expectAxes(rd, QStringLiteral("G112DL"), 10, 20, -1, 1, "reopened, new coordinates");
        }
    }

    // ---- 112e: the Y title "Counts" of a histogram with a function / fit curve before it -----
    {
        const QString prjE = tmp.filePath(QStringLiteral("histtitle_112e.sciprj"));
        auto yTitle = [](Rpc112 &rpc, const QString &graph) {
            const QJsonObject r = Rpc112::result(
                    rpc.call(QStringLiteral("graph.get_info"), QJsonObject { { "graph", graph } }));
            return r.value(QStringLiteral("layers"))
                    .toArray()
                    .first()
                    .toObject()
                    .value(QStringLiteral("axes"))
                    .toObject()
                    .value(QStringLiteral("y"))
                    .toObject()
                    .value(QStringLiteral("title"))
                    .toString();
        };
        const QString counts = Graph::tr("Counts"); // 日本語では「度数」
        const QStringList graphs { QStringLiteral("G112EF"), QStringLiteral("G112EG"),
                                   QStringLiteral("G112EB") };
        {
            AppWinGuard112 w(new ApplicationWindow());
            Rpc112 rpc;
            rpc.srv = w->pythonAutomationServer();
            CHECK_COND(rpc.srv != nullptr, "112e: pythonAutomationServer null");
            CHECK_COND(rpc.makeTable(QStringLiteral("T112E"), QStringLiteral("y"),
                                     normalSample112(200, 5.0, 1.0)),
                       "112e: table T112E");
            for (const QString &gn : graphs) {
                CHECK112(!rpc.histogram(QStringLiteral("T112E"), QStringLiteral("y"), gn).isEmpty(),
                         "112e: plot.histogram " << gn.toStdString());
                const QString t0 = yTitle(rpc, gn);
                CHECK112(t0 == counts,
                         "112e: " << gn.toStdString() << " initial Y title '" << t0.toStdString()
                                  << "'");
                // G112EF: a function curve, G112EG: a Gauss fit (function curve), G112EB: both
                if (gn != QStringLiteral("G112EG"))
                    CHECK112(Rpc112::ok(rpc.call(QStringLiteral("plot.function"),
                                                 QJsonObject { { "formula", "40*exp(-(x-5)^2/2)" },
                                                               { "x_min", 1.0 },
                                                               { "x_max", 9.0 },
                                                               { "graph", gn } })),
                             "112e: plot.function " << gn.toStdString());
                if (gn != QStringLiteral("G112EF"))
                    CHECK112(Rpc112::ok(rpc.call(QStringLiteral("analysis.fit_nonlinear"),
                                                 QJsonObject { { "graph", gn },
                                                               { "curve", 0 },
                                                               { "model", "Gauss" } })),
                             "112e: analysis.fit_nonlinear " << gn.toStdString());
                // the histogram goes to the end of the list, an auxiliary curve comes first
                CHECK112(Rpc112::ok(rpc.call(QStringLiteral("graph.move_curve"),
                                             QJsonObject { { "graph", gn },
                                                           { "curve", 0 },
                                                           { "direction", "top" } })),
                         "112e: graph.move_curve " << gn.toStdString());
                CHECK112(curveTypeOf112(w, gn) != Graph::Histogram,
                         "112e: " << gn.toStdString() << " still has the histogram first");
                const QString t1 = yTitle(rpc, gn);
                CHECK112(t1 == counts,
                         "112e: " << gn.toStdString() << " Y title after the move '"
                                  << t1.toStdString() << "'");
                CHECK112(Rpc112::ok(rpc.call(QStringLiteral("app.undo"),
                                             QJsonObject { { "graph", gn } }))
                                 && Rpc112::ok(rpc.call(QStringLiteral("app.redo"),
                                                        QJsonObject { { "graph", gn } })),
                         "112e: undo / redo " << gn.toStdString());
                const QString t2 = yTitle(rpc, gn);
                std::cout << "[INFO] 112e " << gn.toStdString() << ": Y title '"
                          << t0.toStdString() << "' -> moved '" << t1.toStdString()
                          << "' -> undo/redo '" << t2.toStdString() << "'\n";
                CHECK112(t2 == counts,
                         "112e: " << gn.toStdString() << " Y title after undo / redo '"
                                  << t2.toStdString() << "'");
            }
            CHECK_COND(w->saveFolder(w->projectFolder(), prjE), "112e: saving the project failed");
        }
        {
            AppWinGuard112 rd(new ApplicationWindow());
            CHECK_COND(rd->loadProject(prjE), "112e: reopening the project failed");
            QCoreApplication::processEvents();
            Rpc112 rpc;
            rpc.srv = rd->pythonAutomationServer();
            for (const QString &gn : graphs) {
                const QString t = yTitle(rpc, gn);
                std::cout << "[INFO] 112e reopened " << gn.toStdString() << ": Y title '"
                          << t.toStdString() << "'\n";
                CHECK112(t == counts,
                         "112e: reopened " << gn.toStdString() << " Y title '" << t.toStdString()
                                           << "'");
            }
        }
    }

    // ---- 112f: closing a matrix keeps its heatmap / contour for Undo -----------------------
    {
        AppWinGuard112 w(new ApplicationWindow());
        Rpc112 rpc;
        rpc.srv = w->pythonAutomationServer();
        CHECK_COND(rpc.srv != nullptr, "112f: pythonAutomationServer null");
        CHECK112(Rpc112::ok(rpc.call(
                         QStringLiteral("matrix.create"),
                         QJsonObject { { "rows", 10 }, { "cols", 10 }, { "name", "M112E" } })),
                 "112f: matrix.create");
        CHECK112(Rpc112::ok(rpc.call(QStringLiteral("matrix.set_formula"),
                                     QJsonObject { { "matrix", "M112E" }, { "formula", "x+y" } })),
                 "112f: matrix.set_formula");
        CHECK112(Rpc112::ok(rpc.call(
                         QStringLiteral("plot.matrix"),
                         QJsonObject { { "matrix", "M112E" }, { "graph_name", "G112EH" } })),
                 "112f: plot.matrix heatmap");
        CHECK112(Rpc112::ok(rpc.call(QStringLiteral("plot.matrix"),
                                     QJsonObject { { "matrix", "M112E" },
                                                   { "style", "contour" },
                                                   { "graph_name", "G112EC" } })),
                 "112f: plot.matrix contour");
        // a line curve after the heatmap (its place in the layer must stay)
        CHECK112(Rpc112::ok(rpc.call(QStringLiteral("table.create"),
                                     QJsonObject { { "rows", 3 },
                                                   { "cols", 2 },
                                                   { "name", "T112E" },
                                                   { "col_names", QJsonArray { "x", "y" } } }))
                         && Rpc112::ok(rpc.call(
                                 QStringLiteral("table.set_data_dict"),
                                 QJsonObject { { "table", "T112E" },
                                               { "data",
                                                 QJsonObject { { "x", QJsonArray { 1, 2, 3 } },
                                                               { "y", QJsonArray { 1, 4, 2 } } } },
                                               { "column_order", QJsonArray { "x", "y" } } })),
                 "112f: table T112E");
        CHECK112(Rpc112::ok(rpc.call(QStringLiteral("plot.add_curve"),
                                     QJsonObject { { "graph", "G112EH" },
                                                   { "table", "T112E" },
                                                   { "x_col", "x" },
                                                   { "y_col", "y" } })),
                 "112f: plot.add_curve");
        auto layerOf = [&](const QString &graph) -> Graph * {
            auto *ml = qobject_cast<MultiLayer *>(findWindow112(w, graph));
            return (ml && !ml->layersList().isEmpty()) ? ml->layersList().first() : nullptr;
        };
        Graph *gh = layerOf(QStringLiteral("G112EH"));
        Graph *gc = layerOf(QStringLiteral("G112EC"));
        CHECK_COND(gh && gc, "112f: graphs");
        // a look of its own: palette, reversed, levels, colour bar title, contour pen
        Spectrogram *sh = nullptr, *sc = nullptr;
        for (int i = 0; i < gh->curves(); ++i)
            if (!sh)
                sh = dynamic_cast<Spectrogram *>(gh->plotItem(i));
        for (int i = 0; i < gc->curves(); ++i)
            if (!sc)
                sc = dynamic_cast<Spectrogram *>(gc->plotItem(i));
        CHECK_COND(sh && sc, "112f: spectrograms");
        sh->setColorMapPreset(Spectrogram::PresetMagma, true);
        sh->setLevelsNumber(7);
        sh->setColorBarTitle(QStringLiteral("z 112f"));
        sc->setLevelsNumber(9);
        sc->setDefaultContourPen(QPen(QColor(200, 30, 30), 2.0));
        gh->replot();
        gc->replot();
        // the layer as the user sees it: each item (type, spectrogram look), colour bar, legend
        auto sig = [](Graph *g) {
            std::string s;
            for (int i = 0; i < g->curves(); ++i) {
                QwtPlotItem *it = g->plotItem(i);
                s += std::to_string(g->curveType(i)) + ":";
                if (auto *sp = dynamic_cast<Spectrogram *>(it)) {
                    s += "sp("
                            + (sp->matrix() ? sp->matrix()->name().toStdString() : std::string("-"))
                            + ",p" + std::to_string(int(sp->colorMapPreset())) + ",r"
                            + std::to_string(int(sp->reversePalette())) + ",l"
                            + std::to_string(sp->levels()) + ",img"
                            + std::to_string(
                                    int(sp->testDisplayMode(QwtPlotSpectrogram::ImageMode)))
                            + ",cnt"
                            + std::to_string(
                                    int(sp->testDisplayMode(QwtPlotSpectrogram::ContourMode)))
                            + ",pen" + sp->defaultContourPen().color().name().toStdString() + ",bar"
                            + std::to_string(int(sp->hasColorScale())) + ",t"
                            + sp->colorBarTitle().toStdString() + ")";
                } else if (it) {
                    s += it->title().text().toStdString();
                }
                s += ";";
            }
            const QwtScaleDiv *zd = g->plotWidget()->axisScaleDiv(QwtPlot::yRight);
            if (zd)
                s += "z" + std::to_string(zd->lowerBound()) + ".."
                        + std::to_string(zd->upperBound());
            return s + "|" + (g->legend() ? g->legend()->text().toStdString() : std::string());
        };
        auto spCount = [](Graph *g) {
            int n = 0;
            for (int i = 0; i < g->curves(); ++i)
                n += dynamic_cast<Spectrogram *>(g->plotItem(i)) ? 1 : 0;
            return n;
        };
        const std::string h0 = sig(gh), c0 = sig(gc);
        std::cout << "[INFO] 112f before: " << h0 << " / " << c0 << "\n";
        CHECK112(h0.find(",bar1,") != std::string::npos,
                 "112f: the heatmap has no colour bar: " << h0);
        Matrix *m = w->matrix(QStringLiteral("M112E"));
        CHECK_COND(m && m->d_future_matrix && m->d_future_matrix->undoStack(),
                   "112f: matrix M112E");
        QUndoStack *ps = m->d_future_matrix->undoStack();
        QPointer<future::Matrix> fm(m->d_future_matrix.data());

        auto close = [&](const char *when) {
            CHECK112(Rpc112::ok(rpc.call(QStringLiteral("app.close_window"),
                                         QJsonObject { { "name", "M112E" } })),
                     "112f: app.close_window " << when);
            std::cout << "[INFO] 112f closed (" << when << "): " << sig(gh) << " / " << sig(gc)
                      << "\n";
            CHECK112(w->matrix(QStringLiteral("M112E")) == nullptr && spCount(gh) == 0
                             && spCount(gc) == 0,
                     "112f: closing M112E (" << when << ") left a spectrogram: " << sig(gh) << " / "
                                             << sig(gc));
        };
        auto expectBack = [&](const char *when) {
            const std::string h = sig(gh), c = sig(gc);
            std::cout << "[INFO] 112f " << when << ": " << h << " / " << c << "\n";
            CHECK112(w->matrix(QStringLiteral("M112E")) != nullptr,
                     "112f: " << when << ": no matrix");
            CHECK112(h == h0 && c == c0,
                     "112f: " << when << ": " << h << " / " << c << " expected " << h0 << " / "
                              << c0);
            CHECK112(spCount(gh) == 1 && gh->plotItem(0) == sh && spCount(gc) == 1
                             && gc->plotItem(0) == sc,
                     "112f: " << when << ": not the spectrograms that were held");
        };
        close("first");
        CHECK112(Rpc112::ok(rpc.call(QStringLiteral("app.undo"), QJsonObject {})),
                 "112f: app.undo");
        expectBack("after app.undo");
        CHECK112(Rpc112::ok(rpc.call(QStringLiteral("app.redo"), QJsonObject {})),
                 "112f: app.redo");
        std::cout << "[INFO] 112f after app.redo: " << sig(gh) << " / " << sig(gc) << "\n";
        CHECK112(w->matrix(QStringLiteral("M112E")) == nullptr && spCount(gh) == 0
                         && spCount(gc) == 0,
                 "112f: app.redo did not close M112E with its spectrograms: " << sig(gh));
        CHECK112(Rpc112::ok(rpc.call(QStringLiteral("app.undo"), QJsonObject {})),
                 "112f: app.undo 2");
        expectBack("after the second app.undo");
        gh->replot();
        gc->replot();

        // closed again and the Undo steps dropped: the held spectrograms go with the matrix
        close("second");
        QPointer<QObject> held = fm
                ? fm->findChild<QObject *>(QStringLiteral("_tg_held_removed_curves"),
                                           Qt::FindDirectChildrenOnly)
                : nullptr;
        CHECK112(!held.isNull(), "112f: the closed matrix does not hold its spectrograms");
        ps->clear();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        std::cout << "[INFO] 112f after clearing the Undo stack: matrix "
                  << (fm ? "alive" : "deleted") << ", holder " << (held ? "alive" : "deleted")
                  << "\n";
        CHECK112(fm.isNull() && held.isNull(),
                 "112f: clearing the Undo stack kept the held spectrograms");
        CHECK112(spCount(gh) == 0 && spCount(gc) == 0 && gh->curves() == 1,
                 "112f: after clearing the Undo stack: " << sig(gh));
        gh->replot();
        gc->replot();
    }

    if (g112Failures > 0) {
        std::cerr << "[FAIL] Suite 112: " << g112Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 112 passed.\n";
    return 0;
}
