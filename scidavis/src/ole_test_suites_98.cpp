#include "ole_test_common.h"
#include "PythonAutomationServer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUndoStack>

#include <cmath>
#include <memory>

// =============================================================================
// Suite 98: robustness of the JSON-RPC text import (app.import_csv / table.paste_tsv).
//   98a Rigaku .ras with scans of different length: the shorter scan has no phantom rows
//       (import_csv and paste_tsv)
//   98b separator from several lines: a comma in a column name does not beat ';' / TAB
//   98c a row with numbers is not a units row ("1,10,," above ",,1.5,20")
//   98d '#' comment lines further down are skipped; '#1, 520' specimen rows stay data; SPEC
//       (#S / #L) files give the GUI import's per-scan columns
//   98e CR-only line ends in whitespace-separated .dat / .xy files
//   98f paste_tsv(header=true) over a table recalculates the dependent formula column, in one
//       Undo step
//   98g ';'-separated decimal-comma CSV (auto, separator=";", headerless, paste_tsv)
//   98h blank lines above the header
//   98i table.column_stats: sd / se are null for n < 2
//   98j paste_tsv of a range whose first row is empty: the data rows are not shifted
//   98k ';' decimal-comma file: "#A1;12,5;3,4" after the data stays data, as in the GUI
// =============================================================================

namespace {
bool rpcIsError98(const QJsonObject &resp)
{
    return resp.contains(QStringLiteral("error"));
}
QJsonObject rpcResult98(const QJsonObject &resp)
{
    return resp.value(QStringLiteral("result")).toObject();
}
} // namespace

