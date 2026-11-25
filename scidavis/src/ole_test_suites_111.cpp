#include "ole_test_common.h"
#include "BoxCurve.h"
#include "PythonAutomationServer.h"
#include "future/table/future_Table.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QUndoStack>

#include <cmath>
#include <vector>

// =============================================================================
// Suite 111: dual-Y legend headroom and curves of a removed column (JSON-RPC path).
//   111a lab_slide_83mm on a dual-Y graph (plot.double_y) keeps the left Y axis close to the
//        data: the legend headroom used to widen only the left axis (250..650 -> 200..1600,
//        a TDS temperature ramp 200..900 -> 0..2500) because the right-axis curve still met
//        the legend however far the left axis grew, and each placement widened it again;
//        applying the preset twice does not widen it further. Single-axis graphs keep the
//        result they had (one curve unchanged, two curves still get the headroom).
//   111b removing a column that curves use (X, Y) and undoing it brings the curves back in
//        every graph and layer that used them, with their style and order; Redo removes them
//        again; removing a Y column only takes that curve and Undo puts it back in place
//   111c Undo puts the curves back into the graph as it is now: an X range set and a curve
//        added after the removal stay, the legend gets the lines back in order, the error bars
//        hang on their curve again, a layer deleted meanwhile is skipped; removing only the
//        error column takes and restores only the error bars
//   111d a curve is known by its key as well as its address: after only the error column was
//        removed, the user deletes the error bars' curve and a new curve comes (if possible at
//        the same address); the table's Undo does not hang the error bars on it. A restored
//        curve the user deletes, followed by a new one: the table's Redo leaves the new one
//   111e a column that is empty for a while (cells cleared, a formula 1/0 that makes every row
//        invalid, its data entry undone) keeps its curve as an empty curve with its style and
//        legend line: the curve used to leave the graph without an Undo step, so Undo / Redo
//        brought the data back but not the curve. Saving works with an empty curve; plotting a
//        column without data is still refused; the saved project opens with the empty curve in
//        place (it used to be dropped on reading) and the curve takes the data when it comes
//   111f a box (box plot) whose column is empty for a while stays in place without points and
//        Undo brings its data back (it used to leave the graph for good)
//   111g the curves of a table or column that leaves the project come back with it: undoing the
//        creation of a table (table.create), of integrate's result table (app.undo / app.redo)
//        and of an added column, then redoing it, brings the curve back (it was lost); deleting
//        the data table and undoing that brings its curve back with its style and legend line,
//        Redo takes it again
//   111h held curves go back only to their own layer and only with their tables: a layer the
//        user deleted meanwhile does not hand its curve to the next layer; after the graph's Undo
//        rebuilt its layers, the curve and its error bars come back; held error bars from
//        another table whose window was deleted meanwhile are dropped when a table comes back
//        (they used to be read through the deleted window: use after free)
//   111i held curves follow their layer through the graph's Undo / Redo, which rebuilds the
//        layers (also several times in a row): a removed column's Undo brings its curve, error
//        bars and legend line back (the layer used to be lost), Redo takes them from the rebuilt
//        layer; a curve held while a layer was added goes back to layer 0 after the "add layer"
//        is undone (it used to be dropped since the layer count differed); a curve put back into
//        a rebuilt layer keeps its place in the curve order; after the layer before it is
//        deleted and that undone, it goes back to its own layer; after the user deletes its
//        layer, it does not go into another one
//   111j the legend gets its text back as it was when the curves come back: an empty line in it
//        (left by removing the last curve and adding another, or typed by the user) used to be
//        dropped by the Undo of a column removal or of a table deletion
//   111k the graph's Undo of a step made before a column was removed rebuilds the layer from a
//        snapshot with the removed column's curve: the curve is left out and so is its legend
//        line now (it stayed, and the column's Undo put it in again: \c{2}z\n\c{3}z), for a Y
//        column with error bars, one in the middle and the last one
//   111l removing error bars (curve or column) moves the legend numbers after them up (\c{3}w
//        stayed with two curves), removing the last curve removes its line also when the legend
//        has fewer lines than curves; a line that is not a curve's and a curve without a line
//        stay as they are; a column used by a curve and by error bars, with a fit curve after
//        them, leaves each number once and within the curves
//   111m a table closed through app.close_window comes back with app.undo without a window
//        (the RPC deleted the window its removal keeps for Undo), with its curve; app.redo
//        closes it again; the menu's Undo with another worksheet active brings it back as well
// =============================================================================

