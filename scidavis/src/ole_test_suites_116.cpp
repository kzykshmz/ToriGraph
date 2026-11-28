// Suite 116: regression checks from the 2026-11-28 review / debug campaign.
//   116a Project load limits: a <row index="2147483647"> (Column::load), a table rows="2147483647"
//        (future::Table::load, also through a whole .sciprj), a 1e5 x 1e5 matrix
//        (future::Matrix::load) and legacy (0.1.x) table / matrix headers and data row indices
//        are refused cleanly instead of overflowing or allocating gigabytes.
//   116b ImportOPJU: parse-wide cell budget for CPYUA datasets and for ZIP columns sharing one
//        stream; the FPC predictor tables are allocated once per payload (thousands of tiny
//        FPC runs used to allocate and zero 16 MiB each) and still start zeroed for every run.
//   116c ASCII import: SPEC / Rigaku .ras files with one run-together line are refused like a
//        plain table; alternating blank cells (CSV, SPEC, .ras, .xlsx) are marked in linear
//        time; the .ras scan units are found once, not once per *RAS_INT_START block.
#include "ole_test_common.h"
#include "ImportOPJU.h"
#include "XlsxImport.h"
#include "future/lib/XmlStreamReader.h"
#include "future/matrix/future_Matrix.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"

#include <QBuffer>
#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QXmlStreamWriter>

#include <cmath>
#include <cstring>
#include <memory>

// ImportOPJU.cpp (regression-test hook, not part of ImportOPJU.h): cells one .opju import may
// decode in all; <= 0 restores the default.
void torigraphSetOpjuParseCellBudgetForTesting(qint64 cells);

