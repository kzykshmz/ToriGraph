#include "ole_test_common.h"

// =============================================================================
// Suite 64 (D1): Excel .xlsx import without external libraries
// =============================================================================
#include "XlsxImport.h"
#include "ZipReader.h"
#include "future/core/datatypes/DateTime2StringFilter.h"

namespace {
QByteArray suite64Workbook()
{
    const QByteArray contentTypes =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"/>";
    const QByteArray rootRels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
            "</Relationships>";
    // Tab order Tensile, Dates, Empty; part file names deliberately not in tab order.
    const QByteArray workbook =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
            "<workbookPr/><sheets>"
            "<sheet name=\"Tensile\" sheetId=\"1\" r:id=\"rId3\"/>"
            "<sheet name=\"Dates\" sheetId=\"2\" r:id=\"rId1\"/>"
            "<sheet name=\"Empty\" sheetId=\"3\" r:id=\"rId2\"/>"
            "</sheets></workbook>";
    const QByteArray wbRels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
            "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"/xl/worksheets/sheet2.xml\"/>"
            "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet3.xml\"/>"
            "<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>"
            "<Relationship Id=\"rId5\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
            "</Relationships>";
    // 0 "Strain" as rich text runs, 1 Stress, 2 Sample, 3 %, 4 MPa, 5 S1, 6 (Japanese) + furigana
    const QByteArray sst =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"7\" uniqueCount=\"7\">"
            "<si><r><rPr><b/></rPr><t>Str</t></r><r><t>ain</t></r></si>"
            "<si><t>Stress</t></si><si><t>Sample</t></si><si><t>%</t></si><si><t>MPa</t></si>"
            "<si><t xml:space=\"preserve\">S1</t></si>"
            "<si><t>\xE8\xA9\xA6\xE9\xA8\x93\xE7\x89\x87</t><rPh sb=\"0\" eb=\"3\"><t>\xE3\x82\xB7\xE3\x82\xB1\xE3\x83\xB3\xE3\x83\x98\xE3\x83\xB3</t></rPh></si>"
            "</sst>";
    // xf 0 General, 1 builtin 14 (date), 2 custom 164 date+time, 3 custom 165 "0.00" (number),
    // 4 custom 166 [h]:mm elapsed (number)
    const QByteArray styles =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
            "<numFmts count=\"3\"><numFmt numFmtId=\"164\" formatCode=\"yyyy/m/d\\ h:mm\"/>"
            "<numFmt numFmtId=\"165\" formatCode=\"0.00\"/>"
            "<numFmt numFmtId=\"166\" formatCode=\"[h]:mm\"/></numFmts>"
            "<cellStyleXfs count=\"1\"><xf numFmtId=\"14\"/></cellStyleXfs>"
            "<cellXfs count=\"5\"><xf numFmtId=\"0\"/><xf numFmtId=\"14\" applyNumberFormat=\"1\"/>"
            "<xf numFmtId=\"164\"/><xf numFmtId=\"165\"/><xf numFmtId=\"166\"/></cellXfs>"
            "</styleSheet>";
    // Tensile: header row, unit row, 3 data rows; B4 missing, D mixes text/number/bool,
    // E has a Japanese header with furigana.
    const QByteArray tensile =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>"
            "<row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c><c r=\"B1\" t=\"s\"><v>1</v></c>"
            "<c r=\"C1\" t=\"s\"><v>2</v></c><c r=\"D1\" t=\"inlineStr\"><is><t>Note</t></is></c>"
            "<c r=\"E1\" t=\"s\"><v>6</v></c></row>"
            "<row r=\"2\"><c r=\"A2\" t=\"s\"><v>3</v></c><c r=\"B2\" t=\"s\"><v>4</v></c></row>"
            "<row r=\"3\"><c r=\"A3\"><v>0</v></c><c r=\"B3\"><v>0</v></c>"
            "<c r=\"C3\" t=\"s\"><v>5</v></c><c r=\"D3\" t=\"str\"><f>\"ok\"</f><v>ok</v></c>"
            "<c r=\"E3\" s=\"3\"><v>10</v></c></row>"
            "<row r=\"4\"><c r=\"A4\"><v>1.5</v></c><c r=\"C4\" t=\"inlineStr\"><is><t>S2</t></is></c>"
            "<c r=\"D4\"><v>7</v></c><c r=\"E4\" s=\"4\"><v>20</v></c></row>"
            "<row r=\"5\"><c r=\"A5\"><v>3</v></c><c r=\"B5\"><v>250.25</v></c>"
            "<c r=\"C5\" t=\"s\"><v>5</v></c><c r=\"D5\" t=\"b\"><v>1</v></c><c r=\"E5\"><v>3.0E1</v></c></row>"
            "</sheetData></worksheet>";
    // Dates: starts at B2 (leading empty row/column), some cells without r attributes
    const QByteArray dates =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>"
            "<row r=\"2\"><c r=\"B2\" t=\"inlineStr\"><is><t>Date</t></is></c>"
            "<c t=\"inlineStr\"><is><t>Temp (K)</t></is></c></row>"
            "<row r=\"3\"><c r=\"B3\" s=\"1\"><v>45000</v></c><c><v>300</v></c></row>"
            "<row r=\"4\"><c r=\"B4\" s=\"2\"><v>45001.5</v></c><c><v>310.5</v></c></row>"
            "<row r=\"5\"><c r=\"C5\"><v>320</v></c></row>"
            "</sheetData></worksheet>";
    const QByteArray empty =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData/></worksheet>";

    QList<Suite64ZipEntry> entries;
    entries << Suite64ZipEntry { "[Content_Types].xml", contentTypes, false }
            << Suite64ZipEntry { "_rels/.rels", rootRels, false }
            << Suite64ZipEntry { "xl/workbook.xml", workbook, true }
            << Suite64ZipEntry { "xl/_rels/workbook.xml.rels", wbRels, false }
            << Suite64ZipEntry { "xl/sharedStrings.xml", sst, true }
            << Suite64ZipEntry { "xl/styles.xml", styles, false }
            << Suite64ZipEntry { "xl/worksheets/sheet1.xml", dates, true }
            << Suite64ZipEntry { "xl/worksheets/sheet2.xml", empty, false }
            << Suite64ZipEntry { "xl/worksheets/sheet3.xml", tensile, true };
    return suite64BuildZip(entries);
}

Table *suite64FindTable(ApplicationWindow *mw, const QString &name)
{
    for (MyWidget *w : mw->windowsList()) {
        if (w && w->name() == name)
            return qobject_cast<Table *>(w);
    }
    return nullptr;
}

void suite64CloseTables(ApplicationWindow *mw, const QStringList &names)
{
    for (const QString &n : names) {
        if (Table *t = suite64FindTable(mw, n)) {
            t->askOnCloseEvent(false);
            t->close();
        }
    }
    QApplication::processEvents();
}
} // namespace

int runSuite64XlsxImport(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 64: Excel .xlsx import (ZIP reader, sheets, shared strings, "
                 "units, dates, protected/damaged files)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 64");
    qApp->setProperty("torigraph_noninteractive", true);
    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "64: temp dir");

    // ---- 64a: shared ZIP reader (stored + deflated entries, exact lookup, truncation)
    const QByteArray xlsx = suite64Workbook();
    {
        torizip::ZipArchive zip;
        QString err;
        CHECK_COND(zip.openData(xlsx, &err), QString("64a: zip open: %1").arg(err).toStdString());
        CHECK_COND(zip.entries().size() == 9, "64a: entry count");
        const int wbIdx = zip.findExactEntry(QStringLiteral("XL/Workbook.xml"));
        CHECK_COND(wbIdx >= 0, "64a: case-insensitive exact lookup");
        QByteArray wb;
        CHECK_COND(zip.extract(wbIdx, wb, &err) && wb.contains("<sheet name=\"Dates\""),
                   QString("64a: deflated entry: %1").arg(err).toStdString());
        CHECK_COND(zip.findExactEntry(QStringLiteral("sheet1.xml")) < 0,
                   "64a: exact lookup must not fall back to a suffix match");
        QByteArray bad = xlsx;
        bad.chop(10);
        torizip::ZipArchive badZip;
        QString badErr;
        CHECK_COND(!badZip.openData(bad, &badErr) && !badErr.isEmpty(), "64a: truncated zip");
    }

    // ---- 64b: serial date conversion (1900 system incl. the 1900-02-29 bug, 1904 system)
    {
        using XlsxImport::serialToDateTime;
        CHECK_COND(serialToDateTime(1, false).date() == QDate(1900, 1, 1), "64b: serial 1");
        CHECK_COND(serialToDateTime(59, false).date() == QDate(1900, 2, 28), "64b: serial 59");
        CHECK_COND(serialToDateTime(61, false).date() == QDate(1900, 3, 1), "64b: serial 61");
        CHECK_COND(serialToDateTime(45000, false).date() == QDate(2023, 3, 15), "64b: 45000");
        CHECK_COND(serialToDateTime(45001.5, false) == QDateTime(QDate(2023, 3, 16), QTime(12, 0)),
                   "64b: 45001.5");
        CHECK_COND(serialToDateTime(0, true).date() == QDate(1904, 1, 1), "64b: 1904 system");
        CHECK_COND(serialToDateTime(0.25, false).time() == QTime(6, 0), "64b: time of day");
    }

    // ---- 64c: workbook model
    {
        XlsxImport::Workbook wb;
        QString err;
        CHECK_COND(XlsxImport::readWorkbookData(xlsx, wb, &err),
                   QString("64c: read: %1").arg(err).toStdString());
        CHECK_COND(wb.sheets.size() == 3 && wb.sheets[0].name == "Tensile"
                           && wb.sheets[1].name == "Dates" && wb.sheets[2].name == "Empty",
                   "64c: sheet names / tab order");
        const XlsxImport::Sheet &ts = wb.sheets[0];
        CHECK_COND(ts.rows.size() == 5 && ts.columnCount == 5, "64c: Tensile extent");
        CHECK_COND(ts.rows[0][0].text == "Strain", "64c: rich-text shared string");
        CHECK_COND(ts.rows[0][4].text == QString::fromUtf8("\xE8\xA9\xA6\xE9\xA8\x93\xE7\x89\x87"),
                   "64c: furigana (rPh) must not be part of the text");
        CHECK_COND(ts.rows[3].size() < 2 || ts.rows[3][1].type == XlsxImport::Cell::Empty,
                   "64c: missing cell B4 stays empty");
        const XlsxImport::Sheet &ds = wb.sheets[1];
        CHECK_COND(ds.rows.size() == 5 && ds.rows[2][1].isDate && !ds.rows[2][2].isDate,
                   "64c: date style detection");
        CHECK_COND(ds.rows[4][2].type == XlsxImport::Cell::Number && ds.rows[4][2].number == 320.0,
                   "64c: explicit C5 reference");
        CHECK_COND(wb.sheets[2].rows.isEmpty(), "64c: empty sheet");

        XlsxImport::Workbook only;
        CHECK_COND(XlsxImport::readWorkbookData(xlsx, only, &err, QStringLiteral("dates"))
                           && only.sheets.size() == 1 && only.sheets[0].name == "Dates",
                   "64c: single sheet (case-insensitive)");
        QString missErr;
        CHECK_COND(!XlsxImport::readWorkbookData(xlsx, only, &missErr, QStringLiteral("Nope"))
                           && missErr.contains("Nope") && missErr.contains("Tensile"),
                   "64c: unknown sheet name lists the sheets");
    }

    // ---- 64d: import into the project (names, Long Name, Units, types, dates)
    const QString path = tmp.filePath(QString::fromUtf8("\xE5\xBC\x95\xE5\xBC\xB5 data.xlsx"));
    CHECK_COND(suite59WriteFile(path, xlsx), "64d: write xlsx");
    QStringList created;
    {
        QString err;
        XlsxImport::ImportResult res;
        CHECK_COND(XlsxImport::importIntoProject(mw, path, QString(), &err, &res),
                   QString("64d: import: %1").arg(err).toStdString());
        created = res.tables;
        const QString tensileName = QString::fromUtf8("\xE5\xBC\x95\xE5\xBC\xB5_data_Tensile");
        const QString datesName = QString::fromUtf8("\xE5\xBC\x95\xE5\xBC\xB5_data_Dates");
        CHECK_COND(res.tables == (QStringList() << tensileName << datesName),
                   QString("64d: table names: %1").arg(res.tables.join(",")).toStdString());
        CHECK_COND(res.emptySheets == QStringList(QStringLiteral("Empty")),
                   "64d: empty sheet skipped");
        CHECK_COND(!res.summary.isEmpty(), "64d: summary");

        Table *t = suite64FindTable(mw, tensileName);
        CHECK_COND(t && t->numCols() == 5 && t->numRows() == 3, "64d: Tensile shape");
        Column *a = t->column(0);
        Column *b = t->column(1);
        Column *c = t->column(2);
        Column *d = t->column(3);
        Column *e = t->column(4);
        CHECK_COND(a->name() == "Strain" && a->longName() == "Strain" && a->units() == "%",
                   "64d: Strain name / Long Name / Units");
        CHECK_COND(b->longName() == "Stress" && b->units() == "MPa", "64d: Stress units row");
        CHECK_COND(a->columnMode() == SciDAVis::ColumnMode::Numeric && a->valueAt(1) == 1.5
                           && a->valueAt(2) == 3.0,
                   "64d: numeric values");
        CHECK_COND(b->isInvalid(1) && !b->isInvalid(0) && b->valueAt(2) == 250.25,
                   "64d: empty cell B4 -> invalid row");
        CHECK_COND(c->columnMode() == SciDAVis::ColumnMode::Text && c->textAt(0) == "S1"
                           && c->textAt(1) == "S2",
                   "64d: text column");
        CHECK_COND(d->columnMode() == SciDAVis::ColumnMode::Text && d->textAt(0) == "ok"
                           && d->textAt(1) == "7" && d->textAt(2) == "TRUE",
                   "64d: mixed text/number/bool column");
        CHECK_COND(e->columnMode() == SciDAVis::ColumnMode::Numeric && e->valueAt(0) == 10.0
                           && e->valueAt(1) == 20.0 && e->valueAt(2) == 30.0,
                   "64d: 0.00 and [h]:mm formats stay numbers");
        CHECK_COND(e->longName() == QString::fromUtf8("\xE8\xA9\xA6\xE9\xA8\x93\xE7\x89\x87")
                           && e->units().isEmpty(),
                   "64d: Japanese header");
        CHECK_COND(a->plotDesignation() == SciDAVis::X && b->plotDesignation() == SciDAVis::Y,
                   "64d: designations");

        Table *dt = suite64FindTable(mw, datesName);
        CHECK_COND(dt && dt->numCols() == 2 && dt->numRows() == 3, "64d: Dates shape (B2 origin)");
        Column *dc = dt->column(0);
        CHECK_COND(dc->columnMode() == SciDAVis::ColumnMode::DateTime, "64d: date column mode");
        CHECK_COND(dc->dateTimeAt(0) == QDateTime(QDate(2023, 3, 15), QTime(0, 0))
                           && dc->dateTimeAt(1) == QDateTime(QDate(2023, 3, 16), QTime(12, 0))
                           && dc->isInvalid(2),
                   "64d: date values");
        auto *fmt = qobject_cast<DateTime2StringFilter *>(dc->outputFilter());
        CHECK_COND(fmt && fmt->format() == "yyyy-MM-dd hh:mm:ss", "64d: date+time display format");
        Column *tc = dt->column(1);
        CHECK_COND(tc->longName() == "Temp" && tc->units() == "K" && tc->valueAt(1) == 310.5
                           && tc->valueAt(2) == 320.0,
                   "64d: 'Temp (K)' header split");
    }
    suite64CloseTables(mw, created);

    // ---- 64e: drop .xlsx (reads it), .xls keeps the save-as hint, damaged / encrypted stop
    {
        const int before = mw->windowsList().size();
        mw->setProperty("torigraph_last_drop_import_summary", QVariant());
        suite59Drop(mw, QStringList() << path);
        CHECK_COND(mw->windowsList().size() == before + 2, "64e: drop created two tables");
        CHECK_COND(mw->property("torigraph_last_drop_import_summary")
                           .toString()
                           .contains(QFileInfo(path).fileName()),
                   "64e: drop summary");
        QStringList dropped;
        for (MyWidget *w : mw->windowsList())
            if (w->name().endsWith("_Tensile") || w->name().endsWith("_Dates"))
                dropped << w->name();
        suite64CloseTables(mw, dropped);

        const QByteArray oleBytes = QByteArray::fromHex("D0CF11E0A1B11AE1") + QByteArray(600, '\0');
        const QString xlsPath = tmp.filePath("old64.xls");
        CHECK_COND(suite59WriteFile(xlsPath, oleBytes), "64e: write xls");
        mw->setProperty("torigraph_last_drop_unsupported_message", QVariant());
        const int before2 = mw->windowsList().size();
        suite59Drop(mw, QStringList() << xlsPath);
        const QString xlsMsg = mw->property("torigraph_last_drop_unsupported_message").toString();
        CHECK_COND(mw->windowsList().size() == before2 && xlsMsg.contains("xlsx")
                           && xlsMsg.contains("old64.xls"),
                   "64e: .xls drop hint");

        // Password-protected (encrypted) workbook: OLE container holding "EncryptedPackage"
        QByteArray enc = QByteArray::fromHex("D0CF11E0A1B11AE1") + QByteArray(512, '\0');
        for (char ch : QByteArray("EncryptedPackage")) {
            enc.append(ch);
            enc.append('\0');
        }
        enc.append(QByteArray(64, '\0'));
        const QString encPath = tmp.filePath("locked64.xlsx");
        CHECK_COND(suite59WriteFile(encPath, enc), "64e: write encrypted");
        QString encErr;
        CHECK_COND(!XlsxImport::importIntoProject(mw, encPath, QString(), &encErr)
                           && !encErr.isEmpty(),
                   "64e: encrypted workbook must be refused");
        QString xlsErr;
        XlsxImport::Workbook dummy;
        CHECK_COND(!XlsxImport::readWorkbookData(oleBytes, dummy, &xlsErr), "64e: xls bytes refused");
        CHECK_COND(encErr != xlsErr, "64e: encrypted and .xls get different reasons");
        mw->setProperty("torigraph_last_xlsx_import_error", QVariant());
        const int before3 = mw->windowsList().size();
        suite59Drop(mw, QStringList() << encPath);
        CHECK_COND(mw->windowsList().size() == before3
                           && mw->property("torigraph_last_xlsx_import_error").toString() == encErr,
                   "64e: encrypted drop shows the reason");

        // Damaged: the archive is cut off
        QByteArray damaged = xlsx;
        damaged.chop(30);
        const QString badPath = tmp.filePath("bad64.xlsx");
        CHECK_COND(suite59WriteFile(badPath, damaged), "64e: write damaged");
        QString badErr;
        CHECK_COND(!XlsxImport::importIntoProject(mw, badPath, QString(), &badErr)
                           && badErr.contains("CSV"),
                   "64e: damaged workbook refused with a hint");
        CHECK_COND(mw->windowsList().size() == before3, "64e: nothing created for bad files");
    }

    // ---- 64f: Python RPC app.import_xlsx
    if (PythonAutomationServer *srv = mw->pythonAutomationServer()) {
        QJsonObject req;
        req["jsonrpc"] = "2.0";
        req["id"] = 64;
        req["method"] = "app.import_xlsx";
        QJsonObject params;
        params["path"] = path;
        params["sheet"] = "Dates";
        req["params"] = params;
        const QJsonObject resp = srv->handleRpcRequest(req);
        const QJsonArray tables = resp.value("result").toObject().value("tables").toArray();
        CHECK_COND(tables.size() == 1 && tables.at(0).toString().endsWith("_Dates"),
                   "64f: app.import_xlsx(sheet='Dates')");
        CHECK_COND(PythonAutomationServer::supportedMethods().contains("app.import_xlsx"),
                   "64f: method listed");
        suite64CloseTables(mw, QStringList() << tables.at(0).toString());
        std::cout << "[INFO] Suite 64f: app.import_xlsx RPC verified\n";
    } else {
        std::cout << "[INFO] Suite 64f: no Python automation server; RPC check skipped\n";
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 64 Excel .xlsx import passed!\n";
    return 0;
}

// =============================================================================
// Suite 65: E11 graph types for materials labs.
//   65a error band (symmetric ± column and asymmetric + / − columns): band curves,
//       z order under the lines, legend shows only the data curves, pixels inside the
//       band are tinted and outside stay white
//   65b colour-mapped scatter: Viridis colours of the min / max Z, colour bar range,
//       optional size column
//   65c histogram + cumulative %: 1..20 with bin width 5 -> counts 4,5,5,5,1, cumulative
//       0, 20, 45, 70, 95, 100 % on the right axis
//   65d probability plots with known parameters:
//       lognormal mu = 2.0, sigma = 0.5 from exact median-rank quantiles (n = 15)
//       Gumbel lambda = 10, delta = 2, n = 3, mean rank: y = -0.326634, 0.366513, 1.245899
//       (hand values), x_T(T = 100) = 10 + 2 * 4.600149 = 19.200298
//   65e save / reopen: bands, colour map (Z / size columns, colour-bar range) and the
//       right-axis cumulative curve come back
//   65f menus, ribbon and Japanese strings
// =============================================================================
static MultiLayer *suite65FindGraph(ApplicationWindow *app, const QString &name)
{
    if (!app)
        return nullptr;
    for (MyWidget *w : app->windowsList()) {
        if (auto *ml = qobject_cast<MultiLayer *>(w))
            if (ml->name() == name)
                return ml;
    }
    return nullptr;
}

static bool suite65Near(double a, double b, double tol)
{
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol;
}

