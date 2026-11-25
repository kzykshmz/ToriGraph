#include "ole_test_common.h"
#include "PythonAutomationServer.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextCodec>

#include <cmath>
#include <memory>

// =============================================================================
// Suite 103: text import, second round of data-driven findings. Every case is checked on the
// GUI path (AsciiTableImportFilter::importDroppedTextData, as File > Open / drag & drop / the
// command line read a file) and on the JSON-RPC app.import_csv path.
//   103a a missing value in the first data row ("0,", "0,NaN", "1<TAB>inf", sparse / interleaved
//        columns, a "; pause" line) keeps the header and every row
//   103b metadata preamble: app.import_csv reads tensile tester (Shift-JIS) / "; Instrument"
//        CSVs like the GUI import; a CSV without preamble keeps the RPC import
//   103c encoding=auto: Latin-1 / Windows-1252 / EUC-JP are not read as Shift-JIS; Shift-JIS
//        files with kanji or half-width katakana stay Shift-JIS
//   103d units "mm2", "mm3", "N/mm2", "kgf/mm2", "mm2/s", "1/s" split off the column name
//   103e "Infinity", "-Infinity" and an overflowing "1e400" are missing values of a numeric
//        column
//   103f Fortran exponents ("1.0D+00", "3.5d+02") are numbers; "2D3" stays text
//   103g full-width digits ("１", "２．５", "－３") are numbers, no row becomes header / units
//   103h headerless file whose first row holds NaN; a separator at the end of every line does
//        not add an empty column (import_csv, GUI; paste_tsv keeps the pasted range, 103j3)
//   103i plotFile() of a headless window gives a headless, hidden window
//   103j regressions of the round-2 fixes: a table with one numeric column and a text column
//        that is empty further down keeps its header and first rows (GUI and RPC); paste_tsv
//        with an empty last column clears the cells it is pasted over; "3D-1" (no point) is
//        an identifier, not a Fortran number; nearly-UTF-8 text with one stray Latin-1 byte
//        stays UTF-8 with a warning
// =============================================================================

namespace {
bool rpcIsError103(const QJsonObject &resp)
{
    return resp.contains(QStringLiteral("error"));
}
QJsonObject rpcResult103(const QJsonObject &resp)
{
    return resp.value(QStringLiteral("result")).toObject();
}
QByteArray encode103(const QString &text, const char *codecName)
{
    QTextCodec *codec = QTextCodec::codecForName(codecName);
    return codec ? codec->fromUnicode(text) : text.toUtf8();
}
} // namespace

