#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/future_Table.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUndoStack>

#include <cmath>

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

    std::cout << "[PASS] Suite 111: dual-Y legend headroom and removed-column undo passed.\n";
    return 0;
}