int runSuite65E11ExtraGraphTypes(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 65: E11 Error Band, Color-Mapped Scatter, Histogram + "
                 "Cumulative, Lognormal / Gumbel Probability Plots...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 65");
    qApp->setProperty("torigraph_noninteractive", true);
    QTemporaryDir tmpDir65;
    CHECK_COND(tmpDir65.isValid(), "Suite 65: Failed to create QTemporaryDir");

    // ---------------- 65a: error band ----------------
    Table *t = mw->newTable("T65", 11, 6);
    CHECK_COND(t != nullptr, "65a: table");
    t->setHeader(QStringList() << "x" << "y" << "err" << "y2" << "ep" << "em");
    for (int r = 0; r <= 10; ++r) {
        t->setCell(r, 0, r);
        t->setCell(r, 1, r);
        t->setCell(r, 2, 0.5);
        t->setCell(r, 3, 20.0 - r);
        t->setCell(r, 4, 1.0);
        t->setCell(r, 5, 2.0);
    }
    MultiLayer *mlBand = mw->newGraph("Band65");
    CHECK_COND(mlBand && mlBand->activeGraph(), "65a: graph");
    Graph *g = mlBand->activeGraph();
    const QString bandGraphName = mlBand->name();
    CHECK_COND(g->insertCurve(t, "T65_x", "T65_y", Graph::Line), "65a: insert y");
    CHECK_COND(g->insertCurve(t, "T65_x", "T65_y2", Graph::Line), "65a: insert y2");
    CHECK_COND(ExtraPlotTypes::addErrorBand(g, "T65_y", t, "T65_err", QString(), 64) == 1,
               "65a: symmetric band not added");
    CHECK_COND(ExtraPlotTypes::addErrorBand(g, "T65_y2", t, "T65_ep", "T65_em", 80) == 2,
               "65a: asymmetric band must add a plus-only and a minus-only band");
    CHECK_COND(g->curves() == 5, "65a: expected 2 data curves + 3 band curves");
    CHECK_COND(g->errorBandCount() == 3, "65a: errorBandCount != 3");
    CHECK_COND(ExtraPlotTypes::addErrorBand(g, "T65_nope", t, "T65_err") == 0,
               "65a: unknown curve must fail");
    {
        int plusOnly = 0, minusOnly = 0;
        for (int i = 0; i < g->curves(); ++i) {
            auto *er = dynamic_cast<QwtErrorPlotCurve *>(g->curve(i));
            if (!er)
                continue;
            CHECK_COND(er->isBand(), "65a: error curve is not a band");
            CHECK_COND(er->z() < er->masterCurve()->z() - 1.0,
                       "65a: band is not drawn under the curves");
            if (er->plusSide() && !er->minusSide())
                ++plusOnly;
            if (!er->plusSide() && er->minusSide())
                ++minusOnly;
            const QwtDoubleRect br = er->boundingRect();
            if (er->masterCurve()->title().text() == "T65_y")
                CHECK_COND(suite65Near(br.top(), -0.5, 1e-9) && suite65Near(br.bottom(), 10.5, 1e-9),
                           "65a: symmetric band bounding rect is not y ± 0.5");
        }
        CHECK_COND(plusOnly == 1 && minusOnly == 1, "65a: asymmetric halves not set");
        const QStringList lines =
                g->legendText().split('\n', Qt::SkipEmptyParts);
        CHECK_COND(lines.size() == 2, "65a: legend must list only the two data curves");
    }
    {
        // pixels: inside the symmetric band tinted, outside white (band 25 % opacity)
        g->setScale(QwtPlot::xBottom, 0.0, 10.0);
        g->setScale(QwtPlot::yLeft, 0.0, 22.0);
        mlBand->resize(700, 600);
        g->replot();
        QwtPlotCanvas *cv = g->plotWidget()->canvas();
        const QImage img = cv->grab().toImage();
        const Plot *pw = g->plotWidget();
        const double dpr = cv->width() > 0 ? double(img.width()) / double(cv->width()) : 1.0;
        auto px = [&](double x, double y) {
            return img.pixelColor(qRound(pw->transform(QwtPlot::xBottom, x) * dpr),
                                  qRound(pw->transform(QwtPlot::yLeft, y) * dpr));
        };
        const QColor inBand = px(4.5, 4.5 + 0.35);
        const QColor outside = px(4.5, 4.5 + 2.5);
        CHECK_COND(outside.red() > 245 && outside.green() > 245 && outside.blue() > 245,
                   "65a: outside the band must stay white");
        CHECK_COND(inBand.red() + inBand.green() + inBand.blue() < 3 * 250
                           && inBand.red() + inBand.green() + inBand.blue() > 3 * 120,
                   "65a: inside the band must be lightly tinted");
        // asymmetric band of y2 at x = 3 (y = 17): +1 above, -2 below
        const QColor up = px(3.5, 16.5 + 0.8);
        const QColor down = px(3.5, 16.5 - 1.7);
        const QColor beyondUp = px(3.5, 16.5 + 1.5);
        CHECK_COND(up.red() + up.green() + up.blue() < 3 * 250, "65a: + half of the band missing");
        CHECK_COND(down.red() + down.green() + down.blue() < 3 * 250, "65a: − half of the band missing");
        CHECK_COND(beyondUp.red() > 245 && beyondUp.green() > 245 && beyondUp.blue() > 245,
                   "65a: + half extends beyond +1");
    }

    // ---------------- 65b: colour-mapped scatter ----------------
    Table *tz = mw->newTable("Z65", 10, 4);
    CHECK_COND(tz != nullptr, "65b: table");
    tz->setHeader(QStringList() << "X" << "Y" << "Z" << "S");
    for (int r = 0; r < 10; ++r) {
        tz->setCell(r, 0, r);
        tz->setCell(r, 1, r * r);
        tz->setCell(r, 2, 10.0 * (r + 1)); // 10 ... 100
        tz->setCell(r, 3, 1.0 + r);
    }
    MultiLayer *mlZ = ExtraPlotTypes::plotColorMappedScatter(mw, tz, "X", "Y", "Z", "S");
    CHECK_COND(mlZ && mlZ->activeGraph(), "65b: colour scatter graph");
    const QString zGraphName = mlZ->name();
    Graph *gz = mlZ->activeGraph();
    CHECK_COND(gz->curves() == 1, "65b: one curve expected");
    auto *dz = dynamic_cast<DataCurve *>(gz->curve(0));
    CHECK_COND(dz && dz->hasPointColorMap(), "65b: curve has no colour map");
    CHECK_COND(dz->pointColorMap().zColumn == "Z65_Z" && dz->pointColorMap().sizeColumn == "Z65_S",
               "65b: Z / size columns");
    CHECK_COND(dz->pointColorMap().preset == Spectrogram::PresetViridis, "65b: default must be Viridis");
    {
        double lo = 0, hi = 0;
        dz->pointColorRange(&lo, &hi);
        CHECK_COND(suite65Near(lo, 10.0, 1e-12) && suite65Near(hi, 100.0, 1e-12),
                   "65b: colour range must be the Z min/max 10..100");
        const QwtLinearColorMap vir = Spectrogram::presetColorMap(Spectrogram::PresetViridis);
        CHECK_COND(dz->pointColor(0) == QColor(vir.rgb(QwtDoubleInterval(0, 1), 0.0)),
                   "65b: Z min must take the first Viridis colour");
        CHECK_COND(dz->pointColor(9) == QColor(vir.rgb(QwtDoubleInterval(0, 1), 1.0)),
                   "65b: Z max must take the last Viridis colour");
        CHECK_COND(dz->pointColor(0) != dz->pointColor(5), "65b: colours do not vary with Z");
    }
    CHECK_COND(gz->hasColorScaleOnAxis(QwtPlot::yRight), "65b: no colour bar on the right axis");
    {
        const QwtScaleDiv *d = gz->plotWidget()->axisScaleDiv(QwtPlot::yRight);
        CHECK_COND(d && suite65Near(d->lowerBound(), 10.0, 1e-9) && suite65Near(d->upperBound(), 100.0, 1e-9),
                   "65b: colour bar scale is not 10..100");
        CHECK_COND(gz->plotWidget()->axisWidget(QwtPlot::yRight)->isColorBarEnabled(),
                   "65b: colour bar not enabled");
        gz->replot(); // the colour bar range must survive a replot / autoscale
        d = gz->plotWidget()->axisScaleDiv(QwtPlot::yRight);
        CHECK_COND(d && suite65Near(d->upperBound(), 100.0, 1e-9), "65b: colour bar range lost on replot");
    }
    CHECK_COND(ExtraPlotTypes::plotColorMappedScatter(mw, tz, "X", "Y", "nope") == nullptr,
               "65b: missing Z column must fail");

    // ---------------- 65c: histogram + cumulative % ----------------
    Table *th = mw->newTable("H65", 20, 1);
    CHECK_COND(th != nullptr, "65c: table");
    for (int r = 0; r < 20; ++r)
        th->setCell(r, 0, r + 1);
    const ExtraPlotTypes::HistogramCumulativeResult hr =
            ExtraPlotTypes::plotHistogramCumulative(mw, th, th->colName(0), 5.0, 0.0);
    CHECK_COND(hr.valid && hr.plot && hr.table, "65c: histogram + cumulative failed");
    const QString hGraphName = hr.plot->name();
    CHECK_COND(hr.bins == 5 && suite65Near(hr.binWidth, 5.0, 1e-12) && hr.total == 20,
               "65c: expected 5 bins of width 5 holding 20 values");
    {
        const double expPct[6] = { 0, 20, 45, 70, 95, 100 };
        CHECK_COND(hr.cumulativePct.size() == 6, "65c: cumulative size");
        for (int i = 0; i < 6; ++i) {
            CHECK_COND(suite65Near(hr.cumulativePct[i], expPct[i], 1e-9), "65c: cumulative % value");
            CHECK_COND(suite65Near(hr.upperEdges[i], 5.0 * i, 1e-9), "65c: upper bin edge");
        }
        CHECK_COND(hr.cumulativePct.last() == 100.0, "65c: cumulative must end at exactly 100 %");
        Graph *gh = hr.plot->activeGraph();
        CHECK_COND(gh && gh->curves() == 2, "65c: histogram + cumulative curve expected");
        CHECK_COND(gh->curveType(0) == Graph::Histogram, "65c: first curve is not a histogram");
        CHECK_COND(gh->curve(1)->yAxis() == QwtPlot::yRight, "65c: cumulative curve not on the right axis");
        const QwtScaleDiv *d = gh->plotWidget()->axisScaleDiv(QwtPlot::yRight);
        CHECK_COND(d && suite65Near(d->lowerBound(), 0.0, 1e-9) && suite65Near(d->upperBound(), 100.0, 1e-9),
                   "65c: right axis must be 0..100 %");
        CHECK_COND(suite65Near(hr.table->cell(5, 2), 100.0, 1e-12) && suite65Near(hr.table->cell(5, 1), 20.0, 1e-12),
                   "65c: cumulative table last row");
    }

    // ---------------- 65d: probability plots ----------------
    Table *tp = mw->newTable("P65", 15, 2);
    CHECK_COND(tp != nullptr, "65d: table");
    {
        // exact lognormal quantiles at the median-rank positions, stored unsorted
        const int n = 15;
        for (int i = 1; i <= n; ++i) {
            const double p = (i - 0.3) / (n + 0.4);
            const double xv = std::exp(2.0 + 0.5 * gsl_cdf_ugaussian_Pinv(p));
            tp->setCell((i * 7) % n, 0, xv);
        }
        const double gx[3] = { 9.346732, 10.733026, 12.491798 };
        tp->setCell(0, 1, gx[2]);
        tp->setCell(1, 1, gx[0]);
        tp->setCell(2, 1, gx[1]);
    }
    const ExtraPlotTypes::ProbabilityPlotResult ln = ExtraPlotTypes::plotProbability(
            mw, tp, tp->colName(0), ExtraPlotTypes::LogNormal, ExtraPlotTypes::MedianRank);
    CHECK_COND(ln.valid && ln.n == 15, "65d: lognormal plot failed");
    CHECK_COND(suite65Near(ln.location, 2.0, 1e-9) && suite65Near(ln.scale, 0.5, 1e-9),
               "65d: lognormal mu / sigma not recovered");
    CHECK_COND(suite65Near(ln.rSquare, 1.0, 1e-9), "65d: lognormal R^2 != 1");
    CHECK_COND(ln.plot && ln.plot->activeGraph() && ln.plot->activeGraph()->curves() == 2,
               "65d: lognormal plot curves");
    {
        Graph *gl = ln.plot->activeGraph();
        CHECK_COND(gl->hasCustomTickLabels(QwtPlot::yLeft), "65d: probability axis labels missing");
        CHECK_COND(gl->plotWidget()->axisScaleEngine(QwtPlot::xBottom)->transformation()->type()
                           == QwtScaleTransformation::Log10,
                   "65d: lognormal X axis must be logarithmic");
        CHECK_COND(gl->categoryLabels(QwtPlot::yLeft).contains("50"), "65d: 50 % tick missing");
    }
    const ExtraPlotTypes::ProbabilityPlotResult gu = ExtraPlotTypes::plotProbability(
            mw, tp, tp->colName(1), ExtraPlotTypes::Gumbel, ExtraPlotTypes::MeanRank, 100.0);
    CHECK_COND(gu.valid && gu.n == 3, "65d: Gumbel plot failed");
    CHECK_COND(suite65Near(gu.variate[0], -0.326634, 1e-6) && suite65Near(gu.variate[1], 0.366513, 1e-6)
                       && suite65Near(gu.variate[2], 1.245899, 1e-6),
               "65d: Gumbel reduced variates (mean rank) differ from the hand values");
    CHECK_COND(suite65Near(gu.location, 10.0, 1e-5) && suite65Near(gu.scale, 2.0, 1e-5),
               "65d: Gumbel lambda / delta not recovered");
    CHECK_COND(suite65Near(gu.returnValue, 19.200298, 1e-4), "65d: Gumbel x_T(T=100)");
    CHECK_COND(suite65Near(ExtraPlotTypes::plottingPosition(1, 10, ExtraPlotTypes::MedianRank), 0.7 / 10.4, 1e-15)
                       && suite65Near(ExtraPlotTypes::plottingPosition(2, 10, ExtraPlotTypes::Hazen), 0.15, 1e-15)
                       && suite65Near(ExtraPlotTypes::plottingPosition(1, 4, ExtraPlotTypes::Blom), 0.625 / 4.25, 1e-15),
               "65d: plotting positions");
    {
        Table *tbad = mw->newTable("Pbad65", 3, 1);
        tbad->setCell(0, 0, 1.0);
        tbad->setCell(1, 0, -1.0);
        tbad->setCell(2, 0, 0.0);
        const ExtraPlotTypes::ProbabilityPlotResult bad = ExtraPlotTypes::plotProbability(
                mw, tbad, tbad->colName(0), ExtraPlotTypes::LogNormal, ExtraPlotTypes::MedianRank,
                0.0, false);
        CHECK_COND(!bad.valid && bad.excluded == 2 && !bad.error.isEmpty(),
                   "65d: non-positive values must be excluded and too few values reported");
    }

    // ---------------- 65e: save / reopen ----------------
    {
        const QString prj = tmpDir65.filePath("e11_65.sciprj");
        CHECK_COND(mw->saveFolder(mw->projectFolder(), prj), "65e: save failed");
        QFile f(prj);
        CHECK_COND(f.open(QIODevice::ReadOnly), "65e: cannot read project");
        const QByteArray txt = f.readAll();
        f.close();
        CHECK_COND(txt.contains("<ToriErrorBand") && txt.contains("<ToriPointColorMap"),
                   "65e: E11 tags not written");
        ApplicationWindow *re = mw->openProject(prj);
        CHECK_COND(re != nullptr, "65e: reopen failed");
        MultiLayer *rb = suite65FindGraph(re, bandGraphName);
        CHECK_COND(rb && rb->activeGraph(), "65e: band graph missing");
        Graph *rg = rb->activeGraph();
        CHECK_COND(rg->curves() == 5 && rg->errorBandCount() == 3, "65e: bands not restored");
        int alpha80 = 0;
        for (int i = 0; i < rg->curves(); ++i)
            if (auto *er = dynamic_cast<QwtErrorPlotCurve *>(rg->curve(i))) {
                CHECK_COND(er->z() < er->masterCurve()->z() - 1.0, "65e: band z order not restored");
                if (er->bandAlpha() == 80 && er->plusSide() != er->minusSide())
                    ++alpha80;
            }
        CHECK_COND(alpha80 == 2, "65e: asymmetric band halves / opacity not restored");
        CHECK_COND(rg->legendText().split('\n', Qt::SkipEmptyParts).size() == 2,
                   "65e: legend after reopening");

        MultiLayer *rz = suite65FindGraph(re, zGraphName);
        CHECK_COND(rz && rz->activeGraph(), "65e: colour scatter graph missing");
        Graph *rgz = rz->activeGraph();
        auto *rdz = dynamic_cast<DataCurve *>(rgz->curve(0));
        CHECK_COND(rdz && rdz->hasPointColorMap() && rdz->pointColorMap().zColumn == "Z65_Z"
                           && rdz->pointColorMap().sizeColumn == "Z65_S",
                   "65e: colour map not restored");
        CHECK_COND(rgz->hasColorScaleOnAxis(QwtPlot::yRight), "65e: colour bar not restored");
        const QwtScaleDiv *d = rgz->plotWidget()->axisScaleDiv(QwtPlot::yRight);
        CHECK_COND(d && suite65Near(d->lowerBound(), 10.0, 1e-9) && suite65Near(d->upperBound(), 100.0, 1e-9),
                   "65e: colour bar range not restored");

        MultiLayer *rh = suite65FindGraph(re, hGraphName);
        CHECK_COND(rh && rh->activeGraph() && rh->activeGraph()->curves() == 2, "65e: histogram graph");
        CHECK_COND(rh->activeGraph()->curve(1)->yAxis() == QwtPlot::yRight,
                   "65e: cumulative curve left the right axis");
        re->savedProject();
        re->setOleEmbeddedMode(true);
        delete re;
    }

    // removing the colour-mapped curve removes its colour bar
    gz->removeCurve(0);
    CHECK_COND(!gz->hasColorScaleOnAxis(QwtPlot::yRight)
                       && !gz->plotWidget()->axisWidget(QwtPlot::yRight)->isColorBarEnabled(),
               "65e: colour bar left behind after removing the scatter curve");

    // ---------------- 65g: Python RPC ----------------
    {
        PythonAutomationServer *srv = mw->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "65g: no automation server");
        int rid = 6500;
        auto rpc = [&](const char *method, const QJsonObject &params) {
            QJsonObject req;
            req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            req[QStringLiteral("id")] = ++rid;
            req[QStringLiteral("method")] = QString::fromLatin1(method);
            req[QStringLiteral("params")] = params;
            return srv->handleRpcRequest(req);
        };
        QJsonObject pp;
        pp["table"] = "P65";
        pp["col"] = 1;
        pp["distribution"] = "gumbel";
        pp["return_period"] = 100.0;
        pp["plot"] = false;
        const QJsonObject rp = rpc("stats.probability_plot", pp).value("result").toObject();
        CHECK_COND(rp.value("ok").toBool() && suite65Near(rp.value("lambda").toDouble(), 10.0, 1e-5)
                           && suite65Near(rp.value("delta").toDouble(), 2.0, 1e-5)
                           && suite65Near(rp.value("return_value").toDouble(), 19.200298, 1e-4),
                   "65g: stats.probability_plot (gumbel)");
        QJsonObject hp;
        hp["table"] = "H65";
        hp["col"] = 0;
        hp["bin_width"] = 5.0;
        const QJsonObject rh = rpc("plot.histogram_cumulative", hp).value("result").toObject();
        const QJsonArray cum = rh.value("cumulative_pct").toArray();
        CHECK_COND(rh.value("ok").toBool() && cum.size() == 6 && cum.last().toDouble() == 100.0,
                   "65g: plot.histogram_cumulative");
        QJsonObject cp;
        cp["table"] = "Z65";
        cp["x"] = "X";
        cp["y"] = "Y";
        cp["z"] = "Z";
        cp["palette"] = "plasma";
        const QJsonObject rc = rpc("plot.color_scatter", cp).value("result").toObject();
        CHECK_COND(rc.value("ok").toBool() && suite65Near(rc.value("z_min").toDouble(), 10.0, 1e-9)
                           && suite65Near(rc.value("z_max").toDouble(), 100.0, 1e-9),
                   "65g: plot.color_scatter");
        QJsonObject bp;
        bp["graph"] = bandGraphName;
        bp["curve"] = "y";
        bp["err"] = "err";
        const QJsonObject rb = rpc("graph.add_error_band", bp).value("result").toObject();
        CHECK_COND(rb.value("ok").toBool() && rb.value("band_curves").toInt() == 1
                           && rb.value("error_band_count").toInt() == 4,
                   "65g: graph.add_error_band");
    }

    // ---------------- 65f: menus, ribbon, translations ----------------
    {
        const char *names[] = { "actionE11ErrorBand", "actionE11ColorScatter",
                                "actionE11HistogramCumulative", "actionE11LognormalProbability",
                                "actionE11GumbelProbability" };
        for (const char *nm : names) {
            QAction *a = mw->findChild<QAction *>(QString::fromLatin1(nm));
            CHECK_COND(a != nullptr, "65f: E11 action missing");
            CHECK_COND(!a->associatedWidgets().isEmpty(), "65f: E11 action is in no menu");
            CHECK_COND(!a->text().isEmpty(), "65f: E11 action has no text");
        }
        if (QMenu *addMenu = mw->findChild<QMenu *>(QStringLiteral("RibbonAddToGraphMenu"))) {
            CHECK_COND(addMenu->actions().contains(
                               mw->findChild<QAction *>(QStringLiteral("actionE11ErrorBand"))),
                       "65f: ribbon Add menu lacks the error band");
        }
        ExtraPlotTypes::registerTranslations();
        const QString ja = ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("Add Error &Band..."));
        CHECK_COND(ja.contains(QString::fromUtf8("エラーバンド")), "65f: Japanese for error band");
        const QString jg = ToriGraphFullJapaneseTranslator::lookupJapanese(
                QStringLiteral("&Gumbel (Extreme Value) Probability Plot..."));
        CHECK_COND(jg.contains(QString::fromUtf8("極値")), "65f: Japanese for Gumbel plot");
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 65 E11 extra graph types passed!\n";
    return 0;
}

#include <QToolButton>
#include <functional>

