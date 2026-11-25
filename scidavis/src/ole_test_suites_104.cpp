#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <memory>

// =============================================================================
// Suite 104: text import, round-3 findings (rows lost or values cut apart). Every case is
// checked on the GUI path (AsciiTableImportFilter::importDroppedTextData, as File > Open /
// drag & drop / the command line read a file) and on the JSON-RPC app.import_csv path.
//   104a several outlier rows at the head of a value column ("0,<0.01" twice, "1,N.A." twice,
//        "0,\"12,345\"" three times) keep the header and every row; nothing goes to a preamble
//   104b a TAB-separated table with decimal commas ("0,50<TAB>1,234", .txt and .tsv) keeps the
//        TAB as separator: the header stays, no cell is cut at its comma
//   104c a table whose rows get wider further down ("t,T1 / 0,20 / 2,22,32", "a,b / 1,2 /
//        5,6,7 / 8,9,10,11 / 12", "1,2 / 3,4,5"), and blocks separated by a blank line: the GUI
//        keeps every row as app.import_csv does (no preamble of number rows)
// =============================================================================

namespace {
bool rpcIsError104(const QJsonObject &resp)
{
    return resp.contains(QStringLiteral("error"));
}
QJsonObject rpcResult104(const QJsonObject &resp)
{
    return resp.value(QStringLiteral("result")).toObject();
}
} // namespace

