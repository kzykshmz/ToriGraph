// Suite 118: regression checks from the 2026-11-28 review / debug campaign.
//   118a double -> int conversions clamp as double first (no UB float-cast-overflow):
//        graph.set_axis label_rotation 1e10 -> 90 (not -90), graph.add_shaded_band alpha 1e10
//        -> opaque and a string alpha is rejected, graph.set_curve_style alpha / symbol_size /
//        colour array 1e10 saturate at the top, plot.vectors / set_curve_style lw 1e10 -> 20,
//        graph.add_inset rel_x 1e10 ends at the right edge and keeps a sane normalized origin
//   118b graph.merge_graphs rejects the same graph named twice
//   118c app.batch rejects $steps[4294967296] instead of binding it to step 0
//   118d table.mask_rows rejects non-integer / out-of-range row entries
//   118e the TCP token file is 0600 inside a 0700 directory; a directory owned by another user
//        is refused (non-Windows)
#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "VectorCurve.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>

#ifndef Q_OS_WIN
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

int g118Failures = 0;

#define CHECK118(cond, msg)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;            \
            ++g118Failures;                                                                        \
        }                                                                                          \
    } while (0)

struct NonInteractiveGuard118
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard118()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard118()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard118
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard118(ApplicationWindow *w = nullptr) : win(w)
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
    ~AppWinGuard118()
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
    AppWinGuard118(const AppWinGuard118 &) = delete;
    AppWinGuard118 &operator=(const AppWinGuard118 &) = delete;
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

struct Rpc118
{
    PythonAutomationServer *srv = nullptr;
    int id = 11800;
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
    // an RPC error, or a batch whose result reports ok=false
    static bool failed(const QJsonObject &resp)
    {
        return !ok(resp) || !result(resp).value(QStringLiteral("ok")).toBool(true);
    }
    static QString text(const QJsonObject &resp)
    {
        return QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact));
    }
};

MultiLayer *findGraph118(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer") && w->name() == name)
            return static_cast<MultiLayer *>(w);
    }
    return nullptr;
}

Table *makeTable118(ApplicationWindow *app, const QString &name, int rows, int cols)
{
    Table *t = app->newTable(name, rows, cols);
    if (!t)
        return nullptr;
    for (int c = 0; c < cols; ++c)
        t->setColName(c, QStringLiteral("C%1").arg(c + 1));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c)
            t->setCell(r, c, double(r + 1) * double(c + 1));
    }
    return t;
}

// The ARGB colour of the first shaded band written by Graph::saveToString().
QString bandColor118(Graph *g)
{
    const QString s = g ? g->saveToString() : QString();
    const int at = s.indexOf(QStringLiteral("kind=\"band\" color=\""));
    if (at < 0)
        return QString();
    const int start = at + int(QStringLiteral("kind=\"band\" color=\"").size());
    return s.mid(start, s.indexOf(QLatin1Char('"'), start) - start).toLower();
}

