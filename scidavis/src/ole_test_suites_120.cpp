// Suite 120: regressions found by the 2026-11-28 Linux ASan+UBSan fuzzing campaign
// (scripts/fuzz/gen_corpus.py + rpc_fuzz.py).
//
//   120a every tab-separated line of the graph / 3D sections of test/testProject.sciprj and
//        test/3dplot.sciprj cut to its keyword, cut to one field, or given INT_MIN / 99999 in
//        its first numeric field: loading must not crash or trip UBSan (Grid::load, enableAxes,
//        setAxesColors/NumColors, setAxesTitlesAlignment, setLabelsNumericFormat,
//        SymbolBox::style, CurveStyle / CaptionPolicy / TicksStyle enums, layout overflow,
//        axis baseline, 3D mesh line width)
//   120b ArrowMarker starts without a start arrow head (d_start_arrow was uninitialized) and
//        keeps finite pixel positions on a tiny axis range
//   120c plot.xy of 300 columns: one auto axis title / updatePlot per bulk insert (was O(N^3))
//   120d app.import_opj of test/Histo.opj fills its sheets without a formula recalculation per
//        cell (took minutes)
#include "ole_test_common.h"

#include "ArrowMarker.h"

namespace {

int g120Failures = 0;

#define CHECK120(cond, msg)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;            \
            ++g120Failures;                                                                        \
        }                                                                                          \
    } while (0)

struct NonInteractiveGuard120
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard120()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard120()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard120
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard120(ApplicationWindow *w) : win(w)
    {
        win->setHeadlessMode(true);
        win->applyUserSettings();
        win->confirmCloseTable = false;
        win->confirmCloseMatrix = false;
        win->confirmClosePlot2D = false;
        win->confirmClosePlot3D = false;
        win->confirmCloseFolder = false;
        win->confirmCloseNotes = false;
    }
    ~AppWinGuard120()
    {
        if (win) {
            win->savedProject();
            win->setOleEmbeddedMode(true);
            delete win;
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
    }
    ApplicationWindow *operator->() const { return win; }
    operator ApplicationWindow *() const { return win; }
};

struct Rpc120
{
    PythonAutomationServer *srv = nullptr;
    int id = 12000;
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
    static bool ok(const QJsonObject &r) { return !r.contains(QStringLiteral("error")); }
    static QJsonObject result(const QJsonObject &r) { return r.value(QStringLiteral("result")).toObject(); }
};

QString sourceFile120(const QString &rel)
{
#ifdef TORIGRAPH_SOURCE_DIR
    return QDir(QStringLiteral(TORIGRAPH_SOURCE_DIR)).filePath(rel);
#else
    return QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).filePath(rel);
#endif
}

// Line variants like scripts/fuzz/gen_corpus.py gen_project_line_sweep().
QList<QPair<QString, QString>> lineSweep120(const QString &text)
{
    QList<QPair<QString, QString>> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    QSet<QString> seen;
    bool inWindow = false;
    for (int i = 0; i < lines.size(); ++i) {
        const QString &ln = lines.at(i);
        if (ln.startsWith(QLatin1String("<multiLayer>")) || ln.startsWith(QLatin1String("<SurfacePlot>"))
            || ln.startsWith(QLatin1String("<matrix>")) || ln.startsWith(QLatin1String("<note>")))
            inWindow = true;
        if (!inWindow || !ln.contains(QLatin1Char('\t')) || ln.startsWith(QLatin1String("<table")))
            continue;
        const QStringList f = ln.split(QLatin1Char('\t'));
        const QString key = f.first();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        QList<QPair<QString, QString>> vars;
        vars << qMakePair(QStringLiteral("cut1"), f.first());
        vars << qMakePair(QStringLiteral("cut2"), QStringList(f.mid(0, 2)).join(QLatin1Char('\t')));
        for (int k = 1; k < f.size(); ++k) {
            bool isInt = false;
            f.at(k).toLongLong(&isInt);
            if (!isInt)
                continue;
            for (const QString &v : { QStringLiteral("-2147483648"), QStringLiteral("99999") }) {
                QStringList g = f;
                g[k] = v;
                vars << qMakePair(v, g.join(QLatin1Char('\t')));
            }
            break;
        }
        for (const auto &v : vars) {
            QStringList m = lines;
            m[i] = v.second;
            out << qMakePair(key + QLatin1Char('/') + v.first, m.join(QLatin1Char('\n')));
        }
    }
    return out;
}

} // namespace

