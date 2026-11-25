#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUndoStack>

// =============================================================================
// Suite 99: date/time text parsing and wide tables.
//   99a text -> date/time conversion (Column > Type and the RPC writes use the same input
//       filter): ISO 8601 with 'T' (Python datetime.isoformat()), fractions of a second up to
//       microseconds, 'Z' / +09:00 / -05:30 zones (the written clock time is kept), the basic
//       form 20240102T030405; impossible dates and times stay invalid; every format read before
//       (space-separated, date only, time only, German/European dates, yyyyMMdd, a short
//       fraction) gives exactly the result of the old parser.
//   99b the same through JSON-RPC: add_column(mode=datetime) and set_cell on a date/time
//       column.
//   99c month and day-of-week columns read English names (full and short, any case) when the
//       locale's own spelling does not match ("January", "Feb", "tue" on a Japanese PC);
//       numbers keep their meaning, unknown words stay invalid.
//   99d table.create with 2000 columns x 5 rows, app.import_csv of a 2000 x 5 CSV and the GUI
//       column count change 2 -> 2000 (with its Undo / Redo) finish well within limits; names,
//       designations, header labels and values are right. These were quadratic in the column
//       count (600 columns took 10 s to create; 2000 columns ran past the 120 s RPC timeout).
//   99e header labels stay right through the incremental update: several X columns
//       ([X1]..[Y2]), a rename, a designation change to a single X, an insert and a removal.
//   99f automatic recalculation still finds formula columns after the formula scan was
//       cached: a formula set after a write, a formula in another table, a removed column
//       before the formula column, and a cleared formula.
// =============================================================================

namespace {

// The parser before ISO 8601 support, for strings it already read.
QDateTime legacyDateTimeParse(const QString &input_value, const QString &d_format)
{
    static const QStringList date_formats = { "yyyy-M-d", "yyyy/M/d", "d/M/yyyy", "d/M/yy",
                                              "d-M-yyyy", "d-M-yy",   "d.M.yyyy", "d.M.yy",
                                              "M/yyyy",   "d.M.",     "yyyyMMdd" };
    static const QStringList time_formats = { "h",           "h ap",        "h:mm",
                                              "h:mm ap",     "h:mm:ss",     "h:mm:ss.zzz",
                                              "h:mm:ss:zzz", "mm:ss.zzz",   "hmmss" };
    QDateTime result = QDateTime::fromString(input_value, d_format);
    if (result.isValid())
        return result;
    QStringList strings = input_value.simplified().split(",", Qt::SkipEmptyParts);
    if (strings.size() == 1)
        strings = strings.at(0).split(" ", Qt::SkipEmptyParts);
    if (strings.size() < 1)
        return result;
    QDate date_result;
    QTime time_result;
    const QString date_string = strings.at(0).trimmed();
    const QString time_string = strings.size() > 1 ? strings.at(1).trimmed() : date_string;
    for (const auto &format : date_formats) {
        date_result = QDate::fromString(date_string, format);
        if (date_result.isValid())
            break;
    }
    for (const auto &format : time_formats) {
        time_result = QTime::fromString(time_string, format);
        if (time_result.isValid())
            break;
    }
    if (!date_result.isValid() && time_result.isValid())
        date_result.setDate(1900, 1, 1);
    return QDateTime(date_result, time_result);
}

QString dtText(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
                        : QStringLiteral("<invalid>");
}

} // namespace