int runSuite104ImportRound3(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 104: text import, round-3 findings...\n";

    struct NonInteractiveQuitGuard104
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard104()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard104()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard104;

    struct AppWinGuard104
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard104(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard104() { reset(); }
        AppWinGuard104(const AppWinGuard104 &) = delete;
        AppWinGuard104 &operator=(const AppWinGuard104 &) = delete;
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

    AppWinGuard104 w104(new ApplicationWindow());
    w104->applyUserSettings();
    w104->confirmCloseTable = false;
    w104->confirmCloseMatrix = false;
    w104->confirmClosePlot2D = false;
    w104->confirmClosePlot3D = false;
    w104->confirmCloseFolder = false;
    w104->resize(1280, 800);
    w104->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w104->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "104: pythonAutomationServer null");

    QTemporaryDir tmp104;
    CHECK_COND(tmp104.isValid(), "104: temporary directory");

    int rpcId = 10400;
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
        const QString path = tmp104.filePath(name);
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
    struct Col104
    {
        QString name;
        bool numeric = false;
        QVector<double> nums;
        QStringList texts;
        int rows() const { return numeric ? nums.size() : texts.size(); }
    };
    const double NaN = std::numeric_limits<double>::quiet_NaN();

    auto rpcImport = [&](const QString &path) {
        QVector<Col104> out;
        QJsonObject params;
        params[QStringLiteral("path")] = path;
        const QJsonObject r = rpc(QStringLiteral("app.import_csv"), params);
        if (rpcIsError104(r))
            return out;
        const QString tbl = rpcResult104(r).value(QStringLiteral("name")).toString();
        const QJsonArray cols = rpcResult104(rpc(QStringLiteral("table.get_info"),
                                                 QJsonObject { { "table", tbl } }))
                                        .value(QStringLiteral("columns"))
                                        .toArray();
        for (const QJsonValue &cv : cols) {
            const QJsonObject c = cv.toObject();
            Col104 col;
            col.name = c.value(QStringLiteral("name")).toString();
            col.numeric = c.value(QStringLiteral("mode"))
                                  .toString()
                                  .compare(QStringLiteral("numeric"), Qt::CaseInsensitive)
                    == 0;
            const QJsonArray data =
                    rpcResult104(rpc(QStringLiteral("table.get_column_data"),
                                     QJsonObject { { "table", tbl }, { "col", col.name } }))
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
    auto guiImport = [&](const QString &path, AsciiTableImportFilter::ImportReport *rep = nullptr) {
        QVector<Col104> out;
        std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                readFile(path), QStringLiteral("Gui104"), path, rep));
        if (!ft)
            return out;
        for (int c = 0; c < ft->columnCount(); ++c) {
            Column *src = ft->column(c);
            Col104 col;
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
    auto dumpCols = [](const QVector<Col104> &cols) {
        QStringList s;
        for (const Col104 &c : cols) {
            QStringList v;
            if (c.numeric) {
                for (double d : c.nums)
                    v << (std::isnan(d) ? QStringLiteral("NaN") : QString::number(d));
            } else {
                v = c.texts;
            }
            s << QStringLiteral("%1[%2]{%3}")
                            .arg(c.name, c.numeric ? QStringLiteral("num") : QStringLiteral("txt"),
                                 v.join(QLatin1Char('|')));
        }
        return s.join(QStringLiteral(" ")).toStdString();
    };
    // Expected cell as written in the file ("" = empty). A text column must hold it (trimmed);
    // a numeric column the number it reads as -- with a decimal comma ("0,50" -> 0.5) or
    // thousands separators ("12,345" -> 12345) when those were read -- or NaN for a cell that is
    // not a number ("<0.01", "N.A.", "x"); no cell may be lost or cut apart.
    auto cellMatches = [](const Col104 &col, int r, const QString &cell) -> bool {
        if (!col.numeric)
            return r < col.texts.size() && col.texts[r].trimmed() == cell;
        if (r >= col.nums.size())
            return false;
        const double got = col.nums[r];
        bool ok = false;
        const double plain = cell.toDouble(&ok);
        auto closeTo = [got](double want) {
            return !std::isnan(got)
                    && std::fabs(got - want) <= 1e-9 * std::max(1.0, std::fabs(want));
        };
        if (ok)
            return closeTo(plain);
        bool okDec = false;
        const double dec =
                QString(cell).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&okDec);
        bool okThou = false;
        const double thou = QString(cell).remove(QLatin1Char(',')).toDouble(&okThou);
        if (okDec || okThou) // a decimal comma / thousands value: read as such, never dropped
            return (okDec && closeTo(dec)) || (okThou && closeTo(thou));
        return std::isnan(got);
    };
    struct ExpCol104
    {
        QString name; // empty: not checked (generated name of a headerless column)
        QStringList cells;
        int mustBeNumeric = -1; // 1: numeric, 0: text, -1: either
    };
    auto colsMatch = [&](const QVector<Col104> &cols, const QVector<ExpCol104> &exp) -> bool {
        if (cols.size() != exp.size())
            return false;
        for (int c = 0; c < exp.size(); ++c) {
            const Col104 &col = cols[c];
            if (!exp[c].name.isEmpty() && col.name != exp[c].name)
                return false;
            if (exp[c].mustBeNumeric >= 0 && col.numeric != (exp[c].mustBeNumeric == 1))
                return false;
            // trailing empty cells of a shorter column are not stored as rows
            if (col.rows() > exp[c].cells.size())
                return false;
            for (int r = 0; r < exp[c].cells.size(); ++r) {
                if (r >= col.rows()) {
                    if (!exp[c].cells[r].isEmpty())
                        return false;
                    continue;
                }
                if (exp[c].cells[r].isEmpty()) {
                    if (col.numeric ? !std::isnan(col.nums[r]) : !col.texts[r].trimmed().isEmpty())
                        return false;
                    continue;
                }
                if (!cellMatches(col, r, exp[c].cells[r]))
                    return false;
            }
        }
        return true;
    };
    auto split = [](const char *s) { return QString::fromLatin1(s).split(QLatin1Char('|')); };

    struct Case104
    {
        const char *tag;
        const char *file;
        QByteArray bytes;
        QVector<ExpCol104> cols;
        QString separator; // expected GUI separator (empty: not checked)
    };
    int failures = 0;
    int failuresBefore = 0;
    auto reportSection = [&](const char *what) {
        if (failures == failuresBefore)
            std::cout << "  [OK] " << what << "\n";
        else
            std::cerr << "[FAIL] " << what << ": " << (failures - failuresBefore)
                      << " failure(s)\n";
        failuresBefore = failures;
    };
    auto runCases = [&](const QVector<Case104> &cases) {
        for (const Case104 &cs : cases) {
            const QString path = writeFile(QString::fromLatin1(cs.file), cs.bytes);
            AsciiTableImportFilter::ImportReport rep;
            const QVector<Col104> gui = guiImport(path, &rep);
            if (!colsMatch(gui, cs.cols)) {
                std::cerr << "[FAIL] 104" << cs.tag << " (GUI) " << cs.file << ": " << dumpCols(gui)
                          << "\n";
                ++failures;
            }
            if (!rep.preamble.isEmpty()) {
                std::cerr << "[FAIL] 104" << cs.tag << " (GUI) " << cs.file
                          << ": rows set aside as a preamble '" << rep.preamble.toStdString()
                          << "'\n";
                ++failures;
            }
            if (!cs.separator.isNull() && rep.separator != cs.separator) {
                std::cerr << "[FAIL] 104" << cs.tag << " (GUI) " << cs.file << ": separator '"
                          << rep.separator.toStdString() << "'\n";
                ++failures;
            }
            if (AsciiTableImportFilter::hasDataPreamble(cs.bytes, path)) {
                std::cerr << "[FAIL] 104" << cs.tag << " " << cs.file
                          << ": hasDataPreamble() is true\n";
                ++failures;
            }
            const QVector<Col104> viaRpc = rpcImport(path);
            std::cout << "    104" << cs.tag << " GUI " << dumpCols(gui) << "\n"
                      << "    104" << cs.tag << " RPC " << dumpCols(viaRpc) << "\n";
            if (!colsMatch(viaRpc, cs.cols)) {
                std::cerr << "[FAIL] 104" << cs.tag << " (RPC) " << cs.file << ": "
                          << dumpCols(viaRpc) << "\n";
                ++failures;
            }
        }
    };

    // -------------------------------------------------------------------------
    // 104a: several outlier rows at the head of a value column
    // -------------------------------------------------------------------------
    runCases({
            { "a1",
              "s_lod_first.csv",
              "t_h,H_ppm\n0,<0.01\n1,<0.01\n2,0.05\n3,0.12\n4,0.20\n5,0.31\n",
              { { "t_h", split("0|1|2|3|4|5"), 1 },
                { "H_ppm", split("<0.01|<0.01|0.05|0.12|0.20|0.31") } } },
            { "a2",
              "s_ct_na_first.csv",
              "Slice,Vf\n1,N.A.\n2,N.A.\n3,0.52\n4,0.55\n5,0.53\n6,0.51\n7,0.50\n",
              { { "Slice", split("1|2|3|4|5|6|7"), 1 },
                { "Vf", split("N.A.|N.A.|0.52|0.55|0.53|0.51|0.50") } } },
            { "a3",
              "s_decay_thou.csv",
              "Time,Counts\n0,\"12,345\"\n1,\"5,432\"\n2,\"1,234\"\n3,987\n4,654\n5,432\n6,321\n"
              "7,210\n",
              { { "Time", split("0|1|2|3|4|5|6|7"), 1 },
                { "Counts", split("12,345|5,432|1,234|987|654|432|321|210") } } },
            // three rows, TAB separated, with a unit in the header
            { "a4",
              "lod3.txt",
              "Time (h)\tH (ppm)\n0\t<0.01\n1\t<0.01\n2\t<0.01\n3\t0.12\n4\t0.20\n",
              { { "Time", split("0|1|2|3|4"), 1 },
                { "H", split("<0.01|<0.01|<0.01|0.12|0.20") } } },
            // a single outlier row stays as before (e2e13af2, 103j6e)
            { "a5",
              "t_lod_one.csv",
              "t_h,H_ppm\n0,<0.01\n1,0.03\n2,0.05\n3,0.12\n4,0.20\n5,0.31\n",
              { { "t_h", split("0|1|2|3|4|5"), 1 },
                { "H_ppm", split("<0.01|0.03|0.05|0.12|0.20|0.31") } } },
    });
    // a name over the numeric first column is still a header with a preamble above it, also
    // above two outlier rows ("Month,2024" under "Operator,Tanaka", 103j6d)
    {
        const QByteArray bytes = "Operator,Tanaka\nMonth,2024\n1,100\n2,200\n3,300\n";
        const QString path = writeFile(QStringLiteral("month.csv"), bytes);
        AsciiTableImportFilter::ImportReport rep;
        const QVector<Col104> gui = guiImport(path, &rep);
        if (!(rep.preamble == QLatin1String("Operator,Tanaka")
              && colsMatch(
                      gui,
                      { { "Month", split("1|2|3"), 1 }, { "2024", split("100|200|300"), 1 } }))) {
            std::cerr << "[FAIL] 104a6 (GUI): preamble '" << rep.preamble.toStdString() << "' "
                      << dumpCols(gui) << "\n";
            ++failures;
        }
    }
    reportSection("104a outlier rows at the head of a value column");

    // -------------------------------------------------------------------------
    // 104b: TAB-separated table with decimal commas
    // -------------------------------------------------------------------------
    {
        QByteArray de2 = "Zeit\tKraft\n";
        QByteArray de3 = "Zeit\tKraft\tWeg\n";
        QStringList zeit, kraft, weg;
        for (int i = 0; i < 10; ++i) {
            const QString z =
                    QString::number(i * 0.5, 'f', 2).replace(QLatin1Char('.'), QLatin1Char(','));
            const QString k =
                    QString::number(i * 1.234, 'f', 3).replace(QLatin1Char('.'), QLatin1Char(','));
            const QString w =
                    QString::number(i * 0.01, 'f', 4).replace(QLatin1Char('.'), QLatin1Char(','));
            zeit << z;
            kraft << k;
            weg << w;
            de2 += (z + QLatin1Char('\t') + k + QLatin1Char('\n')).toLatin1();
            de3 += (z + QLatin1Char('\t') + k + QLatin1Char('\t') + w + QLatin1Char('\n'))
                           .toLatin1();
        }
        const QString tab = QStringLiteral("\t");
        runCases({
                { "b1", "s_de_tab2.txt", de2, { { "Zeit", zeit }, { "Kraft", kraft } }, tab },
                { "b2", "s_de_tab2.tsv", de2, { { "Zeit", zeit }, { "Kraft", kraft } }, tab },
                { "b3",
                  "s_de_tab3.txt",
                  de3,
                  { { "Zeit", zeit }, { "Kraft", kraft }, { "Weg", weg } },
                  tab },
                { "b4",
                  "t_de_tab_neg.txt",
                  "Zeit\tKraft\n0,5\t-1,25\n1,0\t-2,50\n1,5\t-3,75\n2,0\t-5,00\n",
                  { { "Zeit", split("0,5|1,0|1,5|2,0") },
                    { "Kraft", split("-1,25|-2,50|-3,75|-5,00") } },
                  tab },
        });
    }
    reportSection("104b TAB-separated decimal-comma table keeps the TAB");

    // -------------------------------------------------------------------------
    // 104c: rows that get wider further down; blocks separated by blank lines
    // -------------------------------------------------------------------------
    runCases({
            { "c1",
              "t_grow_short.csv",
              "t,T1\n0,20\n1,21\n2,22,32\n3,23,33\n4,24,34\n5,25,35\n",
              { { "t", split("0|1|2|3|4|5"), 1 },
                { "T1", split("20|21|22|23|24|25"), 1 },
                { "", split("||32|33|34|35"), 1 } } },
            { "c2",
              "cols_change.csv",
              "a,b\n1,2\n3,4\n5,6,7\n8,9,10,11\n12\n",
              { { "a", split("1|3|5|8|12"), 1 },
                { "b", split("2|4|6|9|"), 1 },
                { "", split("||7|10|"), 1 },
                { "", split("|||11|"), 1 } } },
            { "c3",
              "cols_grow_first.csv",
              "1,2\n3,4,5\n6,7,8\n",
              { { "", split("1|3|6"), 1 }, { "", split("2|4|7"), 1 }, { "", split("|5|8"), 1 } } },
            { "c4",
              "s_blocks2.dat",
              "x y\n1 10\n2 20\n\nx y\n1 11\n2 21\n3 31\n",
              { { "x", split("1|2|x|1|2|3") }, { "y", split("10|20|y|11|21|31") } } },
    });
    reportSection("104c rows getting wider further down keep every row (GUI as RPC)");
    // every section runs, so that one failing case does not hide the others
    CHECK_COND(failures == 0, "104: " << failures << " failure(s)");

    std::cout << "[PASS] Suite 104: text import round-3 findings passed!\n";
    return 0;
}