// ============================================================================
// Suite 70: H8 icon clean-up and the light-only theme.
//   70a  the legacy .xpm / 16x16 / 32x32 / icons/dark|light resources are gone, the three
//        cursor bitmaps are still there and load
//   70b  key actions (File/Edit/View/Plot/3D) carry a registered duotone icon
//   70c  every icon in the menu bar, ribbon, mini toolbar and inspector that has an icon
//        name uses a registered duotone id; the main menus contain no nameless raster icons
//   70d  column type / formula decoration icons come from the duotone registry
//   70e  no icon name fell back to a keyword guess (IconLoader::unresolvedIconNames)
//   70f  a dark OS palette does not leak into the app: the theme re-pins the light palette
// ============================================================================
int runSuite70IconCleanupAndLightTheme(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 70: icon clean-up (duotone ids only) and light theme...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 70");
    mw->show();
    QApplication::processEvents();
    qApp->setProperty("torigraph_noninteractive", true);

    const QStringList regList = IconLoader::registeredIconIds();
    const QSet<QString> registered(regList.cbegin(), regList.cend());
    CHECK_COND(registered.contains(QStringLiteral("visibility"))
                       && registered.contains(QStringLiteral("inspector-panel")),
               "70: new duotone ids (visibility, inspector-panel) are not registered");

    // ---------------- 70a: resources ----------------
    {
        const char *gone[] = { ":/fileopen.xpm", ":/new_graph.xpm", ":/close.xpm",
                               ":/16x16/fx.png", ":/32x32/fx.png", ":/24x24/table.png",
                               ":/numerictype.png", ":/equals.png",
                               ":/icons/light/16/edit-copy.svg",
                               ":/icons/dark/24/edit-undo.svg" };
        for (const char *r : gone)
            CHECK_COND(!QFile::exists(QString::fromLatin1(r)),
                       "70a: a removed legacy icon resource is still compiled in");
        const char *cursors[] = { ":/vizor.xpm", ":/cursor.xpm", ":/lens.xpm" };
        for (const char *r : cursors) {
            CHECK_COND(QFile::exists(QString::fromLatin1(r)), "70a: cursor bitmap missing");
            CHECK_COND(!QPixmap(QString::fromLatin1(r)).isNull(), "70a: cursor bitmap does not load");
        }
        CHECK_COND(QFile::exists(QStringLiteral(":/appicon")), "70a: application icon missing");
        // Legacy names still resolve (old call sites / saved toolbar states) without falling back.
        CHECK_COND(IconLoader::resolveIconId(QStringLiteral(":/fileopen.xpm")) == QStringLiteral("file-open"),
                   "70a: legacy name :/fileopen.xpm no longer maps to file-open");
        CHECK_COND(IconLoader::resolveIconId(QStringLiteral("edit-copy")) == QStringLiteral("copy"),
                   "70a: freedesktop name edit-copy no longer maps to copy");
    }

    auto iconId = [](const QIcon &ic) -> QString {
        QString n = ic.name();
        if (n.endsWith(QStringLiteral(":dark")))
            n.chop(5);
        return n;
    };
    auto iconHasInk = [](const QIcon &ic) -> bool {
        const QImage img = ic.pixmap(QSize(20, 20)).toImage().convertToFormat(QImage::Format_ARGB32);
        if (img.isNull())
            return false;
        int ink = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (qAlpha(img.pixel(x, y)) > 64)
                    ++ink;
        return ink >= 12;
    };

    // ---------------- 70b: key actions ----------------
    ModernUiTheme::upgradeAllWindowIcons(mw);
    {
        struct KeyAction {
            const char *objectName;
            bool mustExist;
        };
        const KeyAction keys[] = {
            { "actionNewProject", true },   { "actionOpen", true },
            { "actionSaveProject", true },  { "actionUndo", true },
            { "actionRedo", true },         { "actionCutSelection", true },
            { "actionCopySelection", true }, { "actionPasteSelection", true },
            { "actionNewTable", true },     { "actionNewGraph", true },
            { "actionNewMatrix", true },    { "actionNewNote", true },
            { "actionShowLog", true },      { "actionShowExplorer", true },
            { "actionUnzoom", true },       { "actionNewLegend", true },
            { "actionAddImage", true },     { "actionToggleGraphInspector", true },
            { "actionPlotHorSteps", false }, { "actionPerspective", false },
            { "actionResetRotation", false }, { "actionFitFrame", false },
            { "actionAnimate", false },
        };
        int checked = 0;
        for (const KeyAction &k : keys) {
            QAction *a = mw->findChild<QAction *>(QString::fromLatin1(k.objectName));
            if (!a) {
                if (k.mustExist)
                    std::cerr << "[FAIL] 70b: action " << k.objectName << " not found\n";
                CHECK_COND(!k.mustExist, "70b: key action missing");
                continue;
            }
            const QString id = iconId(a->icon());
            if (a->icon().isNull() || !registered.contains(id) || !iconHasInk(a->icon())) {
                std::cerr << "[FAIL] 70b: " << k.objectName << " icon='" << id.toStdString()
                          << "' null=" << a->icon().isNull() << "\n";
            }
            CHECK_COND(!a->icon().isNull(), "70b: key action has no icon");
            CHECK_COND(registered.contains(id), "70b: key action icon is not a duotone id");
            CHECK_COND(iconHasInk(a->icon()), "70b: key action icon renders empty");
            ++checked;
        }
        CHECK_COND(checked >= 18, "70b: too few key actions checked");
        // A few semantic choices that used to fall back to the generic graph icon.
        auto idOf = [&](const char *nm) {
            QAction *a = mw->findChild<QAction *>(QString::fromLatin1(nm));
            return a ? iconId(a->icon()) : QString();
        };
        CHECK_COND(idOf("actionOpen") == QStringLiteral("file-open"), "70b: Open icon");
        CHECK_COND(idOf("actionUndo") == QStringLiteral("undo"), "70b: Undo icon");
        CHECK_COND(idOf("actionNewTable") == QStringLiteral("new-table"), "70b: New Table icon");
    }

    // ---------------- 70c: menus, ribbon, mini toolbar, inspector ----------------
    QStringList offenders;
    auto checkAction = [&](QAction *a, const QString &where, bool nameless_is_error) {
        if (!a || a->isSeparator() || a->icon().isNull())
            return;
        const QString id = iconId(a->icon());
        if (id.isEmpty()) {
            if (nameless_is_error)
                offenders << where + QStringLiteral(": nameless raster icon on '") + a->text()
                                + QStringLiteral("'");
            return;
        }
        if (!registered.contains(id))
            offenders << where + QStringLiteral(": '") + id + QStringLiteral("' on '") + a->text()
                            + QStringLiteral("'");
    };
    std::function<void(QMenu *, const QString &, int)> walkMenu = [&](QMenu *m, const QString &where,
                                                                      int depth) {
        if (!m || depth > 6)
            return;
        for (QAction *a : m->actions()) {
            checkAction(a, where, true);
            if (QMenu *sub = a->menu())
                walkMenu(sub, where + QStringLiteral(" > ") + sub->title(), depth + 1);
        }
    };
    int menuIcons = 0;
    if (QMenuBar *mb = mw->menuBar()) {
        for (QAction *top : mb->actions()) {
            if (QMenu *m = top->menu()) {
                walkMenu(m, m->title(), 0);
                for (QAction *a : m->actions())
                    if (!a->icon().isNull())
                        ++menuIcons;
            }
        }
    }
    CHECK_COND(menuIcons >= 20, "70c: the main menus show almost no icons");

    int ribbonIcons = 0;
    if (ModernRibbonBar *ribbon = mw->modernRibbonBar()) {
        for (QToolButton *b : ribbon->findChildren<QToolButton *>()) {
            if (b->icon().isNull())
                continue;
            const QString id = iconId(b->icon());
            if (id.isEmpty())
                continue; // colour swatches and similar painted icons
            if (!registered.contains(id))
                offenders << QStringLiteral("ribbon: '") + id + QStringLiteral("' on '")
                                + b->text() + QStringLiteral("'");
            else
                ++ribbonIcons;
        }
        for (QMenu *m : ribbon->findChildren<QMenu *>())
            walkMenu(m, QStringLiteral("ribbon menu ") + m->title(), 0);
    }
    CHECK_COND(ribbonIcons >= 20, "70c: the ribbon shows too few duotone icons");

    Table *t70 = mw->newTable("T70", 5, 2);
    CHECK_COND(t70 != nullptr, "70c: table");
    for (int r = 0; r < 5; ++r) {
        t70->setCell(r, 0, r);
        t70->setCell(r, 1, r * r);
    }
    MultiLayer *ml70 = mw->newGraph("G70");
    CHECK_COND(ml70 && ml70->activeGraph(), "70c: graph");
    Graph *g70 = ml70->activeGraph();
    CHECK_COND(g70->insertCurve(t70, "T70_1", "T70_2", Graph::LineSymbols), "70c: curve");
    QApplication::processEvents();

    {
        const bool wasEnabled = GraphMiniToolbar::isMiniToolbarEnabled();
        GraphMiniToolbar::setMiniToolbarEnabled(true);
        GraphMiniToolbar *mtb = GraphMiniToolbar::instance();
        CHECK_COND(mtb != nullptr, "70c: mini toolbar instance");
        // The pop-up is suppressed in non-interactive mode and normally waits for a timer;
        // lift the flag and fire the timer slot directly so the buttons are built now.
        qApp->setProperty("torigraph_noninteractive", QVariant());
        auto countMini = [&](const QString &where) {
            int n = 0;
            for (QToolButton *b : mtb->findChildren<QToolButton *>()) {
                if (b->icon().isNull())
                    continue;
                const QString id = iconId(b->icon());
                if (id.isEmpty())
                    continue; // colour swatch / painted symbol preview
                if (!registered.contains(id))
                    offenders << where + QStringLiteral(": '") + id + QStringLiteral("'");
                else
                    ++n;
            }
            return n;
        };
        ml70->showNormal();
        QApplication::processEvents();
        mtb->popupForCanvas(g70, QPoint(240, 240));
        QMetaObject::invokeMethod(mtb, "onPopupTimerTimeout", Qt::DirectConnection);
        QApplication::processEvents();
        const int canvasIcons = countMini(QStringLiteral("mini toolbar (canvas)"));
        mtb->popupForCurve(g70, 0, QPoint(240, 240));
        QMetaObject::invokeMethod(mtb, "onPopupTimerTimeout", Qt::DirectConnection);
        QApplication::processEvents();
        const int curveIcons = countMini(QStringLiteral("mini toolbar (curve)"));
        mtb->hide();
        qApp->setProperty("torigraph_noninteractive", true);
        GraphMiniToolbar::setMiniToolbarEnabled(wasEnabled);
        std::cout << "[INFO] 70c mini toolbar duotone icons: canvas=" << canvasIcons
                  << " curve=" << curveIcons << std::endl;
        CHECK_COND(canvasIcons >= 3 && curveIcons >= 2,
                   "70c: the mini toolbar shows too few duotone icons");
    }

    {
        GraphInspectorDock *dock = mw->graphInspectorDock();
        CHECK_COND(dock != nullptr, "70c: inspector dock");
        int inspIcons = 0;
        for (QAbstractButton *b : dock->findChildren<QAbstractButton *>()) {
            if (b->icon().isNull())
                continue;
            const QString id = iconId(b->icon());
            if (id.isEmpty())
                continue; // colour swatch
            if (!registered.contains(id))
                offenders << QStringLiteral("inspector: '") + id + QStringLiteral("' on '")
                                + b->objectName() + QStringLiteral("'");
            else
                ++inspIcons;
        }
        for (QTreeWidget *tw : dock->findChildren<QTreeWidget *>()) {
            QTreeWidgetItemIterator it(tw);
            while (*it) {
                const QIcon ic = (*it)->icon(0);
                const QString id = iconId(ic);
                if (!ic.isNull() && !id.isEmpty() && !registered.contains(id))
                    offenders << QStringLiteral("inspector tree: '") + id + QStringLiteral("'");
                ++it;
            }
        }
        CHECK_COND(inspIcons >= 3, "70c: the inspector shows too few duotone icons");
    }

    for (const QString &o : offenders)
        std::cerr << "[FAIL] 70c: " << o.toUtf8().constData() << "\n";
    CHECK_COND(offenders.isEmpty(), "70c: icons outside the duotone registry are still shown");

    // ---------------- 70d: column / table decoration icons ----------------
    {
        Column *c0 = t70->column(0);
        CHECK_COND(c0 != nullptr, "70d: column");
        CHECK_COND(iconId(c0->icon()) == QStringLiteral("row-numbers"),
                   "70d: numeric column icon is not the duotone row-numbers icon");
        CHECK_COND(iconHasInk(c0->icon()), "70d: numeric column icon renders empty");
        CHECK_COND(registered.contains(iconId(IconLoader::load(QStringLiteral("formula")))),
                   "70d: formula icon");
    }

    // ---------------- 70e: no fallback guesses ----------------
    {
        const QStringList unresolved = IconLoader::unresolvedIconNames();
        for (const QString &u : unresolved)
            std::cerr << "[FAIL] 70e: icon name without a registered id or alias: '"
                      << u.toUtf8().constData() << "'\n";
        CHECK_COND(unresolved.isEmpty(), "70e: some icon names fell back to a keyword guess");
    }

    // ---------------- 70f: dark OS palette does not leak ----------------
    {
        const QPalette before = qApp->palette();
        QPalette dark = before;
        dark.setColor(QPalette::Window, QColor(0x20, 0x20, 0x20));
        dark.setColor(QPalette::WindowText, QColor(0xF0, 0xF0, 0xF0));
        dark.setColor(QPalette::Base, QColor(0x10, 0x10, 0x10));
        dark.setColor(QPalette::Text, QColor(0xF0, 0xF0, 0xF0));
        dark.setColor(QPalette::Button, QColor(0x30, 0x30, 0x30));
        dark.setColor(QPalette::ButtonText, QColor(0xF0, 0xF0, 0xF0));
        dark.setColor(QPalette::ToolTipBase, QColor(0x20, 0x20, 0x20));
        dark.setColor(QPalette::ToolTipText, QColor(0x20, 0x20, 0x20));
        qApp->setPalette(dark);
        ModernUiTheme::applyModernTheme(qApp);
        const QPalette pinned = qApp->palette();
        auto lum = [](const QColor &c) {
            auto ch = [](double v) {
                v /= 255.0;
                return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * ch(c.red()) + 0.7152 * ch(c.green()) + 0.0722 * ch(c.blue());
        };
        auto contrast = [&](const QColor &a, const QColor &b) {
            const double la = lum(a), lb = lum(b);
            return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
        };
        const bool lightWindow = pinned.color(QPalette::Window).lightness() >= 230
                && pinned.color(QPalette::Base).lightness() >= 240;
        const double cText = contrast(pinned.color(QPalette::Window), pinned.color(QPalette::WindowText));
        const double cBase = contrast(pinned.color(QPalette::Base), pinned.color(QPalette::Text));
        const double cButton = contrast(pinned.color(QPalette::Button), pinned.color(QPalette::ButtonText));
        const double cTip = contrast(pinned.color(QPalette::ToolTipBase), pinned.color(QPalette::ToolTipText));
        const bool tooltipQss = qApp->styleSheet().contains(QStringLiteral("QToolTip"));
        const bool menuQss = qApp->styleSheet().contains(QStringLiteral("QMenu {"));
        // A message box is a Qt widget dialog and must follow the pinned palette.
        QMessageBox box(QMessageBox::Information, QStringLiteral("T"), QStringLiteral("text"));
        const QPalette boxPal = box.palette();
        const bool boxLight = boxPal.color(QPalette::Window).lightness() >= 230
                && contrast(boxPal.color(QPalette::Window), boxPal.color(QPalette::WindowText)) >= 7.0;
        qApp->setPalette(before);
        CHECK_COND(lightWindow, "70f: a dark incoming palette leaked into the theme");
        CHECK_COND(cText >= 7.0 && cBase >= 7.0 && cButton >= 7.0,
                   "70f: window / base / button text contrast below 7:1");
        CHECK_COND(cTip >= 7.0, "70f: tooltip text contrast below 7:1");
        CHECK_COND(tooltipQss && menuQss, "70f: tooltips and menus are not styled by the theme");
        CHECK_COND(boxLight, "70f: QMessageBox does not use the light palette");
    }

    if (ml70) {
        ml70->askOnCloseEvent(false);
        ml70->close();
    }
    if (t70) {
        t70->askOnCloseEvent(false);
        t70->close();
    }
    QApplication::processEvents();
    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 70 icon clean-up and light theme passed!\n";
    return 0;
}

// ============================================================================
// Suite 67: E11b 2D density (2D histogram) and XRD pole figure
// ============================================================================

namespace {

struct Suite67Rng {
    quint64 s = 0x9E3779B97F4A7C15ull;
    double uniform() // (0, 1)
    {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return (double((s >> 11) & ((1ull << 53) - 1)) + 0.5) / 9007199254740992.0;
    }
    void normalPair(double &z1, double &z2)
    {
        const double u1 = uniform(), u2 = uniform();
        const double r = std::sqrt(-2.0 * std::log(u1));
        z1 = r * std::cos(2.0 * 3.14159265358979323846 * u2);
        z2 = r * std::sin(2.0 * 3.14159265358979323846 * u2);
    }
};

Spectrogram *suite67Spectrogram(Graph *g)
{
    if (!g)
        return nullptr;
    for (int i = 0; i < g->curves(); ++i)
        if (QwtPlotItem *it = g->plotItem(i))
            if (it->rtti() == QwtPlotItem::Rtti_PlotSpectrogram)
                return static_cast<Spectrogram *>(it);
    return nullptr;
}

bool suite67HasPoleFrame(Graph *g)
{
    if (!g)
        return false;
    for (QwtPlotItem *it : g->plotWidget()->itemList())
        if (it && it->rtti() == QwtPlotItem::Rtti_PlotUserItem + 67)
            return true;
    return false;
}

} // namespace

int runSuite67Density2DAndPoleFigure(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 67: E11b 2D Density (2D Histogram) and XRD Pole Figure...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 67");
    qApp->setProperty("torigraph_noninteractive", true);
    QTemporaryDir tmpDir67;
    CHECK_COND(tmpDir67.isValid(), "Suite 67: Failed to create QTemporaryDir");

    // ---------------- 67a: exact small 2D histogram ----------------
    {
        const QVector<double> x = { 0.1, 0.4, 1.2, 1.9, 2.0, std::nan("") };
        const QVector<double> y = { 0.1, 0.2, 0.3, 1.5, 1.99, 0.5 };
        const ExtraPlotTypes::Density2DResult r = ExtraPlotTypes::binDensity2D(x, y, 0, 0, 1.0, 1.0);
        CHECK_COND(r.valid && r.n == 5 && r.excluded == 1, "67a: valid / n / excluded");
        CHECK_COND(r.binsX == 3 && r.binsY == 2, "67a: x needs 3 bins (2.0 sits on an edge), y 2 bins");
        CHECK_COND(r.x0 == 0.0 && r.y0 == 0.0 && r.widthX == 1.0 && r.widthY == 1.0, "67a: edges");
        const double exp[6] = { 2, 1, 0, 0, 1, 1 };
        for (int k = 0; k < 6; ++k)
            CHECK_COND(r.counts[k] == exp[k], "67a: count per bin differs from the hand count");
        CHECK_COND(r.maxCount == 2.0, "67a: max count");
        // bin-count mode: [min, max] split evenly, the maximum falls in the last bin
        const ExtraPlotTypes::Density2DResult rb = ExtraPlotTypes::binDensity2D(x, y, 4, 2);
        CHECK_COND(rb.valid && rb.binsX == 4 && suite65Near(rb.widthX, 1.9 / 4.0, 1e-15)
                           && suite65Near(rb.x0, 0.1, 1e-15),
                   "67a: bins mode spans min..max");
        double tot = 0.0;
        for (double c : rb.counts)
            tot += c;
        CHECK_COND(tot == 5.0 && rb.counts[1 * 4 + 3] == 2.0, "67a: maximum must land in the last bin");
        // automatic bins (Rice rule 2 n^(1/3), at least 5)
        const ExtraPlotTypes::Density2DResult ra = ExtraPlotTypes::binDensity2D(x, y);
        CHECK_COND(ra.valid && ra.binsX == 5 && ra.binsY == 5, "67a: automatic bin count");
        const ExtraPlotTypes::Density2DResult rbad = ExtraPlotTypes::binDensity2D(x, y, 0, 0, 1e-9, 1.0);
        CHECK_COND(!rbad.valid && !rbad.error.isEmpty(), "67a: too many bins must be refused");
    }

    // ---------------- 67b: 2D normal distribution, 200 000 points ----------------
    const int N = 200000;
    const double mux = 1.25, muy = -0.75, sx = 1.0, sy = 2.0, w = 0.5;
    Table *tn = mw->newTable("Dens67", N, 2);
    CHECK_COND(tn != nullptr, "67b: table");
    tn->setHeader(QStringList() << "X" << "Y");
    {
        QVector<qreal> xs(N), ys(N);
        Suite67Rng rng;
        for (int i = 0; i < N; ++i) {
            double z1 = 0, z2 = 0;
            rng.normalPair(z1, z2);
            xs[i] = mux + sx * z1;
            ys[i] = muy + sy * z2;
        }
        tn->column(0)->replaceValues(0, xs);
        tn->column(1)->replaceValues(0, ys);
    }
    QElapsedTimer timer67;
    timer67.start();
    const ExtraPlotTypes::Density2DResult dn =
            ExtraPlotTypes::plotDensity2D(mw, tn, "X", "Y", 0, 0, w, w, false, Spectrogram::PresetViridis);
    const qint64 msDensity = timer67.elapsed();
    std::cout << "[INFO] 67b: 2D density of " << N << " points (table read + bin + matrix + graph) took "
              << msDensity << " ms\n";
    CHECK_COND(dn.valid && dn.matrix && dn.plot, "67b: plotDensity2D failed");
    CHECK_COND(dn.n == N && dn.excluded == 0, "67b: all points counted");
    CHECK_COND(msDensity < 5000, "67b: 200 000 points must be binned quickly");
    {
        // compare every well-populated bin with N * Px * Py of the independent normals
        int checked = 0;
        double total = 0.0;
        int argmax = 0;
        for (int iy = 0; iy < dn.binsY; ++iy)
            for (int ix = 0; ix < dn.binsX; ++ix) {
                const double c = dn.counts[iy * dn.binsX + ix];
                total += c;
                if (c > dn.counts[argmax])
                    argmax = iy * dn.binsX + ix;
                const double xa = dn.x0 + ix * w, ya = dn.y0 + iy * w;
                const double px = gsl_cdf_ugaussian_P((xa + w - mux) / sx) - gsl_cdf_ugaussian_P((xa - mux) / sx);
                const double py = gsl_cdf_ugaussian_P((ya + w - muy) / sy) - gsl_cdf_ugaussian_P((ya - muy) / sy);
                const double e = N * px * py;
                if (e < 200.0)
                    continue;
                ++checked;
                CHECK_COND(std::fabs(c - e) <= 5.0 * std::sqrt(e) + 2.0,
                           "67b: bin count differs from the 2D normal probability");
            }
        CHECK_COND(total == N, "67b: counts must add up to N");
        CHECK_COND(checked > 40, "67b: too few bins compared");
        // the mean (1.25, -0.75) is a bin centre; the fullest bin must be at (or next to) it
        const double cx = dn.x0 + (argmax % dn.binsX + 0.5) * w;
        const double cy = dn.y0 + (argmax / dn.binsX + 0.5) * w;
        CHECK_COND(std::fabs(cx - mux) <= w + 1e-9 && std::fabs(cy - muy) <= w + 1e-9,
                   "67b: maximum bin is not at the mean");
        std::cout << "[INFO] 67b: " << dn.binsX << " x " << dn.binsY << " bins, max " << dn.maxCount
                  << " at (" << cx << ", " << cy << "), " << checked << " bins within 5 sigma\n";
    }
    {
        Matrix *m = dn.matrix;
        CHECK_COND(m->numRows() == dn.binsY && m->numCols() == dn.binsX, "67b: matrix size");
        CHECK_COND(suite65Near(m->xStart(), dn.x0 + 0.5 * w, 1e-12)
                           && suite65Near(m->yEnd(), dn.y0 + (dn.binsY - 0.5) * w, 1e-12),
                   "67b: matrix coordinates must be the bin centres");
        double sum = 0.0;
        bool same = true;
        for (int r = 0; r < m->numRows(); ++r)
            for (int c = 0; c < m->numCols(); ++c) {
                sum += m->cell(r, c);
                same = same && m->cell(r, c) == dn.counts[r * dn.binsX + c];
            }
        CHECK_COND(same && sum == N, "67b: matrix must hold the counts (row = Y bin, column = X bin)");
        CHECK_COND(m->windowLabel().contains("Dens67"), "67b: matrix label must name the source");
        Graph *g = dn.plot->activeGraph();
        Spectrogram *sp = suite67Spectrogram(g);
        CHECK_COND(sp && sp->cellBlocks() && !sp->logColorScale(), "67b: spectrogram with flat cells");
        CHECK_COND(g->plotWidget()->axisWidget(QwtPlot::yRight)->isColorBarEnabled(),
                   "67b: colour bar missing");
        const QwtScaleDiv *d = g->plotWidget()->axisScaleDiv(QwtPlot::yRight);
        CHECK_COND(d && suite65Near(d->upperBound(), dn.maxCount, 1e-9), "67b: colour bar max = max count");
        const QwtScaleDiv *dx = g->plotWidget()->axisScaleDiv(QwtPlot::xBottom);
        CHECK_COND(dx && suite65Near(dx->lowerBound(), dn.x0, 1e-9)
                           && suite65Near(dx->upperBound(), dn.x0 + dn.binsX * w, 1e-9),
                   "67b: X axis must span the bin edges");
        const QwtDoubleRect br = sp->boundingRect();
        CHECK_COND(suite65Near(br.left(), dn.x0, 1e-9) && suite65Near(br.right(), dn.x0 + dn.binsX * w, 1e-9),
                   "67b: flat cells must cover the full bins");
        // a point inside one bin takes exactly that bin's count (no smoothing)
        const double v = sp->data().value(dn.x0 + 0.3 * w, dn.y0 + 0.7 * w);
        CHECK_COND(v == dn.counts[0], "67b: cell value must be the bin count");
        CHECK_COND(!ExtraPlotTypes::graphTag(g, "ToriDensity2D").isEmpty(), "67b: source/settings not recorded");
    }
    // speed of the binning itself with 10^6 points
    {
        QVector<double> bx(1000000), by(1000000);
        Suite67Rng rng;
        for (int i = 0; i < bx.size(); ++i)
            rng.normalPair(bx[i], by[i]);
        timer67.restart();
        const ExtraPlotTypes::Density2DResult big = ExtraPlotTypes::binDensity2D(bx, by, 200, 200);
        const qint64 ms = timer67.elapsed();
        std::cout << "[INFO] 67b: binning 1 000 000 points into 200 x 200 took " << ms << " ms\n";
        CHECK_COND(big.valid && big.n == 1000000 && ms < 2000, "67b: 10^6 points binning too slow");
    }
    // log colour scale
    const ExtraPlotTypes::Density2DResult dl =
            ExtraPlotTypes::plotDensity2D(mw, tn, "X", "Y", 40, 40, 0.0, 0.0, true, Spectrogram::PresetMagma);
    CHECK_COND(dl.valid && dl.plot && dl.binsX == 40, "67b: log density failed");
    const QString densLogName = dl.plot->name();
    const QString densName = dn.plot->name();
    {
        Graph *g = dl.plot->activeGraph();
        Spectrogram *sp = suite67Spectrogram(g);
        CHECK_COND(sp && sp->logColorScale(), "67b: log flag");
        CHECK_COND(Plot::isLog10ScaleEngine(g->plotWidget()->axisScaleEngine(QwtPlot::yRight)),
                   "67b: colour bar axis must be logarithmic");
        double minPos = 1e300;
        for (double c : dl.counts)
            if (c > 0.0)
                minPos = qMin(minPos, c);
        const QwtScaleDiv *d = g->plotWidget()->axisScaleDiv(QwtPlot::yRight);
        CHECK_COND(d && suite65Near(d->lowerBound(), minPos, 1e-6 * minPos)
                           && suite65Near(d->upperBound(), dl.maxCount, 1e-6 * dl.maxCount),
                   "67b: log colour bar must run from the smallest non-zero count to the maximum");
        int emptyIdx = -1;
        for (int k = 0; k < dl.counts.size() && emptyIdx < 0; ++k)
            if (dl.counts[k] == 0.0)
                emptyIdx = k;
        CHECK_COND(emptyIdx >= 0, "67b: expected an empty corner bin");
        const double vx = dl.x0 + (emptyIdx % dl.binsX + 0.5) * dl.widthX;
        const double vy = dl.y0 + (emptyIdx / dl.binsX + 0.5) * dl.widthY;
        CHECK_COND(std::isnan(sp->data().value(vx, vy)), "67b: empty bin must be transparent in log mode");
    }
    // rebuild from the recorded source after the data changed
    {
        QVector<qreal> nanX(1000, std::nan(""));
        tn->column(0)->replaceValues(0, nanX);
        CHECK_COND(ExtraPlotTypes::rebuildDensity2D(mw, dn.plot->activeGraph()), "67b: rebuild failed");
        double sum = 0.0;
        for (int r = 0; r < dn.matrix->numRows(); ++r)
            for (int c = 0; c < dn.matrix->numCols(); ++c)
                sum += dn.matrix->cell(r, c);
        CHECK_COND(sum == N - 1000, "67b: rebuilt matrix must count the remaining points");
        CHECK_COND(!ExtraPlotTypes::rebuildDensity2D(mw, nullptr), "67b: rebuild of nothing must fail");
    }
    CHECK_COND(!ExtraPlotTypes::plotDensity2D(mw, tn, "X", "nope").valid, "67b: missing column must fail");

    // ---------------- 67c: pole figure ----------------
    {
        double x = 0, y = 0;
        CHECK_COND(ExtraPlotTypes::poleProjection(45.0, 30.0, ExtraPlotTypes::EqualArea, &x, &y)
                           && suite65Near(x, 0.468689571, 1e-8) && suite65Near(y, 0.270598050, 1e-8),
                   "67c: Schmidt projection of (45°, 30°) != sqrt(2) sin 22.5° (cos 30°, sin 30°)");
        CHECK_COND(ExtraPlotTypes::poleProjection(45.0, 30.0, ExtraPlotTypes::EqualAngle, &x, &y)
                           && suite65Near(x, 0.358719468, 1e-8) && suite65Near(y, 0.207106781, 1e-8),
                   "67c: Wulff projection of (45°, 30°) != tan 22.5° (cos 30°, sin 30°)");
        CHECK_COND(ExtraPlotTypes::poleProjection(90.0, 0.0, ExtraPlotTypes::EqualArea, &x, &y)
                           && suite65Near(x, 1.0, 1e-12) && suite65Near(y, 0.0, 1e-12),
                   "67c: alpha = 90° must be on the rim");
        CHECK_COND(ExtraPlotTypes::poleProjection(45.0, 30.0, ExtraPlotTypes::EqualArea, &x, &y, true, true)
                           && suite65Near(x, 0.270598050, 1e-8) && suite65Near(y, 0.468689571, 1e-8),
                   "67c: beta 0° at the top, clockwise");
        CHECK_COND(!ExtraPlotTypes::poleProjection(95.0, 0.0, ExtraPlotTypes::EqualArea, &x, &y),
                   "67c: alpha > 90° must be refused");
        double a = 0, b = 0;
        CHECK_COND(ExtraPlotTypes::inversePoleProjection(0.468689571, 0.270598050, ExtraPlotTypes::EqualArea, &a, &b)
                           && suite65Near(a, 45.0, 1e-6) && suite65Near(b, 30.0, 1e-6),
                   "67c: inverse Schmidt projection");
        CHECK_COND(ExtraPlotTypes::inversePoleProjection(0.270598050, 0.468689571, ExtraPlotTypes::EqualArea, &a, &b, true, true)
                           && suite65Near(a, 45.0, 1e-6) && suite65Near(b, 30.0, 1e-6),
                   "67c: inverse projection (beta 0° at the top, clockwise)");
    }
    // synthetic XRD scan: alpha 0..75 (5°), beta 0..360 (5°, 360 duplicates 0), peak at (45°, 30°)
    const int nA = 16, nB = 73;
    Table *tp = mw->newTable("Pole67", nA * nB, 4);
    CHECK_COND(tp != nullptr, "67c: table");
    tp->setHeader(QStringList() << "alpha" << "beta" << "I" << "flat");
    {
        QVector<qreal> ca(nA * nB), cb(nA * nB), ci(nA * nB), cf(nA * nB, 50.0);
        int k = 0;
        for (int i = 0; i < nA; ++i)
            for (int j = 0; j < nB; ++j, ++k) {
                ca[k] = 5.0 * i;
                cb[k] = 5.0 * j;
                ci[k] = suite67PoleIntensity(5.0 * i, 5.0 * j);
            }
        tp->column(0)->replaceValues(0, ca);
        tp->column(1)->replaceValues(0, cb);
        tp->column(2)->replaceValues(0, ci);
        tp->column(3)->replaceValues(0, cf);
    }
    ExtraPlotTypes::PoleFigureOptions po;
    po.label = "(111)";
    const ExtraPlotTypes::PoleFigureResult pf =
            ExtraPlotTypes::plotPoleFigure(mw, tp, "alpha", "beta", "I", po);
    CHECK_COND(pf.valid && pf.matrix && pf.plot, "67c: pole figure failed");
    const QString poleName = pf.plot->name();
    const QString poleMatrixName = pf.matrix->name();
    CHECK_COND(pf.nAlpha == 16 && pf.nBeta == 72, "67c: beta 360 must merge into 0 (72 beta steps)");
    CHECK_COND(suite65Near(pf.alphaMax, 75.0, 1e-9) && pf.missing == 0, "67c: measured alpha range");
    CHECK_COND(suite65Near(pf.maxAlpha, 45.0, 1e-9) && suite65Near(pf.maxBeta, 30.0, 1e-9),
               "67c: maximum must be at alpha 45°, beta 30°");
    CHECK_COND(suite65Near(pf.maxX, 0.468689571, 1e-8) && suite65Near(pf.maxY, 0.270598050, 1e-8),
               "67c: projected maximum differs from the hand value");
    CHECK_COND(suite65Near(pf.maxValue * pf.meanIntensity, 1000.0, 1e-6),
               "67c: MRD = intensity / solid-angle mean");
    CHECK_COND(pf.meanIntensity > 100.0 && pf.meanIntensity < 300.0, "67c: mean intensity implausible");
    {
        Matrix *m = pf.matrix;
        const int Nm = m->numRows();
        CHECK_COND(Nm == 201 && m->numCols() == 201, "67c: projected grid 201 x 201");
        auto cellAt = [&](double x, double y) {
            const int c = qRound((x + 1.0) / 2.0 * (Nm - 1));
            const int r = qRound((y + 1.0) / 2.0 * (Nm - 1));
            return m->cell(r, c);
        };
        // matrix maximum close to the hand-computed projected position
        double best = -1e300;
        int br = 0, bc = 0;
        for (int r = 0; r < Nm; ++r)
            for (int c = 0; c < Nm; ++c)
                if (std::isfinite(m->cell(r, c)) && m->cell(r, c) > best) {
                    best = m->cell(r, c);
                    br = r;
                    bc = c;
                }
        const double bx = -1.0 + 2.0 * bc / (Nm - 1), by = -1.0 + 2.0 * br / (Nm - 1);
        std::cout << "[INFO] 67c: projected maximum " << best << " MRD at (" << bx << ", " << by
                  << "), hand value (0.46869, 0.27060)\n";
        CHECK_COND(std::fabs(bx - 0.468689571) <= 0.015 && std::fabs(by - 0.270598050) <= 0.015,
                   "67c: projected matrix maximum not at the hand-computed position");
        CHECK_COND(best <= pf.maxValue + 1e-9 && best > 0.97 * pf.maxValue, "67c: matrix maximum value");
        CHECK_COND(std::isnan(cellAt(0.99, 0.99)), "67c: outside the circle must be NaN");
        double x85 = 0, y85 = 0, x60 = 0, y60 = 0;
        ExtraPlotTypes::poleProjection(85.0, 200.0, ExtraPlotTypes::EqualArea, &x85, &y85);
        ExtraPlotTypes::poleProjection(60.0, 200.0, ExtraPlotTypes::EqualArea, &x60, &y60);
        CHECK_COND(std::isnan(cellAt(x85, y85)), "67c: unmeasured alpha 85° must be NaN");
        CHECK_COND(std::isfinite(cellAt(x60, y60)), "67c: measured alpha 60° must have a value");
        CHECK_COND(std::isfinite(cellAt(0.0, 0.0)), "67c: centre must have a value");

        Graph *g = pf.plot->activeGraph();
        Spectrogram *sp = suite67Spectrogram(g);
        CHECK_COND(sp && sp->maskInvalidCells() && sp->testDisplayMode(QwtPlotSpectrogram::ContourMode)
                           && sp->testDisplayMode(QwtPlotSpectrogram::ImageMode),
                   "67c: masked colour fill + contour lines");
        CHECK_COND(sp->levels() == 8, "67c: contour levels");
        CHECK_COND(suite67HasPoleFrame(g), "67c: outer circle / ticks / RD-TD frame missing");
        CHECK_COND(!g->plotWidget()->axisEnabled(QwtPlot::xBottom) && !g->plotWidget()->axisEnabled(QwtPlot::yLeft),
                   "67c: X/Y axes must be hidden");
        CHECK_COND(g->plotWidget()->axisWidget(QwtPlot::yRight)->isColorBarEnabled(), "67c: colour bar");
        const QwtScaleDiv *d = g->plotWidget()->axisScaleDiv(QwtPlot::yRight);
        CHECK_COND(d && d->upperBound() <= pf.maxValue + 1e-9 && d->upperBound() > 0.97 * pf.maxValue,
                   "67c: colour bar must reach the maximum MRD");
        bool mrdBox = false;
        for (QwtPlotItem *mk : g->plotWidget()->itemList())
            if (auto *lg = dynamic_cast<Legend *>(mk))
                if (lg->text().contains("MRD") && lg->text().contains("(111)"))
                    mrdBox = true;
        CHECK_COND(mrdBox, "67c: maximum (MRD) summary box missing");
        CHECK_COND(!ExtraPlotTypes::graphTag(g, "ToriPoleFigure").isEmpty(), "67c: pole figure tag missing");

        // pixels: unmeasured ring and outside the circle stay white, measured area is coloured
        pf.plot->resize(760, 640);
        g->replot();
        QwtPlotCanvas *cv = g->plotWidget()->canvas();
        std::cout << "[INFO] 67c: canvas " << cv->width() << " x " << cv->height() << " px\n";
        const QImage img = cv->grab().toImage();
        const Plot *pw = g->plotWidget();
        const double dpr = cv->width() > 0 ? double(img.width()) / double(cv->width()) : 1.0;
        auto px = [&](double x, double y) {
            return img.pixelColor(qRound(pw->transform(QwtPlot::xBottom, x) * dpr),
                                  qRound(pw->transform(QwtPlot::yLeft, y) * dpr));
        };
        auto isWhite = [](const QColor &c) { return c.red() > 245 && c.green() > 245 && c.blue() > 245; };
        CHECK_COND(isWhite(px(1.15, -1.15)), "67c: outside the circle must be blank");
        CHECK_COND(isWhite(px(x85, y85)), "67c: unmeasured alpha must be blank (not filled)");
        CHECK_COND(!isWhite(px(x60, y60)) && !isWhite(px(0.1, 0.1)), "67c: measured area must be coloured");
    }
    // uniform intensity -> 1 MRD everywhere
    {
        const ExtraPlotTypes::PoleFigureResult flat =
                ExtraPlotTypes::plotPoleFigure(mw, tp, "alpha", "beta", "flat", po, false);
        CHECK_COND(flat.valid && suite65Near(flat.maxValue, 1.0, 1e-12) && suite65Near(flat.meanIntensity, 50.0, 1e-9),
                   "67c: uniform intensity must give 1 MRD");
        double vmin = 1e300, vmax = -1e300;
        for (int r = 0; r < flat.matrix->numRows(); ++r)
            for (int c = 0; c < flat.matrix->numCols(); ++c)
                if (std::isfinite(flat.matrix->cell(r, c))) {
                    vmin = qMin(vmin, flat.matrix->cell(r, c));
                    vmax = qMax(vmax, flat.matrix->cell(r, c));
                }
        CHECK_COND(suite65Near(vmin, 1.0, 1e-9) && suite65Near(vmax, 1.0, 1e-9), "67c: flat matrix != 1 MRD");
    }
    // Wulff projection, Rigaku alpha convention, matrix input
    {
        ExtraPlotTypes::PoleFigureOptions pw = po;
        pw.projection = ExtraPlotTypes::EqualAngle;
        const ExtraPlotTypes::PoleFigureResult wf =
                ExtraPlotTypes::plotPoleFigure(nullptr, tp, "alpha", "beta", "I", pw, false);
        CHECK_COND(wf.valid && suite65Near(wf.maxX, 0.358719468, 1e-8) && suite65Near(wf.maxY, 0.207106781, 1e-8),
                   "67c: Wulff maximum position");
        Table *tr = mw->newTable("PoleRim67", nA * nB, 3);
        QVector<qreal> ca(nA * nB), cb(nA * nB), ci(nA * nB);
        int k = 0;
        for (int i = 0; i < nA; ++i)
            for (int j = 0; j < nB; ++j, ++k) {
                ca[k] = 90.0 - 5.0 * i;
                cb[k] = 5.0 * j;
                ci[k] = suite67PoleIntensity(5.0 * i, 5.0 * j);
            }
        tr->column(0)->replaceValues(0, ca);
        tr->column(1)->replaceValues(0, cb);
        tr->column(2)->replaceValues(0, ci);
        ExtraPlotTypes::PoleFigureOptions prim = po;
        prim.alphaFromRim = true;
        const ExtraPlotTypes::PoleFigureResult rf =
                ExtraPlotTypes::plotPoleFigure(nullptr, tr, tr->colName(0), tr->colName(1), tr->colName(2), prim, false);
        CHECK_COND(rf.valid && suite65Near(rf.maxAlpha, 45.0, 1e-9) && suite65Near(rf.maxX, pf.maxX, 1e-12)
                           && suite65Near(rf.alphaMax, 75.0, 1e-9),
                   "67c: Rigaku alpha (90° at the centre) must give the same figure");
        const ExtraPlotTypes::PoleFigureResult wrongConv =
                ExtraPlotTypes::plotPoleFigure(nullptr, tr, tr->colName(0), tr->colName(1), tr->colName(2), po, false);
        CHECK_COND(wrongConv.valid && suite65Near(wrongConv.alphaMin, 15.0, 1e-9), "67c: unconverted alpha range");

        Matrix *mi = mw->newMatrix("PoleIn67", nA, nB);
        CHECK_COND(mi != nullptr, "67c: input matrix");
        mi->setCoordinates(0.0, 360.0, 0.0, 75.0);
        for (int i = 0; i < nA; ++i)
            for (int j = 0; j < nB; ++j)
                mi->setCell(i, j, suite67PoleIntensity(5.0 * i, 5.0 * j));
        const ExtraPlotTypes::PoleFigureResult mf =
                ExtraPlotTypes::plotPoleFigureFromMatrix(nullptr, mi, po, false);
        CHECK_COND(mf.valid && mf.nAlpha == 16 && mf.nBeta == 72 && suite65Near(mf.maxBeta, 30.0, 1e-9)
                           && suite65Near(mf.maxValue, pf.maxValue, 1e-9),
                   "67c: alpha x beta matrix input must match the column input");

        Table *tbad = mw->newTable("PoleBad67", 6, 3);
        const double ba[6] = { 3, 17, 29, 41, 63, 77 }, bb[6] = { 11, 97, 151, 203, 287, 331 };
        for (int r = 0; r < 6; ++r) {
            tbad->setCell(r, 0, ba[r]);
            tbad->setCell(r, 1, bb[r]);
            tbad->setCell(r, 2, 1.0 + r);
        }
        const ExtraPlotTypes::PoleFigureResult bad = ExtraPlotTypes::plotPoleFigure(
                nullptr, tbad, tbad->colName(0), tbad->colName(1), tbad->colName(2), po, false);
        CHECK_COND(!bad.valid && !bad.error.isEmpty(), "67c: scattered (non-grid) data must be refused");
        tbad->setCell(0, 0, 120.0);
        CHECK_COND(!ExtraPlotTypes::plotPoleFigure(nullptr, tbad, tbad->colName(0), tbad->colName(1),
                                                   tbad->colName(2), po, false).valid,
                   "67c: alpha > 90° must be refused");
    }

    // ---------------- 67d: save / reopen ----------------
    {
        const QString prj = tmpDir67.filePath("e11b_67.sciprj");
        CHECK_COND(mw->saveFolder(mw->projectFolder(), prj), "67d: save failed");
        QFile f(prj);
        CHECK_COND(f.open(QIODevice::ReadOnly), "67d: cannot read project");
        const QByteArray txt = f.readAll();
        f.close();
        CHECK_COND(txt.contains("<ToriDensity2D") && txt.contains("<ToriPoleFigure")
                           && txt.contains("<ToriMaskInvalid>1") && txt.contains("<ToriLogColor>1")
                           && txt.contains("<ToriCellBlocks>1"),
                   "67d: E11b tags not written");
        ApplicationWindow *re = mw->openProject(prj);
        CHECK_COND(re != nullptr, "67d: reopen failed");
        MultiLayer *rd = suite65FindGraph(re, densName);
        CHECK_COND(rd && rd->activeGraph(), "67d: density graph missing");
        Spectrogram *rsp = suite67Spectrogram(rd->activeGraph());
        CHECK_COND(rsp && rsp->cellBlocks() && !rsp->logColorScale(), "67d: density flags not restored");
        CHECK_COND(ExtraPlotTypes::rebuildDensity2D(re, rd->activeGraph()), "67d: rebuild after reopening");
        MultiLayer *rl = suite65FindGraph(re, densLogName);
        CHECK_COND(rl && rl->activeGraph(), "67d: log density graph missing");
        Spectrogram *rlsp = suite67Spectrogram(rl->activeGraph());
        CHECK_COND(rlsp && rlsp->logColorScale() && rlsp->cellBlocks(), "67d: log flag not restored");
        CHECK_COND(Plot::isLog10ScaleEngine(rl->activeGraph()->plotWidget()->axisScaleEngine(QwtPlot::yRight)),
                   "67d: log colour axis not restored");
        {
            const QwtScaleDiv *d = rl->activeGraph()->plotWidget()->axisScaleDiv(QwtPlot::yRight);
            CHECK_COND(d && suite65Near(d->upperBound(), dl.maxCount, 1e-6 * dl.maxCount),
                       "67d: log colour range not restored");
        }
        MultiLayer *rp = suite65FindGraph(re, poleName);
        CHECK_COND(rp && rp->activeGraph(), "67d: pole figure graph missing");
        Graph *rg = rp->activeGraph();
        Spectrogram *rpsp = suite67Spectrogram(rg);
        CHECK_COND(rpsp && rpsp->maskInvalidCells() && rpsp->testDisplayMode(QwtPlotSpectrogram::ContourMode)
                           && rpsp->levels() == 8,
                   "67d: pole figure spectrogram not restored");
        CHECK_COND(suite67HasPoleFrame(rg), "67d: pole figure frame not restored");
        CHECK_COND(!rg->plotWidget()->axisEnabled(QwtPlot::xBottom), "67d: hidden axes not restored");
        Matrix *rm = re->matrix(poleMatrixName);
        CHECK_COND(rm && rm->numRows() == 201, "67d: projected matrix missing");
        CHECK_COND(std::isnan(rm->cell(0, 0)) && std::isfinite(rm->cell(100, 100)),
                   "67d: NaN (outside / unmeasured) cells must survive saving");
        re->savedProject();
        re->setOleEmbeddedMode(true);
        delete re;
    }

    // ---------------- 67e: Python RPC ----------------
    {
        PythonAutomationServer *srv = mw->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "67e: no automation server");
        int rid = 6700;
        auto rpc = [&](const char *method, const QJsonObject &params) {
            QJsonObject req;
            req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            req[QStringLiteral("id")] = ++rid;
            req[QStringLiteral("method")] = QString::fromLatin1(method);
            req[QStringLiteral("params")] = params;
            return srv->handleRpcRequest(req);
        };
        QJsonObject dp;
        dp["table"] = "Dens67";
        dp["x"] = "X";
        dp["y"] = "Y";
        dp["width_x"] = 0.5;
        dp["width_y"] = 0.5;
        dp["log"] = true;
        const QJsonObject rdn = rpc("plot.density2d", dp).value("result").toObject();
        CHECK_COND(rdn.value("ok").toBool() && rdn.value("n").toInt() == N - 1000
                           && suite65Near(rdn.value("width_x").toDouble(), 0.5, 1e-12)
                           && !rdn.value("matrix").toString().isEmpty(),
                   "67e: plot.density2d");
        QJsonObject rb;
        rb["graph"] = rdn.value("graph").toString();
        CHECK_COND(rpc("graph.rebuild_density2d", rb).value("result").toObject().value("ok").toBool(),
                   "67e: graph.rebuild_density2d");
        QJsonObject pp;
        pp["table"] = "Pole67";
        pp["alpha"] = "alpha";
        pp["beta"] = "beta";
        pp["intensity"] = "I";
        pp["projection"] = "wulff";
        pp["plot"] = false;
        const QJsonObject rpf = rpc("plot.pole_figure", pp).value("result").toObject();
        CHECK_COND(rpf.value("ok").toBool() && suite65Near(rpf.value("max_x").toDouble(), 0.358719468, 1e-8)
                           && suite65Near(rpf.value("max_alpha").toDouble(), 45.0, 1e-9)
                           && rpf.value("n_beta").toInt() == 72,
                   "67e: plot.pole_figure");
        QJsonObject pm;
        pm["matrix"] = "PoleIn67";
        pm["plot"] = false;
        const QJsonObject rpm = rpc("plot.pole_figure", pm).value("result").toObject();
        CHECK_COND(rpm.value("ok").toBool() && suite65Near(rpm.value("max_beta").toDouble(), 30.0, 1e-9),
                   "67e: plot.pole_figure from a matrix");
        QJsonObject bad;
        bad["table"] = "Pole67";
        bad["projection"] = "mercator";
        CHECK_COND(rpc("plot.pole_figure", bad).contains("error"), "67e: unknown projection must be an error");
    }

    // ---------------- 67f: menus and translations ----------------
    {
        const char *names[] = { "actionE11Density2D", "actionE11PoleFigure" };
        for (const char *nm : names) {
            QAction *a = mw->findChild<QAction *>(QString::fromLatin1(nm));
            CHECK_COND(a != nullptr, "67f: E11b action missing");
            CHECK_COND(!a->associatedWidgets().isEmpty(), "67f: E11b action is in no menu");
            CHECK_COND(!a->text().isEmpty(), "67f: E11b action has no text");
        }
        ExtraPlotTypes::registerTranslations();
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("&Pole Figure (XRD)..."))
                           .contains(QString::fromUtf8("極点図")),
                   "67f: Japanese for pole figure");
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("2D &Density (2D Histogram)..."))
                           .contains(QString::fromUtf8("密度")),
                   "67f: Japanese for 2D density");
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("Equal-area (Schmidt)"))
                           .contains(QString::fromUtf8("等面積")),
                   "67f: Japanese for the projection");
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 67 E11b 2D density and pole figure passed!\n";
    return 0;
}

