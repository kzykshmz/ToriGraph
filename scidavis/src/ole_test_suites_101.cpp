#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "core/datatypes/NumericDateTimeBaseFilter.h"

#include <QBuffer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QXmlStreamWriter>

#include <cmath>
#include <functional>
#include <memory>

// =============================================================================
// Suite 101: worksheet fixes from the second data-driven review.
//   101a Long Name / Units set through table.set_column_meta: Undo and Redo walk the whole
//        history (the RPC's own undo command called Column::setLongName / setUnits from its
//        undo()/redo(), which pushed new commands and dropped the Redo entries).
//   101b col() / cell() of a date/time, month, day of week or text column in a column formula:
//        the numbers of the column type conversion (days since 1970-01-01 00:00, month and
//        weekday numbers, the number a text spells), not 0; a text that is no number gives an
//        empty cell.
//   101c date/time text: fractions of a second with 1-2 digits and a decimal comma, a leap
//        second (23:59:60 -> 23:59:59.999), a 12-hour clock with am / pm; a date with a time
//        part that cannot be read is invalid (was the date at 00:00); a date alone is still 00:00.
//   101d date/time <-> number without the PC's time zone: 2024-03-01 00:00 is 19783, 0 is
//        1970-01-01 00:00; the base is saved as wall-clock time ("base_wall"), and files written
//        before it (base 0, or an instant in the PC's zone) load to the same clock time.
//   101e matrix.set_data keeps finite values beyond 1e300 (as table.set_column_data does).
//   101f Undo / Redo of a column move keep the moved column connected to the table: rename,
//        designation and data changes still reach the header and the views, each change once.
//   101g opening a project with a 2000-column table adds the columns in one go (it took
//        ~18 s; the columns were appended one by one): time limit, names, values, widths.
// =============================================================================

namespace {

QString dt101(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
                        : QStringLiteral("<invalid>");
}

QByteArray save101(const Column *col)
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buf);
    writer.writeStartDocument();
    col->save(&writer);
    writer.writeEndDocument();
    buf.close();
    return bytes;
}

std::unique_ptr<Column> load101(const QByteArray &xml, bool *ok)
{
    std::unique_ptr<Column> col(new Column("s101_loaded", SciDAVis::ColumnMode::Numeric));
    XmlStreamReader reader(xml);
    while (!reader.atEnd() && !(reader.isStartElement() && reader.name() == "column"))
        reader.readNext();
    *ok = reader.isStartElement() && col->load(&reader);
    return col;
}

} // namespace