namespace {

#define CHECK116(cond, msg) CHECK_COND(cond, "Suite 116 " << msg)

struct AppWinGuard116
{
    ApplicationWindow *win = nullptr;
    explicit AppWinGuard116(ApplicationWindow *w = nullptr) : win(w) { configure(win); }
    static void configure(ApplicationWindow *w)
    {
        if (!w)
            return;
        w->setHeadlessMode(true);
        w->confirmCloseTable = false;
        w->confirmCloseMatrix = false;
        w->confirmClosePlot2D = false;
        w->confirmClosePlot3D = false;
        w->confirmCloseFolder = false;
        w->confirmCloseNotes = false;
    }
    ~AppWinGuard116() { reset(nullptr); }
    void reset(ApplicationWindow *w)
    {
        if (win) {
            win->savedProject();
            win->setOleEmbeddedMode(true);
            delete win;
        }
        win = w;
        configure(win);
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    ApplicationWindow *operator->() const { return win; }
    ApplicationWindow *get() const { return win; }
};

struct NonInteractiveGuard116
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard116()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard116()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AsciiLimitsGuard116
{
    ~AsciiLimitsGuard116() { AsciiTableImportFilter::setImportSizeLimitsForTesting(0, 0, 0); }
};

struct OpjuBudgetGuard116
{
    ~OpjuBudgetGuard116() { torigraphSetOpjuParseCellBudgetForTesting(0); }
};

bool write116(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(bytes) == bytes.size();
}

QString columnXml116(const Column *col)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    col->save(&writer);
    return xml;
}

bool loadColumn116(const QString &xml, SciDAVis::ColumnMode mode, QString *err)
{
    std::unique_ptr<Column> col(new Column(QStringLiteral("c116_loaded"), mode));
    XmlStreamReader reader(xml);
    while (!reader.atEnd() && !(reader.isStartElement() && reader.name() == "column"))
        reader.readNext();
    const bool ok = reader.isStartElement() && col->load(&reader);
    if (err)
        *err = reader.errorString();
    return ok && col->rowCount() < 1000;
}

//! Replaces the first `attr="..."` of the opening tag of xml.
QString withAttr116(QString xml, const QString &attr, const QString &value)
{
    const QRegularExpression re(QStringLiteral("\\b%1=\"[^\"]*\"").arg(attr));
    const QRegularExpressionMatch m = re.match(xml);
    if (m.hasMatch())
        xml.replace(m.capturedStart(), m.capturedLength(),
                    QStringLiteral("%1=\"%2\"").arg(attr, value));
    return xml;
}

// ---- CPYUA builders (same container layout as Suite 80) -----------------------------------
QByteArray encodeZeroRle116(const QByteArray &raw)
{
    QByteArray out;
    int i = 0;
    while (i < raw.size()) {
        int run = 0;
        if (raw[i] == '\0') {
            while (i + run < raw.size() && raw[i + run] == '\0' && run < 127)
                ++run;
            out.append(static_cast<char>(0x80U | static_cast<unsigned char>(run)));
        } else {
            while (i + run < raw.size() && raw[i + run] != '\0' && run < 127)
                ++run;
            out.append(static_cast<char>(static_cast<unsigned char>(run)));
            out.append(raw.constData() + i, run);
        }
        i += run;
    }
    return out;
}

void appendLE116(QByteArray &buf, quint64 v, int bytes)
{
    for (int b = 0; b < bytes; ++b)
        buf.append(static_cast<char>((v >> (8 * b)) & 0xFFU));
}

void appendLeb116(QByteArray &buf, quint64 v)
{
    do {
        unsigned char b = static_cast<unsigned char>(v & 0x7FU);
        v >>= 7;
        if (v)
            b |= 0x80U;
        buf.append(static_cast<char>(b));
    } while (v);
}

quint64 doubleBits116(double v)
{
    quint64 bits = 0;
    std::memcpy(&bits, &v, sizeof(double));
    return bits;
}

//! 0x05 numeric payload: <0x05> <totalRows> 0xFFFF <cnt> <kind> <body> 0xFACE
QByteArray numericPayload116(quint64 rows, const QByteArray &body)
{
    QByteArray p;
    p.append('\x05');
    appendLeb116(p, rows);
    p.append("\xFF\xFF", 2);
    appendLeb116(p, rows);
    p.append('\x00');
    p.append(body);
    p.append("\xCE\xFA", 2);
    return p;
}

//! `count` copies of `value` in one run tag
QByteArray constRunPayload116(quint64 count, double value)
{
    QByteArray body;
    appendLeb116(body, count << 1);
    body.append('\x50');
    appendLE116(body, doubleBits116(value), 8);
    return numericPayload116(count, body);
}

QByteArray cpyuaFile116(const QByteArray &book, const QList<QPair<QByteArray, QByteArray>> &sets)
{
    QByteArray buf("CPYUA  2024\0", 12);
    buf.append(QByteArray(52, '\0'));
    buf.append("\x7c\x00\x00\x00\x0a", 5);
    buf.append(QByteArray(0x7C + 1 + 5, '\0'));
    for (const auto &ds : sets) {
        const QByteArray fullName = book + '_' + ds.first;
        QByteArray hdr(114, '\0');
        hdr[2] = '\x01';
        hdr[3] = '\xFA';
        const int nameOff = hdr.size() - 53;
        for (int i = 0; i < std::min(fullName.size(), 21); ++i)
            hdr[nameOff + i] = fullName[i];
        const QByteArray rleHdr = encodeZeroRle116(hdr);
        buf.append('\x80');
        buf.append(static_cast<char>(rleHdr.size()));
        buf.append(rleHdr);
        buf.append('\x18');
        appendLE116(buf, static_cast<quint64>(ds.second.size()), 8);
        appendLE116(buf, static_cast<quint64>(ds.second.size()), 8);
        buf.append(QByteArray(8, '\0'));
        buf.append('\x0A');
        buf.append(ds.second);
        appendLE116(buf, 0U, 4);
    }
    buf.append("\x00\x00\x00\x00\x00\x0A", 6);

    QByteArray winRaw(540, '\0');
    for (int i = 0; i < book.size() && i < 20; ++i)
        winRaw[2 + i] = book[i];
    const char origin[] = "Origin";
    for (int i = 0; i < 6; ++i)
        winRaw[57 + i] = origin[i];
    for (int k = 120; k < 210; ++k)
        winRaw[k] = static_cast<char>('A' + (k % 26));
    const QByteArray rleWin = encodeZeroRle116(winRaw);
    buf.append('\x80');
    buf.append(static_cast<char>(rleWin.size()));
    buf.append(rleWin);
    for (int i = 0; i < sets.size(); ++i) {
        QByteArray colRaw(468, '\0');
        colRaw[0] = '\x10';
        colRaw[1] = static_cast<char>((i + 1) & 0xFF);
        colRaw[8] = static_cast<char>(i == 0 ? 0x03 : 0x00); // X, then Y
        colRaw[9] = sets.at(i).first.isEmpty() ? 'A' : sets.at(i).first.at(0);
        for (int k = 300; k < 325; ++k)
            colRaw[k] = 'P';
        QByteArray rleCol;
        rleCol.append('\x01');
        rleCol.append('\x10');
        rleCol.append(encodeZeroRle116(colRaw.mid(1)));
        buf.append('\x80');
        buf.append(static_cast<char>(rleCol.size()));
        buf.append(rleCol);
    }
    return buf;
}

Table *findTable116(ApplicationWindow *app, const QString &part)
{
    for (MyWidget *w : app->windowsList()) {
        if (auto *t = qobject_cast<Table *>(w)) {
            if (t->name().contains(part))
                return t;
        }
    }
    return nullptr;
}

XlsxImport::Cell xlsxNum116(double v, bool isDate = false)
{
    XlsxImport::Cell c;
    c.type = XlsxImport::Cell::Number;
    c.number = v;
    c.isDate = isDate;
    c.hasDatePart = isDate;
    return c;
}

XlsxImport::Cell xlsxText116(const QString &s)
{
    XlsxImport::Cell c;
    c.type = XlsxImport::Cell::Text;
    c.text = s;
    return c;
}

} // namespace

