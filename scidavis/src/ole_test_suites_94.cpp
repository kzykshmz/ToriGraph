#include "ole_test_common.h"
#include "Folder.h"
#include "PythonAutomationServer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUndoStack>

// =============================================================================
// Suite 94: worksheet data fidelity through the JSON-RPC server.
//   94a add_column / set_column_data with mode=datetime|month write the values and keep the
//       type (strings via the GUI text conversion, numbers via the GUI number conversion)
//   94b get_column_data / get_cell return date/time, month and day cells as displayed text, and
//       set_cell on a date/time column keeps the type and parses the value
//   94c one RPC call is one Undo step: paste_tsv (with header), set_data_dict, resize,
//       set_cell with a type change, matrix.flip rot90_cw
//   94d set_data_dict on an existing table writes keys to the columns of the same name and
//       appends the others; with no matching name it still writes from the left
//   94e circular formulas: set_formula / table.recalculate / app.recalculate report a formula
//       error and the values stay put (also in manual recalculation mode)
//   94f atomic app.batch rollback keeps the root folder name and reports undo_history_cleared
//   94g table.recalculate / set_formula evaluate only the named worksheet (only `col`; no
//       dependent columns of a manual worksheet), other manual worksheets are left alone, and a
//       cycle through a manual worksheet is still reported
//   94h set_cell with a number on date/time / month / day columns converts the number like
//       set_column_data(mode=...) (a day count, not "3" read as 03:00)
//   94i a rollback that fails before clearing the workspace does not report
//       undo_history_cleared
// =============================================================================

