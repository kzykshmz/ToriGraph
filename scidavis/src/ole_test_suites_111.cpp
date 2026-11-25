#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/future_Table.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
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
//        column without data is still refused
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
        CHECK_COND(undoE(true) && sigE() == e0, "111e: undo clear z: " << sigE());
        r = rpc(QString::fromLatin1("plot.xy"),
                QJsonObject{ { "table", "E111" }, { "x_col", "x" }, { "y_cols", QJsonArray{ "w" } },
                             { "graph_name", "EW111" } });
        std::cout << "[INFO] 111e: plotting the empty column w: "
                  << (isErr(r) ? errText(r) : std::string("no error")) << "\n";
        CHECK_COND(isErr(r), "111e: plotting a column without data is no longer refused");
        std::cout << "[PASS] 111e: an emptied column keeps its curve and Undo / Redo bring the data back\n";
    }

    std::cout << "[PASS] Suite 111: dual-Y legend headroom and removed-column undo passed.\n";
    return 0;
}
