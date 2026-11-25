#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <cstring>
#include <memory>

// =============================================================================
// Suite 105: column types of the JSON-RPC text import (app.import_csv) follow the GUI import
// (AsciiTableImportFilter::importDroppedTextData, File > Open / drag & drop). Every case
// imports the same file on both paths and checks that each column has the same name, the same
// type and the same values (numeric: the same invalid cells; text: the same strings).
//   105a missing / Excel error tokens ("N.D.", "ND", "#N/A", "#DIV/0!", "#VALUE!", quoted
//        "N.D.") are missing values of a numeric column
//   105b one non-numeric cell in a column of numbers ("OVER", a repeated "x y" header line
//        between two data blocks) is an invalid cell of a numeric column (90 % rule)
//   105c a column holding only "--" / "N/A" is an empty numeric column; only quoted
//        missing tokens ("\"N/A\"") make it a text column
//   105d label columns, columns with too many non-numeric cells, too few numbers and date /
//        time columns stay text
//   105e table.paste_tsv into existing columns keeps their type as table.set_cell does (text,
//        date/time, month and numeric columns; a non-number turns a numeric column into text
//        without losing its other rows); a one-line paste without tab is one cell; one Undo
//        restores the table; empty default columns still take the type of the pasted data
//   105f paste_tsv into a numeric column with data keeps it numeric when 90 % of the pasted
//        values are numbers (one "OVER" in 100 is an invalid cell); a column that becomes text
//        keeps its numbers exactly (round-trip digits, not the 6-digit display); set_cell of
//        "N.D." / "ND" / "#N/A" is a missing value of the numeric column as in the import
//   105g paste_tsv of a number into an existing date/time column is the day count from the
//        column's numeric base, the same cell as set_cell with that number or that text
//        ("45000" was 04:50:00 on 1900-01-01, "2" 02:00, "45000.5" and "1.5e4" empty cells);
//        digits written as a date (yyyyMMdd) or a clock time with a leading zero ("0930") and
//        other date/time texts keep the input filter's reading; a value that cannot be read is
//        an empty cell with a warning
// =============================================================================

namespace {
bool rpcIsError105(const QJsonObject &resp)
{
    return resp.contains(QStringLiteral("error"));
}
QJsonObject rpcResult105(const QJsonObject &resp)
{
    return resp.value(QStringLiteral("result")).toObject();
}
} // namespace