int runSuite101WorksheetRound2(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 101: worksheet fixes (second data-driven review)...\n";

    struct NonInteractiveQuitGuard101
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard101()
            : prevNonInteractive(qApp->property("torigraph_noninteractive"))
            , prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard101()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard101;

    struct AppWinGuard101
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard101(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard101() { reset(); }
        AppWinGuard101(const AppWinGuard101 &) = delete;
        AppWinGuard101 &operator=(const AppWinGuard101 &) = delete;
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

    AppWinGuard101 w101(new ApplicationWindow());
    w101->applyUserSettings();
    w101->confirmCloseTable = false;
    w101->confirmCloseMatrix = false;
    w101->confirmClosePlot2D = false;
    w101->confirmClosePlot3D = false;
    w101->confirmCloseFolder = false;
    w101->resize(1280, 800);
    w101->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w101->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "101: pythonAutomationServer null");

    int rpcId = 10100;
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
        Table *t = w101->newTable(texts.size(), 1, tableName, tableName);
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
    // 101a: Long Name / Units through RPC, Undo x3 and Redo x3
    // -------------------------------------------------------------------------
    {
        Table *t = w101->newTable(4, 2, QStringLiteral("T101Meta"), QStringLiteral("Meta101"));
        CHECK_COND(t != nullptr, "101a: newTable failed");
        QUndoStack *us = t->d_future_table->undoStack();
        CHECK_COND(us != nullptr, "101a: no undo stack");
        us->clear(); // stay below the undo limit (it drops the oldest commands)
        auto meta = [&](const QString &ln, const QString &un) {
            return rpc(QStringLiteral("table.set_column_meta"),
                       QJsonObject{ { "table", "T101Meta" }, { "col", 1 },
                                    { "long_name", ln }, { "units", un } });
        };
        auto state = [&]() {
            return QStringLiteral("%1|%2|%3")
                    .arg(t->column(1)->longName(), t->column(1)->units())
                    .arg(t->column(0)->valueAt(1));
        };
        t->column(0)->setValueAt(1, 1.0);
        const QString s0 = state();
        QJsonObject r = meta(QStringLiteral("L0"), QStringLiteral("mm"));
        CHECK_COND(!isErr(r), "101a: set_column_meta failed: " << errText(r));
        const QString s1 = state();
        t->column(0)->setValueAt(1, 101.0);
        const QString s2 = state();
        r = meta(QStringLiteral("L2"), QStringLiteral("um"));
        CHECK_COND(!isErr(r), "101a: second set_column_meta failed: " << errText(r));
        const QString s3 = state();
        CHECK_COND(s3 == QStringLiteral("L2|um|101"), "101a: state after the edits " << s3.toStdString());
        const QStringList back = { s2, s1, s0 };
        for (const QString &exp : back) {
            us->undo();
            QCoreApplication::processEvents();
            CHECK_COND(state() == exp, "101a: undo gave " << state().toStdString() << ", expected "
                                                          << exp.toStdString());
        }
        const QStringList fwd = { s1, s2, s3 };
        for (int i = 0; i < fwd.size(); ++i) {
            CHECK_COND(us->canRedo(), "101a: Redo history lost before redo " << i + 1);
            us->redo();
            QCoreApplication::processEvents();
            CHECK_COND(state() == fwd[i], "101a: redo " << i + 1 << " gave "
                                                        << state().toStdString() << ", expected "
                                                        << fwd[i].toStdString());
        }
        CHECK_COND(!us->canRedo(), "101a: Redo left after redoing everything");
        // the header shows the restored Long Name after an Undo
        us->undo();
        QCoreApplication::processEvents();
        CHECK_COND(t->column(1)->longName() == QStringLiteral("L0"),
                   "101a: long name after undo " << t->column(1)->longName().toStdString());
        us->redo();
        std::cout << "[PASS] 101a: Long Name / Units via RPC keep the whole Undo / Redo history\n";
    }

    // -------------------------------------------------------------------------
    // 101b: col() / cell() of non-numeric columns in a formula
    // -------------------------------------------------------------------------
    {
        Table *t = w101->newTable(3, 1, QStringLiteral("T101Ops"), QStringLiteral("Ops101"));
        CHECK_COND(t != nullptr, "101b: newTable failed");
        auto addCol = [&](const char *name, const char *mode, const QJsonArray &data) {
            return rpc(QStringLiteral("table.add_column"),
                       QJsonObject{ { "table", "T101Ops" }, { "name", name }, { "mode", mode },
                                    { "data", data } });
        };
        QJsonObject r = addCol("A", "datetime",
                               QJsonArray{ "2024-03-01 10:00", "2023-12-31 00:00", "1999-01-01" });
        CHECK_COND(!isErr(r), "101b: add A failed: " << errText(r));
        r = addCol("B", "datetime",
                   QJsonArray{ "2024-03-02 10:00", "2024-01-01 12:00", "2000-01-01" });
        CHECK_COND(!isErr(r), "101b: add B failed: " << errText(r));
        r = addCol("M", "month", QJsonArray{ 1, 6, 12 });
        CHECK_COND(!isErr(r), "101b: add M failed: " << errText(r));
        r = addCol("D", "day", QJsonArray{ 1, 3, 7 });
        CHECK_COND(!isErr(r), "101b: add D failed: " << errText(r));
        r = addCol("S", "text", QJsonArray{ "1.5", " 2 ", "abc" });
        CHECK_COND(!isErr(r), "101b: add S failed: " << errText(r));
        struct FormulaCase
        {
            const char *name;
            const char *formula;
            std::vector<double> expected; // NaN: empty cell
        };
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const std::vector<FormulaCase> cases = {
            { "Diff", "col(\"B\")-col(\"A\")", { 1.0, 1.5, 365.0 } },
            { "AN", "col(\"A\")", { 19783.0 + 10.0 / 24.0, 19722.0, 10592.0 } },
            { "MN", "col(\"M\")", { 1.0, 6.0, 12.0 } },
            { "DN", "col(\"D\")", { 1.0, 3.0, 7.0 } },
            { "SN", "col(\"S\")*2", { 3.0, 4.0, nan } },
            { "CD", "cell(\"B\", 1)-cell(\"A\", 2)", { 62.0 + 10.0 / 24.0, 62.0 + 10.0 / 24.0,
                                                      62.0 + 10.0 / 24.0 } },
        };
        for (const FormulaCase &fc : cases) {
            r = rpc(QStringLiteral("table.add_column"),
                    QJsonObject{ { "table", "T101Ops" }, { "name", fc.name } });
            CHECK_COND(!isErr(r), "101b: add " << fc.name << " failed: " << errText(r));
            r = rpc(QStringLiteral("table.set_formula"),
                    QJsonObject{ { "table", "T101Ops" }, { "col", fc.name },
                                 { "formula", fc.formula } });
            CHECK_COND(!isErr(r), "101b: set_formula " << fc.formula << " failed: " << errText(r));
            const int c = t->numCols() - 1;
            Column *col = t->column(c);
            for (int row = 0; row < 3; ++row) {
                const double exp = fc.expected[size_t(row)];
                if (std::isnan(exp)) {
                    CHECK_COND(col->isInvalid(row), "101b: " << fc.formula << " row " << row
                                                             << " = " << col->valueAt(row)
                                                             << ", expected an empty cell");
                    continue;
                }
                CHECK_COND(!col->isInvalid(row) && std::fabs(col->valueAt(row) - exp) < 1e-9,
                           "101b: " << fc.formula << " row " << row << " = "
                                    << (col->isInvalid(row) ? std::string("<empty>")
                                                            : std::to_string(col->valueAt(row)))
                                    << ", expected " << exp);
            }
        }
        std::cout << "[PASS] 101b: col() / cell() of date/time, month, day and text columns\n";
    }

    // -------------------------------------------------------------------------
    // 101c: date/time text with short fractions, decimal comma, leap second, bad time parts
    // -------------------------------------------------------------------------
    {
        const QDate d0301(2024, 3, 1);
        const QDate d1231(2016, 12, 31);
        struct TextCase
        {
            const char *text;
            QDateTime expected; // invalid: must be invalid
        };
        const std::vector<TextCase> cases = {
            { "2024-03-01 10:00:00.5", QDateTime(d0301, QTime(10, 0, 0, 500)) },
            { "2024-03-01 10:00:00.25", QDateTime(d0301, QTime(10, 0, 0, 250)) },
            { "2024/03/01 10:00:00.5", QDateTime(d0301, QTime(10, 0, 0, 500)) },
            { "2024/03/01 10:00:00,25", QDateTime(d0301, QTime(10, 0, 0, 250)) },
            { "2024-03-01 10:00:00,5", QDateTime(d0301, QTime(10, 0, 0, 500)) },
            { "10:00:00.5", QDateTime(QDate(1900, 1, 1), QTime(10, 0, 0, 500)) },
            { "05:30.5", QDateTime(QDate(1900, 1, 1), QTime(0, 5, 30, 500)) },
            { "2016-12-31 23:59:60", QDateTime(d1231, QTime(23, 59, 59, 999)) },
            { "2016-12-31T23:59:60", QDateTime(d1231, QTime(23, 59, 59, 999)) },
            { "2016/12/31 23:59:60.5", QDateTime(d1231, QTime(23, 59, 59, 999)) },
            { "2024-03-01 10:00:15 pm", QDateTime(d0301, QTime(22, 0, 15)) },
            { "2024/03/01 12:30 AM", QDateTime(d0301, QTime(0, 30)) },
            { "2024-03-01 13:00 pm", QDateTime() },
            { "2024-03-01, 10:00", QDateTime(d0301, QTime(10, 0)) },
            { "2024-03-01 10:00", QDateTime(d0301, QTime(10, 0)) },
            { "2024-03-01", QDateTime(d0301, QTime(0, 0)) },
            { "2024-03-01 25:00", QDateTime() },
            { "2024-03-01 abc", QDateTime() },
            { "2024-01-02 09:00-10:30", QDateTime() },
            { "2024-03-01 10:00:61", QDateTime() },
            { "abc 10:00", QDateTime() },
        };
        QStringList texts;
        for (const TextCase &c : cases)
            texts << QString::fromUtf8(c.text);
        Column *col = convertTexts(QStringLiteral("T101Text"), texts, SciDAVis::ColumnMode::DateTime);
        CHECK_COND(col != nullptr, "101c: text -> date/time conversion failed");
        for (int i = 0; i < int(cases.size()); ++i) {
            const bool gotValid = !col->isInvalid(i) && col->dateTimeAt(i).isValid();
            const QDateTime &exp = cases[size_t(i)].expected;
            if (!exp.isValid()) {
                CHECK_COND(!gotValid, "101c: '" << cases[size_t(i)].text << "' -> "
                                                << dt101(col->dateTimeAt(i)).toStdString()
                                                << ", expected invalid");
                continue;
            }
            CHECK_COND(gotValid && col->dateTimeAt(i) == exp,
                       "101c: '" << cases[size_t(i)].text << "' -> "
                                 << (gotValid ? dt101(col->dateTimeAt(i)).toStdString()
                                              : std::string("<invalid>"))
                                 << ", expected " << dt101(exp).toStdString());
        }
        // the RPC write path uses the same filter
        Table *t = w101->newTable(2, 1, QStringLiteral("T101TextRpc"), QStringLiteral("Rpc101"));
        CHECK_COND(t != nullptr, "101c: newTable failed");
        QJsonObject r = rpc(QStringLiteral("table.add_column"),
                            QJsonObject{ { "table", "T101TextRpc" }, { "name", "When" },
                                         { "mode", "datetime" },
                                         { "data", QJsonArray{ "2024-03-01 10:00:00.5",
                                                               "2024-03-01 25:00" } } });
        CHECK_COND(!isErr(r), "101c: add_column failed: " << errText(r));
        Column *rc = t->column(t->numCols() - 1);
        CHECK_COND(!rc->isInvalid(0) && rc->dateTimeAt(0) == QDateTime(d0301, QTime(10, 0, 0, 500)),
                   "101c: RPC '2024-03-01 10:00:00.5' -> " << dt101(rc->dateTimeAt(0)).toStdString());
        CHECK_COND(rc->isInvalid(1) || !rc->dateTimeAt(1).isValid(),
                   "101c: RPC '2024-03-01 25:00' -> " << dt101(rc->dateTimeAt(1)).toStdString());
        std::cout << "[PASS] 101c: short fractions, decimal comma, leap second, unreadable time "
                     "parts are invalid\n";
    }

    // -------------------------------------------------------------------------
    // 101d: date/time <-> number without the PC's zone; saved base
    // -------------------------------------------------------------------------
    {
        Column *col = convertTexts(QStringLiteral("T101Tz"),
                                   { QStringLiteral("2024-03-01 00:00"),
                                     QStringLiteral("1970-01-01 00:00"),
                                     QStringLiteral("1970-01-01 09:00"),
                                     QStringLiteral("2024-03-01 12:00") },
                                   SciDAVis::ColumnMode::DateTime);
        CHECK_COND(col != nullptr, "101d: text -> date/time conversion failed");
        const QDateTime defaultBase = col->numericDateTimeBaseFilter()->getBaseDateTime();
        CHECK_COND(defaultBase == QDateTime(QDate(1970, 1, 1), QTime(0, 0)),
                   "101d: default base " << dt101(defaultBase).toStdString());

        // the base survives save -> load, and older files load to the same clock time
        const QByteArray xml = save101(col);
        CHECK_COND(xml.contains("base_wall=\"1\""), "101d: base_wall not written");
        bool ok = false;
        std::unique_ptr<Column> loaded = load101(xml, &ok);
        CHECK_COND(ok && loaded->numericDateTimeBaseFilter()
                           && loaded->numericDateTimeBaseFilter()->getBaseDateTime() == defaultBase,
                   "101d: base after save -> load is not 1970-01-01 00:00");
        // an older file: base 0 (the default) without base_wall
        QString legacy = QString::fromUtf8(xml);
        legacy.remove(QStringLiteral(" base_wall=\"1\""));
        legacy.replace(QRegularExpression(QStringLiteral("base_datetime=\"-?\\d+\"")),
                       QStringLiteral("base_datetime=\"0\""));
        loaded = load101(legacy.toUtf8(), &ok);
        CHECK_COND(ok && loaded->numericDateTimeBaseFilter()
                           && loaded->numericDateTimeBaseFilter()->getBaseDateTime() == defaultBase,
                   "101d: legacy base 0 did not load as 1970-01-01 00:00");
        // an older file with a base chosen in the dialog: an instant in the PC's zone
        const QDateTime chosen(QDate(2020, 1, 1), QTime(6, 0));
        legacy.replace(QRegularExpression(QStringLiteral("base_datetime=\"-?\\d+\"")),
                       QStringLiteral("base_datetime=\"%1\"").arg(chosen.toMSecsSinceEpoch()));
        loaded = load101(legacy.toUtf8(), &ok);
        CHECK_COND(ok && loaded->numericDateTimeBaseFilter()
                           && loaded->numericDateTimeBaseFilter()->getBaseDateTime() == chosen,
                   "101d: legacy chosen base loaded as "
                           << dt101(loaded->numericDateTimeBaseFilter()->getBaseDateTime())
                                      .toStdString());
        // and a non-default base written now survives save -> load
        {
            NumericDateTimeBaseFilter f(NumericDateTimeBaseFilter::UnitInterval::Hour, chosen);
            CHECK_COND(f.getBaseDateTime() == chosen, "101d: chosen base changed");
            CHECK_COND(std::fabs(f.offsetToDouble(QDateTime(QDate(2020, 1, 2), QTime(6, 0)))
                                 - 24.0) < 1e-9,
                       "101d: one day after the base is not 24 h");
        }

        // date/time -> number
        col->setColumnMode(SciDAVis::ColumnMode::Numeric);
        QCoreApplication::processEvents();
        const std::vector<double> nums = { 19783.0, 0.0, 0.375, 19783.5 };
        for (int i = 0; i < int(nums.size()); ++i)
            CHECK_COND(std::fabs(col->valueAt(i) - nums[size_t(i)]) < 1e-9,
                       "101d: row " << i << " -> " << col->valueAt(i) << ", expected "
                                    << nums[size_t(i)]);
        // number -> date/time
        col->setValueAt(1, 1.0);
        col->setColumnMode(SciDAVis::ColumnMode::DateTime);
        QCoreApplication::processEvents();
        const std::vector<QDateTime> dts = { QDateTime(QDate(2024, 3, 1), QTime(0, 0)),
                                             QDateTime(QDate(1970, 1, 2), QTime(0, 0)),
                                             QDateTime(QDate(1970, 1, 1), QTime(9, 0)),
                                             QDateTime(QDate(2024, 3, 1), QTime(12, 0)) };
        for (int i = 0; i < int(dts.size()); ++i)
            CHECK_COND(col->dateTimeAt(i) == dts[size_t(i)],
                       "101d: number -> " << dt101(col->dateTimeAt(i)).toStdString()
                                          << ", expected " << dt101(dts[size_t(i)]).toStdString());

        // RPC set_cell of a number in a date/time column
        Table *t = w101->newTable(2, 1, QStringLiteral("T101TzRpc"), QStringLiteral("TzRpc101"));
        CHECK_COND(t != nullptr, "101d: newTable failed");
        QJsonObject r = rpc(QStringLiteral("table.add_column"),
                            QJsonObject{ { "table", "T101TzRpc" }, { "name", "When" },
                                         { "mode", "datetime" },
                                         { "data", QJsonArray{ "2024-03-01 10:00", "2024-03-01" } } });
        CHECK_COND(!isErr(r), "101d: add_column failed: " << errText(r));
        r = rpc(QStringLiteral("table.set_cell"),
                QJsonObject{ { "table", "T101TzRpc" }, { "row", 0 }, { "col", "When" }, { "value", 0 } });
        CHECK_COND(!isErr(r), "101d: set_cell failed: " << errText(r));
        Column *rc = t->column(t->numCols() - 1);
        CHECK_COND(rc->dateTimeAt(0) == QDateTime(QDate(1970, 1, 1), QTime(0, 0)),
                   "101d: set_cell(0) -> " << dt101(rc->dateTimeAt(0)).toStdString());
        std::cout << "[PASS] 101d: date/time <-> number in wall-clock time, base saved and "
                     "older files read\n";
    }

    // -------------------------------------------------------------------------
    // 101e: matrix.set_data keeps finite values beyond 1e300
    // -------------------------------------------------------------------------
    {
        QJsonObject r = rpc(QStringLiteral("matrix.create"),
                            QJsonObject{ { "rows", 2 }, { "cols", 2 }, { "name", "M101Big" } });
        CHECK_COND(!isErr(r), "101e: matrix.create failed: " << errText(r));
        const QString mname = res(r).value(QStringLiteral("name")).toString();
        r = rpc(QStringLiteral("matrix.set_data"),
                QJsonObject{ { "matrix", mname },
                             { "data", QJsonArray{ QJsonArray{ 1e301, -1.7e308 },
                                                   QJsonArray{ 1e300, 2.0 } } } });
        CHECK_COND(!isErr(r), "101e: matrix.set_data failed: " << errText(r));
        r = rpc(QStringLiteral("matrix.get_data"), QJsonObject{ { "matrix", mname } });
        CHECK_COND(!isErr(r), "101e: matrix.get_data failed: " << errText(r));
        const QJsonArray rows = res(r).value(QStringLiteral("data")).toArray();
        CHECK_COND(rows.size() == 2, "101e: rows " << rows.size());
        const QJsonArray r0 = rows.at(0).toArray();
        const QJsonArray r1 = rows.at(1).toArray();
        CHECK_COND(r0.at(0).isDouble() && r0.at(0).toDouble() == 1e301,
                   "101e: 1e301 not kept");
        CHECK_COND(r0.at(1).isDouble() && r0.at(1).toDouble() == -1.7e308,
                   "101e: -1.7e308 not kept");
        CHECK_COND(r1.at(0).toDouble() == 1e300 && r1.at(1).toDouble() == 2.0,
                   "101e: other cells changed");
        std::cout << "[PASS] 101e: matrix.set_data keeps finite values beyond 1e300\n";
    }

    // -------------------------------------------------------------------------
    // 101f: Undo / Redo of a column move keep the column connected
    // -------------------------------------------------------------------------
    {
        Table *t = w101->newTable(3, 3, QStringLiteral("T101Move"), QStringLiteral("Move101"));
        CHECK_COND(t != nullptr, "101f: newTable failed");
        QUndoStack *us = t->d_future_table->undoStack();
        CHECK_COND(us != nullptr, "101f: no undo stack");
        us->clear(); // stay below the undo limit (it drops the oldest commands)
        int dataSignals = 0;
        QObject::connect(t->d_future_table, &future::Table::dataChanged,
                         [&](int, int, int, int) { ++dataSignals; });
        auto header = [&](int c) {
            return t->d_future_table->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
        };
        Column *moved = t->column(0);
        // reference: signals of one cell change before any move
        dataSignals = 0;
        moved->setValueAt(0, 1.0);
        const int perChange = dataSignals;
        CHECK_COND(perChange > 0, "101f: a cell change emits no dataChanged");

        // checks that every kind of change of `moved` (at index `idx`) still reaches the table
        auto checkConnected = [&](int idx, const char *step) -> bool {
            if (t->column(idx) != moved) {
                std::cerr << "[FAIL] 101f: " << step << ": moved column not at " << idx << "\n";
                return false;
            }
            const QString old = moved->name();
            const QString renamed = old + QStringLiteral("r");
            moved->setName(renamed);
            QCoreApplication::processEvents();
            if (!header(idx).contains(renamed)) {
                std::cerr << "[FAIL] 101f: " << step << ": rename did not reach the header ("
                          << header(idx).toStdString() << ")\n";
                return false;
            }
            moved->setName(old);
            moved->setPlotDesignation(SciDAVis::Z);
            QCoreApplication::processEvents();
            const bool zShown = header(idx).contains(QStringLiteral("[Z"));
            moved->setPlotDesignation(SciDAVis::X);
            QCoreApplication::processEvents();
            if (!zShown) {
                std::cerr << "[FAIL] 101f: " << step
                          << ": designation change did not reach the header\n";
                return false;
            }
            dataSignals = 0;
            moved->setValueAt(1, moved->valueAt(1) + 1.0);
            if (dataSignals != perChange) {
                std::cerr << "[FAIL] 101f: " << step << ": a cell change gave " << dataSignals
                          << " dataChanged signals (expected " << perChange << ")\n";
                return false;
            }
            return true;
        };

        const int baseIndex = us->index();
        t->d_future_table->moveColumn(0, 2);
        QCoreApplication::processEvents();
        const int afterMove = us->index();
        CHECK_COND(checkConnected(2, "after the move"), "101f: after the move");
        // undo the checks' own commands, then the move itself
        while (us->index() > afterMove)
            us->undo();
        us->undo();
        QCoreApplication::processEvents();
        CHECK_COND(us->index() == baseIndex, "101f: undo stack index " << us->index());
        CHECK_COND(checkConnected(0, "after Undo of the move"), "101f: after Undo of the move");
        // the checks pushed commands, so the move's redo is gone: move again and use redo
        t->d_future_table->moveColumn(0, 2);
        const int afterMove2 = us->index();
        us->undo();
        QCoreApplication::processEvents();
        CHECK_COND(t->column(0) == moved, "101f: undo of the second move");
        us->redo();
        QCoreApplication::processEvents();
        CHECK_COND(us->index() == afterMove2, "101f: redo index");
        CHECK_COND(checkConnected(2, "after Redo of the move"), "101f: after Redo of the move");
        std::cout << "[PASS] 101f: the moved column stays connected through Undo / Redo\n";
    }

    // -------------------------------------------------------------------------
    // 101g: opening a project with a 2000-column table
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        CHECK_COND(tmp.isValid(), "101g: no temporary directory");
        const int nCols = 2000;
        const int nRows = 5;
        Table *t = w101->newTable(nRows, nCols, QStringLiteral("W101"), QStringLiteral("Wide101"));
        CHECK_COND(t != nullptr && t->numCols() == nCols, "101g: newTable failed");
        for (int c = 0; c < nCols; ++c) {
            QVector<qreal> v(nRows);
            for (int r = 0; r < nRows; ++r)
                v[r] = c * 10.0 + r;
            t->column(c)->replaceValues(0, v);
        }
        t->column(5)->setName(QStringLiteral("Five"));
        t->column(1999)->setPlotDesignation(SciDAVis::Z);
        t->d_future_table->setColumnWidth(7, 123);
        const QString path = tmp.filePath(QStringLiteral("wide101.sciprj"));
        QJsonObject r = rpc(QStringLiteral("app.save_project"), QJsonObject{ { "path", path } });
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("ok")).toBool(),
                   "101g: save_project failed: " << errText(r));
        QElapsedTimer timer;
        timer.start();
        r = rpc(QStringLiteral("app.open_project"),
                QJsonObject{ { "path", path }, { "force", true } });
        const qint64 ms = timer.elapsed();
        CHECK_COND(!isErr(r) && res(r).value(QStringLiteral("ok")).toBool(),
                   "101g: open_project failed: " << errText(r));
        std::cout << "[INFO] 101g: opening " << nCols << " columns x " << nRows << " rows took "
                  << ms << " ms\n";
        CHECK_COND(ms < 8000, "101g: opening took " << ms << " ms (was ~18 s)");
        Table *lt = w101->table(QStringLiteral("W101"));
        CHECK_COND(lt != nullptr, "101g: table W101 not found after open");
        CHECK_COND(lt->numCols() == nCols && lt->numRows() == nRows,
                   "101g: " << lt->numCols() << " x " << lt->numRows() << " after open");
        CHECK_COND(lt->column(5)->name() == QStringLiteral("Five"), "101g: column name lost");
        CHECK_COND(lt->column(1999)->plotDesignation() == SciDAVis::Z, "101g: designation lost");
        CHECK_COND(lt->d_future_table->columnWidth(7) == 123,
                   "101g: column width " << lt->d_future_table->columnWidth(7));
        for (int c = 0; c < nCols; c += 97)
            for (int row = 0; row < nRows; ++row)
                CHECK_COND(lt->column(c)->valueAt(row) == c * 10.0 + row,
                           "101g: value (" << row << ", " << c << ") = "
                                           << lt->column(c)->valueAt(row));
        CHECK_COND(lt->d_future_table->headerData(1999, Qt::Horizontal, Qt::DisplayRole)
                           .toString()
                           .contains(QStringLiteral("[Z")),
                   "101g: header of the last column");
        std::cout << "[PASS] 101g: a 2000-column project opens with its columns added at once\n";
    }

    std::cout << "[PASS] Suite 101 passed.\n";
    return 0;
}