// =============================================================================
// Suite 68: ribbon folding on narrow screens, minimized ribbon, Home "Copy for PowerPoint",
//           recent fit models, keyboard access to the mini toolbar
//           (C14, C4, F11 and H5 of the 2026-11-21 review)
// =============================================================================
static bool suite68TabOverflows(ModernRibbonBar *rib, int tabIndex)
{
    QStackedWidget *stack = rib->findChild<QStackedWidget *>(QStringLiteral("ModernRibbonTabStack"));
    QScrollArea *sa = stack ? qobject_cast<QScrollArea *>(stack->widget(tabIndex)) : nullptr;
    if (!sa || !sa->horizontalScrollBar())
        return false;
    return sa->horizontalScrollBar()->maximum() > sa->horizontalScrollBar()->minimum();
}

static void suite68Settle()
{
    for (int pass = 0; pass < 4; ++pass) {
        QCoreApplication::sendPostedEvents();
        QApplication::processEvents();
    }
}

int runSuite68RibbonFoldingRecentModelsAndMiniToolbarKeys(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 68: Ribbon Group Folding, Minimized Ribbon, Home Copy Button, Recent Fit Models & Mini Toolbar Keyboard Access...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 68");
    qApp->setProperty("torigraph_noninteractive", true);

    ApplicationWindow *w68 = new ApplicationWindow();
    w68->applyUserSettings();
    w68->resize(1380, 860);
    w68->show();
    w68->switchToLanguage("ja");
    suite68Settle();
    ModernRibbonBar *rib = w68->modernRibbonBar();
    CHECK_COND(rib && rib->isVisible(), "68: ribbon missing");
    QStackedWidget *stack = rib->findChild<QStackedWidget *>(QStringLiteral("ModernRibbonTabStack"));
    CHECK_COND(stack != nullptr, "68: ribbon tab stack missing");
    CHECK_COND(rib->tabCount() == 7, "68: tab count changed");

    auto resizeTo = [&](int w) {
        w68->setMinimumWidth(0);
        w68->resize(w, 760);
        suite68Settle();
        rib->updateGroupCollapse();
        suite68Settle();
    };
    auto report = [&](int w) {
        std::cout << "[INFO] Suite 68: window " << w << " px -> ribbon " << rib->width() << " px\n";
        for (int ti = 0; ti < rib->tabCount(); ++ti) {
            rib->setCurrentTabIndex(ti);
            suite68Settle();
            std::cout << "[INFO]   tab " << ti << " '" << rib->tabTitle(ti).toUtf8().constData()
                      << "' folded: '"
                      << rib->collapsedGroupTitles(ti).join(QStringLiteral(", ")).toUtf8().constData()
                      << "' overflow=" << (suite68TabOverflows(rib, ti) ? 1 : 0) << "\n";
        }
    };

    // ---- 68a (C14): wide window - nothing folded
    resizeTo(1920);
    report(1920);
    for (int ti = 0; ti < rib->tabCount(); ++ti) {
        CHECK_COND(!rib->groupTitles(ti).isEmpty(), "68a: tab without foldable groups: " << ti);
        CHECK_COND(rib->collapsedGroupTitles(ti).isEmpty(), "68a: group folded at 1920 px, tab " << ti);
        CHECK_COND(!suite68TabOverflows(rib, ti), "68a: tab overflows at 1920 px: " << ti);
    }

    // ---- 68b (C14): 1280 px - no tab scrolls sideways; Home keeps its frequent groups
    resizeTo(1280);
    report(1280);
    for (int ti = 0; ti < rib->tabCount(); ++ti) {
        rib->setCurrentTabIndex(ti);
        suite68Settle();
        CHECK_COND(!suite68TabOverflows(rib, ti), "68b: tab overflows at 1280 px: " << ti);
    }
    {
        const QStringList folded = rib->collapsedGroupTitles(0);
        const QStringList all = rib->groupTitles(0);
        CHECK_COND(all.size() >= 6, "68b: Home should have 6 groups (file, undo, clipboard, create, recalc, panels)");
        // File and Clipboard (Copy for PowerPoint) are used most and stay open at 1280 px;
        // Recalculation folds first. Undo/Redo (two icons) and Panels (already one
        // drop-down) never fold: a "<group> ▾" button would not be narrower.
        CHECK_COND(!folded.contains(all.at(0)) && !folded.contains(all.at(2)),
                   "68b: File / Clipboard folded at 1280 px");
        CHECK_COND(folded.contains(all.at(4)), "68b: Recalculation not folded at 1280 px");
        CHECK_COND(!folded.contains(all.at(1)) && !folded.contains(all.at(5)),
                   "68b: a group folded although its drop-down saves no space");
    }

    // ---- 68c (C14): 1366x768 at 150 % = 911 px logical - folding instead of scrolling
    resizeTo(911);
    report(911);
    CHECK_COND(rib->width() <= 960, "68c: window could not be made 911 px wide (ribbon " << rib->width() << ")");
    int foldedTabs = 0;
    for (int ti = 0; ti < rib->tabCount(); ++ti) {
        rib->setCurrentTabIndex(ti);
        suite68Settle();
        if (!rib->collapsedGroupTitles(ti).isEmpty())
            ++foldedTabs;
        CHECK_COND(!suite68TabOverflows(rib, ti), "68c: tab overflows at 911 px: " << ti);
    }
    CHECK_COND(foldedTabs >= 3, "68c: hardly any group folded at 911 px");
    {
        // the less used groups fold first: Recalculation before File on Home
        const QStringList all = rib->groupTitles(0);
        const QStringList folded = rib->collapsedGroupTitles(0);
        CHECK_COND(!folded.isEmpty(), "68c: nothing folded on Home at 911 px");
        CHECK_COND(folded.contains(all.at(4)), "68c: Recalculation not folded first on Home");
        CHECK_COND(!folded.contains(all.at(0))
                           || (folded.contains(all.at(2)) && folded.contains(all.at(3))
                               && folded.contains(all.at(4))),
                   "68c: File folded while less used groups are still open");

        // the folded button is "<group> ▾" and lists the group's own commands
        rib->setCurrentTabIndex(0);
        suite68Settle();
        QToolButton *fb = rib->collapsedGroupButton(0, all.at(4));
        CHECK_COND(fb && fb->isVisible() && fb->menu(), "68c: folded Recalculation button not shown");
        CHECK_COND(fb->text().startsWith(all.at(4)) && fb->text().contains(QString::fromUtf8("\xE2\x96\xBE")),
                   "68c: folded button text is not '<group> ▾'");
        CHECK_COND(fb->focusPolicy() != Qt::NoFocus, "68c: folded button not reachable with Tab");
        emit fb->menu()->aboutToShow();
        const QList<QAction *> acts = fb->menu()->actions();
        CHECK_COND(acts.size() == 2, "68c: folded Recalculation menu should list Auto and Recalc, got " << acts.size());
        CHECK_COND(acts.at(0)->isCheckable() && !acts.at(0)->text().isEmpty(),
                   "68c: the Auto toggle lost its check state in the folded menu");
        // a drop-down inside a folded group becomes a sub-menu (Create: New Table, New Graph,
        // More New ▾)
        CHECK_COND(folded.contains(all.at(3)), "68c: Create not folded at 911 px");
        {
            QToolButton *cb = rib->collapsedGroupButton(0, all.at(3));
            emit cb->menu()->aboutToShow();
            const QList<QAction *> ca = cb->menu()->actions();
            CHECK_COND(ca.size() == 3 && !ca.at(0)->menu() && ca.at(2)->menu(),
                       "68c: folded Create group should list 2 commands and 1 sub-menu");
            CHECK_COND(ca.at(2)->menu()->actions().size() >= 3,
                       "68c: folded 'More New' sub-menu lost its entries");
            CHECK_COND(!ca.at(2)->text().contains(QString::fromUtf8("\xE2\x96\xBE")),
                       "68c: sub-menu title still carries the drop-down mark");
            // opening it twice must not duplicate entries
            emit cb->menu()->aboutToShow();
            CHECK_COND(cb->menu()->actions().size() == 3, "68c: folded menu entries duplicated");
        }
        // the folded commands still work: Recalc through the menu calls the button
        int clicks = 0;
        QList<QToolButton *> recalcButtons;
        QAction *recalcAct = acts.at(1);
        for (QToolButton *b : rib->findChildren<QToolButton *>()) {
            if (b->text() == recalcAct->text() && b != fb)
                recalcButtons << b;
        }
        CHECK_COND(recalcButtons.size() == 1, "68c: Recalc button not found for the folded action");
        QObject::connect(recalcButtons.first(), &QToolButton::clicked, [&clicks]() { ++clicks; });
        recalcAct->trigger();
        CHECK_COND(clicks == 1, "68c: folded menu entry did not run the command");
    }

    // ---- 68d (C14): widening again unfolds every group
    resizeTo(1920);
    for (int ti = 0; ti < rib->tabCount(); ++ti)
        CHECK_COND(rib->collapsedGroupTitles(ti).isEmpty(), "68d: group stays folded after widening, tab " << ti);
    rib->setCurrentTabIndex(0);
    suite68Settle();
    for (const QString &title : rib->groupTitles(0)) {
        QToolButton *fb = rib->collapsedGroupButton(0, title);
        CHECK_COND(fb && fb->isHidden(), "68d: folded button still visible after widening");
    }

    // ---- 68e (C4): "Copy for PowerPoint" on Home, bound to the application's action, visible at 1280 px
    resizeTo(1280);
    rib->setCurrentTabIndex(0);
    suite68Settle();
    {
        QToolButton *copyBtn = rib->actionBoundButton(QStringLiteral("actionCopyGraphOle"));
        QAction *copyAct = w68->findChild<QAction *>(QStringLiteral("actionCopyGraphOle"));
        CHECK_COND(copyBtn && copyAct, "68e: Home copy button or action missing");
        CHECK_COND(copyBtn->objectName() == QLatin1String("RibbonHomeCopyForPowerPointBtn"),
                   "68e: copy button is not the Home one");
        CHECK_COND(copyBtn->isVisible(), "68e: Home copy button hidden (folded) at 1280 px");
        CHECK_COND(copyBtn->text() == ToriGraphOleUi::copyForPowerPointLabel(),
                   "68e: Home copy button label differs from the menu wording");
        QScrollArea *homeSa = qobject_cast<QScrollArea *>(stack->widget(0));
        CHECK_COND(homeSa && homeSa->viewport()->isAncestorOf(copyBtn), "68e: copy button is not on Home");
        const int right = copyBtn->mapTo(homeSa->viewport(), QPoint(copyBtn->width(), 0)).x();
        CHECK_COND(right <= homeSa->viewport()->width(), "68e: copy button clipped at 1280 px");
        copyAct->setEnabled(false);
        CHECK_COND(!copyBtn->isEnabled(), "68e: copy button does not follow its action");
        copyAct->setEnabled(true);
        CHECK_COND(copyBtn->isEnabled(), "68e: copy button not re-enabled");
    }

    // ---- 68f (C14): minimized ribbon (double-click a tab), single click brings it back
    {
        QTabBar *tb = rib->tabBar();
        const int fullH = rib->height();
        rib->setRibbonMinimized(true);
        suite68Settle();
        CHECK_COND(rib->isRibbonMinimized() && stack->isHidden() && tb->isVisible(),
                   "68f: minimized ribbon should show only the tab row");
        CHECK_COND(rib->height() < fullH - 20, "68f: minimized ribbon did not get shorter ("
                           << rib->height() << " vs " << fullH << ")");
        CHECK_COND(ApplicationWindow::getSettings().value("/UX/RibbonMinimized").toBool(),
                   "68f: minimized state not stored");
        rib->setRibbonMinimized(false);
        suite68Settle();
        CHECK_COND(!rib->isRibbonMinimized() && stack->isVisible(), "68f: ribbon not restored");
        CHECK_COND(qAbs(rib->height() - fullH) <= 2, "68f: restored ribbon height differs");

        const QPoint tabPos = tb->tabRect(1).center();
        auto sendMouse = [&](QEvent::Type type) {
            QMouseEvent ev(type, tabPos, tb->mapToGlobal(tabPos), Qt::LeftButton,
                           type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                           Qt::NoModifier);
            QApplication::sendEvent(tb, &ev);
        };
        sendMouse(QEvent::MouseButtonDblClick);
        suite68Settle();
        CHECK_COND(rib->isRibbonMinimized(), "68f: double-click on a tab did not minimize");
        // Office habit: double-click again to restore (first click restores, the second
        // half of the double-click must not minimize again)
        sendMouse(QEvent::MouseButtonPress);
        sendMouse(QEvent::MouseButtonRelease);
        sendMouse(QEvent::MouseButtonDblClick);
        sendMouse(QEvent::MouseButtonRelease);
        suite68Settle();
        CHECK_COND(!rib->isRibbonMinimized() && stack->isVisible(),
                   "68f: double-click on the minimized ribbon did not restore it");
        CHECK_COND(rib->currentTabIndex() == 1, "68f: clicked tab not selected");
        CHECK_COND(!ApplicationWindow::getSettings().value("/UX/RibbonMinimized").toBool(),
                   "68f: restored state not stored");
        CHECK_COND(tb->toolTip().contains(QStringLiteral("Ctrl+F1")), "68f: tab row has no minimize hint");
    }

    // ---- 68g (C14): Japanese for the folded group names
    resizeTo(911);
    for (int ti = 0; ti < rib->tabCount(); ++ti) {
        for (const QString &title : rib->groupTitles(ti)) {
            bool ja = false;
            for (const QChar &ch : title)
                ja = ja || ch.unicode() >= 0x3000;
            CHECK_COND(ja, "68g: group name not translated: " << title.toUtf8().constData());
            QToolButton *fb = rib->collapsedGroupButton(ti, title);
            CHECK_COND(fb && !fb->toolTip().isEmpty(), "68g: folded button without tooltip");
        }
    }
    resizeTo(1380);

    // ---- 68h (F11): recently used fit models (max 8, newest first, stored in QSettings)
    {
        const QStringList saved = FitDialog::recentModels();
        FitDialog::clearRecentModels();
        const QStringList names = NonLinearFit::builtInModelNames();
        CHECK_COND(names.size() >= 10, "68h: fewer than 10 built-in models");
        for (int i = 0; i < 10; ++i)
            FitDialog::recordRecentModel(names.at(i));
        QStringList rec = FitDialog::recentModels();
        CHECK_COND(rec.size() == FitDialog::kMaxRecentModels && rec.size() == 8,
                   "68h: recent list not capped at 8 (" << rec.size() << ")");
        CHECK_COND(rec.first() == names.at(9) && rec.last() == names.at(2),
                   "68h: recent list not newest first");
        FitDialog::recordRecentModel(names.at(5));
        rec = FitDialog::recentModels();
        CHECK_COND(rec.size() == 8 && rec.first() == names.at(5) && rec.count(names.at(5)) == 1,
                   "68h: re-used model not moved to the top");
        CHECK_COND(ApplicationWindow::getSettings().value("/Fit/RecentModels").toStringList() == rec,
                   "68h: recent models not stored in the settings");

        FitDialog *fd = new FitDialog(w68);
        QComboBox *box = fd->recentModelsBox();
        QLineEdit *search = fd->findChild<QLineEdit *>(QStringLiteral("fitModelSearchBox"));
        CHECK_COND(box && box->isEnabled() && box->count() == 9, "68h: recent box should list 8 models");
        CHECK_COND(box->itemText(1) == names.at(5), "68h: newest model is not first in the box");
        CHECK_COND(search && box->parentWidget() == search->parentWidget(),
                   "68h: recent box is not next to the search box");
        // picking a recent model selects it in its category
        const QString pick = names.at(9);
        const int idx = box->findText(pick);
        CHECK_COND(idx > 0, "68h: model missing from the box");
        box->setCurrentIndex(idx);
        emit box->activated(idx);
        bool selected = false;
        for (QListWidget *lw : fd->findChildren<QListWidget *>())
            selected = selected || (lw->currentItem() && lw->currentItem()->text() == pick);
        CHECK_COND(selected, "68h: picking a recent model did not select it");
        CHECK_COND(box->currentIndex() == 0, "68h: recent box not reset after picking");
        CHECK_COND(!fd->selectRecentModel(QStringLiteral("NoSuchModel68")),
                   "68h: unknown recent model accepted");
        delete fd;

        FitDialog::clearRecentModels();
        FitDialog *fd2 = new FitDialog(w68);
        CHECK_COND(fd2->recentModelsBox()->count() == 1 && !fd2->recentModelsBox()->isEnabled(),
                   "68h: empty recent list should show one disabled placeholder");
        delete fd2;
        ApplicationWindow::getSettings().setValue("/Fit/RecentModels", saved);
    }

    // ---- 68i (H5): Alt+F10 moves the keyboard into the mini toolbar, Esc goes back
    {
        CHECK_COND(GraphMiniToolbar::keyboardFocusShortcut() == QKeySequence(Qt::ALT | Qt::Key_F10),
                   "68i: unexpected mini toolbar shortcut");
        // the key must not be taken by an application action
        for (QAction *a : w68->findChildren<QAction *>()) {
            for (const QKeySequence &ks : a->shortcuts())
                CHECK_COND(ks != GraphMiniToolbar::keyboardFocusShortcut(),
                           "68i: Alt+F10 already used by " << a->objectName().toUtf8().constData());
        }
        Table *t = w68->newTable(5, 2, "S68Mini", "Suite 68 mini toolbar");
        for (int r = 0; r < 5; ++r) {
            t->setCell(r, 0, r + 1.0);
            t->setCell(r, 1, (r + 1.0) * 2.0);
        }
        MultiLayer *ml = w68->multilayerPlot(t, QStringList() << (t->name() + "_2"), Graph::LineSymbols);
        CHECK_COND(ml && ml->activeGraph(), "68i: graph not created");
        Graph *g = ml->activeGraph();
        suite68Settle();
        const bool prevEnabled = GraphMiniToolbar::isMiniToolbarEnabled();
        const QVariant prevNi = qApp->property("torigraph_noninteractive");
        qApp->setProperty("torigraph_noninteractive", QVariant());
        GraphMiniToolbar::setMiniToolbarEnabled(true);
        GraphMiniToolbar *bar = GraphMiniToolbar::instance();
        CHECK_COND(bar != nullptr, "68i: no mini toolbar");
        CHECK_COND(bar->windowFlags() & Qt::WindowDoesNotAcceptFocus,
                   "68i: mini toolbar must not take focus by itself");
        bar->popupForCanvas(g, g->mapToGlobal(g->rect().center()));
        QElapsedTimer waitT;
        waitT.start();
        while (!bar->isVisible() && waitT.elapsed() < 2000) {
            QApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        if (!bar->isVisible()) {
            std::cout << "[INFO] Suite 68i: mini toolbar did not pop up in this session; keyboard check skipped\n";
        } else {
            QWidget *canvas = g->plotWidget() ? static_cast<QWidget *>(g->plotWidget()) : g;
            canvas->setFocus();
            // pressing Alt alone (start of the chord) keeps the bar open
            QKeyEvent alt(QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
            QApplication::sendEvent(canvas, &alt);
            CHECK_COND(bar->isVisible(), "68i: pressing Alt dismissed the mini toolbar");
            QKeyEvent f10(QEvent::KeyPress, Qt::Key_F10, Qt::AltModifier);
            QApplication::sendEvent(canvas, &f10);
            suite68Settle();
            CHECK_COND(bar->isVisible() && bar->isKeyboardFocusMode(),
                       "68i: Alt+F10 did not move the keyboard into the mini toolbar");
            CHECK_COND(!(bar->windowFlags() & Qt::WindowDoesNotAcceptFocus),
                       "68i: mini toolbar still refuses the focus in keyboard mode");
            QToolButton *fbtn = qobject_cast<QToolButton *>(bar->focusWidget());
            CHECK_COND(fbtn && bar->isAncestorOf(fbtn), "68i: no mini toolbar button got the focus");
            QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(fbtn, &esc);
            suite68Settle();
            CHECK_COND(!bar->isVisible() && !bar->isKeyboardFocusMode(),
                       "68i: Esc did not leave the mini toolbar");
            CHECK_COND(bar->windowFlags() & Qt::WindowDoesNotAcceptFocus,
                       "68i: mini toolbar takes focus again after Esc");
            {
                QWidget *back = canvas->window()->focusWidget();
                CHECK_COND(back == canvas || back == g || (back && g->isAncestorOf(back)),
                           "68i: focus did not go back to the graph canvas (now "
                                   << (back ? back->metaObject()->className() : "none") << " '"
                                   << (back ? back->objectName().toUtf8().constData() : "") << "')");
            }
            // an ordinary popup afterwards still does not steal the focus
            bar->popupForCanvas(g, g->mapToGlobal(g->rect().center()));
            waitT.restart();
            while (!bar->isVisible() && waitT.elapsed() < 2000)
                QApplication::processEvents(QEventLoop::AllEvents, 20);
            CHECK_COND(!bar->isVisible() || (bar->windowFlags() & Qt::WindowDoesNotAcceptFocus),
                       "68i: popup after keyboard mode accepts focus");
            QKeyEvent other(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
            QApplication::sendEvent(canvas, &other);
            CHECK_COND(!bar->isVisible(), "68i: typing on the canvas no longer dismisses the bar");
        }
        bar->cancelScheduledAndHide();
        GraphMiniToolbar::setMiniToolbarEnabled(prevEnabled);
        qApp->setProperty("torigraph_noninteractive", prevNi);
    }

    w68->savedProject();
    w68->setOleEmbeddedMode(true);
    delete w68;
    QApplication::processEvents();
    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 68 ribbon folding, minimized ribbon, recent fit models and mini toolbar keys passed!\n";
    return 0;
}

// =============================================================================
// Suite 66: D8 workbooks (one book, several sheets as tabs)
//   66a new workbook: sheet tables "<book>_SheetN", only the current sheet is shown,
//       every sheet carries a tab bar, the explorer marks the sheets
//   66b switching sheets (API and a tab click): the new sheet takes the old one's place
//   66c add / rename / move (API and tab drag) / take out / put back / delete
//   66d graph columns and column formulas keep working while sheets switch, after renaming
//       the book and after save / reopen
//   66e save / reopen: sheet order, names, the visible sheet and the tab labels come back
//   66f old project format (no <workbook> line): separate tables, nothing lost; a project
//       saved with workbooks parses in the old way (forward compatible)
//   66g Excel import: one workbook with a tab per sheet (default) or separate worksheets
//   66h Python RPC workbook.* and app.import_xlsx(workbook=...)
//   66i menus and Japanese strings
// =============================================================================
#include "Workbook.h"

namespace {
QTabBar *suite66Tabs(Table *t)
{
    return t ? t->findChild<QTabBar *>(QStringLiteral("torigraph_workbook_tabs")) : nullptr;
}

QStringList suite66Names(const QList<Table *> &list)
{
    QStringList out;
    for (Table *t : list)
        out << (t ? t->name() : QStringLiteral("<null>"));
    return out;
}

Table *suite66Find(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList())
        if (Table *t = qobject_cast<Table *>(w))
            if (t->name() == name)
                return t;
    return nullptr;
}

//! Watches a reopened project window. The full run once crashed in Suite 66 inside
//! ApplicationWindow::window() -> windowsList() -> folders.topLevelItem(0) on a window whose
//! folder tree was already gone: the reopened window had been closed (WA_DeleteOnClose) and
//! deleted while the suite pumped events, and the suite then used the dangling pointer. The spy
//! records who closed it, so that such a run fails with an explanation instead of crashing.
class Suite66CloseSpy : public QObject
{
public:
    explicit Suite66CloseSpy(ApplicationWindow *w) : guard(w)
    {
        if (w)
            w->installEventFilter(this);
    }
    bool alive() const { return !guard.isNull(); }
    std::string describe(const char *step) const
    {
        std::string s = std::string(step) + ": the reopened project window was deleted while events were processed";
        if (closeEvents > 0)
            s += " (close event " + std::string(lastSpontaneous ? "from the system / window manager"
                                                               : "sent by this process")
                 + ", active window then: " + activeWindowAtClose.toStdString() + ")";
        else
            s += " (no close event seen)";
        return s;
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (o == guard.data() && e->type() == QEvent::Close) {
            ++closeEvents;
            lastSpontaneous = e->spontaneous();
            QWidget *aw = QApplication::activeWindow();
            activeWindowAtClose = aw ? (QString::fromLatin1(aw->metaObject()->className())
                                        + QStringLiteral(" \"") + aw->windowTitle() + QLatin1Char('"'))
                                     : QStringLiteral("none");
            std::cerr << "[WARN] Suite 66: close event on the reopened project window ("
                      << (lastSpontaneous ? "spontaneous" : "synthetic") << ", active window "
                      << activeWindowAtClose.toStdString() << ")" << std::endl;
        }
        return false;
    }

private:
    QPointer<ApplicationWindow> guard;
    int closeEvents = 0;
    bool lastSpontaneous = false;
    QString activeWindowAtClose;
};

bool suite66Shown(ApplicationWindow *app, Table *t)
{
    return t && !t->isHidden() && !app->hidden(t);
}

QString suite66ExplorerType(ApplicationWindow *app, Table *t)
{
    for (int i = 0; i < app->lv.topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = app->lv.topLevelItem(i);
        if (it && it->type() == WindowListItem::WindowType
            && static_cast<WindowListItem *>(it)->window() == t)
            return it->text(1);
    }
    return QString();
}
} // namespace

int runSuite66Workbooks(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 66: D8 workbooks (several sheets as tabs of one book)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 66");
    qApp->setProperty("torigraph_noninteractive", true);
    QTemporaryDir tmp;
    CHECK_COND(tmp.isValid(), "66: temp dir");
    WorkbookManager *wm = WorkbookManager::instance(mw);
    CHECK_COND(wm && WorkbookManager::existing(mw) == wm, "66: manager");
    const bool tabbedView = mw->isTabbedWorkspaceView();

    // ---------------- 66a: new workbook ----------------
    QString err;
    const QString book = wm->createBook(QStringLiteral("Spec66"), 3, &err);
    QApplication::processEvents();
    CHECK_COND(book == "Spec66", QString("66a: book name '%1' (%2)").arg(book, err).toStdString());
    QList<Table *> sh = wm->sheets(book);
    CHECK_COND(suite66Names(sh)
                       == (QStringList() << "Spec66_Sheet1" << "Spec66_Sheet2" << "Spec66_Sheet3"),
               QString("66a: sheet names %1").arg(suite66Names(sh).join(",")).toStdString());
    Table *s1 = sh.value(0), *s2 = sh.value(1), *s3 = sh.value(2);
    CHECK_COND(wm->currentSheet(book) == s1, "66a: first sheet is current");
    CHECK_COND(suite66Shown(mw, s1) && !suite66Shown(mw, s2) && !suite66Shown(mw, s3),
               "66a: only the current sheet is shown");
    CHECK_COND(wm->bookOf(s2) == book && wm->bookOf(nullptr).isEmpty(), "66a: bookOf");
    {
        QTabBar *tb1 = suite66Tabs(s1);
        CHECK_COND(tb1 && tb1->count() == 3 && tb1->currentIndex() == 0, "66a: sheet tab bar");
        for (int i = 0; i < 3; ++i) {
            CHECK_COND(tb1->tabText(i) == QStringLiteral("Sheet%1").arg(i + 1),
                       "66a: tab label leaves the book prefix out");
        }
    }
    CHECK_COND(suite66ExplorerType(mw, s2).contains("Spec66"), "66a: explorer marks the sheet");
    CHECK_COND(WorkbookManager::sheetLabel("Spec66", "Spec66-Raw") == "Raw"
                       && WorkbookManager::sheetLabel("Spec66", "Other") == "Other"
                       && WorkbookManager::sheetLabel("Spec66", "Spec66") == "Spec66",
               "66a: sheetLabel");

    // ---------------- 66b: switching ----------------
    if (!tabbedView && !s1->isMaximized())
        s1->setGeometry(QRect(20, 15, 520, 380));
    const QRect g1 = s1->geometry();
    CHECK_COND(wm->setCurrentSheet(s2, true, &err), "66b: setCurrentSheet");
    QApplication::processEvents();
    CHECK_COND(wm->currentSheet(book) == s2 && suite66Shown(mw, s2) && !suite66Shown(mw, s1),
               "66b: sheet 2 replaced sheet 1");
    if (!tabbedView && !s2->isMaximized())
        CHECK_COND(s2->geometry() == g1, "66b: the new sheet takes the old sheet's place");
    CHECK_COND(suite66Tabs(s2)->currentIndex() == 1, "66b: tab follows");
    // a click on the third tab of the visible sheet
    suite66Tabs(s2)->setCurrentIndex(2);
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
    CHECK_COND(wm->currentSheet(book) == s3 && suite66Shown(mw, s3) && !suite66Shown(mw, s2),
               "66b: tab click switched to sheet 3");
    // showing a sheet from outside (explorer / Window menu) makes it the visible sheet
    mw->activateSubWindow(s1);
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
    CHECK_COND(wm->currentSheet(book) == s1 && suite66Shown(mw, s1) && !suite66Shown(mw, s3),
               "66b: activating a hidden sheet switches the book to it");

    // ---------------- 66c: sheet operations ----------------
    Table *ex = wm->addSheet(book, QStringLiteral("Extra"), -1, true, &err);
    QApplication::processEvents();
    CHECK_COND(ex && ex->name() == "Spec66_Extra" && wm->currentSheet(book) == ex
                       && suite66Shown(mw, ex) && !suite66Shown(mw, s1),
               "66c: added sheet is shown");
    CHECK_COND(wm->sheets(book).size() == 4 && suite66Tabs(ex) && suite66Tabs(ex)->count() == 4,
               "66c: active tab bar has the new sheet");
    CHECK_COND(wm->renameSheet(s3, QStringLiteral("Raw"), &err), "66c: rename sheet");
    CHECK_COND(s3->name() == "Spec66-Raw" && suite66Tabs(ex)->tabText(2) == "Raw",
               QString("66c: renamed table '%1'").arg(s3->name()).toStdString());
    CHECK_COND(wm->moveSheet(book, 3, 0, &err), "66c: moveSheet");
    CHECK_COND(wm->sheets(book) == (QList<Table *>() << ex << s1 << s2 << s3), "66c: order");
    // dragging a tab of the visible sheet: Extra from the front to the end
    suite66Tabs(ex)->moveTab(0, 3);
    QApplication::processEvents();
    CHECK_COND(wm->sheets(book) == (QList<Table *>() << s1 << s2 << s3 << ex),
               QString("66c: tab drag order %1").arg(suite66Names(wm->sheets(book)).join(","))
                       .toStdString());
    CHECK_COND(wm->setCurrentSheet(s1, true, &err) && suite66Tabs(s1)
                       && suite66Tabs(s1)->tabText(3) == "Extra",
               "66c: other bars follow the drag when shown");
    CHECK_COND(wm->setCurrentSheet(ex, true, &err) && suite66Shown(mw, ex),
               "66c: restore Extra as shown sheet");
    // tabMoved must also directly refresh current sheet's bar without calling setCurrentSheet in between
    CHECK_COND(wm->setCurrentSheet(s1, false, &err) && wm->currentSheet(book) == s1
                       && suite66Shown(mw, ex) && !suite66Shown(mw, s1),
               "66c: mark s1 current while ex stays shown");
    suite66Tabs(ex)->moveTab(3, 0);
    QApplication::processEvents();
    CHECK_COND(suite66Tabs(s1) && suite66Tabs(s1)->tabText(0) == "Extra"
                       && suite66Tabs(s1)->tabText(1) == "Sheet1",
               "66c: tabMoved did not directly refresh current sheet bar without setCurrentSheet!");
    suite66Tabs(ex)->moveTab(0, 3);
    QApplication::processEvents();
    CHECK_COND(suite66Tabs(s1) && suite66Tabs(s1)->tabText(3) == "Extra",
               "66c: tabMoved reverse drag did not directly refresh current sheet bar!");
    CHECK_COND(wm->setCurrentSheet(ex, true, &err) && suite66Shown(mw, ex),
               "66c: restore Extra as current shown sheet after direct tabMoved check");
    // take sheet 2 out, then put it back at position 1
    CHECK_COND(wm->detachSheet(s2, true, &err), "66c: detach");
    QApplication::processEvents();
    CHECK_COND(wm->bookOf(s2).isEmpty() && suite66Shown(mw, s2) && !suite66Tabs(s2),
               "66c: the detached sheet is a stand-alone shown table without tabs");
    CHECK_COND(suite66Shown(mw, ex), "66c: the book still shows its sheet");
    CHECK_COND(wm->attachTable(book, s2, 1, &err), "66c: attach");
    QApplication::processEvents();
    CHECK_COND(wm->sheets(book) == (QList<Table *>() << s1 << s2 << s3 << ex)
                       && wm->currentSheet(book) == s2 && suite66Shown(mw, s2)
                       && !suite66Shown(mw, ex),
               "66c: put back at position 1 and shown in the book's place");
    // delete the visible sheet: the next one takes its place
    CHECK_COND(wm->setCurrentSheet(ex, true, &err) && suite66Shown(mw, ex), "66c: show Extra");
    CHECK_COND(wm->deleteSheet(ex, false, &err), "66c: delete call");
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
    sh = wm->sheets(book);
    CHECK_COND(sh == (QList<Table *>() << s1 << s2 << s3),
               QString("66c: after delete %1").arg(suite66Names(sh).join(",")).toStdString());
    Table *afterDel = wm->currentSheet(book);
    CHECK_COND(afterDel && suite66Shown(mw, afterDel), "66c: a sheet is shown after deleting");
    CHECK_COND(!suite66Find(mw, "Spec66_Extra"), "66c: deleted table is gone");

    // ---------------- 66d: graph columns and formulas ----------------
    for (int r = 0; r < 5; ++r) {
        s1->setCell(r, 0, r + 1.0);
        s1->setCell(r, 1, 2.0 * (r + 1.0));
    }
    MultiLayer *ml = mw->multilayerPlot(s1, QStringList() << "Spec66_Sheet1_2", Graph::LineSymbols);
    CHECK_COND(ml && ml->activeGraph() && ml->activeGraph()->curves() == 1, "66d: graph");
    const QString graphName = ml->name();
    Table *f66 = mw->newTable("F66", 5, 2);
    CHECK_COND(f66, "66d: formula table");
    for (int r = 0; r < 5; ++r)
        f66->setCell(r, 0, r);
    f66->column(1)->setFormula(Interval<int>(0, 4), "tablecol(\"Spec66_Sheet1\", \"2\")*10");
    CHECK_COND(f66->recalculate(1, false), "66d: recalculate");
    CHECK_COND(f66->cell(0, 1) == 20.0 && f66->cell(4, 1) == 100.0, "66d: formula values");
    // switching sheets leaves the plotted / referenced table alone
    wm->setCurrentSheet(s3, true);
    QApplication::processEvents();
    CHECK_COND(ml->activeGraph()->curves() == 1 && ml->activeGraph()->curve(0)->dataSize() == 5,
               "66d: curve after switching sheets");
    s1->setCell(0, 1, 7.0);
    CHECK_COND(f66->recalculate(1, false) && f66->cell(0, 1) == 70.0,
               "66d: formula reads a hidden sheet");
    // renaming the book keeps every table name
    CHECK_COND(wm->renameBook(book, QStringLiteral("Specimens"), &err), "66d: rename book");
    const QString bk = QStringLiteral("Specimens");
    CHECK_COND(wm->hasBook(bk) && !wm->hasBook(book) && s1->name() == "Spec66_Sheet1",
               "66d: book renamed, sheets not");
    CHECK_COND(suite66Tabs(s3) && suite66Tabs(s3)->tabText(0) == "Sheet1",
               "66d: labels after renaming the book");
    CHECK_COND(suite66ExplorerType(mw, s3).contains("Specimens"),
               "66d: explorer follows the book name");
    CHECK_COND(f66->recalculate(1, false) && f66->cell(0, 1) == 70.0, "66d: formula after rename");
    CHECK_COND(wm->addSheet(bk, QString(), -1, false, &err)
                       && wm->sheets(bk).last()->name() == "Spec66_Sheet3"
                       && !suite66Shown(mw, wm->sheets(bk).last()) && wm->currentSheet(bk) == s3,
               "66d: new sheets keep the prefix; add without showing");

    // ---------------- 66e: save / reopen ----------------
    const QString prj = tmp.filePath("wb66.sciprj");
    CHECK_COND(mw->saveFolder(mw->projectFolder(), prj), "66e: save");
    QByteArray txt;
    {
        QFile f(prj);
        CHECK_COND(f.open(QIODevice::ReadOnly), "66e: read back");
        txt = f.readAll();
    }
    const QByteArray expectLine = "<workbook>\tname=Specimens\tprefix=Spec66\tcurrent=Spec66-Raw"
                                  "\tsheet=Spec66_Sheet1\tsheet=Spec66_Sheet2\tsheet=Spec66-Raw"
                                  "\tsheet=Spec66_Sheet3\n";
    CHECK_COND(txt.contains(expectLine), "66e: workbook line not written as expected");
    CHECK_COND(txt.count("<table>") >= 5, "66e: every sheet is still a <table>");
    {
        ApplicationWindow *re = mw->openProject(prj);
        CHECK_COND(re != nullptr, "66e: reopen");
        Suite66CloseSpy spy66e(re);
        QApplication::processEvents();
        CHECK_COND(spy66e.alive(), spy66e.describe("66e"));
        WorkbookManager *rw = WorkbookManager::existing(re);
        CHECK_COND(rw && rw->hasBook(bk), "66e: workbook restored");
        const QList<Table *> rs = rw->sheets(bk);
        CHECK_COND(suite66Names(rs)
                           == (QStringList() << "Spec66_Sheet1" << "Spec66_Sheet2" << "Spec66-Raw"
                                             << "Spec66_Sheet3"),
                   QString("66e: order %1").arg(suite66Names(rs).join(",")).toStdString());
        CHECK_COND(rw->currentSheet(bk) && rw->currentSheet(bk)->name() == "Spec66-Raw",
                   "66e: current sheet");
        int shown = 0;
        for (Table *t : rs)
            if (suite66Shown(re, t))
                ++shown;
        CHECK_COND(shown == 1 && suite66Shown(re, rw->currentSheet(bk)), "66e: one sheet shown");
        QTabBar *rt = suite66Tabs(rw->currentSheet(bk));
        CHECK_COND(rt && rt->count() == 4 && rt->currentIndex() == 2 && rt->tabText(2) == "Raw",
                   "66e: tab bar restored");
        MultiLayer *rml = qobject_cast<MultiLayer *>(re->window(graphName));
        CHECK_COND(rml && rml->activeGraph() && rml->activeGraph()->curves() == 1
                           && rml->activeGraph()->curve(0)->dataSize() == 5,
                   "66e: graph column reference after reopening");
        Table *rf = suite66Find(re, "F66");
        CHECK_COND(rf && rf->recalculate(1, false) && rf->cell(0, 1) == 70.0
                           && rf->cell(4, 1) == 100.0,
                   "66e: formula after reopening");
        re->savedProject();
        re->setOleEmbeddedMode(true);
        delete re;
        QApplication::processEvents();
    }

    // ---------------- 66f: old format ----------------
    {
        QByteArray old = txt;
        int pos;
        while ((pos = old.indexOf("<workbook>")) >= 0)
            old.remove(pos, old.indexOf('\n', pos) - pos + 1);
        CHECK_COND(!old.contains("<workbook>"), "66f: strip");
        const QString oldPrj = tmp.filePath("old66.sciprj");
        CHECK_COND(suite59WriteFile(oldPrj, old), "66f: write old project");
        ApplicationWindow *re = mw->openProject(oldPrj);
        CHECK_COND(re != nullptr, "66f: open old project");
        Suite66CloseSpy spy66f(re);
        QApplication::processEvents();
        CHECK_COND(spy66f.alive(), spy66f.describe("66f"));
        WorkbookManager *rw = WorkbookManager::existing(re);
        CHECK_COND(!rw || rw->bookNames().isEmpty(), "66f: no workbook without the line");
        for (const char *n : { "Spec66_Sheet1", "Spec66_Sheet2", "Spec66-Raw", "Spec66_Sheet3", "F66" }) {
            Table *t = suite66Find(re, QString::fromLatin1(n));
            CHECK_COND(t && !suite66Tabs(t), "66f: separate table without tabs");
        }
        CHECK_COND(suite66Shown(re, suite66Find(re, "Spec66-Raw"))
                           && re->hidden(suite66Find(re, "Spec66_Sheet1")),
                   "66f: the saved visible sheet is shown, the others are hidden tables");
        MultiLayer *rml = qobject_cast<MultiLayer *>(re->window(graphName));
        CHECK_COND(rml && rml->activeGraph() && rml->activeGraph()->curve(0)->dataSize() == 5,
                   "66f: graph in the old format");
        re->savedProject();
        re->setOleEmbeddedMode(true);
        delete re;
        QApplication::processEvents();
    }
    // an unknown key in the line is ignored (later versions may add some)
    {
        QByteArray fut = txt;
        fut.replace("\tprefix=Spec66", "\tprefix=Spec66\tcolor=red");
        const QString futPrj = tmp.filePath("future66.sciprj");
        CHECK_COND(suite59WriteFile(futPrj, fut), "66f: write future project");
        ApplicationWindow *re = mw->openProject(futPrj);
        CHECK_COND(re && WorkbookManager::existing(re)
                           && WorkbookManager::existing(re)->sheets(bk).size() == 4,
                   "66f: unknown keys are ignored");
        re->savedProject();
        re->setOleEmbeddedMode(true);
        delete re;
        QApplication::processEvents();
    }

    // ungroup: every sheet becomes a visible stand-alone table
    CHECK_COND(wm->dissolveBook(bk, &err) && !wm->hasBook(bk), "66f: ungroup");
    QApplication::processEvents();
    CHECK_COND(suite66Shown(mw, s1) && suite66Shown(mw, s2) && !suite66Tabs(s1), "66f: ungrouped");
    // make a workbook from one table, move another table into it
    const QString one = wm->createBookFromTables(QString(), QList<Table *>() << s1, s1, &err);
    CHECK_COND(one == "Spec66_Sheet1" && wm->sheets(one).size() == 1 && suite66Tabs(s1)
                       && suite66Tabs(s1)->count() == 1,
               "66f: workbook from a single worksheet");
    CHECK_COND(wm->attachTable(one, s2, -1, &err) && wm->currentSheet(one) == s2
                       && !suite66Shown(mw, s1),
               "66f: move a worksheet into the workbook");
    QApplication::processEvents();

    // close what this suite made
    {
        QStringList names;
        names << "F66" << "Spec66_Sheet1" << "Spec66_Sheet2" << "Spec66-Raw" << "Spec66_Sheet3";
        if (MultiLayer *g = qobject_cast<MultiLayer *>(mw->window(graphName))) {
            g->askOnCloseEvent(false);
            g->close();
        }
        for (const QString &n : names)
            if (Table *t = suite66Find(mw, n)) {
                t->askOnCloseEvent(false);
                t->close();
            }
        for (int i = 0; i < 5; ++i)
            QApplication::processEvents();
        CHECK_COND(!wm->hasBook(one), "66f: closing all sheets removes the workbook");
    }

    // ---------------- 66g: Excel import ----------------
    const QString xpath = tmp.filePath("Book66.xlsx");
    CHECK_COND(suite59WriteFile(xpath, suite64Workbook()), "66g: write xlsx");
    {
        XlsxImport::ImportResult res;
        CHECK_COND(XlsxImport::importIntoProject(mw, xpath, QString(), &err, &res, true),
                   QString("66g: import %1").arg(err).toStdString());
        QApplication::processEvents();
        CHECK_COND(res.workbook == "Book66" && wm->sheets(res.workbook).size() == 2,
                   QString("66g: workbook '%1'").arg(res.workbook).toStdString());
        const QList<Table *> xs = wm->sheets(res.workbook);
        CHECK_COND(suite66Names(xs) == (QStringList() << "Book66_Tensile" << "Book66_Dates"),
                   "66g: sheets in tab order");
        CHECK_COND(suite66Shown(mw, xs.at(0)) && !suite66Shown(mw, xs.at(1)),
                   "66g: first sheet shown");
        CHECK_COND(suite66Tabs(xs.at(0))->tabText(1) == "Dates", "66g: tab label is the sheet name");
        suite64CloseTables(mw, res.tables);
        QApplication::processEvents();
        CHECK_COND(!wm->hasBook(res.workbook), "66g: closed");

        XlsxImport::ImportResult sep;
        CHECK_COND(XlsxImport::importIntoProject(mw, xpath, QString(), &err, &sep, false),
                   "66g: import as separate worksheets");
        CHECK_COND(sep.workbook.isEmpty() && sep.tables.size() == 2
                           && wm->bookOf(suite66Find(mw, sep.tables.at(0))).isEmpty()
                           && suite66Shown(mw, suite66Find(mw, sep.tables.at(1))),
                   "66g: separate worksheets");
        suite64CloseTables(mw, sep.tables);
        XlsxImport::ImportResult one1;
        CHECK_COND(XlsxImport::importIntoProject(mw, xpath, QStringLiteral("Dates"), &err, &one1, true)
                           && one1.workbook.isEmpty(),
                   "66g: a single sheet needs no workbook");
        suite64CloseTables(mw, one1.tables);
    }

    // ---------------- 66h: Python RPC ----------------
    if (PythonAutomationServer *srv = mw->pythonAutomationServer()) {
        int rid = 6600;
        auto rpc = [&](const char *method, const QJsonObject &params) {
            QJsonObject req;
            req["jsonrpc"] = "2.0";
            req["id"] = ++rid;
            req["method"] = QString::fromLatin1(method);
            req["params"] = params;
            return srv->handleRpcRequest(req);
        };
        QJsonObject cp;
        cp["name"] = "Rpc66";
        cp["sheets"] = 2;
        const QJsonObject cr = rpc("workbook.create", cp).value("result").toObject();
        CHECK_COND(cr.value("ok").toBool() && cr.value("name").toString() == "Rpc66"
                           && cr.value("sheets").toArray().size() == 2
                           && cr.value("current").toString() == "Rpc66_Sheet1",
                   "66h: workbook.create");
        QJsonObject ap;
        ap["book"] = "Rpc66";
        ap["name"] = "S3";
        const QJsonObject ar = rpc("workbook.add_sheet", ap).value("result").toObject();
        CHECK_COND(ar.value("table").toString() == "Rpc66_S3"
                           && ar.value("current").toString() == "Rpc66_S3",
                   "66h: workbook.add_sheet");
        QJsonObject sp;
        sp["table"] = "Rpc66_Sheet2";
        CHECK_COND(rpc("workbook.set_current", sp).value("result").toObject().value("current")
                                   .toString()
                           == "Rpc66_Sheet2",
                   "66h: workbook.set_current");
        QJsonObject mp;
        mp["table"] = "Rpc66_S3";
        mp["index"] = 0;
        const QJsonArray order =
                rpc("workbook.move_sheet", mp).value("result").toObject().value("sheets").toArray();
        CHECK_COND(order.size() == 3 && order.at(0).toObject().value("label").toString() == "S3",
                   "66h: workbook.move_sheet");
        const QJsonArray books =
                rpc("workbook.list", QJsonObject()).value("result").toObject().value("workbooks").toArray();
        CHECK_COND(books.size() == 1 && books.at(0).toObject().value("name").toString() == "Rpc66",
                   "66h: workbook.list");
        QJsonObject bad;
        bad["book"] = "Nope66";
        CHECK_COND(rpc("workbook.add_sheet", bad).contains("error"), "66h: unknown book -> error");
        QJsonObject xp;
        xp["path"] = xpath;
        xp["workbook"] = false;
        const QJsonObject xr = rpc("app.import_xlsx", xp).value("result").toObject();
        CHECK_COND(xr.value("tables").toArray().size() == 2 && xr.value("workbook").toString().isEmpty(),
                   "66h: app.import_xlsx(workbook=false)");
        QStringList toClose;
        for (const QJsonValue &v : xr.value("tables").toArray())
            toClose << v.toString();
        xp["workbook"] = true;
        const QJsonObject xr2 = rpc("app.import_xlsx", xp).value("result").toObject();
        CHECK_COND(!xr2.value("workbook").toString().isEmpty(), "66h: app.import_xlsx(workbook=true)");
        for (const QJsonValue &v : xr2.value("tables").toArray())
            toClose << v.toString();
        toClose << "Rpc66_Sheet1" << "Rpc66_Sheet2" << "Rpc66_S3";
        CHECK_COND(PythonAutomationServer::supportedMethods().contains("workbook.create"),
                   "66h: method listed");
        suite64CloseTables(mw, toClose);
        for (int i = 0; i < 5; ++i)
            QApplication::processEvents();
        CHECK_COND(wm->bookNames().isEmpty(), "66h: all workbooks closed");
    } else {
        std::cout << "[INFO] Suite 66h: no Python automation server; RPC check skipped\n";
    }

    // ---------------- 66i: menus and translations ----------------
    {
        QAction *a = mw->findChild<QAction *>(QStringLiteral("actionNewWorkbook"));
        CHECK_COND(a && !a->associatedWidgets().isEmpty() && !a->text().isEmpty(),
                   "66i: New Workbook action in a menu");
        const QString ja = ToriGraphFullJapaneseTranslator::lookupJapanese(QStringLiteral("New &Workbook"));
        CHECK_COND(ja.contains(QString::fromUtf8("\xE3\x83\x96\xE3\x83\x83\xE3\x82\xAF")),
                   "66i: Japanese for New Workbook");
        const QString jx = ToriGraphFullJapaneseTranslator::lookupJapanese(
                QStringLiteral("Put the sheets of a file into one workbook (sheet tabs)"));
        CHECK_COND(!jx.isEmpty(), "66i: Japanese for the import choice");
        // the window menu of a worksheet offers the workbook submenu
        Table *t = mw->newTable("Menu66", 3, 2);
        QMenu m;
        wm->populateWindowMenu(&m, t);
        QMenu *sub = m.findChild<QMenu *>(QStringLiteral("torigraph_workbook_window_menu"));
        CHECK_COND(sub && sub->actions().size() >= 2, "66i: workbook submenu");
        t->askOnCloseEvent(false);
        t->close();
        QApplication::processEvents();
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 66 workbooks passed!\n";
    return 0;
}

// ============================================================================
// Suite 69 (I10): settings export -> reset -> import round trip, lab defaults
// priority (user value > lab ini > built-in), save suppression after import/reset,
// menu actions, and graph.to_python coverage ("# 未再現:" notes, style preset,
// error band).
// ============================================================================
int runSuite69SettingsTransferAndScriptCoverage(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 69: Settings Export/Import/Reset, Lab Defaults INI & "
                 "to_python_script Coverage...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 69");
    qApp->setProperty("torigraph_noninteractive", true);
    QTemporaryDir tmp69;
    CHECK_COND(tmp69.isValid(), "69: QTemporaryDir");

    // ---------------- 69a: export -> reset -> import round trip ----------------
    {
        const QString userIni = tmp69.filePath("user.ini");
        const QString exportIni = tmp69.filePath("sub/export.ini");
        QSettings user(userIni, QSettings::IniFormat);
        const QByteArray rawBlob("\x01\x02\xff\x00\x7f", 5);
        user.setValue("General/Language", "en");
        user.setValue("UserFunctions/FitFunctions", QStringList() << "a*x+b" << "c*exp(-x/d)");
        user.setValue("Graph/Width", 123);
        user.setValue("Tables/CustomBlob", rawBlob);
        user.setValue("General/Paths/LockToolbars", true);
        // Personal / transient keys must NEVER be exported to shared lab defaults:
        user.setValue("General/RecentProjects", QStringList() << "C:/a.sciprj" << "D:/b c.sciprj");
        user.setValue("General/Paths/WorkingDir", "C:/Users/Alice/Documents");
        user.setValue("Geometry/State", rawBlob);
        user.setValue("LabDefaults/Applied/Graph/Width", 5); // bookkeeping: never exported
        user.sync();
        QString err;
        const int nOut = SettingsTransfer::exportToIni(user, exportIni, &err);
        CHECK_COND(nOut == 5, "69a: export must write only the 5 non-personal user keys (got "
                                      << nOut << ", " << err.toStdString() << ")");
        CHECK_COND(QFileInfo(exportIni).isFile(), "69a: export file missing");
        CHECK_COND(!QFileInfo(exportIni + ".tmp").exists(), "69a: temp file left behind");

        SettingsTransfer::resetAll(user);
        CHECK_COND(user.allKeys().isEmpty(), "69a: reset must remove every key");

        const int nIn = SettingsTransfer::importFromIni(user, exportIni, &err);
        CHECK_COND(nIn == 5, "69a: import must read 5 keys (got " << nIn << ")");
        CHECK_COND(user.value("General/Language").toString() == "en", "69a: Language not restored");
        CHECK_COND(user.value("UserFunctions/FitFunctions").toStringList()
                           == (QStringList() << "a*x+b" << "c*exp(-x/d)"),
                   "69a: string list not restored");
        CHECK_COND(user.value("Graph/Width").toInt() == 123, "69a: int not restored");
        CHECK_COND(user.value("Tables/CustomBlob").toByteArray() == rawBlob, "69a: byte array not restored");
        CHECK_COND(user.value("General/Paths/LockToolbars").toBool() == true,
                   "69a: LockToolbars must be preserved across export/import");
        CHECK_COND(!user.contains("General/RecentProjects")
                           && !user.contains("General/Paths/WorkingDir")
                           && !user.contains("Geometry/State")
                           && !user.contains("LabDefaults/Applied/Graph/Width"),
                   "69a: personal paths/history/geometry or bookkeeping leaked into exported settings");

        // import keeps keys that the file does not have
        user.setValue("Only/Local", 7);
        CHECK_COND(SettingsTransfer::importFromIni(user, exportIni) == 5, "69a: second import");
        CHECK_COND(user.value("Only/Local").toInt() == 7, "69a: import must not drop other keys");

        CHECK_COND(SettingsTransfer::importFromIni(user, tmp69.filePath("missing.ini"), &err) == -1,
                   "69a: missing file must fail");
        {
            QFile emptyIni(tmp69.filePath("empty.ini"));
            CHECK_COND(emptyIni.open(QIODevice::WriteOnly), "69a: empty ini");
            emptyIni.close();
        }
        CHECK_COND(SettingsTransfer::importFromIni(user, tmp69.filePath("empty.ini"), &err) == -1,
                   "69a: a file without settings must fail");
        CHECK_COND(SettingsTransfer::exportToIni(user, QString(), &err) == -1,
                   "69a: empty export path must fail");
    }

    // ---------------- 69b: lab defaults priority ----------------
    {
        CHECK_COND(SettingsTransfer::labDefaultsPath()
                           == QDir(QCoreApplication::applicationDirPath())
                                      .filePath("torigraph-lab-defaults.ini"),
                   "69b: lab defaults must live next to the executable");
        const QString labIni = tmp69.filePath("torigraph-lab-defaults.ini");
        QSettings user(tmp69.filePath("user69b.ini"), QSettings::IniFormat);
        user.setValue("Graph/Width", 123); // the user's own choice
        {
            QSettings lab(labIni, QSettings::IniFormat);
            lab.setValue("General/Language", "ja");
            lab.setValue("Graph/FontSize", 11);
            lab.setValue("Graph/Width", 999);
            lab.sync();
        }
        CHECK_COND(SettingsTransfer::applyLabDefaults(user, labIni) == 2,
                   "69b: only the 2 keys the user lacks must be filled");
        CHECK_COND(user.value("Graph/Width").toInt() == 123, "69b: user value must win over lab");
        CHECK_COND(user.value("Graph/FontSize").toInt() == 11, "69b: lab value not applied");
        CHECK_COND(user.value("General/Language").toString() == "ja", "69b: lab Language");
        CHECK_COND(SettingsTransfer::applyLabDefaults(user, labIni) == 0,
                   "69b: applying the same lab file twice must change nothing");

        // the lab revises a value the user never touched -> follows the lab
        {
            QSettings lab(labIni, QSettings::IniFormat);
            lab.setValue("Graph/FontSize", 12);
            lab.sync();
        }
        CHECK_COND(SettingsTransfer::applyLabDefaults(user, labIni) == 1, "69b: revised lab value");
        CHECK_COND(user.value("Graph/FontSize").toInt() == 12, "69b: revised lab value not followed");

        // the user changes it -> later lab revisions no longer override it
        user.setValue("Graph/FontSize", 14);
        {
            QSettings lab(labIni, QSettings::IniFormat);
            lab.setValue("Graph/FontSize", 13);
            lab.sync();
        }
        CHECK_COND(SettingsTransfer::applyLabDefaults(user, labIni) == 0,
                   "69b: the user's own change must win over a revised lab file");
        CHECK_COND(user.value("Graph/FontSize").toInt() == 14, "69b: user change overwritten");
        CHECK_COND(SettingsTransfer::applyLabDefaults(user, tmp69.filePath("nope.ini")) == 0,
                   "69b: missing lab file must be a no-op");
        // an export never carries the lab bookkeeping
        const QString ex = tmp69.filePath("export69b.ini");
        CHECK_COND(SettingsTransfer::exportToIni(user, ex) == 3, "69b: export count");
        QSettings exS(ex, QSettings::IniFormat);
        for (const QString &k : exS.allKeys())
            CHECK_COND(!k.startsWith("LabDefaults"), "69b: bookkeeping exported: " << k.toStdString());
    }

    // ---------------- 69c: save suppressed after import / reset ----------------
    {
        QSettings &live = ApplicationWindow::getSettings();
        mw->saveSettings();
        live.sync();
        CHECK_COND(live.contains("General/Language"), "69c: saveSettings must write General/Language");
        live.remove("General/Language");
        SettingsTransfer::setSaveSuppressed(true);
        mw->saveSettings();
        live.sync();
        const bool written = live.contains("General/Language");
        SettingsTransfer::setSaveSuppressed(false);
        CHECK_COND(!written, "69c: saveSettings must not run after an import/reset");
        mw->saveSettings();
        live.sync();
        CHECK_COND(live.contains("General/Language"), "69c: saveSettings must work again");
    }

    // ---------------- 69d: menu actions ----------------
    {
        const char *const names[] = { "actionSettingsExport", "actionSettingsImport",
                                      "actionSettingsReset" };
        for (const char *n : names) {
            auto *a = mw->findChild<QAction *>(QString::fromLatin1(n));
            CHECK_COND(a != nullptr, "69d: missing action " << n);
            CHECK_COND(!a->text().isEmpty() && !a->toolTip().isEmpty(), "69d: empty text " << n);
        }
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese("E&xport Settings...")
                           == QString::fromUtf8("設定を書き出す(&X)..."),
                   "69d: Japanese text of Export Settings");
        CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese("&Reset Settings...")
                           == QString::fromUtf8("設定を初期化(&R)..."),
                   "69d: Japanese text of Reset Settings");
    }

    // ---------------- 69e: to_python_script coverage ----------------
    {
        const QString prevLang69e = mw->appLanguage;
        mw->switchToLanguage(QStringLiteral("ja"));
        QCoreApplication::processEvents();
        Table *t = mw->newTable("T69", 6, 4);
        CHECK_COND(t != nullptr, "69e: table");
        t->setHeader(QStringList() << "x" << "y" << "err" << "y2");
        for (int r = 0; r < 6; ++r) {
            t->setCell(r, 0, r);
            t->setCell(r, 1, 2.0 * r);
            t->setCell(r, 2, 0.5);
            t->setCell(r, 3, 10.0 - r);
        }
        MultiLayer *ml = mw->newGraph("G69");
        CHECK_COND(ml && ml->activeGraph(), "69e: graph");
        Graph *g = ml->activeGraph();
        CHECK_COND(g->insertCurve(t, "T69_x", "T69_y", Graph::LineSymbols), "69e: curve y");
        CHECK_COND(g->insertCurve(t, "T69_x", "T69_y2", Graph::LineSymbols), "69e: curve y2");
        CHECK_COND(ExtraPlotTypes::addErrorBand(g, "T69_y", t, "T69_err", QString(), 70) == 1,
                   "69e: error band");
        MultiLayer::GraphStylePresetSpec spec;
        CHECK_COND(MultiLayer::findStylePreset("isij_85mm", &spec), "69e: isij_85mm preset");
        ml->applyStylePresetSpec(spec);

        QString code = PythonAutomationServer::generatePythonScriptForWindow(mw, ml, true, "inline");
        CHECK_COND(code.contains("g.apply_style_preset('isij_85mm')"),
                   "69e: style preset not reproduced:\n" << code.toStdString());
        CHECK_COND(code.contains("g.add_error_band(0, 'err', table=tbl.name, alpha=70)"),
                   "69e: error band not reproduced:\n" << code.toStdString());
        CHECK_COND(!code.contains("g.add_error_bars('err'"),
                   "69e: an error band must not come back as error bars");
        CHECK_COND(!code.contains(QString::fromUtf8("# 未再現:")),
                   "69e: nothing unreproducible yet, but got a note:\n" << code.toStdString());
        CHECK_COND(code.contains(QString::fromUtf8("解析やフィットの計算そのものは再現しません")),
                   "69e: scope header missing");

        // fit + stack offsets -> "# 未再現:" notes before the code
        g->addFitCurve(g->curve(1));
        g->setStackOffsets(0, 15.0, 0.0, false);
        code = PythonAutomationServer::generatePythonScriptForWindow(mw, ml, true, "inline");
        const int fitNote = code.indexOf(QString::fromUtf8("# 未再現: フィット"));
        const int stackNote = code.indexOf(QString::fromUtf8("# 未再現: スタックオフセット"));
        const int connectAt = code.indexOf("app = tg.connect_or_launch()");
        CHECK_COND(fitNote > 0, "69e: fit note missing:\n" << code.toStdString());
        CHECK_COND(stackNote > 0, "69e: stack offset note missing:\n" << code.toStdString());
        CHECK_COND(connectAt > fitNote && connectAt > stackNote,
                   "69e: notes must come before the code");
        CHECK_COND(code.count(QString::fromUtf8("# 未再現: フィット")) == 1,
                   "69e: a note must appear once");
        g->deleteFitCurves();

        // an unknown preset id is reported instead of emitted
        ml->setProperty("toriAppliedStylePreset", QStringLiteral("no_such_preset_69"));
        code = PythonAutomationServer::generatePythonScriptForWindow(mw, ml, true, "inline");
        CHECK_COND(!code.contains("apply_style_preset('no_such_preset_69')"),
                   "69e: unknown preset must not be emitted");
        CHECK_COND(code.contains("no_such_preset_69") && code.contains(QString::fromUtf8("# 未再現: スタイルプリセット")),
                   "69e: unknown preset must be noted");

        mw->closeWindow(ml);
        mw->closeWindow(t);
        mw->switchToLanguage(prevLang69e.isEmpty() ? QStringLiteral("en") : prevLang69e);
        QCoreApplication::processEvents();
    }

    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 69 settings transfer & script coverage passed!\n";
    return 0;
}