int runSuite105RpcImportColumnTypes(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 105: RPC import column types follow the GUI import...\n";

    struct NonInteractiveQuitGuard105
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard105()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard105()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard105;

    struct AppWinGuard105
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard105(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard105() { reset(); }
        AppWinGuard105(const AppWinGuard105 &) = delete;
        AppWinGuard105 &operator=(const AppWinGuard105 &) = delete;
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

    AppWinGuard105 w105(new ApplicationWindow());
    w105->applyUserSettings();
    w105->confirmCloseTable = false;
    w105->confirmCloseMatrix = false;
    w105->confirmClosePlot2D = false;
    w105->confirmClosePlot3D = false;
    w105->confirmCloseFolder = false;
    w105->resize(1280, 800);
    w105->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w105->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "105: pythonAutomationServer null");

    QTemporaryDir tmp105;
    CHECK_COND(tmp105.isValid(), "105: temporary directory");

    int rpcId = 10500;
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
    auto writeFile = [&](const QString &name, const QByteArray &bytes) -> QString {
        const QString path = tmp105.filePath(name);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(bytes);
            f.close();
        }
        return path;
    };
    auto readFile = [](const QString &path) -> QByteArray {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };

    // One imported column, the same shape for both paths: NaN = empty / invalid numeric cell.
    struct Col105
    {
        QString name;
        bool numeric = false;
        QVector<double> nums;
        QStringList texts;
    };
    const double NaN = std::numeric_limits<double>::quiet_NaN();

    auto rpcImport = [&](const QString &path) {
        QVector<Col105> out;
        const QJsonObject r =
                rpc(QStringLiteral("app.import_csv"), QJsonObject { { "path", path } });
        if (rpcIsError105(r))
            return out;
        const QString tbl = rpcResult105(r).value(QStringLiteral("name")).toString();
        const QJsonArray cols = rpcResult105(rpc(QStringLiteral("table.get_info"),
                                                 QJsonObject { { "table", tbl } }))
                                        .value(QStringLiteral("columns"))
                                        .toArray();
        for (const QJsonValue &cv : cols) {
            const QJsonObject c = cv.toObject();
            Col105 col;
            col.name = c.value(QStringLiteral("name")).toString();
            col.numeric = c.value(QStringLiteral("mode")).toString().compare(
                                  QStringLiteral("numeric"), Qt::CaseInsensitive)
                    == 0;
            const QJsonArray data = rpcResult105(rpc(QStringLiteral("table.get_column_data"),
                                                     QJsonObject { { "table", tbl },
                                                                   { "col", col.name } }))
                                            .value(QStringLiteral("data"))
                                            .toArray();
            for (const QJsonValue &v : data) {
                if (col.numeric)
                    col.nums << (v.isDouble() ? v.toDouble() : NaN);
                else
                    col.texts << (v.isNull() ? QString() : v.toString());
            }
            out << col;
        }
        return out;
    };
    auto guiImport = [&](const QString &path) {
        QVector<Col105> out;
        std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                readFile(path), QStringLiteral("Gui105"), path));
        if (!ft)
            return out;
        for (int c = 0; c < ft->columnCount(); ++c) {
            Column *src = ft->column(c);
            Col105 col;
            col.name = src->name();
            col.numeric = src->columnMode() == SciDAVis::ColumnMode::Numeric;
            for (int r = 0; r < src->rowCount(); ++r) {
                if (col.numeric)
                    col.nums << (src->isInvalid(r) ? NaN : src->valueAt(r));
                else
                    col.texts << src->textAt(r);
            }
            out << col;
        }
        return out;
    };
    auto dumpCols = [](const QVector<Col105> &cols) {
        QStringList s;
        for (const Col105 &c : cols) {
            QStringList v;
            if (c.numeric) {
                for (double d : c.nums)
                    v << (std::isnan(d) ? QStringLiteral("NaN") : QString::number(d));
            } else {
                v = c.texts;
            }
            s << QStringLiteral("%1[%2]{%3}")
                            .arg(c.name, c.numeric ? QStringLiteral("num") : QStringLiteral("txt"),
                                 v.join(QLatin1Char(',')));
        }
        return s.join(QStringLiteral(" ")).toStdString();
    };
    // trailing empty cells / NaN do not count (the RPC table may keep its default row count)
    auto sameColumn = [](const Col105 &a, const Col105 &b) {
        if (a.name != b.name || a.numeric != b.numeric)
            return false;
        if (a.numeric) {
            QVector<double> x = a.nums, y = b.nums;
            while (!x.isEmpty() && std::isnan(x.last()))
                x.removeLast();
            while (!y.isEmpty() && std::isnan(y.last()))
                y.removeLast();
            if (x.size() != y.size())
                return false;
            for (int i = 0; i < x.size(); ++i) {
                if (std::isnan(x[i]) != std::isnan(y[i]))
                    return false;
                if (!std::isnan(x[i]) && std::fabs(x[i] - y[i]) > 1e-12)
                    return false;
            }
            return true;
        }
        QStringList x = a.texts, y = b.texts;
        while (!x.isEmpty() && x.last().isEmpty())
            x.removeLast();
        while (!y.isEmpty() && y.last().isEmpty())
            y.removeLast();
        return x == y;
    };
    // GUI and RPC agree, and the column types are the expected ones ('n' numeric, 't' text)
    auto checkSame = [&](const char *tag, const QString &path, const char *types) -> bool {
        const QVector<Col105> gui = guiImport(path);
        const QVector<Col105> rpcCols = rpcImport(path);
        bool ok = !gui.isEmpty() && gui.size() == rpcCols.size()
                && gui.size() == int(std::strlen(types));
        for (int i = 0; ok && i < gui.size(); ++i)
            ok = sameColumn(gui[i], rpcCols[i]) && gui[i].numeric == (types[i] == 'n');
        if (!ok) {
            std::cerr << "[FAIL] 105" << tag << " expected types " << types
                      << "\n  GUI: " << dumpCols(gui) << "\n  RPC: " << dumpCols(rpcCols) << "\n";
        }
        return ok;
    };

    // every case runs, so that one report lists all the disagreements
    int failures105 = 0;
