#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/future_Table.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>

#include <cmath>

// =============================================================================
// Suite 109: worksheet formulas and masks, data-driven review findings (JSON-RPC path).
//   109a the column a formula reads by name is removed: col("1") (default numeric names) and
//        col("A") (letter names) become a formula error instead of silently reading the column
//        that now sits at that position, also when an edit elsewhere triggers the automatic
//        recalculation; the values are not overwritten
//   109b the Origin-style positional reading still works where it does not clash with the
//        table's own names: col("A") in a table named "1", "2", ...; col("2") in a table named
//        "A", "B", ...; a Long Name
//   109c masking rows of a column that a formula column reads: 8000 of 20000 rows and a list
//        of 2000 scattered rows take seconds at most (each masked row used to recalculate the
//        whole formula column), the formula values stay, Undo / Redo restore the masks, and a
//        data change still recalculates the formula column; masking a GUI selection (runs of
//        rows) is one Undo step
// =============================================================================

int runSuite109WorksheetFormulaRefsAndMasks(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 109: worksheet formula references and masks...\n";

    struct NonInteractiveQuitGuard109
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard109()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard109()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard109;

    struct AppWinGuard109
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard109(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard109()
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            QApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        AppWinGuard109(const AppWinGuard109 &) = delete;
        AppWinGuard109 &operator=(const AppWinGuard109 &) = delete;
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard109 w109(new ApplicationWindow());
    w109->applyUserSettings();
    w109->confirmCloseTable = false;
    w109->confirmCloseMatrix = false;
    w109->confirmClosePlot2D = false;
    w109->confirmClosePlot3D = false;
    w109->confirmCloseFolder = false;
    w109->resize(1280, 800);
    w109->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w109->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "109: pythonAutomationServer null");

    int rpcId = 10900;
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
    auto isErr = [](const QJsonObject &resp) { return resp.contains(QStringLiteral("error")); };
    auto res = [](const QJsonObject &resp) { return resp.value(QStringLiteral("result")).toObject(); };
    auto errText = [](const QJsonObject &resp) {
        return resp.value(QStringLiteral("error"))
                .toObject()
                .value(QStringLiteral("message"))
                .toString()
                .toStdString();
    };
    auto numbers = [](std::initializer_list<double> v) {
        QJsonArray a;
        for (double d : v)
            a.append(d);
        return a;
    };
    auto columnValues = [&](const QString &table, const QString &col) {
        QVector<double> out;
        const QJsonObject r = rpc(QString::fromLatin1("table.get_column_data"),
                                  QJsonObject{ { "table", table }, { "col", col } });
        for (const QJsonValue &v : res(r).value(QString::fromLatin1("data")).toArray())
            out.append(v.isDouble() ? v.toDouble() : std::nan(""));
        return out;
    };
    auto sameValues = [](const QVector<double> &got, std::initializer_list<double> want) {
        if (got.size() != int(want.size()))
            return false;
        int k = 0;
        for (double w : want)
            if (std::fabs(got.at(k++) - w) > 1e-9)
                return false;
        return true;
    };
    auto formulaError = [&](const QString &table, const QString &col) {
        return res(rpc(QString::fromLatin1("table.get_column_meta"),
                       QJsonObject{ { "table", table }, { "col", col } }))
                .value(QString::fromLatin1("formula_error"))
                .toString();
    };
    auto q = [](const QVector<double> &v) {
        std::string s;
        for (double d : v)
            s += std::to_string(d) + " ";
        return s;
    };

    // ---- 109a: the referenced column is removed ----
    {
        // default numeric names "1", "2", "3"; column "3" = col("1")*10
        Table *t = w109->newTable(QStringLiteral("R109"), 3, 3);
        CHECK_COND(t != nullptr && t->column(0)->name() == QLatin1String("1"), "109a: table R109");
        QJsonObject r = rpc(QStringLiteral("table.set_column_data"),
                            QJsonObject{ { "table", "R109" }, { "col", "1" }, { "data", numbers({ 1, 2, 3 }) } });
        CHECK_COND(!isErr(r), "109a: set column 1: " << errText(r));
        r = rpc(QStringLiteral("table.set_column_data"),
                QJsonObject{ { "table", "R109" }, { "col", "2" }, { "data", numbers({ 100, 200, 300 }) } });
        CHECK_COND(!isErr(r), "109a: set column 2: " << errText(r));
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "R109" }, { "col", "3" }, { "formula", "col(\"1\")*10" } });
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("ok")).toBool(),
                   "109a: set_formula col(\"1\")*10: " << errText(r));
        CHECK_COND(sameValues(columnValues("R109", "3"), { 10, 20, 30 }),
                   "109a: col(\"1\")*10 before removal: " << q(columnValues("R109", "3")));

        r = rpc(QStringLiteral("table.remove_column"), QJsonObject{ { "table", "R109" }, { "col", "1" } });
        CHECK_COND(!isErr(r), "109a: remove column 1: " << errText(r));
        r = rpc(QStringLiteral("table.recalculate"), QJsonObject{ { "table", "R109" } });
        CHECK_COND(!isErr(r), "109a: recalculate: " << errText(r));
        const QVector<double> afterRecalc = columnValues("R109", "3");
        std::cout << "[INFO] 109a: column 3 after removing column 1 and recalculating: "
                  << q(afterRecalc) << " error: " << formulaError("R109", "3").toStdString() << "\n";
        CHECK_COND(!res(r).value(QStringLiteral("ok")).toBool()
                           && !res(r).value(QStringLiteral("formula_errors")).toArray().isEmpty(),
                   "109a: recalculate after removing column 1 reports no formula error");
        CHECK_COND(!formulaError("R109", "3").isEmpty(),
                   "109a: column 3 has no formula error after its column 1 was removed");
        CHECK_COND(sameValues(afterRecalc, { 10, 20, 30 }),
                   "109a: column 3 was overwritten (read another column): " << q(afterRecalc));

        // an edit elsewhere triggers the automatic recalculation: values stay, error stays
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "R109" }, { "row", 0 }, { "col", "2" }, { "value", 111.0 } });
        CHECK_COND(!isErr(r), "109a: set_cell: " << errText(r));
        const QVector<double> afterEdit = columnValues("R109", "3");
        CHECK_COND(sameValues(afterEdit, { 10, 20, 30 }) && !formulaError("R109", "3").isEmpty(),
                   "109a: auto-recalculation after an edit read another column: " << q(afterEdit));

        // Undo of the removal brings column 1 back; the formula evaluates again
        CHECK_COND(!isErr(rpc(QStringLiteral("app.undo"), QJsonObject{ { "table", "R109" } })),
                   "109a: undo set_cell");
        CHECK_COND(!isErr(rpc(QStringLiteral("app.undo"), QJsonObject{ { "table", "R109" } })),
                   "109a: undo remove_column");
        CHECK_COND(t->numCols() == 3 && t->column(0)->name() == QLatin1String("1"),
                   "109a: undo did not restore column 1");
        r = rpc(QStringLiteral("table.recalculate"), QJsonObject{ { "table", "R109" } });
        CHECK_COND(res(r).value(QStringLiteral("ok")).toBool()
                           && sameValues(columnValues("R109", "3"), { 10, 20, 30 }),
                   "109a: formula after undoing the removal: " << q(columnValues("R109", "3")));

        // letter names A, B, C; C = col("A")+1
        Table *u = w109->newTable(QStringLiteral("L109"), 2, 3);
        CHECK_COND(u != nullptr, "109a: table L109");
        u->setColName(0, QStringLiteral("A"));
        u->setColName(1, QStringLiteral("B"));
        u->setColName(2, QStringLiteral("C"));
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "L109" }, { "col", "A" }, { "data", numbers({ 1, 2 }) } });
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "L109" }, { "col", "B" }, { "data", numbers({ 5, 6 }) } });
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "L109" }, { "col", "C" }, { "formula", "col(\"A\")+1" } });
        CHECK_COND(sameValues(columnValues("L109", "C"), { 2, 3 }),
                   "109a: col(\"A\")+1 before removal: " << q(columnValues("L109", "C")));
        rpc(QStringLiteral("table.remove_column"), QJsonObject{ { "table", "L109" }, { "col", "A" } });
        r = rpc(QStringLiteral("table.recalculate"), QJsonObject{ { "table", "L109" } });
        const QVector<double> letterAfter = columnValues("L109", "C");
        CHECK_COND(!formulaError("L109", "C").isEmpty() && sameValues(letterAfter, { 2, 3 }),
                   "109a: col(\"A\") read column B after A was removed: " << q(letterAfter));
        std::cout << "[PASS] 109a: a removed column referenced by name is a formula error\n";
    }

    // ---- 109b: positional reading where it does not clash with the table's names ----
    {
        Table *t = w109->newTable(QStringLiteral("P109"), 3, 3); // names "1", "2", "3"
        CHECK_COND(t != nullptr, "109b: table P109");
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "P109" }, { "col", "1" }, { "data", numbers({ 1, 2, 3 }) } });
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "P109" }, { "col", "2" }, { "data", numbers({ 4, 5, 6 }) } });
        QJsonObject r = rpc(QStringLiteral("table.set_formula"),
                            QJsonObject{ { "table", "P109" }, { "col", "3" }, { "formula", "col(\"B\")*2" } });
        CHECK_COND(res(r).value(QStringLiteral("ok")).toBool()
                           && sameValues(columnValues("P109", "3"), { 8, 10, 12 }),
                   "109b: col(\"B\") in a table named 1,2,3: " << q(columnValues("P109", "3")));

        Table *u = w109->newTable(QStringLiteral("Q109"), 3, 3);
        CHECK_COND(u != nullptr, "109b: table Q109");
        u->setColName(0, QStringLiteral("A"));
        u->setColName(1, QStringLiteral("B"));
        u->setColName(2, QStringLiteral("C"));
        u->column(1)->setLongName(QStringLiteral("Stress"));
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "Q109" }, { "col", "A" }, { "data", numbers({ 1, 2, 3 }) } });
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "Q109" }, { "col", "B" }, { "data", numbers({ 7, 8, 9 }) } });
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "Q109" }, { "col", "C" }, { "formula", "col(\"2\")+col(\"Stress\")" } });
        CHECK_COND(res(r).value(QStringLiteral("ok")).toBool()
                           && sameValues(columnValues("Q109", "C"), { 14, 16, 18 }),
                   "109b: col(\"2\") / Long Name in a table named A,B,C: " << q(columnValues("Q109", "C")));

        // names such as X, Y or 2024 cannot be positions in a 3-column table: letters and numbers
        // still read by position there
        Table *v = w109->newTable(QStringLiteral("XY109"), 3, 3);
        CHECK_COND(v != nullptr, "109b: table XY109");
        v->setColName(0, QStringLiteral("X"));
        v->setColName(1, QStringLiteral("Y"));
        v->setColName(2, QStringLiteral("2024"));
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "XY109" }, { "col", "X" }, { "data", numbers({ 1, 2, 3 }) } });
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "XY109" }, { "col", "Y" }, { "data", numbers({ 5, 6, 7 }) } });
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "XY109" }, { "col", "2024" }, { "formula", "col(\"A\")+col(\"2\")" } });
        CHECK_COND(res(r).value(QStringLiteral("ok")).toBool()
                           && sameValues(columnValues("XY109", "2024"), { 6, 8, 10 }),
                   "109b: col(\"A\")+col(\"2\") in a table named X,Y,2024: "
                           << q(columnValues("XY109", "2024")));

        // a lone one-letter quantity name (C) does not make the table letter-named
        Table *s = w109->newTable(QStringLiteral("TSC109"), 3, 3);
        CHECK_COND(s != nullptr, "109b: table TSC109");
        s->setColName(0, QStringLiteral("Time"));
        s->setColName(1, QStringLiteral("Strain"));
        s->setColName(2, QStringLiteral("C"));
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "TSC109" }, { "col", "Time" }, { "data", numbers({ 1, 2, 3 }) } });
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "TSC109" }, { "col", "C" }, { "formula", "col(\"A\")*2" } });
        CHECK_COND(res(r).value(QStringLiteral("ok")).toBool()
                           && sameValues(columnValues("TSC109", "C"), { 2, 4, 6 }),
                   "109b: col(\"A\")*2 in a table named Time,Strain,C: " << q(columnValues("TSC109", "C")));

        // two default-named columns: removing "1" leaves "2" alone, col("1") must not read it
        Table *d = w109->newTable(QStringLiteral("D2109"), 3, 2); // names "1", "2"
        CHECK_COND(d != nullptr, "109b: table D2109");
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "D2109" }, { "col", "2" }, { "data", numbers({ 7, 8, 9 }) } });
        d->d_future_table->removeColumns(0, 1);
        CHECK_COND(d->d_future_table->columnCount() == 1
                           && d->d_future_table->column(QStringLiteral("1")) == nullptr,
                   "109b: col(\"1\") after removing column 1 of a 1,2 table resolves to another column");
        std::cout << "[PASS] 109b: column letters, numbers and Long Names still resolve\n";
    }

    // ---- 109c: masking rows read by a formula column ----
    {
        const int N = 20000;
        Table *t = w109->newTable(QStringLiteral("M109"), N, 3);
        CHECK_COND(t != nullptr && t->numRows() == N, "109c: table M109");
        QJsonArray xs;
        for (int i = 0; i < N; ++i)
            xs.append(double(i));
        QJsonObject r = rpc(QStringLiteral("table.set_column_data"),
                            QJsonObject{ { "table", "M109" }, { "col", "1" }, { "data", xs } });
        CHECK_COND(!isErr(r), "109c: set column 1: " << errText(r));
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "M109" }, { "col", "3" }, { "formula", "col(\"1\")*2" } });
        CHECK_COND(res(r).value(QStringLiteral("ok")).toBool(), "109c: set_formula: " << errText(r));
        Column *src = t->column(0);
        Column *fx = t->column(2);
        CHECK_COND(std::fabs(fx->valueAt(12345) - 24690.0) < 1e-9, "109c: formula values");

        QElapsedTimer timer;
        timer.start();
        r = rpc(QStringLiteral("table.mask_rows"),
                QJsonObject{ { "table", "M109" }, { "col", 0 }, { "start_row", 10 }, { "end_row", 8009 } });
        const qint64 msRange = timer.elapsed();
        std::cout << "[INFO] 109c: mask 8000 of 20000 rows with a dependent formula column: "
                  << msRange << " ms\n";
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("changed_cells")).toInt() == 8000,
                   "109c: mask_rows range: " << errText(r));
        CHECK_COND(msRange < 5000 * kTestTimingScale, "109c: masking 8000 rows took " << msRange << " ms");
        CHECK_COND(src->isMasked(10) && src->isMasked(8009) && !src->isMasked(9) && !src->isMasked(8010),
                   "109c: masked range");
        CHECK_COND(std::fabs(fx->valueAt(100) - 200.0) < 1e-9 && fx->formulaError().isEmpty(),
                   "109c: formula values changed by masking");

        // Undo / Redo of the mask
        CHECK_COND(!isErr(rpc(QStringLiteral("app.undo"), QJsonObject{ { "table", "M109" } })),
                   "109c: undo mask");
        CHECK_COND(!src->isMasked(10) && !src->isMasked(8009) && src->maskedIntervals().isEmpty(),
                   "109c: undo did not remove the masks");
        CHECK_COND(!isErr(rpc(QStringLiteral("app.redo"), QJsonObject{ { "table", "M109" } })),
                   "109c: redo mask");
        CHECK_COND(src->isMasked(10) && src->isMasked(8009) && !src->isMasked(8010),
                   "109c: redo did not restore the masks");
        CHECK_COND(!isErr(rpc(QStringLiteral("app.undo"), QJsonObject{ { "table", "M109" } })),
                   "109c: undo mask again");

        // a list of 2000 scattered rows (every second row): mask one by one, unmask all
        QJsonArray rows;
        for (int i = 0; i < 4000; i += 2)
            rows.append(i);
        timer.restart();
        r = rpc(QStringLiteral("table.mask_rows"),
                QJsonObject{ { "table", "M109" }, { "col", 0 }, { "rows", rows } });
        const qint64 msList = timer.elapsed();
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("changed_cells")).toInt() == 2000,
                   "109c: mask_rows list: " << errText(r));
        CHECK_COND(src->isMasked(0) && !src->isMasked(1) && src->isMasked(3998) && !src->isMasked(3999),
                   "109c: masked list rows");
        timer.restart();
        r = rpc(QStringLiteral("table.unmask_rows"), QJsonObject{ { "table", "M109" }, { "col", 0 } });
        const qint64 msUnmask = timer.elapsed();
        std::cout << "[INFO] 109c: mask 2000 scattered rows " << msList << " ms, unmask all "
                  << msUnmask << " ms\n";
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("changed_cells")).toInt() == 2000
                           && src->maskedIntervals().isEmpty(),
                   "109c: unmask_rows: " << errText(r));
        CHECK_COND(msList < 5000 * kTestTimingScale && msUnmask < 5000 * kTestTimingScale,
                   "109c: scattered mask " << msList << " ms / unmask " << msUnmask << " ms");
        CHECK_COND(!isErr(rpc(QStringLiteral("app.undo"), QJsonObject{ { "table", "M109" } })),
                   "109c: undo unmask");
        CHECK_COND(src->isMasked(0) && src->isMasked(3998) && !src->isMasked(1),
                   "109c: undo of unmask did not restore the scattered masks");

        // a list given out of order with a repeated row counts each row once
        r = rpc(QStringLiteral("table.mask_rows"),
                QJsonObject{ { "table", "M109" }, { "col", 0 }, { "rows", QJsonArray{ 7, 5, 5, 6, 3 } } });
        // (rows 0, 2, 4, ... are masked again after the Undo; 3, 5 and 7 change)
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("changed_cells")).toInt() == 3
                           && src->isMasked(3) && src->isMasked(5) && src->isMasked(7)
                           && !src->isMasked(9),
                   "109c: unsorted list with a repeated row: "
                           << res(r).value(QStringLiteral("changed_cells")).toInt());

        // data changes still recalculate the formula column
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "M109" }, { "row", 1 }, { "col", "1" }, { "value", 500.0 } });
        CHECK_COND(!isErr(r) && std::fabs(fx->valueAt(1) - 1000.0) < 1e-9,
                   "109c: formula column not recalculated after a data change: " << fx->valueAt(1));

        // GUI: mask a selection of two runs of rows in column 2 (one Undo step)
        Column *c2 = t->column(1);
        t->deselectAll();
        t->setCellsSelected(100, 1, 349, 1, true);
        t->setCellsSelected(200, 1, 299, 1, false);
        QUndoStack *us = t->d_future_table->undoStack();
        const int idxBefore = us ? us->index() : 0;
        t->d_future_table->maskSelection();
        CHECK_COND(c2->isMasked(100) && c2->isMasked(199) && c2->isMasked(300) && c2->isMasked(349)
                           && !c2->isMasked(200) && !c2->isMasked(299) && !c2->isMasked(350),
                   "109c: GUI mask of a two-run selection");
        CHECK_COND(us && us->index() == idxBefore + 1, "109c: GUI mask is not one Undo step");
        us->undo();
        CHECK_COND(c2->maskedIntervals().isEmpty(), "109c: Undo of the GUI mask");
        t->deselectAll();
        std::cout << "[PASS] 109c: masking rows read by a formula column is fast and undoable\n";
    }

    std::cout << "[PASS] Suite 109: worksheet formula references and masks\n";
    return 0;
}