int runSuite111DualYHeadroomAndRemovedColumnUndo(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 111: dual-Y legend headroom and removed-column undo...\n";

    struct NonInteractiveQuitGuard111
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard111()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard111()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard111;

    struct AppWinGuard111
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard111(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard111()
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        AppWinGuard111(const AppWinGuard111 &) = delete;
        AppWinGuard111 &operator=(const AppWinGuard111 &) = delete;
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard111 w111(new ApplicationWindow());
    w111->applyUserSettings();
    w111->confirmCloseTable = false;
    w111->confirmCloseMatrix = false;
    w111->confirmClosePlot2D = false;
    w111->confirmClosePlot3D = false;
    w111->confirmCloseFolder = false;
    w111->resize(1280, 800);
    w111->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w111->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "111: pythonAutomationServer null");

    int rpcId = 11100;
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
    auto res = [](const QJsonObject &resp) { return resp.value(QString::fromLatin1("result")).toObject(); };
    auto errText = [](const QJsonObject &resp) {
        return resp.value(QString::fromLatin1("error"))
                .toObject()
                .value(QString::fromLatin1("message"))
                .toString()
                .toStdString();
    };
    auto toArr = [](const QVector<double> &v) {
        QJsonArray a;
        for (double d : v)
            a.append(d);
        return a;
    };
    auto setCol = [&](const QString &table, const QString &col, const QVector<double> &v) {
        const QJsonObject r = rpc(QString::fromLatin1("table.set_column_data"),
                                  QJsonObject{ { "table", table }, { "col", col }, { "data", toArr(v) } });
        return !isErr(r);
    };
    auto graphInfo = [&](const QString &graph) {
        return res(rpc(QString::fromLatin1("graph.get_info"), QJsonObject{ { "graph", graph } }));
    };
    struct AxisRange
    {
        double lo = std::nan("");
        double hi = std::nan("");
    };
    auto axisRange = [&](const QString &graph, const char *axis) {
        const QJsonObject a = graphInfo(graph)
                                      .value(QString::fromLatin1("layers"))
                                      .toArray()
                                      .at(0)
                                      .toObject()
                                      .value(QString::fromLatin1("axes"))
                                      .toObject()
                                      .value(QString::fromLatin1(axis))
                                      .toObject();
        AxisRange r;
        r.lo = a.value(QString::fromLatin1("min")).toDouble(std::nan(""));
        r.hi = a.value(QString::fromLatin1("max")).toDouble(std::nan(""));
        return r;
    };
    auto rs = [](const AxisRange &r) {
        return std::to_string(r.lo) + ".." + std::to_string(r.hi);
    };
    auto applyLab = [&](const QString &graph) {
        const QJsonObject r = rpc(QString::fromLatin1("graph.apply_style_preset"),
                                  QJsonObject{ { "graph", graph }, { "preset", "lab_slide_83mm" } });
        return !isErr(r);
    };

    // ---- 111a: dual-Y legend headroom ----
    {
        Table *t = w111->newTable(QString::fromLatin1("S111"), 4, 4);
        CHECK_COND(t != nullptr, "111a: table S111");
        t->setColName(0, QString::fromLatin1("t"));
        t->setColName(1, QString::fromLatin1("a"));
        t->setColName(2, QString::fromLatin1("b"));
        t->setColName(3, QString::fromLatin1("c"));
        CHECK_COND(setCol("S111", "t", { 0, 1, 2, 3 }) && setCol("S111", "a", { 300, 400, 500, 600 })
                           && setCol("S111", "b", { 1e-11, 2e-11, 1.5e-11, 0.5e-11 })
                           && setCol("S111", "c", { 1, 1, 1, 1 }),
                   "111a: fill S111");

        // single-axis references: the preset results these graphs had before the fix
        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "S111" }, { "x_col", "t" },
                                         { "y_cols", QJsonArray{ "a" } }, { "graph_name", "One111" } });
        CHECK_COND(!isErr(r), "111a: plot One111: " << errText(r));
        const AxisRange one0 = axisRange("One111", "y");
        CHECK_COND(applyLab("One111"), "111a: preset on One111");
        const AxisRange one1 = axisRange("One111", "y");
        std::cout << "[INFO] 111a: single axis " << rs(one0) << " -> " << rs(one1) << "\n";
        CHECK_COND(std::fabs(one1.lo - 200.0) < 1e-9 && std::fabs(one1.hi - 700.0) < 1e-9,
                   "111a: one curve: expected 200..700 (no headroom), got " << rs(one0) << " -> "
                                                                             << rs(one1));

        r = rpc(QString::fromLatin1("plot.xy"),
                QJsonObject{ { "table", "S111" }, { "x_col", "t" }, { "y_cols", QJsonArray{ "a", "c" } },
                             { "graph_name", "TwoL111" } });
        CHECK_COND(!isErr(r), "111a: plot TwoL111: " << errText(r));
        const AxisRange two0 = axisRange("TwoL111", "y");
        CHECK_COND(applyLab("TwoL111"), "111a: preset on TwoL111");
        const AxisRange two1 = axisRange("TwoL111", "y");
        std::cout << "[INFO] 111a: two curves on the left axis " << rs(two0) << " -> " << rs(two1)
                  << "\n";
        CHECK_COND(std::fabs(two1.lo - 0.0) < 1e-9 && std::fabs(two1.hi - 800.0) < 1e-9,
                   "111a: two curves on one axis: headroom changed, expected 0..800, got "
                           << rs(two1));

        // dual Y: left 300..600, right 0.5e-11..2e-11
        r = rpc(QString::fromLatin1("plot.double_y"),
                QJsonObject{ { "table", "S111" }, { "x_col", "t" }, { "y_left_col", "a" },
                             { "y_right_col", "b" }, { "graph_name", "Dy111" } });
        CHECK_COND(!isErr(r), "111a: double_y Dy111: " << errText(r));
        const AxisRange dy0 = axisRange("Dy111", "y");
        const AxisRange dyR0 = axisRange("Dy111", "right");
        CHECK_COND(applyLab("Dy111"), "111a: preset on Dy111");
        const AxisRange dy1 = axisRange("Dy111", "y");
        const AxisRange dyR1 = axisRange("Dy111", "right");
        std::cout << "[INFO] 111a: double_y left " << rs(dy0) << " -> " << rs(dy1) << ", right "
                  << rs(dyR0) << " -> " << rs(dyR1) << "\n";
        CHECK_COND(dy1.lo <= 300.0 && dy1.hi >= 600.0, "111a: dual-Y left axis cuts the data: " << rs(dy1));
        CHECK_COND((600.0 - 300.0) / (dy1.hi - dy1.lo) >= 0.45,
                   "111a: dual-Y left axis widened so the data fills less than 45 %: " << rs(dy0)
                                                                                      << " -> " << rs(dy1));
        CHECK_COND(dy1.hi <= 900.0 + 1e-9,
                   "111a: dual-Y left axis top above 900 (single axis gives 700): " << rs(dy1));
        CHECK_COND(applyLab("Dy111"), "111a: preset on Dy111 again");
        const AxisRange dy2 = axisRange("Dy111", "y");
        CHECK_COND(std::fabs(dy2.lo - dy1.lo) < 1e-9 && std::fabs(dy2.hi - dy1.hi) < 1e-9,
                   "111a: applying the preset again widened the dual-Y left axis " << rs(dy1)
                                                                                   << " -> " << rs(dy2));

        // TDS-like dual Y: temperature ramp 200..900 on the left, a desorption peak on the right
        const int n = 141;
        Table *td = w111->newTable(QString::fromLatin1("T111"), n, 3);
        CHECK_COND(td != nullptr, "111a: table T111");
        td->setColName(0, QString::fromLatin1("time"));
        td->setColName(1, QString::fromLatin1("temp"));
        td->setColName(2, QString::fromLatin1("sig"));
        QVector<double> tv, temp, sig;
        for (int i = 0; i < n; ++i) {
            const double x = double(i) * 10.0;
            tv << x;
            temp << 200.0 + 5.0 * double(i);
            const double d1 = (x - 600.0) / 120.0;
            const double d2 = (x - 1000.0) / 90.0;
            sig << 1e-10 * (0.05 + std::exp(-0.5 * d1 * d1) + 0.6 * std::exp(-0.5 * d2 * d2));
        }
        CHECK_COND(setCol("T111", "time", tv) && setCol("T111", "temp", temp) && setCol("T111", "sig", sig),
                   "111a: fill T111");
        r = rpc(QString::fromLatin1("plot.double_y"),
                QJsonObject{ { "table", "T111" }, { "x_col", "time" }, { "y_left_col", "temp" },
                             { "y_right_col", "sig" }, { "graph_name", "Tds111" } });
        CHECK_COND(!isErr(r), "111a: double_y Tds111: " << errText(r));
        const AxisRange td0 = axisRange("Tds111", "y");
        CHECK_COND(applyLab("Tds111"), "111a: preset on Tds111");
        const AxisRange td1 = axisRange("Tds111", "y");
        std::cout << "[INFO] 111a: TDS double_y left " << rs(td0) << " -> " << rs(td1) << "\n";
        CHECK_COND(td1.lo <= 200.0 && td1.hi >= 900.0, "111a: TDS left axis cuts the data: " << rs(td1));
        CHECK_COND((900.0 - 200.0) / (td1.hi - td1.lo) >= 0.45,
                   "111a: TDS left axis widened so the data fills less than 45 %: " << rs(td0)
                                                                                 << " -> " << rs(td1));
    }

    // ---- 111b: Undo of a removed column brings its curves back ----
    {
        Table *t = w111->newTable(QString::fromLatin1("U111"), 5, 4);
        CHECK_COND(t != nullptr, "111b: table U111");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y1"));
        t->setColName(2, QString::fromLatin1("y2"));
        t->setColName(3, QString::fromLatin1("y3"));
        CHECK_COND(setCol("U111", "x", { 0, 1, 2, 3, 4 }) && setCol("U111", "y1", { 1, 3, 2, 5, 4 })
                           && setCol("U111", "y2", { 10, 12, 11, 15, 13 })
                           && setCol("U111", "y3", { -1, -2, -3, -2, -1 }),
                   "111b: fill U111");

        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "U111" }, { "x_col", "x" },
                                         { "y_cols", QJsonArray{ "y1", "y2", "y3" } },
                                         { "graph_name", "UG111" } });
        CHECK_COND(!isErr(r), "111b: plot UG111: " << errText(r));
        r = rpc(QString::fromLatin1("graph.set_curve_style"),
                QJsonObject{ { "graph", "UG111" }, { "curve", 1 }, { "color", "#123456" }, { "lw", 3.0 } });
        CHECK_COND(!isErr(r), "111b: style curve 1: " << errText(r));
        r = rpc(QString::fromLatin1("graph.add_layer"), QJsonObject{ { "graph", "UG111" } });
        CHECK_COND(!isErr(r), "111b: add layer: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "UG111" }, { "table", "U111" }, { "x_col", "x" },
                             { "y_col", "y2" }, { "layer", 1 } });
        CHECK_COND(!isErr(r), "111b: add curve to layer 1: " << errText(r));
        r = rpc(QString::fromLatin1("plot.xy"),
                QJsonObject{ { "table", "U111" }, { "x_col", "x" }, { "y_cols", QJsonArray{ "y3" } },
                             { "graph_name", "UH111" } });
        CHECK_COND(!isErr(r), "111b: plot UH111: " << errText(r));

        // "layer:title:color:width;..." for every layer
        auto curveSig = [&](const QString &graph) {
            std::string s;
            const QJsonArray layers = graphInfo(graph).value(QString::fromLatin1("layers")).toArray();
            for (int li = 0; li < layers.size(); ++li) {
                for (const QJsonValue &cv :
                     layers.at(li).toObject().value(QString::fromLatin1("curves")).toArray()) {
                    const QJsonObject c = cv.toObject();
                    s += std::to_string(li) + ":" + c.value(QString::fromLatin1("title")).toString().toStdString()
                            + ":" + c.value(QString::fromLatin1("color")).toString().toStdString() + ":"
                            + std::to_string(c.value(QString::fromLatin1("line_width")).toDouble()) + ":"
                            + std::to_string(c.value(QString::fromLatin1("points")).toInt()) + ";";
                }
            }
            return s;
        };
        auto undoT = [&](bool undo) {
            return !isErr(rpc(QString::fromLatin1(undo ? "app.undo" : "app.redo"),
                              QJsonObject{ { "table", "U111" } }));
        };
        auto colNames = [&]() {
            std::string s;
            for (int c = 0; c < t->numCols(); ++c)
                s += t->colLabel(c).toStdString() + ",";
            return s;
        };

        const std::string g0 = curveSig("UG111");
        const std::string h0 = curveSig("UH111");
        const std::string cols0 = colNames();
        std::cout << "[INFO] 111b: UG111 " << g0 << " UH111 " << h0 << "\n";
        CHECK_COND(g0.find("#123456") != std::string::npos && g0.find("1:U111_y2") != std::string::npos,
                   "111b: setup did not give a styled curve and a second layer: " << g0);

        // remove the X column: every curve goes
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "U111" }, { "col", "x" } });
        CHECK_COND(!isErr(r), "111b: remove x: " << errText(r));
        CHECK_COND(curveSig("UG111").empty() && curveSig("UH111").empty(),
                   "111b: curves left after removing x: " << curveSig("UG111") << " / " << curveSig("UH111"));

        CHECK_COND(undoT(true), "111b: undo remove x");
        CHECK_COND(colNames() == cols0, "111b: undo did not restore column x: " << colNames());
        CHECK_COND(curveSig("UG111") == g0,
                   "111b: UG111 curves after undoing the removal of x: " << curveSig("UG111")
                                                                         << " expected " << g0);
        CHECK_COND(curveSig("UH111") == h0,
                   "111b: UH111 curves after undoing the removal of x: " << curveSig("UH111")
                                                                         << " expected " << h0);

        CHECK_COND(undoT(false), "111b: redo remove x");
        CHECK_COND(t->numCols() == 3 && curveSig("UG111").empty() && curveSig("UH111").empty(),
                   "111b: redo of the removal of x: cols " << colNames() << " curves "
                                                          << curveSig("UG111") << " / " << curveSig("UH111"));
        CHECK_COND(undoT(true), "111b: undo remove x again");
        CHECK_COND(colNames() == cols0 && curveSig("UG111") == g0 && curveSig("UH111") == h0,
                   "111b: second undo of the removal of x: " << curveSig("UG111") << " / "
                                                             << curveSig("UH111"));

        // remove a Y column: only its curves go, and Undo puts them back in place
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "U111" }, { "col", "y2" } });
        CHECK_COND(!isErr(r), "111b: remove y2: " << errText(r));
        const std::string gNoY2 = curveSig("UG111");
        CHECK_COND(gNoY2.find("U111_y2") == std::string::npos && gNoY2.find("U111_y1") != std::string::npos
                           && gNoY2.find("U111_y3") != std::string::npos && curveSig("UH111") == h0,
                   "111b: after removing y2: " << gNoY2 << " / " << curveSig("UH111"));
        CHECK_COND(undoT(true), "111b: undo remove y2");
        CHECK_COND(colNames() == cols0 && curveSig("UG111") == g0 && curveSig("UH111") == h0,
                   "111b: undo of the removal of y2: " << curveSig("UG111") << " expected " << g0);

        // the restored curves follow the data again
        CHECK_COND(setCol("U111", "y2", { 20, 22, 21, 25, 23 }), "111b: edit y2");
        const QJsonArray l0 = graphInfo("UG111")
                                      .value(QString::fromLatin1("layers"))
                                      .toArray()
                                      .at(0)
                                      .toObject()
                                      .value(QString::fromLatin1("curves"))
                                      .toArray();
        CHECK_COND(l0.size() == 3 && std::fabs(l0.at(1).toObject().value(QString::fromLatin1("y_max")).toDouble() - 25.0) < 1e-9,
                   "111b: restored curve y2 does not follow its column: y_max "
                           << (l0.size() > 1 ? l0.at(1).toObject().value(QString::fromLatin1("y_max")).toDouble() : -1.0));
    }

    // ---- 111c: Undo puts the curves back into the graph as it is now ----
    {
        Table *t = w111->newTable(QString::fromLatin1("V111"), 5, 4);
        Table *t2 = w111->newTable(QString::fromLatin1("W111"), 3, 2);
        CHECK_COND(t != nullptr && t2 != nullptr, "111c: tables");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y1"));
        t->setColName(2, QString::fromLatin1("y2"));
        t->setColName(3, QString::fromLatin1("e1"));
        t2->setColName(0, QString::fromLatin1("xx"));
        t2->setColName(1, QString::fromLatin1("yy"));
        CHECK_COND(setCol("V111", "x", { 0, 1, 2, 3, 4 }) && setCol("V111", "y1", { 1, 3, 2, 5, 4 })
                           && setCol("V111", "y2", { 10, 12, 11, 15, 13 })
                           && setCol("V111", "e1", { 0.1, 0.2, 0.3, 0.2, 0.1 })
                           && setCol("W111", "xx", { 0, 2, 4 }) && setCol("W111", "yy", { 7, 8, 9 }),
                   "111c: fill tables");

        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "V111" }, { "x_col", "x" },
                                         { "y_cols", QJsonArray{ "y1", "y2" } }, { "graph_name", "VG111" } });
        CHECK_COND(!isErr(r), "111c: plot VG111: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_error_bars"),
                QJsonObject{ { "graph", "VG111" }, { "err_col", "e1" }, { "curve", "V111_y1" },
                             { "table", "V111" } });
        CHECK_COND(!isErr(r), "111c: error bars: " << errText(r));
        r = rpc(QString::fromLatin1("graph.set_curve_style"),
                QJsonObject{ { "graph", "VG111" }, { "curve", "V111_y2" }, { "color", "#654321" } });
        CHECK_COND(!isErr(r), "111c: style y2: " << errText(r));
        r = rpc(QString::fromLatin1("graph.add_layer"), QJsonObject{ { "graph", "VG111" } });
        CHECK_COND(!isErr(r), "111c: add layer: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "VG111" }, { "table", "V111" }, { "x_col", "x" },
                             { "y_col", "y1" }, { "layer", 1 } });
        CHECK_COND(!isErr(r), "111c: curve on layer 1: " << errText(r));

        MultiLayer *ml = nullptr;
        for (MyWidget *mw2 : w111->windowsList()) {
            if (mw2 && mw2->inherits("MultiLayer") && mw2->name() == QLatin1String("VG111"))
                ml = static_cast<MultiLayer *>(mw2);
        }
        CHECK_COND(ml != nullptr && ml->layersList().size() == 2, "111c: graph VG111 with 2 layers");
        Graph *g0 = ml->layersList().at(0);

        auto layerSig = [&](Graph *g) {
            std::string s;
            for (int i = 0; i < g->curves(); ++i) {
                QwtPlotCurve *c = g->curve(i);
                s += (c ? c->title().text().toStdString() : std::string("?")) + ":"
                        + (c ? c->pen().color().name().toStdString() : std::string()) + ";";
            }
            return s;
        };
        // the error bars hang on y1 and y1 lists them
        auto errLinked = [&](Graph *g) {
            for (int i = 0; i < g->curves(); ++i) {
                auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(i));
                if (!er)
                    continue;
                DataCurve *m = er->masterCurve();
                return m && g->plotItemIndex(m) >= 0 && m->title().text() == QLatin1String("V111_y1")
                        && m->errorBarsList().contains(er) && er->dataSize() == 5;
            }
            return false;
        };
        auto legendText = [&](Graph *g) {
            return g->legend() ? g->legend()->text().toStdString() : std::string();
        };
        const std::string sig0 = layerSig(g0);
        const std::string leg0 = legendText(g0);
        std::cout << "[INFO] 111c: VG111 layer 0 " << sig0 << " legend " << leg0 << "\n";
        CHECK_COND(errLinked(g0) && sig0.find("#654321") != std::string::npos,
                   "111c: setup: error bars on y1 and styled y2 expected: " << sig0);

        auto undoV = [&](bool undo) {
            return !isErr(rpc(QString::fromLatin1(undo ? "app.undo" : "app.redo"),
                              QJsonObject{ { "table", "V111" } }));
        };

        // remove x; then edit the graph: X range, a new curve on layer 0, delete layer 1
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "V111" }, { "col", "x" } });
        CHECK_COND(!isErr(r), "111c: remove x: " << errText(r));
        CHECK_COND(g0->curves() == 0, "111c: curves left after removing x: " << layerSig(g0));
        r = rpc(QString::fromLatin1("graph.set_axis"),
                QJsonObject{ { "graph", "VG111" }, { "axis", "x" }, { "min", -3.0 }, { "max", 7.0 }, { "layer", 0 } });
        CHECK_COND(!isErr(r), "111c: set x range: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "VG111" }, { "table", "W111" }, { "x_col", "xx" },
                             { "y_col", "yy" }, { "layer", 0 } });
        CHECK_COND(!isErr(r), "111c: add W111 curve: " << errText(r));
        r = rpc(QString::fromLatin1("graph.remove_layer"), QJsonObject{ { "graph", "VG111" }, { "layer", 1 } });
        CHECK_COND(!isErr(r) && ml->layersList().size() == 1, "111c: remove layer 1: " << errText(r));
        auto xRange = [&]() {
            return std::to_string(g0->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->lowerBound()) + ".."
                    + std::to_string(g0->plotWidget()->axisScaleDiv(QwtPlot::xBottom)->upperBound());
        };
        const std::string range1 = xRange();

        CHECK_COND(undoV(true), "111c: undo remove x");
        const std::string sigU = layerSig(g0);
        std::cout << "[INFO] 111c: after undo " << sigU << " x " << xRange() << " legend "
                  << legendText(g0) << "\n";
        CHECK_COND(t->numCols() == 4, "111c: undo did not restore x");
        CHECK_COND(sigU.compare(0, sig0.size(), sig0) == 0 && sigU.find("W111_yy:") == sig0.size()
                           && g0->curves() == 4,
                   "111c: after undo expected the original curves then W111_yy: " << sigU
                                                                                << " original " << sig0);
        CHECK_COND(xRange() == range1, "111c: undo changed the X range " << range1 << " -> " << xRange());
        CHECK_COND(errLinked(g0), "111c: error bars not linked to y1 after undo");
        const std::string legU = legendText(g0);
        // y1, y2, the error bars (no legend line), W111_yy
        CHECK_COND(legU.find("\\c{1}") != std::string::npos && legU.find("\\c{2}") != std::string::npos
                           && legU.find("\\c{4}") != std::string::npos
                           && legU.find("\\c{3}") == std::string::npos
                           && legU.find("\\c{1}") < legU.find("\\c{2}")
                           && legU.find("\\c{2}") < legU.find("\\c{4}"),
                   "111c: legend after undo: " << legU);

        CHECK_COND(undoV(false), "111c: redo remove x");
        CHECK_COND(t->numCols() == 3 && layerSig(g0).rfind("W111_yy:", 0) == 0 && g0->curves() == 1,
                   "111c: redo should leave only W111_yy: " << layerSig(g0));
        CHECK_COND(xRange() == range1, "111c: redo changed the X range " << xRange());
        CHECK_COND(undoV(true), "111c: undo remove x again");
        CHECK_COND(layerSig(g0) == sigU && errLinked(g0) && legendText(g0) == legU,
                   "111c: second undo: " << layerSig(g0) << " legend " << legendText(g0));

        // removing the error column takes only the error bars; undo hangs them on y1 again
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "V111" }, { "col", "e1" } });
        CHECK_COND(!isErr(r) && !errLinked(g0) && g0->curves() == 3, "111c: remove e1: " << layerSig(g0));
        CHECK_COND(undoV(true) && errLinked(g0) && layerSig(g0) == sigU,
                   "111c: undo remove e1: " << layerSig(g0));

        // ---- 111d: a curve deleted meanwhile is not mistaken for a new one at its address ----
        // A new W111 curve put into layer 0, preferably at the address a deleted curve had:
        // DataCurves are allocated (and kept until the end) until one gets that address.
        int reuseHits = 0;
        auto addCurveAt = [&](const void *addr) -> DataCurve * {
            std::vector<DataCurve *> spare;
            DataCurve *hit = nullptr;
            for (int i = 0; i < 20000 && !hit; ++i) {
                auto *c = new DataCurve(t2, QString::fromLatin1("W111_xx"),
                                        QString::fromLatin1("W111_yy"), 0, 2);
                if (static_cast<const void *>(c) == addr)
                    hit = c;
                else
                    spare.push_back(c);
            }
            if (hit) {
                ++reuseHits;
            } else {
                hit = spare.back();
                spare.pop_back();
            }
            for (DataCurve *c : spare)
                delete c;
            g0->insertPlotItem(hit, Graph::LineSymbols);
            hit->loadData();
            g0->replot();
            return hit;
        };
        auto indexOfTitle = [&](const char *title) {
            for (int i = 0; i < g0->curves(); ++i) {
                QwtPlotCurve *c = g0->curve(i);
                if (c && c->title().text() == QLatin1String(title)
                    && !dynamic_cast<QwtErrorPlotCurve *>(c))
                    return i;
            }
            return -1;
        };
        auto errorBarsIn = [&](Graph *g) {
            int n = 0;
            for (int i = 0; i < g->curves(); ++i)
                n += dynamic_cast<QwtErrorPlotCurve *>(g->curve(i)) ? 1 : 0;
            return n;
        };

        // the error column goes (its error bars are held), then the user deletes y1 and a new
        // curve comes: the table's Undo must not hang the error bars on the new curve
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "V111" }, { "col", "e1" } });
        CHECK_COND(!isErr(r) && errorBarsIn(g0) == 0, "111d: remove e1: " << layerSig(g0));
        const int iY1 = indexOfTitle("V111_y1");
        CHECK_COND(iY1 >= 0, "111d: y1 not in layer 0: " << layerSig(g0));
        const void *y1Addr = g0->curve(iY1);
        g0->removeCurve(iY1);
        DataCurve *n1 = addCurveAt(y1Addr);
        const bool reused1 = static_cast<const void *>(n1) == y1Addr;
        CHECK_COND(undoV(true), "111d: undo remove e1 after deleting y1");
        CHECK_COND(t->numCols() == 4, "111d: e1 not back");
        std::cout << "[INFO] 111d: after undo " << layerSig(g0) << " (address reused: "
                  << (reused1 ? "yes" : "no") << ")\n";
        CHECK_COND(errorBarsIn(g0) == 0 && n1->errorBarsList().isEmpty(),
                   "111d: error bars of the deleted y1 came back on another curve: " << layerSig(g0));

        // y2 goes and comes back with Undo; the user deletes it and a new curve comes: the
        // table's Redo must not take the new curve out
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "V111" }, { "col", "y2" } });
        CHECK_COND(!isErr(r) && indexOfTitle("V111_y2") < 0, "111d: remove y2: " << layerSig(g0));
        CHECK_COND(undoV(true) && indexOfTitle("V111_y2") >= 0, "111d: undo remove y2: " << layerSig(g0));
        const int iY2 = indexOfTitle("V111_y2");
        const void *y2Addr = g0->curve(iY2);
        g0->removeCurve(iY2);
        DataCurve *n2 = addCurveAt(y2Addr);
        const bool reused2 = static_cast<const void *>(n2) == y2Addr;
        const int nBefore = g0->curves();
        CHECK_COND(undoV(false), "111d: redo remove y2");
        std::cout << "[INFO] 111d: after redo " << layerSig(g0) << " (address reused: "
                  << (reused2 ? "yes" : "no") << ")\n";
        CHECK_COND(t->numCols() == 3 && g0->curves() == nBefore && g0->plotItemIndex(n2) >= 0
                           && g0->plotItemIndex(n1) >= 0,
                   "111d: redo took out a curve the user added: " << layerSig(g0));
        CHECK_COND(undoV(true) && t->numCols() == 4 && g0->curves() == nBefore
                           && g0->plotItemIndex(n2) >= 0,
                   "111d: undo after redo: " << layerSig(g0));
        std::cout << "[INFO] 111d: address reuse reproduced " << reuseHits << "/2\n";
    }

    // ---- 111e: a column that is empty for a while keeps its curve ----
    {
        Table *t = w111->newTable(QString::fromLatin1("E111"), 4, 4);
        CHECK_COND(t != nullptr, "111e: table E111");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y"));
        t->setColName(2, QString::fromLatin1("z"));
        t->setColName(3, QString::fromLatin1("w"));
        CHECK_COND(setCol("E111", "x", { 1, 2, 3, 4 }) && setCol("E111", "y", { 1, 4, 9, 16 })
                           && setCol("E111", "z", { 2, 3, 4, 5 }),
                   "111e: fill E111");
        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "E111" }, { "x_col", "x" },
                                         { "y_cols", QJsonArray{ "y", "z" } }, { "graph_name", "EG111" } });
        CHECK_COND(!isErr(r), "111e: plot EG111: " << errText(r));
        r = rpc(QString::fromLatin1("graph.set_curve_style"),
                QJsonObject{ { "graph", "EG111" }, { "curve", 0 }, { "color", "#ff0000" }, { "lw", 4.0 } });
        CHECK_COND(!isErr(r), "111e: style curve 0: " << errText(r));

        // "title:color:width:points;..." of layer 0, and its legend
        auto sigE = [&]() {
            std::string s;
            const QJsonArray layers = graphInfo("EG111").value(QString::fromLatin1("layers")).toArray();
            for (const QJsonValue &cv : layers.at(0).toObject().value(QString::fromLatin1("curves")).toArray()) {
                const QJsonObject c = cv.toObject();
                s += c.value(QString::fromLatin1("title")).toString().toStdString() + ":"
                        + c.value(QString::fromLatin1("color")).toString().toStdString() + ":"
                        + std::to_string(c.value(QString::fromLatin1("line_width")).toDouble()) + ":"
                        + std::to_string(c.value(QString::fromLatin1("points")).toInt()) + ";";
            }
            return s;
        };
        MultiLayer *eml = nullptr;
        for (MyWidget *mw2 : w111->windowsList()) {
            if (mw2 && mw2->inherits("MultiLayer") && mw2->name() == QLatin1String("EG111"))
                eml = static_cast<MultiLayer *>(mw2);
        }
        CHECK_COND(eml != nullptr && !eml->layersList().isEmpty(), "111e: graph EG111");
        Graph *eg = eml->layersList().at(0);
        auto legendE = [&]() { return eg->legend() ? eg->legend()->text().toStdString() : std::string(); };
        auto undoE = [&](bool undo) {
            return !isErr(rpc(QString::fromLatin1(undo ? "app.undo" : "app.redo"),
                              QJsonObject{ { "table", "E111" } }));
        };
        const std::string e0 = sigE();
        const std::string leg0 = legendE();
        std::cout << "[INFO] 111e: EG111 " << e0 << "\n";
        CHECK_COND(e0.find("E111_y:#ff0000:4.000000:4;") == 0 && e0.find("E111_z:") != std::string::npos,
                   "111e: setup: " << e0);

        // y emptied (all cells empty), then Undo: the curve stays (empty) and comes back as it was
        r = rpc(QString::fromLatin1("table.set_column_data"),
                QJsonObject{ { "table", "E111" }, { "col", "y" },
                             { "data", QJsonArray{ QJsonValue(), QJsonValue(), QJsonValue(), QJsonValue() } } });
        CHECK_COND(!isErr(r), "111e: clear y: " << errText(r));
        const std::string e1 = sigE();
        std::cout << "[INFO] 111e: y emptied " << e1 << "\n";
        CHECK_COND(e1.find("E111_y:#ff0000:4.000000:0;") == 0,
                   "111e: the curve of the emptied y left the graph (or kept points): " << e1);
        CHECK_COND(legendE() == leg0, "111e: legend changed while y is empty: " << legendE());
        CHECK_COND(undoE(true), "111e: undo clear y");
        CHECK_COND(sigE() == e0 && legendE() == leg0,
                   "111e: undo of clearing y did not bring the curve back: " << sigE() << " expected " << e0);

        // a formula that makes every row invalid (1/0), then Undo
        r = rpc(QString::fromLatin1("table.set_formula"),
                QJsonObject{ { "table", "E111" }, { "col", "y" }, { "formula", "1/0" } });
        CHECK_COND(!isErr(r), "111e: formula 1/0: " << errText(r));
        std::cout << "[INFO] 111e: y = 1/0 " << sigE() << "\n";
        CHECK_COND(sigE().find("E111_y:#ff0000:4.000000:0;") == 0, "111e: y = 1/0: " << sigE());
        CHECK_COND(undoE(true), "111e: undo formula");
        CHECK_COND(sigE() == e0, "111e: undo of y = 1/0 did not bring the curve back: " << sigE());

        // undo the data entry of y and x (the table's first steps), then redo them
        CHECK_COND(undoE(true) && undoE(true), "111e: undo the data entry");
        std::cout << "[INFO] 111e: data entry undone " << sigE() << "\n";
        CHECK_COND(sigE().find("E111_y:#ff0000:4.000000:0;") == 0, "111e: data entry undone: " << sigE());
        CHECK_COND(undoE(false) && undoE(false), "111e: redo the data entry");
        CHECK_COND(sigE() == e0 && legendE() == leg0,
                   "111e: redo of the data entry did not bring the curves back: " << sigE() << " expected " << e0);

        // saving a graph with an empty curve works; plotting a column without data is refused
        r = rpc(QString::fromLatin1("table.set_column_data"),
                QJsonObject{ { "table", "E111" }, { "col", "z" },
                             { "data", QJsonArray{ QJsonValue(), QJsonValue(), QJsonValue(), QJsonValue() } } });
        CHECK_COND(!isErr(r), "111e: clear z: " << errText(r));
        QTemporaryDir tmp;
        CHECK_COND(tmp.isValid(), "111e: temp dir");
        const QString prj = tmp.filePath(QString::fromLatin1("e111.sciprj"));
        r = rpc(QString::fromLatin1("app.save_project"), QJsonObject{ { "path", prj } });
        CHECK_COND(!isErr(r) && QFileInfo(prj).size() > 0, "111e: save with an empty curve: " << errText(r));
        {
            // the saved project opens with the empty curve in place, as it was, and the curve
            // shows the data when the column gets it
            AppWinGuard111 rd(new ApplicationWindow());
            rd->applyUserSettings();
            CHECK_COND(rd->loadProject(prj), "111e: reopening the project failed");
            QCoreApplication::processEvents();
            MultiLayer *rml = nullptr;
            for (MyWidget *mw2 : rd->windowsList()) {
                if (mw2 && mw2->inherits("MultiLayer") && mw2->name() == QLatin1String("EG111"))
                    rml = static_cast<MultiLayer *>(mw2);
            }
            CHECK_COND(rml != nullptr && !rml->layersList().isEmpty(), "111e: EG111 after reopening");
            Graph *rg = rml->layersList().at(0);
            auto rsig = [&]() {
                std::string s;
                for (int i = 0; i < rg->curves(); ++i) {
                    QwtPlotCurve *c = rg->curve(i);
                    s += (c ? c->title().text().toStdString() + ":" + c->pen().color().name().toStdString()
                                      + ":" + std::to_string(c->dataSize())
                            : std::string("?"))
                            + ";";
                }
                return s;
            };
            std::cout << "[INFO] 111e: reopened EG111 " << rsig() << "\n";
            CHECK_COND(rsig() == "E111_y:#ff0000:4;E111_z:#d55e00:0;",
                       "111e: the empty curve was not read back in place: " << rsig());
            Table *rt = rd->table(QString::fromLatin1("E111"));
            CHECK_COND(rt != nullptr, "111e: table E111 after reopening");
            const int zc = rt->colIndex(QString::fromLatin1("E111_z"));
            CHECK_COND(zc >= 0, "111e: column z after reopening");
            for (int row = 0; row < 4; ++row)
                rt->column(zc)->setValueAt(row, double(row + 2));
            rd->updateCurves(rt, QString::fromLatin1("E111_z"));
            CHECK_COND(rsig() == "E111_y:#ff0000:4;E111_z:#d55e00:4;",
                       "111e: the reopened empty curve did not take the data: " << rsig());
        }
        CHECK_COND(undoE(true) && sigE() == e0, "111e: undo clear z: " << sigE());
        r = rpc(QString::fromLatin1("plot.xy"),
                QJsonObject{ { "table", "E111" }, { "x_col", "x" }, { "y_cols", QJsonArray{ "w" } },
                             { "graph_name", "EW111" } });
        std::cout << "[INFO] 111e: plotting the empty column w: "
                  << (isErr(r) ? errText(r) : std::string("no error")) << "\n";
        CHECK_COND(isErr(r), "111e: plotting a column without data is no longer refused");
        std::cout << "[PASS] 111e: an emptied column keeps its curve and Undo / Redo bring the data back\n";
    }

    // ---- 111f: a box whose column is empty for a while keeps its place ----
    {
        Table *t = w111->newTable(QString::fromLatin1("B111"), 5, 2);
        CHECK_COND(t != nullptr, "111f: table B111");
        t->setColName(0, QString::fromLatin1("a"));
        t->setColName(1, QString::fromLatin1("b"));
        CHECK_COND(setCol("B111", "a", { 1, 2, 3, 4, 5 }) && setCol("B111", "b", { 3, 5, 4, 6, 8 }),
                   "111f: fill B111");
        QJsonObject r = rpc(QString::fromLatin1("plot.box"),
                            QJsonObject{ { "table", "B111" }, { "cols", QJsonArray{ "a", "b" } },
                                         { "graph_name", "BG111" } });
        CHECK_COND(!isErr(r), "111f: plot.box: " << errText(r));
        MultiLayer *bml = nullptr;
        for (MyWidget *mw2 : w111->windowsList()) {
            if (mw2 && mw2->inherits("MultiLayer") && mw2->name() == QLatin1String("BG111"))
                bml = static_cast<MultiLayer *>(mw2);
        }
        CHECK_COND(bml != nullptr && !bml->layersList().isEmpty(), "111f: graph BG111");
        Graph *bg = bml->layersList().at(0);
        // "title@position:points;" of the boxes
        auto boxSig = [&]() {
            std::string s;
            for (int i = 0; i < bg->curves(); ++i) {
                auto *bc = dynamic_cast<BoxCurve *>(bg->curve(i));
                s += bc ? bc->title().text().toStdString() + "@" + std::to_string(bc->x(0)) + ":"
                                + std::to_string(bc->dataSize())
                        : std::string("?");
                s += ";";
            }
            return s;
        };
        const std::string b0 = boxSig();
        std::cout << "[INFO] 111f: BG111 " << b0 << "\n";
        CHECK_COND(bg->curves() == 2 && b0.find(":5;") != std::string::npos, "111f: setup: " << b0);

        r = rpc(QString::fromLatin1("table.set_column_data"),
                QJsonObject{ { "table", "B111" }, { "col", "a" },
                             { "data", QJsonArray{ QJsonValue(), QJsonValue(), QJsonValue(), QJsonValue(),
                                                   QJsonValue() } } });
        CHECK_COND(!isErr(r), "111f: clear a: " << errText(r));
        const std::string b1 = boxSig();
        std::cout << "[INFO] 111f: a emptied " << b1 << "\n";
        CHECK_COND(bg->curves() == 2 && b1.rfind(b0.substr(0, b0.find(':')) + ":0;", 0) == 0,
                   "111f: the box of the emptied column left the graph (or moved): " << b1);
        CHECK_COND(!isErr(rpc(QString::fromLatin1("app.undo"), QJsonObject{ { "table", "B111" } })),
                   "111f: undo clear a");
        CHECK_COND(boxSig() == b0, "111f: undo did not bring the box back: " << boxSig() << " expected " << b0);
        std::cout << "[PASS] 111f: a box keeps its place while its column is empty\n";
    }

    // ---- 111g: curves of a table that leaves the project come back with it ----
    {
        Table *t = w111->newTable(QString::fromLatin1("I111"), 6, 2);
        CHECK_COND(t != nullptr && t->d_future_table && t->d_future_table->undoStack(), "111g: table I111");
        t->setColName(0, QString::fromLatin1("x"));
        t->setColName(1, QString::fromLatin1("y"));
        CHECK_COND(setCol("I111", "x", { 0, 1, 2, 3, 4, 5 }) && setCol("I111", "y", { 1, 2, 4, 3, 5, 6 }),
                   "111g: fill I111");
        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "I111" }, { "x_col", "x" },
                                         { "y_cols", QJsonArray{ "y" } }, { "graph_name", "IG111" } });
        CHECK_COND(!isErr(r), "111g: plot IG111: " << errText(r));
        r = rpc(QString::fromLatin1("graph.set_curve_style"),
                QJsonObject{ { "graph", "IG111" }, { "curve", 0 }, { "color", "#00aa00" }, { "lw", 3.0 } });
        CHECK_COND(!isErr(r), "111g: style: " << errText(r));
        MultiLayer *iml = nullptr;
        for (MyWidget *mw2 : w111->windowsList()) {
            if (mw2 && mw2->inherits("MultiLayer") && mw2->name() == QLatin1String("IG111"))
                iml = static_cast<MultiLayer *>(mw2);
        }
        CHECK_COND(iml != nullptr && !iml->layersList().isEmpty(), "111g: graph IG111");
        Graph *ig = iml->layersList().at(0);
        auto isig = [&]() {
            std::string s;
            for (int i = 0; i < ig->curves(); ++i) {
                QwtPlotCurve *c = ig->curve(i);
                s += (c ? c->title().text().toStdString() + ":" + c->pen().color().name().toStdString()
                                  + ":" + std::to_string(c->dataSize())
                        : std::string("?"))
                        + ";";
            }
            return s + "|" + (ig->legend() ? ig->legend()->text().toStdString() : std::string());
        };
        QUndoStack *ps = t->d_future_table->undoStack();

        // a table made by an RPC (table.create) with a curve on IG111: undo the table's
        // creation (its curve goes with it), then redo it: the curve comes back. (The project
        // stack may be at its Undo limit, so its index does not count the steps.)
        r = rpc(QString::fromLatin1("table.create"),
                QJsonObject{ { "rows", 4 }, { "cols", 2 }, { "name", "J111" },
                             { "col_names", QJsonArray{ "u", "v" } } });
        CHECK_COND(!isErr(r), "111g: table.create J111: " << errText(r));
        CHECK_COND(setCol("J111", "u", { 0, 2, 4, 5 }) && setCol("J111", "v", { 6, 5, 7, 4 }),
                   "111g: fill J111");
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "IG111" }, { "table", "J111" }, { "x_col", "u" }, { "y_col", "v" } });
        CHECK_COND(!isErr(r), "111g: add J111 curve: " << errText(r));
        r = rpc(QString::fromLatin1("graph.set_curve_style"),
                QJsonObject{ { "graph", "IG111" }, { "curve", 1 }, { "color", "#aa00aa" }, { "lw", 2.5 } });
        CHECK_COND(!isErr(r), "111g: style J111 curve: " << errText(r));
        const std::string g1 = isig();
        std::cout << "[INFO] 111g: with J111 " << g1 << "\n";
        CHECK_COND(ig->curves() == 2, "111g: setup with J111: " << g1);
        int steps = 0;
        while (w111->table(QString::fromLatin1("J111")) != nullptr && ps->canUndo() && steps < 20) {
            ps->undo();
            ++steps;
        }
        QCoreApplication::processEvents();
        std::cout << "[INFO] 111g: J111 creation undone " << isig() << "\n";
        CHECK_COND(w111->table(QString::fromLatin1("J111")) == nullptr && ig->curves() == 1,
                   "111g: undoing the creation of J111: " << isig());
        for (int k = 0; k < steps; ++k)
            ps->redo();
        QCoreApplication::processEvents();
        CHECK_COND(w111->table(QString::fromLatin1("J111")) != nullptr && isig() == g1,
                   "111g: redo of the creation of J111 did not bring its curve back: " << isig()
                                                                                       << " expected " << g1);

        // integrate's result table and curve: undo the result table's creation, then redo it
        {
            r = rpc(QString::fromLatin1("curvemath.integrate"),
                    QJsonObject{ { "graph", "IG111" }, { "curve", 0 }, { "plot_curve", true } });
            CHECK_COND(!isErr(r), "111g: integrate: " << errText(r));
            const QString outCurve = res(r).value(QString::fromLatin1("output_curve")).toString();
            const QString outTable = outCurve.section(QLatin1Char('_'), 0, 0);
            const std::string gi = isig();
            std::cout << "[INFO] 111g: integrate -> " << outTable.toStdString() << " " << gi << "\n";
            CHECK_COND(!outTable.isEmpty() && w111->table(outTable) != nullptr
                               && gi.find(outCurve.toStdString() + ":") != std::string::npos,
                       "111g: integrate did not add a result table and curve: " << gi);
            // app.undo / app.redo without a window, as a script does
            int isteps = 0;
            while (w111->table(outTable) != nullptr && isteps < 20) {
                CHECK_COND(!isErr(rpc(QString::fromLatin1("app.undo"), QJsonObject{})), "111g: app.undo");
                ++isteps;
            }
            std::cout << "[INFO] 111g: result table undone (" << isteps << " step(s)) " << isig() << "\n";
            CHECK_COND(w111->table(outTable) == nullptr
                               && isig().find(outCurve.toStdString() + ":") == std::string::npos,
                       "111g: undoing the result table: " << isig());
            for (int k = 0; k < isteps; ++k)
                CHECK_COND(!isErr(rpc(QString::fromLatin1("app.redo"), QJsonObject{})), "111g: app.redo");
            CHECK_COND(w111->table(outTable) != nullptr && isig() == gi,
                       "111g: redo of the result table did not bring its curve back: " << isig()
                                                                                         << " expected " << gi);
            r = rpc(QString::fromLatin1("plot.remove_curve"),
                    QJsonObject{ { "graph", "IG111" }, { "curve", outCurve } });
            CHECK_COND(!isErr(r), "111g: remove the result curve: " << errText(r));
        }

        // a column added by an RPC with a curve: undo the column's addition (the curve goes with
        // it, as the result columns of integrate and the like), then redo it: the curve comes back
        r = rpc(QString::fromLatin1("table.add_column"),
                QJsonObject{ { "table", "I111" }, { "name", "w" }, { "data", QJsonArray{ 9, 8, 7, 8, 9, 7 } } });
        CHECK_COND(!isErr(r), "111g: add column w: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "IG111" }, { "table", "I111" }, { "x_col", "x" }, { "y_col", "w" } });
        CHECK_COND(!isErr(r), "111g: add w curve: " << errText(r));
        const std::string gw = isig();
        CHECK_COND(gw.find("I111_w:") != std::string::npos, "111g: setup with w: " << gw);
        int wsteps = 0;
        while (t->colIndex(QString::fromLatin1("I111_w")) >= 0 && ps->canUndo() && wsteps < 20) {
            ps->undo();
            ++wsteps;
        }
        QCoreApplication::processEvents();
        std::cout << "[INFO] 111g: column w undone (" << wsteps << " step(s)) " << isig() << "\n";
        CHECK_COND(isig().find("I111_w:") == std::string::npos, "111g: the curve of w stayed: " << isig());
        for (int k = 0; k < wsteps; ++k)
            ps->redo();
        QCoreApplication::processEvents();
        CHECK_COND(isig() == gw, "111g: redo of column w did not bring its curve back: " << isig()
                                                                                       << " expected " << gw);
        r = rpc(QString::fromLatin1("plot.remove_curve"),
                QJsonObject{ { "graph", "IG111" }, { "curve", "I111_w" } });
        CHECK_COND(!isErr(r), "111g: remove w curve: " << errText(r));

        // the user deletes the data table: Undo brings it back with its curves, Redo takes them again
        const std::string g2 = isig();
        w111->closeWindow(t);
        QCoreApplication::processEvents();
        std::cout << "[INFO] 111g: I111 deleted " << isig() << "\n";
        CHECK_COND(w111->table(QString::fromLatin1("I111")) == nullptr && ig->curves() < 2,
                   "111g: deleting I111: " << isig());
        ps->undo();
        QCoreApplication::processEvents();
        CHECK_COND(w111->table(QString::fromLatin1("I111")) != nullptr && isig() == g2,
                   "111g: undo of deleting I111 did not bring its curves back: " << isig() << " expected " << g2);
        ps->redo();
        QCoreApplication::processEvents();
        CHECK_COND(w111->table(QString::fromLatin1("I111")) == nullptr && ig->curves() < 2,
                   "111g: redo of deleting I111: " << isig());
        ps->undo();
        QCoreApplication::processEvents();
        CHECK_COND(isig() == g2, "111g: second undo of deleting I111: " << isig());
        std::cout << "[PASS] 111g: curves of a table that leaves the project come back with it\n";
    }

    // ---- 111h: held curves go back only to their own layer, and only with their tables ----
    auto findGraph = [](ApplicationWindow *app, const char *name) -> MultiLayer * {
        for (MyWidget *mw2 : app->windowsList()) {
            if (mw2 && mw2->inherits("MultiLayer") && mw2->name() == QLatin1String(name))
                return static_cast<MultiLayer *>(mw2);
        }
        return nullptr;
    };
    auto layerTitles = [](Graph *g) {
        std::string s;
        for (int i = 0; i < g->curves(); ++i) {
            QwtPlotCurve *c = g->curve(i);
            s += (c ? c->title().text().toStdString() : std::string("?"))
                    + (dynamic_cast<QwtErrorPlotCurve *>(c) ? "(err)" : "") + ";";
        }
        return s;
    };
    {
        // (1) the layer the curve left is deleted by the user: the curve is not put into the
        //     layer that now has its number
        Table *k = w111->newTable(QString::fromLatin1("K111"), 4, 2);
        Table *l = w111->newTable(QString::fromLatin1("L111"), 4, 2);
        CHECK_COND(k && l && k->d_future_table, "111h: tables K111 / L111");
        k->setColName(0, QString::fromLatin1("x"));
        k->setColName(1, QString::fromLatin1("y"));
        l->setColName(0, QString::fromLatin1("x"));
        l->setColName(1, QString::fromLatin1("y"));
        CHECK_COND(setCol("K111", "x", { 1, 2, 3, 4 }) && setCol("K111", "y", { 4, 3, 2, 1 })
                           && setCol("L111", "x", { 1, 2, 3, 4 }) && setCol("L111", "y", { 100, 200, 300, 400 }),
                   "111h: fill K111 / L111");
        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "K111" }, { "x_col", "x" },
                                         { "y_cols", QJsonArray{ "y" } }, { "graph_name", "KG111" } });
        CHECK_COND(!isErr(r), "111h: plot KG111: " << errText(r));
        r = rpc(QString::fromLatin1("graph.add_layer"), QJsonObject{ { "graph", "KG111" } });
        CHECK_COND(!isErr(r), "111h: add layer: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "KG111" }, { "table", "L111" }, { "x_col", "x" },
                             { "y_col", "y" }, { "layer", 1 } });
        CHECK_COND(!isErr(r), "111h: L111 curve on layer 1: " << errText(r));
        MultiLayer *kml = findGraph(w111, "KG111");
        CHECK_COND(kml && kml->layersList().size() == 2, "111h: KG111 with 2 layers");
        QUndoStack *ps = k->d_future_table->undoStack();
        w111->closeWindow(k);
        QCoreApplication::processEvents();
        CHECK_COND(w111->table(QString::fromLatin1("K111")) == nullptr
                           && kml->layersList().at(0)->curves() == 0,
                   "111h: deleting K111: " << layerTitles(kml->layersList().at(0)));
        r = rpc(QString::fromLatin1("graph.remove_layer"), QJsonObject{ { "graph", "KG111" }, { "layer", 0 } });
        CHECK_COND(!isErr(r) && kml->layersList().size() == 1, "111h: remove layer 0: " << errText(r));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); // the closed layer goes
        ps->undo(); // K111 comes back
        QCoreApplication::processEvents();
        const std::string left = layerTitles(kml->layersList().at(0));
        std::cout << "[INFO] 111h: K111 back, the layer left: " << left << "\n";
        CHECK_COND(w111->table(QString::fromLatin1("K111")) != nullptr && left == "L111_y;",
                   "111h: the curve of the deleted layer went into another layer: " << left);
    }
    {
        // (2) error bars on a curve of another table: Undo / Redo of the graph (its layers are
        //     replaced) while the table is out, then the table comes back: the curve and its
        //     error bars come back
        Table *m = w111->newTable(QString::fromLatin1("M111"), 4, 3);
        CHECK_COND(m && m->d_future_table, "111h: table M111");
        m->setColName(0, QString::fromLatin1("x"));
        m->setColName(1, QString::fromLatin1("y"));
        m->setColName(2, QString::fromLatin1("e"));
        CHECK_COND(setCol("M111", "x", { 1, 2, 3, 4 }) && setCol("M111", "y", { 2, 4, 3, 5 })
                           && setCol("M111", "e", { 0.1, 0.2, 0.3, 0.4 }),
                   "111h: fill M111");
        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "M111" }, { "x_col", "x" },
                                         { "y_cols", QJsonArray{ "y" } }, { "graph_name", "MG111" } });
        CHECK_COND(!isErr(r), "111h: plot MG111: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "MG111" }, { "table", "L111" }, { "x_col", "x" }, { "y_col", "y" } });
        CHECK_COND(!isErr(r), "111h: L111 curve on MG111: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_error_bars"),
                QJsonObject{ { "graph", "MG111" }, { "err_col", "e" }, { "curve", "M111_y" },
                             { "table", "M111" } });
        CHECK_COND(!isErr(r), "111h: error bars: " << errText(r));
        MultiLayer *mml = findGraph(w111, "MG111");
        CHECK_COND(mml && !mml->layersList().isEmpty(), "111h: MG111");
        const std::string m0 = layerTitles(mml->layersList().at(0));
        std::cout << "[INFO] 111h: MG111 " << m0 << "\n";
        QUndoStack *ps = m->d_future_table->undoStack();
        w111->closeWindow(m);
        QCoreApplication::processEvents();
        // a graph step and its Undo: the layers are rebuilt from the snapshot
        r = rpc(QString::fromLatin1("graph.set_axis"),
                QJsonObject{ { "graph", "MG111" }, { "axis", "x" }, { "min", 0 }, { "max", 9 } });
        CHECK_COND(!isErr(r), "111h: set_axis: " << errText(r));
        r = rpc(QString::fromLatin1("app.undo"), QJsonObject{ { "graph", "MG111" } });
        CHECK_COND(!isErr(r), "111h: undo set_axis: " << errText(r));
        ps->undo(); // M111 comes back
        QCoreApplication::processEvents();
        const std::string m1 = layerTitles(mml->layersList().at(0));
        std::cout << "[INFO] 111h: M111 back after a graph Undo: " << m1 << "\n";
        CHECK_COND(m1.find("M111_y;") != std::string::npos && m1.find("(err)") != std::string::npos
                           && m1.find("L111_y;") != std::string::npos,
                   "111h: curve or error bars missing after the table came back: " << m1 << " (was " << m0 << ")");
    }
    {
        // (3) error bars from another table B, whose window is deleted while they are held
        //     (app.close_window deletes the window; its table stays with the Undo step): when
        //     another table comes back, the held error bars are not read through the deleted
        //     window (use after free) but dropped; the curve on table A stays as it is
        AppWinGuard111 wl(new ApplicationWindow());
        wl->applyUserSettings();
        wl->confirmCloseTable = false;
        PythonAutomationServer *lsrv = wl->pythonAutomationServer();
        CHECK_COND(lsrv != nullptr, "111h: server of the second window");
        int lid = 11900;
        auto lrpc = [&](const QString &method, const QJsonObject &params) {
            QJsonObject req;
            req[QString::fromLatin1("jsonrpc")] = QString::fromLatin1("2.0");
            req[QString::fromLatin1("id")] = ++lid;
            req[QString::fromLatin1("method")] = method;
            req[QString::fromLatin1("params")] = params;
            const QJsonObject resp = lsrv->handleRpcRequest(req);
            QCoreApplication::processEvents();
            return resp;
        };
        auto lset = [&](const char *table, const char *col, const QVector<double> &v) {
            return !isErr(lrpc(QString::fromLatin1("table.set_column_data"),
                               QJsonObject{ { "table", table }, { "col", col }, { "data", toArr(v) } }));
        };
        Table *a = wl->newTable(QString::fromLatin1("A111"), 4, 2);
        Table *b = wl->newTable(QString::fromLatin1("B111e"), 4, 1);
        Table *c = wl->newTable(QString::fromLatin1("C111"), 4, 1);
        CHECK_COND(a && b && c && a->d_future_table && b->d_future_table, "111h: tables A / B / C");
        a->setColName(0, QString::fromLatin1("x"));
        a->setColName(1, QString::fromLatin1("y"));
        b->setColName(0, QString::fromLatin1("e"));
        c->setColName(0, QString::fromLatin1("z"));
        CHECK_COND(lset("A111", "x", { 1, 2, 3, 4 }) && lset("A111", "y", { 3, 1, 4, 1 })
                           && lset("B111e", "e", { 0.5, 0.5, 0.5, 0.5 }) && lset("C111", "z", { 1, 2, 3, 4 }),
                   "111h: fill A / B / C");
        QJsonObject r = lrpc(QString::fromLatin1("plot.xy"),
                             QJsonObject{ { "table", "A111" }, { "x_col", "x" },
                                          { "y_cols", QJsonArray{ "y" } }, { "graph_name", "AG111" } });
        CHECK_COND(!isErr(r), "111h: plot AG111: " << errText(r));
        r = lrpc(QString::fromLatin1("plot.add_error_bars"),
                 QJsonObject{ { "graph", "AG111" }, { "err_col", "e" }, { "curve", "A111_y" },
                              { "table", "B111e" } });
        CHECK_COND(!isErr(r), "111h: error bars from B: " << errText(r));
        MultiLayer *aml = findGraph(wl, "AG111");
        CHECK_COND(aml && !aml->layersList().isEmpty(), "111h: AG111");
        Graph *ag = aml->layersList().at(0);
        std::cout << "[INFO] 111h: AG111 " << layerTitles(ag) << "\n";
        QPointer<future::Table> fb(b->d_future_table);
        QPointer<Table> bw(b);
        QUndoStack *ps = a->d_future_table->undoStack();
        r = lrpc(QString::fromLatin1("app.close_window"), QJsonObject{ { "name", "B111e" } });
        CHECK_COND(!isErr(r), "111h: close_window B111e: " << errText(r));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        // the window of B goes for good while its curves are held (app.close_window used to
        // delete it; it now keeps it for the Undo, 111m)
        if (bw)
            delete bw.data();
        std::cout << "[INFO] 111h: B111e closed (its table kept: " << (fb ? "yes" : "no") << ") "
                  << layerTitles(ag) << "\n";
        CHECK_COND(layerTitles(ag) == "A111_y;", "111h: closing B111e: " << layerTitles(ag));
        // another table goes and comes back: the held curves are looked at
        wl->closeWindow(c);
        QCoreApplication::processEvents();
        ps->undo();
        QCoreApplication::processEvents();
        const bool heldLeft = fb && fb->findChild<QObject *>(QString::fromLatin1("_tg_held_removed_curves"),
                                                             Qt::FindDirectChildrenOnly);
        std::cout << "[INFO] 111h: C111 back: " << layerTitles(ag) << ", error bars of B still held: "
                  << (heldLeft ? "yes" : "no") << "\n";
        CHECK_COND(wl->table(QString::fromLatin1("C111")) != nullptr && layerTitles(ag) == "A111_y;",
                   "111h: after C111 came back: " << layerTitles(ag));
        CHECK_COND(!heldLeft, "111h: the error bars of the deleted window B111e are still held (read "
                              "through a deleted window)");
        wl->savedProject();
    }
    std::cout << "[PASS] 111h: held curves go back only to their own layer and with their tables\n";

    // ---- 111i: curves held while the graph's Undo / Redo rebuilds its layers ----
    // (each case is checked on its own, so that every one that fails is reported)
    int fails111i = 0;
    auto soft111i = [&fails111i](bool ok, const std::string &msg) {
        if (!ok) {
            std::cerr << "[FAIL] " << msg << std::endl;
            ++fails111i;
        }
        return ok;
    };
    {
        auto graphStep = [&](const char *graph, bool undo) {
            return !isErr(rpc(QString::fromLatin1(undo ? "app.undo" : "app.redo"),
                              QJsonObject{ { "graph", graph } }));
        };
        auto tableStep = [&](const char *table, bool undo) {
            return !isErr(rpc(QString::fromLatin1(undo ? "app.undo" : "app.redo"),
                              QJsonObject{ { "table", table } }));
        };
        // a graph step and its Undo: every layer is rebuilt from the snapshot
        auto rebuild = [&](const char *graph, const char *title) {
            const QJsonObject r = rpc(QString::fromLatin1("graph.set_title"),
                                      QJsonObject{ { "graph", graph }, { "title", title } });
            return !isErr(r) && graphStep(graph, true);
        };
        auto legendOf = [](Graph *g) {
            return g->legend() ? g->legend()->text().toStdString() : std::string();
        };
        auto tableNames = [&]() {
            QStringList names;
            for (MyWidget *mw2 : w111->windowsList()) {
                if (mw2 && mw2->inherits("Table"))
                    names << mw2->name();
            }
            return names;
        };

        // (1) a removed column, then the graph's Undo (twice) and Undo / Redo: the column's Undo
        //     brings the curve, its error bars and its legend line back (TakenCurvesUndoCommand
        //     lost its layer); Redo takes them again from a layer rebuilt meanwhile
        {
            Table *t = w111->newTable(QString::fromLatin1("PT111"), 6, 4);
            CHECK_COND(t && t->d_future_table, "111i: table PT111");
            const char *pc[] = { "x", "y", "z", "e" };
            for (int c = 0; c < 4; ++c)
                t->setColName(c, QString::fromLatin1(pc[c]));
            CHECK_COND(setCol("PT111", "x", { 1, 2, 3, 4, 5, 6 })
                               && setCol("PT111", "y", { 1, 4, 9, 16, 25, 36 })
                               && setCol("PT111", "z", { 2, 4, 6, 8, 10, 12 })
                               && setCol("PT111", "e", { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 }),
                       "111i: fill P111");
            QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                                QJsonObject{ { "table", "PT111" },
                                              { "x_col", "x" },
                                              { "y_cols", QJsonArray{ "y", "z" } },
                                              { "graph_name", "PG111" } });
            CHECK_COND(!isErr(r), "111i: plot PG111: " << errText(r));
            r = rpc(QString::fromLatin1("plot.add_error_bars"),
                    QJsonObject{ { "graph", "PG111" },
                                  { "err_col", "e" },
                                  { "curve", "PT111_z" },
                                  { "table", "PT111" } });
            CHECK_COND(!isErr(r), "111i: error bars: " << errText(r));
            r = rpc(QString::fromLatin1("graph.set_legend"),
                    QJsonObject{ { "graph", "PG111" }, { "visible", true } });
            CHECK_COND(!isErr(r), "111i: legend: " << errText(r));
            MultiLayer *pml = findGraph(w111, "PG111");
            CHECK_COND(pml && pml->layersList().size() == 1, "111i: PG111");
            auto psig = [&]() {
                Graph *g = pml->layersList().at(0);
                return layerTitles(g) + "|" + legendOf(g);
            };
            const std::string p0 = psig();
            std::cout << "[INFO] 111i: PG111 " << p0 << "\n";
            CHECK_COND(p0.find("PT111_z;") != std::string::npos
                               && p0.find("(err)") != std::string::npos,
                       "111i: setup of PG111: " << p0);
            r = rpc(QString::fromLatin1("table.remove_column"),
                    QJsonObject{ { "table", "PT111" }, { "col", "z" } });
            CHECK_COND(!isErr(r), "111i: remove z: " << errText(r));
            CHECK_COND(psig().find("PT111_z") == std::string::npos,
                       "111i: z curve left: " << psig());
            CHECK_COND(rebuild("PG111", "t1") && rebuild("PG111", "t2"),
                       "111i: graph steps undone");
            r = rpc(QString::fromLatin1("graph.set_title"),
                    QJsonObject{ { "graph", "PG111" }, { "title", "t3" } });
            CHECK_COND(!isErr(r) && graphStep("PG111", true) && graphStep("PG111", false),
                       "111i: graph Undo / Redo");
            CHECK_COND(tableStep("PT111", true), "111i: undo remove z");
            std::cout << "[INFO] 111i: z back after 4 rebuilds: " << psig() << "\n";
            if (soft111i(psig() == p0,
                         "111i (1): undoing the removal of z after the graph's Undo: " + psig()
                                 + " expected " + p0)) {
                // the layer is rebuilt again while the curve is back: Redo takes the curve of the
                // rebuilt layer, Undo brings it back
                CHECK_COND(rebuild("PG111", "t4"), "111i: graph step undone (2)");
                CHECK_COND(tableStep("PT111", false), "111i: redo remove z");
                CHECK_COND(psig().find("PT111_z") == std::string::npos,
                           "111i: Redo left the z curve: " << psig());
                CHECK_COND(rebuild("PG111", "t5"), "111i: graph step undone (3)");
                CHECK_COND(tableStep("PT111", true), "111i: undo remove z (2)");
                soft111i(psig() == p0,
                         "111i (1): second Undo of removing z: " + psig() + " expected " + p0);
            }
            // only the error column: the error bars are taken without their curve, which the
            // graph's Undo then rebuilds; the column's Undo hangs them on the rebuilt curve
            r = rpc(QString::fromLatin1("table.remove_column"),
                    QJsonObject{ { "table", "PT111" }, { "col", "e" } });
            CHECK_COND(!isErr(r), "111i: remove e: " << errText(r));
            CHECK_COND(psig().find("(err)") == std::string::npos,
                       "111i: error bars left: " << psig());
            CHECK_COND(rebuild("PG111", "t6"), "111i: graph step undone (4)");
            CHECK_COND(tableStep("PT111", true), "111i: undo remove e");
            bool linked = false;
            Graph *pg = pml->layersList().at(0);
            for (int i = 0; i < pg->curves(); ++i) {
                auto *er = dynamic_cast<QwtErrorPlotCurve *>(pg->curve(i));
                DataCurve *m = er ? er->masterCurve() : nullptr;
                linked = linked
                        || (m && pg->curveIndex(static_cast<QwtPlotCurve *>(m)) >= 0
                            && m->title().text() == QLatin1String("PT111_z")
                            && m->errorBarsList().contains(er));
            }
            std::cout << "[INFO] 111i: e back after a rebuild: " << psig()
                      << (linked ? " (linked)" : "") << "\n";
            soft111i(psig() == p0 && linked,
                     "111i (1): the error bars did not come back on the rebuilt curve: " + psig());
        }

        // (2) a layer added, a curve held (its table's creation undone), the graph's "add layer"
        //     undone: layer 0 is still there and gets its curve back (the held curve used to be
        //     dropped because the window had fewer layers than when it was taken)
        {
            Table *t = w111->newTable(QString::fromLatin1("ST111"), 41, 2);
            CHECK_COND(t && t->d_future_table, "111i: table ST111");
            t->setColName(0, QString::fromLatin1("x"));
            t->setColName(1, QString::fromLatin1("y"));
            QVector<double> xs, ys;
            for (int i = 0; i < 41; ++i) {
                xs << 0.25 * i;
                ys << 0.5 * 0.25 * i + 1 + ((i % 3) - 1) * 0.1;
            }
            CHECK_COND(setCol("ST111", "x", xs) && setCol("ST111", "y", ys), "111i: fill S111");
            QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                                QJsonObject{ { "table", "ST111" },
                                              { "x_col", "x" },
                                              { "y_cols", QJsonArray{ "y" } },
                                              { "graph_name", "SG111" } });
            CHECK_COND(!isErr(r), "111i: plot SG111: " << errText(r));
            const QStringList before = tableNames();
            r = rpc(QString::fromLatin1("signal.smooth"),
                    QJsonObject{ { "graph", "SG111" },
                                  { "curve", 0 },
                                  { "method", "savgol" },
                                  { "window", 5 },
                                  { "poly_order", 2 } });
            CHECK_COND(!isErr(r), "111i: smooth: " << errText(r));
            QString outTable;
            for (const QString &n : tableNames()) {
                if (!before.contains(n))
                    outTable = n;
            }
            MultiLayer *sml = findGraph(w111, "SG111");
            CHECK_COND(!outTable.isEmpty() && sml && sml->layersList().size() == 1,
                       "111i: smoothing made no table");
            const std::string s0 = layerTitles(sml->layersList().at(0));
            std::cout << "[INFO] 111i: SG111 " << s0 << " (" << outTable.toStdString() << ")\n";
            CHECK_COND(sml->layersList().at(0)->curves() == 2, "111i: setup of SG111: " << s0);
            r = rpc(QString::fromLatin1("graph.add_layer"), QJsonObject{ { "graph", "SG111" } });
            CHECK_COND(!isErr(r) && sml->layersList().size() == 2,
                       "111i: add layer: " << errText(r));
            int steps = 0;
            while (w111->table(outTable) != nullptr && steps < 20) {
                CHECK_COND(!isErr(rpc(QString::fromLatin1("app.undo"),
                                      QJsonObject{ { "table", "ST111" } })),
                           "111i: undo of the smoothing");
                ++steps;
            }
            CHECK_COND(
                    w111->table(outTable) == nullptr
                            && layerTitles(sml->layersList().at(0)) == "ST111_y;",
                    "111i: undoing the smoothed table: " << layerTitles(sml->layersList().at(0)));
            CHECK_COND(graphStep("SG111", true) && sml->layersList().size() == 1,
                       "111i: undo add layer");
            for (int k = 0; k < steps; ++k)
                CHECK_COND(tableStep("ST111", false), "111i: redo of the smoothing");
            const std::string s1 = layerTitles(sml->layersList().at(0));
            std::cout << "[INFO] 111i: smoothed table back after the layer's Undo: " << s1 << "\n";
            soft111i(w111->table(outTable) != nullptr && s1 == s0,
                     "111i (2): the held curve was not put back into layer 0: " + s1 + " expected "
                             + s0);
        }

        // (3) the curve order: a held curve put back into a rebuilt layer goes after the curve that
        //     preceded it (the rebuilt layer numbered its curves anew: it went before Smoothed)
        {
            Table *t = w111->newTable(QString::fromLatin1("OT111"), 41, 2);
            CHECK_COND(t && t->d_future_table, "111i: table OT111");
            t->setColName(0, QString::fromLatin1("x"));
            t->setColName(1, QString::fromLatin1("y"));
            QVector<double> xs, ys;
            for (int i = 0; i < 41; ++i) {
                xs << 0.25 * i;
                ys << 0.5 * 0.25 * i + 1;
            }
            CHECK_COND(setCol("OT111", "x", xs) && setCol("OT111", "y", ys), "111i: fill O111");
            QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                                QJsonObject{ { "table", "OT111" },
                                              { "x_col", "x" },
                                              { "y_cols", QJsonArray{ "y" } },
                                              { "graph_name", "OG111" } });
            CHECK_COND(!isErr(r), "111i: plot OG111: " << errText(r));
            r = rpc(QString::fromLatin1("analysis.fit_linear"),
                    QJsonObject{ { "graph", "OG111" }, { "curve", 0 } });
            CHECK_COND(!isErr(r), "111i: fit: " << errText(r));
            r = rpc(QString::fromLatin1("plot.remove_curve"),
                    QJsonObject{ { "graph", "OG111" }, { "curve", 1 } });
            CHECK_COND(!isErr(r), "111i: remove the fit curve: " << errText(r));
            r = rpc(QString::fromLatin1("signal.smooth"),
                    QJsonObject{ { "graph", "OG111" },
                                  { "curve", 0 },
                                  { "method", "savgol" },
                                  { "window", 5 },
                                  { "poly_order", 2 } });
            CHECK_COND(!isErr(r), "111i: smooth OG111: " << errText(r));
            const QStringList before = tableNames();
            r = rpc(QString::fromLatin1("curvemath.integrate"),
                    QJsonObject{ { "graph", "OG111" }, { "curve", 0 }, { "plot_curve", true } });
            CHECK_COND(!isErr(r), "111i: integrate OG111: " << errText(r));
            QString outTable;
            for (const QString &n : tableNames()) {
                if (!before.contains(n))
                    outTable = n;
            }
            MultiLayer *oml = findGraph(w111, "OG111");
            CHECK_COND(!outTable.isEmpty() && oml && oml->layersList().size() == 1, "111i: OG111");
            const std::string o0 = layerTitles(oml->layersList().at(0));
            std::cout << "[INFO] 111i: OG111 " << o0 << "\n";
            CHECK_COND(oml->layersList().at(0)->curves() == 3, "111i: setup of OG111: " << o0);
            int steps = 0;
            while (w111->table(outTable) != nullptr && steps < 20) {
                CHECK_COND(!isErr(rpc(QString::fromLatin1("app.undo"),
                                      QJsonObject{ { "table", "OT111" } })),
                           "111i: undo of the integration");
                ++steps;
            }
            CHECK_COND(
                    w111->table(outTable) == nullptr && oml->layersList().at(0)->curves() == 2,
                    "111i: undoing the integrated table: " << layerTitles(oml->layersList().at(0)));
            CHECK_COND(rebuild("OG111", "t1") && rebuild("OG111", "t2"),
                       "111i: OG111 graph steps undone");
            for (int k = 0; k < steps; ++k)
                CHECK_COND(tableStep("OT111", false), "111i: redo of the integration");
            const std::string o1 = layerTitles(oml->layersList().at(0));
            std::cout << "[INFO] 111i: integrated table back after the graph's Undo: " << o1
                      << "\n";
            soft111i(o1 == o0, "111i (3): curve order changed: " + o1 + " expected " + o0);
        }

        // (4) the layer before the curve's layer is deleted and the deletion undone (the layers
        //     move up and back): the removed column's Undo puts the curve into its own layer 1,
        //     not into layer 0. (5) The user deletes the curve's layer after a rebuild: the
        //     curve does not go into the layer left.
        {
            Table *t = w111->newTable(QString::fromLatin1("QT111"), 4, 3);
            CHECK_COND(t && t->d_future_table, "111i: table QT111");
            t->setColName(0, QString::fromLatin1("x"));
            t->setColName(1, QString::fromLatin1("y"));
            t->setColName(2, QString::fromLatin1("q"));
            CHECK_COND(setCol("QT111", "x", { 1, 2, 3, 4 }) && setCol("QT111", "y", { 4, 3, 2, 1 })
                               && setCol("QT111", "q", { 10, 30, 20, 40 }),
                       "111i: fill Q111");
            QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                                QJsonObject{ { "table", "QT111" },
                                              { "x_col", "x" },
                                              { "y_cols", QJsonArray{ "y" } },
                                              { "graph_name", "QG111" } });
            CHECK_COND(!isErr(r), "111i: plot QG111: " << errText(r));
            r = rpc(QString::fromLatin1("graph.add_layer"), QJsonObject{ { "graph", "QG111" } });
            CHECK_COND(!isErr(r), "111i: QG111 add layer: " << errText(r));
            r = rpc(QString::fromLatin1("plot.add_curve"),
                    QJsonObject{ { "graph", "QG111" },
                                  { "table", "QT111" },
                                  { "x_col", "x" },
                                  { "y_col", "q" },
                                  { "layer", 1 } });
            CHECK_COND(!isErr(r), "111i: q curve on layer 1: " << errText(r));
            MultiLayer *qml = findGraph(w111, "QG111");
            CHECK_COND(qml && qml->layersList().size() == 2, "111i: QG111 with 2 layers");
            auto qsig = [&]() {
                std::string s;
                for (Graph *g : qml->layersList())
                    s += "[" + layerTitles(g) + "]";
                return s;
            };
            const std::string q0 = qsig();
            CHECK_COND(q0 == "[QT111_y;][QT111_q;]", "111i: setup of QG111: " << q0);
            r = rpc(QString::fromLatin1("table.remove_column"),
                    QJsonObject{ { "table", "QT111" }, { "col", "q" } });
            CHECK_COND(!isErr(r) && qsig() == "[QT111_y;][]", "111i: remove q: " << qsig());
            r = rpc(QString::fromLatin1("graph.remove_layer"),
                    QJsonObject{ { "graph", "QG111" }, { "layer", 0 } });
            CHECK_COND(!isErr(r) && qml->layersList().size() == 1,
                       "111i: remove layer 0: " << errText(r));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            CHECK_COND(graphStep("QG111", true) && qml->layersList().size() == 2,
                       "111i: undo remove layer 0");
            CHECK_COND(tableStep("QT111", true), "111i: undo remove q");
            std::cout << "[INFO] 111i: q back after layer 0 came back: " << qsig() << "\n";
            soft111i(qsig() == q0, "111i (4): the q curve did not go back to layer 1: " + qsig());

            // (5)
            CHECK_COND(tableStep("QT111", false) && qsig() == "[QT111_y;][]",
                       "111i: redo remove q: " << qsig());
            CHECK_COND(rebuild("QG111", "t1"), "111i: QG111 graph step undone");
            r = rpc(QString::fromLatin1("graph.remove_layer"),
                    QJsonObject{ { "graph", "QG111" }, { "layer", 1 } });
            CHECK_COND(!isErr(r) && qml->layersList().size() == 1,
                       "111i: remove layer 1: " << errText(r));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            CHECK_COND(tableStep("QT111", true), "111i: undo remove q (2)");
            std::cout << "[INFO] 111i: q back after its layer was deleted: " << qsig() << "\n";
            soft111i(qsig() == "[QT111_y;]",
                     "111i (5): the q curve went into another layer: " + qsig());
        }
    }
    CHECK_COND(fails111i == 0, "111i: " << fails111i << " case(s) failed");
    std::cout << "[PASS] 111i: held curves follow their layer through the graph's Undo / Redo\n";

    // ---- 111j: the legend text comes back as it was, empty lines included ----
    {
        int fails111j = 0;
        Table *t = w111->newTable(QString::fromLatin1("LT111"), 4, 4);
        CHECK_COND(t && t->d_future_table, "111j: table LT111");
        const char *lc[] = { "x", "a", "b", "d" };
        for (int c = 0; c < 4; ++c)
            t->setColName(c, QString::fromLatin1(lc[c]));
        CHECK_COND(setCol("LT111", "x", { 1, 2, 3, 4 }) && setCol("LT111", "a", { 1, 4, 9, 16 })
                           && setCol("LT111", "b", { 2, 3, 2, 3 }) && setCol("LT111", "d", { 2, 3, 4, 5 }),
                   "111j: fill LT111");
        QJsonObject r = rpc(QString::fromLatin1("plot.xy"),
                            QJsonObject{ { "table", "LT111" }, { "x_col", "x" },
                                         { "y_cols", QJsonArray{ "a" } }, { "graph_name", "LG111" } });
        CHECK_COND(!isErr(r), "111j: plot LG111: " << errText(r));
        r = rpc(QString::fromLatin1("graph.set_legend"), QJsonObject{ { "graph", "LG111" }, { "visible", true } });
        CHECK_COND(!isErr(r), "111j: legend: " << errText(r));
        MultiLayer *lml = findGraph(w111, "LG111");
        CHECK_COND(lml && !lml->layersList().isEmpty(), "111j: LG111");
        auto legendText = [&]() {
            Graph *g = lml->layersList().at(0);
            return g->legend() ? g->legend()->text().toStdString() : std::string("(none)");
        };
        auto shown = [](std::string s) {
            std::string o;
            for (char ch : s)
                o += (ch == '\n') ? std::string("\\n") : std::string(1, ch);
            return o;
        };
        QUndoStack *ps = t->d_future_table->undoStack();

        // (1) the last curve removed, another added: the legend starts with an empty line
        r = rpc(QString::fromLatin1("plot.remove_curve"), QJsonObject{ { "graph", "LG111" }, { "curve", 0 } });
        CHECK_COND(!isErr(r), "111j: remove curve a: " << errText(r));
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "LG111" }, { "table", "LT111" }, { "x_col", "x" }, { "y_col", "d" } });
        CHECK_COND(!isErr(r), "111j: add curve d: " << errText(r));
        const std::string l1 = legendText();
        std::cout << "[INFO] 111j: legend with d: " << shown(l1) << "\n";
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "LT111" }, { "col", "d" } });
        CHECK_COND(!isErr(r) && lml->layersList().at(0)->curves() == 0, "111j: remove d: " << errText(r));
        CHECK_COND(!isErr(rpc(QString::fromLatin1("app.undo"), QJsonObject{ { "table", "LT111" } })),
                   "111j: undo remove d");
        std::cout << "[INFO] 111j: d back: " << shown(legendText()) << "\n";
        if (legendText() != l1) {
            std::cerr << "[FAIL] 111j (1): legend after the column's Undo " << shown(legendText())
                      << " expected " << shown(l1) << std::endl;
            ++fails111j;
        }

        // (2) an empty line the user typed between two lines: the column's Undo and the table
        //     deletion's Undo keep it
        r = rpc(QString::fromLatin1("plot.add_curve"),
                QJsonObject{ { "graph", "LG111" }, { "table", "LT111" }, { "x_col", "x" }, { "y_col", "b" } });
        CHECK_COND(!isErr(r) && lml->layersList().at(0)->curves() == 2, "111j: add curve b: " << errText(r));
        r = rpc(QString::fromLatin1("graph.set_legend"),
                QJsonObject{ { "graph", "LG111" }, { "text", "\\c{1}d\n\n\\c{2}b\n" } });
        CHECK_COND(!isErr(r), "111j: legend text: " << errText(r));
        const std::string l2 = legendText();
        std::cout << "[INFO] 111j: legend typed: " << shown(l2) << "\n";
        CHECK_COND(l2.find("\n\n") != std::string::npos, "111j: typed legend: " << shown(l2));
        r = rpc(QString::fromLatin1("table.remove_column"), QJsonObject{ { "table", "LT111" }, { "col", "b" } });
        CHECK_COND(!isErr(r) && lml->layersList().at(0)->curves() == 1, "111j: remove b: " << errText(r));
        CHECK_COND(!isErr(rpc(QString::fromLatin1("app.undo"), QJsonObject{ { "table", "LT111" } })),
                   "111j: undo remove b");
        if (legendText() != l2) {
            std::cerr << "[FAIL] 111j (2): legend after the column's Undo " << shown(legendText())
                      << " expected " << shown(l2) << std::endl;
            ++fails111j;
        }
        r = rpc(QString::fromLatin1("graph.set_legend"),
                QJsonObject{ { "graph", "LG111" }, { "text", "\\c{1}d\n\n\\c{2}b\n" } });
        CHECK_COND(!isErr(r) && legendText() == l2, "111j: legend text again: " << errText(r));
        w111->closeWindow(t);
        QCoreApplication::processEvents();
        CHECK_COND(w111->table(QString::fromLatin1("LT111")) == nullptr
                           && lml->layersList().at(0)->curves() == 0,
                   "111j: deleting LT111");
        ps->undo();
        QCoreApplication::processEvents();
        std::cout << "[INFO] 111j: LT111 back: " << shown(legendText()) << "\n";
        if (w111->table(QString::fromLatin1("LT111")) == nullptr || legendText() != l2) {
            std::cerr << "[FAIL] 111j (3): legend after the table deletion's Undo " << shown(legendText())
                      << " expected " << shown(l2) << std::endl;
            ++fails111j;
        }
        CHECK_COND(fails111j == 0, "111j: " << fails111j << " case(s) failed");
    }
    std::cout << "[PASS] 111j: the legend text comes back as it was\n";

    auto shownText = [](const QString &s) {
        std::string o;
        for (QChar ch : s)
            o += (ch == QLatin1Char('\n')) ? std::string("\\n") : QString(ch).toStdString();
        return o;
    };
    auto legendOf = [&](const char *graph) {
        MultiLayer *ml = findGraph(w111, graph);
        Graph *g = (ml && !ml->layersList().isEmpty()) ? ml->layersList().at(0) : nullptr;
        return (g && g->legend()) ? g->legend()->text() : QStringLiteral("(none)");
    };
    auto curvesOf = [&](const char *graph) {
        MultiLayer *ml = findGraph(w111, graph);
        Graph *g = (ml && !ml->layersList().isEmpty()) ? ml->layersList().at(0) : nullptr;
        return g ? g->curves() : -1;
    };
    auto call = [&](const char *method, const QJsonObject &params) {
        const QJsonObject r = rpc(QString::fromLatin1(method), params);
        if (isErr(r))
            std::cerr << "[INFO] " << method << ": " << errText(r) << "\n";
        return !isErr(r);
    };

    // ---- 111k: a graph Undo from before a column removal does not double its legend line ----
    // y, z, w and error bars of y; the title is set, then a column removed: the graph's Undo
    // (of the title) rebuilds the layer from a snapshot that still has the curve of the removed
    // column. The curve could not be rebuilt but its legend line stayed (and the lines after it
    // were numbered for curves that had moved up), so the column's Undo put the line in a second
    // time: \c{1}y\n\c{2}z\n\c{3}z\n\c{4}w.
    {
        int fails111k = 0;
        for (const char *col : { "z", "y", "w" }) {
            const QByteArray tn = QByteArray("RT111") + col;
            const QByteArray gn = QByteArray("RG111") + col;
            const char *t = tn.constData();
            const char *gname = gn.constData();
            Table *rt = w111->newTable(QString::fromLatin1(t), 6, 5);
            CHECK_COND(rt, "111k: table " << t);
            const char *rc[] = { "x", "y", "z", "e", "w" };
            for (int c = 0; c < 5; ++c)
                rt->setColName(c, QString::fromLatin1(rc[c]));
            CHECK_COND(setCol(t, "x", { 1, 2, 3, 4, 5, 6 }) && setCol(t, "y", { 2, 4, 6, 8, 10, 12 })
                               && setCol(t, "z", { 1, 4, 9, 16, 25, 36 })
                               && setCol(t, "e", { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 })
                               && setCol(t, "w", { 2, 3, 4, 5, 6, 7 }),
                       "111k: fill " << t);
            CHECK_COND(call("plot.xy", QJsonObject{ { "table", t }, { "x_col", "x" },
                                                    { "y_cols", QJsonArray{ "y", "z", "w" } },
                                                    { "graph_name", gname } })
                               && call("graph.set_legend", QJsonObject{ { "graph", gname }, { "visible", true } })
                               && call("plot.add_error_bars",
                                       QJsonObject{ { "graph", gname }, { "err_col", "e" },
                                                    { "curve", QString::fromLatin1(t) + "_y" } }),
                       "111k: graph " << gname);
            const QString init = legendOf(gname);
            CHECK_COND(init == QStringLiteral("\\c{1}y\n\\c{2}z\n\\c{3}w\n") && curvesOf(gname) == 4,
                       "111k: legend of " << gname << ": " << shownText(init));
            CHECK_COND(call("graph.set_title", QJsonObject{ { "graph", gname }, { "title", "T" } })
                               && call("table.remove_column", QJsonObject{ { "table", t }, { "col", col } }),
                       "111k: title / remove column " << col);
            const QString removed = legendOf(gname);
            const int curvesRemoved = curvesOf(gname);
            std::cout << "[INFO] 111k (" << col << "): removed " << shownText(removed) << "\n";
            CHECK_COND(call("app.undo", QJsonObject{ { "graph", gname } }), "111k: graph undo");
            std::cout << "[INFO] 111k (" << col << "): graph Undo " << shownText(legendOf(gname)) << "\n";
            if (legendOf(gname) != removed || curvesOf(gname) != curvesRemoved) {
                std::cerr << "[FAIL] 111k (" << col << "): legend after the graph's Undo "
                          << shownText(legendOf(gname)) << " expected " << shownText(removed)
                          << std::endl;
                ++fails111k;
            }
            CHECK_COND(call("app.undo", QJsonObject{ { "table", t } }), "111k: table undo");
            std::cout << "[INFO] 111k (" << col << "): column back " << shownText(legendOf(gname)) << "\n";
            if (legendOf(gname) != init || curvesOf(gname) != 4) {
                std::cerr << "[FAIL] 111k (" << col << "): legend after the column's Undo "
                          << shownText(legendOf(gname)) << " expected " << shownText(init) << std::endl;
                ++fails111k;
            }
            CHECK_COND(call("app.redo", QJsonObject{ { "table", t } }), "111k: table redo");
            if (legendOf(gname) != removed || curvesOf(gname) != curvesRemoved) {
                std::cerr << "[FAIL] 111k (" << col << "): legend after the column's Redo "
                          << shownText(legendOf(gname)) << " expected " << shownText(removed)
                          << std::endl;
                ++fails111k;
            }
            CHECK_COND(call("app.undo", QJsonObject{ { "table", t } }), "111k: table undo 2");
            if (legendOf(gname) != init || curvesOf(gname) != 4) {
                std::cerr << "[FAIL] 111k (" << col << "): legend after the second Undo "
                          << shownText(legendOf(gname)) << " expected " << shownText(init) << std::endl;
                ++fails111k;
            }
        }
        CHECK_COND(fails111k == 0, "111k: " << fails111k << " case(s) failed");
    }
    std::cout << "[PASS] 111k: a graph Undo from before a column removal keeps the legend whole\n";

    // ---- 111l: the legend numbers after removed error bars move up ----
    // y, error bars e of y, w: the legend is \c{1}y\n\c{3}w (the error bars take number 2).
    // Removing the error bars (curve or column) left \c{3}w with two curves, and removing w left
    // its line, since a curve number at or past the number of legend lines was not looked for.
    {
        int fails111l = 0;
        const char *t = "NT111";
        const char *gname = "NG111";
        Table *nt = w111->newTable(QString::fromLatin1(t), 6, 4);
        CHECK_COND(nt, "111l: table NT111");
        const char *nc[] = { "x", "y", "e", "w" };
        for (int c = 0; c < 4; ++c)
            nt->setColName(c, QString::fromLatin1(nc[c]));
        CHECK_COND(setCol(t, "x", { 1, 2, 3, 4, 5, 6 }) && setCol(t, "y", { 2, 4.2, 6, 8.1, 10, 12.3 })
                           && setCol(t, "e", { 0.5, 0.6, 0.4, 0.5, 0.7, 0.5 })
                           && setCol(t, "w", { 2, 3, 4, 5, 6, 7 }),
                   "111l: fill NT111");
        CHECK_COND(call("plot.xy", QJsonObject{ { "table", t }, { "x_col", "x" },
                                                { "y_cols", QJsonArray{ "y" } }, { "graph_name", gname } })
                           && call("graph.set_legend", QJsonObject{ { "graph", gname }, { "visible", true } })
                           && call("plot.add_error_bars", QJsonObject{ { "graph", gname }, { "err_col", "e" },
                                                                       { "curve", "NT111_y" } })
                           && call("plot.add_curve", QJsonObject{ { "graph", gname }, { "table", t },
                                                                  { "x_col", "x" }, { "y_col", "w" } }),
                   "111l: graph NG111");
        const QString init = legendOf(gname);
        CHECK_COND(init == QStringLiteral("\\c{1}y\n\\c{3}w\n") && curvesOf(gname) == 3,
                   "111l: legend of NG111: " << shownText(init));
        auto expect = [&](const char *what, const QString &want) {
            std::cout << "[INFO] 111l: " << what << ": " << shownText(legendOf(gname)) << "\n";
            if (legendOf(gname) != want) {
                std::cerr << "[FAIL] 111l: " << what << ": legend " << shownText(legendOf(gname))
                          << " expected " << shownText(want) << std::endl;
                ++fails111l;
            }
        };
        // (1) the error bars removed as a curve, (2) through their column, (3) w through its column
        CHECK_COND(call("plot.remove_curve", QJsonObject{ { "graph", gname }, { "curve", 1 } }),
                   "111l: remove the error bars");
        expect("error bars removed", QStringLiteral("\\c{1}y\n\\c{2}w\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "graph", gname } }), "111l: graph undo");
        expect("error bars back", init);
        CHECK_COND(call("table.remove_column", QJsonObject{ { "table", t }, { "col", "e" } }),
                   "111l: remove column e");
        expect("column e removed", QStringLiteral("\\c{1}y\n\\c{2}w\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "table", t } }), "111l: undo e");
        expect("column e back", init);
        CHECK_COND(call("table.remove_column", QJsonObject{ { "table", t }, { "col", "w" } }),
                   "111l: remove column w");
        expect("column w removed", QStringLiteral("\\c{1}y\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "table", t } }), "111l: undo w");
        expect("column w back", init);
        // (4) a legend typed by the user with a line that is not a curve's, (5) without a line
        //     for y: only the numbers move
        const QString typed = QStringLiteral("Data\n\\c{1}y\n\\c{3}w\n");
        CHECK_COND(call("graph.set_legend", QJsonObject{ { "graph", gname }, { "text", typed } }),
                   "111l: typed legend");
        CHECK_COND(call("plot.remove_curve", QJsonObject{ { "graph", gname }, { "curve", 1 } }),
                   "111l: remove the error bars (typed legend)");
        expect("typed legend, error bars removed", QStringLiteral("Data\n\\c{1}y\n\\c{2}w\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "graph", gname } }), "111l: graph undo (typed)");
        expect("typed legend, error bars back", typed);
        CHECK_COND(call("graph.set_legend", QJsonObject{ { "graph", gname }, { "text", "\\c{3}w\n" } }),
                   "111l: legend without y");
        CHECK_COND(call("plot.remove_curve", QJsonObject{ { "graph", gname }, { "curve", 1 } }),
                   "111l: remove the error bars (no line for y)");
        expect("no line for y, error bars removed", QStringLiteral("\\c{2}w\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "graph", gname } }), "111l: graph undo (no y)");
        // (5b) empty lines the user put into the legend stay when the error bars go
        const QString blanks = QStringLiteral("My samples\n\n\\c{1}y\n\n\\c{3}w\n");
        CHECK_COND(call("graph.set_legend", QJsonObject{ { "graph", gname }, { "text", blanks } }),
                   "111l: legend with empty lines");
        CHECK_COND(call("plot.remove_curve", QJsonObject{ { "graph", gname }, { "curve", 1 } }),
                   "111l: remove the error bars (empty lines)");
        expect("empty lines, error bars removed", QStringLiteral("My samples\n\n\\c{1}y\n\n\\c{2}w\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "graph", gname } }), "111l: graph undo (empty lines)");
        // (5c) an Origin-style legend (\l(n) %(n)): the line of the removed curve goes, the
        //      others move up, no line is given twice
        const QString originStyle = QStringLiteral("\\l(1) %(1)\n\\l(3) %(3)\n");
        CHECK_COND(call("graph.set_legend", QJsonObject{ { "graph", gname }, { "text", originStyle } }),
                   "111l: Origin-style legend");
        CHECK_COND(call("table.remove_column", QJsonObject{ { "table", t }, { "col", "w" } }),
                   "111l: remove column w (Origin-style legend)");
        expect("Origin-style legend, column w removed", QStringLiteral("\\l(1) %(1)\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "table", t } }), "111l: undo w (Origin-style)");
        CHECK_COND(call("plot.remove_curve", QJsonObject{ { "graph", gname }, { "curve", 0 } }),
                   "111l: remove y (Origin-style legend)");
        // y goes with its error bars: w is the only item left and takes number 1
        expect("Origin-style legend, y removed", QStringLiteral("\\l(1) %(1)\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "graph", gname } }), "111l: graph undo (Origin-style)");
        // the same with \c{n}: w keeps its line (it lost it to the error bars' old number)
        CHECK_COND(call("graph.set_legend", QJsonObject{ { "graph", gname }, { "text", init } }),
                   "111l: legend back before removing y");
        CHECK_COND(call("plot.remove_curve", QJsonObject{ { "graph", gname }, { "curve", 0 } }),
                   "111l: remove y");
        expect("y with its error bars removed", QStringLiteral("\\c{1}w\n"));
        CHECK_COND(call("app.undo", QJsonObject{ { "graph", gname } }), "111l: graph undo (Origin-style)");
        CHECK_COND(call("graph.set_legend", QJsonObject{ { "graph", gname }, { "text", init } }),
                   "111l: legend back");
        // (6) a column used by error bars and by a curve, with a fit curve after them: every
        //     number stays within the curves and none is given twice, and Undo puts it back
        CHECK_COND(call("plot.add_curve", QJsonObject{ { "graph", gname }, { "table", t },
                                                       { "x_col", "x" }, { "y_col", "e" } })
                           && call("analysis.fit_linear", QJsonObject{ { "graph", gname }, { "curve", "NT111_y" } }),
                   "111l: curve e and a fit");
        const QString withFit = legendOf(gname);
        const int curvesWithFit = curvesOf(gname);
        CHECK_COND(curvesWithFit == 5, "111l: curves with the fit: " << curvesWithFit);
        auto numbersValid = [&](const QString &text, int nCurves) {
            QSet<int> seen;
            static const QRegularExpression rx(QStringLiteral(R"(\\c\{(\d+)\})"));
            QRegularExpressionMatchIterator it = rx.globalMatch(text);
            while (it.hasNext()) {
                const int n = it.next().captured(1).toInt();
                if (n < 1 || n > nCurves || seen.contains(n))
                    return false;
                seen.insert(n);
            }
            return true;
        };
        CHECK_COND(numbersValid(withFit, curvesWithFit), "111l: legend with the fit: " << shownText(withFit));
        CHECK_COND(call("table.remove_column", QJsonObject{ { "table", t }, { "col", "e" } }),
                   "111l: remove column e (fit)");
        std::cout << "[INFO] 111l: e removed with the fit: " << shownText(legendOf(gname)) << "\n";
        if (curvesOf(gname) != 3 || !numbersValid(legendOf(gname), 3)
            || legendOf(gname).count(QStringLiteral("\\c{")) != 3) {
            std::cerr << "[FAIL] 111l: e removed with the fit: legend " << shownText(legendOf(gname))
                      << " for " << curvesOf(gname) << " curves" << std::endl;
            ++fails111l;
        }
        CHECK_COND(call("app.undo", QJsonObject{ { "table", t } }), "111l: undo e (fit)");
        expect("e back with the fit", withFit);
        CHECK_COND(fails111l == 0, "111l: " << fails111l << " case(s) failed");
    }
    std::cout << "[PASS] 111l: the legend numbers after removed error bars move up\n";

    // ---- 111m: a table closed through app.close_window comes back with app.undo ----
    // The RPC deleted the window that the table's removal keeps for its Undo, so app.undo
    // without a window brought back a table without a window (and the graph's curve not at all).
    {
        Table *ct = w111->newTable(QStringLiteral("CT111"), 4, 2);
        Table *cu = w111->newTable(QStringLiteral("CU111"), 4, 2);
        CHECK_COND(ct && cu, "111m: tables");
        ct->setColName(0, QStringLiteral("x"));
        ct->setColName(1, QStringLiteral("y"));
        CHECK_COND(setCol("CT111", "x", { 1, 2, 3, 4 }) && setCol("CT111", "y", { 1, 4, 9, 16 }),
                   "111m: fill CT111");
        CHECK_COND(call("plot.xy", QJsonObject{ { "table", "CT111" }, { "x_col", "x" },
                                                { "y_cols", QJsonArray{ "y" } }, { "graph_name", "CG111" } }),
                   "111m: graph CG111");
        const QString legendBefore = legendOf("CG111");
        CHECK_COND(call("app.close_window", QJsonObject{ { "name", "CT111" } })
                           && w111->table(QStringLiteral("CT111")) == nullptr && curvesOf("CG111") == 0,
                   "111m: app.close_window CT111");
        CHECK_COND(call("app.undo", QJsonObject{}), "111m: app.undo");
        const bool back = w111->table(QStringLiteral("CT111")) != nullptr;
        std::cout << "[INFO] 111m: after app.undo: table " << (back ? "back" : "missing") << ", "
                  << curvesOf("CG111") << " curve(s), legend " << shownText(legendOf("CG111")) << "\n";
        CHECK_COND(back && curvesOf("CG111") == 1 && legendOf("CG111") == legendBefore,
                   "111m: app.undo did not bring back CT111 and its curve");
        CHECK_COND(call("app.redo", QJsonObject{}) && w111->table(QStringLiteral("CT111")) == nullptr
                           && curvesOf("CG111") == 0,
                   "111m: app.redo did not close CT111 again");
        CHECK_COND(call("app.undo", QJsonObject{}) && w111->table(QStringLiteral("CT111")) != nullptr
                           && curvesOf("CG111") == 1,
                   "111m: the second app.undo did not bring back CT111");
        // the menu's Undo (Ctrl+Z) with another worksheet active
        Table *ct2 = w111->table(QStringLiteral("CT111"));
        w111->closeWindow(ct2);
        QCoreApplication::processEvents();
        CHECK_COND(w111->table(QStringLiteral("CT111")) == nullptr, "111m: closing CT111 (GUI)");
        w111->activateSubWindow(cu);
        QCoreApplication::processEvents();
        w111->undo();
        QCoreApplication::processEvents();
        CHECK_COND(w111->table(QStringLiteral("CT111")) != nullptr && curvesOf("CG111") == 1,
                   "111m: Ctrl+Z did not bring back CT111");
    }
    std::cout << "[PASS] 111m: a table closed through app.close_window comes back with app.undo\n";

    std::cout << "[PASS] Suite 111: dual-Y legend headroom and removed-column undo passed.\n";
    return 0;
}