int runSuite103ImportRound2(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 103: text import, round-2 data-driven findings...\n";

    struct NonInteractiveQuitGuard103
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard103()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard103()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard103;

    struct AppWinGuard103
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard103(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard103() { reset(); }
        AppWinGuard103(const AppWinGuard103 &) = delete;
        AppWinGuard103 &operator=(const AppWinGuard103 &) = delete;
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

    AppWinGuard103 w103(new ApplicationWindow());
    w103->applyUserSettings();
    w103->confirmCloseTable = false;
    w103->confirmCloseMatrix = false;
    w103->confirmClosePlot2D = false;
    w103->confirmClosePlot3D = false;
    w103->confirmCloseFolder = false;
    w103->resize(1280, 800);
    w103->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w103->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "103: pythonAutomationServer null");

    QTemporaryDir tmp103;
    CHECK_COND(tmp103.isValid(), "103: temporary directory");

    int rpcId = 10300;
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
        const QString path = tmp103.filePath(name);
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
    struct Col103
    {
        QString name, longName, units;
        bool numeric = false;
        QVector<double> nums;
        QStringList texts;
    };
    auto nanOf = [] { return std::numeric_limits<double>::quiet_NaN(); };

    // RPC app.import_csv -> columns (empty on error)
    auto rpcImport = [&](const QString &path, QJsonObject params = QJsonObject()) {
        QVector<Col103> out;
        params[QStringLiteral("path")] = path;
        const QJsonObject r = rpc(QStringLiteral("app.import_csv"), params);
        if (rpcIsError103(r))
            return out;
        const QString tbl = rpcResult103(r).value(QStringLiteral("name")).toString();
        const QJsonArray cols = rpcResult103(rpc(QStringLiteral("table.get_info"),
                                                 QJsonObject { { "table", tbl } }))
                                        .value(QStringLiteral("columns"))
                                        .toArray();
        for (const QJsonValue &cv : cols) {
            const QJsonObject c = cv.toObject();
            Col103 col;
            col.name = c.value(QStringLiteral("name")).toString();
            col.longName = c.value(QStringLiteral("long_name")).toString();
            col.units = c.value(QStringLiteral("units")).toString();
            col.numeric = c.value(QStringLiteral("mode")).toString().compare(
                                  QStringLiteral("numeric"), Qt::CaseInsensitive)
                    == 0;
            const QJsonArray data = rpcResult103(rpc(QStringLiteral("table.get_column_data"),
                                                     QJsonObject { { "table", tbl },
                                                                   { "col", col.name } }))
                                            .value(QStringLiteral("data"))
                                            .toArray();
            for (const QJsonValue &v : data) {
                if (col.numeric)
                    col.nums << (v.isDouble() ? v.toDouble() : nanOf());
                else
                    col.texts << (v.isNull() ? QString() : v.toString());
            }
            out << col;
        }
        return out;
    };
    // GUI path (File > Open, drag & drop, command line) -> columns
    auto guiImport = [&](const QString &path) {
        QVector<Col103> out;
        std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                readFile(path), QStringLiteral("Gui103"), path));
        if (!ft)
            return out;
        for (int c = 0; c < ft->columnCount(); ++c) {
            Column *src = ft->column(c);
            Col103 col;
            col.name = src->name();
            col.longName = src->longName();
            col.units = src->units();
            col.numeric = src->columnMode() == SciDAVis::ColumnMode::Numeric;
            for (int r = 0; r < src->rowCount(); ++r) {
                if (col.numeric)
                    col.nums << (src->isInvalid(r) ? nanOf() : src->valueAt(r));
                else
                    col.texts << src->textAt(r);
            }
            out << col;
        }
        return out;
    };
    auto numsEqual = [](const QVector<double> &got, const QVector<double> &expected) {
        if (got.size() != expected.size())
            return false;
        for (int i = 0; i < expected.size(); ++i) {
            if (std::isnan(expected[i]) != std::isnan(got[i]))
                return false;
            if (!std::isnan(expected[i]) && std::fabs(got[i] - expected[i]) > 1e-12)
                return false;
        }
        return true;
    };
    auto dumpCols = [](const QVector<Col103> &cols) {
        QStringList s;
        for (const Col103 &c : cols) {
            QStringList v;
            if (c.numeric) {
                for (double d : c.nums)
                    v << (std::isnan(d) ? QStringLiteral("NaN") : QString::number(d));
            } else {
                v = c.texts;
            }
            s << QStringLiteral("%1[%2|%3|%4]{%5}")
                            .arg(c.name, c.longName, c.units,
                                 c.numeric ? QStringLiteral("num") : QStringLiteral("txt"),
                                 v.join(QLatin1Char(',')));
        }
        return s.join(QStringLiteral(" ")).toStdString();
    };
    // expected numeric columns by name; both paths are checked
    struct Expect103
    {
        QString name;
        QVector<double> nums;
        QString units;
    };
    auto checkBoth = [&](const char *tag, const QString &path, const QVector<Expect103> &exp,
                         bool checkGui = true, bool checkRpc = true) -> bool {
        for (int pass = 0; pass < 2; ++pass) {
            if ((pass == 0 && !checkGui) || (pass == 1 && !checkRpc))
                continue;
            const QVector<Col103> cols = (pass == 0) ? guiImport(path) : rpcImport(path);
            bool ok = cols.size() == exp.size();
            for (int i = 0; ok && i < exp.size(); ++i) {
                ok = cols[i].numeric && cols[i].name == exp[i].name
                        && numsEqual(cols[i].nums, exp[i].nums)
                        && (exp[i].units.isNull() || cols[i].units == exp[i].units);
            }
            if (!ok) {
                std::cerr << "[FAIL] 103" << tag << " (" << (pass == 0 ? "GUI" : "RPC")
                          << "): " << dumpCols(cols) << "\n";
                return false;
            }
        }
        return true;
    };
    const double NaN = nanOf();

    // -------------------------------------------------------------------------
    // 103a: missing value in the first data row (GUI path lost the header and first rows)
    // -------------------------------------------------------------------------
    {
        CHECK_COND(checkBoth("a1", writeFile(QStringLiteral("mf_empty.csv"),
                                             "Time,Load\n0,\n1,2.5\n2,3.5\n"),
                             { { "Time", { 0, 1, 2 } }, { "Load", { NaN, 2.5, 3.5 } } }),
                   "103a1: first data row '0,' dropped header / rows");
        CHECK_COND(checkBoth("a2", writeFile(QStringLiteral("mf_nan2.csv"),
                                             "Time,Load\n0,NaN\n1,NaN\n2,3.5\n3,4.5\n"),
                             { { "Time", { 0, 1, 2, 3 } }, { "Load", { NaN, NaN, 3.5, 4.5 } } }),
                   "103a2: 'NaN' in the first two data rows");
        CHECK_COND(checkBoth("a3", writeFile(QStringLiteral("mf_units.csv"),
                                             "Time (s),Load (N)\n0,-\n1,2.5\n2,3.5\n"),
                             { { "Time", { 0, 1, 2 }, "s" }, { "Load", { NaN, 2.5, 3.5 }, "N" } }),
                   "103a3: '-' in the first data row with 'Name (unit)' headers");
        CHECK_COND(checkBoth("a4", writeFile(QStringLiteral("mf_inf.txt"),
                                             "x\ty\n1\tinf\n2\t-inf\n3\t4.5\n"),
                             { { "x", { 1, 2, 3 } }, { "y", { NaN, NaN, 4.5 } } }),
                   "103a4: 'inf' in the first data row");
        CHECK_COND(checkBoth("a5", writeFile(QStringLiteral("sparse.tsv"),
                                             "Temp\tA\tB\n300\t1.5\t\n\t\t2.5\n320\t1.7\t2.6\n"),
                             { { "Temp", { 300, NaN, 320 } },
                               { "A", { 1.5, NaN, 1.7 } },
                               { "B", { NaN, 2.5, 2.6 } } }),
                   "103a5: sparse TSV kept only its last row");
        CHECK_COND(checkBoth("a6", writeFile(QStringLiteral("interleaved.csv"),
                                             "x1,y1,x2,y2\n1,10,,\n,,1.5,20\n2,11,,\n,,2.5,21\n"),
                             { { "x1", { 1, NaN, 2, NaN } },
                               { "y1", { 10, NaN, 11, NaN } },
                               { "x2", { NaN, 1.5, NaN, 2.5 } },
                               { "y2", { NaN, 20, NaN, 21 } } }),
                   "103a6: interleaved x/y column pairs");
        // a "; pause" line inside the data: the header and every row stay (the line itself is a
        // row with text in the first column, as app.import_csv reads it)
        const QString semiMid = writeFile(QStringLiteral("semi_mid.csv"),
                                          "Time,Load\n0,1.5\n; pause\n1,2.5\n");
        const QVector<Col103> pause = guiImport(semiMid);
        const QVector<Col103> pauseRpc = rpcImport(semiMid);
        CHECK_COND(pause.size() == 2 && pause[0].name == QLatin1String("Time")
                           && pause[1].name == QLatin1String("Load")
                           && numsEqual(pause[1].nums, { 1.5, NaN, 2.5 })
                           && dumpCols(pause) == dumpCols(pauseRpc),
                   "103a7: a '; pause' line inside the data: GUI " << dumpCols(pause) << " RPC "
                                                                    << dumpCols(pauseRpc));
    }
    std::cout << "  [OK] 103a missing value in the first data row (GUI = RPC)\n";

    // -------------------------------------------------------------------------
    // 103b: metadata preamble read by app.import_csv as by the GUI import
    // -------------------------------------------------------------------------
    {
        QStringList meta { QStringLiteral("[試験条件]"), QStringLiteral("試験名,AL-Zn-Mg_H2_01"),
                           QStringLiteral("試験日,2024/03/01"),
                           QStringLiteral("試験速度,1.0,mm/min"),
                           QStringLiteral("断面積,10.00,mm2"), QString(),
                           QStringLiteral("[データ]"), QStringLiteral("時間,試験力,ストローク"),
                           QStringLiteral("sec,kN,mm") };
        for (int i = 0; i < 5; ++i)
            meta << QStringLiteral("%1,%2,%3")
                            .arg(i * 0.1, 0, 'f', 1)
                            .arg(0.05 * i, 0, 'f', 4)
                            .arg(i * 0.01, 0, 'f', 3);
        const QString tensile = writeFile(
                QStringLiteral("tensile_sjis.csv"),
                encode103(meta.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n"),
                          "Windows-31J"));
        CHECK_COND(AsciiTableImportFilter::hasDataPreamble(readFile(tensile), tensile),
                   "103b: preamble of the tensile CSV not detected");
        CHECK_COND(checkBoth("b1", tensile,
                             { { QStringLiteral("時間"), { 0, 0.1, 0.2, 0.3, 0.4 }, "sec" },
                               { QStringLiteral("試験力"), { 0, 0.05, 0.1, 0.15, 0.2 }, "kN" },
                               { QStringLiteral("ストローク"),
                                 { 0, 0.01, 0.02, 0.03, 0.04 },
                                 "mm" } }),
                   "103b1: tensile tester CSV with a [試験条件] preamble");
        CHECK_COND(checkBoth("b2", writeFile(QStringLiteral("semi_comment.csv"),
                                             "; Instrument: XYZ\n; Date 2024-01-01\nTime,Load\n"
                                             "0,1.5\n1,2.5\n2,3.5\n"),
                             { { "Time", { 0, 1, 2 } }, { "Load", { 1.5, 2.5, 3.5 } } }),
                   "103b2: '; Instrument' preamble");
        const QString plain = writeFile(QStringLiteral("plain.csv"), "x,y\n1,2\n3,4\n");
        CHECK_COND(!AsciiTableImportFilter::hasDataPreamble(readFile(plain), plain),
                   "103b3: a plain CSV is said to have a preamble");
        CHECK_COND(checkBoth("b3", plain, { { "x", { 1, 3 } }, { "y", { 2, 4 } } }),
                   "103b3: plain CSV");
        // explicit options keep the RPC import (skip_lines over the same preamble)
        const QVector<Col103> skipped =
                rpcImport(tensile, QJsonObject { { "skip_lines", 7 }, { "units_row", true } });
        CHECK_COND(skipped.size() == 3 && skipped[0].name == QStringLiteral("時間")
                           && skipped[0].units == QLatin1String("sec") && skipped[0].nums.size() == 5,
                   "103b4: skip_lines over the preamble: " << dumpCols(skipped));
    }
    std::cout << "  [OK] 103b preamble detection shared by app.import_csv\n";

    // -------------------------------------------------------------------------
    // 103c: encoding=auto between Shift-JIS, EUC-JP and Windows-1252 / Latin-1
    // -------------------------------------------------------------------------
    {
        struct EncCase
        {
            const char *tag;
            QString text;
            const char *codec;
            const char *expectEncoding;
        };
        const QVector<EncCase> cases {
            { "c1", QStringLiteral("Temp (°C),Strain (µm/m)\n20,1.5\n"), "ISO-8859-1",
              "Windows-1252" },
            { "c2", QStringLiteral("T (°C),Länge (µm),Größe – x\n20,1.5,3\n"), "windows-1252",
              "Windows-1252" },
            { "c3", QStringLiteral("温度 (℃),応力 (MPa),試料名\n20,100,鋼材A\n"), "EUC-JP",
              "EUC-JP" },
            { "c4", QStringLiteral("時間,応力\n0,1.5\n"), "EUC-JP", "EUC-JP" },
            { "c5",
              QStringLiteral("試験名,試料①\n時間,試験力,ストローク,温度\nsec,kN,mm,℃\n0.0,0.0,0.0,25.1\n"),
              "Windows-31J", "Shift-JIS" },
            { "c6", QStringLiteral("温度,値\n1,2\n"), "Windows-31J", "Shift-JIS" },
            { "c7", QStringLiteral("ｼﾞｶﾝ,ﾁｶﾗ\n1,2\n"), "Windows-31J", "Shift-JIS" },
            { "c8", QStringLiteral("値\n1\n"), "Windows-31J", "Shift-JIS" },
            { "c9", QStringLiteral("x (ﾟC),y\n1,2\n"), "Windows-31J", "Shift-JIS" },
            { "c10", QStringLiteral("*FILE_COMMENT \"試料A 熱処理後\"\n"), "Windows-31J",
              "Shift-JIS" },
        };
        for (const EncCase &ec : cases) {
            const QByteArray raw = encode103(ec.text, ec.codec);
            const QString decoded = AsciiTableImportFilter::decodeLabTextAuto(raw);
            const QString path = writeFile(QStringLiteral("enc_%1.csv").arg(ec.tag), raw);
            AsciiTableImportFilter::ImportReport rep;
            std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                    raw, QStringLiteral("Enc103"), path, &rep));
            CHECK_COND(decoded == ec.text && rep.encoding == QLatin1String(ec.expectEncoding),
                       "103" << ec.tag << ": auto encoding gave " << rep.encoding.toStdString()
                             << " / '" << decoded.left(40).toStdString() << "'");
        }
        // RPC: same names / units as the explicit encoding
        const QString latin = writeFile(QStringLiteral("latin1.csv"),
                                        encode103(QStringLiteral("Temp (°C),Strain (µm/m)\n20,1.5\n25,2.5\n"),
                                                  "ISO-8859-1"));
        const QVector<Col103> rl = rpcImport(latin);
        CHECK_COND(rl.size() == 2 && rl[0].units == QStringLiteral("°C")
                           && rl[1].units == QStringLiteral("µm/m"),
                   "103c: RPC Latin-1 units: " << dumpCols(rl));
        const QString euc = writeFile(
                QStringLiteral("eucjp.csv"),
                encode103(QStringLiteral("温度 (℃),応力 (MPa),試料名\n20,100,鋼材A\n25,110,鋼材B\n"),
                          "EUC-JP"));
        const QVector<Col103> re = rpcImport(euc);
        CHECK_COND(re.size() == 3 && re[0].name == QStringLiteral("温度")
                           && re[0].units == QStringLiteral("℃") && !re[2].numeric
                           && re[2].texts.value(0) == QStringLiteral("鋼材A"),
                   "103c: RPC EUC-JP: " << dumpCols(re));
    }
    std::cout << "  [OK] 103c encoding=auto Latin-1 / Windows-1252 / EUC-JP / Shift-JIS\n";

    // -------------------------------------------------------------------------
    // 103d: units mm2 / mm3 / N/mm2 / kgf/mm2 / mm2/s / 1/s
    // -------------------------------------------------------------------------
    {
        const QStringList hdr { QStringLiteral("Stress (N/mm2)"), QStringLiteral("応力 (N/mm2)"),
                                QStringLiteral("Area (mm2)"),     QStringLiteral("Vol (mm3)"),
                                QStringLiteral("Hv (kgf/mm2)"),   QStringLiteral("D (mm2/s)"),
                                QStringLiteral("rate (1/s)"),     QStringLiteral("rho (g/cm3)") };
        const QStringList expName { QStringLiteral("Stress"), QStringLiteral("応力"),
                                    QStringLiteral("Area"),   QStringLiteral("Vol"),
                                    QStringLiteral("Hv"),     QStringLiteral("D"),
                                    QStringLiteral("rate"),   QStringLiteral("rho") };
        const QStringList expUnit { QStringLiteral("N/mm2"),   QStringLiteral("N/mm2"),
                                    QStringLiteral("mm2"),     QStringLiteral("mm3"),
                                    QStringLiteral("kgf/mm2"), QStringLiteral("mm2/s"),
                                    QStringLiteral("1/s"),     QStringLiteral("g/cm3") };
        for (int i = 0; i < hdr.size(); ++i) {
            QString sn, ln, un;
            AsciiTableImportFilter::splitHeaderNameAndUnits(hdr[i], sn, ln, un);
            CHECK_COND(ln == expName[i] && un == expUnit[i],
                       "103d: '" << hdr[i].toStdString() << "' -> '" << ln.toStdString()
                                 << "' / '" << un.toStdString() << "'");
        }
        QStringList vals;
        for (int i = 0; i < hdr.size(); ++i)
            vals << QString::number(i);
        const QString path = writeFile(
                QStringLiteral("units_mm2.csv"),
                (hdr.join(QLatin1Char(',')) + QLatin1Char('\n') + vals.join(QLatin1Char(','))
                 + QLatin1Char('\n'))
                        .toUtf8());
        for (int pass = 0; pass < 2; ++pass) {
            const QVector<Col103> cols = (pass == 0) ? guiImport(path) : rpcImport(path);
            bool ok = cols.size() == hdr.size();
            for (int i = 0; ok && i < hdr.size(); ++i)
                ok = cols[i].name == expName[i] && cols[i].units == expUnit[i];
            CHECK_COND(ok, "103d (" << (pass == 0 ? "GUI" : "RPC") << "): " << dumpCols(cols));
        }
        CHECK_COND(!AsciiTableImportFilter::looksLikeUnitCell(QStringLiteral("1/2")),
                   "103d: '1/2' taken for a unit");
    }
    std::cout << "  [OK] 103d units mm2 / mm3 / 1/s split off (GUI = RPC)\n";

    // -------------------------------------------------------------------------
    // 103e: Infinity / -Infinity / overflow are missing values of a numeric column
    // -------------------------------------------------------------------------
    {
        CHECK_COND(checkBoth("e1", writeFile(QStringLiteral("infinity.csv"),
                                             "x,y\n1,4.5\n2,-Infinity\n3,5.5\n4,Infinity\n"),
                             { { "x", { 1, 2, 3, 4 } }, { "y", { 4.5, NaN, 5.5, NaN } } }),
                   "103e1: Infinity turned the column into text");
        CHECK_COND(checkBoth("e2", writeFile(QStringLiteral("overflow.csv"),
                                             "x,y\n1,4.5\n2,1e400\n3,5.5\n4,-1.0000000000000000e+999\n"),
                             { { "x", { 1, 2, 3, 4 } }, { "y", { 4.5, NaN, 5.5, NaN } } }),
                   "103e2: 1e400 turned the column into text");
        CHECK_COND(AsciiTableImportFilter::isOverflowNumberToken(QStringLiteral("1e400"))
                           && !AsciiTableImportFilter::isOverflowNumberToken(QStringLiteral("1e300"))
                           && !AsciiTableImportFilter::isOverflowNumberToken(QStringLiteral("1e-400"))
                           && !AsciiTableImportFilter::isOverflowNumberToken(QStringLiteral("abc")),
                   "103e3: isOverflowNumberToken");
    }
    std::cout << "  [OK] 103e Infinity / overflow read as missing values\n";

    // -------------------------------------------------------------------------
    // 103f: Fortran D exponents
    // -------------------------------------------------------------------------
    {
        CHECK_COND(checkBoth("f1", writeFile(QStringLiteral("fortran_hdr.dat"),
                                             "x y\n1.0D+00 2.5D-01\n2.0D+00 3.5d+02\n"),
                             { { "x", { 1, 2 } }, { "y", { 0.25, 350 } } }),
                   "103f1: Fortran exponents with a header");
        const QString headerless = writeFile(
                QStringLiteral("fortran.dat"),
                "   1.0D+00   2.5D-01\n   2.0D+00   3.5d+02\n   3.0E+00   4.5E+00\n");
        for (int pass = 0; pass < 2; ++pass) {
            const QVector<Col103> cols =
                    (pass == 0) ? guiImport(headerless) : rpcImport(headerless);
            CHECK_COND(cols.size() == 2 && cols[0].numeric && cols[1].numeric
                               && numsEqual(cols[0].nums, { 1, 2, 3 })
                               && numsEqual(cols[1].nums, { 0.25, 350, 4.5 }),
                       "103f2 (" << (pass == 0 ? "GUI" : "RPC")
                                 << "): headerless Fortran file lost rows: " << dumpCols(cols));
        }
        double v = 0.0;
        CHECK_COND(AsciiTableImportFilter::parseNumberToken(QStringLiteral("1.0D+00"), v)
                           && v == 1.0
                           && AsciiTableImportFilter::parseNumberToken(QStringLiteral("-2.5d-3"), v)
                           && v == -2.5e-3
                           && AsciiTableImportFilter::parseNumberToken(QStringLiteral("1.D5"), v)
                           && v == 1e5
                           && !AsciiTableImportFilter::parseNumberToken(QStringLiteral("2D+3"), v)
                           && !AsciiTableImportFilter::parseNumberToken(QStringLiteral("3D-1"), v)
                           && !AsciiTableImportFilter::parseNumberToken(QStringLiteral("2D3"), v)
                           && !AsciiTableImportFilter::parseNumberToken(QStringLiteral("D+3"), v)
                           && !AsciiTableImportFilter::parseNumberToken(QStringLiteral("1.0D"), v),
                   "103f3: parseNumberToken Fortran exponents");
    }
    std::cout << "  [OK] 103f Fortran D exponents\n";

    // -------------------------------------------------------------------------
    // 103g: full-width digits
    // -------------------------------------------------------------------------
    {
        CHECK_COND(checkBoth("g1", writeFile(QStringLiteral("zenkaku.csv"),
                                             QStringLiteral("x,y\n１,２．５\n２,－３\n3,4\n").toUtf8()),
                             { { "x", { 1, 2, 3 } }, { "y", { 2.5, -3, 4 } } }),
                   "103g1: full-width digits");
        double v = 0.0;
        CHECK_COND(AsciiTableImportFilter::parseNumberToken(QStringLiteral("１．５Ｅ＋０２"), v)
                           && v == 150.0
                           && !AsciiTableImportFilter::parseNumberToken(QStringLiteral("１ａ"), v),
                   "103g2: parseNumberToken full-width");
    }
    std::cout << "  [OK] 103g full-width digits\n";

    // -------------------------------------------------------------------------
    // 103h: headerless NaN first row; separator at the end of every line
    // -------------------------------------------------------------------------
    {
        const QString nanFirst =
                writeFile(QStringLiteral("nan_first.dat"), "NaN 1\n2 3\n4 5\n");
        for (int pass = 0; pass < 2; ++pass) {
            const QVector<Col103> cols = (pass == 0) ? guiImport(nanFirst) : rpcImport(nanFirst);
            CHECK_COND(cols.size() == 2 && numsEqual(cols[0].nums, { NaN, 2, 4 })
                               && numsEqual(cols[1].nums, { 1, 3, 5 }),
                       "103h1 (" << (pass == 0 ? "GUI" : "RPC")
                                 << "): headerless NaN first row: " << dumpCols(cols));
        }
        CHECK_COND(checkBoth("h2", writeFile(QStringLiteral("trailing_sep.csv"),
                                             "x,y,\n1,2,\n3,4,\n"),
                             { { "x", { 1, 3 } }, { "y", { 2, 4 } } }),
                   "103h2: separator at the end of every line added an empty column");
        // a named last column stays, even if it is empty
        const QVector<Col103> named = rpcImport(
                writeFile(QStringLiteral("named_empty.csv"), "x,y,z\n1,2,\n3,4,\n"));
        CHECK_COND(named.size() == 3 && named[2].name == QLatin1String("z"),
                   "103h3: named empty last column dropped: " << dumpCols(named));
        const QVector<Col103> namedGui = guiImport(tmp103.filePath(QStringLiteral("named_empty.csv")));
        CHECK_COND(namedGui.size() == 3 && namedGui[2].name == QLatin1String("z"),
                   "103h3 (GUI): named empty last column dropped: " << dumpCols(namedGui));
        // (table.paste_tsv keeps the empty last column of the pasted range: 103j3)
    }
    std::cout << "  [OK] 103h headerless NaN first row / trailing separators\n";

    // -------------------------------------------------------------------------
    // 103i: plotFile() of a headless window stays headless and hidden
    // -------------------------------------------------------------------------
    {
        const QString foo = writeFile(QStringLiteral("plain.foo"), "1 2\n3 4\n5 6\n");
        const bool prevHeadless = w103->isHeadlessMode();
        w103->setHeadlessMode(true);
        ApplicationWindow *other = w103->plotFile(foo);
        w103->setHeadlessMode(prevHeadless);
        CHECK_COND(other != nullptr, "103i: plotFile returned no window");
        const bool headless = other->isHeadlessMode();
        const bool visible = other->isVisible();
        other->savedProject();
        other->setOleEmbeddedMode(true);
        delete other;
        QApplication::processEvents();
        CHECK_COND(headless && !visible,
                   "103i: plotFile window headless=" << headless << " visible=" << visible);
    }
    std::cout << "  [OK] 103i plotFile keeps headless mode\n";

    // -------------------------------------------------------------------------
    // 103j: regressions of the round-2 fixes
    // -------------------------------------------------------------------------
    {
        // j1/j2: one numeric column and a text column that is empty further down. The rows
        // "2," / ",355" (numbers and missing cells only) are not the start of the data: the
        // header and the rows with text above them stay (GUI and RPC, no preamble).
        struct MixedCase
        {
            const char *tag;
            QByteArray bytes;
            QStringList names;
            int numCol;
            QVector<double> nums;
            QStringList texts;
        };
        const QVector<MixedCase> mixed {
            { "j1", "Time,Note\n0,start\n1,heated\n2,\n3,\n4,\n",
              { QStringLiteral("Time"), QStringLiteral("Note") }, 0, { 0, 1, 2, 3, 4 },
              { QStringLiteral("start"), QStringLiteral("heated"), QString(), QString(),
                QString() } },
            { "j2", "Sample,HV\nS1,350\nS2,352\n,355\n,351\n,349\n",
              { QStringLiteral("Sample"), QStringLiteral("HV") }, 1, { 350, 352, 355, 351, 349 },
              { QStringLiteral("S1"), QStringLiteral("S2"), QString(), QString(), QString() } },
            { "j2b", "Sample\tHV\r\nS1\t350\r\nS2\t352\r\n\t355\r\n\t351\r\n",
              { QStringLiteral("Sample"), QStringLiteral("HV") }, 1, { 350, 352, 355, 351 },
              { QStringLiteral("S1"), QStringLiteral("S2"), QString(), QString() } },
        };
        for (const MixedCase &mc : mixed) {
            const QString path = writeFile(
                    QStringLiteral("mixed_%1.%2")
                            .arg(QLatin1String(mc.tag),
                                 mc.bytes.contains('\t') ? QStringLiteral("txt")
                                                         : QStringLiteral("csv")),
                    mc.bytes);
            CHECK_COND(!AsciiTableImportFilter::hasDataPreamble(mc.bytes, path),
                       "103" << mc.tag << ": header and first rows taken for a preamble");
            for (int pass = 0; pass < 2; ++pass) {
                const QVector<Col103> cols = (pass == 0) ? guiImport(path) : rpcImport(path);
                const int txtCol = 1 - mc.numCol;
                bool ok = cols.size() == 2 && cols[0].name == mc.names[0]
                        && cols[1].name == mc.names[1] && cols[mc.numCol].numeric
                        && numsEqual(cols[mc.numCol].nums, mc.nums) && !cols[txtCol].numeric
                        && cols[txtCol].texts.size() >= mc.texts.size();
                for (int r = 0; ok && r < mc.texts.size(); ++r)
                    ok = cols[txtCol].texts[r].trimmed() == mc.texts[r];
                CHECK_COND(ok, "103" << mc.tag << " (" << (pass == 0 ? "GUI" : "RPC")
                                     << "): " << dumpCols(cols));
            }
        }
        // missing values in the first data rows still keep the header (103a1)
        CHECK_COND(checkBoth("j1b", writeFile(QStringLiteral("mf_empty2.csv"),
                                              "Time,Load\n0,\n1,\n2,3.5\n3,4.5\n"),
                             { { "Time", { 0, 1, 2, 3 } }, { "Load", { NaN, NaN, 3.5, 4.5 } } }),
                   "103j1b: missing values in the first two data rows");

        // j3: paste_tsv over existing data: the empty last column of the pasted range clears the
        // cells below it (only app.import_csv drops a column that every line leaves empty)
        const QJsonObject base =
                rpc(QStringLiteral("table.paste_tsv"),
                    QJsonObject { { "text", "x\ty\tz\n1\t2\t3\n4\t5\t6\n7\t8\t9\n" },
                                  { "name", "Paste103j" } });
        CHECK_COND(!rpcIsError103(base), "103j3: paste_tsv (new table) failed");
        const QString ptbl = rpcResult103(base).value(QStringLiteral("name")).toString();
        const QJsonObject over = rpc(QStringLiteral("table.paste_tsv"),
                                     QJsonObject { { "text", "1\t2\t\n3\t4\t" },
                                                   { "table", ptbl },
                                                   { "start_row", 1 } });
        CHECK_COND(!rpcIsError103(over), "103j3: paste_tsv over existing data failed");
        auto colData = [&](const char *col) {
            QVector<double> v;
            const QJsonArray data = rpcResult103(rpc(QStringLiteral("table.get_column_data"),
                                                     QJsonObject { { "table", ptbl },
                                                                   { "col", col } }))
                                            .value(QStringLiteral("data"))
                                            .toArray();
            for (const QJsonValue &d : data)
                v << (d.isDouble() ? d.toDouble() : nanOf());
            return v;
        };
        auto numsText = [](const QVector<double> &v) {
            QStringList s;
            for (double d : v)
                s << (std::isnan(d) ? QStringLiteral("NaN") : QString::number(d));
            return s.join(QLatin1Char(',')).toStdString();
        };
        const QVector<double> px = colData("x"), py = colData("y"), pz = colData("z");
        CHECK_COND(numsEqual(px, { 1, 1, 3 }) && numsEqual(py, { 2, 2, 4 })
                           && numsEqual(pz, { 3, NaN, NaN }),
                   "103j3: paste_tsv with an empty last column: x {"
                           << numsText(px) << "} y {" << numsText(py) << "} z {" << numsText(pz)
                           << "}");

        // j4: "3D-1", "3D-2" are identifiers (no point in the mantissa); "1.0D+00" stays a number
        const QString ids = writeFile(QStringLiteral("ids_3d.csv"),
                                      "pos,v\n3D-1,1.0D+00\n3D-2,2.5D-01\n3D-3,3.D2\n");
        for (int pass = 0; pass < 2; ++pass) {
            const QVector<Col103> cols = (pass == 0) ? guiImport(ids) : rpcImport(ids);
            CHECK_COND(cols.size() == 2 && !cols[0].numeric
                               && cols[0].texts.value(0) == QLatin1String("3D-1")
                               && cols[0].texts.value(2) == QLatin1String("3D-3")
                               && cols[1].numeric && numsEqual(cols[1].nums, { 1, 0.25, 300 }),
                       "103j4 (" << (pass == 0 ? "GUI" : "RPC") << "): " << dumpCols(cols));
        }

        // j5: UTF-8 text ("°", "µ", "±", "é", and "Ü", "à", which Shift-JIS cannot read) with one
        // stray Latin-1 byte: UTF-8 with a warning, not Windows-1252 ("Â°C")
        const QByteArray nearUtf8 =
                QStringLiteral("Temp (°C),Size (µm),Note\n20,1.5,Größe ± é Ü\n25,2.5,à Ü\n30,3.5,x")
                        .toUtf8()
                + QByteArray("\xB0") + QByteArray("y\n");
        const QString nearPath = writeFile(QStringLiteral("near_utf8.csv"), nearUtf8);
        AsciiTableImportFilter::ImportReport nrep;
        std::unique_ptr<future::Table> nft(AsciiTableImportFilter::importDroppedTextData(
                nearUtf8, QStringLiteral("Near103"), nearPath, &nrep));
        CHECK_COND(nft && nrep.encoding == QLatin1String("UTF-8") && nrep.invalidChars == 1
                           && !AsciiTableImportFilter::describeImportReport(nrep).isEmpty()
                           && nft->columnCount() == 3
                           && nft->column(0)->units() == QStringLiteral("°C")
                           && nft->column(1)->units() == QStringLiteral("µm")
                           && nft->column(2)->textAt(0) == QStringLiteral("Größe ± é Ü"),
                   "103j5 (GUI): encoding " << nrep.encoding.toStdString() << " invalid "
                                            << nrep.invalidChars << " cols "
                                            << (nft ? nft->columnCount() : -1) << " units0 "
                                            << (nft && nft->columnCount() > 0
                                                        ? nft->column(0)->units().toStdString()
                                                        : std::string()));
        const QVector<Col103> nr = rpcImport(nearPath);
        CHECK_COND(nr.size() == 3 && nr[0].units == QStringLiteral("°C")
                           && nr[1].units == QStringLiteral("µm")
                           && nr[2].texts.value(0) == QStringLiteral("Größe ± é Ü"),
                   "103j5 (RPC): " << dumpCols(nr));
    }
    std::cout << "  [OK] 103j round-2 regressions (text column, paste_tsv, 3D-1, near-UTF-8)\n";

    std::cout << "[PASS] Suite 103: text import round-2 findings passed!\n";
    return 0;
}