// ============================================================================
// Suite 72: ToriGraph Standard palette is the default for new curves
// ============================================================================
int runSuite72ToriGraphStandardDefaultPalette(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 72: ToriGraph Standard default curve palette...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 72");
    qApp->setProperty("torigraph_noninteractive", true);
    QTemporaryDir tmp72;
    CHECK_COND(tmp72.isValid(), "Suite 72: QTemporaryDir failed");

    const QString kKey = QStringLiteral("/Curves/DefaultPalette");
    QSettings &settings = ApplicationWindow::getSettings();
    const bool hadStored = settings.contains(kKey);
    const QVariant prevStored = settings.value(kKey);
    const QString prevNewGraphPreset = MultiLayer::newGraphStylePresetId();
    MultiLayer::setNewGraphStylePresetId(QString()); // plain graphs for colour checks

    const QColor kBlack("#000000"), kVermilion("#d55e00"), kBlue("#0072b2");
    auto makeTable = [](ApplicationWindow *app, const QString &name, int nY) -> Table * {
        Table *t = app->newTable(name, 8, nY + 1);
        if (!t)
            return nullptr;
        t->setColName(0, "x");
        for (int c = 1; c <= nY; ++c)
            t->setColName(c, QStringLiteral("y%1").arg(c));
        for (int r = 0; r < 8; ++r) {
            t->setCell(r, 0, r + 1.0);
            for (int c = 1; c <= nY; ++c)
                t->setCell(r, c, (r + 1.0) * c + 0.5 * c);
        }
        return t;
    };
    auto yCols = [](Table *t, int nY) {
        QStringList cols;
        for (int c = 1; c <= nY; ++c)
            cols << (t->name() + QStringLiteral("_y%1").arg(c));
        return cols;
    };
    auto closeMl = [](MultiLayer *ml) {
        if (!ml)
            return;
        ml->askOnCloseEvent(false);
        ml->close();
        QApplication::processEvents();
    };

    // ---- 72a: palette definition and the default when nothing is stored ------------------
    {
        const QList<QColor> std8 = Graph::toriGraphStandardColors();
        CHECK_COND(std8.size() == 8, "72a: ToriGraph Standard must have 8 colours");
        CHECK_COND(std8.at(0) == kBlack && std8.at(1) == kVermilion && std8.at(2) == kBlue,
                   "72a: ToriGraph Standard must start black, vermilion, blue");
        CHECK_COND(!std8.contains(QColor("#f0e442")),
                   "72a: Okabe-Ito yellow is hard to see on white and must not be used");
        for (const QColor &c : std8)
            CHECK_COND(c != QColor(Qt::white), "72a: no white entry");
        CHECK_COND(Graph::builtInPaletteNames().value(0) == Graph::toriGraphStandardPaletteName(),
                   "72a: ToriGraph Standard must be listed first among the built-in palettes");
        CHECK_COND(Graph::paletteColors(QStringLiteral("torigraph_standard")) == std8
                           && Graph::paletteColors(QStringLiteral("ToriGraph Standard")) == std8,
                   "72a: palette name and alias must resolve");
        // Other palettes are still available and unchanged.
        CHECK_COND(Graph::paletteColors(Graph::originClassicPaletteName()).value(1) == QColor(Qt::red),
                   "72a: Origin Classic must stay available (black, red, ...)");
        CHECK_COND(Graph::paletteColors(QStringLiteral("Okabe-Ito")).value(0) == QColor("#0072b2"),
                   "72a: Okabe-Ito must stay available");

        // A user who never chose a palette (no key stored) gets the new default.
        settings.remove(kKey);
        Graph::reloadDefaultCurvePaletteName();
        CHECK_COND(Graph::defaultCurvePaletteName() == Graph::toriGraphStandardPaletteName(),
                   "72a: default palette must be ToriGraph Standard when nothing is stored, got "
                           << Graph::defaultCurvePaletteName().toStdString());
        CHECK_COND(!settings.contains(kKey), "72a: reading the default must not store it");
        // Clearing the choice returns to "not chosen" (the key is removed).
        Graph::setDefaultCurvePaletteName(QStringLiteral("Okabe-Ito"));
        CHECK_COND(settings.value(kKey).toString() == Graph::okabeItoPaletteName(), "72a: store");
        Graph::setDefaultCurvePaletteName(QString());
        CHECK_COND(!settings.contains(kKey)
                           && Graph::defaultCurvePaletteName() == Graph::toriGraphStandardPaletteName(),
                   "72a: an empty name must remove the stored choice");
    }

    // ---- 72b: three new curves are black, vermilion, blue; shapes cycle as before ----------
    QString savedGraphName;
    {
        // A separate window so the saved project holds only this graph.
        ApplicationWindow *writer = new ApplicationWindow();
        writer->applyUserSettings();
        Table *t = makeTable(writer, QStringLiteral("Pal72"), 3);
        CHECK_COND(t != nullptr, "72b: table");
        MultiLayer *ml = writer->multilayerPlot(t, yCols(t, 3), Graph::LineSymbols);
        CHECK_COND(ml && ml->activeGraph() && ml->activeGraph()->curves() == 3, "72b: graph");
        Graph *g = ml->activeGraph();
        const QColor c0 = g->curve(0)->pen().color();
        const QColor c1 = g->curve(1)->pen().color();
        const QColor c2 = g->curve(2)->pen().color();
        std::cout << "[INFO] 72b curve colours: " << c0.name().toStdString() << " "
                  << c1.name().toStdString() << " " << c2.name().toStdString() << "\n";
        CHECK_COND(c0 == kBlack && c1 == kVermilion && c2 == kBlue,
                   "72b: new curves must be black, vermilion, blue");
        CHECK_COND(g->curve(1)->symbol().pen().color() == kVermilion,
                   "72b: symbol edge follows the curve colour");
        CHECK_COND(g->curve(0)->symbol().style() == QwtSymbol::Rect
                           && g->curve(1)->symbol().style() == QwtSymbol::Ellipse
                           && g->curve(2)->symbol().style() == QwtSymbol::UTriangle,
                   "72b: symbol shapes must still cycle square, circle, up-triangle");

        // ---- 72e (write): the graph is saved with the new default colours ----
        const QString prjPath = tmp72.filePath(QStringLiteral("suite72_palette.sciprj"));
        {
            QFile pf(prjPath);
            CHECK_COND(pf.open(QIODevice::WriteOnly), "72e: cannot write .sciprj");
            pf.write(writer->saveProjectToByteArray());
            pf.close();
        }
        savedGraphName = ml->name();
        writer->savedProject();
        writer->setOleEmbeddedMode(true);
        delete writer;
        QApplication::processEvents();

        // ---- 72c: bars follow the default palette too ----
        Table *tb = makeTable(mw, QStringLiteral("Bar72"), 2);
        CHECK_COND(tb != nullptr, "72c: table");
        MultiLayer *mlb = mw->multilayerPlot(tb, yCols(tb, 2), Graph::VerticalBars);
        CHECK_COND(mlb && mlb->activeGraph() && mlb->activeGraph()->curves() == 2, "72c: graph");
        const QColor b0 = mlb->activeGraph()->curve(0)->brush().color();
        const QColor b1 = mlb->activeGraph()->curve(1)->brush().color();
        std::cout << "[INFO] 72c bar fills: " << b0.name().toStdString() << " "
                  << b1.name().toStdString() << "\n";
        CHECK_COND(b0 == kBlack && b1 == kVermilion, "72c: bar fills must follow the default palette");
        closeMl(mlb);
        mw->closeWindow(tb);

        // ---- 72d: an explicit Origin Classic choice is respected (also after a restart) ----
        Graph::setDefaultCurvePaletteName(Graph::originClassicPaletteName());
        Graph::reloadDefaultCurvePaletteName(); // as if ToriGraph had been restarted
        CHECK_COND(settings.value(kKey).toString() == Graph::originClassicPaletteName()
                           && Graph::defaultCurvePaletteName() == Graph::originClassicPaletteName(),
                   "72d: a stored Origin Classic choice must be kept");
        Table *tc = makeTable(mw, QStringLiteral("Cls72"), 3);
        CHECK_COND(tc != nullptr, "72d: table");
        MultiLayer *mlc = mw->multilayerPlot(tc, yCols(tc, 3), Graph::LineSymbols);
        CHECK_COND(mlc && mlc->activeGraph() && mlc->activeGraph()->curves() == 3, "72d: graph");
        Graph *gc = mlc->activeGraph();
        CHECK_COND(gc->curve(0)->pen().color() == QColor(Qt::black)
                           && gc->curve(1)->pen().color() == QColor(Qt::red)
                           && gc->curve(2)->pen().color() == QColor(Qt::blue),
                   "72d: with Origin Classic chosen, new curves must be black, red, blue");
        closeMl(mlc);
        mw->closeWindow(tc);

        // ---- 72e (read): colours are stored per curve, not re-derived from the palette ----
        ApplicationWindow *reader = new ApplicationWindow();
        reader->applyUserSettings();
        CHECK_COND(reader->loadProject(prjPath), "72e: loadProject failed");
        MultiLayer *mlr = qobject_cast<MultiLayer *>(reader->window(savedGraphName));
        CHECK_COND(mlr && mlr->activeGraph() && mlr->activeGraph()->curves() == 3,
                   "72e: reloaded graph not found: " << savedGraphName.toStdString());
        Graph *gr = mlr->activeGraph();
        CHECK_COND(gr->curve(0)->pen().color() == kBlack && gr->curve(1)->pen().color() == kVermilion
                           && gr->curve(2)->pen().color() == kBlue,
                   "72e: saved colours must survive reload even when the default palette differs");
        CHECK_COND(gr->curve(1)->symbol().pen().color() == kVermilion, "72e: symbol colour reload");
        reader->savedProject();
        reader->setOleEmbeddedMode(true);
        delete reader;
        QApplication::processEvents();
    }

    // ---- 72f: Japanese names show the order ---------------------------------------------
    {
        const QString prevLang = mw->appLanguage;
        mw->switchToLanguage("ja");
        Graph::registerPaletteTranslations();
        const QString jaName = QCoreApplication::translate("ApplicationWindow", "ToriGraph Standard");
        const QString jaLong = QCoreApplication::translate(
                "QObject", "ToriGraph Standard (black, vermilion, blue, ...)");
        std::cout << "[INFO] 72f: " << jaName.toUtf8().constData() << " / "
                  << jaLong.toUtf8().constData() << "\n";
        CHECK_COND(jaName.contains(QString::fromUtf8("標準"))
                           && jaName.contains(QString::fromUtf8("黒・朱・青")),
                   "72f: Japanese palette name must show the colour order");
        CHECK_COND(jaLong == jaName, "72f: the dialog entry must use the same Japanese name");
        mw->switchToLanguage(prevLang.isEmpty() ? QStringLiteral("en") : prevLang);
    }

    // ---- 72g: a lab-default ini palette applies even if the default was read before ----
    {
        settings.remove(kKey);
        Graph::reloadDefaultCurvePaletteName();
        // Start-up reads the default (graph inspector) before readSettings() applies the lab ini.
        CHECK_COND(Graph::defaultCurvePaletteName() == Graph::toriGraphStandardPaletteName(),
                   "72g: precondition");
        const QString labIni = tmp72.filePath(QStringLiteral("torigraph-lab-defaults.ini"));
        {
            QSettings lab(labIni, QSettings::IniFormat);
            lab.setValue(QStringLiteral("Curves/DefaultPalette"), QStringLiteral("Origin Classic"));
            lab.sync();
        }
        CHECK_COND(SettingsTransfer::applyLabDefaults(settings, labIni) >= 1,
                   "72g: lab ini must seed the palette for a user who never chose one");
        Graph::reloadDefaultCurvePaletteName(); // what readSettings() now does after the ini
        CHECK_COND(Graph::defaultCurvePaletteName() == Graph::originClassicPaletteName(),
                   "72g: lab ini palette must take effect after reload");
        Table *tl = makeTable(mw, QStringLiteral("Lab72"), 3);
        CHECK_COND(tl != nullptr, "72g: table");
        MultiLayer *mll = mw->multilayerPlot(tl, yCols(tl, 3), Graph::LineSymbols);
        CHECK_COND(mll && mll->activeGraph() && mll->activeGraph()->curves() == 3, "72g: graph");
        Graph *gl = mll->activeGraph();
        CHECK_COND(gl->curve(0)->pen().color() == QColor(Qt::black)
                           && gl->curve(1)->pen().color() == QColor(Qt::red)
                           && gl->curve(2)->pen().color() == QColor(Qt::blue),
                   "72g: lab ini Origin Classic must give black, red, blue");
        closeMl(mll);
        mw->closeWindow(tl);
        settings.remove(QStringLiteral("LabDefaults"));
        settings.remove(kKey);
        Graph::reloadDefaultCurvePaletteName();
    }

    // ---- 72h: the inspector's series scheme follows the default until the user picks one --
    {
        GraphInspectorDock *dock = mw->graphInspectorDock();
        CHECK_COND(dock != nullptr, "72h: graph inspector dock");
        dock->syncSeriesSchemeToDefaultPalette();
        CHECK_COND(dock->seriesSchemePaletteName() == Graph::toriGraphStandardPaletteName(),
                   "72h: inspector scheme must start at the default palette, got "
                           << dock->seriesSchemePaletteName().toStdString());
        Graph::setDefaultCurvePaletteName(Graph::originClassicPaletteName());
        dock->syncSeriesSchemeToDefaultPalette();
        CHECK_COND(dock->seriesSchemePaletteName() == Graph::originClassicPaletteName(),
                   "72h: untouched inspector scheme must follow a changed default");
        // The user picks Viridis in the combo: that choice is kept from now on.
        QComboBox *schemeCombo = nullptr;
        for (QComboBox *cb : dock->findChildren<QComboBox *>()) {
            if (cb->findData(Graph::toriGraphStandardPaletteName()) >= 0
                && cb->findData(QStringLiteral("Viridis")) >= 0) {
                schemeCombo = cb;
                break;
            }
        }
        CHECK_COND(schemeCombo != nullptr, "72h: series scheme combo not found");
        const int viridisIdx = schemeCombo->findData(QStringLiteral("Viridis"));
        schemeCombo->setCurrentIndex(viridisIdx);
        emit schemeCombo->activated(viridisIdx);
        Graph::setDefaultCurvePaletteName(QStringLiteral("Okabe-Ito"));
        dock->syncSeriesSchemeToDefaultPalette();
        CHECK_COND(dock->seriesSchemePaletteName() == QStringLiteral("Viridis"),
                   "72h: a scheme the user picked must not be replaced by the default");
        Graph::setDefaultCurvePaletteName(QString());
    }

    // ---- 72i: custom palettes cannot hide built-in names; Python name matching ----------
    {
        for (const char *nm : { "ToriGraph", "torigraph_standard", "ToriGraph Standard",
                                "origin_classic", "viridis", "Okabe-Ito" }) {
            QString err;
            CHECK_COND(!Graph::saveCustomPalette(QString::fromLatin1(nm),
                                                 QList<QColor>() << QColor(Qt::green), &err)
                               && !err.isEmpty(),
                       "72i: saving a custom palette named '" << nm << "' must be refused");
        }
        CHECK_COND(Graph::paletteColors(QStringLiteral("ToriGraph")) == Graph::toriGraphStandardColors(),
                   "72i: built-in palette must still win");
        CHECK_COND(!Graph::isBuiltInPaletteName(QStringLiteral("My Lab Colours")),
                   "72i: an ordinary name is not reserved");

        const QStringList pals = Graph::allPaletteNames();
        struct Want {
            const char *raw;
            QString expect;
        };
        const Want wants[] = {
            { "orig", Graph::originClassicPaletteName() },
            { "origin_classic", Graph::originClassicPaletteName() },
            { "ToriGraph", Graph::toriGraphStandardPaletteName() },
            { "torigraph_standard", Graph::toriGraphStandardPaletteName() },
            { "tab10", Graph::tab10PaletteName() },
            { "okabe_ito", Graph::okabeItoPaletteName() },
            { "colorblind", Graph::okabeItoPaletteName() },
            { "viridis", QStringLiteral("Viridis") },
            { "npg", QStringLiteral("Nature (NPG)") },
        };
        for (const Want &w : wants) {
            const QString got =
                    PythonAutomationServer::matchPaletteName(QString::fromLatin1(w.raw), pals);
            CHECK_COND(got == w.expect, "72i: palette '" << w.raw << "' resolved to '"
                                                         << got.toStdString() << "', expected '"
                                                         << w.expect.toStdString() << "'");
        }
    }

    // restore
    if (hadStored)
        settings.setValue(kKey, prevStored);
    else
        settings.remove(kKey);
    Graph::reloadDefaultCurvePaletteName();
    MultiLayer::setNewGraphStylePresetId(prevNewGraphPreset);
    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 72 ToriGraph Standard default palette passed!\n";
    return 0;
}