int runSuite98RpcImportRobustness(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 98: JSON-RPC text import robustness...\n";

    struct NonInteractiveQuitGuard98
    {
        QVariant prevNonInteractive;
        bool prevQuitOnLast = true;
        NonInteractiveQuitGuard98()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevQuitOnLast(QApplication::quitOnLastWindowClosed())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QApplication::setQuitOnLastWindowClosed(false);
        }
        ~NonInteractiveQuitGuard98()
        {
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
            QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
        }
    } envGuard98;

    struct AppWinGuard98
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard98(ApplicationWindow *w = nullptr) : win(w) { }
        ~AppWinGuard98() { reset(); }
        AppWinGuard98(const AppWinGuard98 &) = delete;
        AppWinGuard98 &operator=(const AppWinGuard98 &) = delete;
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

    AppWinGuard98 w98(new ApplicationWindow());
    w98->applyUserSettings();
    w98->confirmCloseTable = false;
    w98->confirmCloseMatrix = false;
    w98->confirmClosePlot2D = false;
    w98->confirmClosePlot3D = false;
    w98->confirmCloseFolder = false;
    w98->resize(1280, 800);
    w98->show();
    QCoreApplication::processEvents();

    PythonAutomationServer *srv = w98->pythonAutomationServer();
    CHECK_COND(srv != nullptr, "98: pythonAutomationServer null");

    QTemporaryDir tmp98;
    CHECK_COND(tmp98.isValid(), "98: temporary directory");

    int rpcId = 9800;
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
        const QString path = tmp98.filePath(name);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(bytes);
            f.close();
        }
        return path;
    };
    // imports `path`; the table name (empty on error)
    auto importCsv = [&](const QString &path, QJsonObject params = QJsonObject()) -> QString {
        params[QStringLiteral("path")] = path;
        const QJsonObject r = rpc(QStringLiteral("app.import_csv"), params);
        return rpcIsError98(r) ? QString()
                               : rpcResult98(r).value(QStringLiteral("name")).toString();
    };
    auto columnNames = [&](const QString &tbl) -> QStringList {
        QJsonObject params;
        params.insert(QLatin1String("table"), tbl);
        const QJsonArray cols = rpcResult98(rpc(QString::fromLatin1("table.get_info"), params))
                                        .value(QLatin1String("columns"))
                                        .toArray();
        QStringList names;
        for (int i = 0; i < cols.size(); ++i)
            names << cols.at(i).toObject().value(QLatin1String("name")).toString();
        return names;
    };
    auto colMeta = [&](const QString &tbl, const QString &col) -> QJsonObject {
        return rpcResult98(rpc(QStringLiteral("table.get_column_meta"),
                               QJsonObject { { "table", tbl }, { "col", col } }));
    };
    auto colData = [&](const QString &tbl, const QString &col) -> QJsonArray {
        return rpcResult98(rpc(QStringLiteral("table.get_column_data"),
                               QJsonObject { { "table", tbl }, { "col", col } }))
                .value(QStringLiteral("data"))
                .toArray();
    };
    // numeric column data equal to `expected` (NaN = empty / invalid cell)
    auto numsEqual = [](const QJsonArray &got, const QVector<double> &expected) {
        if (got.size() != expected.size())
            return false;
        for (int i = 0; i < expected.size(); ++i) {
            if (std::isnan(expected[i])) {
                if (!got[i].isNull())
                    return false;
            } else if (!got[i].isDouble() || std::fabs(got[i].toDouble() - expected[i]) > 1e-12) {
                return false;
            }
        }
        return true;
    };
    auto dump = [](const QJsonArray &a) {
        QStringList s;
        for (const QJsonValue &v : a)
            s << (v.isNull() ? QStringLiteral("null")
                             : (v.isDouble() ? QString::number(v.toDouble()) : v.toString()));
        return s.join(QLatin1Char(',')).toStdString();
    };
    const double NaN = std::numeric_limits<double>::quiet_NaN();

    // -------------------------------------------------------------------------
    // 98a: .ras with scans of 5 and 4 points
    // -------------------------------------------------------------------------
    {
        auto scan = [](double start, double step, int n, double base) {
            QStringList l { QStringLiteral("*RAS_HEADER_START"),
                            QStringLiteral("*MEAS_SCAN_UNIT_X \"deg\""),
                            QStringLiteral("*MEAS_SCAN_UNIT_Y \"counts\""),
                            QStringLiteral("*RAS_HEADER_END"), QStringLiteral("*RAS_INT_START") };
            for (int i = 0; i < n; ++i)
                l << QStringLiteral("%1 %2 1.0000")
                                .arg(start + i * step, 0, 'f', 4)
                                .arg(base + i * 3, 0, 'f', 1);
            l << QStringLiteral("*RAS_INT_END");
            return l;
        };
        QStringList lines { QStringLiteral("*RAS_DATA_START") };
        lines << scan(20, 0.02, 5, 100) << scan(30, 0.05, 4, 500)
              << QStringLiteral("*RAS_DATA_END");
        const QString rasText = lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
        const QString tbl = importCsv(writeFile(QStringLiteral("multi.ras"), rasText.toUtf8()));
        CHECK_COND(!tbl.isEmpty(), "98a: import of the .ras failed");
        const QJsonArray i2 = colData(tbl, QStringLiteral("Intensity_2"));
        CHECK_COND(numsEqual(i2, { 500, 503, 506, 509, NaN }),
                   "98a: shorter .ras scan has a phantom row: " << dump(i2));
        const QJsonObject st =
                rpcResult98(rpc(QStringLiteral("table.column_stats"),
                                QJsonObject { { "table", tbl }, { "col", "TwoTheta_2" } }));
        CHECK_COND(st.value("n").toInt() == 4 && st.value("min").toDouble() == 30.0,
                   "98a: stats of the shorter scan count the empty row (n=" << st.value("n").toInt()
                                                                            << ")");

        const QJsonObject rp = rpc(QStringLiteral("table.paste_tsv"),
                                   QJsonObject { { "text", rasText }, { "name", "Ras98Paste" } });
        CHECK_COND(!rpcIsError98(rp), "98a: paste_tsv of .ras text failed");
        const QString ptbl = rpcResult98(rp).value(QStringLiteral("name")).toString();
        const QJsonArray p2 = colData(ptbl, QStringLiteral("Intensity_2"));
        CHECK_COND(numsEqual(p2, { 500, 503, 506, 509, NaN }),
                   "98a: pasted .ras shorter scan has a phantom row: " << dump(p2));
    }

    // -------------------------------------------------------------------------
    // 98b: separator decided over several lines
    // -------------------------------------------------------------------------
    {
        const struct
        {
            const char *file;
            const char *text;
        } cases[] = {
            { "semi_header_comma.csv", "Time, s;Force, N\n0.1;2.5\n0.2;3.5\n" },
            { "tab_header_comma.txt", "Time, s\tForce, N\n0.1\t2.5\n0.2\t3.5\n" },
        };
        for (const auto &c : cases) {
            const QString tbl = importCsv(writeFile(QString::fromLatin1(c.file), c.text));
            CHECK_COND(!tbl.isEmpty(), "98b: import of " << c.file << " failed");
            const QStringList names = columnNames(tbl);
            CHECK_COND(names.size() == 2,
                       "98b: " << c.file << " gave " << names.size()
                               << " columns: " << names.join('|').toStdString());
            const QJsonArray c0 = colData(tbl, names[0]);
            const QJsonArray c1 = colData(tbl, names[1]);
            CHECK_COND(numsEqual(c0, { 0.1, 0.2 }) && numsEqual(c1, { 2.5, 3.5 }),
                       "98b: " << c.file << " values " << dump(c0) << " / " << dump(c1));
        }
        // plain CSVs keep the comma; a .csv with a few ';' in a text column too
        const QString t1 =
                importCsv(writeFile(QStringLiteral("plain98.csv"), "a,b,c\n1,2,3\n4,5,6\n"));
        CHECK_COND(columnNames(t1) == QStringList({ "a", "b", "c" }),
                   "98b: plain CSV columns " << columnNames(t1).join('|').toStdString());
        const QString t2 = importCsv(writeFile(QStringLiteral("semi_in_text98.csv"),
                                               "id,note,v\n1,a;b,2\n2,c,3\n3,d,4\n"));
        CHECK_COND(columnNames(t2) == QStringList({ "id", "note", "v" })
                           && numsEqual(colData(t2, "v"), { 2, 3, 4 }),
                   "98b: comma CSV with ';' in a text cell: "
                           << columnNames(t2).join('|').toStdString());
    }

    // -------------------------------------------------------------------------
    // 98c: rows holding numbers are not units rows
    // -------------------------------------------------------------------------
    {
        const QString t1 =
                importCsv(writeFile(QStringLiteral("interleaved98.csv"),
                                    "x1,y1,x2,y2\n1,10,,\n,,1.5,20\n2,11,,\n,,2.5,21\n"));
        CHECK_COND(!t1.isEmpty(), "98c: interleaved import failed");
        const QJsonArray x1 = colData(t1, "x1");
        const QJsonArray y2 = colData(t1, "y2");
        CHECK_COND(numsEqual(x1, { 1, NaN, 2, NaN }) && numsEqual(y2, { NaN, 20, NaN, 21 }),
                   "98c: interleaved x1=" << dump(x1) << " y2=" << dump(y2));
        CHECK_COND(colMeta(t1, "x1").value("units").toString().isEmpty()
                           && colMeta(t1, "y1").value("units").toString().isEmpty(),
                   "98c: interleaved first data row taken as units");

        const QString t2 = importCsv(writeFile(QStringLiteral("sparse98.tsv"),
                                               "Temp\tA\tB\n300\t1.5\t\n\t\t2.5\n320\t1.7\t2.6\n"));
        CHECK_COND(numsEqual(colData(t2, "Temp"), { 300, NaN, 320 })
                           && numsEqual(colData(t2, "A"), { 1.5, NaN, 1.7 })
                           && colMeta(t2, "Temp").value("units").toString().isEmpty(),
                   "98c: sparse Temp=" << dump(colData(t2, "Temp")));

        // a real units row is still found
        const QString t3 = importCsv(writeFile(QStringLiteral("units98.csv"),
                                               "Strain,Stress\n%,MPa\n0.1,100\n0.2,200\n"));
        CHECK_COND(colMeta(t3, "Stress").value("units").toString() == "MPa"
                           && numsEqual(colData(t3, "Stress"), { 100, 200 }),
                   "98c: units row no longer detected");
        // paste_tsv uses the same rule
        const QJsonObject rp =
                rpc(QStringLiteral("table.paste_tsv"),
                    QJsonObject { { "text", "Temp\tA\tB\n300\t1.5\t\n\t\t2.5\n320\t1.7\t2.6" },
                                  { "name", "Paste98c" } });
        CHECK_COND(!rpcIsError98(rp) && numsEqual(colData("Paste98c", "Temp"), { 300, NaN, 320 }),
                   "98c: paste_tsv took the first data row as units: "
                           << dump(colData("Paste98c", "Temp")));
    }

    // -------------------------------------------------------------------------
    // 98d: '#' lines further down, SPEC files
    // -------------------------------------------------------------------------
    {
        const QString t1 = importCsv(writeFile(QStringLiteral("comment_mid98.csv"),
                                               "# exported\nx,y\n1,2\n# segment 2\n3,4\n"));
        CHECK_COND(colMeta(t1, "x").value("mode").toString() == "numeric"
                           && numsEqual(colData(t1, "x"), { 1, 3 })
                           && numsEqual(colData(t1, "y"), { 2, 4 }),
                   "98d: mid-file comment became data: x=" << dump(colData(t1, "x")));

        const QString t2 = importCsv(writeFile(QStringLiteral("comment_after_header98.dat"),
                                               "x y\n# units: s m\n1 2\n3 4\n"));
        CHECK_COND(columnNames(t2) == QStringList({ "x", "y" })
                           && numsEqual(colData(t2, "x"), { 1, 3 })
                           && colMeta(t2, "x").value("units").toString().isEmpty(),
                   "98d: comment under the header taken as units / data: "
                           << columnNames(t2).join('|').toStdString());

        // whitespace file with ';' and '!' comment lines between the data
        const QString t3 = importCsv(writeFile(QStringLiteral("bang98.dat"),
                                               "! header\nt v\n1 2\n; pause\n3 4\n! end\n5 6\n"));
        CHECK_COND(numsEqual(colData(t3, "t"), { 1, 3, 5 })
                           && numsEqual(colData(t3, "v"), { 2, 4, 6 }),
                   "98d: ';' / '!' comment lines in a .dat: t=" << dump(colData(t3, "t")));

        // '#<digit>' specimen rows after the header stay data
        const QString t4 = importCsv(writeFile(QStringLiteral("specimen98.csv"),
                                               "id,load\n#1,520\n#2,530\n# note\n#3,540\n"));
        const QJsonArray ids = colData(t4, "id");
        CHECK_COND(ids.size() == 3 && ids[0].toString() == "#1" && ids[2].toString() == "#3"
                           && numsEqual(colData(t4, "load"), { 520, 530, 540 }),
                   "98d: specimen rows: id=" << dump(ids));

        const QByteArray spec =
                "#F t.spec\n\n#S 1 ascan th 0 1 2 1\n#N 2\n#L Theta  Detector\n"
                "0.0 10\n0.5 20\n\n#S 2 ascan th 1 2 2 1\n#N 2\n#L Theta  Detector\n"
                "1.0 50\n1.5 60\n";
        const QString specPath = writeFile(QStringLiteral("two_scans98.spec"), spec);
        const QString t5 = importCsv(specPath);
        CHECK_COND(!t5.isEmpty(), "98d: SPEC import failed");
        std::unique_ptr<future::Table> gui(
                AsciiTableImportFilter::importLabTextData(spec, QStringLiteral("Gui98"), specPath));
        CHECK_COND(gui && gui->columnCount() == 4, "98d: GUI SPEC import");
        const QStringList names = columnNames(t5);
        CHECK_COND(names.size() == gui->columnCount(),
                   "98d: SPEC gave " << names.size()
                                     << " columns: " << names.join('|').toStdString());
        for (int c = 0; c < gui->columnCount(); ++c) {
            Column *gc = gui->column(c);
            CHECK_COND(names[c] == gc->name(),
                       "98d: SPEC column " << c << " is " << names[c].toStdString() << ", GUI "
                                           << gc->name().toStdString());
            QVector<double> expected;
            for (int r = 0; r < gc->rowCount(); ++r)
                expected << (gc->isInvalid(r) ? NaN : gc->valueAt(r));
            CHECK_COND(numsEqual(colData(t5, names[c]), expected),
                       "98d: SPEC column " << names[c].toStdString() << " values "
                                           << dump(colData(t5, names[c])));
        }
        CHECK_COND(names.first().startsWith(QStringLiteral("Theta"))
                           && numsEqual(colData(t5, names[3]), { 50, 60 }),
                   "98d: SPEC columns " << names.join('|').toStdString());
    }

    // -------------------------------------------------------------------------
    // 98e: CR-only line ends in whitespace-separated files
    // -------------------------------------------------------------------------
    {
        const QString t1 = importCsv(
                writeFile(QStringLiteral("cr98.dat"), "Time Stress\r0 1.5\r1 2.5\r2 3.5\r"));
        CHECK_COND(columnNames(t1) == QStringList({ "Time", "Stress" })
                           && numsEqual(colData(t1, "Stress"), { 1.5, 2.5, 3.5 }),
                   "98e: CR-only .dat: " << columnNames(t1).join('|').toStdString());
        const QString t2 = importCsv(
                writeFile(QStringLiteral("cr98.xy"), "10.00 120\r10.02 130\r10.04 125\r"));
        const QStringList n2 = columnNames(t2);
        CHECK_COND(n2.size() == 2 && numsEqual(colData(t2, n2[1]), { 120, 130, 125 }),
                   "98e: CR-only .xy: " << n2.join('|').toStdString());
    }

    // -------------------------------------------------------------------------
    // 98f: paste_tsv(header=true) recalculates the dependent formula column, one Undo step
    // -------------------------------------------------------------------------
    {
        const QString tbl =
                importCsv(writeFile(QStringLiteral("xy98.csv"), "x,y\n1,10\n2,20\n3,30\n"),
                          QJsonObject { { "name", "XY98" } });
        CHECK_COND(tbl == QStringLiteral("XY98"), "98f: import failed");
        CHECK_COND(!rpcIsError98(rpc(QStringLiteral("table.add_column"),
                                     QJsonObject { { "table", tbl }, { "name", "z" } })),
                   "98f: add_column");
        CHECK_COND(!rpcIsError98(rpc(QStringLiteral("table.set_formula"),
                                     QJsonObject { { "table", tbl },
                                                   { "col", "z" },
                                                   { "formula", "col(\"y\")*2" } })),
                   "98f: set_formula");
        rpc(QStringLiteral("table.recalculate"), QJsonObject { { "table", tbl }, { "col", "z" } });
        CHECK_COND(numsEqual(colData(tbl, "z"), { 20, 40, 60 }), "98f: formula fixture");
        Table *t = w98->table(tbl);
        CHECK_COND(t && t->d_future_table && t->d_future_table->undoStack(), "98f: table");
        QUndoStack *us = t->d_future_table->undoStack();
        us->clear();
        const QJsonObject rp = rpc(QStringLiteral("table.paste_tsv"),
                                   QJsonObject { { "table", tbl },
                                                 { "text", "x\ty\n7\t70\n8\t80" },
                                                 { "header", true },
                                                 { "start_row", 0 },
                                                 { "start_col", 0 } });
        CHECK_COND(!rpcIsError98(rp), "98f: paste_tsv failed");
        CHECK_COND(numsEqual(colData(tbl, "y"), { 70, 80, 30 })
                           && numsEqual(colData(tbl, "z"), { 140, 160, 60 }),
                   "98f: formula column not recalculated after paste with header: z="
                           << dump(colData(tbl, "z")));
        CHECK_COND(colMeta(tbl, "z").value("formula_error").toString().isEmpty(),
                   "98f: formula error left after the paste: "
                           << colMeta(tbl, "z").value("formula_error").toString().toStdString());
        CHECK_COND(us->index() == 1, "98f: paste made " << us->index() << " Undo steps");
        us->undo();
        QCoreApplication::processEvents();
        CHECK_COND(numsEqual(colData(tbl, "y"), { 10, 20, 30 })
                           && numsEqual(colData(tbl, "z"), { 20, 40, 60 })
                           && columnNames(tbl) == QStringList({ "x", "y", "z" }),
                   "98f: one Undo did not restore y / z: z=" << dump(colData(tbl, "z")));
    }

    // -------------------------------------------------------------------------
    // 98g: ';'-separated decimal-comma data
    // -------------------------------------------------------------------------
    {
        const QString hdrPath = writeFile(QStringLiteral("de_header98.csv"),
                                          "Zeit;Spannung\n0,5;1,5\n1,5;2,5\n2,5;3,5\n");
        const QString noHdrPath =
                writeFile(QStringLiteral("de_noheader98.csv"), "0,5;1,5\n1,5;2,5\n2,5;3,5\n");
        const struct
        {
            QString path;
            QJsonObject params;
            const char *label;
        } cases[] = {
            { hdrPath, QJsonObject(), "auto" },
            { hdrPath, QJsonObject { { "separator", ";" } }, "separator=;" },
            { noHdrPath, QJsonObject(), "headerless" },
        };
        for (const auto &c : cases) {
            const QString tbl = importCsv(c.path, c.params);
            const QStringList names = columnNames(tbl);
            CHECK_COND(names.size() == 2,
                       "98g: " << c.label << " gave " << names.size()
                               << " columns: " << names.join('|').toStdString());
            CHECK_COND(colMeta(tbl, names[0]).value("mode").toString() == "numeric"
                               && numsEqual(colData(tbl, names[0]), { 0.5, 1.5, 2.5 })
                               && numsEqual(colData(tbl, names[1]), { 1.5, 2.5, 3.5 }),
                       "98g: " << c.label << " values " << dump(colData(tbl, names[0])) << " / "
                               << dump(colData(tbl, names[1])));
        }
        CHECK_COND(columnNames(importCsv(hdrPath)) == QStringList({ "Zeit", "Spannung" }),
                   "98g: header names");
        const QJsonObject rp = rpc(QStringLiteral("table.paste_tsv"),
                                   QJsonObject { { "text", "Zeit;Spannung\n0,5;1,5\n1,5;2,5" },
                                                 { "name", "Paste98g" } });
        CHECK_COND(!rpcIsError98(rp) && numsEqual(colData("Paste98g", "Spannung"), { 1.5, 2.5 }),
                   "98g: paste_tsv decimal comma: " << dump(colData("Paste98g", "Spannung")));
        // dot decimals with ';' stay as they were
        const QString t3 =
                importCsv(writeFile(QStringLiteral("semi_dot98.csv"), "a;b\n0.5;1.5\n1.5;2.5\n"));
        CHECK_COND(numsEqual(colData(t3, "b"), { 1.5, 2.5 }), "98g: ';' with decimal points");
    }

    // -------------------------------------------------------------------------
    // 98h: blank lines above the header
    // -------------------------------------------------------------------------
    {
        const QString t1 = importCsv(
                writeFile(QStringLiteral("blank_top98.csv"), "\n\r\n  \nx,y\n1,2\n3,4\n"));
        CHECK_COND(
                columnNames(t1) == QStringList({ "x", "y" })
                        && colMeta(t1, "x").value("mode").toString() == "numeric"
                        && numsEqual(colData(t1, "y"), { 2, 4 }),
                "98h: blank lines above the header: " << columnNames(t1).join('|').toStdString());
    }

    // -------------------------------------------------------------------------
    // 98i: column_stats with n < 2
    // -------------------------------------------------------------------------
    {
        const QString tbl =
                importCsv(writeFile(QStringLiteral("stats98.csv"), "one,two\n5,1\n,3\n"));
        const QJsonObject s1 = rpcResult98(rpc(QStringLiteral("table.column_stats"),
                                               QJsonObject { { "table", tbl }, { "col", "one" } }));
        CHECK_COND(s1.value("n").toInt() == 1 && s1.value("sd").isNull() && s1.value("se").isNull()
                           && s1.value("mean").toDouble() == 5.0,
                   "98i: n=1 stats sd/se are not null (sd=" << s1.value("sd").toDouble() << ")");
        const QJsonObject s2 = rpcResult98(rpc(QStringLiteral("table.column_stats"),
                                               QJsonObject { { "table", tbl }, { "col", "two" } }));
        CHECK_COND(s2.value("n").toInt() == 2
                           && std::fabs(s2.value("sd").toDouble() - std::sqrt(2.0)) < 1e-12
                           && std::fabs(s2.value("se").toDouble() - 1.0) < 1e-12,
                   "98i: n=2 stats sd=" << s2.value("sd").toDouble());
    }

    // -------------------------------------------------------------------------
    // 98j: paste_tsv of a range whose first row is empty (copied from Excel)
    // -------------------------------------------------------------------------
    {
        const QString text = QStringLiteral("\t\r\n1\t2\r\n3\t4");
        // start_row=0 (header row by default): the empty first row is the header row, the data
        // rows stay data
        const QJsonObject r0 = rpc(QStringLiteral("table.paste_tsv"),
                                   QJsonObject { { "text", text }, { "name", "Paste98j" } });
        CHECK_COND(!rpcIsError98(r0), "98j: paste_tsv failed");
        const QStringList n0 = columnNames(QStringLiteral("Paste98j"));
        CHECK_COND(n0.size() == 2 && numsEqual(colData("Paste98j", n0[0]), { 1, 3 })
                           && numsEqual(colData("Paste98j", n0[1]), { 2, 4 }),
                   "98j: start_row=0: " << n0.join('|').toStdString() << " / "
                                        << dump(colData("Paste98j", n0.value(0))));
        // start_row=2 (no header): the empty row lands on row 2, the data on rows 3 and 4
        const QString tbl = importCsv(writeFile(QStringLiteral("ab98j.csv"), "a,b\n10,20\n30,40\n"),
                                      QJsonObject { { "name", "AB98j" } });
        CHECK_COND(tbl == QStringLiteral("AB98j"), "98j: import failed");
        const QJsonObject r2 = rpc(QStringLiteral("table.paste_tsv"),
                                   QJsonObject { { "table", tbl },
                                                 { "text", text },
                                                 { "start_row", 2 },
                                                 { "start_col", 0 } });
        CHECK_COND(!rpcIsError98(r2), "98j: paste_tsv at row 2 failed");
        CHECK_COND(numsEqual(colData(tbl, "a"), { 10, 30, NaN, 1, 3 })
                           && numsEqual(colData(tbl, "b"), { 20, 40, NaN, 2, 4 }),
                   "98j: start_row=2 shifted the data: a=" << dump(colData(tbl, "a")));
    }

    // -------------------------------------------------------------------------
    // 98k: decimal-comma ';' file: "#A1;12,5;3,4" after the data rows is a specimen row
    // -------------------------------------------------------------------------
    {
        const QByteArray bytes = "Probe;Wert;Dicke\nS0;10,5;3,1\nS1;11,5;3,2\n# Messung 2\n"
                                 "#A1;12,5;3,4\n";
        const QString path = writeFile(QStringLiteral("de_specimen98.csv"), bytes);
        const QString tbl = importCsv(path);
        const QJsonArray ids = colData(tbl, "Probe");
        CHECK_COND(ids.size() == 3 && ids[2].toString() == "#A1"
                           && numsEqual(colData(tbl, "Wert"), { 10.5, 11.5, 12.5 })
                           && numsEqual(colData(tbl, "Dicke"), { 3.1, 3.2, 3.4 }),
                   "98k: Probe=" << dump(ids) << " Wert=" << dump(colData(tbl, "Wert")));
        // the GUI import reads the same rows
        std::unique_ptr<future::Table> gui(
                AsciiTableImportFilter::importLabTextData(bytes, QStringLiteral("Gui98k"), path));
        CHECK_COND(gui && gui->columnCount() == 3 && gui->column(1)->rowCount() >= 3
                           && std::fabs(gui->column(1)->valueAt(2) - 12.5) < 1e-12,
                   "98k: GUI import of the same file differs");
    }

    w98.reset();
    std::cout << "[PASS] Suite 98 passed.\n";
    return 0;
}