int runSuite99DateTimeParsingAndWideTables(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 99: date/time parsing and wide tables...\n";

    struct NonInteractiveQuitGuard99
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard99()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard99()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard99;

    struct AppWinGuard99
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard99(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard99() { reset(); }
        AppWinGuard99(const AppWinGuard99 &) = delete;
        AppWinGuard99 &operator=(const AppWinGuard99 &) = delete;
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

    AppWinGuard99 w99(new ApplicationWindow());
    w99->applyUserSettings();
    w99->confirmCloseTable = false;
    w99->confirmCloseMatrix = false;
    w99->confirmClosePlot2D = false;
    w99->confirmClosePlot3D = false;
    w99->confirmCloseFolder = false;
    w99->resize(1280, 800);
    w99->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w99->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "99: pythonAutomationServer null");

    int rpcId = 9900;
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
        return resp.value(QStringLiteral("error")).toObject().value(QStringLiteral("message"))
                .toString()
                .toStdString();
    };
    // a text column with the given strings, converted to `mode` like Column > Type does
    auto convertTexts = [&](const QString &tableName, const QStringList &texts,
                            SciDAVis::ColumnMode mode) -> Column * {
        Table *t = w99->newTable(texts.size(), 1, tableName, tableName);
        if (!t)
            return nullptr;
        Column *col = t->column(0);
        col->setColumnMode(SciDAVis::ColumnMode::Text);
        col->replaceTexts(0, texts);
        col->setColumnMode(mode);
        QCoreApplication::processEvents();
        return col->columnMode() == mode ? col : nullptr;
    };

    // -------------------------------------------------------------------------
    // 99a: ISO 8601 and the old formats through the text -> date/time conversion
    // -------------------------------------------------------------------------
    {
        struct IsoCase
        {
            const char *text;
            QDateTime expected; // invalid: must stay invalid
        };
        const QDate d1231(2023, 12, 31);
        const QDate d0102(2024, 1, 2);
        const std::vector<IsoCase> isoCases = {
            { "2023-12-31T23:59:59", QDateTime(d1231, QTime(23, 59, 59)) },
            { "2023-12-31T23:59", QDateTime(d1231, QTime(23, 59, 0)) },
            { "2023-12-31t23:59:59", QDateTime(d1231, QTime(23, 59, 59)) },
            { "2023-12-31T23:59:59.123456", QDateTime(d1231, QTime(23, 59, 59, 123)) },
            { "2023-12-31T23:59:59.5", QDateTime(d1231, QTime(23, 59, 59, 500)) },
            { "2023-12-31T23:59:59,25", QDateTime(d1231, QTime(23, 59, 59, 250)) },
            { "2024-01-02T03:04:05Z", QDateTime(d0102, QTime(3, 4, 5)) },
            { "2024-01-02T03:04:05+09:00", QDateTime(d0102, QTime(3, 4, 5)) },
            { "2024-01-02T03:04:05.250-05:30", QDateTime(d0102, QTime(3, 4, 5, 250)) },
            { "2024-01-02T03:04:05+0900", QDateTime(d0102, QTime(3, 4, 5)) },
            { "2024-01-02 03:04:05+09:00", QDateTime(d0102, QTime(3, 4, 5)) },
            { "2024-01-02 03:04:05.123456", QDateTime(d0102, QTime(3, 4, 5, 123)) },
            { "20240102T030405", QDateTime(d0102, QTime(3, 4, 5)) },
            { "20240102T030405Z", QDateTime(d0102, QTime(3, 4, 5)) },
            { "  2024-01-02T03:04:05  ", QDateTime(d0102, QTime(3, 4, 5)) },
            { "12:30:15.250000", QDateTime(QDate(1900, 1, 1), QTime(12, 30, 15, 250)) },
            { "2023-02-30T00:00:00", QDateTime() },
            { "2023-12-31T24:30:00", QDateTime() },
            { "2023-12-31T23:60:00", QDateTime() },
            { "2023-12-31T23:59:59+25:00", QDateTime() },
            { "2023-12-31Tnoon", QDateTime() },
        };
        QStringList texts;
        for (const IsoCase &c : isoCases)
            texts << QString::fromLatin1(c.text);
        Column *col = convertTexts(QStringLiteral("T99Iso"), texts, SciDAVis::ColumnMode::DateTime);
        CHECK_COND(col != nullptr, "99a: text -> date/time conversion failed");
        for (int i = 0; i < int(isoCases.size()); ++i) {
            const QDateTime &exp = isoCases[i].expected;
            const bool gotValid = !col->isInvalid(i) && col->dateTimeAt(i).isValid();
            if (!exp.isValid()) {
                CHECK_COND(!gotValid, "99a: '" << isoCases[i].text
                                              << "' should stay invalid, got "
                                              << dtText(col->dateTimeAt(i)).toStdString());
                continue;
            }
            CHECK_COND(gotValid && col->dateTimeAt(i) == exp,
                       "99a: '" << isoCases[i].text << "' -> "
                                << (gotValid ? dtText(col->dateTimeAt(i)).toStdString()
                                             : std::string("<invalid>"))
                                << ", expected " << dtText(exp).toStdString());
        }

        // formats the old parser read keep exactly its result
        const QStringList oldTexts = { QStringLiteral("2024-03-01 10:00:00"),
                                       QStringLiteral("2024-03-01 10:00"),
                                       QStringLiteral("2024-03-01"),
                                       QStringLiteral("2024/3/1"),
                                       QStringLiteral("31.12.2023"),
                                       QStringLiteral("31/12/2023 10:30"),
                                       QStringLiteral("2024-03-01, 10:00"),
                                       QStringLiteral("10:30"),
                                       QStringLiteral("10:30:15"),
                                       QStringLiteral("20240102"),
                                       QStringLiteral("2024-03-01 10:00:00.5"),
                                       QStringLiteral("2024-03-01 10:00:00.123"),
                                       QStringLiteral("2024-03-01 10:00:00.000") };
        Column *oldCol =
                convertTexts(QStringLiteral("T99Old"), oldTexts, SciDAVis::ColumnMode::DateTime);
        CHECK_COND(oldCol != nullptr, "99a: conversion of the old formats failed");
        const QString defaultFormat = QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz");
        for (int i = 0; i < oldTexts.size(); ++i) {
            const QDateTime legacy = legacyDateTimeParse(oldTexts[i], defaultFormat);
            CHECK_COND(legacy.isValid(), "99a: reference parser did not read " << oldTexts[i].toStdString());
            CHECK_COND(!oldCol->isInvalid(i) && oldCol->dateTimeAt(i) == legacy,
                       "99a: '" << oldTexts[i].toStdString() << "' changed: "
                                << dtText(oldCol->dateTimeAt(i)).toStdString() << " (was "
                                << dtText(legacy).toStdString() << ")");
        }
        std::cout << "[PASS] 99a: ISO 8601 (T, fraction, zones, basic form) and old formats\n";
    }

    // -------------------------------------------------------------------------
    // 99b: the same parsing through JSON-RPC writes
    // -------------------------------------------------------------------------
    {
        Table *t = w99->newTable(4, 1, QStringLiteral("T99Rpc"), QStringLiteral("Rpc99"));
        CHECK_COND(t != nullptr, "99b: newTable failed");
        const QJsonArray dates{ QStringLiteral("2023-12-31T23:59:59"),
                                QStringLiteral("2024-03-01T10:00:00.250000+09:00"),
                                QJsonValue::Null, QStringLiteral("2024-03-01 10:00:00") };
        QJsonObject r = rpc(QStringLiteral("table.add_column"),
                            QJsonObject{ { "table", "T99Rpc" }, { "name", "When" },
                                         { "mode", "datetime" }, { "data", dates } });
        CHECK_COND(!isErr(r), "99b: add_column(mode=datetime) failed: " << errText(r));
        const int c = t->numCols() - 1;
        Column *col = t->column(c);
        CHECK_COND(col && col->columnMode() == SciDAVis::ColumnMode::DateTime,
                   "99b: add_column did not keep the date/time type");
        CHECK_COND(!col->isInvalid(0)
                           && col->dateTimeAt(0) == QDateTime(QDate(2023, 12, 31), QTime(23, 59, 59)),
                   "99b: ISO 'T' value lost or wrong: " << dtText(col->dateTimeAt(0)).toStdString());
        CHECK_COND(!col->isInvalid(1)
                           && col->dateTimeAt(1)
                                   == QDateTime(QDate(2024, 3, 1), QTime(10, 0, 0, 250)),
                   "99b: ISO value with fraction and zone wrong: "
                           << dtText(col->dateTimeAt(1)).toStdString());
        CHECK_COND(col->isInvalid(2), "99b: null row is not empty");
        CHECK_COND(!col->isInvalid(3)
                           && col->dateTimeAt(3) == QDateTime(QDate(2024, 3, 1), QTime(10, 0, 0)),
                   "99b: space-separated value changed");
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T99Rpc" }, { "col", "When" }, { "row", 2 },
                             { "value", "2025-06-30T08:15:30Z" } });
        CHECK_COND(!isErr(r), "99b: set_cell failed: " << errText(r));
        col = t->column(c);
        CHECK_COND(col->columnMode() == SciDAVis::ColumnMode::DateTime && !col->isInvalid(2)
                           && col->dateTimeAt(2)
                                   == QDateTime(QDate(2025, 6, 30), QTime(8, 15, 30)),
                   "99b: set_cell with an ISO value: " << dtText(col->dateTimeAt(2)).toStdString());
        std::cout << "[PASS] 99b: RPC add_column / set_cell read ISO 8601\n";
    }

    // -------------------------------------------------------------------------
    // 99c: English month and day names
    // -------------------------------------------------------------------------
    {
        const QString localeMarch = QDate(2000, 3, 1).toString(QStringLiteral("MMMM"));
        const QStringList monthTexts = { QStringLiteral("January"), QStringLiteral("Feb"),
                                         QStringLiteral("3"),       QStringLiteral("dec"),
                                         QStringLiteral("SEPTEMBER"), localeMarch,
                                         QStringLiteral("Foo") };
        const int monthExpected[] = { 1, 2, 3, 12, 9, 3, 0 };
        Column *mc = convertTexts(QStringLiteral("T99Month"), monthTexts,
                                  SciDAVis::ColumnMode::Month);
        CHECK_COND(mc != nullptr, "99c: text -> month conversion failed");
        for (int i = 0; i < monthTexts.size(); ++i) {
            if (monthExpected[i] == 0) {
                CHECK_COND(mc->isInvalid(i) || !mc->dateTimeAt(i).isValid(),
                           "99c: '" << monthTexts[i].toStdString() << "' became a month");
                continue;
            }
            CHECK_COND(!mc->isInvalid(i)
                               && mc->dateTimeAt(i).date()
                                       == QDate(1900, 1, 1).addMonths(monthExpected[i] - 1),
                       "99c: month '" << monthTexts[i].toStdString() << "' -> "
                                      << dtText(mc->dateTimeAt(i)).toStdString());
        }

        const QString localeFriday = QDate(1900, 1, 5).toString(QStringLiteral("dddd"));
        const QStringList dayTexts = { QStringLiteral("Monday"), QStringLiteral("tue"),
                                       QStringLiteral("7"),      QStringLiteral("SUNDAY"),
                                       QStringLiteral("Thu"),    localeFriday,
                                       QStringLiteral("Xyz") };
        const int dayExpected[] = { 1, 2, 7, 7, 4, 5, 0 };
        Column *dc =
                convertTexts(QStringLiteral("T99Day"), dayTexts, SciDAVis::ColumnMode::Day);
        CHECK_COND(dc != nullptr, "99c: text -> day conversion failed");
        for (int i = 0; i < dayTexts.size(); ++i) {
            if (dayExpected[i] == 0) {
                CHECK_COND(dc->isInvalid(i) || !dc->dateTimeAt(i).isValid(),
                           "99c: '" << dayTexts[i].toStdString() << "' became a day");
                continue;
            }
            CHECK_COND(!dc->isInvalid(i)
                               && dc->dateTimeAt(i).date()
                                       == QDate(1900, 1, 1).addDays(dayExpected[i] - 1),
                       "99c: day '" << dayTexts[i].toStdString() << "' -> "
                                    << dtText(dc->dateTimeAt(i)).toStdString());
        }

        // and through the RPC write
        Table *t = w99->newTable(3, 1, QStringLiteral("T99MRpc"), QStringLiteral("MRpc99"));
        CHECK_COND(t != nullptr, "99c: newTable failed");
        const QJsonObject r = rpc(
                QStringLiteral("table.add_column"),
                QJsonObject{ { "table", "T99MRpc" }, { "name", "M" }, { "mode", "month" },
                             { "data", QJsonArray{ "January", "Feb", "3" } } });
        CHECK_COND(!isErr(r), "99c: add_column(mode=month) failed: " << errText(r));
        Column *rc = t->column(t->numCols() - 1);
        CHECK_COND(rc->columnMode() == SciDAVis::ColumnMode::Month && !rc->isInvalid(0)
                           && !rc->isInvalid(1) && !rc->isInvalid(2)
                           && rc->dateTimeAt(0).date().month() == 1
                           && rc->dateTimeAt(1).date().month() == 2
                           && rc->dateTimeAt(2).date().month() == 3,
                   "99c: add_column(mode=month) with English names lost values");
        std::cout << "[PASS] 99c: English month / day names, numbers unchanged\n";
    }

    // -------------------------------------------------------------------------
    // 99d: 2000-column tables (create, import, GUI column count + Undo/Redo)
    // -------------------------------------------------------------------------
    {
        const int nCols = 2000;
        const int nRows = 5;
        const qint64 limitMs = 30000; // a few seconds on a developer PC; generous for CI

        QElapsedTimer timer;
        timer.start();
        QJsonObject r = rpc(QStringLiteral("table.create"),
                            QJsonObject{ { "rows", nRows }, { "cols", nCols }, { "name", "T99Wide" } });
        const qint64 createMs = timer.elapsed();
        CHECK_COND(!isErr(r), "99d: table.create failed: " << errText(r));
        std::cout << "[INFO] 99d: table.create " << nCols << " x " << nRows << ": " << createMs
                  << " ms\n";
        CHECK_COND(createMs < limitMs, "99d: table.create of 2000 columns took " << createMs << " ms");
        Table *wide = w99->table(res(r).value(QStringLiteral("name")).toString());
        CHECK_COND(wide && wide->numCols() == nCols && wide->numRows() == nRows,
                   "99d: created table has the wrong size");
        for (int c = 0; c < nCols; ++c) {
            CHECK_COND(wide->column(c)->name() == QString::number(c + 1),
                       "99d: created column " << c << " is named " << wide->column(c)->name().toStdString());
            CHECK_COND(wide->colPlotDesignation(c) == (c == 0 ? SciDAVis::X : SciDAVis::Y),
                       "99d: created column " << c << " has the wrong designation");
        }
        CHECK_COND(wide->d_future_table->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString()
                                   == QStringLiteral("1[X]")
                           && wide->d_future_table
                                           ->headerData(nCols - 1, Qt::Horizontal, Qt::DisplayRole)
                                           .toString()
                                   == QStringLiteral("2000[Y]"),
                   "99d: created table header labels wrong");

        // import of a 2000 x 5 CSV (the case that ran past the RPC timeout)
        QTemporaryDir tmp;
        CHECK_COND(tmp.isValid(), "99d: no temporary directory");
        const QString csvPath = tmp.filePath(QStringLiteral("wide2000.csv"));
        {
            QFile f(csvPath);
            CHECK_COND(f.open(QIODevice::WriteOnly | QIODevice::Text), "99d: cannot write CSV");
            QTextStream ts(&f);
            QStringList hdr;
            for (int c = 0; c < nCols; ++c)
                hdr << QStringLiteral("c%1").arg(c);
            ts << hdr.join(QLatin1Char(',')) << "\n";
            for (int row = 0; row < nRows; ++row) {
                QStringList line;
                for (int c = 0; c < nCols; ++c)
                    line << QString::number(row * nCols + c);
                ts << line.join(QLatin1Char(',')) << "\n";
            }
        }
        timer.restart();
        r = rpc(QStringLiteral("app.import_csv"),
                QJsonObject{ { "path", csvPath }, { "name", "T99Imp" } });
        const qint64 importMs = timer.elapsed();
        CHECK_COND(!isErr(r), "99d: app.import_csv failed: " << errText(r));
        std::cout << "[INFO] 99d: app.import_csv " << nCols << " x " << nRows << ": " << importMs
                  << " ms\n";
        CHECK_COND(importMs < limitMs, "99d: import of 2000 columns took " << importMs << " ms");
        Table *imp = w99->table(res(r).value(QStringLiteral("name")).toString());
        CHECK_COND(imp && imp->numCols() == nCols && imp->numRows() == nRows,
                   "99d: imported table has the wrong size");
        for (int c = 0; c < nCols; ++c) {
            CHECK_COND(imp->column(c)->name() == QStringLiteral("c%1").arg(c),
                       "99d: imported column " << c << " is named " << imp->column(c)->name().toStdString());
            Column *col = imp->column(c);
            CHECK_COND(col->columnMode() == SciDAVis::ColumnMode::Numeric,
                       "99d: imported column " << c << " is not numeric");
            for (int row = 0; row < nRows; ++row) {
                CHECK_COND(!col->isInvalid(row) && col->valueAt(row) == double(row * nCols + c),
                           "99d: imported value at row " << row << ", column " << c << " is "
                                                         << col->valueAt(row));
            }
        }

        // GUI path: column count 2 -> 2000 is one Undo step, Undo / Redo restore it
        Table *grow = w99->newTable(nRows, 2, QStringLiteral("T99Grow"), QStringLiteral("Grow99"));
        CHECK_COND(grow != nullptr, "99d: newTable failed");
        QCoreApplication::processEvents();
        QUndoStack *us = grow->d_future_table->undoStack();
        CHECK_COND(us != nullptr, "99d: no undo stack");
        us->clear(); // the project stack is at its Undo limit after the steps above
        const int idx0 = us->index();
        timer.restart();
        grow->setNumCols(nCols);
        QCoreApplication::processEvents();
        const qint64 growMs = timer.elapsed();
        std::cout << "[INFO] 99d: setNumCols 2 -> " << nCols << ": " << growMs << " ms\n";
        CHECK_COND(growMs < limitMs, "99d: adding 1998 columns took " << growMs << " ms");
        CHECK_COND(grow->numCols() == nCols, "99d: setNumCols did not reach 2000 columns");
        CHECK_COND(us->index() == idx0 + 1, "99d: adding the columns was not one Undo step ("
                                                     << idx0 << " -> " << us->index() << ", count "
                                                     << us->count() << ")");
        QStringList grownNames;
        for (int c = 0; c < nCols; ++c)
            grownNames << grow->column(c)->name();
        CHECK_COND(grownNames.removeDuplicates() == 0, "99d: added columns have duplicate names");
        CHECK_COND(grow->d_future_table->headerData(nCols - 1, Qt::Horizontal, Qt::DisplayRole)
                                   .toString()
                           == grow->column(nCols - 1)->name() + QStringLiteral("[Y]"),
                   "99d: header label of the last added column wrong");
        timer.restart();
        us->undo();
        QCoreApplication::processEvents();
        const qint64 undoMs = timer.elapsed();
        CHECK_COND(grow->numCols() == 2, "99d: Undo did not remove the added columns");
        timer.restart();
        us->redo();
        QCoreApplication::processEvents();
        const qint64 redoMs = timer.elapsed();
        std::cout << "[INFO] 99d: Undo " << undoMs << " ms, Redo " << redoMs << " ms\n";
        CHECK_COND(undoMs < limitMs && redoMs < limitMs, "99d: Undo / Redo of 1998 columns too slow");
        CHECK_COND(grow->numCols() == nCols, "99d: Redo did not restore the columns");
        for (int c = 0; c < nCols; ++c) {
            CHECK_COND(grow->column(c)->name() == grownNames.at(c),
                       "99d: Redo changed the name of column " << c);
        }
        std::cout << "[PASS] 99d: 2000-column create " << createMs << " ms, import " << importMs
                  << " ms, add columns " << growMs << " ms\n";
    }

    // -------------------------------------------------------------------------
    // 99e: header labels through the incremental update
    // -------------------------------------------------------------------------
    {
        Table *t = w99->newTable(3, 5, QStringLiteral("T99Hdr"), QStringLiteral("Hdr99"));
        CHECK_COND(t != nullptr, "99e: newTable failed");
        auto labels = [&]() {
            QStringList l;
            for (int c = 0; c < t->numCols(); ++c)
                l << t->d_future_table->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
            return l.join(QLatin1Char(' '));
        };
        CHECK_COND(labels() == QStringLiteral("1[X] 2[Y] 3[Y] 4[Y] 5[Y]"),
                   "99e: initial labels " << labels().toStdString());
        t->setColPlotDesignation(2, SciDAVis::X);
        t->setColPlotDesignation(4, SciDAVis::yErr);
        CHECK_COND(labels() == QStringLiteral("1[X1] 2[Y1] 3[X2] 4[Y2] 5[yEr2]"),
                   "99e: two X columns " << labels().toStdString());
        t->setColName(3, QStringLiteral("Q"));
        CHECK_COND(labels() == QStringLiteral("1[X1] 2[Y1] 3[X2] Q[Y2] 5[yEr2]"),
                   "99e: rename " << labels().toStdString());
        t->setColPlotDesignation(2, SciDAVis::noDesignation);
        CHECK_COND(labels() == QStringLiteral("1[X] 2[Y] 3 Q[Y] 5[yEr]"),
                   "99e: back to one X column " << labels().toStdString());
        Column *nx = new Column(QStringLiteral("N"), SciDAVis::ColumnMode::Numeric);
        nx->setPlotDesignation(SciDAVis::X);
        t->d_future_table->insertColumns(1, QList<Column *>() << nx);
        CHECK_COND(labels() == QStringLiteral("1[X1] N[X2] 2[Y2] 3 Q[Y2] 5[yEr2]"),
                   "99e: inserted X column " << labels().toStdString());
        t->d_future_table->removeColumns(0, 1);
        CHECK_COND(labels() == QStringLiteral("N[X] 2[Y] 3 Q[Y] 5[yEr]"),
                   "99e: removed first column " << labels().toStdString());
        t->d_future_table->undoStack()->undo();
        CHECK_COND(labels() == QStringLiteral("1[X1] N[X2] 2[Y2] 3 Q[Y2] 5[yEr2]"),
                   "99e: undo of the removal " << labels().toStdString());
        std::cout << "[PASS] 99e: header labels (several X, rename, insert, remove, undo)\n";
    }

    // -------------------------------------------------------------------------
    // 99f: automatic recalculation with the cached formula scan
    // -------------------------------------------------------------------------
    {
        Table *t = w99->newTable(3, 3, QStringLiteral("T99Rc"), QStringLiteral("Rc99"));
        Table *o = w99->newTable(3, 1, QStringLiteral("T99Rx"), QStringLiteral("Rx99"));
        CHECK_COND(t && o, "99f: newTable failed");
        t->setAutoRecalcFormulas(true);
        o->setAutoRecalcFormulas(true);
        t->setColName(0, QStringLiteral("A"));
        t->setColName(1, QStringLiteral("B"));
        t->setColName(2, QStringLiteral("C"));
        auto setCell = [&](const char *table, const char *col, int row, double v) {
            return rpc(QStringLiteral("table.set_cell"),
                       QJsonObject{ { "table", table }, { "col", col }, { "row", row }, { "value", v } });
        };
        // a write first, so the scan result (no formulas) is cached
        CHECK_COND(!isErr(setCell("T99Rc", "A", 0, 1.0)), "99f: set_cell failed");
        QJsonObject r = rpc(QStringLiteral("table.set_formula"),
                            QJsonObject{ { "table", "T99Rc" }, { "col", "C" },
                                         { "formula", "col(\"A\")*2" } });
        CHECK_COND(!isErr(r), "99f: set_formula failed: " << errText(r));
        r = rpc(QStringLiteral("table.set_formula"),
                QJsonObject{ { "table", "T99Rx" }, { "col", 0 },
                             { "formula", "tablecol(\"T99Rc\", \"A\")+100" } });
        CHECK_COND(!isErr(r), "99f: cross-table set_formula failed: " << errText(r));
        CHECK_COND(!isErr(setCell("T99Rc", "A", 0, 5.0)), "99f: set_cell failed");
        CHECK_COND(t->cell(0, 2) == 10.0,
                   "99f: formula set after a write was not recalculated: " << t->cell(0, 2));
        CHECK_COND(o->cell(0, 0) == 105.0,
                   "99f: formula in another table was not recalculated: " << o->cell(0, 0));
        // a column removed before the formula column: the formula is now column 1
        t->d_future_table->removeColumns(1, 1);
        QCoreApplication::processEvents();
        CHECK_COND(t->numCols() == 2 && t->column(1)->name() == QStringLiteral("C"),
                   "99f: column removal failed");
        CHECK_COND(!isErr(setCell("T99Rc", "A", 1, 7.0)), "99f: set_cell failed");
        CHECK_COND(t->cell(1, 1) == 14.0,
                   "99f: formula column after a removal was not recalculated: " << t->cell(1, 1));
        // a cleared formula is not recalculated any more
        t->column(1)->clearFormulas();
        const double before = t->cell(2, 1);
        CHECK_COND(!isErr(setCell("T99Rc", "A", 2, 9.0)), "99f: set_cell failed");
        CHECK_COND(t->cell(2, 1) == before || (std::isnan(before) && std::isnan(t->cell(2, 1))),
                   "99f: column with a cleared formula was recalculated");
        std::cout << "[PASS] 99f: dependent recalculation with the cached formula scan\n";
    }

    std::cout << "[PASS] Suite 99 passed.\n";
    return 0;
}
