#include "ole_test_common.h"

// =========================================================================================
// Suite 83: data import robustness (review fixes)
//   83a SPEC + German decimal mark keeps "12.345" as 12.345 (RejectGroupSeparator)
//   83b an unclosed quote at EOF does not swallow the following lines
//   83c overwrite import with an empty file is refused and leaves the columns alone
//   83d swapped headers on overwrite / append rename to [B, A] (no "B 1")
//   83e XLSX data-start detection is linear (string ID column + text in the last row)
//   83f a line with a huge number of fields is refused instead of ending in bad_alloc
//   83g re-import from source keeps the numeric locale (also after save / reopen)
//   83h overwrite [A,B,C] with a [C,A] file gives [C,A] (surplus columns removed before rename)
//   83i size limit: long narrow files (also with one wide annotation line) are read; a width
//       reached by a few outlier lines is refused with its own message
// =========================================================================================
namespace {
bool suite83Write(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = (f.write(bytes) == bytes.size());
    f.close();
    return ok;
}

// Imports `path` with the ASCII filter, non-interactively; nullptr = refused / unparsable.
future::Table *suite83Import(const QString &path, const QString &sep, const QLocale &loc)
{
    AsciiTableImportFilter filter;
    filter.set_separator(sep);
    filter.set_first_row_names_columns(true);
    filter.set_trim_whitespace(true);
    filter.set_convert_to_numeric(true);
    filter.set_numeric_locale(loc);
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly))
        return nullptr;
    return static_cast<future::Table *>(filter.importAspect(in));
}

Table *suite83FindTable(ApplicationWindow *aw, const QString &name)
{
    for (MyWidget *w : aw->windowsList()) {
        if (w && w->name() == name)
            return qobject_cast<Table *>(w);
    }
    return nullptr;
}

XlsxImport::Cell suite83Text(const QString &s)
{
    XlsxImport::Cell c;
    c.type = XlsxImport::Cell::Text;
    c.text = s;
    return c;
}

XlsxImport::Cell suite83Num(double v)
{
    XlsxImport::Cell c;
    c.type = XlsxImport::Cell::Number;
    c.number = v;
    return c;
}
} // namespace

