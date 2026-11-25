#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/future_Table.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
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
//   109d sorting a table of 100000 rows x 20 columns whose column 1 has 50000 masked rows
//        (scattered over the rows by the sort) and 1 % empty cells, with a formula column:
//        sort, Undo and Redo take seconds at most (one mask command per masked run of rows
//        made them grow with the square of the rows: 96 s to sort, Undo over 120 s); masks
//        and empty cells move with their rows, the formula column stays consistent, and
//        Undo / Redo restore the exact masks and values
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

    // ---- 109d: sorting a large table with scattered masks, Undo and Redo ----
    {
        const int N = 100000;
        const int C = 20;
        Table *t = w109->newTable(QStringLiteral("S109"), N, C);
        CHECK_COND(t != nullptr && t->numRows() == N && t->numCols() == C, "109d: table S109");
        quint32 seed = 12345u;
        auto nextRand = [&seed]() {
            seed = seed * 1664525u + 1013904223u;
            return seed;
        };
        // random values with about 1 % empty cells; the key (column 2) values are distinct and
        // in random order, so the sort scatters the masked rows 1001..51000 of column 1
        QVector<int> keyOrder(N);
        for (int k = 0; k < N; ++k)
            keyOrder[k] = k;
        for (int k = N - 1; k > 0; --k)
            std::swap(keyOrder[k], keyOrder[int((nextRand() >> 8) % quint32(k + 1))]);
        for (int c = 0; c < C - 1; ++c) {
            QJsonArray arr;
            for (int k = 0; k < N; ++k) {
                if ((nextRand() >> 8) % 100 == 0) {
                    arr.append(QJsonValue(QJsonValue::Null));
                } else {
                    const double v = (c == 1) ? double(keyOrder.at(k)) * 0.5
                                              : double((nextRand() >> 8) % 1000000) * 1e-3;
                    arr.append(v);
                }
            }
            const QJsonObject r = rpc(QStringLiteral("table.set_column_data"),
                                      QJsonObject{ { "table", "S109" }, { "col", c }, { "data", arr } });
            CHECK_COND(!isErr(r), "109d: set column " << c << ": " << errText(r));
        }
        QJsonObject r = rpc(QStringLiteral("table.set_formula"),
                            QJsonObject{ { "table", "S109" },
                                         { "col", C - 1 },
                                         { "formula", "col(\"1\")*2+col(\"2\")" } });
        CHECK_COND(res(r).value(QStringLiteral("ok")).toBool(), "109d: set_formula: " << errText(r));
        r = rpc(QStringLiteral("table.mask_rows"),
                QJsonObject{ { "table", "S109" }, { "col", 0 }, { "start_row", 1000 }, { "end_row", 50999 } });
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("changed_cells")).toInt() == 50000,
                   "109d: mask_rows: " << errText(r));

        // per-row flags from the interval lists (isMasked(row) scans every interval)
        auto expand = [](const QList<Interval<int>> &ivs, int n) {
            QVector<bool> f(n, false);
            for (const Interval<int> &iv : ivs)
                for (int k = qMax(0, iv.start()); k <= qMin(n - 1, iv.end()); ++k)
                    f[k] = true;
            return f;
        };
        struct Snap
        {
            QVector<QVector<double>> values; // NaN = empty
            QList<Interval<int>> masks0;
        };
        auto snapshot = [&]() {
            Snap s;
            for (int c = 0; c < C; ++c) {
                Column *col = t->column(c);
                const QVector<bool> inv = expand(col->invalidIntervals(), N);
                QVector<double> v(N);
                for (int k = 0; k < N; ++k)
                    v[k] = inv.at(k) ? std::nan("") : col->valueAt(k);
                s.values.append(v);
            }
            s.masks0 = t->column(0)->maskedIntervals();
            return s;
        };
        auto sameDouble = [](double a, double b) {
            return (std::isnan(a) && std::isnan(b)) || (!std::isnan(a) && !std::isnan(b) && a == b);
        };
        auto sameSnap = [&](const Snap &a, const Snap &b, std::string *why) {
            for (int c = 0; c < C; ++c)
                for (int k = 0; k < N; ++k)
                    if (!sameDouble(a.values.at(c).at(k), b.values.at(c).at(k))) {
                        *why = "column " + std::to_string(c + 1) + " row " + std::to_string(k);
                        return false;
                    }
            if (expand(a.masks0, N) != expand(b.masks0, N)) {
                *why = "masks of column 1";
                return false;
            }
            return true;
        };
        const Snap before = snapshot();
        QHash<double, int> rowOfKey; // row of each key value before the sort
        for (int k = 0; k < N; ++k)
            if (!std::isnan(before.values.at(1).at(k)))
                rowOfKey.insert(before.values.at(1).at(k), k);
        const QVector<bool> maskedBefore = expand(before.masks0, N);

        QElapsedTimer timer;
        timer.start();
        r = rpc(QStringLiteral("table.sort"),
                QJsonObject{ { "table", "S109" }, { "by", 1 }, { "ascending", false } });
        const qint64 msSort = timer.elapsed();
        CHECK_COND(!isErr(r), "109d: sort: " << errText(r));
        // (one Undo step: the single app.undo below restores the whole table)
        const Snap sorted = snapshot();
        std::cout << "[INFO] 109d: sort 100000 x 20 with 50000 masked rows: " << msSort << " ms, "
                  << sorted.masks0.size() << " masked runs after the sort\n";

        // key column descending with the empty cells at the end; values and masks moved with
        // their rows; the formula column matches its sources row by row
        const QVector<bool> maskedSorted = expand(sorted.masks0, N);
        int maskedCount = 0;
        for (bool m : maskedSorted)
            maskedCount += m ? 1 : 0;
        CHECK_COND(maskedCount == 50000, "109d: masked rows after the sort: " << maskedCount);
        int validKeys = 0;
        for (int k = 0; k < N; ++k) {
            const double key = sorted.values.at(1).at(k);
            if (std::isnan(key)) {
                for (int j = k; j < N; ++j)
                    CHECK_COND(std::isnan(sorted.values.at(1).at(j)),
                               "109d: an empty key cell is not at the end (row " << j << ")");
                break;
            }
            ++validKeys;
            if (k > 0)
                CHECK_COND(key <= sorted.values.at(1).at(k - 1), "109d: not descending at row " << k);
            const int src = rowOfKey.value(key, -1);
            CHECK_COND(src >= 0, "109d: unknown key at row " << k);
            CHECK_COND(sameDouble(sorted.values.at(0).at(k), before.values.at(0).at(src))
                               && sameDouble(sorted.values.at(5).at(k), before.values.at(5).at(src)),
                       "109d: row " << k << " does not carry the values of row " << src);
            CHECK_COND(maskedSorted.at(k) == maskedBefore.at(src),
                       "109d: the mask of row " << src << " did not move with it to row " << k);
            const double a = sorted.values.at(0).at(k);
            const double f = sorted.values.at(C - 1).at(k);
            if (!std::isnan(a))
                CHECK_COND(std::fabs(a * 2 + key - f) <= 1e-9 * std::fabs(f) + 1e-9,
                           "109d: formula column inconsistent at row " << k);
        }
        CHECK_COND(validKeys == rowOfKey.size(), "109d: key cells lost: " << validKeys);
        CHECK_COND(msSort < 5000 * kTestTimingScale, "109d: sort took " << msSort << " ms");

        timer.restart();
        CHECK_COND(!isErr(rpc(QStringLiteral("app.undo"), QJsonObject{ { "table", "S109" } })),
                   "109d: undo sort");
        const qint64 msUndo = timer.elapsed();
        std::string why;
        CHECK_COND(sameSnap(snapshot(), before, &why), "109d: Undo did not restore the table: " << why);
        CHECK_COND(t->column(0)->maskedIntervals().size() == 1, "109d: Undo: the masks are not one run");
        timer.restart();
        CHECK_COND(!isErr(rpc(QStringLiteral("app.redo"), QJsonObject{ { "table", "S109" } })),
                   "109d: redo sort");
        const qint64 msRedo = timer.elapsed();
        CHECK_COND(sameSnap(snapshot(), sorted, &why), "109d: Redo did not restore the sort: " << why);
        std::cout << "[INFO] 109d: Undo " << msUndo << " ms, Redo " << msRedo << " ms\n";
        CHECK_COND(msUndo < 5000 * kTestTimingScale && msRedo < 5000 * kTestTimingScale,
                   "109d: Undo " << msUndo << " ms / Redo " << msRedo << " ms");
        CHECK_COND(!isErr(rpc(QStringLiteral("app.undo"), QJsonObject{ { "table", "S109" } })),
                   "109d: undo the sort again");
        std::cout << "[PASS] 109d: sorting a large table with scattered masks is fast and undoable\n";
    }

    std::cout << "[PASS] Suite 109: worksheet formula references and masks\n";
    return 0;
}
