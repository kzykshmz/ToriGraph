// Suite 119: regression checks from the 2026-11-28 review / debug campaign.
#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/future_Table.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cmath>

// =============================================================================
// Suite 119: curve <-> column bookkeeping.
//   119a removing a column takes only the curves that use that very column: "y" does not take
//        the curve of "y.1" (a word-boundary match on the plot association did), and a column
//        whose name ends in a non-word character ("t (s)") is found, so its curve is taken
//        with an Undo step and comes back with the table's Undo (it used to be deleted for good)
//   119b a plotted column that becomes Text under a numeric axis leaves its curve empty in the
//        layer (it used to be deleted without an Undo step) without turning the axis into a
//        text axis; the table's Undo brings the curve's data back
//   119c renaming a column keeps the blank lines of the legend and does not rewrite the name
//        inside a longer column name ("L119_y" in "L119_y.1")
//   119d clearing the masks of one curve leaves the masks another curve has on the shared Y
//        column (rows outside its row range)
//   119e a graph's mask step keeps its rows through a table row removal and the table's Undo
//        of it: the graph's Undo unmasks them again and its Redo masks them
// =============================================================================

int runSuite119CurveColumnBookkeeping(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Suite 119: runSuite119CurveColumnBookkeeping\n";

    struct NonInteractiveQuitGuard119
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard119()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard119()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard119;

    struct AppWinGuard119
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard119(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard119()
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        AppWinGuard119(const AppWinGuard119 &) = delete;
        AppWinGuard119 &operator=(const AppWinGuard119 &) = delete;
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard119 w119(new ApplicationWindow());
    w119->applyUserSettings();
    w119->confirmCloseTable = false;
    w119->confirmCloseMatrix = false;
    w119->confirmClosePlot2D = false;
    w119->confirmClosePlot3D = false;
    w119->confirmCloseFolder = false;
    w119->resize(1280, 800);
    w119->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w119->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "119: pythonAutomationServer null");

    int rpcId = 11900;
    auto rpc = [&](const QString &method, const QJsonObject &params) -> QJsonObject {
        QJsonObject req;
        req[QString::fromLatin1("jsonrpc")] = QString::fromLatin1("2.0");
        req[QString::fromLatin1("id")] = ++rpcId;
        req[QString::fromLatin1("method")] = method;
        req[QString::fromLatin1("params")] = params;
        const QJsonObject resp = srv->handleRpcRequest(req);
        QCoreApplication::processEvents();
        return resp;
    };
    auto isErr = [](const QJsonObject &resp) { return resp.contains(QString::fromLatin1("error")); };
    auto errText = [](const QJsonObject &resp) {
        return resp.value(QString::fromLatin1("error"))
                .toObject()
                .value(QString::fromLatin1("message"))
                .toString()
                .toStdString();
    };
    auto setCol = [&](const QString &table, const QString &col, const QVector<double> &v) {
        QJsonArray a;
        for (double d : v)
            a.append(d);
        const QJsonObject r = rpc(QString::fromLatin1("table.set_column_data"),
                                  QJsonObject{ { "table", table }, { "col", col }, { "data", a } });
        return !isErr(r);
    };
    auto plotXY = [&](const QString &table, const QString &x, const QStringList &ys,
                      const QString &graph) {
        QJsonArray ya;
        for (const QString &y : ys)
            ya.append(y);
        return rpc(QString::fromLatin1("plot.xy"),
                   QJsonObject{ { "table", table }, { "x_col", x }, { "y_cols", ya },
                                { "graph_name", graph } });
    };
    auto undoRedo = [&](const char *kind, const QString &name, bool undo) {
        return !isErr(rpc(QString::fromLatin1(undo ? "app.undo" : "app.redo"),
                          QJsonObject{ { QString::fromLatin1(kind), name } }));
    };
    auto findMl = [&](const QString &name) -> MultiLayer * {
        for (MyWidget *mw2 : w119->windowsList()) {
            if (mw2 && mw2->inherits("MultiLayer") && mw2->name() == name)
                return static_cast<MultiLayer *>(mw2);
        }
        return nullptr;
    };
    auto layer0 = [&](const QString &name) -> Graph * {
        MultiLayer *ml = findMl(name);
        if (!ml || ml->layersList().isEmpty())
            return nullptr;
        return ml->layersList().at(0);
    };
    auto sig = [](Graph *g) {
        std::string s;
        if (!g)
            return std::string("<no layer>");
        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            s += (c ? c->title().text().toStdString() : std::string("?")) + ":"
                    + std::to_string(c ? c->dataSize() : -1) + ";";
        }
        return s;
    };
    auto dataCurve = [](Graph *g, int i) -> DataCurve * {
        return g ? dynamic_cast<DataCurve *>(g->curve(i)) : nullptr;
    };

    // ---- 119a: exact column names when a column is removed ----
    {
        Table *t = w119->newTable(QString::fromLatin1("A119"), 5, 4);
        CHECK_COND(t != nullptr, "119a: table");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y"));
        t->setColName(2, QString::fromLatin1("y.1"));
        t->setColName(3, QString::fromLatin1("t (s)"));
        CHECK_COND(t->colName(2) == QLatin1String("A119_y.1")
                           && t->colName(3) == QLatin1String("A119_t (s)"),
                   "119a: column names " << t->colName(2).toStdString() << " / "
                                         << t->colName(3).toStdString());
        CHECK_COND(setCol("A119", "x", { 0, 1, 2, 3, 4 }) && setCol("A119", "y", { 1, 3, 2, 5, 4 })
                           && setCol("A119", "y.1", { 9, 8, 7, 6, 5 })
                           && setCol("A119", "t (s)", { 0, 10, 20, 30, 40 }),
                   "119a: fill table");
        QJsonObject r = plotXY("A119", "x", { "y.1" }, "AG119");
        CHECK_COND(!isErr(r), "119a: plot AG119: " << errText(r));
        r = plotXY("A119", "t (s)", { "y.1" }, "BG119");
        CHECK_COND(!isErr(r), "119a: plot BG119: " << errText(r));
        Graph *ga = layer0("AG119");
        Graph *gb = layer0("BG119");
        CHECK_COND(ga && gb && ga->curves() == 1 && gb->curves() == 1,
                   "119a: setup " << sig(ga) << " | " << sig(gb));
        const std::string sigA = sig(ga);
        const std::string sigB = sig(gb);

        // "y" is not "y.1"
        r = rpc(QString::fromLatin1("table.remove_column"),
                QJsonObject{ { "table", "A119" }, { "col", "y" } });
        CHECK_COND(!isErr(r) && t->numCols() == 3, "119a: remove y: " << errText(r));
        CHECK_COND(sig(ga) == sigA && sig(gb) == sigB,
                   "119a: removing y took the curve of y.1: " << sig(ga) << " | " << sig(gb));
        CHECK_COND(undoRedo("table", "A119", true) && t->numCols() == 4,
                   "119a: undo remove y");

        // "t (s)" (ends in ')') is found: taken with an Undo step and back with the Undo
        r = rpc(QString::fromLatin1("table.remove_column"),
                QJsonObject{ { "table", "A119" }, { "col", "t (s)" } });
        CHECK_COND(!isErr(r) && t->numCols() == 3, "119a: remove t (s): " << errText(r));
        CHECK_COND(gb->curves() == 0 && sig(ga) == sigA,
                   "119a: after removing t (s): " << sig(ga) << " | " << sig(gb));
        CHECK_COND(undoRedo("table", "A119", true) && t->numCols() == 4,
                   "119a: undo remove t (s)");
        CHECK_COND(sig(gb) == sigB && sig(ga) == sigA,
                   "119a: undo did not bring the curve of t (s) back: " << sig(gb) << " expected "
                                                                         << sigB);
        DataCurve *cb = dataCurve(gb, 0);
        CHECK_COND(cb && cb->xColumnName() == QLatin1String("A119_t (s)"),
                   "119a: restored curve X column");
    }

    // ---- 119b: a column that becomes Text keeps its curve (empty) for Undo ----
    {
        Table *t = w119->newTable(QString::fromLatin1("M119"), 5, 2);
        CHECK_COND(t != nullptr, "119b: table");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y"));
        CHECK_COND(setCol("M119", "x", { 0, 1, 2, 3, 4 }) && setCol("M119", "y", { 1, 3, 2, 5, 4 }),
                   "119b: fill table");
        QJsonObject r = plotXY("M119", "x", { "y" }, "MG119");
        CHECK_COND(!isErr(r), "119b: plot MG119: " << errText(r));
        Graph *g = layer0("MG119");
        CHECK_COND(g && g->curves() == 1 && g->curve(0)->dataSize() == 5, "119b: setup " << sig(g));
        const int yAxis = g->curve(0)->yAxis();

        r = rpc(QString::fromLatin1("table.set_column_meta"),
                QJsonObject{ { "table", "M119" }, { "col", "y" }, { "mode", "text" } });
        CHECK_COND(!isErr(r), "119b: mode text: " << errText(r));
        CHECK_COND(t->column(1)->columnMode() == SciDAVis::ColumnMode::Text,
                   "119b: column not Text");
        CHECK_COND(g->curves() == 1 && g->curve(0)->dataSize() == 0,
                   "119b: curve expected to stay, empty: " << sig(g));
        CHECK_COND(g->axesType().value(yAxis) != Graph::AxisType::Txt,
                   "119b: the Y axis became a text axis");
        DataCurve *dc = dataCurve(g, 0);
        CHECK_COND(dc && dc->isHeldEmpty(), "119b: curve not held empty");

        CHECK_COND(undoRedo("table", "M119", true), "119b: undo mode");
        CHECK_COND(t->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric,
                   "119b: undo did not restore Numeric");
        CHECK_COND(g->curves() == 1 && g->curve(0)->dataSize() == 5
                           && std::fabs(g->curve(0)->y(1) - 3.0) < 1e-12,
                   "119b: undo did not bring the curve's data back: " << sig(g));
        CHECK_COND(dc && !dc->isHeldEmpty(), "119b: curve still held after undo");
    }

    // ---- 119c: renaming a column keeps the legend's blank lines and longer names ----
    {
        Table *t = w119->newTable(QString::fromLatin1("L119"), 3, 3);
        CHECK_COND(t != nullptr, "119c: table");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y"));
        t->setColName(2, QString::fromLatin1("y.1"));
        CHECK_COND(setCol("L119", "x", { 0, 1, 2 }) && setCol("L119", "y", { 1, 2, 3 })
                           && setCol("L119", "y.1", { 3, 2, 1 }),
                   "119c: fill table");
        QJsonObject r = plotXY("L119", "x", { "y", "y.1" }, "LG119");
        CHECK_COND(!isErr(r), "119c: plot LG119: " << errText(r));
        Graph *g = layer0("LG119");
        CHECK_COND(g && g->curves() == 2 && g->legend(), "119c: setup " << sig(g));
        g->legend()->setText(QString::fromLatin1("\\l(1)L119_y\n\nL119_y.1 vs L119_y"));

        t->setColName(1, QString::fromLatin1("yy"));
        QCoreApplication::processEvents();
        CHECK_COND(t->colName(1) == QLatin1String("L119_yy"), "119c: rename");
        const QString leg = g->legend() ? g->legend()->text() : QString();
        std::cout << "[INFO] 119c: legend after rename: " << leg.toStdString() << "\n";
        CHECK_COND(leg.contains(QLatin1String("\n\n")), "119c: blank legend line dropped: "
                                                                 << leg.toStdString());
        CHECK_COND(leg.contains(QLatin1String("L119_y.1 vs L119_yy")),
                   "119c: longer column name rewritten or name not updated: "
                           << leg.toStdString());
    }

    // ---- 119d: clearing one curve's masks keeps the other curve's ----
    {
        Table *t = w119->newTable(QString::fromLatin1("K119"), 6, 3);
        CHECK_COND(t != nullptr, "119d: table");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y"));
        t->setColName(2, QString::fromLatin1("x2"));
        CHECK_COND(setCol("K119", "x", { 0, 1, 2, 3, 4, 5 })
                           && setCol("K119", "y", { 5, 4, 3, 2, 1, 0 })
                           && setCol("K119", "x2", { 10, 11, 12, 13, 14, 15 }),
                   "119d: fill table");
        QJsonObject r = plotXY("K119", "x", { "y" }, "KG119");
        CHECK_COND(!isErr(r), "119d: plot KG119: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "KG119" }, { "table", "K119" }, { "x_col", "x2" },
                             { "y_col", "y" } });
        CHECK_COND(!isErr(r), "119d: add curve: " << errText(r));
        Graph *g = layer0("KG119");
        DataCurve *c1 = dataCurve(g, 0);
        DataCurve *c2 = dataCurve(g, 1);
        CHECK_COND(g && g->curves() == 2 && c1 && c2, "119d: setup " << sig(g));
        c1->setRowRange(0, 2);
        c2->setRowRange(3, 5);
        Column *yc = t->column(1);
        CHECK_COND(yc != nullptr, "119d: y column");
        yc->setMasked(1, true);
        yc->setMasked(4, true);
        CHECK_COND(c1->dataSize() == 3 && c2->dataSize() == 3 && yc->isMasked(1) && yc->isMasked(4),
                   "119d: setup masks " << sig(g));
        c1->clearMasks();
        CHECK_COND(!yc->isMasked(1), "119d: the curve's own mask stayed");
        CHECK_COND(yc->isMasked(4), "119d: clearing curve 1 cleared curve 2's mask on shared Y");
    }

    // ---- 119e: a graph mask step survives a table row removal and its Undo ----
    {
        Table *t = w119->newTable(QString::fromLatin1("R119"), 6, 2);
        CHECK_COND(t != nullptr && t->d_future_table, "119e: table");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y"));
        CHECK_COND(setCol("R119", "x", { 0, 1, 2, 3, 4, 5 })
                           && setCol("R119", "y", { 1, 2, 3, 4, 5, 6 }),
                   "119e: fill table");
        QJsonObject r = plotXY("R119", "x", { "y" }, "RG119");
        CHECK_COND(!isErr(r), "119e: plot RG119: " << errText(r));
        Graph *g = layer0("RG119");
        DataCurve *dc = dataCurve(g, 0);
        Column *yc = t->column(1);
        CHECK_COND(g && dc && yc && dc->dataSize() == 6, "119e: setup " << sig(g));
        CHECK_COND(g->runWithColumnMaskUndo(QString::fromLatin1("Mask 119"),
                                            QList<DataCurve *>() << dc,
                                            [&]() { dc->maskPoint(3, true); }),
                   "119e: mask step not recorded");
        CHECK_COND(yc->isMasked(3), "119e: row 3 not masked");

        t->d_future_table->removeRows(2, 3);
        QCoreApplication::processEvents();
        CHECK_COND(t->numRows() == 3, "119e: rows not removed: " << t->numRows());
        CHECK_COND(undoRedo("table", "R119", true) && t->numRows() == 6,
                   "119e: table undo of the row removal");
        CHECK_COND(yc->isMasked(3), "119e: the table's undo did not restore the mask (precondition)");

        CHECK_COND(undoRedo("graph", "RG119", true), "119e: graph undo");
        CHECK_COND(!yc->isMasked(3), "119e: the graph's undo did not unmask row 3 after the rows "
                                     "came back");
        CHECK_COND(undoRedo("graph", "RG119", false), "119e: graph redo");
        CHECK_COND(yc->isMasked(3), "119e: the graph's redo did not mask row 3 again");
    }

    std::cout << "[PASS] Suite 119\n";
    return 0;
}
