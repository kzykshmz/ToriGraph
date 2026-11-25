#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QJsonObject>

// =============================================================================
// Suite 90: tearing down an ApplicationWindow that holds a maximized subwindow created after a
// Filter-derived analysis made its hidden result table + result graph.
//   90a  signal.fft (graph path) on non-uniform X: FFT::output() shows its result graph
//        maximized, so the table created next opens maximized too; deleting the window used to
//        be an access violation (QMdiArea::removeSubWindow re-showed the hidden, still maximized
//        table as a native top-level window, and ~QWidget then hid it after ~TableView had
//        deleted the TableModel the comments header reads in sizeHint()).
//   90b  the same with Differentiation (another Filter with a hidden result table and a result
//        graph) whose result graph is then maximized by hand before the next table is created.
// A crash here kills the test process, which the runner reports as a failed suite.
// =============================================================================

namespace {

struct NonInteractiveGuard90
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard90()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard90()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

// Owns a test ApplicationWindow; reset() deletes it (the step under test).
struct AppWinGuard90
{
    ApplicationWindow *win = nullptr;
    AppWinGuard90()
    {
        win = new ApplicationWindow();
        win->applyUserSettings();
        win->confirmCloseTable = false;
        win->confirmClosePlot2D = false;
        win->confirmClosePlot3D = false;
        win->confirmCloseFolder = false;
        win->resize(1280, 800);
        win->show();
        QCoreApplication::processEvents();
    }
    ~AppWinGuard90() { reset(); }
    AppWinGuard90(const AppWinGuard90 &) = delete;
    AppWinGuard90 &operator=(const AppWinGuard90 &) = delete;
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

// Table with 8 non-uniform X points and a line plot of it.
MultiLayer *makeFixture90(ApplicationWindow *w, const QString &name)
{
    const double xs[8] = { 0.0, 0.1, 0.3, 0.35, 0.6, 0.8, 1.0, 1.4 };
    Table *t = w->newTable(8, 2, name, name);
    if (!t)
        return nullptr;
    for (int r = 0; r < 8; ++r) {
        t->setCell(r, 0, xs[r]);
        t->setCell(r, 1, std::sin(4.0 * xs[r]));
    }
    MultiLayer *ml = w->multilayerPlot(t, QStringList() << t->colName(1), Graph::Line);
    QCoreApplication::processEvents();
    return ml;
}

} // namespace

int runSuite90FftWindowTeardown(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 90: window teardown after FFT / Filter result graphs...\n";

    NonInteractiveGuard90 envGuard90;

    // -------------------------------------------------------------------------
    // 90a: signal.fft (graph path) -> newTable -> delete window
    // -------------------------------------------------------------------------
    {
        AppWinGuard90 w;
        PythonAutomationServer *srv = w->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "90a: pythonAutomationServer null");
        MultiLayer *ml = makeFixture90(w, QStringLiteral("T90Fft"));
        CHECK_COND(ml != nullptr && ml->activeGraph() && ml->activeGraph()->curves() == 1,
                   "90a: plot fixture failed");

        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 9001;
        req[QStringLiteral("method")] = QStringLiteral("signal.fft");
        req[QStringLiteral("params")] = QJsonObject{ { "graph", ml->name() } };
        const QJsonObject resp = srv->handleRpcRequest(req);
        QCoreApplication::processEvents();
        CHECK_COND(!resp.contains(QStringLiteral("error")), "90a: signal.fft returned an error");
        const QString fftName = resp.value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("table"))
                                        .toString();
        CHECK_COND(w->table(fftName) != nullptr, "90a: FFT result table missing");

        Table *tNew = w->newTable();
        CHECK_COND(tNew != nullptr, "90a: newTable after FFT failed");
        QCoreApplication::processEvents();
        // Precondition of the crash: the new table inherits the maximized state.
        CHECK_COND(tNew->isMaximized(),
                   "90a: table created after FFT is not maximized; test would be vacuous");

        std::cout << "[INFO] 90a: deleting the window...\n" << std::flush;
        w.reset();
        std::cout << "[PASS] 90a: window with FFT result graph + later maximized table torn down\n";
    }

    // -------------------------------------------------------------------------
    // 90b: Differentiation -> maximize its result graph -> newTable -> delete window
    // -------------------------------------------------------------------------
    {
        AppWinGuard90 w;
        MultiLayer *ml = makeFixture90(w, QStringLiteral("T90Diff"));
        CHECK_COND(ml != nullptr && ml->activeGraph() && ml->activeGraph()->curves() == 1,
                   "90b: plot fixture failed");
        Graph *g = ml->activeGraph();
        const QList<MyWidget *> before = w->windowsList();
        {
            Differentiation diff(w, g, g->curvesList().value(0));
            CHECK_COND(diff.run(), "90b: Differentiation::run failed");
        }
        QCoreApplication::processEvents();
        Table *diffTbl = nullptr;
        MultiLayer *diffMl = nullptr;
        for (MyWidget *win : w->windowsList()) {
            if (before.contains(win))
                continue;
            if (!diffTbl)
                diffTbl = qobject_cast<Table *>(win);
            if (!diffMl)
                diffMl = qobject_cast<MultiLayer *>(win);
        }
        CHECK_COND(diffTbl != nullptr && diffMl != nullptr,
                   "90b: Differentiation did not create its result table and graph");
        diffMl->showMaximized();
        QCoreApplication::processEvents();

        Table *tNew = w->newTable();
        CHECK_COND(tNew != nullptr, "90b: newTable after Differentiation failed");
        QCoreApplication::processEvents();
        CHECK_COND(tNew->isMaximized(),
                   "90b: table created after the maximized result graph is not maximized; "
                   "test would be vacuous");

        std::cout << "[INFO] 90b: deleting the window...\n" << std::flush;
        w.reset();
        std::cout << "[PASS] 90b: window with Differentiation result graph + later maximized table "
                     "torn down\n";
    }

    std::cout << "[PASS] Suite 90 passed.\n";
    return 0;
}