int runSuite120FuzzCampaignRegressions(ApplicationWindow *mw)
{
    std::cout << "[INFO] Suite 120: runSuite120FuzzCampaignRegressions\n";
    (void)mw;
    g120Failures = 0;
    NonInteractiveGuard120 niGuard;
    QTemporaryDir tmp;
    CHECK120(tmp.isValid(), "120: temp dir");

    // ---------------- 120a: damaged project lines ----------------
    {
        int loaded = 0;
        for (const QString &seed : { QStringLiteral("test/testProject.sciprj"),
                                     QStringLiteral("test/3dplot.sciprj") }) {
            QFile f(sourceFile120(seed));
            CHECK120(f.open(QIODevice::ReadOnly), "120a: cannot read " << seed.toStdString());
            if (!f.isOpen())
                continue;
            const QString text = QString::fromUtf8(f.readAll());
            const auto variants = lineSweep120(text);
            CHECK120(variants.size() >= 20, "120a: too few line variants for " << seed.toStdString());
            for (const auto &v : variants) {
                const QString path = tmp.filePath(QStringLiteral("sweep120.sciprj"));
                QFile out(path);
                if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    continue;
                out.write(v.second.toUtf8());
                out.close();
                AppWinGuard120 app(new ApplicationWindow());
                app->loadProject(path); // any result; a crash / UBSan report ends the run
                for (MyWidget *w : app->windowsList()) {
                    if (auto *ml = qobject_cast<MultiLayer *>(w)) {
                        for (Graph *g : ml->layersList())
                            g->replot();
                    }
                }
                QCoreApplication::processEvents();
                ++loaded;
            }
        }
        std::cout << "[INFO] 120a: " << loaded << " damaged-line projects loaded without a crash\n";
    }

    // ---------------- 120b: ArrowMarker ----------------
    {
        ArrowMarker a;
        CHECK120(!a.hasStartArrow(), "120b: a new arrow must not draw a start arrow head");
        CHECK120(a.hasEndArrow(), "120b: a new arrow draws its end arrow head");

        AppWinGuard120 app(new ApplicationWindow());
        MultiLayer *ml = app->newGraph(QStringLiteral("G120b"));
        Graph *g = ml ? ml->activeGraph() : nullptr;
        CHECK120(g != nullptr, "120b: graph");
        if (g) {
            g->setScale(QwtPlot::xBottom, 0.0, 1e-16);
            g->setScale(QwtPlot::yLeft, 0.0, 1e-16);
            auto *arrow = new ArrowMarker();
            arrow->drawEndArrow(false);
            g->addArrow(arrow); // takes a copy
            QCoreApplication::processEvents();
            // a far-away line on a 1e-16 wide scale: pixel positions overflowed int (UB)
            auto *far = new ArrowMarker();
            far->setBoundingRect(0.0, 0.0, 1.0, 1.0); // 1e16 canvas widths away
            g->addArrow(far);
            delete arrow;
            delete far;
            g->replot();
        }
    }

    // ---------------- 120c: bulk plot ----------------
    {
        AppWinGuard120 app(new ApplicationWindow());
        const int n = 300;
        Table *t = app->newTable(QStringLiteral("T120c"), 3, n);
        CHECK120(t != nullptr, "120c: table");
        if (t) {
            for (int c = 0; c < n; ++c) {
                for (int r = 0; r < 3; ++r)
                    t->setCell(r, c, double(r + c));
            }
            QStringList ys;
            for (int c = 1; c < n; ++c)
                ys << t->colName(c);
            QElapsedTimer timer;
            timer.start();
            MultiLayer *ml = app->multilayerPlot(t, ys, Graph::LineSymbols);
            const qint64 ms = timer.elapsed();
            std::cout << "[INFO] 120c: plotting " << ys.size() << " curves took " << ms << " ms\n";
            Graph *g = ml ? ml->activeGraph() : nullptr;
            CHECK120(g && g->curves() == n - 1, "120c: expected " << n - 1 << " curves");
            CHECK120(ms < 8000 * kTestTimingScale, "120c: plotting " << n - 1 << " curves took " << ms << " ms");
            if (g) {
                CHECK120(!g->axisTitle(QwtPlot::xBottom).trimmed().isEmpty()
                                 && !g->axisTitle(QwtPlot::yLeft).trimmed().isEmpty(),
                         "120c: auto axis titles missing after the bulk insert");
            }
        }
    }

    // ---------------- 120d: Origin .opj import speed ----------------
#ifdef ORIGIN_IMPORT
    {
        const QString histo = sourceFile120(QStringLiteral("test/Histo.opj"));
        if (QFileInfo::exists(histo)) {
            AppWinGuard120 app(new ApplicationWindow());
            PythonAutomationServer srv(app);
            Rpc120 rpc{ &srv };
            QElapsedTimer timer;
            timer.start();
            const QJsonObject r = rpc.call(QStringLiteral("app.import_opj"),
                                           QJsonObject{ { "path", histo } });
            const qint64 ms = timer.elapsed();
            std::cout << "[INFO] 120d: importing Histo.opj took " << ms << " ms\n";
            CHECK120(Rpc120::ok(r) && Rpc120::result(r).value(QStringLiteral("ok")).toBool(),
                     "120d: import_opj failed: "
                             << QString::fromUtf8(QJsonDocument(r).toJson(QJsonDocument::Compact)).toStdString());
            CHECK120(ms < 6000 * kTestTimingScale, "120d: importing Histo.opj took " << ms << " ms");
        }
    }
#endif

    if (g120Failures) {
        std::cerr << "[FAIL] Suite 120: " << g120Failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 120\n";
    return 0;
}