// ============================================================================================
// Suite 74 (C15): legacy UI hidden from menus, ribbon, command search and shortcuts
// ============================================================================================
static bool suite74ShownInMenuTree(QMenu *m, QAction *target, QSet<QMenu *> &seen)
{
    if (!m || seen.contains(m))
        return false;
    seen.insert(m);
    for (QAction *a : m->actions()) {
        if (!a || !a->isVisible())
            continue;
        if (a == target)
            return true;
        if (QMenu *sub = a->menu()) {
            if (suite74ShownInMenuTree(sub, target, seen))
                return true;
        }
    }
    return false;
}

// Is the action shown anywhere below the menu bar (all QMenus, recursively)?
static bool suite74ShownInMenuBar(ApplicationWindow *mw, QAction *target)
{
    QSet<QMenu *> seen;
    for (QAction *top : mw->menuBar()->actions()) {
        if (!top || !top->isVisible())
            continue;
        if (top == target)
            return true;
        if (suite74ShownInMenuTree(top->menu(), target, seen))
            return true;
    }
    return false;
}

// Walk a menu and its sub-menus: is the action, or a visible copy with the same text (the
// ribbon's folded-group menus copy buttons into new actions), listed as visible?
static bool suite74MenuListsVisible(QMenu *m, QAction *target, const QString &title,
                                    QSet<QMenu *> &seen)
{
    if (!m || seen.contains(m))
        return false;
    seen.insert(m);
    for (QAction *a : m->actions()) {
        if (!a || a->isSeparator() || !a->isVisible())
            continue;
        if (a == target || cleanMnemonicForSuite62(a->text()) == title)
            return true;
        if (suite74MenuListsVisible(a->menu(), target, title, seen))
            return true;
    }
    return false;
}