// x of the normalized origin of the first <ToriInset line of a MultiLayer.
double insetNormX118(MultiLayer *ml)
{
    const QString saved = ml ? ml->saveToString(QString()) : QString();
    for (const QString &raw : saved.split(QLatin1Char('\n'))) {
        const QString ln = raw.trimmed();
        if (ln.startsWith(QStringLiteral("<ToriInset\t"))) {
            const QStringList parts = ln.split(QLatin1Char('\t'));
            if (parts.size() > 3)
                return parts[3].toDouble();
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace

int runSuite118RpcParamAndSdkHardening(ApplicationWindow *mw)
{
    std::cout << "[INFO] Suite 118: runSuite118RpcParamAndSdkHardening\n";
    (void)mw;
    g118Failures = 0;
    NonInteractiveGuard118 niGuard;

    {
        AppWinGuard118 app(new ApplicationWindow());
        PythonAutomationServer srv(app);
        Rpc118 rpc { &srv };

        Table *t = makeTable118(app, QStringLiteral("T118"), 8, 4);
        CHECK118(t != nullptr, "118: table");
        const QJsonObject plotResp = rpc.call(
                QStringLiteral("plot.xy"),
                QJsonObject { { "table", "T118" },
                              { "x_col", "C1" },
                              { "y_cols", QJsonArray { "C2" } },
                              { "graph_name", "G118" } });
        CHECK118(Rpc118::ok(plotResp), "118: plot.xy G118: " << Rpc118::text(plotResp).toStdString());
        MultiLayer *ml = findGraph118(app, QStringLiteral("G118"));
        Graph *g = ml ? ml->activeGraph() : nullptr;
        CHECK118(g != nullptr && g->curves() >= 1, "118: G118 graph/curve");

        // ---------------- 118a: clamp as double before converting to int ----------------
        if (g) {
            QJsonObject r = rpc.call(QStringLiteral("graph.set_axis"),
                                     QJsonObject { { "graph", "G118" },
                                                   { "axis", "x" },
                                                   { "label_rotation", 1e10 } });
            CHECK118(Rpc118::ok(r) && g->labelsRotation(QwtPlot::xBottom) == 90,
                     "118a: label_rotation 1e10 -> " << g->labelsRotation(QwtPlot::xBottom)
                                                    << " (want 90)");
            r = rpc.call(QStringLiteral("graph.set_axis"),
                         QJsonObject { { "graph", "G118" }, { "axis", "x" }, { "rotation", -1e10 } });
            CHECK118(Rpc118::ok(r) && g->labelsRotation(QwtPlot::xBottom) == -90,
                     "118a: rotation -1e10 -> " << g->labelsRotation(QwtPlot::xBottom)
                                               << " (want -90)");
            r = rpc.call(QStringLiteral("graph.set_axis"),
                         QJsonObject { { "graph", "G118" }, { "axis", "x" }, { "label_rotation", 45.4 } });
            CHECK118(Rpc118::ok(r) && g->labelsRotation(QwtPlot::xBottom) == 45,
                     "118a: label_rotation 45.4 -> " << g->labelsRotation(QwtPlot::xBottom));

            // shaded band alpha
            r = rpc.call(QStringLiteral("graph.add_shaded_band"),
                         QJsonObject { { "graph", "G118" },
                                       { "y_min", 2.0 },
                                       { "y_max", 4.0 },
                                       { "x_min", 1.0 },
                                       { "x_max", 3.0 },
                                       { "color", "#1e66b8" },
                                       { "alpha", 1e10 },
                                       { "label", "Band118" } });
            CHECK118(Rpc118::ok(r), "118a: add_shaded_band alpha 1e10: "
                                            << Rpc118::text(r).toStdString());
            CHECK118(bandColor118(g).startsWith(QStringLiteral("#ff")),
                     "118a: shaded band alpha 1e10 not opaque: " << bandColor118(g).toStdString());
            g->clearShadedBands();
            r = rpc.call(QStringLiteral("graph.add_shaded_band"),
                         QJsonObject { { "graph", "G118" },
                                       { "y_min", 2.0 },
                                       { "y_max", 4.0 },
                                       { "x_min", 1.0 },
                                       { "x_max", 3.0 },
                                       { "color", "#1e66b8" },
                                       { "alpha", -1e10 } });
            CHECK118(Rpc118::ok(r) && bandColor118(g).startsWith(QStringLiteral("#00")),
                     "118a: shaded band alpha -1e10 not transparent: "
                             << bandColor118(g).toStdString());
            g->clearShadedBands();
            r = rpc.call(QStringLiteral("graph.add_shaded_band"),
                         QJsonObject { { "graph", "G118" },
                                       { "y_min", 2.0 },
                                       { "y_max", 4.0 },
                                       { "x_min", 1.0 },
                                       { "x_max", 3.0 },
                                       { "alpha", "0.5" } });
            CHECK118(!Rpc118::ok(r), "118a: shaded band string alpha accepted");
            g->clearShadedBands();

            // inline curve style: alpha, symbol size, colour array
            r = rpc.call(QStringLiteral("graph.set_curve_style"),
                         QJsonObject { { "graph", "G118" },
                                       { "curve", 0 },
                                       { "color", "#0072b2" },
                                       { "alpha", 1e10 },
                                       { "marker", "o" },
                                       { "symbol_size", 1e300 } });
            QwtPlotCurve *c0 = g->curve(0);
            CHECK118(Rpc118::ok(r) && c0 && c0->pen().color().alpha() == 255,
                     "118a: curve alpha 1e10 -> " << (c0 ? c0->pen().color().alpha() : -1));
            CHECK118(c0 && c0->symbol().size().width() == 200,
                     "118a: symbol_size 1e300 -> " << (c0 ? c0->symbol().size().width() : -1));
            r = rpc.call(QStringLiteral("graph.set_curve_style"),
                         QJsonObject { { "graph", "G118" },
                                       { "curve", 0 },
                                       { "color", QJsonArray { 1e10, 0, -1e10 } } });
            c0 = g->curve(0);
            CHECK118(Rpc118::ok(r) && c0 && c0->pen().color().red() == 255
                             && c0->pen().color().blue() == 0,
                     "118a: colour [1e10, 0, -1e10] -> r="
                             << (c0 ? c0->pen().color().red() : -1)
                             << " b=" << (c0 ? c0->pen().color().blue() : -1));
        }

        // vectors line width
        {
            QJsonObject r = rpc.call(QStringLiteral("plot.vectors"),
                                     QJsonObject { { "table", "T118" },
                                                   { "x_col", "C1" },
                                                   { "y_col", "C2" },
                                                   { "u_col", "C3" },
                                                   { "v_col", "C4" },
                                                   { "lw", 1e10 },
                                                   { "graph_name", "V118" } });
            CHECK118(Rpc118::ok(r), "118a: plot.vectors lw 1e10: " << Rpc118::text(r).toStdString());
            MultiLayer *vml = findGraph118(app, QStringLiteral("V118"));
            Graph *vg = vml ? vml->activeGraph() : nullptr;
            VectorCurve *vc = (vg && vg->curves() > 0)
                    ? dynamic_cast<VectorCurve *>(vg->curve(vg->curves() - 1))
                    : nullptr;
            CHECK118(vc && vc->width() == 20, "118a: plot.vectors lw 1e10 -> "
                                                      << (vc ? vc->width() : -1) << " (want 20)");
            if (vc) {
                r = rpc.call(QStringLiteral("graph.set_curve_style"),
                             QJsonObject { { "graph", "V118" },
                                           { "curve", vg->curves() - 1 },
                                           { "lw", -1e10 } });
                CHECK118(Rpc118::ok(r) && vc->width() == 1,
                         "118a: set_curve_style vector lw -1e10 -> " << vc->width() << " (want 1)");
            }
        }

        // inset origin far outside the canvas
        {
            makeTable118(app, QStringLiteral("TI118"), 8, 2);
            auto mkGraph = [&](const QString &name) {
                return Rpc118::ok(rpc.call(QStringLiteral("plot.xy"),
                                           QJsonObject { { "table", "TI118" },
                                                         { "x_col", "C1" },
                                                         { "y_cols", QJsonArray { "C2" } },
                                                         { "graph_name", name } }));
            };
            CHECK118(mkGraph(QStringLiteral("IA118")) && mkGraph(QStringLiteral("IB118")),
                     "118a: inset source graphs");
            auto insetX = [&](const QString &name, double relX, double *normX) -> int {
                const QJsonObject r = rpc.call(QStringLiteral("graph.add_inset"),
                                               QJsonObject { { "graph", name },
                                                             { "rel_x", relX },
                                                             { "rel_y", 0.1 },
                                                             { "rel_w", 0.38 },
                                                             { "rel_h", 0.38 } });
                MultiLayer *iml = findGraph118(app, name);
                if (!Rpc118::ok(r) || !iml)
                    return INT_MIN;
                const int idx = Rpc118::result(r).value(QStringLiteral("inset_layer")).toInt(-1);
                const QList<Graph *> layers = iml->layersList();
                if (normX)
                    *normX = insetNormX118(iml);
                return (idx >= 0 && idx < layers.size()) ? layers[idx]->x() : INT_MIN;
            };
            double normFar = 0.0;
            const int xNear = insetX(QStringLiteral("IA118"), 0.9, nullptr);
            const int xFar = insetX(QStringLiteral("IB118"), 1e10, &normFar);
            CHECK118(xNear != INT_MIN && xFar != INT_MIN, "118a: add_inset failed");
            CHECK118(xFar >= xNear - 2,
                     "118a: inset rel_x 1e10 at x=" << xFar << ", left of rel_x 0.9 at x=" << xNear);
            CHECK118(std::isfinite(normFar) && normFar <= 3.0,
                     "118a: inset normalized origin kept unclamped: " << normFar);
        }

        // ---------------- 118b: merge_graphs de-duplicates its sources ----------------
        {
            QJsonObject r = rpc.call(QStringLiteral("graph.merge_graphs"),
                                     QJsonObject { { "graphs", QJsonArray { "G118", "G118" } } });
            CHECK118(!Rpc118::ok(r), "118b: merge_graphs accepted the same graph twice");
            r = rpc.call(QStringLiteral("graph.merge_graphs"),
                         QJsonObject { { "graphs", QJsonArray { "G118", "g118" } } });
            CHECK118(!Rpc118::ok(r), "118b: merge_graphs accepted G118 / g118");
        }

        // ---------------- 118c: $steps[N] overflowing int is rejected ----------------
        {
            QJsonArray cmds;
            cmds.append(QJsonObject { { "method", "table.set_cell" },
                                      { "params",
                                        QJsonObject { { "table", "T118" },
                                                      { "row", 0 },
                                                      { "col", "C2" },
                                                      { "value", 7.0 } } } });
            cmds.append(QJsonObject { { "method", "table.set_cell" },
                                      { "params",
                                        QJsonObject { { "table", "$steps[4294967296].table" },
                                                      { "row", 1 },
                                                      { "col", "C2" },
                                                      { "value", 8.0 } } } });
            const QJsonObject r = rpc.call(QStringLiteral("app.batch"),
                                           QJsonObject { { "commands", cmds },
                                                         { "atomic", true },
                                                         { "stop_on_error", true } });
            const QString txt = Rpc118::text(r);
            CHECK118(Rpc118::failed(r) && txt.contains(QStringLiteral("out-of-range")),
                     "118c: $steps[4294967296] not rejected: " << txt.toStdString());
        }

        // The atomic batch above failed and was rolled back by reloading the pre-batch
        // snapshot, which replaces every window: look the table up again.
        t = app->table(QStringLiteral("T118"));

        // ---------------- 118d: mask_rows validates every row entry ----------------
        {
            const QJsonValue bad[] = { QJsonValue(2.5), QJsonValue(QStringLiteral("3")),
                                       QJsonValue(1e12), QJsonValue(-1), QJsonValue(8) };
            for (const QJsonValue &b : bad) {
                const QJsonObject r = rpc.call(QStringLiteral("table.mask_rows"),
                                               QJsonObject { { "table", "T118" },
                                                             { "col", "C2" },
                                                             { "rows", QJsonArray { 1, b } } });
                CHECK118(!Rpc118::ok(r),
                         "118d: mask_rows accepted entry "
                                 << QString::fromUtf8(QJsonDocument(QJsonArray { b }).toJson(
                                                              QJsonDocument::Compact))
                                            .toStdString());
            }
            Column *c2 = t ? t->column(1) : nullptr;
            CHECK118(c2 && !c2->isMasked(1), "118d: a rejected mask_rows call masked row 1");
            const QJsonObject r = rpc.call(QStringLiteral("table.mask_rows"),
                                           QJsonObject { { "table", "T118" },
                                                         { "col", "C2" },
                                                         { "rows", QJsonArray { 1, 2 } } });
            CHECK118(Rpc118::ok(r) && c2 && c2->isMasked(1) && c2->isMasked(2),
                     "118d: valid mask_rows failed: " << Rpc118::text(r).toStdString());
            const QJsonObject rs = rpc.call(QStringLiteral("table.unmask_rows"),
                                            QJsonObject { { "table", "T118" },
                                                          { "col", "C2" },
                                                          { "start_row", 1.5 } });
            CHECK118(!Rpc118::ok(rs), "118d: unmask_rows start_row 1.5 accepted");
        }
    }

    // ---------------- 118e: owner-only TCP token file ----------------
#ifndef Q_OS_WIN
    {
        QTemporaryDir tmp(QDir::tempPath() + QStringLiteral("/torigraph_suite118_XXXXXX"));
        CHECK118(tmp.isValid(), "118e: temporary directory");
        const QByteArray prevLad = qgetenv("LOCALAPPDATA");
        const bool hadLad = qEnvironmentVariableIsSet("LOCALAPPDATA");
        const mode_t prevMask = ::umask(022);
        {
            qputenv("LOCALAPPDATA", QFile::encodeName(tmp.filePath(QStringLiteral("lad"))));
            AppWinGuard118 app(new ApplicationWindow());
            PythonAutomationServer srv(app);
            CHECK118(srv.startTcp(0, QHostAddress::LocalHost), "118e: startTcp");
            const QString rpcDir = tmp.filePath(QStringLiteral("lad/ToriGraph/rpc"));
            const QString tokPath =
                    QDir(rpcDir).filePath(QStringLiteral("tcp_%1.token").arg(srv.tcpPort()));
            struct stat st {};
            CHECK118(::stat(QFile::encodeName(tokPath).constData(), &st) == 0
                             && (st.st_mode & 0777) == 0600,
                     "118e: token file mode " << std::oct << (st.st_mode & 0777) << std::dec
                                              << " (want 0600)");
            QFile tf(tokPath);
            CHECK118(tf.open(QIODevice::ReadOnly)
                             && QString::fromUtf8(tf.readAll()) == srv.tcpAuthToken(),
                     "118e: token file content");
            struct stat dst {};
            CHECK118(::stat(QFile::encodeName(rpcDir).constData(), &dst) == 0
                             && (dst.st_mode & 0777) == 0700,
                     "118e: rpc dir mode " << std::oct << (dst.st_mode & 0777) << std::dec
                                           << " (want 0700)");
            srv.stop();
        }
        if (::geteuid() == 0) {
            // a pre-created rpc directory owned by another user is not used
            const QString foreign = tmp.filePath(QStringLiteral("foreign/ToriGraph/rpc"));
            QDir().mkpath(foreign);
            const QByteArray fb = QFile::encodeName(foreign);
            CHECK118(::chmod(fb.constData(), 0777) == 0 && ::chown(fb.constData(), 65534, 65534) == 0,
                     "118e: chown foreign dir");
            qputenv("LOCALAPPDATA", QFile::encodeName(tmp.filePath(QStringLiteral("foreign"))));
            AppWinGuard118 app(new ApplicationWindow());
            PythonAutomationServer srv(app);
            CHECK118(srv.startTcp(0, QHostAddress::LocalHost), "118e: startTcp (foreign dir)");
            const QString tokPath =
                    QDir(foreign).filePath(QStringLiteral("tcp_%1.token").arg(srv.tcpPort()));
            CHECK118(!QFile::exists(tokPath),
                     "118e: token written into a directory owned by another user");
            srv.stop();
        }
        ::umask(prevMask);
        if (hadLad)
            qputenv("LOCALAPPDATA", prevLad);
        else
            qunsetenv("LOCALAPPDATA");
    }
#endif

    if (g118Failures > 0) {
        std::cerr << "[FAIL] Suite 118: " << g118Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 118\n";
    return 0;
}