int runSuite83ImportRobustness(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 83: Import robustness (SPEC locale, unclosed quotes, empty "
                 "overwrite, header swap, XLSX O(N), field limit, re-import locale)...\n";

    struct EnvGuard83
    {
        QVariant prevNonInteractive;
        EnvGuard83() : prevNonInteractive(qApp->property("torigraph_noninteractive"))
        {
            qApp->setProperty("torigraph_noninteractive", true);
        }
        ~EnvGuard83() { qApp->setProperty("torigraph_noninteractive", prevNonInteractive); }
    } envGuard83;

    struct AppWinGuard83
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard83(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard83() { reset(); QApplication::processEvents(); }
        AppWinGuard83(const AppWinGuard83 &) = delete;
        AppWinGuard83 &operator=(const AppWinGuard83 &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
        }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    QTemporaryDir tmp83;
    CHECK_COND(tmp83.isValid(), "83: temp dir creation failed");

    AppWinGuard83 w83(new ApplicationWindow());
    w83->applyUserSettings();
    w83->confirmCloseTable = false;
    w83->confirmClosePlot2D = false;
    w83->confirmClosePlot3D = false;
    w83->confirmCloseFolder = false;
    w83->switchToLanguage("en");
    QCoreApplication::processEvents();

    const QLocale german(QLocale::German);

    // ---- 83a: SPEC scan file with a German numeric locale
    {
        const QString spec = tmp83.filePath("scan83.spec");
        CHECK_COND(suite83Write(spec, "#F scan83.spec\n#S 1 ascan th 0 1 2 0.1\n#N 2\n"
                                      "#L Theta  Counts\n12.345  100\n13.5  200\n"),
                   "83a: write");
        std::unique_ptr<future::Table> t(suite83Import(spec, QStringLiteral("\t"), german));
        CHECK_COND(t && t->columnCount() == 2 && t->rowCount() == 2, "83a: SPEC not parsed");
        Column *x = t->column(0);
        CHECK_COND(x->columnMode() == SciDAVis::ColumnMode::Numeric, "83a: SPEC X not numeric");
        CHECK_COND(std::fabs(x->valueAt(0) - 12.345) < 1e-12,
                   "83a: SPEC 12.345 read as " << x->valueAt(0)
                                               << " (German group separator accepted)");
        CHECK_COND(std::fabs(x->valueAt(1) - 13.5) < 1e-12 && t->column(1)->valueAt(1) == 200.0,
                   "83a: SPEC values");
    }

    // ---- 83b: an unclosed quote at EOF must not swallow every later line
    {
        const QString csv = tmp83.filePath("unclosed83.csv");
        CHECK_COND(suite83Write(csv, "x,y\n1,\"abc\n2,3\n4,5\n"), "83b: write");
        std::unique_ptr<future::Table> t(suite83Import(csv, QStringLiteral(","), QLocale::c()));
        CHECK_COND(t && t->columnCount() == 2, "83b: CSV not parsed");
        CHECK_COND(t->rowCount() == 3,
                   "83b: rows after the unclosed quote were swallowed (rows=" << t->rowCount()
                                                                               << ")");
        Column *x = t->column(0);
        CHECK_COND(x->columnMode() == SciDAVis::ColumnMode::Numeric && x->valueAt(1) == 2.0
                           && x->valueAt(2) == 4.0,
                   "83b: x values after the unclosed quote");
        // a properly closed multi-line field still spans lines
        const QString ok = tmp83.filePath("multiline83.csv");
        CHECK_COND(suite83Write(ok, "x,y\n1,\"a\nb\"\n2,c\n"), "83b: write 2");
        std::unique_ptr<future::Table> t2(suite83Import(ok, QStringLiteral(","), QLocale::c()));
        CHECK_COND(t2 && t2->rowCount() == 2 && t2->column(1)->textAt(0) == "a\nb",
                   "83b: closed multi-line quoted field broken");
    }

    // ---- 83c: overwrite with an empty file is refused, the worksheet keeps its columns
    {
        Table *t = w83->newTable("Emp83", 2, 2);
        CHECK_COND(t, "83c: newTable");
        t->setCell(0, 0, 1.0);
        t->setCell(0, 1, 2.0);
        t->setCell(1, 0, 3.0);
        t->setCell(1, 1, 4.0);
        const QList<QByteArray> empties = { QByteArray(), QByteArray("   \n\t\n\n"),
                                            QByteArray("# comment only\n# another\n") };
        int idx = 0;
        for (const QByteArray &bytes : empties) {
            const QString f = tmp83.filePath(QStringLiteral("empty83_%1.csv").arg(idx++));
            CHECK_COND(suite83Write(f, bytes), "83c: write");
            w83->d_workspace.setActiveSubWindow(t);
            w83->setProperty("torigraph_last_import_error", QVariant());
            w83->importASCII(QStringList() << f, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                             false, true, QLocale::c());
            CHECK_COND(t->numCols() == 2 && t->numRows() == 2,
                       "83c: empty file #" << idx << " changed the table to " << t->numCols()
                                           << " columns");
            CHECK_COND(t->cell(1, 0) == 3.0 && t->cell(1, 1) == 4.0,
                       "83c: empty file #" << idx << " changed the values");
            CHECK_COND(!w83->property("torigraph_last_import_error").toString().isEmpty(),
                       "83c: empty file #" << idx << " was not reported");
        }
    }

    // ---- 83d: swapped headers keep their names (two-step rename via temporary names)
    {
        Table *t = w83->newTable("Swap83", 2, 2);
        CHECK_COND(t, "83d: newTable");
        const QString fAB = tmp83.filePath("ab83.csv");
        const QString fBA = tmp83.filePath("ba83.csv");
        CHECK_COND(suite83Write(fAB, "A,B\n1,2\n3,4\n") && suite83Write(fBA, "B,A\n5,6\n7,8\n"),
                   "83d: write");
        w83->d_workspace.setActiveSubWindow(t);
        w83->importASCII(QStringList() << fAB, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                         false, true, QLocale::c());
        CHECK_COND(t->column(0)->name() == "A" && t->column(1)->name() == "B",
                   "83d: [A,B] precondition: " << t->column(0)->name().toStdString() << ","
                                               << t->column(1)->name().toStdString());
        w83->d_workspace.setActiveSubWindow(t);
        w83->importASCII(QStringList() << fBA, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                         false, true, QLocale::c());
        CHECK_COND(t->column(0)->name() == "B" && t->column(1)->name() == "A",
                   "83d: overwrite with [B,A] gave [" << t->column(0)->name().toStdString() << ","
                                                     << t->column(1)->name().toStdString()
                                                     << "]");
        CHECK_COND(t->cell(0, 0) == 5.0 && t->cell(1, 1) == 8.0, "83d: overwrite values");

        // append (new rows): [B,A] worksheet + file with [A,B] header -> [A,B]
        w83->d_workspace.setActiveSubWindow(t);
        w83->importASCII(QStringList() << fAB, ImportASCIIDialog::NewRows, ",", 0, true, true,
                         false, true, QLocale::c());
        CHECK_COND(t->numRows() == 4 && t->column(0)->name() == "A" && t->column(1)->name() == "B",
                   "83d: append with [A,B] gave [" << t->column(0)->name().toStdString() << ","
                                                   << t->column(1)->name().toStdString() << "]");
    }

    // ---- 83e: XLSX data-start detection stays linear
    {
        const int n = 100000;
        XlsxImport::Sheet sh;
        sh.name = QStringLiteral("Big");
        sh.columnCount = 2;
        sh.rows.reserve(n + 1);
        sh.rows.append(QVector<XlsxImport::Cell>() << suite83Text("ID") << suite83Text("Val"));
        for (int r = 1; r <= n; ++r) {
            sh.rows.append(QVector<XlsxImport::Cell>()
                           << suite83Text(QStringLiteral("S%1").arg(r))
                           << (r == n ? suite83Text("end") : suite83Num(r)));
        }
        QElapsedTimer timer;
        timer.start();
        XlsxImport::SheetLayout layout;
        QList<Column *> cols = XlsxImport::sheetToColumns(sh, false, &layout);
        const qint64 ms = timer.elapsed();
        const int nCols = cols.size();
        qDeleteAll(cols);
        CHECK_COND(nCols == 2 && layout.headerRow && layout.dataRows == n,
                   "83e: big sheet layout (cols=" << nCols << " rows=" << layout.dataRows << ")");
        CHECK_COND(ms < 5000, "83e: sheetToColumns took " << ms << " ms for " << n << " rows");

        // same data start as before for the preamble / mixed-header cases
        XlsxImport::Sheet pre;
        pre.columnCount = 2;
        pre.rows << (QVector<XlsxImport::Cell>() << suite83Text("Title"))
                 << (QVector<XlsxImport::Cell>() << suite83Text("x") << suite83Text("y"))
                 << (QVector<XlsxImport::Cell>() << suite83Num(1) << suite83Num(2))
                 << (QVector<XlsxImport::Cell>() << suite83Num(3) << suite83Num(4));
        XlsxImport::SheetLayout preLayout;
        QList<Column *> preCols = XlsxImport::sheetToColumns(pre, false, &preLayout);
        const bool preOk = preCols.size() == 2 && preLayout.dataRows == 2
                && preCols.at(0)->valueAt(0) == 1.0 && preCols.at(1)->valueAt(1) == 4.0;
        qDeleteAll(preCols);
        CHECK_COND(preOk, "83e: preamble sheet data start (rows=" << preLayout.dataRows << ")");

        XlsxImport::Sheet mixed;
        mixed.columnCount = 3;
        mixed.rows << (QVector<XlsxImport::Cell>() << suite83Text("Time (s)") << suite83Num(300)
                                                   << suite83Num(400))
                   << (QVector<XlsxImport::Cell>() << suite83Num(1) << suite83Num(2)
                                                   << suite83Num(3))
                   << (QVector<XlsxImport::Cell>() << suite83Num(4) << suite83Num(5)
                                                   << suite83Num(6));
        XlsxImport::SheetLayout mixLayout;
        QList<Column *> mixCols = XlsxImport::sheetToColumns(mixed, false, &mixLayout);
        const bool mixOk = mixCols.size() == 3 && mixLayout.dataRows == 2
                && mixCols.at(0)->valueAt(0) == 1.0 && mixCols.at(2)->valueAt(1) == 6.0;
        qDeleteAll(mixCols);
        CHECK_COND(mixOk, "83e: mixed header row swallowed as data (rows=" << mixLayout.dataRows
                                                                          << ")");
    }

    // ---- 83f: one line with a huge number of fields is refused, not allocated
    {
        QByteArray wide = "a,b\n1,2\n3,4\n1";
        for (int i = 1; i < 70000; ++i)
            wide += ",1";
        wide += "\n5,6\n";
        const QString fWide = tmp83.filePath("wide83.csv");
        CHECK_COND(suite83Write(fWide, wide), "83f: write wide");
        std::unique_ptr<future::Table> t(suite83Import(fWide, QStringLiteral(","), QLocale::c()));
        CHECK_COND(!t, "83f: 70000-field line was imported (" << (t ? t->columnCount() : 0)
                                                              << " columns)");
        CHECK_COND(AsciiTableImportFilter::lastImportReport().refusal.contains(
                           QStringLiteral("70000 fields")),
                   "83f: no column-limit reason: '"
                           << AsciiTableImportFilter::lastImportReport().refusal.toStdString()
                           << "'");

        // 20000 fields x 6000 rows = 1.2e8 cells (over the cell limit, under the column limit)
        QByteArray tall = "a,b\n1";
        for (int i = 1; i < 20000; ++i)
            tall += ",1";
        tall += "\n";
        for (int r = 0; r < 6000; ++r)
            tall += "1,2\n";
        const QString fTall = tmp83.filePath("tall83.csv");
        CHECK_COND(suite83Write(fTall, tall), "83f: write tall");
        std::unique_ptr<future::Table> t2(suite83Import(fTall, QStringLiteral(","), QLocale::c()));
        CHECK_COND(!t2, "83f: 1.2e8-cell import was not refused");
        CHECK_COND(AsciiTableImportFilter::lastImportReport().refusal.contains(
                           QStringLiteral("far more fields")),
                   "83f: no outlier reason: '"
                           << AsciiTableImportFilter::lastImportReport().refusal.toStdString()
                           << "'");

        // through the GUI path: error recorded, worksheet untouched
        Table *keep = w83->newTable("Wide83", 2, 2);
        keep->setCell(0, 0, 9.0);
        w83->d_workspace.setActiveSubWindow(keep);
        w83->setProperty("torigraph_last_import_error", QVariant());
        w83->importASCII(QStringList() << fWide, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                         false, true, QLocale::c());
        CHECK_COND(!w83->property("torigraph_last_import_error").toString().isEmpty()
                           && keep->numCols() == 2 && keep->cell(0, 0) == 9.0,
                   "83f: refused wide file not reported / changed the worksheet");
        CHECK_COND(w83->property("torigraph_last_import_error")
                           .toString()
                           .contains(QStringLiteral("at most 65536 columns")),
                   "83f: GUI error does not name the column limit: '"
                           << w83->property("torigraph_last_import_error").toString().toStdString()
                           << "'");
    }

    // ---- 83g: re-import from source keeps the German decimal mark
    {
        const QString f = tmp83.filePath("german83.txt");
        CHECK_COND(suite83Write(f, "x\ty\n1,5\t2,25\n3,5\t4,75\n"), "83g: write");
        Table *t = w83->newTable(f, "\t", 0, true, true, false, true, german);
        CHECK_COND(t && t->numCols() == 2 && t->numRows() == 2, "83g: German import");
        CHECK_COND(t->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && t->cell(0, 0) == 1.5 && t->cell(1, 1) == 4.75,
                   "83g: German values");
        CHECK_COND(t->d_future_table->importSourceNumericLocale() == german.name(),
                   "83g: numeric locale not recorded ("
                           << t->d_future_table->importSourceNumericLocale().toStdString() << ")");
        const QString tName = t->name();

        CHECK_COND(suite83Write(f, "x\ty\n10,5\t20,25\n30,5\t40,75\n"), "83g: rewrite");
        CHECK_COND(t->reimportFromSource(), "83g: reimportFromSource failed");
        QCoreApplication::processEvents();
        CHECK_COND(t->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && t->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric,
                   "83g: re-import turned the German numbers into text");
        CHECK_COND(t->cell(0, 0) == 10.5 && t->cell(1, 1) == 40.75, "83g: re-imported values");

        // project round trip: the key is written only for a non-C locale and read back
        const QString prj = tmp83.filePath("locale83.sciprj");
        CHECK_COND(w83->saveFolder(w83->projectFolder(), prj), "83g: save failed");
        QFile pf(prj);
        CHECK_COND(pf.open(QIODevice::ReadOnly), "83g: cannot read project");
        const QByteArray txt = pf.readAll();
        pf.close();
        const QByteArray key = "import_source_numeric_locale=\"" + german.name().toUtf8() + "\"";
        CHECK_COND(txt.count(key) == 1 && txt.count("import_source_numeric_locale") == 1,
                   "83g: numeric locale key written " << txt.count("import_source_numeric_locale")
                                                      << " time(s)");
        ApplicationWindow *re = w83->openProject(prj);
        CHECK_COND(re != nullptr, "83g: reopen failed");
        Table *rt = suite83FindTable(re, tName);
        const bool found = rt && rt->d_future_table
                && rt->d_future_table->importSourceNumericLocale() == german.name();
        bool reOk = false;
        if (found) {
            CHECK_COND(suite83Write(f, "x\ty\n7,25\t8,5\n"), "83g: rewrite 2");
            reOk = rt->reimportFromSource()
                    && rt->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                    && rt->cell(0, 0) == 7.25 && rt->cell(0, 1) == 8.5;
        }
        Table *other = suite83FindTable(re, QStringLiteral("Swap83"));
        const bool otherEmpty =
                other && other->d_future_table
                && other->d_future_table->importSourceNumericLocale().isEmpty();
        re->savedProject();
        re->setOleEmbeddedMode(true);
        delete re;
        QApplication::processEvents();
        CHECK_COND(found, "83g: numeric locale lost after save / reopen");
        CHECK_COND(reOk, "83g: re-import after reopening did not use the German decimal mark");
        CHECK_COND(otherEmpty, "83g: C-locale import got a numeric locale after reopening");
    }

    // ---- 83h: overwrite [A,B,C] with a [C,A] file: the surplus column "C" is removed before
    //      the rename, so the result is [C,A] and not [C 1,A]
    {
        const QString fABC = tmp83.filePath("abc83.csv");
        const QString fCA = tmp83.filePath("ca83.csv");
        CHECK_COND(suite83Write(fABC, "A,B,C\n1,2,3\n4,5,6\n")
                           && suite83Write(fCA, "C,A\n7,8\n9,10\n"),
                   "83h: write");
        auto names = [](Table *t) {
            QStringList n;
            for (int c = 0; c < t->numCols(); ++c)
                n << t->column(c)->name();
            return n.join(',').toStdString();
        };
        Table *t = w83->newTable("Drop83", 2, 2);
        CHECK_COND(t, "83h: newTable");
        w83->d_workspace.setActiveSubWindow(t);
        w83->importASCII(QStringList() << fABC, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                         false, true, QLocale::c());
        CHECK_COND(t->numCols() == 3 && t->column(2)->name() == "C",
                   "83h: [A,B,C] precondition: " << names(t));
        w83->d_workspace.setActiveSubWindow(t);
        w83->importASCII(QStringList() << fCA, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                         false, true, QLocale::c());
        CHECK_COND(t->numCols() == 2 && t->column(0)->name() == "C" && t->column(1)->name() == "A",
                   "83h: overwrite [A,B,C] with [C,A] gave [" << names(t) << "]");
        CHECK_COND(t->cell(0, 0) == 7.0 && t->cell(1, 1) == 10.0, "83h: overwrite values");

        // the same through Table::importASCII (scripting interface)
        Table *t2 = w83->newTable("Drop83b", 2, 2);
        CHECK_COND(t2, "83h: newTable 2");
        t2->importASCII(fABC, ",", 0, true, true, false, false);
        CHECK_COND(t2->numCols() == 3 && t2->column(2)->name() == "C",
                   "83h: Table::importASCII [A,B,C] precondition: " << names(t2));
        t2->importASCII(fCA, ",", 0, true, true, false, false);
        CHECK_COND(t2->numCols() == 2 && t2->column(0)->name() == "C"
                           && t2->column(1)->name() == "A",
                   "83h: Table::importASCII [A,B,C] -> [C,A] gave [" << names(t2) << "]");
    }

    // ---- 83i: size limits, exercised with small thresholds (same decision as the defaults,
    //      where the outlier test starts at 1e8 cells: e.g. 2 columns x 6e7 lines, or 3 columns
    //      x 1e7 lines plus one wide annotation line, must still be read)
    {
        struct LimitGuard83
        {
            ~LimitGuard83() { AsciiTableImportFilter::setImportSizeLimitsForTesting(0, 0, 0); }
        } limitGuard83;
        AsciiTableImportFilter::setImportSizeLimitsForTesting(0, 1000, 0);

        // 2 columns x 1000 lines = 2000 cells: over the outlier threshold, but no outlier
        QByteArray narrow = "x,y\n";
        for (int r = 0; r < 1000; ++r)
            narrow += QByteArray::number(r) + "," + QByteArray::number(2 * r) + "\n";
        const QString fNarrow = tmp83.filePath("narrow83.csv");
        CHECK_COND(suite83Write(fNarrow, narrow), "83i: write narrow");
        std::unique_ptr<future::Table> tn(suite83Import(fNarrow, QStringLiteral(","), QLocale::c()));
        CHECK_COND(tn && tn->columnCount() == 2 && tn->rowCount() == 1000,
                   "83i: long two-column file refused or truncated (refusal '"
                           << AsciiTableImportFilter::lastImportReport().refusal.toStdString()
                           << "')");

        // 3 columns x 1000 lines + one 11-field annotation line: 11 x 1001 cells, 11 < 16 x 3
        QByteArray annotated = "x,y,z\n";
        for (int r = 0; r < 500; ++r)
            annotated += QByteArray::number(r) + ",1,2\n";
        annotated += "1,2,3,4,5,6,7,8,9,10,11\n";
        for (int r = 500; r < 1000; ++r)
            annotated += QByteArray::number(r) + ",1,2\n";
        const QString fAnn = tmp83.filePath("annotated83.csv");
        CHECK_COND(suite83Write(fAnn, annotated), "83i: write annotated");
        std::unique_ptr<future::Table> ta(suite83Import(fAnn, QStringLiteral(","), QLocale::c()));
        CHECK_COND(ta && ta->columnCount() == 11 && ta->rowCount() == 1001
                           && ta->column(0)->valueAt(1000) == 999.0,
                   "83i: narrow file with one wide line refused or changed (refusal '"
                           << AsciiTableImportFilter::lastImportReport().refusal.toStdString()
                           << "')");

        // 2 columns x 1000 lines + one 100-field line (< 1 % of the lines, > 16 x 2): refused
        QByteArray outlier = "x,y\n1";
        for (int i = 1; i < 100; ++i)
            outlier += ",1";
        outlier += "\n";
        for (int r = 0; r < 1000; ++r)
            outlier += "1,2\n";
        const QString fOut = tmp83.filePath("outlier83.csv");
        CHECK_COND(suite83Write(fOut, outlier), "83i: write outlier");
        std::unique_ptr<future::Table> to(suite83Import(fOut, QStringLiteral(","), QLocale::c()));
        const QString reason = AsciiTableImportFilter::lastImportReport().refusal;
        CHECK_COND(!to, "83i: outlier width was imported (" << (to ? to->columnCount() : 0)
                                                            << " columns)");
        CHECK_COND(reason.contains(QStringLiteral("far more fields"))
                           && reason.contains(QStringLiteral("up to 100"))
                           && reason.contains(QStringLiteral("typically 2")),
                   "83i: outlier reason '" << reason.toStdString() << "'");

        // GUI paths report that reason instead of "could not parse"
        Table *keep = w83->newTable("Out83", 2, 2);
        keep->setCell(0, 0, 9.0);
        w83->d_workspace.setActiveSubWindow(keep);
        w83->setProperty("torigraph_last_import_error", QVariant());
        w83->importASCII(QStringList() << fOut, ImportASCIIDialog::Overwrite, ",", 0, true, true,
                         false, true, QLocale::c());
        const QString guiErr = w83->property("torigraph_last_import_error").toString();
        CHECK_COND(guiErr.contains(QStringLiteral("far more fields"))
                           && !guiErr.contains(QStringLiteral("Could not parse any columns"))
                           && keep->numCols() == 2 && keep->cell(0, 0) == 9.0,
                   "83i: overwrite error '" << guiErr.toStdString() << "'");
        w83->setProperty("torigraph_last_import_error", QVariant());
        const int windowsBefore = w83->windowsList().size();
        w83->importASCII(QStringList() << fOut, ImportASCIIDialog::NewTables, ",", 0, true, true,
                         false, true, QLocale::c());
        const QString newErr = w83->property("torigraph_last_import_error").toString();
        CHECK_COND(newErr.contains(QStringLiteral("far more fields"))
                           && w83->windowsList().size() == windowsBefore,
                   "83i: new-table error '" << newErr.toStdString() << "'");

        // absolute bound
        AsciiTableImportFilter::setImportSizeLimitsForTesting(0, 0, 1500);
        std::unique_ptr<future::Table> th(suite83Import(fNarrow, QStringLiteral(","), QLocale::c()));
        CHECK_COND(!th
                           && AsciiTableImportFilter::lastImportReport().refusal.contains(
                                   QStringLiteral("too large")),
                   "83i: absolute cell bound not applied ('"
                           << AsciiTableImportFilter::lastImportReport().refusal.toStdString()
                           << "')");

        // defaults restored: the annotated file reads, no refusal left behind
        AsciiTableImportFilter::setImportSizeLimitsForTesting(0, 0, 0);
        std::unique_ptr<future::Table> td(suite83Import(fAnn, QStringLiteral(","), QLocale::c()));
        CHECK_COND(td && td->columnCount() == 11
                           && AsciiTableImportFilter::lastImportReport().refusal.isEmpty(),
                   "83i: default limits");
    }

    std::cout << "[INFO] Suite 83 Import Robustness passed!\n";
    return 0;
}