// Is the action shown in any menu of the main window (menu bar, ribbon drop-downs, context
// menus kept as children), walking QMenu::actions() recursively?
static bool suite74ShownInAnyMenu(ApplicationWindow *mw, QAction *target)
{
    const QString title = cleanMnemonicForSuite62(target->text());
    QSet<QMenu *> seen;
    for (QMenu *m : mw->findChildren<QMenu *>()) {
        if (suite74MenuListsVisible(m, target, title, seen))
            return true;
    }
    return false;
}

// Fill the ribbon's folded-group menus (they are built on aboutToShow) and look for an entry.
static bool suite74ListedInFoldedRibbonMenus(ModernRibbonBar *ribbon, const QString &text)
{
    bool listed = false;
    for (QToolButton *b : ribbon->findChildren<QToolButton *>("RibbonCollapsedGroupBtn")) {
        QMenu *m = b->menu();
        if (!m)
            continue;
        emit m->aboutToShow();
        for (QAction *a : m->actions()) {
            if (a && a->isVisible() && a->text().trimmed() == text.trimmed())
                listed = true;
        }
    }
    return listed;
}

static bool suite74ListedInCommandSearch(CommandPalettePopup *cp, const QString &title)
{
    cp->refreshCommands();
    cp->setFilterText(title);
    bool listed = false;
    for (int i = 0; i < cp->resultsList()->count(); ++i) {
        if (cp->resultsList()->item(i)->text() == title)
            listed = true;
    }
    cp->setFilterText(QString());
    return listed;
}

static QStringList suite74ActionsWithKey(ApplicationWindow *mw, const QKeySequence &ks)
{
    QStringList names;
    for (QAction *a : mw->findChildren<QAction *>()) {
        if (a && a->shortcuts().contains(ks))
            names << a->objectName();
    }
    return names;
}

int runSuite74LegacyUiHidden(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 74: Legacy UI hidden (C15)...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 74");
    ModernRibbonBar *ribbon = mw->modernRibbonBar();
    CHECK_COND(ribbon != nullptr, "74: ribbon missing");
    CommandPalettePopup *cp = ribbon->commandPalette();
    CHECK_COND(cp != nullptr, "74: command palette missing");
    qApp->setProperty("torigraph_noninteractive", true);

    const bool wasShown = mw->showsLegacyUi();
    CHECK_COND(!wasShown, "74: legacy UI must be off by default");

    // A graph is active so the Graph menu (time stamp) is in the menu bar.
    MultiLayer *ml = mw->newGraph("G74");
    CHECK_COND(ml && ml->activeGraph(), "74: graph");
    mw->activateSubWindow(ml);
    mw->customMenu(ml);

    const QStringList names = ApplicationWindow::legacyUiActionNames();
    CHECK_COND(names.size() == 4, "74: unexpected legacy action list");
    QList<QAction *> acts;
    for (const QString &n : names) {
        QAction *a = mw->findChild<QAction *>(n);
        CHECK_COND(a != nullptr, "74: QAction must still exist: " << n.toStdString());
        acts << a;
    }
    QAction *timeStamp = mw->findChild<QAction *>("actionTimeStamp");
    QAction *plotWizard = mw->findChild<QAction *>("actionShowPlotWizard");
    QAction *lockBars = mw->findChild<QAction *>("locktoolbar");
    QMenu *toolbarsMenu = mw->findChild<QMenu *>("menuToolbars");
    CHECK_COND(toolbarsMenu != nullptr, "74: View > Toolbars menu missing");
    QToolButton *classicBtn = ribbon->findChild<QToolButton *>("RibbonClassicToolbarsButton");
    CHECK_COND(classicBtn != nullptr, "74: ribbon Classic Toolbars button missing");
    QToolBar *const legacyBars[] = { mw->file_tools,  mw->edit_tools,  mw->graph_tools,
                                     mw->plot_tools,  mw->table_tools, mw->matrix_plot_tools,
                                     mw->graph_3D_tools };
    const QKeySequence keyTimeStamp(QStringLiteral("Ctrl+Alt+T"));
    const QKeySequence keyWizard(QStringLiteral("Ctrl+Alt+W"));

    // ---- 74a: hidden by default from menus, ribbon, right-click menu and command search
    {
        QAction *toggle = mw->findChild<QAction *>("actionShowLegacyUi");
        CHECK_COND(toggle && toggle->isCheckable() && !toggle->isChecked(),
                   "74a: Show Legacy UI switch missing or checked by default");
        CHECK_COND(suite74ShownInMenuBar(mw, toggle), "74a: Show Legacy UI switch not in the menus");
        for (QAction *a : acts) {
            CHECK_COND(!a->isVisible(), "74a: visible by default: " << a->objectName().toStdString());
            CHECK_COND(!suite74ShownInMenuBar(mw, a),
                       "74a: still in the menu bar: " << a->objectName().toStdString());
            CHECK_COND(!suite74ShownInAnyMenu(mw, a),
                       "74a: still in a menu: " << a->objectName().toStdString());
            const QString title = cleanMnemonicForSuite62(a->text());
            CHECK_COND(!suite74ListedInCommandSearch(cp, title),
                       "74a: still found by command search: " << title.toStdString());
        }
        CHECK_COND(!toolbarsMenu->menuAction()->isVisible()
                           && !suite74ShownInMenuBar(mw, toolbarsMenu->menuAction()),
                   "74a: View > Toolbars still shown");
        CHECK_COND(classicBtn->isHidden(), "74a: ribbon Classic Toolbars button still shown");
        for (QToolBar *tb : legacyBars) {
            CHECK_COND(tb && tb->isHidden(), "74a: a classic toolbar is shown");
            CHECK_COND(!tb->toggleViewAction()->isVisible(),
                       "74a: toolbar switch visible: " << tb->objectName().toStdString());
        }
        QMenu *popup = mw->createPopupMenu();
        if (popup) {
            for (QAction *pa : popup->actions()) {
                for (QToolBar *tb : legacyBars)
                    CHECK_COND(!(pa == tb->toggleViewAction() && pa->isVisible()),
                               "74a: right-click menu lists toolbar " << tb->objectName().toStdString());
            }
            delete popup;
        }
        // the ribbon's folded-group menus are rebuilt (on aboutToShow) from visible buttons only
        CHECK_COND(!suite74ListedInFoldedRibbonMenus(ribbon, classicBtn->text()),
                   "74a: Classic Toolbars listed in a folded ribbon group menu");
    }

    // ---- 74b: shortcuts of hidden actions are off (and stay off after retranslation)
    {
        CHECK_COND(timeStamp->shortcut().isEmpty() && plotWizard->shortcut().isEmpty(),
                   "74b: hidden actions keep their shortcuts");
        CHECK_COND(!timeStamp->isEnabled() && !plotWizard->isEnabled(),
                   "74b: hidden actions must not react to keys");
        // nobody else owns these keys, so they are simply free now
        CHECK_COND(suite74ActionsWithKey(mw, keyTimeStamp).isEmpty(),
                   "74b: Ctrl+Alt+T still bound: "
                           << suite74ActionsWithKey(mw, keyTimeStamp).join(",").toStdString());
        CHECK_COND(suite74ActionsWithKey(mw, keyWizard).isEmpty(),
                   "74b: Ctrl+Alt+W still bound: "
                           << suite74ActionsWithKey(mw, keyWizard).join(",").toStdString());
        mw->translateActionsStrings(); // what a language switch does
        CHECK_COND(timeStamp->shortcut().isEmpty() && plotWizard->shortcut().isEmpty(),
                   "74b: retranslation brought the shortcuts of hidden actions back");
        CHECK_COND(!timeStamp->isVisible(), "74b: retranslation showed a hidden action");
    }

    // ---- 74c: the actions still work from code (scripts, OLE, Python SDK)
    {
        Graph *g = ml->activeGraph();
        const int before = g->textMarkerKeys().size();
        timeStamp->trigger();
        CHECK_COND(g->textMarkerKeys().size() == before + 1,
                   "74c: hidden Add Time Stamp did not run on trigger()");
        const bool movable = mw->file_tools->isMovable();
        lockBars->trigger();
        CHECK_COND(mw->file_tools->isMovable() != movable, "74c: hidden Lock Toolbars did not run");
        lockBars->trigger();
        CHECK_COND(mw->file_tools->isMovable() == movable, "74c: Lock Toolbars not restored");
    }

    // ---- 74d: the hidden setting brings everything back
    {
        mw->setShowLegacyUi(true);
        CHECK_COND(mw->showsLegacyUi(), "74d: switch did not turn on");
        if (!mw->isHeadlessMode())
            CHECK_COND(ApplicationWindow::getSettings().value("/UX/ShowLegacyUi", false).toBool(),
                       "74d: setting not stored");
        CHECK_COND(mw->findChild<QAction *>("actionShowLegacyUi")->isChecked(),
                   "74d: menu switch not checked");
        mw->customMenu(ml);
        for (QAction *a : acts) {
            CHECK_COND(a->isVisible() && a->isEnabled(),
                       "74d: not shown again: " << a->objectName().toStdString());
            CHECK_COND(suite74ShownInMenuBar(mw, a),
                       "74d: not in the menu bar again: " << a->objectName().toStdString());
        }
        CHECK_COND(suite74ListedInCommandSearch(cp, cleanMnemonicForSuite62(plotWizard->text())),
                   "74d: Plot Wizard not found by command search");
        CHECK_COND(timeStamp->shortcut() == keyTimeStamp, "74d: Ctrl+Alt+T not restored");
        CHECK_COND(plotWizard->shortcut() == keyWizard, "74d: Ctrl+Alt+W not restored");
        CHECK_COND(suite74ActionsWithKey(mw, keyTimeStamp) == QStringList { "actionTimeStamp" },
                   "74d: Ctrl+Alt+T must belong to Add Time Stamp only");
        CHECK_COND(suite74ActionsWithKey(mw, keyWizard) == QStringList { "actionShowPlotWizard" },
                   "74d: Ctrl+Alt+W must belong to Plot Wizard only");
        CHECK_COND(toolbarsMenu->menuAction()->isVisible()
                           && suite74ShownInMenuBar(mw, toolbarsMenu->menuAction()),
                   "74d: View > Toolbars not shown again");
        CHECK_COND(!classicBtn->isHidden(), "74d: ribbon Classic Toolbars button not shown again");
        CHECK_COND(suite74ListedInFoldedRibbonMenus(ribbon, classicBtn->text()),
                   "74d: Classic Toolbars missing from its folded ribbon group menu");
        for (QAction *a : acts)
            CHECK_COND(suite74ShownInAnyMenu(mw, a),
                       "74d: menu walk does not find " << a->objectName().toStdString());
        for (QToolBar *tb : legacyBars)
            CHECK_COND(tb->toggleViewAction()->isVisible(), "74d: toolbar switch still hidden");
        // the classic toolbars themselves stay off until asked for
        CHECK_COND(mw->file_tools->isHidden(), "74d: classic toolbars must not pop up");
        ribbon->setClassicToolbarsVisible(true);
        CHECK_COND(!mw->file_tools->isHidden(), "74d: classic toolbars cannot be shown");
    }

    // ---- 74e: turning it off again hides the toolbars and the switches
    {
        mw->setShowLegacyUi(false);
        CHECK_COND(mw->file_tools->isHidden(), "74e: classic toolbars left on screen");
        for (QAction *a : acts)
            CHECK_COND(!a->isVisible() && !suite74ShownInMenuBar(mw, a),
                       "74e: not hidden again: " << a->objectName().toStdString());
        CHECK_COND(timeStamp->shortcut().isEmpty() && plotWizard->shortcut().isEmpty(),
                   "74e: shortcuts not removed again");
        CHECK_COND(classicBtn->isHidden() && !toolbarsMenu->menuAction()->isVisible(),
                   "74e: toolbar switches not hidden again");
        if (!mw->isHeadlessMode())
            CHECK_COND(!ApplicationWindow::getSettings().value("/UX/ShowLegacyUi", true).toBool(),
                       "74e: setting not stored");
        for (QAction *a : acts)
            CHECK_COND(!suite74ShownInAnyMenu(mw, a),
                       "74e: still in a menu: " << a->objectName().toStdString());
        CHECK_COND(!suite74ListedInFoldedRibbonMenus(ribbon, classicBtn->text()),
                   "74e: Classic Toolbars still in a folded ribbon group menu");
    }

    // ---- 74g: switching it off during the focus HUD (F11) must not bring the classic
    //      toolbars back when the HUD closes (their switches are hidden by then)
    {
        mw->setShowLegacyUi(true);
        ribbon->setClassicToolbarsVisible(true);
        CHECK_COND(!mw->file_tools->isHidden(), "74g: classic toolbars not shown before the HUD");
        ribbon->setFocusHudMode(true);
        CHECK_COND(ribbon->isFocusHudMode() && mw->file_tools->isHidden(), "74g: HUD did not start");
        mw->setShowLegacyUi(false);
        ribbon->setFocusHudMode(false);
        CHECK_COND(!ribbon->isFocusHudMode(), "74g: HUD did not end");
        for (QToolBar *tb : legacyBars)
            CHECK_COND(tb->isHidden(),
                       "74g: classic toolbar back after the HUD: " << tb->objectName().toStdString());
        CHECK_COND(!mw->statusBar()->isHidden(), "74g: HUD exit must still restore the status bar");
    }

    // ---- 74h: a real language switch keeps the shortcuts off and translates the switches
    {
        const QString startLang = mw->appLanguage;
        const bool startJa = startLang.startsWith("ja");
        auto checkLang = [&](bool ja) -> int {
            CHECK_COND(timeStamp->shortcut().isEmpty() && plotWizard->shortcut().isEmpty(),
                       "74h: language switch brought the shortcuts back (ja=" << ja << ")");
            CHECK_COND(!timeStamp->isVisible() && !plotWizard->isVisible(),
                       "74h: language switch showed hidden actions");
            QAction *legacy = mw->findChild<QAction *>("actionShowLegacyUi");
            QAction *adv = mw->findChild<QAction *>("actionShowAdvancedDomainFeatures");
            CHECK_COND(legacy && adv, "74h: switches missing");
            const QString wantLegacy =
                    ja ? ToriGraphFullJapaneseTranslator::lookupJapanese("Show Legacy UI")
                       : QStringLiteral("Show Legacy UI");
            const QString wantAdv = ja ? ToriGraphFullJapaneseTranslator::lookupJapanese(
                                                 "Show Other-Field / Advanced Features")
                                       : QStringLiteral("Show Other-Field / Advanced Features");
            CHECK_COND(legacy->text() == wantLegacy,
                       "74h: Show Legacy UI not retranslated: " << legacy->text().toStdString());
            CHECK_COND(adv->text() == wantAdv,
                       "74h: G1 switch not retranslated: " << adv->text().toStdString());
            const QString tipEn = QStringLiteral(
                    "Show the old toolbar menus, Plot Wizard, Add Time Stamp and Scripting "
                    "Language in menus, the ribbon and command search");
            const QString wantTip = ja ? ToriGraphFullJapaneseTranslator::lookupJapanese(tipEn) : tipEn;
            CHECK_COND(!wantTip.isEmpty() && legacy->toolTip() == wantTip,
                       "74h: Show Legacy UI tooltip not retranslated: "
                               << legacy->toolTip().toStdString());
            return 0;
        };
        mw->switchToLanguage(startJa ? QStringLiteral("en") : QStringLiteral("ja"));
        if (checkLang(!startJa) != 0)
            return 1;
        mw->switchToLanguage(startJa ? QStringLiteral("ja") : QStringLiteral("en"));
        if (checkLang(startJa) != 0)
            return 1;
    }

    // ---- 74f: Japanese text of the switch
    CHECK_COND(ToriGraphFullJapaneseTranslator::lookupJapanese("Show Legacy UI")
                       == QString::fromUtf8("旧 UI を表示"),
               "74f: Japanese text of Show Legacy UI");

    mw->closeWindow(ml);
    qApp->setProperty("torigraph_noninteractive", QVariant());
    std::cout << "[INFO] Suite 74 legacy UI hidden passed!\n";
    return 0;
}

// Suite 71: lab-slide preset text size (18 pt on the slide) and Ctrl+wheel page zoom that makes
// the text follow the zoom on screen while the stored pt, project files and exports stay as at
// 100 %.
static QStringList suite71FontLines(Graph *g)
{
    // Font records of Graph::saveToString(): TitleFont / ScaleFontN / AxisFontN keep
    // family..strikeOut; <legend>/<text> keep the font fields only (family..strikeOut).
    QStringList out;
    const QStringList lines = g->saveToString().split(QLatin1Char('\n'));
    for (const QString &ln : lines) {
        if (ln.startsWith(QLatin1String("TitleFont")) || ln.startsWith(QLatin1String("ScaleFont"))
            || ln.startsWith(QLatin1String("AxisFont"))) {
            out << ln;
        } else if (ln.startsWith(QLatin1String("<legend>")) || ln.startsWith(QLatin1String("<text>"))) {
            const QStringList f = ln.split(QLatin1Char('\t'));
            out << (f.value(0) + QLatin1Char('|') + f.mid(3, 6).join(QLatin1Char('|')));
        }
    }
    return out;
}

static QStringList suite71SvgFontSizes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QStringList();
    const QString svg = QString::fromUtf8(f.readAll());
    QStringList sizes;
    QRegularExpression rx(QStringLiteral("font-size=\"([^\"]+)\""));
    auto it = rx.globalMatch(svg);
    while (it.hasNext())
        sizes << it.next().captured(1);
    sizes.sort();
    return sizes;
}

// lfHeight of every font the EMF creates (EMR_EXTCREATEFONTINDIRECTW = 82), sorted.
static QList<int> suite71EmfFontHeights(const QByteArray &emf)
{
    QList<int> h;
    int off = 0;
    while (off + 8 <= emf.size()) {
        quint32 type = 0, size = 0;
        memcpy(&type, emf.constData() + off, 4);
        memcpy(&size, emf.constData() + off + 4, 4);
        if (size < 8 || off + int(size) > emf.size())
            break;
        if (type == 82 && size >= 16) {
            qint32 lfHeight = 0;
            memcpy(&lfHeight, emf.constData() + off + 12, 4);
            h << int(lfHeight);
        }
        off += int(size);
    }
    std::sort(h.begin(), h.end());
    return h;
}