int runSuite116ImportAndProjectLoadLimits(ApplicationWindow *mw)
{
    std::cout << "[INFO] Suite 116: runSuite116ImportAndProjectLoadLimits\n";
    Q_UNUSED(mw);
    NonInteractiveGuard116 nonInteractive;
    QTemporaryDir tmp(QDir::tempPath() + QStringLiteral("/tg116_XXXXXX"));
    CHECK116(tmp.isValid(), "116: temporary directory");
    const qint64 fastMs = 4000 * kTestTimingScale;

    // =========================================================================================
    // 116a: project load limits
    // =========================================================================================
    {
        AppWinGuard116 w(new ApplicationWindow());

        // 1. <row index> of a column (double and text): INT_MAX overflowed row + 1 (out-of-bounds
        //    write / endless loop), 1e9 ended in bad_alloc.
        for (const SciDAVis::ColumnMode mode :
             { SciDAVis::ColumnMode::Numeric, SciDAVis::ColumnMode::Text }) {
            Column src(QStringLiteral("c116"), mode);
            if (mode == SciDAVis::ColumnMode::Numeric) {
                src.setValueAt(0, 1.0);
                src.setValueAt(1, 2.0);
                src.setValueAt(2, 3.0);
            } else {
                src.setTextAt(0, QStringLiteral("a"));
                src.setTextAt(1, QStringLiteral("b"));
                src.setTextAt(2, QStringLiteral("c"));
            }
            const QString xml = columnXml116(&src);
            CHECK116(xml.contains(QLatin1String("index=\"2\"")), "116a: column XML has no row 2");
            QString err;
            CHECK116(loadColumn116(xml, mode, &err),
                     "116a: valid column did not load: " << err.toStdString());
            for (const char *bad : { "2147483647", "2147483646", "1000000000", "67108864" }) {
                QString badXml = xml;
                badXml.replace(QLatin1String("index=\"2\""),
                               QStringLiteral("index=\"%1\"").arg(QLatin1String(bad)));
                QElapsedTimer t;
                t.start();
                CHECK116(!loadColumn116(badXml, mode, &err),
                         "116a: column with <row index=\"" << bad << "\"> was accepted");
                CHECK116(err.contains(QLatin1String("out of range")),
                         "116a: row index " << bad << " error: " << err.toStdString());
                CHECK116(t.elapsed() < fastMs,
                         "116a: rejecting row index " << bad << " took " << t.elapsed() << " ms");
            }
            // a sparse but sane index still loads (rows in between are empty)
            QString sparse = xml;
            sparse.replace(QLatin1String("index=\"2\""), QLatin1String("index=\"500\""));
            {
                std::unique_ptr<Column> col(new Column(QStringLiteral("c116_sparse"), mode));
                XmlStreamReader reader(sparse);
                while (!reader.atEnd() && !(reader.isStartElement() && reader.name() == "column"))
                    reader.readNext();
                CHECK116(col->load(&reader) && col->rowCount() == 501 && col->isInvalid(250)
                                 && !col->isInvalid(500),
                         "116a: sparse row index 500 not loaded as before");
            }
        }

        // 2. future::Table / future::Matrix dimensions
        {
            Table *t = w->newTable(QStringLiteral("Tbl116"), 3, 2);
            CHECK116(t && t->d_future_table, "116a: newTable");
            QString xml;
            {
                QXmlStreamWriter writer(&xml);
                t->d_future_table->save(&writer);
            }
            CHECK116(xml.contains(QLatin1String("rows=\"3\"")), "116a: table XML rows attribute");
            const QList<QPair<QString, QString>> badDims = {
                { QStringLiteral("rows"), QStringLiteral("2147483647") },
                { QStringLiteral("rows"), QStringLiteral("100000000") },
                { QStringLiteral("rows"), QStringLiteral("-1") },
                { QStringLiteral("columns"), QStringLiteral("-5") },
                { QStringLiteral("columns"), QStringLiteral("1000000") },
            };
            for (const auto &bd : badDims) {
                Table *t2 = w->newTable(QStringLiteral("Tbl116b"), 1, 1);
                XmlStreamReader reader(withAttr116(xml, bd.first, bd.second));
                while (!reader.atEnd() && !reader.isStartElement())
                    reader.readNext();
                QElapsedTimer timer;
                timer.start();
                const bool ok = t2->d_future_table->load(&reader);
                CHECK116(!ok && t2->d_future_table->rowCount() <= 3,
                         "116a: table " << bd.first.toStdString() << "=" << bd.second.toStdString()
                                        << " accepted (rows " << t2->d_future_table->rowCount()
                                        << ")");
                CHECK116(timer.elapsed() < fastMs,
                         "116a: table dimension check took " << timer.elapsed() << " ms");
                w->closeWindow(t2);
            }
            {
                Table *t3 = w->newTable(QStringLiteral("Tbl116c"), 1, 1);
                XmlStreamReader reader(xml);
                while (!reader.atEnd() && !reader.isStartElement())
                    reader.readNext();
                CHECK116(t3->d_future_table->load(&reader) && t3->d_future_table->rowCount() == 3
                                 && t3->d_future_table->columnCount() == 2,
                         "116a: valid table XML did not load");
                w->closeWindow(t3);
            }

            Matrix *m = w->newMatrix(QStringLiteral("Mat116"), 2, 2);
            CHECK116(m && m->d_future_matrix, "116a: newMatrix");
            QString mxml;
            {
                QXmlStreamWriter writer(&mxml);
                m->d_future_matrix->save(&writer);
            }
            QString hugeM =
                    withAttr116(withAttr116(mxml, QStringLiteral("rows"), QStringLiteral("100000")),
                                QStringLiteral("columns"), QStringLiteral("100000"));
            Matrix *m2 = w->newMatrix(QStringLiteral("Mat116b"), 1, 1);
            XmlStreamReader mreader(hugeM);
            while (!mreader.atEnd() && !mreader.isStartElement())
                mreader.readNext();
            QElapsedTimer timer;
            timer.start();
            CHECK116(!m2->d_future_matrix->load(&mreader)
                             && qint64(m2->d_future_matrix->rowCount())
                                             * m2->d_future_matrix->columnCount()
                                     <= 4,
                     "116a: 100000 x 100000 matrix accepted");
            CHECK116(timer.elapsed() < fastMs,
                     "116a: matrix dimension check took " << timer.elapsed() << " ms");
        }

        // 3. Whole projects: XML table rows="2147483647" (fuzz: 3dplot_rows_huge.sciprj crashed
        //    in QHeaderView) and legacy 0.1.x headers / data rows.
        {
            Table *t = w->newTable(QStringLiteral("Prj116"), 3, 2);
            for (int r = 0; r < 3; ++r)
                t->setCell(r, 0, r + 1.0);
            QString xml;
            {
                QXmlStreamWriter writer(&xml);
                t->d_future_table->save(&writer);
            }
            const QString hugeXml =
                    withAttr116(xml, QStringLiteral("rows"), QStringLiteral("2147483647"));
            QByteArray prj = "SciDAVis 1.21.0 project file\n<scripting-lang>\tmuParser\n"
                             "<windows>\t1\n<table>\n";
            prj += QByteArray::number(hugeXml.length()) + "\n" + hugeXml.toUtf8()
                    + "\ngeometry\t0\t0\t400\t300\t\n</table>\n";
            const QString path = tmp.filePath(QStringLiteral("rows_huge116.sciprj"));
            CHECK116(write116(path, prj), "116a: write rows_huge116.sciprj");
            QElapsedTimer timer;
            timer.start();
            AppWinGuard116 opened(w->openProject(path));
            CHECK116(timer.elapsed() < 3 * fastMs,
                     "116a: project with rows=2147483647 took " << timer.elapsed() << " ms");
            if (opened.get()) {
                for (MyWidget *mw2 : opened->windowsList()) {
                    if (auto *tt = qobject_cast<Table *>(mw2))
                        CHECK116(tt->numRows() <= 1000,
                                 "116a: project table got " << tt->numRows() << " rows");
                }
            }
        }
        const QList<QPair<QString, QByteArray>> legacy = {
            { QStringLiteral("legacy_rows116"),
              "<table>\nT116a\t2000000000\t2\t2024-01-01\n<data>\n0\t1\t2\n</data>\n</table>\n" },
            { QStringLiteral("legacy_cols116"),
              "<table>\nT116b\t3\t-4\t2024-01-01\n<data>\n0\t1\t2\n</data>\n</table>\n" },
            { QStringLiteral("legacy_index116"),
              "<table>\nT116c\t3\t2\t2024-01-01\n<data>\n0\t1\t2\n2147483647\t5\t6\n"
              "</data>\n</table>\n" },
            { QStringLiteral("legacy_matrix116"),
              "<matrix>\nM116\t100000\t100000\t2024-01-01\n</matrix>\n" },
        };
        for (const auto &lc : legacy) {
            const QString path = tmp.filePath(lc.first + QStringLiteral(".sciprj"));
            CHECK116(write116(path,
                              QByteArray("SciDAVis 0.1.4 project file\n<scripting-lang>\tmuParser\n"
                                         "<windows>\t1\n")
                                      + lc.second),
                     "116a: write " << lc.first.toStdString());
            QElapsedTimer timer;
            timer.start();
            AppWinGuard116 opened(w->openProject(path));
            CHECK116(timer.elapsed() < 3 * fastMs,
                     "116a: " << lc.first.toStdString() << " took " << timer.elapsed() << " ms");
            CHECK116(opened.get() == nullptr,
                     "116a: " << lc.first.toStdString() << " was opened instead of refused");
        }
        {
            // a sane legacy table still opens with its values
            const QString path = tmp.filePath(QStringLiteral("legacy_ok116.sciprj"));
            CHECK116(write116(path,
                              "SciDAVis 0.1.4 project file\n<scripting-lang>\tmuParser\n"
                              "<windows>\t1\n<table>\nT116ok\t3\t2\t2024-01-01\n<data>\n"
                              "0\t1\t2\n2\t5\t6\n</data>\n</table>\n"),
                     "116a: write legacy_ok116");
            AppWinGuard116 opened(w->openProject(path));
            CHECK116(opened.get() != nullptr, "116a: sane legacy table project not opened");
            Table *ok = findTable116(opened.get(), QStringLiteral("T116ok"));
            CHECK116(ok && ok->numRows() == 3 && std::fabs(ok->cell(2, 1) - 6.0) < 1e-12,
                     "116a: sane legacy table values");
        }

        // core guard: setValueAt / setTextAt at INT_MAX no longer overflow row + 1
        {
            Column c(QStringLiteral("cmax116"), SciDAVis::ColumnMode::Numeric);
            c.setValueAt(std::numeric_limits<int>::max(), 1.0);
            CHECK116(c.rowCount() == 0, "116a: setValueAt(INT_MAX) changed the column");
            Column s(QStringLiteral("smax116"), SciDAVis::ColumnMode::Text);
            s.setTextAt(std::numeric_limits<int>::max(), QStringLiteral("x"));
            CHECK116(s.rowCount() == 0, "116a: setTextAt(INT_MAX) changed the column");
        }
    }

    // =========================================================================================
    // 116b: ImportOPJU
    // =========================================================================================
    {
        OpjuBudgetGuard116 budgetGuard;

        // 1. FPC: semantics (tables zeroed for every run) and one allocation per payload.
        {
            // run A: 1.5 (8 literal bytes), then a 0-byte value (predicted from an untouched
            // slot = 0); run B: a 0-byte value: 0 when the tables start zeroed (1.5 if run A's
            // table entries leaked into it), then 0.
            QByteArray semBody;
            semBody.append('\x03'); // rc = 2
            semBody.append('\x14'); // preds = 20
            semBody.append('\x07'); // nibble 7: 8 bytes (FCM), nibble 0: 0 bytes
            appendLE116(semBody, doubleBits116(1.5), 8);
            semBody.append("\x03\x14\x00", 3);
            // 20000 runs of 2 values with preds = 20: each allocated and zeroed two 8 MiB tables
            QByteArray manyBody;
            for (int i = 0; i < 20000; ++i)
                manyBody.append("\x03\x14\x00", 3);
            const QByteArray file =
                    cpyuaFile116("Fpc116",
                                 { { QByteArray("A"), numericPayload116(4, semBody) },
                                   { QByteArray("B"), numericPayload116(40000, manyBody) } });
            const QString path = tmp.filePath(QStringLiteral("fpc116.opju"));
            CHECK116(write116(path, file), "116b: write fpc116.opju");
            AppWinGuard116 app(new ApplicationWindow());
            QString err;
            QElapsedTimer timer;
            timer.start();
            const bool ok = ImportOPJU::importFile(app.get(), path, &err);
            const qint64 ms = timer.elapsed();
            CHECK116(ok, "116b: FPC .opju import failed: " << err.toStdString());
            CHECK116(ms < 3 * fastMs, "116b: 20000 FPC runs took " << ms << " ms");
            Table *t = findTable116(app.get(), QStringLiteral("Fpc116"));
            CHECK116(t && t->numCols() == 2 && t->numRows() == 40000,
                     "116b: FPC table shape " << (t ? t->numCols() : -1) << "x"
                                              << (t ? t->numRows() : -1));
            CHECK116(std::fabs(t->cell(0, 0) - 1.5) < 1e-12 && t->cell(1, 0) == 0.0
                             && t->cell(2, 0) == 0.0 && t->cell(3, 0) == 0.0,
                     "116b: FPC values " << t->cell(0, 0) << "," << t->cell(1, 0) << ","
                                         << t->cell(2, 0) << "," << t->cell(3, 0));
            CHECK116(t->cell(39999, 1) == 0.0, "116b: last FPC value");
        }

        // 2. CPYUA parse-wide budget: three 600-value runs exceed a budget of 1000 cells.
        {
            const QByteArray file =
                    cpyuaFile116("Bud116",
                                 { { QByteArray("A"), constRunPayload116(600, 1.0) },
                                   { QByteArray("B"), constRunPayload116(600, 2.0) },
                                   { QByteArray("C"), constRunPayload116(600, 3.0) } });
            const QString path = tmp.filePath(QStringLiteral("budget116.opju"));
            CHECK116(write116(path, file), "116b: write budget116.opju");
            {
                torigraphSetOpjuParseCellBudgetForTesting(1000);
                AppWinGuard116 app(new ApplicationWindow());
                QString err;
                CHECK116(!ImportOPJU::importFile(app.get(), path, &err),
                         "116b: CPYUA file over the cell budget was imported");
                CHECK116(err.contains(QLatin1String("1000")),
                         "116b: CPYUA budget error: " << err.toStdString());
                CHECK116(findTable116(app.get(), QStringLiteral("Bud116")) == nullptr,
                         "116b: table created although the budget was exceeded");
            }
            {
                torigraphSetOpjuParseCellBudgetForTesting(0);
                AppWinGuard116 app(new ApplicationWindow());
                QString err;
                CHECK116(
                        ImportOPJU::importFile(app.get(), path, &err),
                        "116b: CPYUA file within the default budget failed: " << err.toStdString());
                Table *t = findTable116(app.get(), QStringLiteral("Bud116"));
                CHECK116(t && t->numCols() == 3 && t->numRows() == 600
                                 && std::fabs(t->cell(599, 2) - 3.0) < 1e-12,
                         "116b: CPYUA const runs decoded wrong");
            }
        }

        // 3. ZIP .opju: three columns reading the same stream share the budget too.
        {
            QByteArray vals;
            for (int i = 0; i < 600; ++i)
                appendLE116(vals, doubleBits116(i + 1.0), 8);
            const QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<OriginProject>"
                                   "<Spreadsheet name=\"Zb116\">"
                                   "<Column name=\"A\" type=\"X\" dataRef=\"vals.bin\"/>"
                                   "<Column name=\"B\" type=\"Y\" dataRef=\"vals.bin\"/>"
                                   "<Column name=\"C\" type=\"Y\" dataRef=\"vals.bin\"/>"
                                   "</Spreadsheet></OriginProject>\n";
            QList<Suite64ZipEntry> entries;
            entries.append(Suite64ZipEntry { QByteArray("Project.xml"), xml, false });
            entries.append(Suite64ZipEntry { QByteArray("vals.bin"), vals, true });
            const QString path = tmp.filePath(QStringLiteral("zipbudget116.opju"));
            CHECK116(write116(path, suite64BuildZip(entries)), "116b: write zipbudget116.opju");
            {
                torigraphSetOpjuParseCellBudgetForTesting(1000);
                AppWinGuard116 app(new ApplicationWindow());
                QString err;
                CHECK116(!ImportOPJU::importFile(app.get(), path, &err),
                         "116b: ZIP .opju over the cell budget was imported");
                CHECK116(err.contains(QLatin1String("1000")),
                         "116b: ZIP budget error: " << err.toStdString());
            }
            {
                torigraphSetOpjuParseCellBudgetForTesting(0);
                AppWinGuard116 app(new ApplicationWindow());
                QString err;
                CHECK116(ImportOPJU::importFile(app.get(), path, &err),
                         "116b: ZIP .opju within the budget failed: " << err.toStdString());
                Table *t = findTable116(app.get(), QStringLiteral("Zb116"));
                CHECK116(t && t->numCols() == 3 && t->numRows() == 600
                                 && std::fabs(t->cell(599, 2) - 600.0) < 1e-12
                                 && std::fabs(t->cell(0, 1) - 1.0) < 1e-12,
                         "116b: shared ZIP stream decoded wrong");
            }
        }
    }

    // =========================================================================================
    // 116c: ASCII / xlsx import
    // =========================================================================================
    {
        AsciiLimitsGuard116 limitsGuard;

        // 1. SPEC / .ras with one run-together line: refused like a plain table above the
        //    outlier limit (lowered here to 10000 cells; the default is 1e8).
        AsciiTableImportFilter::setImportSizeLimitsForTesting(0, 10000, 0);
        {
            QByteArray spec = "#F t116.spec\n#S 1 ascan th 0 1 2 1\n#N 2\n#L Theta  Detector\n";
            QByteArray ras = "*RAS_DATA_START\n*RAS_HEADER_START\n*MEAS_SCAN_UNIT_X \"deg.\"\n"
                             "*RAS_HEADER_END\n*RAS_INT_START\n";
            QByteArray rows;
            for (int i = 0; i < 2000; ++i)
                rows += QByteArray::number(i) + " " + QByteArray::number(2 * i) + " 1\n";
            QByteArray wide;
            for (int i = 0; i < 300; ++i)
                wide += QByteArray::number(i) + " ";
            wide += "\n";
            for (const bool withWide : { true, false }) {
                const QByteArray body = rows + (withWide ? wide : QByteArray());
                std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                        spec + body, QStringLiteral("Spec116"), QStringLiteral("t116.spec")));
                const AsciiTableImportFilter::ImportReport rep =
                        AsciiTableImportFilter::lastImportReport();
                if (withWide)
                    CHECK116(!ft && !rep.refusal.isEmpty(),
                             "116c: SPEC with a 300-field line was not refused (columns "
                                     << (ft ? ft->columnCount() : -1) << ")");
                else
                    CHECK116(ft && ft->columnCount() == 3 && ft->rowCount() == 2000,
                             "116c: plain SPEC refused: " << rep.refusal.toStdString());

                std::unique_ptr<future::Table> rt(AsciiTableImportFilter::importDroppedTextData(
                        ras + body + "*RAS_INT_END\n*RAS_DATA_END\n", QStringLiteral("Ras116"),
                        QStringLiteral("t116.ras")));
                const AsciiTableImportFilter::ImportReport rrep =
                        AsciiTableImportFilter::lastImportReport();
                if (withWide)
                    CHECK116(!rt && !rrep.refusal.isEmpty(),
                             "116c: .ras with a 300-field line was not refused (columns "
                                     << (rt ? rt->columnCount() : -1) << ")");
                else
                    CHECK116(rt && rt->columnCount() == 3 && rt->rowCount() == 2000,
                             "116c: plain .ras refused: " << rrep.refusal.toStdString());
            }
        }
        AsciiTableImportFilter::setImportSizeLimitsForTesting(0, 0, 0);

        // 2. Alternating blank cells: every other row of column 2 is empty (60000 rows, 30000
        //    runs). Marking them one by one scanned all the runs so far.
        {
            const int n = 60000;
            QByteArray csv = "x,y\n";
            QByteArray spec = "#F a116.spec\n#S 1 ascan th 0 1 2 1\n#N 2\n#L X  Y\n";
            QByteArray ras = "*RAS_DATA_START\n*RAS_HEADER_START\n*RAS_HEADER_END\n"
                             "*RAS_INT_START\n";
            for (int i = 0; i < n; ++i) {
                const QByteArray x = QByteArray::number(i);
                csv += x + (i % 2 ? ",\n" : ",7\n");
                spec += x + (i % 2 ? "\n" : " 7\n");
                ras += x + (i % 2 ? " \n" : " 7 1\n");
            }
            ras += "*RAS_INT_END\n*RAS_DATA_END\n";
            const QList<QPair<QString, QByteArray>> files = {
                { QStringLiteral("a116.csv"), csv },
                { QStringLiteral("a116.spec"), spec },
                { QStringLiteral("a116.ras"), ras },
            };
            for (const auto &f : files) {
                QElapsedTimer timer;
                timer.start();
                std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                        f.second, QStringLiteral("Alt116"), f.first));
                const qint64 ms = timer.elapsed();
                CHECK116(ft && ft->columnCount() >= 2 && ft->rowCount() == n,
                         "116c: " << f.first.toStdString() << " alternating blanks: shape "
                                  << (ft ? ft->columnCount() : -1) << "x"
                                  << (ft ? ft->rowCount() : -1));
                CHECK116(ms < 3 * fastMs,
                         "116c: " << f.first.toStdString() << " alternating blanks took " << ms
                                  << " ms");
                Column *y = ft->column(1);
                CHECK116(!y->isInvalid(0) && y->isInvalid(1) && !y->isInvalid(n - 2)
                                 && y->isInvalid(n - 1) && y->invalidIntervals().size() == n / 2,
                         "116c: " << f.first.toStdString() << " empty cells ("
                                  << y->invalidIntervals().size() << " runs)");
            }

            XlsxImport::Sheet sh;
            sh.name = QStringLiteral("S116");
            sh.columnCount = 3;
            sh.rows.append(QVector<XlsxImport::Cell>()
                           << xlsxText116(QStringLiteral("x")) << xlsxText116(QStringLiteral("y"))
                           << xlsxText116(QStringLiteral("d")));
            for (int i = 0; i < n; ++i) {
                const bool blank = (i % 2) != 0;
                sh.rows.append(QVector<XlsxImport::Cell>()
                               << xlsxNum116(i) << (blank ? XlsxImport::Cell() : xlsxNum116(7))
                               << (blank ? XlsxImport::Cell() : xlsxNum116(45000 + i, true)));
            }
            QElapsedTimer timer;
            timer.start();
            QList<Column *> cols = XlsxImport::sheetToColumns(sh, false);
            const qint64 ms = timer.elapsed();
            struct ColsGuard
            {
                QList<Column *> &c;
                ~ColsGuard() { qDeleteAll(c); }
            } colsGuard { cols };
            CHECK116(cols.size() == 3, "116c: xlsx sheet gave " << cols.size() << " columns");
            CHECK116(ms < 3 * fastMs, "116c: xlsx alternating blanks took " << ms << " ms");
            for (int c = 1; c < 3; ++c) {
                Column *col = cols.at(c);
                CHECK116(
                        col->rowCount() == n && !col->isInvalid(0) && col->isInvalid(1)
                                && col->isInvalid(n - 1) && col->invalidIntervals().size() == n / 2,
                        "116c: xlsx column " << c << " empty cells (mode " << int(col->columnMode())
                                             << ", " << col->invalidIntervals().size() << " runs)");
            }
            CHECK116(cols.at(2)->columnMode() == SciDAVis::ColumnMode::DateTime,
                     "116c: xlsx date column not DateTime");
        }

        // 3. .ras: the scan units are looked up once (30000 empty blocks after the first scan
        //    re-scanned the whole text twice per block), nearest header above each block.
        {
            QByteArray ras = "*RAS_DATA_START\n*RAS_HEADER_START\n*MEAS_SCAN_UNIT_X \"deg.\"\n"
                             "*MEAS_SCAN_UNIT_Y \"cps\"\n*RAS_HEADER_END\n*RAS_INT_START\n"
                             "10 100 1\n10.5 110 1\n*RAS_INT_END\n";
            for (int i = 0; i < 30000; ++i)
                ras += "*RAS_INT_START\n*RAS_INT_END\n";
            ras += "*RAS_HEADER_START\n*MEAS_SCAN_UNIT_X \"rad\"\n*MEAS_SCAN_UNIT_Y \"\"\n"
                   "*RAS_HEADER_END\n*RAS_INT_START\n0.1 5 1\n*RAS_INT_END\n*RAS_DATA_END\n";
            QElapsedTimer timer;
            timer.start();
            AsciiTableImportFilter::ImportReport rep;
            std::unique_ptr<future::Table> ft(AsciiTableImportFilter::importDroppedTextData(
                    ras, QStringLiteral("Units116"), QStringLiteral("u116.ras"), &rep));
            const qint64 ms = timer.elapsed();
            CHECK116(ft && rep.rasBlocks == 2 && ft->columnCount() == 6,
                     "116c: .ras with empty blocks: blocks " << rep.rasBlocks << ", columns "
                                                             << (ft ? ft->columnCount() : -1));
            CHECK116(ms < 3 * fastMs, "116c: .ras with 30000 empty blocks took " << ms << " ms");
            CHECK116(ft->column(0)->units() == QLatin1String("deg.")
                             && ft->column(1)->units() == QLatin1String("cps")
                             && ft->column(3)->units() == QLatin1String("rad")
                             && ft->column(4)->units() == QLatin1String("cps"),
                     "116c: .ras units " << ft->column(0)->units().toStdString() << ", "
                                         << ft->column(1)->units().toStdString() << ", "
                                         << ft->column(3)->units().toStdString() << ", "
                                         << ft->column(4)->units().toStdString());
        }
    }

    std::cout << "[PASS] Suite 116\n";
    return 0;
}