#define EXPECT105(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl; \
            ++failures105; \
        } \
    } while (0)

    // -------------------------------------------------------------------------
    // 105a: missing / Excel error tokens
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("a1", writeFile(QStringLiteral("r_tok00_tab.txt"),
                                         "x\ty\n1\t2\n2\tN.D.\n3\t4\n4\t5\n"),
                         "nn"),
               "105a1: 'N.D.' is a missing value on both paths");
    EXPECT105(checkSame("a2", writeFile(QStringLiteral("r_tok02_3col.csv"),
                                         "x,y,z\n1,2,3\n2,#N/A,4\n3,4,5\n4,5,6\n"),
                         "nnn"),
               "105a2: '#N/A' is a missing value on both paths");
    EXPECT105(checkSame("a3", writeFile(QStringLiteral("tokens.csv"),
                                         "x,a,b,c,d\n1,ND,1.5,#DIV/0!,2\n2,2,#VALUE!,7,n.d.\n"
                                         "3,3,2.5,8,\"N.D.\"\n4,4,3.5,#NUM!,5\n"),
                         "nnnnn"),
               "105a3: 'ND', '#DIV/0!', '#VALUE!', '#NUM!', 'n.d.', quoted 'N.D.' are missing");

    // -------------------------------------------------------------------------
    // 105b: one stray non-numeric cell in a numeric column (90 % rule)
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("b1", writeFile(QStringLiteral("stray.csv"),
                                         "x,y\n1,10\n2,11\n3,OVER\n4,13\n5,14\n6,15\n7,16\n8,17\n"
                                         "9,18\n10,19\n"),
                         "nn"),
               "105b1: one 'OVER' among nine numbers is an invalid cell of a numeric column");
    EXPECT105(checkSame("b2", writeFile(QStringLiteral("s_blocks6.dat"),
                                         "x y\n0 10\n1 11\n2 12\n3 13\n4 14\n5 15\n\nx y\n0 20\n"
                                         "1 21\n2 22\n3 23\n4 24\n5 25\n"),
                         "nn"),
               "105b2: a repeated 'x y' header line between two blocks is an invalid row");

    // -------------------------------------------------------------------------
    // 105c: columns of missing tokens only
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("c1", writeFile(QStringLiteral("missing_all.csv"),
                                         "x,y\n1,--\n2,--\n3,--\n"),
                         "nn"),
               "105c1: a column of '--' only is an empty numeric column");
    EXPECT105(checkSame("c2", writeFile(QStringLiteral("na_all.csv"),
                                         "x,y\n1,N/A\n2,N.D.\n3,#N/A\n"),
                         "nn"),
               "105c2: a column of missing tokens only is an empty numeric column");
    EXPECT105(checkSame("c3", writeFile(QStringLiteral("quoted_na.csv"),
                                         "x,y\n1,\"N/A\"\n2,\"N/A\"\n3,\"N/A\"\n"),
                         "nt"),
               "105c3: a column of quoted 'N/A' only stays text");

    // -------------------------------------------------------------------------
    // 105d: columns that stay text
    // -------------------------------------------------------------------------
    EXPECT105(checkSame("d1", writeFile(QStringLiteral("labels.csv"),
                                         "Sample,Value\nA1,1\nB2,2\nC3,3\nD4,4\nE5,5\nF6,6\n"),
                         "tn"),
               "105d1: a label column stays text");
    EXPECT105(checkSame("d2", writeFile(QStringLiteral("mixed.csv"),
                                         "x,y\n1,10\n2,abc\n3,12\n4,def\n5,14\n6,15\n7,16\n8,17\n"
                                         "9,18\n10,19\n"),
                         "nt"),
               "105d2: two text cells among eight numbers keep the column text");
    EXPECT105(checkSame("d3", writeFile(QStringLiteral("few.csv"),
                                         "x,y\n1,10\n2,OVER\n3,12\n4,13\n"),
                         "nt"),
               "105d3: one text cell with fewer than five numbers keeps the column text");
    EXPECT105(checkSame("d4", writeFile(QStringLiteral("dates.csv"),
                                         "Date,Time,Value\n2026-01-01,12:00:00,1\n"
                                         "2026-01-02,12:30:00,2\n2026-01-03,13:00:00,3\n"
                                         "2026-01-04,13:30:00,4\n2026-01-05,14:00:00,5\n"
                                         "2026-01-06,14:30:00,6\n"),
                         "ttn"),
               "105d4: date and time columns keep the type of the GUI import");
    EXPECT105(checkSame("d5", writeFile(QStringLiteral("label_one_num.csv"),
                                         "Name,Value\nalpha,1\nbeta,2\n42,3\ngamma,4\ndelta,5\n"
                                         "eps,6\n"),
                         "tn"),
               "105d5: a label column with one number stays text");

    // -------------------------------------------------------------------------
    // 105e: paste_tsv into existing columns keeps their type (as set_cell does)
    // -------------------------------------------------------------------------
    {
        auto tableState = [](Table *t) -> QStringList {
            QStringList st;
            st << QString::number(t->numRows()) << QString::number(t->numCols());
            for (int c = 0; c < t->numCols(); ++c) {
                Column *col = t->column(c);
                st << t->colLabel(c) << QString::number(int(col->columnMode()));
                for (int r = 0; r < t->numRows(); ++r)
                    st << (col->isInvalid(r) ? QStringLiteral("<inv>") : t->text(r, c));
            }
            return st;
        };
        auto undoStackOf = [](Table *t) -> QUndoStack * {
            return (t && t->d_future_table) ? t->d_future_table->undoStack() : nullptr;
        };
        // text Al..Cu, date/time, month, numeric: the worksheet of the report
        auto makeFixture = [&](const QString &name) -> Table * {
            Table *t = w105->newTable(4, 4, name, name);
            if (!t)
                return nullptr;
            rpc(QStringLiteral("table.set_column_data"),
                QJsonObject { { "table", name }, { "col", 0 }, { "mode", "text" },
                              { "data", QJsonArray { "Al", "Zn", "Mg", "Cu" } } });
            rpc(QStringLiteral("table.set_column_data"),
                QJsonObject { { "table", name },
                              { "col", 1 },
                              { "mode", "datetime" },
                              { "data", QJsonArray { "2024-01-01 10:00", "2024-01-02 11:00",
                                                     "2024-01-03 12:00", "2024-01-04 13:00" } } });
            rpc(QStringLiteral("table.set_column_data"),
                QJsonObject { { "table", name }, { "col", 2 },
                              { "data", QJsonArray { 1, 2, 3, 4 } } });
            rpc(QStringLiteral("table.set_column_meta"),
                QJsonObject { { "table", name }, { "col", 2 }, { "mode", "month" } });
            rpc(QStringLiteral("table.set_column_data"),
                QJsonObject { { "table", name }, { "col", 3 },
                              { "data", QJsonArray { 1.5, 2.5, 3.5, 4.5 } } });
            return t;
        };
        // paste_tsv on one fixture, set_cell of the same strings on a twin: same table, one
        // Undo step that restores the table
        int caseNo = 0;
        auto pasteLikeSetCell = [&](const char *tag, const QString &text, int startRow,
                                    int startCol, const QStringList &cells) -> bool {
            const QString nameP = QStringLiteral("P105_%1").arg(caseNo);
            const QString nameS = QStringLiteral("S105_%1").arg(caseNo++);
            Table *tp = makeFixture(nameP);
            Table *ts = makeFixture(nameS);
            QUndoStack *us = undoStackOf(tp);
            if (!tp || !ts || !us) {
                std::cerr << "[FAIL] 105" << tag << ": fixture\n";
                return false;
            }
            // the worksheets may share one undo stack: paste last, check its one step, Undo /
            // Redo it, then write the twin with set_cell
            us->clear(); // stay below the project's undo limit, so index() counts the steps
            const QStringList before = tableState(tp);
            const QJsonObject r = rpc(QStringLiteral("table.paste_tsv"),
                                      QJsonObject { { "table", nameP },
                                                    { "text", text },
                                                    { "header", false },
                                                    { "start_row", startRow },
                                                    { "start_col", startCol } });
            const QStringList pasted = tableState(tp);
            const int steps = us->index();
            us->undo();
            QCoreApplication::processEvents();
            const QStringList undone = tableState(tp);
            us->redo();
            QCoreApplication::processEvents();
            if (rpcIsError105(r) || steps != 1 || undone != before || tableState(tp) != pasted) {
                std::cerr << "[FAIL] 105" << tag << ": paste_tsv made " << steps
                          << " Undo step(s); after one Undo: "
                          << undone.join(QLatin1Char('|')).toStdString() << "\n";
                return false;
            }
            for (int k = 0; k < cells.size(); ++k) {
                rpc(QStringLiteral("table.set_cell"),
                    QJsonObject { { "table", nameS },
                                  { "row", startRow },
                                  { "col", startCol + k },
                                  { "value", cells[k] } });
            }
            const QStringList viaSetCell = tableState(ts);
            if (pasted != viaSetCell) {
                std::cerr << "[FAIL] 105" << tag << ": paste_tsv differs from set_cell\n  paste   : "
                          << pasted.join(QLatin1Char('|')).toStdString()
                          << "\n  set_cell: " << viaSetCell.join(QLatin1Char('|')).toStdString()
                          << "\n";
                return false;
            }
            return true;
        };

        EXPECT105(pasteLikeSetCell("e1", QStringLiteral("12\t45000\t7\t9"), 3, 0,
                                   { "12", "45000", "7", "9" }),
                  "105e1: paste of one row keeps the text / date-time / month / numeric columns");
        EXPECT105(pasteLikeSetCell("e2", QStringLiteral("x"), 3, 3, { "x" }),
                  "105e2: a non-number turns the numeric column into text, its rows kept");
        EXPECT105(pasteLikeSetCell("e3", QStringLiteral("2025-05-06 07:08"), 1, 1,
                                   { "2025-05-06 07:08" }),
                  "105e3: a date/time pasted without tab is one cell of the date/time column");
        EXPECT105(pasteLikeSetCell("e4", QStringLiteral("2025-05-06 07:08:09\tMay"), 0, 1,
                                   { "2025-05-06 07:08:09", "May" }),
                  "105e4: date/time and month texts go through the columns' input filters");

        // the rows that were not pasted over keep their data, the types do not change
        {
            Table *t = makeFixture(QStringLiteral("K105"));
            rpc(QStringLiteral("table.paste_tsv"),
                QJsonObject { { "table", "K105" }, { "text", "12\t45000\t7\t9" },
                              { "header", false }, { "start_row", 3 }, { "start_col", 0 } });
            const bool ok = t && t->column(0)->columnMode() == SciDAVis::ColumnMode::Text
                    && t->column(1)->columnMode() == SciDAVis::ColumnMode::DateTime
                    && t->column(2)->columnMode() == SciDAVis::ColumnMode::Month
                    && t->column(3)->columnMode() == SciDAVis::ColumnMode::Numeric
                    && t->text(0, 0) == QStringLiteral("Al") && t->text(2, 0) == QStringLiteral("Mg")
                    && t->text(3, 0) == QStringLiteral("12")
                    && t->column(1)->dateTimeAt(0) == QDateTime(QDate(2024, 1, 1), QTime(10, 0))
                    && !t->column(2)->isInvalid(1) && t->column(3)->valueAt(0) == 1.5
                    && t->column(3)->valueAt(3) == 9.0 && t->numCols() == 4;
            EXPECT105(ok, "105e5: rows not pasted over keep their data and the column types");
        }
        // empty default columns and new columns still take the type of the pasted data
        {
            Table *t = w105->newTable(30, 2, QStringLiteral("N105"), QStringLiteral("N105"));
            const QJsonObject r =
                    rpc(QStringLiteral("table.paste_tsv"),
                        QJsonObject { { "table", "N105" },
                                      { "text", "Sample\tv\tw\nA\t1\t5\nB\t2\t6\nC\tN.D.\t7\n" },
                                      { "header", true } });
            const bool ok = !rpcIsError105(r) && t && t->numCols() == 3
                    && t->column(0)->columnMode() == SciDAVis::ColumnMode::Text
                    && t->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                    && t->column(1)->isInvalid(2)
                    && t->column(2)->columnMode() == SciDAVis::ColumnMode::Numeric
                    && t->text(1, 0) == QStringLiteral("B");
            EXPECT105(ok, "105e6: paste into empty default / new columns decides their type");
        }
    }

    // -------------------------------------------------------------------------
    // 105f: paste_tsv into a numeric column with data uses the 90 % rule; a column that does
    // become text keeps its numbers exactly; set_cell reads the import's missing tokens
    // -------------------------------------------------------------------------
    {
        // values that need all 17 digits (1/3, 2/3, ...): a 6-digit display would cut them
        auto exact = [](int r) { return (r + 1) / 3.0 + 1000.0; };
        auto makeNumeric = [&](const QString &name, int rows, int filled) -> Table * {
            Table *t = w105->newTable(rows, 1, name, name);
            if (!t)
                return nullptr;
            for (int r = 0; r < filled; ++r)
                t->setCell(r, 0, exact(r));
            return t;
        };
        // 200 numbers, then 100 pasted values of which one is "OVER": still numeric
        {
            Table *t = makeNumeric(QStringLiteral("F105a"), 300, 200);
            QStringList lines;
            for (int k = 0; k < 100; ++k)
                lines << ((k == 37) ? QStringLiteral("OVER") : QString::number(5000 + k));
            const QJsonObject r = rpc(QStringLiteral("table.paste_tsv"),
                                      QJsonObject { { "table", "F105a" },
                                                    { "text", lines.join(QLatin1Char('\n')) },
                                                    { "header", false },
                                                    { "start_row", 200 },
                                                    { "start_col", 0 } });
            Column *c = t ? t->column(0) : nullptr;
            bool kept = c && !rpcIsError105(r)
                    && c->columnMode() == SciDAVis::ColumnMode::Numeric;
            for (int row = 0; kept && row < 200; ++row)
                kept = !c->isInvalid(row) && c->valueAt(row) == exact(row);
            for (int k = 0; kept && k < 100; ++k)
                kept = (k == 37) ? c->isInvalid(200 + k)
                                 : (!c->isInvalid(200 + k) && c->valueAt(200 + k) == 5000 + k);
            std::cout << "    105f1 mode " << (c ? int(c->columnMode()) : -1) << ", row 0 '"
                      << (t ? t->text(0, 0).toStdString() : std::string()) << "'\n";
            EXPECT105(kept, "105f1: one unreadable value in 100 pasted keeps the numeric column "
                            "(the cell is invalid, the other rows keep their values)");
        }
        // too many unreadable values: the column becomes text, its numbers written exactly
        {
            Table *t = makeNumeric(QStringLiteral("F105b"), 20, 10);
            const QJsonObject r = rpc(QStringLiteral("table.paste_tsv"),
                                      QJsonObject { { "table", "F105b" },
                                                    { "text", "1\nx\ny\n4\nz" },
                                                    { "header", false },
                                                    { "start_row", 10 },
                                                    { "start_col", 0 } });
            Column *c = t ? t->column(0) : nullptr;
            bool ok = c && !rpcIsError105(r) && c->columnMode() == SciDAVis::ColumnMode::Text;
            for (int row = 0; ok && row < 10; ++row) {
                bool conv = false;
                const double v = c->textAt(row).toDouble(&conv);
                ok = conv && v == exact(row);
            }
            ok = ok && c->textAt(11) == QStringLiteral("x") && c->textAt(13) == QStringLiteral("4");
            std::cout << "    105f2 mode " << (c ? int(c->columnMode()) : -1) << ", row 0 '"
                      << (c ? c->textAt(0).toStdString() : std::string()) << "'\n";
            EXPECT105(ok, "105f2: a numeric column turned into text keeps its numbers exactly");
        }
        // set_cell with a missing token of the import ("N.D.", "ND", "#N/A") into a numeric
        // column: an invalid cell, the column stays numeric (as paste_tsv / the import read it)
        {
            Table *t = makeNumeric(QStringLiteral("F105c"), 10, 10);
            bool ok = t != nullptr;
            int row = 2;
            for (const char *tok : { "N.D.", "ND", "#N/A" }) {
                const QJsonObject r =
                        rpc(QStringLiteral("table.set_cell"),
                            QJsonObject { { "table", "F105c" }, { "row", row }, { "col", 0 },
                                          { "value", QString::fromLatin1(tok) } });
                Column *c = t ? t->column(0) : nullptr;
                const bool okTok = c && !rpcIsError105(r)
                        && c->columnMode() == SciDAVis::ColumnMode::Numeric && c->isInvalid(row)
                        && c->valueAt(0) == exact(0);
                if (!okTok)
                    std::cerr << "    105f3 set_cell '" << tok << "': mode "
                              << (c ? int(c->columnMode()) : -1) << "\n";
                ok = ok && okTok;
                ++row;
            }
            EXPECT105(ok, "105f3: set_cell of an import missing token is a missing value");
        }
    }

    // -------------------------------------------------------------------------
    // 105g: numbers pasted into an existing date/time column
    // -------------------------------------------------------------------------
    {
        const int R = 16;
        auto makeDateTimeTable = [&](const QString &name) -> Table * {
            Table *t = w105->newTable(R, 1, name, name);
            QJsonArray texts;
            for (int r = 0; r < R; ++r)
                texts.append(QStringLiteral("2024-01-%1 10:00").arg(r + 1, 2, 10, QLatin1Char('0')));
            rpc(QStringLiteral("table.set_column_data"),
                QJsonObject { { "table", name }, { "col", 0 }, { "mode", "datetime" },
                              { "data", texts } });
            return (t && t->column(0)->columnMode() == SciDAVis::ColumnMode::DateTime) ? t
                                                                                        : nullptr;
        };
        Table *tp = makeDateTimeTable(QStringLiteral("G105"));   // paste_tsv
        Table *tn = makeDateTimeTable(QStringLiteral("GN105"));  // set_cell with a number
        Table *ts = makeDateTimeTable(QStringLiteral("GS105"));  // set_cell with a text
        Table *tg = makeDateTimeTable(QStringLiteral("GG105"));  // typed into the cell (filter)
        EXPECT105(tp && tn && ts && tg, "105g: date/time fixtures");
        if (tp && tn && ts && tg) {
            auto cellText = [](Table *t, int row) {
                Column *c = t->column(0);
                return c->isInvalid(row) ? QStringLiteral("<empty>")
                                         : c->dateTimeAt(row).toString(Qt::ISODateWithMs);
            };
            auto paste = [&](int row, const QString &text) {
                return rpc(QStringLiteral("table.paste_tsv"),
                           QJsonObject { { "table", "G105" }, { "text", text },
                                         { "header", false }, { "separator", "\t" },
                                         { "start_row", row }, { "start_col", 0 } });
            };
            auto hasWarnings = [](const QJsonObject &resp) {
                return !rpcResult105(resp).value(QStringLiteral("warnings")).toArray().isEmpty();
            };
            // numbers: the day count, as set_cell of the number and of the text
            const QStringList numbers = { "45000", "45000.5", "2", "1.5e4", "0", "-1.25" };
            int row = 0;
            for (const QString &text : numbers) {
                const QJsonObject r = paste(row, text);
                rpc(QStringLiteral("table.set_cell"),
                    QJsonObject { { "table", "GN105" }, { "row", row }, { "col", 0 },
                                  { "value", text.toDouble() } });
                rpc(QStringLiteral("table.set_cell"),
                    QJsonObject { { "table", "GS105" }, { "row", row }, { "col", 0 },
                                  { "value", text } });
                const QString got = cellText(tp, row);
                EXPECT105(!rpcIsError105(r) && !hasWarnings(r) && got == cellText(tn, row)
                                  && cellText(ts, row) == got && got != QStringLiteral("<empty>"),
                          "105g: paste '" << text.toStdString() << "' -> " << got.toStdString()
                                          << ", set_cell(number) -> "
                                          << cellText(tn, row).toStdString()
                                          << ", set_cell(text) -> "
                                          << cellText(ts, row).toStdString());
                ++row;
            }
            EXPECT105(tp->column(0)->dateTimeAt(0).date() == QDate(2093, 3, 16)
                              && tp->column(0)->dateTimeAt(2) == QDateTime(QDate(1970, 1, 3), QTime(0, 0))
                              && tp->column(0)->dateTimeAt(1).time() == QTime(12, 0),
                      "105g: '45000' / '2' / '45000.5' are not days from 1970-01-01: "
                              << cellText(tp, 0).toStdString() << " " << cellText(tp, 2).toStdString()
                              << " " << cellText(tp, 1).toStdString());
            // dates and clock times written with digits, and other date/time texts: the input
            // filter's reading, as typed into the cell
            const QStringList filterTexts = { "20250506", "0930", "093000", "130000", "10:30",
                                              "2024-01-02 03:04", "2025-05-06T07:08:09" };
            for (const QString &text : filterTexts) {
                const QJsonObject r = paste(row, text);
                tg->setText(row, 0, text);
                const QString got = cellText(tp, row);
                std::cout << "[INFO] 105g: paste '" << text.toStdString() << "' -> "
                          << got.toStdString() << "\n";
                EXPECT105(!rpcIsError105(r) && got == cellText(tg, row),
                          "105g: paste '" << text.toStdString() << "' -> " << got.toStdString()
                                          << ", typed -> " << cellText(tg, row).toStdString());
                ++row;
            }
            EXPECT105(tp->column(0)->dateTimeAt(6) == QDateTime(QDate(2025, 5, 6), QTime(0, 0)),
                      "105g: '20250506' is not 2025-05-06: " << cellText(tp, 6).toStdString());
            // a value that cannot be read: an empty cell and a warning; a missing token: empty
            {
                const QJsonObject r = paste(row, QStringLiteral("abc12"));
                EXPECT105(!rpcIsError105(r) && tp->column(0)->isInvalid(row) && hasWarnings(r)
                                  && rpcResult105(r)
                                             .value(QStringLiteral("warnings"))
                                             .toArray()
                                             .at(0)
                                             .toString()
                                             .contains(QStringLiteral("abc12")),
                          "105g: 'abc12' gives no warning: " << cellText(tp, row).toStdString());
                ++row;
                const QJsonObject m = paste(row, QStringLiteral("N.D."));
                EXPECT105(!rpcIsError105(m) && tp->column(0)->isInvalid(row) && !hasWarnings(m),
                          "105g: 'N.D.' in a date/time column: " << cellText(tp, row).toStdString());
                ++row;
            }
            // several rows in one paste are one Undo step
            {
                QUndoStack *us = tp->d_future_table ? tp->d_future_table->undoStack() : nullptr;
                const QString before0 = cellText(tp, R - 2), before1 = cellText(tp, R - 1);
                us->clear();
                const QJsonObject r = paste(R - 2, QStringLiteral("45000\n2024-03-01 10:00"));
                const QString after0 = cellText(tp, R - 2), after1 = cellText(tp, R - 1);
                const bool after1Ok =
                        tp->column(0)->dateTimeAt(R - 1) == QDateTime(QDate(2024, 3, 1), QTime(10, 0));
                const int steps = us->index();
                us->undo();
                QCoreApplication::processEvents();
                EXPECT105(!rpcIsError105(r) && steps == 1 && after0 == cellText(tn, 0) && after1Ok
                                  && cellText(tp, R - 2) == before0 && cellText(tp, R - 1) == before1,
                          "105g: two-row paste " << after0.toStdString() << " / "
                                                 << after1.toStdString() << ", " << steps
                                                 << " Undo step(s), after Undo "
                                                 << cellText(tp, R - 2).toStdString());
            }
        }
    }

#undef EXPECT105
    if (failures105 > 0) {
        std::cerr << "[FAIL] Suite 105: " << failures105 << " case(s) failed\n";
        return 1;
    }
    std::cout << "[PASS] Suite 105: RPC import column types follow the GUI import\n";
    return 0;
}