int runSuite71LabSlideTextAndZoomFollowsText(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 71: lab slide 18 pt text and page zoom with following text...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 71");
    qApp->setProperty("torigraph_noninteractive", true);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir tmp71;
    CHECK_COND(tmp71.isValid(), "71: QTemporaryDir failed");

    // ---- 71a: the lab slide preset reads 18 pt on the slide after the 140 % enlargement ----
    MultiLayer::GraphStylePresetSpec lab;
    CHECK_COND(MultiLayer::findStylePreset(QStringLiteral("lab_slide_83mm"), &lab), "71a: preset");
    std::cout << "[INFO] 71a lab slide: title " << lab.axisTitlePt << " pt, ticks " << lab.tickLabelPt
              << " pt, legend " << lab.legendPt << " pt\n";
    CHECK_COND(std::fabs(lab.axisTitlePt - 12.9) < 1e-9 && std::fabs(lab.tickLabelPt - 12.9) < 1e-9,
               "71a: axis titles / tick labels must be 12.9 pt (18 pt / 1.40)");
    CHECK_COND(std::fabs(lab.axisTitlePt * 1.40 - 18.0) < 0.1, "71a: 12.9 pt x 140 % must read 18 pt");
    // Legend follows the same ratio as the axis text over the former 10 pt (10 x 12.9 / 11.2).
    CHECK_COND(std::fabs(lab.legendPt - 11.5) < 1e-9 && std::fabs(lab.legendPt - 10.0 * 12.9 / 11.2) < 0.05,
               "71a: legend must be 11.5 pt");
    CHECK_COND(std::fabs(lab.pageWidthMm - 83.3) < 1e-9 && std::fabs(lab.pageHeightMm - 83.3) < 1e-9,
               "71a: page must stay 83.3 mm square");
    CHECK_COND(lab.name.contains(QStringLiteral("18 pt")), "71a: preset name must say 18 pt on the slide");

    ApplicationWindow *w71 = new ApplicationWindow();
    w71->applyUserSettings();
    MultiLayer::setNewGraphStylePresetId(QString());
    Table *t = w71->newTable(QStringLiteral("Zoom71"), 8, 3);
    CHECK_COND(t != nullptr, "71: table");
    t->setColName(0, "x");
    t->setColName(1, "y1");
    t->setColName(2, "y2");
    for (int r = 0; r < 8; ++r) {
        t->setCell(r, 0, r + 1.0);
        t->setCell(r, 1, (r + 1.0) * 2.0);
        t->setCell(r, 2, (r + 1.0) * 3.0 + 1.0);
    }
    MultiLayer *ml = w71->multilayerPlot(t, QStringList() << (t->name() + "_y1"), Graph::LineSymbols);
    CHECK_COND(ml && ml->activeGraph(), "71: graph");
    const QString mlName = ml->name();
    w71->show();
    ml->show();
    for (int k = 0; k < 5; ++k)
        QApplication::processEvents();
    CHECK_COND(ml->applyStylePresetById(QStringLiteral("lab_slide_83mm")), "71a: apply lab preset");
    QApplication::processEvents();
    Graph *g = ml->activeGraph();
    Plot *pl = g->plotWidget();
    CHECK_COND(g->legend() != nullptr, "71a: legend");
    auto storedPt = [](const QFont &f) { return f.pointSizeF(); };
    CHECK_COND(std::fabs(storedPt(g->axisTitleFont(QwtPlot::xBottom)) - 12.9) < 1e-6
                       && std::fabs(storedPt(g->axisFont(QwtPlot::xBottom)) - 12.9) < 1e-6
                       && std::fabs(storedPt(g->axisTitleFont(QwtPlot::yLeft)) - 12.9) < 1e-6,
               "71a: applied axis title / tick label pt must be 12.9");
    CHECK_COND(std::fabs(storedPt(g->legend()->font()) - 11.5) < 1e-6, "71a: applied legend pt must be 11.5");

    QwtPlot *base = pl; // QwtPlot members give the fonts the widgets actually draw with
    // Small pages open at a 160 % view: the text must already be drawn at that view zoom.
    std::cout << "[INFO] 71a view zoom after the preset: " << ml->pageZoom() << ", drawn title "
              << base->axisTitle(QwtPlot::xBottom).font().pointSizeF() << " pt\n";
    CHECK_COND(std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() - 12.9 * ml->pageZoom()) < 1e-6
                       && std::fabs(base->axisWidget(QwtPlot::xBottom)->font().pointSizeF()
                                    - 12.9 * ml->pageZoom()) < 1e-6,
               "71a: the preset's initial view zoom must also enlarge the drawn text");

    // Snapshot at 100 %.
    ml->setPageZoom(1.0);
    for (int k = 0; k < 3; ++k)
        QApplication::processEvents();
    CHECK_COND(std::fabs(ml->pageZoom() - 1.0) < 1e-9, "71: page must be at 100 %");
    const QFont titleStored = g->axisTitleFont(QwtPlot::xBottom);
    const QFont tickStored = g->axisFont(QwtPlot::xBottom);
    const QFont yTitleStored = g->axisTitleFont(QwtPlot::yLeft);
    const QFont legStored = g->legend()->font();
    const QFont plotTitleStored = pl->title().font();
    const double titleH1 = QFontMetricsF(base->axisTitle(QwtPlot::xBottom).font()).height();
    const double tickH1 = QFontMetricsF(base->axisWidget(QwtPlot::xBottom)->font()).height();
    const QRect legRect1 = g->legend()->rect();
    const int axisWidgetH1 = base->axisWidget(QwtPlot::xBottom)->height();
    const int layerH1 = g->height();
    const QStringList fontLines1 = suite71FontLines(g);
    CHECK_COND(fontLines1.size() >= 9, "71c: font records missing from saveToString");
    const QString svg1 = tmp71.filePath(QStringLiteral("zoom100.svg"));
    const QString png1 = tmp71.filePath(QStringLiteral("zoom100.png"));
    ml->exportSVG(svg1);
    ml->exportImage(png1, 100, false, 150);
    const QStringList svgSizes1 = suite71SvgFontSizes(svg1);
    const QImage img1(png1);
    CHECK_COND(!svgSizes1.isEmpty(), "71c: SVG at 100 % has no font sizes");
    CHECK_COND(!img1.isNull(), "71c: PNG at 100 % missing");
    // PowerPoint / clipboard paths: the OLE metafile and the page image.
    const OleRenderedGraph emf1 = SciDAVisEmf::renderMultiLayer(ml);
    const QImage page1 = ml->copyPageAsImage(300);
    CHECK_COND(!emf1.emfBytes.isEmpty() && !page1.isNull(), "71c: EMF / page image at 100 % missing");

    // ---- 71b: 200 % doubles the drawn text, the stored pt stay ----
    ml->setPageZoom(2.0);
    for (int k = 0; k < 3; ++k)
        QApplication::processEvents();
    CHECK_COND(std::fabs(ml->pageZoom() - 2.0) < 1e-9, "71b: zoom 2.0");
    const double titleH2 = QFontMetricsF(base->axisTitle(QwtPlot::xBottom).font()).height();
    const double tickH2 = QFontMetricsF(base->axisWidget(QwtPlot::xBottom)->font()).height();
    const QRect legRect2 = g->legend()->rect();
    const int axisWidgetH2 = base->axisWidget(QwtPlot::xBottom)->height();
    const int layerH2 = g->height();
    std::cout << "[INFO] 71b text height px at 100 % / 200 %: title " << titleH1 << " / " << titleH2
              << ", ticks " << tickH1 << " / " << tickH2 << ", legend box " << legRect1.height()
              << " / " << legRect2.height() << ", x-axis band " << axisWidgetH1 << " / "
              << axisWidgetH2 << " (layer " << layerH1 << " / " << layerH2 << ")\n";
    CHECK_COND(titleH2 / titleH1 > 1.85 && titleH2 / titleH1 < 2.15,
               "71b: axis title must be drawn about twice as tall at 200 %");
    CHECK_COND(tickH2 / tickH1 > 1.85 && tickH2 / tickH1 < 2.15,
               "71b: tick labels must be drawn about twice as tall at 200 %");
    CHECK_COND(double(legRect2.height()) / qMax(1, legRect1.height()) > 1.4,
               "71b: legend text must grow with the zoom");
    CHECK_COND(double(axisWidgetH2) / qMax(1, axisWidgetH1) > 1.5,
               "71b: the axis band (ticks + labels + title) must grow with the zoom");
    CHECK_COND(std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() - 25.8) < 1e-6,
               "71b: drawn axis title font must be 12.9 x 2 = 25.8 pt");
    CHECK_COND(std::fabs(storedPt(g->axisTitleFont(QwtPlot::xBottom)) - 12.9) < 1e-9
                       && std::fabs(storedPt(g->axisFont(QwtPlot::xBottom)) - 12.9) < 1e-9
                       && std::fabs(storedPt(g->axisTitleFont(QwtPlot::yLeft)) - 12.9) < 1e-9
                       && std::fabs(storedPt(pl->axisTitle(QwtPlot::xBottom).font()) - 12.9) < 1e-9,
               "71b: stored (dialog / inspector) pt must stay 12.9 at 200 %");
    CHECK_COND(std::fabs(storedPt(g->legend()->font()) - 11.5) < 1e-9, "71b: stored legend pt must stay 11.5");
    CHECK_COND(g->axisTitleFont(QwtPlot::xBottom) == titleStored && g->axisFont(QwtPlot::xBottom) == tickStored
                       && pl->title().font() == plotTitleStored,
               "71b: stored fonts must be unchanged at 200 %");

    // ---- 71c: save / export at 200 % equal 100 % ----
    CHECK_COND(suite71FontLines(g) == fontLines1, "71c: saveToString font records differ at 200 %");
    const QString svg2 = tmp71.filePath(QStringLiteral("zoom200.svg"));
    const QString png2 = tmp71.filePath(QStringLiteral("zoom200.png"));
    ml->exportSVG(svg2);
    ml->exportImage(png2, 100, false, 150);
    QApplication::processEvents();
    CHECK_COND(std::fabs(ml->pageZoom() - 2.0) < 1e-9, "71c: export must leave the view at 200 %");
    CHECK_COND(std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() - 25.8) < 1e-6,
               "71c: export must leave the drawn text at 200 %");
    const QStringList svgSizes2 = suite71SvgFontSizes(svg2);
    {
        QStringList uniq = svgSizes1;
        uniq.removeDuplicates();
        std::cout << "[INFO] 71c SVG: " << svgSizes1.size() << " / " << svgSizes2.size()
                  << " font-size attributes at 100 % / 200 %, values " << uniq.join(QLatin1Char(',')).toStdString()
                  << "\n";
    }
    CHECK_COND(svgSizes2 == svgSizes1, "71c: SVG exported at 200 % has different font sizes");
    const QImage img2(png2);
    CHECK_COND(!img2.isNull() && img2.size() == img1.size(), "71c: PNG exported at 200 % has a different size");
    {
        const QImage a = img1.convertToFormat(QImage::Format_ARGB32);
        const QImage b = img2.convertToFormat(QImage::Format_ARGB32);
        qint64 diff = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x)
                if (a.pixel(x, y) != b.pixel(x, y))
                    ++diff;
        const double frac = double(diff) / qMax<qint64>(1, qint64(a.width()) * a.height());
        std::cout << "[INFO] 71c PNG 100 % vs 200 %: " << a.width() << "x" << a.height()
                  << ", differing pixels " << frac * 100.0 << " %\n";
        CHECK_COND(frac < 0.01, "71c: PNG exported at 200 % differs from the 100 % export");
    }

    {
        const OleRenderedGraph emf2 = SciDAVisEmf::renderMultiLayer(ml);
        const QImage page2 = ml->copyPageAsImage(300);
        std::cout << "[INFO] 71c EMF bytes 100 % / 200 %: " << emf1.emfBytes.size() << " / "
                  << emf2.emfBytes.size() << ", extent " << emf1.extentHm.cx << "x" << emf1.extentHm.cy
                  << " / " << emf2.extentHm.cx << "x" << emf2.extentHm.cy << ", page image "
                  << page1.width() << "x" << page1.height() << " / " << page2.width() << "x"
                  << page2.height() << "\n";
        CHECK_COND(emf2.extentHm.cx == emf1.extentHm.cx && emf2.extentHm.cy == emf1.extentHm.cy,
                   "71c: OLE metafile size differs at 200 %");
        // The EMF bytes are not reproducible to the byte between two renders, so compare the fonts
        // it creates (lfHeight) and its size.
        const QList<int> fh1 = suite71EmfFontHeights(emf1.emfBytes);
        const QList<int> fh2 = suite71EmfFontHeights(emf2.emfBytes);
        QStringList fhs;
        for (int v : fh1)
            fhs << QString::number(v);
        fhs.removeDuplicates();
        std::cout << "[INFO] 71c EMF fonts: " << fh1.size() << " / " << fh2.size() << ", lfHeight "
                  << fhs.join(QLatin1Char(',')).toStdString() << "\n";
        CHECK_COND(!fh1.isEmpty() && fh2 == fh1, "71c: OLE metafile fonts at 200 % differ from 100 %");
        CHECK_COND(std::abs(emf2.emfBytes.size() - emf1.emfBytes.size()) <= emf1.emfBytes.size() / 100,
                   "71c: OLE metafile at 200 % is not the same drawing as at 100 %");
        CHECK_COND(page2.convertToFormat(QImage::Format_ARGB32) == page1.convertToFormat(QImage::Format_ARGB32),
                   "71c: copied page image at 200 % differs from 100 %");
        CHECK_COND(std::fabs(ml->pageZoom() - 2.0) < 1e-9
                           && std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() - 25.8) < 1e-6,
                   "71c: OLE / copy must leave the view at 200 %");
    }
    // Layer exports (File > Export Layer), the layer copy and Copy Page at 100 % vs 200 %.
    {
        auto pngDiffFrac = [](const QImage &x, const QImage &y) -> double {
            if (x.isNull() || y.isNull() || x.size() != y.size())
                return 1.0;
            const QImage a = x.convertToFormat(QImage::Format_ARGB32);
            const QImage b = y.convertToFormat(QImage::Format_ARGB32);
            qint64 diff = 0;
            for (int yy = 0; yy < a.height(); ++yy)
                for (int xx = 0; xx < a.width(); ++xx)
                    if (a.pixel(xx, yy) != b.pixel(xx, yy))
                        ++diff;
            return double(diff) / qMax<qint64>(1, qint64(a.width()) * a.height());
        };
        auto layerOutputs = [&](const QString &tag, QImage &png, QStringList &svgSizes, qint64 &pdfSize,
                                QImage &layerCopy, QImage &pageCopy) {
            const QString pngPath = tmp71.filePath(QStringLiteral("layer_%1.png").arg(tag));
            const QString svgPath = tmp71.filePath(QStringLiteral("layer_%1.svg").arg(tag));
            const QString pdfPath = tmp71.filePath(QStringLiteral("layer_%1.pdf").arg(tag));
            g->exportImage(pngPath, 100, false, 150);
            g->exportSVG(svgPath);
            g->exportVector(pdfPath);
            png = QImage(pngPath);
            svgSizes = suite71SvgFontSizes(svgPath);
            pdfSize = QFileInfo(pdfPath).size();
            QApplication::clipboard()->clear();
            g->copyImage();
            QApplication::processEvents();
            layerCopy = QApplication::clipboard()->image();
            QApplication::clipboard()->clear();
            ml->copyAllLayers();
            QApplication::processEvents();
            pageCopy = QApplication::clipboard()->image();
        };
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        QImage png1L, png2L, copy1L, copy2L, page1C, page2C;
        QStringList svg1L, svg2L;
        qint64 pdf1L = 0, pdf2L = 0;
        layerOutputs(QStringLiteral("100"), png1L, svg1L, pdf1L, copy1L, page1C);
        ml->setPageZoom(2.0);
        QApplication::processEvents();
        layerOutputs(QStringLiteral("200"), png2L, svg2L, pdf2L, copy2L, page2C);
        if (pngDiffFrac(copy1L, copy2L) > 0.01 || pngDiffFrac(page1C, page2C) > 0.01) {
            // Another process may have used the clipboard in between: take the copies once more.
            QImage dummyPng;
            QStringList dummySvg;
            qint64 dummyPdf = 0;
            ml->setPageZoom(1.0);
            QApplication::processEvents();
            layerOutputs(QStringLiteral("100b"), dummyPng, dummySvg, dummyPdf, copy1L, page1C);
            ml->setPageZoom(2.0);
            QApplication::processEvents();
            layerOutputs(QStringLiteral("200b"), dummyPng, dummySvg, dummyPdf, copy2L, page2C);
        }
        QStringList uniqL = svg1L;
        uniqL.removeDuplicates();
        std::cout << "[INFO] 71c layer export 100 % / 200 %: PNG " << png1L.width() << "x" << png1L.height()
                  << " / " << png2L.width() << "x" << png2L.height() << " (diff "
                  << pngDiffFrac(png1L, png2L) * 100.0 << " %), SVG font sizes "
                  << uniqL.join(QLatin1Char(',')).toStdString() << " (" << svg1L.size() << " / "
                  << svg2L.size() << "), PDF " << pdf1L << " / " << pdf2L << " bytes, layer copy "
                  << copy1L.width() << "x" << copy1L.height() << " / " << copy2L.width() << "x"
                  << copy2L.height() << " (diff " << pngDiffFrac(copy1L, copy2L) * 100.0
                  << " %), page copy " << page1C.width() << "x" << page1C.height() << " / "
                  << page2C.width() << "x" << page2C.height() << " (diff "
                  << pngDiffFrac(page1C, page2C) * 100.0 << " %)\n";
        CHECK_COND(!png1L.isNull() && pngDiffFrac(png1L, png2L) < 0.01,
                   "71c: layer PNG exported at 200 % differs from 100 %");
        CHECK_COND(!svg1L.isEmpty() && svg2L == svg1L, "71c: layer SVG font sizes differ at 200 %");
        CHECK_COND(svg1L.contains(QStringLiteral("17.2")) && !svg1L.contains(QStringLiteral("34.4")),
                   "71c: layer SVG must carry the 12.9 pt text (17.2 px), never the zoomed size");
        CHECK_COND(pdf1L > 0 && qAbs(pdf2L - pdf1L) <= qMax<qint64>(64, pdf1L / 100),
                   "71c: layer PDF exported at 200 % is not the same drawing as at 100 %");
        CHECK_COND(!copy1L.isNull() && pngDiffFrac(copy1L, copy2L) < 0.01,
                   "71c: copied layer image at 200 % differs from 100 %");
        if (!page1C.isNull() || !page2C.isNull()) {
            CHECK_COND(pngDiffFrac(page1C, page2C) < 0.01,
                       "71c: Copy Page (copyAllLayers) at 200 % differs from 100 %");
        } else {
            std::cout << "[INFO] 71c copyAllLayers left no bitmap on the clipboard (OLE object only)\n";
        }
        CHECK_COND(std::fabs(ml->pageZoom() - 2.0) < 1e-9
                           && std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() - 25.8) < 1e-6,
                   "71c: layer exports / copies must leave the view at 200 %");
    }

    // Edits while zoomed: new curve, axis title text, a new font size.
    CHECK_COND(g->insertCurve(t, t->name() + "_x", t->name() + "_y2", Graph::LineSymbols),
               "71: add a curve at 200 %");
    g->setAxisTitle(QwtPlot::xBottom, QStringLiteral("Strain, e / %"));
    QApplication::processEvents();
    CHECK_COND(g->curves() == 2, "71: two curves after adding at 200 %");
    CHECK_COND(std::fabs(storedPt(g->axisTitleFont(QwtPlot::xBottom)) - 12.9) < 1e-9
                       && std::fabs(storedPt(g->legend()->font()) - 11.5) < 1e-9,
               "71: adding a curve / editing a title at 200 % must keep the stored pt");
    CHECK_COND(std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() - 25.8) < 1e-6
                       && base->axisTitle(QwtPlot::xBottom).text() == QStringLiteral("Strain, e / %"),
               "71: edited axis title must still be drawn at 200 %");
    {
        QFont f14 = titleStored;
        f14.setPointSizeF(14.0);
        g->setAxisTitleFont(QwtPlot::yLeft, f14);
        CHECK_COND(std::fabs(storedPt(g->axisTitleFont(QwtPlot::yLeft)) - 14.0) < 1e-9
                           && std::fabs(base->axisTitle(QwtPlot::yLeft).font().pointSizeF() - 28.0) < 1e-6,
                   "71: a 14 pt font set at 200 % must be stored as 14 pt and drawn at 28 pt");
    }

    {
        g->setAxisTitleFont(QwtPlot::yLeft, yTitleStored);
        CHECK_COND(std::fabs(storedPt(g->axisTitleFont(QwtPlot::yLeft)) - 12.9) < 1e-9, "71: restore y title");
    }

    // ---- 71d: 2.0 -> 0.5 -> 1.0 returns to the exact original fonts ----
    ml->setPageZoom(0.5);
    QApplication::processEvents();
    CHECK_COND(std::fabs(base->axisWidget(QwtPlot::xBottom)->font().pointSizeF() - 6.45) < 1e-6,
               "71d: tick labels drawn at 6.45 pt at 50 %");
    ml->setPageZoom(1.0);
    QApplication::processEvents();
    CHECK_COND(g->axisTitleFont(QwtPlot::xBottom) == titleStored && g->axisFont(QwtPlot::xBottom) == tickStored
                       && g->legend()->font() == legStored && pl->title().font() == plotTitleStored,
               "71d: fonts after 2.0 -> 0.5 -> 1.0 differ from the originals");
    CHECK_COND(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() == 12.9
                       && base->axisWidget(QwtPlot::xBottom)->font().pointSizeF() == 12.9,
               "71d: drawn fonts at 100 % must be exactly the stored 12.9 pt");
    // Many small Ctrl+wheel steps (1.1x each) there and back must not drift either.
    for (int i = 0; i < 9; ++i) {
        QWheelEvent we(QPointF(20, 20), ml->mapToGlobal(QPoint(20, 20)), QPoint(), QPoint(0, 120),
                       Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(ml, &we);
    }
    QApplication::processEvents();
    std::cout << "[INFO] 71d after 9 Ctrl+wheel steps: zoom " << ml->pageZoom() << ", drawn title "
              << base->axisTitle(QwtPlot::xBottom).font().pointSizeF() << " pt\n";
    CHECK_COND(ml->pageZoom() > 2.3 && std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF()
                                                 - 12.9 * ml->pageZoom()) < 1e-6,
               "71d: Ctrl+wheel zoom must enlarge the drawn text by the zoom");
    ml->setPageZoom(4.0);
    ml->setPageZoom(0.25);
    QApplication::processEvents();
    CHECK_COND(std::fabs(base->axisTitle(QwtPlot::xBottom).font().pointSizeF() - 12.9 * 0.25) < 1e-6
                       && std::fabs(storedPt(g->axisTitleFont(QwtPlot::xBottom)) - 12.9) < 1e-9,
               "71d: 25 % must draw 3.225 pt and keep 12.9 pt stored");
    ml->setPageZoom(1.0);
    QApplication::processEvents();
    CHECK_COND(g->axisTitleFont(QwtPlot::xBottom) == titleStored
                       && base->axisTitle(QwtPlot::xBottom).font().pointSizeF() == 12.9
                       && g->legend()->font() == legStored,
               "71d: fonts after the wheel / 400 % / 25 % round trip differ from the originals");
    CHECK_COND(suite71FontLines(g) == fontLines1, "71d: saveToString font records changed after zooming");

    // Undo restored in place while the view zoom differs from the snapshot's zoom.
    {
        ml->setPageZoom(2.0);
        QApplication::processEvents();
        const QString snap = ml->captureUndoSnapshot();
        QFont f16 = titleStored;
        f16.setPointSizeF(16.0);
        g->setAxisTitleFont(QwtPlot::xBottom, f16);
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        CHECK_COND(ml->restoreSnapshotInPlace(w71, snap), "71: undo snapshot restore");
        QApplication::processEvents();
        Graph *gU = ml->activeGraph();
        CHECK_COND(gU != nullptr, "71: graph after undo restore");
        QwtPlot *baseU = gU->plotWidget();
        const double ptU = storedPt(gU->axisTitleFont(QwtPlot::xBottom));
        std::cout << "[INFO] 71 undo restore: zoom " << ml->pageZoom() << ", stored x title " << ptU
                  << " pt, drawn " << baseU->axisTitle(QwtPlot::xBottom).font().pointSizeF() << " pt\n";
        CHECK_COND(std::fabs(ptU - 12.9) < 1e-9, "71: undo must bring back exactly 12.9 pt (not 16, not 13)");
        CHECK_COND(std::fabs(storedPt(gU->axisFont(QwtPlot::xBottom)) - 12.9) < 1e-9
                           && std::fabs(storedPt(gU->axisTitleFont(QwtPlot::yLeft)) - 12.9) < 1e-9
                           && gU->legend() && std::fabs(storedPt(gU->legend()->font()) - 11.5) < 1e-9,
                   "71: undo must keep 12.9 pt ticks / titles and the 11.5 pt legend");
        CHECK_COND(std::fabs(baseU->axisTitle(QwtPlot::xBottom).font().pointSizeF() - ptU * ml->pageZoom()) < 1e-6,
                   "71: after undo the text must be drawn at the page zoom");
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        CHECK_COND(std::fabs(baseU->axisTitle(QwtPlot::xBottom).font().pointSizeF() - ptU) < 1e-9,
                   "71: after undo, 100 % must draw the stored pt");
        g = gU;
    }

    // Save and reload the project saved at 200 % (done last: creating another
    // ApplicationWindow resets the tick-label font of open graphs to 10 pt, also at 100 %).
    ml->setPageZoom(2.0);
    {
        QFont f14 = yTitleStored;
        f14.setPointSizeF(14.0);
        g->setAxisTitleFont(QwtPlot::yLeft, f14);
    }
    {
        const QByteArray prj = w71->saveProjectToByteArray();
        const QString prjPath = tmp71.filePath(QStringLiteral("suite71_zoom200.sciprj"));
        QFile pf(prjPath);
        CHECK_COND(pf.open(QIODevice::WriteOnly), "71: write project");
        pf.write(prj);
        pf.close();
        ApplicationWindow *reader = new ApplicationWindow();
        reader->applyUserSettings();
        CHECK_COND(reader->loadProject(prjPath), "71: reload project saved at 200 %");
        MultiLayer *mlR = qobject_cast<MultiLayer *>(reader->window(mlName));
        CHECK_COND(mlR && mlR->activeGraph(), "71: reloaded graph");
        for (int k = 0; k < 3; ++k)
            QApplication::processEvents();
        Graph *gR = mlR->activeGraph();
        QwtPlot *baseR = gR->plotWidget();
        const double ptR = storedPt(gR->axisTitleFont(QwtPlot::xBottom));
        const double shownR = baseR->axisTitle(QwtPlot::xBottom).font().pointSizeF();
        std::cout << "[INFO] 71 reload: zoom " << mlR->pageZoom() << ", stored x title " << ptR
                  << " pt, drawn " << shownR << " pt, y title " << storedPt(gR->axisTitleFont(QwtPlot::yLeft))
                  << " pt\n";
        // The exact pt travel in the project file next to the legacy integer field.
        CHECK_COND(std::fabs(ptR - 12.9) < 1e-9, "71: reloaded x title must be exactly 12.9 pt");
        CHECK_COND(std::fabs(storedPt(gR->axisFont(QwtPlot::xBottom)) - 12.9) < 1e-9
                           && gR->legend() && std::fabs(storedPt(gR->legend()->font()) - 11.5) < 1e-9,
                   "71: reloaded ticks must be 12.9 pt and the legend 11.5 pt");
        CHECK_COND(std::fabs(storedPt(gR->axisTitleFont(QwtPlot::yLeft)) - 14.0) < 1e-9,
                   "71: reloaded y title must be the 14 pt set while zoomed");
        CHECK_COND(gR->curves() == 2, "71: reloaded curves");
        CHECK_COND(std::fabs(mlR->pageZoom() - 2.0) < 1e-6
                           && std::fabs(shownR - ptR * mlR->pageZoom()) < 1e-6,
                   "71: reloaded page must draw the text at its saved zoom");
        mlR->setPageZoom(1.0);
        QApplication::processEvents();
        CHECK_COND(std::fabs(baseR->axisTitle(QwtPlot::xBottom).font().pointSizeF() - ptR) < 1e-9,
                   "71: reloaded page back at 100 % must draw the stored pt");
        reader->savedProject();
        reader->setOleEmbeddedMode(true);
        delete reader;
    }
    // A layer added while the page is zoomed draws its text at the page zoom too.
    {
        ml->setPageZoom(2.0);
        QApplication::processEvents();
        Graph *gNew = ml->addLayer();
        QApplication::processEvents();
        CHECK_COND(gNew && gNew->plotWidget(), "71: add a layer at 200 %");
        const double storedNew = storedPt(gNew->axisFont(QwtPlot::xBottom));
        const double drawnNew = static_cast<QwtPlot *>(gNew->plotWidget())->axisFont(QwtPlot::xBottom).pointSizeF();
        std::cout << "[INFO] 71 new layer at 200 %: stored ticks " << storedNew << " pt, drawn " << drawnNew
                  << " pt\n";
        CHECK_COND(std::fabs(gNew->plotWidget()->displayFontScale() - 2.0) < 1e-9
                           && std::fabs(drawnNew - storedNew * 2.0) < 1e-6,
                   "71: a layer added at 200 % must draw its text at 200 %");
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        CHECK_COND(std::fabs(static_cast<QwtPlot *>(gNew->plotWidget())->axisFont(QwtPlot::xBottom).pointSizeF()
                             - storedNew) < 1e-9,
                   "71: the new layer must be back to its stored pt at 100 %");
    }

    ml->askOnCloseEvent(false);
    w71->savedProject();
    w71->setOleEmbeddedMode(true);
    delete w71;
    QApplication::processEvents();
    std::cout << "[PASS] Suite 71 passed.\n";
    return 0;
}

