#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QJsonArray>
#include <QJsonObject>

// Suite 82: JSON-RPC parameter validation and atomic-batch rollback regressions.
//   82a graph.mask_region with an unresolvable explicit 'curve' must fail and mask nothing
//   82b analysis.analyze_peaks rejects out-of-range doublet indices and oversized 'centers'
//   82c app.batch atomic rollback restores table-column masks written by graph.mask_region
//   82d table.set_data_dict rejects a final footprint above 25,000,000 cells, table unchanged

int runSuite82JsonRpcParamValidation(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 82: JSON-RPC parameter validation & batch rollback...\n";

    struct NonInteractiveQuitGuard82
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard82()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard82()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard82;

    struct AppWinGuard82
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard82(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard82() { reset(); }
        AppWinGuard82(const AppWinGuard82 &) = delete;
        AppWinGuard82 &operator=(const AppWinGuard82 &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        ApplicationWindow *get() const { return win; }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard82 w82(new ApplicationWindow());
    w82->applyUserSettings();
    w82->confirmCloseTable = false;
    w82->confirmClosePlot2D = false;
    w82->confirmClosePlot3D = false;
    w82->confirmCloseFolder = false;
    w82->resize(1280, 800);
    w82->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w82->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "82: pythonAutomationServer null");

    int rpcId = 8200;
    auto rpc = [&](const QString &method, const QJsonObject &params) -> QJsonObject {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = ++rpcId;
        req[QStringLiteral("method")] = method;
        req[QStringLiteral("params")] = params;
        const QJsonObject resp = srv->handleRpcRequest(req);
        QCoreApplication::processEvents();
        return resp;
    };
    auto errCode = [](const QJsonObject &resp) -> int {
        return resp.contains(QStringLiteral("error"))
                ? resp.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt()
                : 0;
    };
    auto columnHasMask = [](Table *t, int c) -> bool {
        Column *col = t ? t->column(c) : nullptr;
        return col && !col->maskedIntervals().isEmpty();
    };

    // Shared fixture: table with X + two Y columns, plotted as two DataCurves on one layer.
    Table *tM = w82->newTable(8, 3, QStringLiteral("T82M"), QStringLiteral("Mask82"));
    CHECK_COND(tM != nullptr, "82: newTable T82M failed");
    for (int r = 0; r < 8; ++r) {
        tM->setCell(r, 0, double(r));
        tM->setCell(r, 1, double(r * r));
        tM->setCell(r, 2, double(10 - r));
    }
    MultiLayer *mlM = w82->multilayerPlot(
            tM, QStringList() << tM->colName(1) << tM->colName(2), Graph::Scatter);
    Graph *gM = mlM ? mlM->activeGraph() : nullptr;
    CHECK_COND(gM != nullptr && gM->curves() == 2, "82: two-curve mask fixture setup failed");
    const QString mlMName = mlM->name();
    const QString curve0Title = gM->curve(0)->title().text();
    QCoreApplication::processEvents();

    // -------------------------------------------------------------------------
    // 82a: graph.mask_region with an explicit but unknown curve
    // -------------------------------------------------------------------------
    {
        const QJsonObject resp = rpc(QStringLiteral("graph.mask_region"),
                                     QJsonObject{ { "graph", mlMName },
                                                  { "curve", "NoSuchCurve82" },
                                                  { "x_min", -1000.0 },
                                                  { "x_max", 1000.0 } });
        CHECK_COND(errCode(resp) == -32602,
                   "82a: mask_region with unknown curve did not return invalid params (code="
                           << errCode(resp) << ")");
        CHECK_COND(!columnHasMask(tM, 0) && !columnHasMask(tM, 1) && !columnHasMask(tM, 2),
                   "82a: mask_region with unknown curve masked table columns (data damage)!");

        // Positive control: a resolvable curve masks only its own Y column.
        const QJsonObject okResp = rpc(QStringLiteral("graph.mask_region"),
                                       QJsonObject{ { "graph", mlMName },
                                                    { "curve", curve0Title },
                                                    { "x_min", 2.0 },
                                                    { "x_max", 4.0 } });
        CHECK_COND(!okResp.contains(QStringLiteral("error"))
                           && okResp.value("result").toObject().value("masked_points").toInt() == 3,
                   "82a: mask_region on a valid curve did not mask exactly 3 points");
        CHECK_COND(columnHasMask(tM, 1) && !columnHasMask(tM, 2),
                   "82a: mask_region on curve 0 leaked masks into the other curve's column");

        const QJsonObject clrResp = rpc(QStringLiteral("graph.clear_masks"),
                                        QJsonObject{ { "graph", mlMName } });
        CHECK_COND(!clrResp.contains(QStringLiteral("error")) && !columnHasMask(tM, 1),
                   "82a: graph.clear_masks did not clear the fixture masks");
    }

    // -------------------------------------------------------------------------
    // 82b: analysis.analyze_peaks doublet / centers bounds
    // -------------------------------------------------------------------------
    {
        const int winsBefore = w82->windowsList().size();
        const int curvesBefore = gM->curves();
        const QJsonObject hugeSec = rpc(
                QStringLiteral("analysis.analyze_peaks"),
                QJsonObject{ { "graph", mlMName },
                             { "curve", curve0Title },
                             { "doublets",
                               QJsonArray{ QJsonObject{ { "secondary", 2000000000 },
                                                        { "primary", 0 } } } } });
        CHECK_COND(errCode(hugeSec) == -32602,
                   "82b: huge doublet 'secondary' was not rejected as invalid params (code="
                           << errCode(hugeSec) << ")");

        const QJsonObject hugePri = rpc(
                QStringLiteral("analysis.analyze_peaks"),
                QJsonObject{ { "graph", mlMName },
                             { "curve", curve0Title },
                             { "doublets",
                               QJsonArray{ QJsonObject{ { "secondary", 1 },
                                                        { "primary", 500 } } } } });
        CHECK_COND(errCode(hugePri) == -32602,
                   "82b: doublet 'primary'=500 (>= 500-peak cap) was not rejected");

        const QJsonObject fracSec = rpc(
                QStringLiteral("analysis.analyze_peaks"),
                QJsonObject{ { "graph", mlMName },
                             { "curve", curve0Title },
                             { "doublets",
                               QJsonArray{ QJsonObject{ { "secondary", 1.5 },
                                                        { "primary", 0 } } } } });
        CHECK_COND(errCode(fracSec) == -32602,
                   "82b: non-integer doublet 'secondary' was not rejected");

        QJsonArray manyCenters;
        for (int i = 0; i < 501; ++i)
            manyCenters.append(double(i) * 0.01);
        const QJsonObject tooMany = rpc(QStringLiteral("analysis.analyze_peaks"),
                                        QJsonObject{ { "graph", mlMName },
                                                     { "curve", curve0Title },
                                                     { "centers", manyCenters } });
        CHECK_COND(errCode(tooMany) == -32602,
                   "82b: 501 'centers' were not rejected as invalid params");

        CHECK_COND(w82->windowsList().size() == winsBefore && gM->curves() == curvesBefore,
                   "82b: rejected analyze_peaks calls still created windows or curves");
    }

    // -------------------------------------------------------------------------
    // 82c: atomic app.batch with mask_region then a failing step rolls the mask back
    // -------------------------------------------------------------------------
    {
        CHECK_COND(!columnHasMask(tM, 1) && !columnHasMask(tM, 2),
                   "82c: precondition - fixture columns already masked");
        const QJsonArray cmds{
            QJsonObject{ { "method", "graph.mask_region" },
                         { "params",
                           QJsonObject{ { "graph", mlMName },
                                        { "x_min", -1000.0 },
                                        { "x_max", 1000.0 } } } },
            QJsonObject{ { "method", "graph.mask_region" },
                         { "params",
                           QJsonObject{ { "graph", "NoSuchGraph82" },
                                        { "x_min", 0.0 },
                                        { "x_max", 1.0 } } } },
        };
        const QJsonObject resp = rpc(QStringLiteral("app.batch"),
                                     QJsonObject{ { "atomic", true }, { "commands", cmds } });
        CHECK_COND(!resp.contains(QStringLiteral("error")), "82c: app.batch returned an RPC error");
        const QJsonObject res = resp.value(QStringLiteral("result")).toObject();
        CHECK_COND(!res.value("ok").toBool(true) && res.value("failed_index").toInt(-1) == 1,
                   "82c: batch did not fail at step 1 as arranged");
        CHECK_COND(res.value("results").toArray().at(0).toObject().value("masked_points").toInt()
                           > 0,
                   "82c: step 0 mask_region masked nothing, test would be vacuous");
        CHECK_COND(res.value("rolled_back").toBool(false) && !res.contains("rollback_error"),
                   "82c: atomic batch did not report a clean rollback");

        // The full-snapshot rollback reloads the project, so look the table up again by name.
        Table *tAfter = w82->table(QStringLiteral("T82M"));
        CHECK_COND(tAfter != nullptr && tAfter->numRows() == 8 && tAfter->numCols() == 3,
                   "82c: table T82M missing or reshaped after rollback");
        CHECK_COND(!columnHasMask(tAfter, 0) && !columnHasMask(tAfter, 1)
                           && !columnHasMask(tAfter, 2),
                   "82c: rolled_back:true but graph.mask_region masks remained in the table!");
        CHECK_COND(std::fabs(tAfter->cell(3, 1) - 9.0) < 1e-12
                           && std::fabs(tAfter->cell(7, 2) - 3.0) < 1e-12,
                   "82c: table values changed across the rollback");
        MyWidget *mlAfter = w82->window(mlMName);
        CHECK_COND(mlAfter != nullptr && mlAfter->inherits("MultiLayer"),
                   "82c: graph window missing after rollback");

        // A successful atomic batch with mask_region must still commit its masks.
        const QJsonObject okResp = rpc(
                QStringLiteral("app.batch"),
                QJsonObject{ { "atomic", true },
                             { "commands",
                               QJsonArray{ QJsonObject{
                                       { "method", "graph.mask_region" },
                                       { "params",
                                         QJsonObject{ { "graph", mlMName },
                                                      { "x_min", 0.0 },
                                                      { "x_max", 1.0 } } } } } } });
        const QJsonObject okRes = okResp.value(QStringLiteral("result")).toObject();
        Table *tCommit = w82->table(QStringLiteral("T82M"));
        CHECK_COND(okRes.value("ok").toBool() && !okRes.value("rolled_back").toBool(true)
                           && tCommit && columnHasMask(tCommit, 1),
                   "82c: successful atomic batch did not keep its mask_region result");
    }

    // -------------------------------------------------------------------------
    // 82d: table.set_data_dict final footprint cap on an existing table
    // -------------------------------------------------------------------------
    {
        Table *tD = w82->newTable(7000, 2, QStringLiteral("T82D"), QStringLiteral("Dict82"));
        CHECK_COND(tD != nullptr, "82d: newTable T82D failed");
        tD->setCell(0, 0, 1.25);
        const QString col0Name = tD->colName(0);
        QJsonObject dataDict;
        for (int k = 0; k < 4096; ++k)
            dataDict.insert(QStringLiteral("k%1").arg(k), QJsonArray{ double(k) });
        // keys x rows (4096 x 1) passes the per-call check, but the table would become
        // 7000 x 4096 = 28,672,000 cells.
        const QJsonObject resp = rpc(QStringLiteral("table.set_data_dict"),
                                     QJsonObject{ { "table", QStringLiteral("T82D") },
                                                  { "data", dataDict } });
        CHECK_COND(errCode(resp) == -32602,
                   "82d: oversize set_data_dict on existing table was not rejected (code="
                           << errCode(resp) << ")");
        Table *tDAfter = w82->table(QStringLiteral("T82D"));
        CHECK_COND(tDAfter == tD && tD->numRows() == 7000 && tD->numCols() == 2
                           && tD->colName(0) == col0Name
                           && std::fabs(tD->cell(0, 0) - 1.25) < 1e-12,
                   "82d: rejected set_data_dict still modified the table ("
                           << tD->numRows() << "x" << tD->numCols() << ")");

        // Positive control: a small dict on the same table still works.
        const QJsonObject okResp = rpc(
                QStringLiteral("table.set_data_dict"),
                QJsonObject{ { "table", QStringLiteral("T82D") },
                             { "data",
                               QJsonObject{ { "p", QJsonArray{ 1.0, 2.0 } },
                                            { "q", QJsonArray{ 3.0, 4.0 } },
                                            { "r", QJsonArray{ 5.0, 6.0 } } } } });
        CHECK_COND(!okResp.contains(QStringLiteral("error")) && tD->numCols() == 3
                           && tD->numRows() == 7000,
                   "82d: small set_data_dict on an existing table failed");
    }

    w82.reset();
    std::cout << "[PASS] Suite 82 passed.\n";
    return 0;
}