int runSuite94RpcWorksheetDataFidelity(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 94: JSON-RPC worksheet data fidelity...\n";

    struct NonInteractiveQuitGuard94
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard94()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard94()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard94;

    struct AppWinGuard94
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard94(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard94() { reset(); }
        AppWinGuard94(const AppWinGuard94 &) = delete;
        AppWinGuard94 &operator=(const AppWinGuard94 &) = delete;
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
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    AppWinGuard94 w94(new ApplicationWindow());
    w94->applyUserSettings();
    w94->confirmCloseTable = false;
    w94->confirmCloseMatrix = false;
    w94->confirmClosePlot2D = false;
    w94->confirmClosePlot3D = false;
    w94->confirmCloseFolder = false;
    w94->resize(1280, 800);
    w94->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w94->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "94: pythonAutomationServer null");

    int rpcId = 9400;
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
    auto colData = [&](const QString &tbl, const QString &col) -> QJsonArray {
        return res(rpc(QStringLiteral("table.get_column_data"),
                       QJsonObject{ { "table", tbl }, { "col", col } }))
                .value(QStringLiteral("data"))
                .toArray();
    };
    auto colMode = [](Table *t, int c) {
        return t && t->column(c) ? t->column(c)->columnMode() : SciDAVis::ColumnMode::Numeric;
    };
    // whole visible state of a table: size, names, types, invalid flags and displayed texts
    auto tableState = [](Table *t) -> QStringList {
        QStringList st;
        if (!t)
            return st;
        st << QStringLiteral("%1x%2").arg(t->numRows()).arg(t->numCols());
        for (int c = 0; c < t->numCols(); ++c) {
            Column *col = t->column(c);
            st << t->colLabel(c) << QString::number(int(col->columnMode()))
               << QString::number(int(t->colPlotDesignation(c)));
            for (int r = 0; r < t->numRows(); ++r)
                st << (col->isInvalid(r) ? QStringLiteral("<inv>") : t->text(r, c));
        }
        return st;
    };
    auto undoStackOf = [](Table *t) -> QUndoStack * {
        return (t && t->d_future_table) ? t->d_future_table->undoStack() : nullptr;
    };

    // -------------------------------------------------------------------------
    // 94a: date/time, month and day modes on write
    // -------------------------------------------------------------------------
    {
        Table *t = w94->newTable(4, 1, QStringLiteral("T94DT"), QStringLiteral("DT94"));
        CHECK_COND(t != nullptr, "94a: newTable failed");
        const QJsonArray dates{ QStringLiteral("2024-03-01 10:00:00"), QStringLiteral("2024-03-01"),
                                QJsonValue::Null, QStringLiteral("2023-12-31 23:59:59") };
        QJsonObject r = rpc(QStringLiteral("table.add_column"),
                            QJsonObject{ { "table", "T94DT" }, { "name", "When" },
                                         { "mode", "datetime" }, { "data", dates } });
        CHECK_COND(!isErr(r) && res(r).value("mode").toString() == "datetime",
                   "94a: add_column(mode=datetime) did not keep the type: "
                           << res(r).value("mode").toString().toStdString());
        int cWhen = t->numCols() - 1;
        CHECK_COND(colMode(t, cWhen) == SciDAVis::ColumnMode::DateTime,
                   "94a: column When is not a date/time column");
        CHECK_COND(!t->column(cWhen)->isInvalid(0) && !t->column(cWhen)->isInvalid(1)
                           && t->column(cWhen)->isInvalid(2) && !t->column(cWhen)->isInvalid(3),
                   "94a: add_column(mode=datetime) lost values (validity 0..3 = "
                           << !t->column(cWhen)->isInvalid(0) << !t->column(cWhen)->isInvalid(1)
                           << !t->column(cWhen)->isInvalid(2) << !t->column(cWhen)->isInvalid(3)
                           << ")");
        CHECK_COND(t->column(cWhen)->dateTimeAt(0)
                           == QDateTime(QDate(2024, 3, 1), QTime(10, 0, 0))
                           && t->column(cWhen)->dateTimeAt(3)
                                   == QDateTime(QDate(2023, 12, 31), QTime(23, 59, 59)),
                   "94a: add_column(mode=datetime) parsed wrong date/times");

        // set_column_data with mode=datetime over an existing text column
        r = rpc(QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T94DT" }, { "name", "S2" }, { "mode", "text" },
                             { "data", QJsonArray{ "a", "b", "c", "d" } } });
        CHECK_COND(!isErr(r), "94a: add_column(text) failed");
        r = rpc(QStringLiteral("table.set_column_data"),
                QJsonObject{ { "table", "T94DT" }, { "col", "S2" }, { "mode", "datetime" },
                             { "data", dates } });
        const int cS2 = t->numCols() - 1;
        CHECK_COND(!isErr(r) && colMode(t, cS2) == SciDAVis::ColumnMode::DateTime
                           && t->column(cS2)->dateTimeAt(0)
                                   == QDateTime(QDate(2024, 3, 1), QTime(10, 0, 0))
                           && t->column(cS2)->isInvalid(2),
                   "94a: set_column_data(mode=datetime) lost the values or the type");

        // numbers with mode=datetime keep the GUI number -> date/time conversion
        r = rpc(QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T94DT" }, { "name", "NumRef" },
                             { "data", QJsonArray{ 0.0, 1.0, 45352.0, 2.5 } } });
        CHECK_COND(!isErr(r), "94a: add_column(numbers) failed");
        const int cRef = t->numCols() - 1;
        r = rpc(QStringLiteral("table.set_column_meta"),
                QJsonObject{ { "table", "T94DT" }, { "col", "NumRef" }, { "mode", "datetime" } });
        CHECK_COND(!isErr(r) && colMode(t, cRef) == SciDAVis::ColumnMode::DateTime,
                   "94a: set_column_meta(mode=datetime) reference conversion failed");
        r = rpc(QStringLiteral("table.set_column_data"),
                QJsonObject{ { "table", "T94DT" }, { "col", "S2" }, { "mode", "datetime" },
                             { "data", QJsonArray{ 0.0, 1.0, 45352.0, 2.5 } } });
        CHECK_COND(!isErr(r) && colMode(t, cS2) == SciDAVis::ColumnMode::DateTime,
                   "94a: set_column_data(numbers, mode=datetime) changed the type");
        for (int row = 0; row < 4; ++row) {
            CHECK_COND(t->text(row, cS2) == t->text(row, cRef),
                       "94a: numbers with mode=datetime differ from the GUI conversion at row "
                               << row << ": " << t->text(row, cS2).toStdString() << " vs "
                               << t->text(row, cRef).toStdString());
        }
        r = rpc(QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T94DT" }, { "name", "NumDT" }, { "mode", "datetime" },
                             { "data", QJsonArray{ 0.0, 1.0, 45352.0, 2.5 } } });
        const int cNumDT = t->numCols() - 1;
        CHECK_COND(!isErr(r) && colMode(t, cNumDT) == SciDAVis::ColumnMode::DateTime
                           && t->text(2, cNumDT) == t->text(2, cRef),
                   "94a: add_column(numbers, mode=datetime) did not convert like the GUI");

        // month names: the GUI conversion reads them in the locale's spelling (QDate "MMMM")
        const QString januaryName = QDate(2000, 1, 1).toString(QStringLiteral("MMMM"));
        r = rpc(QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T94DT" }, { "name", "M" }, { "mode", "month" },
                             { "data", QJsonArray{ januaryName, "3", QJsonValue::Null, "12" } } });
        const int cM = t->numCols() - 1;
        CHECK_COND(!isErr(r) && colMode(t, cM) == SciDAVis::ColumnMode::Month,
                   "94a: add_column(mode=month) did not keep the type");
        CHECK_COND(!t->column(cM)->isInvalid(0) && t->column(cM)->dateAt(0).month() == 1
                           && !t->column(cM)->isInvalid(1) && t->column(cM)->dateAt(1).month() == 3
                           && t->column(cM)->isInvalid(2),
                   "94a: add_column(mode=month) lost the month values");
    }

    // -------------------------------------------------------------------------
    // 94b: reading date/time / month / day cells, set_cell on a date/time column
    // -------------------------------------------------------------------------
    {
        Table *t = w94->table(QStringLiteral("T94DT"));
        CHECK_COND(t != nullptr, "94b: T94DT missing");
        const QJsonArray when = colData(QStringLiteral("T94DT"), QStringLiteral("When"));
        CHECK_COND(when.size() == 4 && when.at(0).isString()
                           && when.at(0).toString() == t->text(0, 1)
                           && when.at(0).toString().startsWith(QStringLiteral("2024-03-01"))
                           && when.at(2).isNull(),
                   "94b: get_column_data of a date/time column is not the displayed text: "
                           << QJsonDocument(when).toJson(QJsonDocument::Compact).toStdString());
        QJsonObject cell = res(rpc(QStringLiteral("table.get_cell"),
                                   QJsonObject{ { "table", "T94DT" }, { "row", 0 }, { "col", "M" } }));
        CHECK_COND(cell.value("value").isString() && !cell.value("value").toString().isEmpty()
                           && cell.value("value").toString() == cell.value("text").toString(),
                   "94b: get_cell of a month column is empty");

        // day-of-week column via set_column_meta
        QJsonObject r = rpc(QStringLiteral("table.add_column"),
                            QJsonObject{ { "table", "T94DT" }, { "name", "Dow" },
                                         { "data", QJsonArray{ 1.0, 2.0, 7.0, 3.0 } } });
        r = rpc(QStringLiteral("table.set_column_meta"),
                QJsonObject{ { "table", "T94DT" }, { "col", "Dow" }, { "mode", "day" } });
        const QJsonArray dow = colData(QStringLiteral("T94DT"), QStringLiteral("Dow"));
        const int cDow = t->numCols() - 1;
        CHECK_COND(dow.size() == 4 && dow.at(0).isString() && !dow.at(0).toString().isEmpty()
                           && dow.at(0).toString() == t->text(0, cDow),
                   "94b: get_column_data of a day-of-week column is empty");

        const int cWhen = 1;
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T94DT" }, { "row", 2 }, { "col", "When" },
                             { "value", "2025-01-02 03:04:05" } });
        CHECK_COND(!isErr(r) && colMode(t, cWhen) == SciDAVis::ColumnMode::DateTime,
                   "94b: set_cell turned the date/time column into another type");
        CHECK_COND(!t->column(cWhen)->isInvalid(2)
                           && t->column(cWhen)->dateTimeAt(2)
                                   == QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5))
                           && t->column(cWhen)->dateTimeAt(0)
                                   == QDateTime(QDate(2024, 3, 1), QTime(10, 0, 0)),
                   "94b: set_cell on a date/time column did not parse the value");
    }

    // -------------------------------------------------------------------------
    // 94c: one RPC call = one Undo step
    // -------------------------------------------------------------------------
    {
        auto makeFixture = [&](const QString &name) -> Table * {
            Table *t = w94->newTable(6, 3, name, name);
            if (!t)
                return nullptr;
            for (int r = 0; r < 6; ++r) {
                t->setCell(r, 0, double(r + 1));
                t->setCell(r, 1, 10.0 * (r + 1));
                t->setCell(r, 2, 0.5 * r);
            }
            return t;
        };
        struct Op
        {
            const char *label;
            const char *method;
            QJsonObject params;
        };
        const QList<Op> ops{
            { "paste_tsv header", "table.paste_tsv",
              QJsonObject{ { "text", "a\tb\n1\t2\n3\t4\n" }, { "header", true } } },
            { "paste_tsv grow", "table.paste_tsv",
              QJsonObject{ { "text", "1\t2\n3\t4\n5\t6\n7\t8\n9\t10\n11\t12\n13\t14\n" },
                           { "header", false },
                           { "start_row", 2 },
                           { "start_col", 1 } } },
            { "set_data_dict", "table.set_data_dict",
              QJsonObject{ { "data", QJsonObject{ { "x", QJsonArray{ 1, 2, 3 } },
                                                  { "y", QJsonArray{ 4, 5, 6 } } } } } },
            { "resize up", "table.resize", QJsonObject{ { "rows", 10 }, { "cols", 5 } } },
            { "resize down", "table.resize", QJsonObject{ { "rows", 3 }, { "cols", 2 } } },
            { "set_cell text", "table.set_cell",
              QJsonObject{ { "row", 2 }, { "col", 1 }, { "value", "abc" } } },
            { "set_cell grow", "table.set_cell",
              QJsonObject{ { "row", 9 }, { "col", 0 }, { "value", 5 } } },
        };
        int k = 0;
        for (const Op &op : ops) {
            const QString name = QStringLiteral("T94U%1").arg(k++);
            Table *t = makeFixture(name);
            CHECK_COND(t != nullptr, "94c: fixture " << op.label);
            QUndoStack *us = undoStackOf(t);
            CHECK_COND(us != nullptr, "94c: no undo stack");
            us->clear(); // stay below the project's undo limit, so index() counts the steps
            const QStringList before = tableState(t);
            const int idx0 = us->index();
            QJsonObject p = op.params;
            p[QStringLiteral("table")] = name;
            const QJsonObject r = rpc(QString::fromLatin1(op.method), p);
            CHECK_COND(!isErr(r), "94c: " << op.label << " failed");
            const QStringList after = tableState(t);
            CHECK_COND(after != before, "94c: " << op.label << " changed nothing");
            CHECK_COND(us->index() == idx0 + 1,
                       "94c: " << op.label << " made " << (us->index() - idx0) << " Undo steps");
            us->undo();
            QCoreApplication::processEvents();
            const QStringList undone = tableState(t);
            CHECK_COND(undone == before,
                       "94c: one Undo of " << op.label << " did not restore the table ("
                                           << undone.join('|').left(300).toStdString() << ")");
            us->redo();
            QCoreApplication::processEvents();
            CHECK_COND(tableState(t) == after, "94c: Redo of " << op.label << " differs");
        }

        // matrix rotation = transpose + mirror in one step
        Matrix *m = w94->newMatrix(QStringLiteral("M94"), 2, 3);
        CHECK_COND(m != nullptr, "94c: newMatrix failed");
        QJsonObject r = rpc(QStringLiteral("matrix.set_data"),
                            QJsonObject{ { "matrix", "M94" },
                                         { "data", QJsonArray{ QJsonArray{ 1, 2, 3 },
                                                               QJsonArray{ 4, 5, 6.5 } } } });
        CHECK_COND(!isErr(r), "94c: matrix.set_data failed");
        QUndoStack *mus = m->d_future_matrix ? m->d_future_matrix->undoStack() : nullptr;
        CHECK_COND(mus != nullptr, "94c: matrix undo stack missing");
        mus->clear();
        const int midx = mus->index();
        r = rpc(QStringLiteral("matrix.flip"), QJsonObject{ { "matrix", "M94" }, { "mode", "rot90_cw" } });
        CHECK_COND(!isErr(r) && m->numRows() == 3 && m->numCols() == 2
                           && std::fabs(m->cell(0, 0) - 4.0) < 1e-12
                           && std::fabs(m->cell(0, 1) - 1.0) < 1e-12,
                   "94c: rot90_cw result wrong");
        CHECK_COND(mus->index() == midx + 1,
                   "94c: matrix.flip rot90_cw made " << (mus->index() - midx) << " Undo steps");
        mus->undo();
        QCoreApplication::processEvents();
        CHECK_COND(m->numRows() == 2 && m->numCols() == 3 && std::fabs(m->cell(0, 0) - 1.0) < 1e-12
                           && std::fabs(m->cell(1, 2) - 6.5) < 1e-12,
                   "94c: one Undo of matrix rot90_cw did not restore the matrix");
        r = rpc(QStringLiteral("matrix.flip"), QJsonObject{ { "matrix", "M94" }, { "mode", "bogus" } });
        CHECK_COND(isErr(r) && mus->index() == midx,
                   "94c: rejected matrix.flip mode left an Undo step");
    }

    // -------------------------------------------------------------------------
    // 94d: set_data_dict by column name
    // -------------------------------------------------------------------------
    {
        Table *t = w94->newTable(3, 3, QStringLiteral("T94N"), QStringLiteral("N94"));
        CHECK_COND(t != nullptr, "94d: newTable failed");
        t->setColName(0, QStringLiteral("x"));
        t->setColName(1, QStringLiteral("y"));
        t->setColName(2, QStringLiteral("z"));
        for (int r = 0; r < 3; ++r) {
            t->setCell(r, 0, double(r + 1));
            t->setCell(r, 1, double(r + 4));
            t->setCell(r, 2, double(r + 7));
        }
        QJsonObject r = rpc(QStringLiteral("table.set_data_dict"),
                            QJsonObject{ { "table", "T94N" },
                                         { "data", QJsonObject{ { "y", QJsonArray{ 10, 20, 30 } } } } });
        CHECK_COND(!isErr(r) && t->numCols() == 3 && t->colLabel(0) == "x" && t->colLabel(1) == "y"
                           && t->colLabel(2) == "z",
                   "94d: set_data_dict {y} renamed or added columns: " << t->colLabel(0).toStdString()
                                                                       << "," << t->colLabel(1).toStdString());
        CHECK_COND(t->cell(0, 0) == 1.0 && t->cell(2, 0) == 3.0 && t->cell(0, 1) == 10.0
                           && t->cell(2, 1) == 30.0 && t->cell(0, 2) == 7.0,
                   "94d: set_data_dict {y} wrote into the wrong column");

        r = rpc(QStringLiteral("table.set_data_dict"),
                QJsonObject{ { "table", "T94N" },
                             { "data", QJsonObject{ { "z", QJsonArray{ -1, -2, -3 } },
                                                    { "w", QJsonArray{ 5, 6, 7 } } } },
                             { "column_order", QJsonArray{ "z", "w" } } });
        CHECK_COND(!isErr(r) && t->numCols() == 4 && t->colLabel(3) == "w" && t->cell(0, 2) == -1.0
                           && t->cell(1, 3) == 6.0 && t->cell(0, 0) == 1.0 && t->cell(0, 1) == 10.0,
                   "94d: set_data_dict {z, w} did not overwrite z and append w");

        // no key names a column: written from the left (fresh default columns), as before
        Table *t2 = w94->newTable(2, 2, QStringLiteral("T94P"), QStringLiteral("P94"));
        r = rpc(QStringLiteral("table.set_data_dict"),
                QJsonObject{ { "table", "T94P" },
                             { "data", QJsonObject{ { "Z_Temp", QJsonArray{ 300.0, 400.0 } },
                                                    { "A_Rate", QJsonArray{ 1.5, 2.5 } } } },
                             { "column_order", QJsonArray{ "Z_Temp", "A_Rate" } } });
        CHECK_COND(!isErr(r) && t2->numCols() == 2 && t2->colLabel(0) == "Z_Temp"
                           && t2->colLabel(1) == "A_Rate" && t2->cell(1, 0) == 400.0
                           && t2->cell(0, 1) == 1.5,
                   "94d: set_data_dict on default columns no longer writes from the left");
        // same names and order again: same result, no extra columns
        r = rpc(QStringLiteral("table.set_data_dict"),
                QJsonObject{ { "table", "T94P" },
                             { "data", QJsonObject{ { "Z_Temp", QJsonArray{ 1.0, 2.0 } },
                                                    { "A_Rate", QJsonArray{ 3.0, 4.0 } } } },
                             { "column_order", QJsonArray{ "Z_Temp", "A_Rate" } } });
        CHECK_COND(!isErr(r) && t2->numCols() == 2 && t2->cell(1, 0) == 2.0 && t2->cell(1, 1) == 4.0,
                   "94d: set_data_dict with the same names did not overwrite in place");
    }

    // -------------------------------------------------------------------------
    // 94e: circular formulas
    // -------------------------------------------------------------------------
    {
        Table *t = w94->newTable(3, 3, QStringLiteral("T94C"), QStringLiteral("C94"));
        CHECK_COND(t != nullptr, "94e: newTable failed");
        t->setColName(0, QStringLiteral("C"));
        t->setColName(1, QStringLiteral("D"));
        t->setColName(2, QStringLiteral("E"));
        const QJsonObject rc = res(rpc(QStringLiteral("table.set_formula"),
                                       QJsonObject{ { "table", "T94C" }, { "col", "C" },
                                                    { "formula", "col(\"D\")+1" } }));
        const QJsonObject rd = res(rpc(QStringLiteral("table.set_formula"),
                                       QJsonObject{ { "table", "T94C" }, { "col", "D" },
                                                    { "formula", "col(\"C\")+1" } }));
        CHECK_COND(rd.value("ok").toBool() == false
                           && !rd.value("formula_error").toString().isEmpty(),
                   "94e: set_formula closing a cycle reported ok / no formula_error");
        const QStringList s0 = tableState(t);
        for (int pass = 0; pass < 3; ++pass) {
            const QJsonObject ra = res(rpc(QStringLiteral("app.recalculate"), QJsonObject{}));
            CHECK_COND(ra.value("ok").toBool() == false
                               && !ra.value("formula_errors").toArray().isEmpty(),
                       "94e: app.recalculate did not report the circular formula");
            CHECK_COND(tableState(t) == s0, "94e: app.recalculate pass " << pass
                                                                         << " changed the circular columns");
        }
        const QJsonObject rt = res(rpc(QStringLiteral("table.recalculate"),
                                       QJsonObject{ { "table", "T94C" }, { "col", "C" } }));
        CHECK_COND(rt.value("ok").toBool() == false && tableState(t) == s0,
                   "94e: table.recalculate changed / accepted the circular column");
        const QJsonObject meta = res(rpc(QStringLiteral("table.get_column_meta"),
                                         QJsonObject{ { "table", "T94C" }, { "col", "C" } }));
        CHECK_COND(!meta.value("formula_error").toString().isEmpty(),
                   "94e: get_column_meta has no formula_error for the circular column");
        Q_UNUSED(rc);

        // a non-circular formula next to it still evaluates
        rpc(QStringLiteral("table.set_column_data"),
            QJsonObject{ { "table", "T94C" }, { "col", "E" }, { "data", QJsonArray{ 1, 2, 3 } } });
        t->addCol();
        const int cF = t->numCols() - 1;
        const QJsonObject rf = res(rpc(QStringLiteral("table.set_formula"),
                                       QJsonObject{ { "table", "T94C" }, { "col", cF },
                                                    { "formula", "col(\"E\")*2" } }));
        CHECK_COND(rf.value("ok").toBool() && rf.value("formula_error").toString().isEmpty()
                           && t->cell(2, cF) == 6.0,
                   "94e: plain formula next to a cycle did not evaluate");

        // manual recalculation mode: the same cycle check
        Table *tm = w94->newTable(3, 2, QStringLiteral("T94CM"), QStringLiteral("CM94"));
        CHECK_COND(tm != nullptr, "94e: newTable manual failed");
        tm->setAutoRecalcFormulas(false);
        tm->setColName(0, QStringLiteral("P"));
        tm->setColName(1, QStringLiteral("Q"));
        rpc(QStringLiteral("table.set_formula"),
            QJsonObject{ { "table", "T94CM" }, { "col", "P" }, { "formula", "col(\"Q\")+1" } });
        const QJsonObject rq = res(rpc(QStringLiteral("table.set_formula"),
                                       QJsonObject{ { "table", "T94CM" }, { "col", "Q" },
                                                    { "formula", "col(\"P\")+1" } }));
        CHECK_COND(!rq.value("ok").toBool() && !rq.value("formula_error").toString().isEmpty(),
                   "94e: manual mode set_formula accepted the cycle");
        CHECK_COND(!tm->autoRecalcFormulas(), "94e: manual table switched to auto recalculation");
        const QStringList m0 = tableState(tm);
        rpc(QStringLiteral("table.recalculate"), QJsonObject{ { "table", "T94CM" } });
        rpc(QStringLiteral("app.recalculate"), QJsonObject{});
        rpc(QStringLiteral("app.recalculate"), QJsonObject{});
        CHECK_COND(tableState(tm) == m0 && !tm->autoRecalcFormulas(),
                   "94e: manual mode recalculation changed the circular columns");

        // cycle across two worksheets
        Table *ta = w94->newTable(2, 1, QStringLiteral("T94XA"), QStringLiteral("XA94"));
        Table *tb = w94->newTable(2, 1, QStringLiteral("T94XB"), QStringLiteral("XB94"));
        CHECK_COND(ta && tb, "94e: cross tables");
        ta->setColName(0, QStringLiteral("a"));
        tb->setColName(0, QStringLiteral("b"));
        rpc(QStringLiteral("table.set_formula"),
            QJsonObject{ { "table", "T94XA" }, { "col", "a" },
                         { "formula", "tablecol(\"T94XB\",\"b\")+1" } });
        rpc(QStringLiteral("table.set_formula"),
            QJsonObject{ { "table", "T94XB" }, { "col", "b" },
                         { "formula", "tablecol(\"T94XA\",\"a\")+1" } });
        const QStringList xa0 = tableState(ta), xb0 = tableState(tb);
        rpc(QStringLiteral("app.recalculate"), QJsonObject{});
        rpc(QStringLiteral("app.recalculate"), QJsonObject{});
        rpc(QStringLiteral("table.recalculate"), QJsonObject{ { "table", "T94XA" } });
        CHECK_COND(tableState(ta) == xa0 && tableState(tb) == xb0,
                   "94e: cross-worksheet cycle grew on recalculation ("
                           << ta->text(0, 0).toStdString() << ", " << tb->text(0, 0).toStdString()
                           << ")");
    }

    // -------------------------------------------------------------------------
    // 94f: atomic batch rollback keeps the root folder name
    // -------------------------------------------------------------------------
    {
        const QString rootBefore = w94->projectFolder()->name();
        const QString projBefore = w94->projectname;
        Table *t = w94->table(QStringLiteral("T94N"));
        CHECK_COND(t != nullptr, "94f: T94N missing");
        const double v00 = t->cell(0, 0);
        const QJsonArray cmds{
            QJsonObject{ { "method", "table.set_cell" },
                         { "params", QJsonObject{ { "table", "T94N" }, { "row", 0 }, { "col", 0 },
                                                  { "value", 999.0 } } } },
            QJsonObject{ { "method", "table.set_cell" },
                         { "params", QJsonObject{ { "table", "NoSuchTable94" }, { "row", 0 },
                                                  { "col", 0 }, { "value", 1.0 } } } },
        };
        const QJsonObject r = res(rpc(QStringLiteral("app.batch"),
                                      QJsonObject{ { "commands", cmds }, { "atomic", true } }));
        CHECK_COND(r.value("rolled_back").toBool(), "94f: batch was not rolled back");
        CHECK_COND(r.value("undo_history_cleared").toBool(),
                   "94f: rollback did not report undo_history_cleared");
        CHECK_COND(w94->projectFolder()->name() == rootBefore,
                   "94f: root folder renamed by the rollback: "
                           << w94->projectFolder()->name().toStdString() << " (was "
                           << rootBefore.toStdString() << ")");
        CHECK_COND(w94->projectname == projBefore, "94f: project name changed by the rollback");
        Table *tAfter = w94->table(QStringLiteral("T94N"));
        CHECK_COND(tAfter != nullptr && tAfter->cell(0, 0) == v00,
                   "94f: rollback did not restore T94N");
        // a successful batch still reports undo_history_cleared = false
        const QJsonObject ok = res(rpc(
                QStringLiteral("app.batch"),
                QJsonObject{ { "commands",
                               QJsonArray{ QJsonObject{
                                       { "method", "table.set_cell" },
                                       { "params", QJsonObject{ { "table", "T94N" }, { "row", 0 },
                                                                { "col", 0 }, { "value", 5.0 } } } } } },
                             { "atomic", true } }));
        CHECK_COND(ok.value("ok").toBool() && !ok.value("undo_history_cleared").toBool(true),
                   "94f: successful batch reported undo_history_cleared");
    }

    // -------------------------------------------------------------------------
    // 94g: recalculation scope -- table.recalculate / set_formula evaluate only the named
    //      worksheet (and only `col`), whatever the mode of the other worksheets
    // -------------------------------------------------------------------------
    {
        auto colText = [](Table *t, int c, int rows) {
            QStringList out;
            for (int r = 0; r < rows; ++r)
                out << (t->column(c)->isInvalid(r) ? QStringLiteral("_") : t->text(r, c));
            return out.join(QLatin1Char(','));
        };

        // another worksheet in manual mode that reads this one is not recalculated
        Table *ta = w94->newTable(3, 1, QStringLiteral("T94RA"), QStringLiteral("RA94"));
        Table *tb = w94->newTable(3, 1, QStringLiteral("T94RB"), QStringLiteral("RB94"));
        CHECK_COND(ta && tb, "94g: tables RA/RB");
        ta->setColName(0, QStringLiteral("x"));
        tb->setColName(0, QStringLiteral("b"));
        for (int r = 0; r < 3; ++r)
            ta->setCell(r, 0, double(r + 1));
        ta->addCol();
        ta->setColName(1, QStringLiteral("f"));
        QJsonObject r = rpc(QStringLiteral("table.set_formula"),
                            QJsonObject{ { "table", "T94RA" }, { "col", "f" },
                                         { "formula", "col(\"x\")*2" } });
        CHECK_COND(res(r).value("ok").toBool() && colText(ta, 1, 3) == QLatin1String("2,4,6"),
                   "94g: set_formula on the auto worksheet: " << colText(ta, 1, 3).toStdString());
        tb->setAutoRecalcFormulas(false);
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "T94RB" }, { "col", "b" },
                             { "formula", "tablecol(\"T94RA\",\"x\")*10" } });
        CHECK_COND(res(r).value("ok").toBool() && colText(tb, 0, 3) == QLatin1String("10,20,30"),
                   "94g: set_formula(recalculate) in manual mode did not evaluate the column: "
                           << colText(tb, 0, 3).toStdString());
        for (int row = 0; row < 3; ++row)
            ta->setCell(row, 0, double(row + 5));
        CHECK_COND(colText(tb, 0, 3) == QLatin1String("10,20,30"),
                   "94g: manual worksheet followed a change of the other one");
        r = rpc(QStringLiteral("table.recalculate"), QJsonObject{ { "table", "T94RA" } });
        CHECK_COND(!isErr(r) && res(r).value("ok").toBool()
                           && colText(ta, 1, 3) == QLatin1String("10,12,14"),
                   "94g: table.recalculate(T94RA) did not evaluate its formula: "
                           << colText(ta, 1, 3).toStdString());
        CHECK_COND(colText(tb, 0, 3) == QLatin1String("10,20,30") && !tb->autoRecalcFormulas(),
                   "94g: table.recalculate(T94RA) recalculated the manual worksheet T94RB: "
                           << colText(tb, 0, 3).toStdString());

        // col = X evaluates X only; set_formula does not recalculate the dependent columns of a
        // manual worksheet; without col the whole worksheet is evaluated in dependency order
        Table *tc = w94->newTable(3, 4, QStringLiteral("T94RC"), QStringLiteral("RC94"));
        CHECK_COND(tc != nullptr, "94g: table RC");
        tc->setAutoRecalcFormulas(false);
        const char *names[] = { "x", "f", "g", "h" };
        for (int c = 0; c < 4; ++c)
            tc->setColName(c, QString::fromLatin1(names[c]));
        for (int row = 0; row < 3; ++row)
            tc->setCell(row, 0, double(row + 1));
        const QList<QPair<QString, QString>> formulas{ { "f", "col(\"x\")*2" },
                                                       { "g", "col(\"x\")+100" },
                                                       { "h", "col(\"f\")+1" } };
        for (const auto &fx : formulas) {
            r = rpc(QStringLiteral("table.set_formula"),
                    QJsonObject{ { "table", "T94RC" }, { "col", fx.first }, { "formula", fx.second } });
            CHECK_COND(res(r).value("ok").toBool(), "94g: set_formula " << fx.first.toStdString());
        }
        CHECK_COND(colText(tc, 1, 3) == QLatin1String("2,4,6")
                           && colText(tc, 2, 3) == QLatin1String("101,102,103")
                           && colText(tc, 3, 3) == QLatin1String("3,5,7"),
                   "94g: initial formulas of T94RC: " << colText(tc, 1, 3).toStdString() << " | "
                                                     << colText(tc, 2, 3).toStdString() << " | "
                                                     << colText(tc, 3, 3).toStdString());
        for (int row = 0; row < 3; ++row)
            tc->setCell(row, 0, double(10 * (row + 1)));
        r = rpc(QStringLiteral("table.recalculate"),
                QJsonObject{ { "table", "T94RC" }, { "col", "f" } });
        CHECK_COND(!isErr(r) && res(r).value("ok").toBool()
                           && colText(tc, 1, 3) == QLatin1String("20,40,60"),
                   "94g: table.recalculate(col=f) did not evaluate f: " << colText(tc, 1, 3).toStdString());
        CHECK_COND(colText(tc, 2, 3) == QLatin1String("101,102,103")
                           && colText(tc, 3, 3) == QLatin1String("3,5,7"),
                   "94g: table.recalculate(col=f) also recalculated g/h: "
                           << colText(tc, 2, 3).toStdString() << " | " << colText(tc, 3, 3).toStdString());
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "T94RC" }, { "col", "f" }, { "formula", "col(\"x\")*3" } });
        CHECK_COND(res(r).value("ok").toBool() && colText(tc, 1, 3) == QLatin1String("30,60,90"),
                   "94g: set_formula(f) in manual mode: " << colText(tc, 1, 3).toStdString());
        CHECK_COND(colText(tc, 2, 3) == QLatin1String("101,102,103")
                           && colText(tc, 3, 3) == QLatin1String("3,5,7"),
                   "94g: set_formula(f) in manual mode recalculated other columns: "
                           << colText(tc, 2, 3).toStdString() << " | " << colText(tc, 3, 3).toStdString());
        CHECK_COND(!tc->autoRecalcFormulas(), "94g: T94RC switched to automatic recalculation");
        r = rpc(QStringLiteral("table.recalculate"), QJsonObject{ { "table", "T94RC" } });
        CHECK_COND(!isErr(r) && res(r).value("ok").toBool()
                           && colText(tc, 1, 3) == QLatin1String("30,60,90")
                           && colText(tc, 2, 3) == QLatin1String("110,120,130")
                           && colText(tc, 3, 3) == QLatin1String("31,61,91"),
                   "94g: table.recalculate(T94RC) without col: " << colText(tc, 2, 3).toStdString()
                                                               << " | " << colText(tc, 3, 3).toStdString());

        // a cycle through a manual worksheet is still found by table.recalculate
        Table *ya = w94->newTable(2, 1, QStringLiteral("T94YA"), QStringLiteral("YA94"));
        Table *yb = w94->newTable(2, 1, QStringLiteral("T94YB"), QStringLiteral("YB94"));
        CHECK_COND(ya && yb, "94g: tables YA/YB");
        ya->setColName(0, QStringLiteral("a"));
        yb->setColName(0, QStringLiteral("b"));
        yb->setAutoRecalcFormulas(false);
        rpc(QStringLiteral("table.set_formula"),
            QJsonObject{ { "table", "T94YA" }, { "col", "a" },
                         { "formula", "tablecol(\"T94YB\",\"b\")+1" } });
        const QJsonObject rb = res(rpc(QStringLiteral("table.set_formula"),
                                       QJsonObject{ { "table", "T94YB" }, { "col", "b" },
                                                    { "formula", "tablecol(\"T94YA\",\"a\")+1" } }));
        CHECK_COND(!rb.value("ok").toBool() && !rb.value("formula_error").toString().isEmpty(),
                   "94g: set_formula closing a cycle through a manual worksheet reported ok");
        const QStringList ya0 = tableState(ya), yb0 = tableState(yb);
        const QJsonObject ry = res(rpc(QStringLiteral("table.recalculate"),
                                       QJsonObject{ { "table", "T94YA" } }));
        CHECK_COND(!ry.value("ok").toBool() && !ry.value("formula_errors").toArray().isEmpty()
                           && tableState(ya) == ya0 && tableState(yb) == yb0,
                   "94g: table.recalculate missed the cycle through the manual worksheet ("
                           << ya->text(0, 0).toStdString() << ", " << yb->text(0, 0).toStdString()
                           << ")");
    }

    // -------------------------------------------------------------------------
    // 94h: set_cell with a number on date/time, month and day columns converts the number
    //      (a day count / month / weekday), like set_column_data(mode=...) with numbers
    // -------------------------------------------------------------------------
    {
        Table *t = w94->newTable(2, 1, QStringLiteral("T94SN"), QStringLiteral("SN94"));
        CHECK_COND(t != nullptr, "94h: newTable failed");
        QJsonObject r = rpc(QStringLiteral("table.add_column"),
                            QJsonObject{ { "table", "T94SN" }, { "name", "When" }, { "mode", "datetime" },
                                         { "data", QJsonArray{ "2024-03-01 10:00:00",
                                                               "2024-03-02 10:00:00" } } });
        CHECK_COND(!isErr(r), "94h: add_column(datetime) failed");
        const int cWhen = t->numCols() - 1;
        r = rpc(QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T94SN" }, { "name", "Ref" }, { "mode", "datetime" },
                             { "data", QJsonArray{ 3.0, 2460000.5 } } });
        CHECK_COND(!isErr(r), "94h: add_column(numbers, datetime) failed");
        const int cRef = t->numCols() - 1;
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T94SN" }, { "row", 0 }, { "col", "When" }, { "value", 3 } });
        CHECK_COND(!isErr(r) && colMode(t, cWhen) == SciDAVis::ColumnMode::DateTime,
                   "94h: set_cell(3) changed the type of the date/time column");
        const QDateTime got0 = t->column(cWhen)->dateTimeAt(0);
        CHECK_COND(!t->column(cWhen)->isInvalid(0) && got0 == t->column(cRef)->dateTimeAt(0)
                           && got0 != QDateTime(QDate(1900, 1, 1), QTime(3, 0, 0)),
                   "94h: set_cell(3) on a date/time column is not the day count 3: "
                           << got0.toString(Qt::ISODate).toStdString() << " vs "
                           << t->column(cRef)->dateTimeAt(0).toString(Qt::ISODate).toStdString());
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T94SN" }, { "row", 1 }, { "col", "When" },
                             { "value", 2460000.5 } });
        CHECK_COND(!isErr(r) && !t->column(cWhen)->isInvalid(1)
                           && t->column(cWhen)->dateTimeAt(1) == t->column(cRef)->dateTimeAt(1),
                   "94h: set_cell(2460000.5) on a date/time column: "
                           << t->column(cWhen)->dateTimeAt(1).toString(Qt::ISODate).toStdString()
                           << " vs "
                           << t->column(cRef)->dateTimeAt(1).toString(Qt::ISODate).toStdString());
        // a string still goes through the input filter
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T94SN" }, { "row", 1 }, { "col", "When" },
                             { "value", "2025-01-02 03:04:05" } });
        CHECK_COND(!isErr(r) && t->column(cWhen)->dateTimeAt(1)
                                   == QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5)),
                   "94h: set_cell(string) on a date/time column");

        r = rpc(QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T94SN" }, { "name", "Mo" }, { "mode", "month" },
                             { "data", QJsonArray{ 1.0, 2.0 } } });
        const int cMo = t->numCols() - 1;
        CHECK_COND(!isErr(r) && colMode(t, cMo) == SciDAVis::ColumnMode::Month,
                   "94h: add_column(month) failed");
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T94SN" }, { "row", 0 }, { "col", "Mo" }, { "value", 5 } });
        CHECK_COND(!isErr(r) && colMode(t, cMo) == SciDAVis::ColumnMode::Month
                           && !t->column(cMo)->isInvalid(0) && t->column(cMo)->dateAt(0).month() == 5,
                   "94h: set_cell(5) on a month column is not May: "
                           << t->column(cMo)->dateTimeAt(0).toString(Qt::ISODate).toStdString());

        r = rpc(QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T94SN" }, { "name", "Dw" }, { "mode", "day" },
                             { "data", QJsonArray{ 1.0, 2.0 } } });
        const int cDw = t->numCols() - 1;
        CHECK_COND(!isErr(r) && colMode(t, cDw) == SciDAVis::ColumnMode::Day,
                   "94h: add_column(day) failed");
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T94SN" }, { "row", 1 }, { "col", "Dw" }, { "value", 3 } });
        CHECK_COND(!isErr(r) && colMode(t, cDw) == SciDAVis::ColumnMode::Day
                           && !t->column(cDw)->isInvalid(1)
                           && t->column(cDw)->dateAt(1).dayOfWeek() == 3,
                   "94h: set_cell(3) on a day-of-week column is not Wednesday: "
                           << t->column(cDw)->dateTimeAt(1).toString(Qt::ISODate).toStdString());
    }

    // -------------------------------------------------------------------------
    // 94i: an atomic rollback that fails before the workspace is cleared (the snapshot cannot be
    //      staged in the temp folder) leaves the windows -- and their Undo history -- in place,
    //      so it must not report undo_history_cleared
    // -------------------------------------------------------------------------
    {
        Table *t = w94->table(QStringLiteral("T94N"));
        CHECK_COND(t != nullptr, "94i: T94N missing");
        const QByteArray prevTmp = qgetenv("TMP");
        const QByteArray prevTemp = qgetenv("TEMP");
        const QByteArray prevTmpDir = qgetenv("TMPDIR"); // QDir::tempPath() on Unix
        const QString badTemp = QDir(QDir::tempPath())
                                        .filePath(QStringLiteral("tg94_missing_dir/sub"));
        qputenv("TMP", QDir::toNativeSeparators(badTemp).toLocal8Bit());
        qputenv("TEMP", QDir::toNativeSeparators(badTemp).toLocal8Bit());
        qputenv("TMPDIR", QDir::toNativeSeparators(badTemp).toLocal8Bit());
        const bool tempRedirected = QDir::cleanPath(QDir::tempPath()) == QDir::cleanPath(badTemp);
        const QJsonArray cmds{
            QJsonObject{ { "method", "table.set_cell" },
                         { "params", QJsonObject{ { "table", "T94N" }, { "row", 0 }, { "col", 0 },
                                                  { "value", 777.0 } } } },
            QJsonObject{ { "method", "table.set_cell" },
                         { "params", QJsonObject{ { "table", "NoSuchTable94" }, { "row", 0 },
                                                  { "col", 0 }, { "value", 1.0 } } } },
        };
        const QJsonObject r = res(rpc(QStringLiteral("app.batch"),
                                      QJsonObject{ { "commands", cmds }, { "atomic", true } }));
        auto restoreEnv = [](const char *name, const QByteArray &prev) {
            if (prev.isNull())
                qunsetenv(name);
            else
                qputenv(name, prev);
        };
        restoreEnv("TMP", prevTmp);
        restoreEnv("TEMP", prevTemp);
        restoreEnv("TMPDIR", prevTmpDir);
#ifndef Q_OS_WIN
        // Unix Qt resolves TMPDIR with canonicalPath(), so a missing folder cannot be made the
        // temp folder there: the failure path this check needs cannot be produced.
        if (!tempRedirected) {
            std::cout << "[INFO] 94i skipped: TMPDIR cannot point at a missing folder on this platform\n";
        } else
#endif
        {
        CHECK_COND(tempRedirected, "94i: could not point the temp folder at a missing directory ("
                                           << QDir::tempPath().toStdString() << ")");
        CHECK_COND(!r.value("rolled_back").toBool(true)
                           && !r.value("rollback_error").toString().isEmpty(),
                   "94i: rollback without a temp folder did not fail as expected");
        CHECK_COND(!r.value("undo_history_cleared").toBool(true),
                   "94i: failed rollback (workspace untouched) reported undo_history_cleared");
        CHECK_COND(w94->table(QStringLiteral("T94N")) == t && t->cell(0, 0) == 777.0,
                   "94i: the workspace was rebuilt although the rollback failed early");
        }
    }

    w94.reset();
    std::cout << "[PASS] Suite 94 passed.\n";
    return 0;
}
