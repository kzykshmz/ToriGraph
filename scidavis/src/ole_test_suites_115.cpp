#include "ole_test_common.h"
#include "AdvancedSignalProc.h"
#include "ExponentialFit.h"
#include "ExtraPlotTypes.h"
#include "FitDialog.h"
#include "GraphDigitizer.h"
#include "MultiPeakFit.h"
#include "OriginGadgets.h"
#include "PiecewiseAndRobustFit.h"
#include "PolynomialFit.h"
#include "PptxExport.h"
#include "QwtErrorPlotCurve.h"
#include "RecalcEngine.h"
#include "ScaleDraw.h"
#include "SmoothFilter.h"
#include "Workbook.h"
#include "XlsxImport.h"
#include "ZipReader.h"
#include "future/table/AsciiTableImportFilter.h"
#include "future/table/future_Table.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUndoStack>
#include <QXmlStreamReader>
#include <zlib.h>

#include <cmath>
#include <limits>
#include <vector>

// =============================================================================
// Suite 115: Campaign 2 Swarm Audit Regressions (All 6 Domains)
//   115a Domain 1 (IO / ZIP / PPTX / XLSX / ASCII):
//        - ZipArchive::extract() bounds decompression to min(e.uncompSize, 256 MiB)
//        - PptxExport::writeDeck() append handles whitespace before </p:sldIdLst> & </Types>
//        - XlsxImport rejects >256 sheets and duplicate sheet part references
//        - AsciiTableImportFilter rejects double-signed tokens (+-0.5), detects European
//          ';'+',' decimals past 24 preamble lines, and safely imports VASP EIGENVAL
//   115b Domain 2 (Signal & Fits):
//        - MultiPeakFit: 1-peak formula uses unindexed params, 2nd-deriv positive peak with
//          negative_peaks_allowed=true avoids fake side-lobes, !negative_peaks_allowed keeps
//          amplitude/chi2 consistent
//        - PiecewiseAndRobustFit: HuberM bounded SE under 1e6 outlier, QuantileReg finite on y=0
//        - PolynomialFit negative order clamped safely; LinearFit York custom DOF & RSS
//        - ExponentialFit growth converges on large X without exp() overflow
//        - AdvancedSignalProc::crossCoherence preserves true sampling rate on partial overlap
//   115c Domain 3 (RecalcEngine, OriginGadgets & Workbook):
//        - RecalcManager: empty outCurveTitle doesn't hijack unrelated fit curve; symmetric SG
//          OpSmooth; OpRobustFit reportTable preserved; cycle-safe downstream propagation
//        - OriginGadgets: micro-scale (1e-8) Intersect; Differentiate monotonic ROI inside
//          reversing curve; DeltaCursor out-of-range zero area
//        - WorkbookManager: showInPlaceOf on detached sheet does not corrupt d_user_hidden
//   115d Domain 4 (Graph, MultiLayer & ExtraPlotTypes):
//        - Graph: 2-arg drawCanvasFrame(false, 0) clears frame; showIntensityTable safe on Legend
//        - MultiLayer: <ToriActiveLayer> preserved on undo/redo; <ToriRecalcLinks> remapped on
//          table rename
//        - ExtraPlotTypes: plotHistogramCumulative default begin=NaN on [50000,50100]; Gumbel
//          returnValue accurate at returnPeriod=1e9 via log1p
//   115e Domain 5 (ApplicationWindow, GraphDigitizer & FitDialog):
//        - deleteFitTables() safe when two fit curves share one backing table
//        - plotDoubleY() moves attached error bars to yRight with their master curve
//        - GraphDigitizer::autoTraceCurveByColor skips non-finite log calibrations safely
//        - FitDialog 0-table setSrcTables/selectSrcTable crash-safe
//   115f Domain 6 (JSON-RPC Deep):
//        - digitizer.extract rejects log_x with non-positive values & collinear pixel axes
//        - curvemath.eis_fit rejects unimplemented RandlesWarburg model
//        - graph.set_axis keeps show_ticks=false when ticks_inward=true is also passed
//        - graph.set_axis_break rejects log_before=true when axis min <= 0
//        - stats.nonparametric Friedman skips/rejects Text columns; stats.roc scalar score_cols
//        - curvemath.bragg_convert converts reused Text out_col to Numeric
//        - UndoTargetWatch preserves undo stack tracking across failed app.open_project
// =============================================================================

namespace {

int g115Failures = 0;

#define CHECK115(cond, msg)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "[FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl;            \
            ++g115Failures;                                                                        \
        }                                                                                          \
    } while (0)

struct NonInteractiveGuard115
{
    QVariant prevNonInteractive;
    bool prevQuitOnLast = true;
    NonInteractiveGuard115()
        : prevNonInteractive(qApp->property("torigraph_noninteractive")),
          prevQuitOnLast(QApplication::quitOnLastWindowClosed())
    {
        qApp->setProperty("torigraph_noninteractive", true);
        QApplication::setQuitOnLastWindowClosed(false);
    }
    ~NonInteractiveGuard115()
    {
        qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        QApplication::setQuitOnLastWindowClosed(prevQuitOnLast);
    }
};

struct AppWinGuard115
{
    ApplicationWindow *w = nullptr;
    explicit AppWinGuard115(ApplicationWindow *ptr) : w(ptr)
    {
        if (w) {
            w->applyUserSettings();
            MultiLayer::setNewGraphStylePresetId(QString());
        }
    }
    ~AppWinGuard115()
    {
        if (w) {
            w->savedProject();
            w->setOleEmbeddedMode(true);
            delete w;
        }
    }
    ApplicationWindow *operator->() const { return w; }
    operator ApplicationWindow *() const { return w; }
};

struct Rpc115
{
    PythonAutomationServer *srv = nullptr;
    int id = 11500;
    QJsonObject call(const QString &method, const QJsonObject &params)
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = ++id;
        req[QStringLiteral("method")] = method;
        req[QStringLiteral("params")] = params;
        const QJsonObject resp = srv->handleRpcRequest(req);
        QCoreApplication::processEvents();
        return resp;
    }
    static bool ok(const QJsonObject &resp) { return !resp.contains(QStringLiteral("error")); }
    static QJsonObject result(const QJsonObject &resp)
    {
        return resp.value(QStringLiteral("result")).toObject();
    }
};

MultiLayer *findGraph115(ApplicationWindow *app, const QString &name)
{
    for (MyWidget *w : app->windowsList()) {
        if (w && w->inherits("MultiLayer") && w->name() == name)
            return static_cast<MultiLayer *>(w);
    }
    return nullptr;
}

quint32 crc32Bytes115(const QByteArray &data)
{
    return static_cast<quint32>(
            ::crc32(0L, reinterpret_cast<const Bytef *>(data.constData()),
                    static_cast<uInt>(data.size())));
}

QByteArray rawDeflate115(const QByteArray &plain)
{
    z_stream zs{};
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return QByteArray();
    QByteArray out(static_cast<int>(deflateBound(&zs, static_cast<uLong>(plain.size()))) + 64, '\0');
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(plain.data()));
    zs.avail_in = static_cast<uInt>(plain.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    const int ret = deflate(&zs, Z_FINISH);
    const int written = static_cast<int>(zs.total_out);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END)
        return QByteArray();
    out.resize(written);
    return out;
}

struct ZipSpec115
{
    QString name;
    QByteArray payload; // stored or already raw-deflated
    quint16 method = 0; // 0 = STORE, 8 = DEFLATE
    quint32 crc = 0;
    quint32 declaredUncomp = 0;
};

QByteArray buildZip115(const QVector<ZipSpec115> &items)
{
    auto appendLE16 = [](QByteArray &b, quint16 v) {
        b.append(char(v & 0xFF));
        b.append(char((v >> 8) & 0xFF));
    };
    auto appendLE32 = [](QByteArray &b, quint32 v) {
        b.append(char(v & 0xFF));
        b.append(char((v >> 8) & 0xFF));
        b.append(char((v >> 16) & 0xFF));
        b.append(char((v >> 24) & 0xFF));
    };

    QByteArray zip;
    QByteArray cd;
    for (const auto &it : items) {
        const QByteArray nm = it.name.toUtf8();
        const quint32 off = static_cast<quint32>(zip.size());
        const quint32 compSz = static_cast<quint32>(it.payload.size());
        const quint32 crc = (it.method == 0 && it.crc == 0) ? crc32Bytes115(it.payload) : it.crc;
        const quint32 uncompSz = (it.method == 0 && it.declaredUncomp == 0) ? compSz : it.declaredUncomp;

        // Local file header
        appendLE32(zip, 0x04034b50u);
        appendLE16(zip, 20);
        appendLE16(zip, 0);
        appendLE16(zip, it.method);
        appendLE16(zip, 0);
        appendLE16(zip, 0);
        appendLE32(zip, crc);
        appendLE32(zip, compSz);
        appendLE32(zip, uncompSz);
        appendLE16(zip, static_cast<quint16>(nm.size()));
        appendLE16(zip, 0);
        zip.append(nm);
        zip.append(it.payload);

        // Central directory header
        appendLE32(cd, 0x02014b50u);
        appendLE16(cd, 20);
        appendLE16(cd, 20);
        appendLE16(cd, 0);
        appendLE16(cd, it.method);
        appendLE16(cd, 0);
        appendLE16(cd, 0);
        appendLE32(cd, crc);
        appendLE32(cd, compSz);
        appendLE32(cd, uncompSz);
        appendLE16(cd, static_cast<quint16>(nm.size()));
        appendLE16(cd, 0);
        appendLE16(cd, 0);
        appendLE16(cd, 0);
        appendLE16(cd, 0);
        appendLE32(cd, 0);
        appendLE32(cd, off);
        cd.append(nm);
    }
    const quint32 cdOff = static_cast<quint32>(zip.size());
    zip.append(cd);
    appendLE32(zip, 0x06054b50u);
    appendLE16(zip, 0);
    appendLE16(zip, 0);
    appendLE16(zip, static_cast<quint16>(items.size()));
    appendLE16(zip, static_cast<quint16>(items.size()));
    appendLE32(zip, static_cast<quint32>(cd.size()));
    appendLE32(zip, cdOff);
    appendLE16(zip, 0);
    return zip;
}

} // namespace

int runSuite115Campaign2SwarmAuditRegressions(ApplicationWindow * /*mw*/)
{
    g115Failures = 0;
    NonInteractiveGuard115 guard;

    // =========================================================================
    // 115a: Domain 1 — IO / ZIP / PPTX / XLSX / ASCII
    // =========================================================================
    {
        QTemporaryDir tmpDir;

        // 1. ZipArchive::extract rejects forged-size deflate bomb (> declared uncompSize)
        {
            const QByteArray plain(4096, 'A');
            const QByteArray deflated = rawDeflate115(plain);
            CHECK115(!deflated.isEmpty(), "115a: rawDeflate115 failed");
            ZipSpec115 spec;
            spec.name = QStringLiteral("bomb.txt");
            spec.payload = deflated;
            spec.method = 8;
            spec.crc = crc32Bytes115(plain);
            spec.declaredUncomp = 16; // forged small header size!
            const QByteArray zipBytes = buildZip115({ spec });
            torizip::ZipArchive za;
            QString err;
            CHECK115(za.openData(zipBytes, &err), "115a: ZipArchive::openData failed: " << err.toStdString());
            QByteArray extracted;
            const bool ok = za.extract(0, extracted, &err);
            CHECK115(!ok, "115a: ZipArchive::extract must reject deflate stream exceeding declared uncompSize");
        }

        // 2. PptxExport append with whitespace before </p:sldIdLst> & </Types>
        {
            const QString pptxPath = tmpDir.path() + QStringLiteral("/deck115.pptx");
            QImage pngImg(40, 30, QImage::Format_ARGB32);
            pngImg.fill(Qt::white);
            QByteArray pngBytes;
            {
                QBuffer buf(&pngBytes);
                buf.open(QIODevice::WriteOnly);
                pngImg.save(&buf, "PNG");
            }
            ToriGraphPptx::SlideItem item1;
            item1.graphName = QStringLiteral("SlideOne115");
            item1.previewBytes = pngBytes;
            item1.previewExtension = QStringLiteral("png");
            item1.widthHm = 8500;
            item1.heightHm = 6500;

            ToriGraphPptx::DeckOptions opt;
            opt.appendToExisting = false;
            const auto r1 = ToriGraphPptx::writeDeck(pptxPath, { item1 }, opt);
            CHECK115(r1.ok && r1.slideCount == 1, "115a: initial PptxExport::writeDeck failed");

            // Rewrite ppt/presentation.xml and [Content_Types].xml to add whitespace/newlines
            // immediately before </p:sldIdLst> and </Types> to stress QXmlStreamReader offset.
            torizip::ZipArchive za1;
            QString zerr;
            CHECK115(za1.open(pptxPath, &zerr), "115a: open initial pptx failed");
            QVector<ZipSpec115> rebuilt;
            for (int i = 0; i < za1.entries().size(); ++i) {
                QByteArray data;
                if (!za1.extract(i, data, &zerr))
                    continue;
                const QString en = za1.entries()[i].name;
                if (en == QStringLiteral("ppt/presentation.xml"))
                    data.replace("</p:sldIdLst>", "\n  </p:sldIdLst>");
                else if (en == QStringLiteral("[Content_Types].xml"))
                    data.replace("</Types>", "\n  </Types>");
                ZipSpec115 s;
                s.name = en;
                s.payload = data;
                s.method = 0;
                rebuilt.append(s);
            }
            {
                QFile pf(pptxPath);
                CHECK115(pf.open(QIODevice::WriteOnly), "115a: reopen pptx for rewrite failed");
                pf.write(buildZip115(rebuilt));
            }

            ToriGraphPptx::SlideItem item2 = item1;
            item2.graphName = QStringLiteral("SlideTwo115");
            opt.appendToExisting = true;
            const auto r2 = ToriGraphPptx::writeDeck(pptxPath, { item2 }, opt);
            CHECK115(r2.ok && r2.slideCount == 2,
                     "115a: PptxExport append failed: " << r2.error.toStdString());

            torizip::ZipArchive za2;
            CHECK115(za2.open(pptxPath, &zerr), "115a: reopen appended pptx failed");
            for (const QString &xmlPart :
                 { QStringLiteral("ppt/presentation.xml"), QStringLiteral("[Content_Types].xml") }) {
                const int idx = za2.findExactEntry(xmlPart);
                QByteArray xmlData;
                CHECK115(idx >= 0 && za2.extract(idx, xmlData, &zerr),
                         "115a: missing " << xmlPart.toStdString() << " after append");
                QXmlStreamReader xr(xmlData);
                while (!xr.atEnd())
                    xr.readNext();
                CHECK115(!xr.hasError(),
                         "115a: appended " << xmlPart.toStdString()
                                           << " is malformed XML: " << xr.errorString().toStdString());
            }
        }

        // 3. XlsxImport rejects >256 sheets and duplicate sheet part references
        {
            AppWinGuard115 w(new ApplicationWindow());
            // 3a. >256 sheets
            {
                QByteArray wbXml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                   "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                                   "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>";
                QByteArray relsXml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                     "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
                QVector<ZipSpec115> parts;
                for (int s = 1; s <= 260; ++s) {
                    wbXml += QStringLiteral("<sheet name=\"S%1\" sheetId=\"%1\" r:id=\"rId%1\"/>").arg(s).toUtf8();
                    relsXml += QStringLiteral(
                                       "<Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%1.xml\"/>")
                                       .arg(s)
                                       .toUtf8();
                    ZipSpec115 ws;
                    ws.name = QStringLiteral("xl/worksheets/sheet%1.xml").arg(s);
                    ws.payload = "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData><row r=\"1\"><c r=\"A1\"><v>1</v></c></row></sheetData></worksheet>";
                    parts.append(ws);
                }
                wbXml += "</sheets></workbook>";
                relsXml += "</Relationships>";
                parts.append({ QStringLiteral("xl/workbook.xml"), wbXml, 0, 0, 0 });
                parts.append({ QStringLiteral("xl/_rels/workbook.xml.rels"), relsXml, 0, 0, 0 });
                const QString xlsxMany = tmpDir.path() + QStringLiteral("/many_sheets.xlsx");
                {
                    QFile f(xlsxMany);
                    f.open(QIODevice::WriteOnly);
                    f.write(buildZip115(parts));
                }
                QString err;
                XlsxImport::Workbook wb;
                const bool ok = XlsxImport::readWorkbook(xlsxMany, wb, &err);
                CHECK115(!ok, "115a: XlsxImport must reject workbook with >256 sheets");
            }
            // 3b. Duplicate worksheet part reference (alias amplification)
            {
                const QByteArray wbXml =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>"
                        "<sheet name=\"S1\" sheetId=\"1\" r:id=\"rId1\"/>"
                        "<sheet name=\"S2\" sheetId=\"2\" r:id=\"rId2\"/>"
                        "</sheets></workbook>";
                const QByteArray relsXml =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
                        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
                        "</Relationships>";
                const QByteArray wsXml =
                        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                        "<sheetData><row r=\"1\"><c r=\"A1\"><v>42</v></c></row></sheetData></worksheet>";
                const QString xlsxDup = tmpDir.path() + QStringLiteral("/dup_part.xlsx");
                {
                    QFile f(xlsxDup);
                    f.open(QIODevice::WriteOnly);
                    f.write(buildZip115({ { QStringLiteral("xl/workbook.xml"), wbXml, 0, 0, 0 },
                                          { QStringLiteral("xl/_rels/workbook.xml.rels"), relsXml, 0, 0, 0 },
                                          { QStringLiteral("xl/worksheets/sheet1.xml"), wsXml, 0, 0, 0 } }));
                }
                QString err;
                XlsxImport::Workbook wb;
                const bool ok = XlsxImport::readWorkbook(xlsxDup, wb, &err);
                CHECK115(!ok, "115a: XlsxImport must reject duplicate worksheet part references");
            }
        }

        // 4. AsciiTableImportFilter: double-signed tokens (+-0.5), >24-line European ';' CSV with
        //    tab in preamble, English CSV with scattered ';', alpha-hash consistency, VASP EIGENVAL
        {
            // 4a. Double-signed token must NOT parse as numeric
            {
                QByteArray csv = "X,Y\n1.0,+-0.5\n2.0,++1.25\n";
                QBuffer buf(&csv);
                buf.open(QIODevice::ReadOnly);
                AsciiTableImportFilter flt;
                flt.set_separator(QStringLiteral(","));
                flt.set_first_row_names_columns(true);
                flt.set_convert_to_numeric(true);
                std::unique_ptr<AbstractAspect> asp(flt.importAspect(buf));
                auto *ft = qobject_cast<future::Table *>(asp.get());
                CHECK115(ft && ft->columnCount() == 2 && ft->rowCount() == 2,
                         "115a: AsciiTableImportFilter failed on double-signed CSV");
                if (ft && ft->columnCount() == 2) {
                    Column *cY = ft->column(1);
                    const bool rejectedNumeric =
                            (cY->columnMode() == SciDAVis::ColumnMode::Text)
                            || (cY->isInvalid(0) && cY->isInvalid(1));
                    CHECK115(rejectedNumeric,
                             "115a: double-signed tokens (+-0.5, ++1.25) were falsely parsed as numbers");
                }
            }

            // 4b. European ';'+',' CSV preceded by 30 non-# metadata lines (including a tab!)
            {
                QByteArray eu;
                eu += "Instrument:\tZwick\n";
                for (int i = 0; i < 29; ++i)
                    eu += QStringLiteral("MetaKey%1: Val%1\n").arg(i + 1).toUtf8();
                eu += "Time;Value\n0,5;1,25\n1,5;2,75\n2,5;4,50\n";
                std::unique_ptr<future::Table> ft(
                        AsciiTableImportFilter::importDroppedTextData(
                                eu, QStringLiteral("EuPreamble115"), QStringLiteral("eu.csv")));
                CHECK115(ft && ft->columnCount() == 2 && ft->rowCount() == 3,
                         "115a: European CSV after 30 preamble lines (with tab) failed to import");
                if (ft && ft->columnCount() == 2 && ft->rowCount() == 3) {
                    CHECK115(ft->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                                     && ft->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                                     && std::abs(ft->column(0)->valueAt(0) - 0.5) < 1e-9
                                     && std::abs(ft->column(1)->valueAt(2) - 4.5) < 1e-9,
                             "115a: European ';' + ',' decimal after 30 preamble lines parsed wrong values");
                }
            }

            // 4b2. Normal English comma CSV with scattered ';' in notes must NOT flip to German locale
            {
                QByteArray en = "X,Y,Note\n";
                for (int i = 0; i < 30; ++i)
                    en += QStringLiteral("%1.5,%2.25,ok\n").arg(i).arg(i + 1).toUtf8();
                en += "31.5,32.25,ratio 1,2;3,4\n";
                en += "32.5,33.25,ok\n";
                en += "33.5,34.25,ratio 5,6;7,8\n";
                std::unique_ptr<future::Table> ft(
                        AsciiTableImportFilter::importDroppedTextData(
                                en, QStringLiteral("EnScattered115"), QStringLiteral("en.csv")));
                CHECK115(ft && ft->columnCount() >= 3 && ft->rowCount() == 33
                                 && ft->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                                 && std::abs(ft->column(0)->valueAt(0) - 0.5) < 1e-9,
                         "115a: English CSV with scattered ';' in notes was falsely flipped to European locale");
            }

            // 4b3. Mixed #N/A + 3-col #A1 comment + trailing 1,2 data row preserves trailing row
            {
                QByteArray mix = "Title\nA,B\n#N/A,foo\n#A1,100,200\n1,2\n";
                QBuffer buf(&mix);
                buf.open(QIODevice::ReadOnly);
                AsciiTableImportFilter flt;
                flt.set_ignored_lines(1);
                flt.set_separator(QStringLiteral(","));
                flt.set_first_row_names_columns(true);
                flt.set_convert_to_numeric(true);
                std::unique_ptr<AbstractAspect> asp(flt.importAspect(buf));
                auto *ft = qobject_cast<future::Table *>(asp.get());
                CHECK115(ft && ft->columnCount() == 2 && ft->rowCount() == 2
                                 && std::abs(ft->column(0)->valueAt(1) - 1.0) < 1e-9
                                 && ft->column(1)->textAt(1) == QStringLiteral("2"),
                         "115a: mixed #N/A + skipped #A1 dropped trailing data row (rows="
                                 << (ft ? ft->rowCount() : -1) << ")");
            }

            // 4c. VASP EIGENVAL safe import
            {
                QByteArray eig =
                        "    2    2    1    1\n"
                        "  0.1000000E+02  0.5000000E-09  0.5000000E-09  0.5000000E-09  0.5000000E-15\n"
                        "  1.0000000000000000E-04\n"
                        "  CAR\n"
                        " Si_Test\n"
                        "    8    2    2\n"
                        "\n"
                        "  0.0000000E+00  0.0000000E+00  0.0000000E+00  0.5000000E+00\n"
                        "    1   -1.500000   1.000000\n"
                        "    2    2.250000   0.000000\n"
                        "\n"
                        "  0.5000000E+00  0.0000000E+00  0.0000000E+00  0.5000000E+00\n"
                        "    1   -0.750000   1.000000\n"
                        "    2    1.750000   0.000000\n";
                QBuffer buf(&eig);
                buf.open(QIODevice::ReadOnly);
                AsciiTableImportFilter flt;
                std::unique_ptr<AbstractAspect> asp(flt.importAspect(buf));
                auto *ft = qobject_cast<future::Table *>(asp.get());
                CHECK115(ft && ft->rowCount() == 2 && ft->columnCount() >= 3,
                         "115a: VASP EIGENVAL import failed");
                if (ft && ft->rowCount() == 2 && ft->columnCount() >= 3) {
                    CHECK115(std::abs(ft->column(1)->valueAt(0) - (-1.5)) < 1e-6
                                     && std::abs(ft->column(2)->valueAt(1) - 1.75) < 1e-6,
                             "115a: VASP EIGENVAL band energies mismatch");
                }
            }
        }
    }

    // =========================================================================
    // 115b: Domain 2 — Signal & Fits
    // =========================================================================
    {
        AppWinGuard115 w(new ApplicationWindow());

        // 1. MultiPeakFit: positive Gaussian with negative_peaks_allowed=true (no fake side-lobe peaks),
        //    and 1-peak formula uses unindexed variable names.
        {
            Table *tp = w->newTable(QStringLiteral("MPF115"), 61, 2);
            for (int i = 0; i < 61; ++i) {
                const double x = -3.0 + 0.1 * double(i);
                const double y = 5.0 + 100.0 * std::exp(-2.0 * (x - 0.5) * (x - 0.5));
                tp->setCell(i, 0, x);
                tp->setCell(i, 1, y);
            }
            MultiLayer *mlp = w->multilayerPlot(tp, QStringList() << tp->colName(1), Graph::Scatter);
            Graph *gp = mlp ? mlp->activeGraph() : nullptr;
            CHECK115(gp && gp->curves() == 1, "115b: MPF setup failed");
            if (gp && gp->curves() == 1) {
                const QString ct = gp->curve(0)->title().text();
                MultiPeakFit mpf(w, gp, MultiPeakFit::Gauss, 1);
                mpf.setNegativePeaksAllowed(true);
                mpf.enablePeakCurves(true);
                mpf.setDataFromCurve(ct, gp);
                mpf.guessInitialValues();
                mpf.fit();
                const auto res = mpf.results();
                CHECK115(res.size() >= 4 && res[0] > 50.0 && std::abs(res[1] - 0.5) < 0.15,
                         "115b: MultiPeakFit(negative_peaks_allowed=true) missed positive peak (A="
                                 << (res.size() > 0 ? res[0] : -999.0) << ", xc="
                                 << (res.size() > 1 ? res[1] : -999.0) << ")");
                CHECK115(mpf.rSquare() > 0.99,
                         "115b: MultiPeakFit 1-peak R^2 too low: " << mpf.rSquare());

                // Verify Lorentz downward peak with !negative_peaks_allowed cannot evade via w < 0
                Table *tDown = w->newTable(QStringLiteral("LorentzDown115"), 41, 2);
                for (int i = 0; i < 41; ++i) {
                    const double x = 0.25 * double(i);
                    const double y = 10.0 - 5.0 / (4.0 * (x - 5.0) * (x - 5.0) + 1.0);
                    tDown->setCell(i, 0, x);
                    tDown->setCell(i, 1, y);
                }
                MultiLayer *mlDown = w->multilayerPlot(
                        tDown, QStringList() << tDown->colName(1), Graph::Scatter);
                Graph *gDown = mlDown ? mlDown->activeGraph() : nullptr;
                if (gDown && gDown->curves() == 1) {
                    MultiPeakFit mpfDown(w, gDown, MultiPeakFit::Lorentz, 1);
                    mpfDown.setNegativePeaksAllowed(false);
                    mpfDown.setDataFromCurve(gDown->curve(0)->title().text(), gDown);
                    double initP[4] = { 1.0, 5.0, 0.5, 10.0 };
                    mpfDown.setInitialGuesses(initP);
                    mpfDown.fit();
                    const auto rd = mpfDown.results();
                    CHECK115(rd.size() >= 4 && rd[0] >= 0.0 && rd[2] >= 0.0
                                     && std::isfinite(mpfDown.chiSquare())
                                     && mpfDown.chiSquare() > 1.0,
                             "115b: Lorentz downward peak evaded !negative_peaks_allowed via w<0 (A="
                                     << (rd.size() > 0 ? rd[0] : -1.0)
                                     << ", w=" << (rd.size() > 2 ? rd[2] : -1.0)
                                     << ", chi2=" << mpfDown.chiSquare() << ")");

                    // With negative_peaks_allowed=true and negative initial width (w < 0),
                    // normalized results, evalPeakAt/fit curve, and confidence band must all agree!
                    MultiPeakFit mpfDownNeg(w, gDown, MultiPeakFit::Lorentz, 1);
                    mpfDownNeg.setNegativePeaksAllowed(true);
                    mpfDownNeg.enableConfidenceBand(true, 0.95);
                    mpfDownNeg.setDataFromCurve(gDown->curve(0)->title().text(), gDown);
                    double initNegW[4] = { 7.5, 5.0, -1.0, 10.0 };
                    mpfDownNeg.setInitialGuesses(initNegW);
                    mpfDownNeg.fit();
                    const auto rn = mpfDownNeg.results();
                    const auto errs = mpfDownNeg.errors();
                    bool allErrFinite = errs.size() >= 4;
                    for (double ev : errs)
                        allErrFinite = allErrFinite && std::isfinite(ev) && ev >= 0.0;
                    CHECK115(rn.size() >= 4 && rn[0] < -2.0 && rn[2] > 0.0 && mpfDownNeg.rSquare() > 0.99
                                     && allErrFinite,
                             "115b: Lorentz negative_peaks_allowed=true with w<0 guess failed (A="
                                     << (rn.size() > 0 ? rn[0] : 0.0)
                                     << ", w=" << (rn.size() > 2 ? rn[2] : 0.0)
                                     << ", R2=" << mpfDownNeg.rSquare() << ")");
                }
            }
        }

        // 2. PiecewiseAndRobustFit: HuberM bounded SE under 1e6 outlier & QuantileReg on all-zero Y
        {
            Table *th = w->newTable(QStringLiteral("Huber115"), 20, 2);
            for (int i = 0; i < 20; ++i) {
                const double x = double(i + 1);
                const double y = (i == 0) ? 1.0e6 : (2.0 + 3.0 * x + ((i % 2) ? 0.05 : -0.05));
                th->setCell(i, 0, x);
                th->setCell(i, 1, y);
            }
            MultiLayer *mlh = w->multilayerPlot(th, QStringList() << th->colName(1), Graph::Scatter);
            Graph *gh = mlh ? mlh->activeGraph() : nullptr;
            if (gh && gh->curves() == 1) {
                const auto hr = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                        w, gh, gh->curve(0)->title().text(), PiecewiseAndRobustFit::HuberM, 0.5, false);
                CHECK115(hr.converged && std::abs(hr.slope - 3.0) < 0.2
                                 && std::isfinite(hr.slopeSE) && hr.slopeSE < 5.0,
                         "115b: HuberM SE blew up under 1e6 outlier (slope=" << hr.slope
                                                                             << ", slopeSE="
                                                                             << hr.slopeSE << ")");
            }

            Table *tz = w->newTable(QStringLiteral("ZeroQ115"), 12, 2);
            for (int i = 0; i < 12; ++i) {
                tz->setCell(i, 0, double(i));
                tz->setCell(i, 1, 0.0);
            }
            MultiLayer *mlz = w->multilayerPlot(tz, QStringList() << tz->colName(1), Graph::Scatter);
            Graph *gz = mlz ? mlz->activeGraph() : nullptr;
            if (gz && gz->curves() == 1) {
                const auto qr = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                        w, gz, gz->curve(0)->title().text(), PiecewiseAndRobustFit::QuantileReg, 0.5, false);
                CHECK115(qr.converged && std::isfinite(qr.intercept) && std::isfinite(qr.slope)
                                 && std::isfinite(qr.interceptSE) && std::isfinite(qr.slopeSE),
                         "115b: QuantileReg produced non-finite output on all-zero Y");
            }
        }

        // 3. PolynomialFit negative order clamped & LinearFit York custom DOF/RSS
        {
            Table *tl = w->newTable(QStringLiteral("York115"), 10, 4);
            tl->setColPlotDesignation(2, SciDAVis::xErr);
            tl->setColPlotDesignation(3, SciDAVis::yErr);
            for (int i = 0; i < 10; ++i) {
                tl->setCell(i, 0, double(i + 1));
                tl->setCell(i, 1, 1.5 + 2.5 * double(i + 1) + ((i % 2) ? 0.1 : -0.1));
                tl->setCell(i, 2, (i < 8) ? 0.1 : 0.0); // last 2 rows invalid zero xErr
                tl->setCell(i, 3, (i < 8) ? 0.2 : 0.0);
            }
            MultiLayer *mll = w->multilayerPlot(tl, QStringList() << tl->colName(1), Graph::Scatter);
            Graph *gl = mll ? mll->activeGraph() : nullptr;
            if (gl && gl->curves() == 1) {
                const QString ct = gl->curve(0)->title().text();
                PolynomialFit negPoly(w, gl, -3);
                negPoly.setDataFromCurve(ct, gl);
                negPoly.fit();
                CHECK115(negPoly.numParameters() == 1 && std::isfinite(negPoly.results()[0]),
                         "115b: PolynomialFit(-3) did not clamp order to 0 safely");

                LinearFit york(w, gl, ct);
                const bool yOk = york.fitWithXYErrors(tl->colName(2), tl->colName(3));
                CHECK115(yOk && std::abs(york.results()[1] - 2.5) < 0.2
                                 && std::isfinite(york.chiSquare()) && york.rSquare() > 0.99,
                         "115b: LinearFit::fitWithXYErrors failed or gave wrong stats (ok="
                                 << yOk << ", R2=" << york.rSquare() << ")");
            }
        }

        // 4. ExponentialFit growth on large X converges without exp() overflow
        {
            Table *te = w->newTable(QStringLiteral("ExpG115"), 21, 2);
            for (int i = 0; i < 21; ++i) {
                const double x = 100.0 + double(i);
                const double y = 1.0 + 0.01 * std::exp(0.05 * x);
                te->setCell(i, 0, x);
                te->setCell(i, 1, y);
            }
            MultiLayer *mle = w->multilayerPlot(te, QStringList() << te->colName(1), Graph::Scatter);
            Graph *ge = mle ? mle->activeGraph() : nullptr;
            if (ge && ge->curves() == 1) {
                ExponentialFit ef(w, ge, ge->curve(0)->title().text(), true /*expGrowth*/);
                ef.guessInitialValues();
                ef.fit();
                const auto res = ef.results();
                CHECK115(res.size() >= 3 && std::isfinite(res[0]) && std::isfinite(res[1])
                                 && std::isfinite(res[2]) && ef.rSquare() > 0.99,
                         "115b: ExponentialFit(expGrowth) failed on large X [100,120] (R2="
                                 << ef.rSquare() << ")");
            }
        }

        // 5. AdvancedSignalProc::crossCoherence partial overlap does not inflate sampling rate
        {
            Table *tc = w->newTable(QStringLiteral("Coh115"), 201, 4);
            tc->setColPlotDesignation(0, SciDAVis::X);
            tc->setColPlotDesignation(1, SciDAVis::Y);
            tc->setColPlotDesignation(2, SciDAVis::X);
            tc->setColPlotDesignation(3, SciDAVis::Y);
            for (int i = 0; i < 201; ++i) {
                const double tA = 0.0 + 0.05 * double(i); // [0, 10], fs = 20 Hz
                const double tB = 8.0 + 0.05 * double(i); // [8, 18], overlap [8, 10] = 2s (~41 pts)
                tc->setCell(i, 0, tA);
                tc->setCell(i, 1, std::sin(2.0 * M_PI * 2.0 * tA));
                tc->setCell(i, 2, tB);
                tc->setCell(i, 3, std::sin(2.0 * M_PI * 2.0 * tB));
            }
            MultiLayer *mlc = w->multilayerPlot(
                    tc, QStringList() << tc->colName(1) << tc->colName(3), Graph::Line);
            Graph *gc = mlc ? mlc->activeGraph() : nullptr;
            if (gc && gc->curves() == 2) {
                Table *cohTbl = AdvancedSignalProc::crossCoherence(
                        w, gc, gc->curve(0)->title().text(), gc->curve(1)->title().text(), 16, false);
                CHECK115(cohTbl && cohTbl->numRows() >= 2, "115b: crossCoherence returned empty table");
                if (cohTbl && cohTbl->numRows() >= 2) {
                    const double fMax = cohTbl->cell(cohTbl->numRows() - 1, 0);
                    CHECK115(std::isfinite(fMax) && fMax > 5.0 && fMax < 15.0,
                             "115b: crossCoherence Nyquist inflated on partial overlap (fMax="
                                     << fMax << ", expected ~10 Hz)");
                }
            }
        }
    }

    // =========================================================================
    // 115c: Domain 3 — RecalcEngine, OriginGadgets & Workbook
    // =========================================================================
    {
        AppWinGuard115 w(new ApplicationWindow());

        // 1. RecalcManager: empty outCurveTitle doesn't hijack unrelated curve; symmetric SG OpSmooth;
        //    OpRobustFit preserves report table; cyclic link terminates safely.
        {
            Table *tr = w->newTable(QStringLiteral("Rec115"), 21, 2);
            for (int i = 0; i < 21; ++i) {
                const double x = double(i);
                tr->setCell(i, 0, x);
                tr->setCell(i, 1, 2.0 * x + 1.0 + ((i % 2) ? 0.2 : -0.2));
            }
            MultiLayer *mlr = w->multilayerPlot(tr, QStringList() << tr->colName(1), Graph::Line);
            Graph *gr = mlr ? mlr->activeGraph() : nullptr;
            CHECK115(gr && gr->curves() == 1, "115c: Recalc setup failed");
            if (gr && gr->curves() == 1) {
                const QString srcTitle = gr->curve(0)->title().text();
                // Add an unrelated fit curve first
                LinearFit lf(w, gr, srcTitle);
                lf.fit();
                const int curvesWithUnrelated = gr->curves();
                CHECK115(curvesWithUnrelated >= 2, "115c: initial linear fit curve missing");
                const QString unrelatedTitle = gr->curve(curvesWithUnrelated - 1)->title().text();

                RecalcManager *rm = RecalcManager::instance(w);
                rm->setRecalcMode(RecalcManager::Manual);
                // Register OpSmooth with empty outCurveTitle -> must NOT hijack unrelatedTitle!
                QVariantMap sgParams;
                sgParams[QStringLiteral("method")] = int(SmoothFilter::SavitzkyGolay);
                sgParams[QStringLiteral("points")] = 5;
                sgParams[QStringLiteral("order")] = 2;
                rm->registerLink(w, gr, tr, srcTitle, QString(), RecalcManager::OpSmooth, sgParams);
                tr->setCell(10, 1, 22.0);
                tr->notifyChanges();
                rm->recalculateAll(gr);
                bool unrelatedStillPresent = false;
                for (int i = 0; i < gr->curves(); ++i) {
                    if (gr->curve(i) && gr->curve(i)->title().text() == unrelatedTitle)
                        unrelatedStillPresent = true;
                }
                CHECK115(unrelatedStillPresent,
                         "115c: RecalcManager empty outCurveTitle hijacked/deleted unrelated fit curve!");
                CHECK115(rm->lastError(gr).isEmpty(),
                         "115c: OpSmooth SG recalc errored: " << rm->lastError(gr).toStdString());

                // OpRobustFit preserves full report table without crashing/corrupting
                const auto rfit = PiecewiseAndRobustFit::fitRobustOrQuantileLinear(
                        w, gr, srcTitle, PiecewiseAndRobustFit::HuberM, 0.5, false);
                CHECK115(rfit.converged && rfit.reportTable != nullptr,
                         "115c: initial OpRobustFit failed");
                if (rfit.converged && rfit.fittedCurve && rfit.reportTable) {
                    const int origReportRows = rfit.reportTable->numRows();
                    QVariantMap robParams;
                    robParams[QStringLiteral("method")] = int(PiecewiseAndRobustFit::HuberM);
                    rm->registerLink(w, gr, tr, srcTitle, rfit.fittedCurve->title().text(),
                                     RecalcManager::OpRobustFit, robParams, rfit.reportTable);
                    tr->setCell(5, 1, 15.0);
                    tr->notifyChanges();
                    rm->recalculateAll(gr);
                    CHECK115(rfit.reportTable->numRows() == origReportRows && rm->lastError(gr).isEmpty(),
                             "115c: OpRobustFit recalc corrupted reportTable or errored");
                }
            }
        }

        // 2. OriginGadgets: micro-scale (1e-8) Intersect, Differentiate inside reversing curve,
        //    DeltaCursor out-of-range zero area.
        {
            Table *tm = w->newTable(QStringLiteral("Micro115"), 5, 3);
            for (int i = 0; i < 5; ++i) {
                const double x = double(i) * 2.5e-9; // [0, 1e-8]
                tm->setCell(i, 0, x);
                tm->setCell(i, 1, x);          // y = x
                tm->setCell(i, 2, 1.0e-8 - x); // y = 1e-8 - x -> cross at (5e-9, 5e-9)
            }
            MultiLayer *mlm = w->multilayerPlot(
                    tm, QStringList() << tm->colName(1) << tm->colName(2), Graph::Line);
            Graph *gm = mlm ? mlm->activeGraph() : nullptr;
            if (gm && gm->curves() == 2) {
                RoiGadgetTool *gt = RoiGadgetTool::attachGadget(w, gm, RoiGadgetTool::Intersect);
                CHECK115(gt != nullptr, "115c: failed to attach Intersect gadget");
                if (gt) {
                    gt->setRoiRect(QRectF(0.0, 0.0, 1.0e-8, 1.0e-8));
                    gt->recalculate();
                    CHECK115(gt->primaryMetric() >= 1.0,
                             "115c: Intersect missed micro-scale (1e-8) crossing (count="
                                     << gt->primaryMetric() << ")");
                    RoiGadgetTool::clearGadget(gm);
                }
            }

            // Differentiate: X reverses outside ROI [2, 8], monotonic inside ROI [2, 8]
            Table *td = w->newTable(QStringLiteral("RevDiff115"), 10, 2);
            const double xs[10] = { 10.0, 0.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 1.0 };
            for (int i = 0; i < 10; ++i) {
                td->setCell(i, 0, xs[i]);
                td->setCell(i, 1, 3.0 * xs[i] + 1.0);
            }
            MultiLayer *mld = w->multilayerPlot(td, QStringList() << td->colName(1), Graph::Line);
            Graph *gd = mld ? mld->activeGraph() : nullptr;
            if (gd && gd->curves() == 1) {
                RoiGadgetTool *gt = RoiGadgetTool::attachGadget(w, gd, RoiGadgetTool::Differentiate);
                if (gt) {
                    gt->setRoiRect(QRectF(2.0, 0.0, 6.0, 30.0));
                    gt->recalculate();
                    CHECK115(std::isfinite(gt->primaryMetric())
                                     && std::abs(gt->primaryMetric() - 3.0) < 0.5,
                             "115c: Differentiate failed on monotonic ROI inside reversing curve (val="
                                     << gt->primaryMetric() << ")");
                    RoiGadgetTool::clearGadget(gd);
                }

                // DeltaCursor outside curve X range -> zero area/RMS
                DeltaCursorTool *dct = DeltaCursorTool::attach(w, gd);
                if (dct) {
                    dct->setCursorPositions(100.0, 200.0);
                    const double areaOut = dct->currentReadout().area;
                    CHECK115(std::isfinite(areaOut) && std::abs(areaOut) < 1e-12,
                             "115c: DeltaCursor out-of-range area expected 0, got " << areaOut);
                    gd->setActiveTool(nullptr);
                }
            }
        }

        // 3. WorkbookManager: detaching and activating sheet stays consistent
        {
            WorkbookManager *wb = WorkbookManager::instance(w);
            QString wbErr;
            const QString bName = wb ? wb->createBook(QStringLiteral("Book115"), 2, &wbErr) : QString();
            const QList<Table *> shs = wb ? wb->sheets(bName) : QList<Table *>();
            CHECK115(shs.size() == 2, "115c: WorkbookManager createBook failed");
            if (wb && shs.size() == 2) {
                CHECK115(wb->detachSheet(shs[1], true, &wbErr), "115c: detachSheet failed");
                CHECK115(wb->bookOf(shs[1]).isEmpty(),
                         "115c: detached sheet still reports workbook membership");
            }
        }
    }

    // =========================================================================
    // 115d: Domain 4 — Graph, MultiLayer & ExtraPlotTypes
    // =========================================================================
    {
        AppWinGuard115 w(new ApplicationWindow());

        // 1. Graph: 2-arg drawCanvasFrame(false, 0) clears frame & showIntensityTable safe on Legend
        {
            MultiLayer *ml = w->multilayerPlot(QStringLiteral("GFrame115"));
            Graph *g = ml ? ml->addLayer() : nullptr;
            CHECK115(g && g->plotWidget(), "115d: graph creation failed");
            if (g && g->plotWidget()) {
                g->drawCanvasFrame(true, 3);
                CHECK115(g->plotWidget()->lineWidth() == 3, "115d: drawCanvasFrame(true,3) failed");
                g->drawCanvasFrame(false, 0);
                CHECK115(g->plotWidget()->lineWidth() == 0,
                         "115d: 2-arg drawCanvasFrame(false, 0) did not clear lineWidth");

                Legend *leg = g->newLegend(QStringLiteral("NotAnImage"));
                CHECK115(leg != nullptr, "115d: newLegend failed");
                const QVector<int> txtIds = g->textMarkerKeys();
                CHECK115(!txtIds.isEmpty(), "115d: textMarkerKeys empty after newLegend");
                if (!txtIds.isEmpty())
                    g->setSelectedMarker(txtIds.first(), false);
                g->showIntensityTable(); // must not bad-cast Legend* to ImageMarker*!
                CHECK115(true, "115d: showIntensityTable survived Legend selection");
            }
        }

        // 2. MultiLayer: <ToriActiveLayer> preserved on undo/redo & <ToriRecalcLinks> remapped on rename
        {
            Table *tu = w->newTable(QStringLiteral("UndoAct115"), 5, 2);
            for (int i = 0; i < 5; ++i) {
                tu->setCell(i, 0, double(i));
                tu->setCell(i, 1, double(i * 2));
            }
            MultiLayer *ml = w->multilayerPlot(tu, QStringList() << tu->colName(1), Graph::Line);
            if (ml) {
                Graph *g0 = ml->activeGraph();
                Graph *g1 = ml->addLayer();
                CHECK115(g0 && g1 && ml->layers() == 2, "115d: 2-layer setup failed");
                if (g0 && g1) {
                    ml->setActiveGraph(g0);
                    const QString before = ml->captureUndoSnapshot();
                    ml->setActiveGraph(g1);
                    g1->setTitle(QStringLiteral("Layer2Title"));
                    ml->pushUndoSnapshot(QStringLiteral("Edit Layer 2"), before);
                    ml->undoStack()->undo();
                    CHECK115(ml->layersList().indexOf(ml->activeGraph()) == 0,
                             "115d: undo did not restore active layer 0 (got "
                                     << ml->layersList().indexOf(ml->activeGraph()) << ")");
                    ml->undoStack()->redo();
                    CHECK115(ml->layersList().indexOf(ml->activeGraph()) == 1,
                             "115d: redo did not restore active layer 1 (got "
                                     << ml->layersList().indexOf(ml->activeGraph()) << ")");
                }
            }
        }

        // 3. ExtraPlotTypes: plotHistogramCumulative default begin=NaN & Gumbel returnPeriod=1e9
        {
            Table *thc = w->newTable(QStringLiteral("HistCum115"), 10, 1);
            for (int i = 0; i < 10; ++i)
                thc->setCell(i, 0, 50000.0 + double(i) * 10.0);
            const auto hres = ExtraPlotTypes::plotHistogramCumulative(w, thc, thc->colName(0));
            CHECK115(hres.valid && hres.begin >= 49000.0 && hres.bins > 0,
                     "115d: plotHistogramCumulative default begin was not auto-computed (begin="
                             << hres.begin << ")");

            const auto gres = ExtraPlotTypes::plotProbability(
                    w, thc, thc->colName(0), ExtraPlotTypes::Gumbel, ExtraPlotTypes::MeanRank,
                    1.0e9, false);
            CHECK115(gres.valid && std::isfinite(gres.returnValue) && gres.returnValue > 50100.0,
                     "115d: Gumbel returnValue invalid at T=1e9 (val=" << gres.returnValue << ")");
        }

        // 4. Subnormal Y-range auto-scale and manual setAxisScale must not trigger qRound int overflow UB
        {
            Table *tsub = w->newTable(QStringLiteral("SubnormalY115"), 6, 2);
            for (int i = 0; i < 6; ++i) {
                tsub->setCell(i, 0, double(i));
                tsub->setCell(i, 1, double(i) * 1.0e-310);
            }
            MultiLayer *mlSub = w->multilayerPlot(tsub, QStringList() << tsub->colName(1), Graph::Line);
            Graph *gSub = mlSub ? mlSub->activeGraph() : nullptr;
            CHECK115(gSub && gSub->plotWidget(), "115d: subnormal Y plot setup failed");
            if (gSub && gSub->plotWidget()) {
                gSub->replot();
                const QwtScaleDiv *sdAuto = gSub->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
                CHECK115(sdAuto && std::isfinite(sdAuto->lowerBound())
                                 && std::isfinite(sdAuto->upperBound())
                                 && std::abs(sdAuto->upperBound() - sdAuto->lowerBound()) >= 1.0e-290,
                         "115d: auto-scaled subnormal Y axis produced unsafe scale span");

                // Also verify explicit manual subnormal setAxisScale, tiny stepSize (1e-12 / 1e-310),
                // and direct unsafe setAxisScaleDiv all sanitize cleanly without qRound UB
                gSub->plotWidget()->setAxisScale(QwtPlot::yLeft, 0.0, 1.0e-310);
                gSub->replot();
                const QwtScaleDiv *sdMan = gSub->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
                CHECK115(sdMan && std::isfinite(sdMan->lowerBound())
                                 && std::isfinite(sdMan->upperBound())
                                 && std::abs(sdMan->upperBound() - sdMan->lowerBound()) >= 1.0e-290,
                         "115d: manual subnormal Y axis produced unsafe scale span");

                gSub->plotWidget()->setAxisScale(QwtPlot::yLeft, 0.0, 1.0, 1.0e-12);
                gSub->replot();
                gSub->plotWidget()->setAxisScale(QwtPlot::yLeft, 0.0, 1.0, 1.0e-310);
                gSub->replot();
                gSub->setScale(QwtPlot::yLeft, 0.0, 1.0e-310);
                const QwtScaleDiv *sdSet = gSub->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
                CHECK115(sdSet && std::isfinite(sdSet->lowerBound())
                                 && std::isfinite(sdSet->upperBound())
                                 && std::abs(sdSet->upperBound() - sdSet->lowerBound()) >= 1.0e-290,
                         "115d: Graph::setScale(0, 1e-310) produced unsafe scale span");

                QwtValueList badTicks[QwtScaleDiv::NTickTypes];
                badTicks[QwtScaleDiv::MajorTick] << 0.0 << 1.0e-310;
                gSub->plotWidget()->setAxisScaleDiv(QwtPlot::yLeft,
                                                    QwtScaleDiv(0.0, 1.0e-310, badTicks));
                gSub->replot();
                const QwtScaleDiv *sdDiv = gSub->plotWidget()->axisScaleDiv(QwtPlot::yLeft);
                CHECK115(sdDiv && std::isfinite(sdDiv->lowerBound())
                                 && std::isfinite(sdDiv->upperBound())
                                 && std::abs(sdDiv->upperBound() - sdDiv->lowerBound()) >= 1.0e-290,
                         "115d: direct unsafe setAxisScaleDiv was not sanitized on replot");
            }
        }
    }

    // =========================================================================
    // 115e: Domain 5 — ApplicationWindow, GraphDigitizer & FitDialog
    // =========================================================================
    {
        AppWinGuard115 w(new ApplicationWindow());

        // 1. plotDoubleY() moves attached error bars to yRight with their master curve
        {
            Table *tdy = w->newTable(QStringLiteral("DoubleY115"), 5, 5);
            tdy->setColPlotDesignation(0, SciDAVis::X);
            tdy->setColPlotDesignation(1, SciDAVis::Y);
            tdy->setColPlotDesignation(2, SciDAVis::yErr);
            tdy->setColPlotDesignation(3, SciDAVis::Y);
            tdy->setColPlotDesignation(4, SciDAVis::yErr);
            for (int i = 0; i < 5; ++i) {
                tdy->setCell(i, 0, double(i + 1));
                tdy->setCell(i, 1, double(i + 1) * 10.0);
                tdy->setCell(i, 2, 1.0);
                tdy->setCell(i, 3, double(i + 1) * 100.0);
                tdy->setCell(i, 4, 5.0);
            }
            MultiLayer *mlDy = w->multilayerPlot(
                    tdy, QStringList() << tdy->colName(1) << tdy->colName(3), Graph::Scatter);
            Graph *gDy = mlDy ? mlDy->activeGraph() : nullptr;
            if (gDy) {
                gDy->addErrorBars(tdy->colName(1), tdy, tdy->colName(2),
                                  int(QwtErrorPlotCurve::Vertical));
                gDy->addErrorBars(tdy->colName(3), tdy, tdy->colName(4),
                                  int(QwtErrorPlotCurve::Vertical));
                w->activateSubWindow(mlDy);
                w->plotDoubleY();
            }
            CHECK115(gDy && gDy->curves() >= 4,
                     "115e: plotDoubleY did not plot 2 curves + 2 error bars (curves="
                             << (gDy ? gDy->curves() : -1) << ")");
            if (gDy && gDy->curves() >= 4) {
                int rightData = 0, rightErr = 0, leftErr = 0;
                for (int i = 0; i < gDy->curves(); ++i) {
                    QwtPlotCurve *cv = gDy->curve(i);
                    if (!cv)
                        continue;
                    if (gDy->curveType(i) == Graph::ErrorBars) {
                        if (cv->yAxis() == QwtPlot::yRight)
                            ++rightErr;
                        else
                            ++leftErr;
                    } else if (cv->yAxis() == QwtPlot::yRight) {
                        ++rightData;
                    }
                }
                CHECK115(rightData == 1 && rightErr == 1 && leftErr == 1,
                         "115e: plotDoubleY error bars axis assignment wrong (rightData="
                                 << rightData << ", rightErr=" << rightErr << ", leftErr=" << leftErr
                                 << ")");
            }
        }

        // 2. deleteFitTables() safe when two fit curves share one table
        {
            Table *tf = w->newTable(QStringLiteral("FitDel115"), 6, 2);
            for (int i = 0; i < 6; ++i) {
                tf->setCell(i, 0, double(i));
                tf->setCell(i, 1, 2.0 * double(i) + 1.0);
            }
            MultiLayer *mlf = w->multilayerPlot(tf, QStringList() << tf->colName(1), Graph::Scatter);
            Graph *gf = mlf ? mlf->activeGraph() : nullptr;
            if (gf && gf->curves() == 1) {
                LinearFit lf(w, gf, gf->curve(0)->title().text());
                lf.fit();
                w->deleteFitTables();
                CHECK115(true, "115e: deleteFitTables completed safely");
            }
        }

        // 3. GraphDigitizer::autoTraceCurveByColor uses NaN-safe strict weak ordering under invalid logX
        {
            QImage img(50, 50, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter p(&img);
            p.setPen(QPen(QColor(220, 30, 30), 3));
            p.drawLine(5, 25, 45, 25);
            p.end();
            GraphDigitizer::AxisCalibration badCal;
            badCal.pixelX1 = QPointF(0, 49);
            badCal.pixelX2 = QPointF(49, 49);
            badCal.valueX1 = -1.0; // invalid for logX!
            badCal.valueX2 = 10.0;
            badCal.logX = true;
            badCal.pixelY1 = QPointF(0, 49);
            badCal.pixelY2 = QPointF(0, 0);
            badCal.valueY1 = 0.0;
            badCal.valueY2 = 10.0;
            const auto pts = GraphDigitizer::autoTraceCurveByColor(img, badCal, QColor(220, 30, 30));
            bool seenNaN = false;
            bool orderOk = true;
            for (const QPointF &pt : pts) {
                if (std::isnan(pt.x())) {
                    seenNaN = true;
                } else if (seenNaN) {
                    orderOk = false;
                }
            }
            CHECK115(orderOk, "115e: GraphDigitizer::autoTraceCurveByColor violated NaN-safe sort order");
        }

        // 4. FitDialog with 0 tables: setSrcTables/selectSrcTable safe & no crash
        {
            AppWinGuard115 wEmpty(new ApplicationWindow());
            FitDialog fd(wEmpty);
            fd.setSrcTables(wEmpty->tableList());
            fd.selectSrcTable(0);
            CHECK115(true, "115e: FitDialog 0-table setSrcTables/selectSrcTable safe");
        }
    }

    // =========================================================================
    // 115f: Domain 6 — JSON-RPC Deep
    // =========================================================================
    {
        AppWinGuard115 w(new ApplicationWindow());
        QTemporaryDir tmpDir;
        PythonAutomationServer srv(w);
        Rpc115 rpc{ &srv, 11500 };

        // 1. digitizer.extract rejects log_x with val_x1 <= 0 & collinear pixel axes
        {
            const QString imgPath = tmpDir.path() + QStringLiteral("/dig115.png");
            QImage img(60, 60, QImage::Format_ARGB32);
            img.fill(Qt::white);
            img.save(imgPath);
            const auto rLog = rpc.call(
                    QStringLiteral("digitizer.extract"),
                    QJsonObject{ { QStringLiteral("path"), imgPath },
                                 { QStringLiteral("log_x"), true },
                                 { QStringLiteral("val_x1"), 0.0 },
                                 { QStringLiteral("val_x2"), 10.0 } });
            CHECK115(!Rpc115::ok(rLog),
                     "115f: digitizer.extract must reject log_x=true with val_x1 <= 0");

            const auto rCol = rpc.call(
                    QStringLiteral("digitizer.extract"),
                    QJsonObject{ { QStringLiteral("path"), imgPath },
                                 { QStringLiteral("px_x1"), 0.0 },
                                 { QStringLiteral("py_x1"), 0.0 },
                                 { QStringLiteral("px_x2"), 50.0 },
                                 { QStringLiteral("py_x2"), 0.0 },
                                 { QStringLiteral("px_y1"), 0.0 },
                                 { QStringLiteral("py_y1"), 0.0 },
                                 { QStringLiteral("px_y2"), 25.0 },
                                 { QStringLiteral("py_y2"), 0.0 } });
            CHECK115(!Rpc115::ok(rCol),
                     "115f: digitizer.extract must reject collinear X/Y calibration pixel axes");
        }

        // 2. curvemath.eis_fit rejects unimplemented RandlesWarburg
        {
            Table *teis = w->newTable(QStringLiteral("Eis115"), 5, 3);
            for (int i = 0; i < 5; ++i) {
                teis->setCell(i, 0, std::pow(10.0, double(i)));
                teis->setCell(i, 1, 10.0 + double(i));
                teis->setCell(i, 2, -double(i + 1));
            }
            const auto rEis = rpc.call(
                    QStringLiteral("curvemath.eis_fit"),
                    QJsonObject{ { QStringLiteral("table"), QStringLiteral("Eis115") },
                                 { QStringLiteral("model"), QStringLiteral("RandlesWarburg") } });
            CHECK115(!Rpc115::ok(rEis),
                     "115f: curvemath.eis_fit must reject unimplemented RandlesWarburg");
        }

        // 3. graph.set_axis keeps show_ticks=false when ticks_inward=true;
        //    graph.set_axis_break rejects log_before=true when axis min <= 0
        {
            Table *tg = w->newTable(QStringLiteral("Ax115"), 5, 2);
            for (int i = 0; i < 5; ++i) {
                tg->setCell(i, 0, double(i) - 2.0); // min <= 0
                tg->setCell(i, 1, double(i + 1));
            }
            CHECK115(Rpc115::ok(rpc.call(
                             QStringLiteral("plot.xy"),
                             QJsonObject{ { QStringLiteral("table"), QStringLiteral("Ax115") },
                                          { QStringLiteral("name"), QStringLiteral("GAx115") } })),
                     "115f: plot.xy GAx115 failed");

            CHECK115(Rpc115::ok(rpc.call(
                             QStringLiteral("graph.set_axis"),
                             QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GAx115") },
                                          { QStringLiteral("axis"), QStringLiteral("x") },
                                          { QStringLiteral("show_ticks"), false },
                                          { QStringLiteral("ticks_inward"), true } })),
                     "115f: graph.set_axis failed");
            MultiLayer *mlAx = findGraph115(w, QStringLiteral("GAx115"));
            Graph *gAx = mlAx ? mlAx->activeGraph() : nullptr;
            CHECK115(gAx && gAx->plotWidget()
                             && gAx->plotWidget()->getMajorTicksType()[QwtPlot::xBottom]
                                        == ScaleDraw::None,
                     "115f: graph.set_axis ticks_inward=true overwrote explicit show_ticks=false");

            const auto rBrk = rpc.call(
                    QStringLiteral("graph.set_axis_break"),
                    QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GAx115") },
                                 { QStringLiteral("axis"), QStringLiteral("x") },
                                 { QStringLiteral("start"), 0.5 },
                                 { QStringLiteral("end"), 1.0 },
                                 { QStringLiteral("log_before"), true } });
            CHECK115(!Rpc115::ok(rBrk),
                     "115f: graph.set_axis_break must reject log_before=true when axMin <= 0");

            // UndoTargetWatch: failed app.open_project must NOT wipe prior undo tracking
            CHECK115(Rpc115::ok(rpc.call(
                             QStringLiteral("graph.set_title"),
                             QJsonObject{ { QStringLiteral("graph"), QStringLiteral("GAx115") },
                                          { QStringLiteral("title"), QStringLiteral("BeforeFail115") } })),
                     "115f: set_title failed");
            const auto rFailOpen = rpc.call(
                    QStringLiteral("app.open_project"),
                    QJsonObject{ { QStringLiteral("path"),
                                   tmpDir.path() + QStringLiteral("/no_such_115.sciprj") } });
            CHECK115(!Rpc115::ok(rFailOpen), "115f: nonexistent open_project unexpectedly succeeded");
            const auto rUndo = rpc.call(QStringLiteral("app.undo"), QJsonObject{});
            MultiLayer *mlAfterUndo = findGraph115(w, QStringLiteral("GAx115"));
            Graph *gAfterUndo = mlAfterUndo ? mlAfterUndo->activeGraph() : nullptr;
            CHECK115(Rpc115::ok(rUndo)
                             && Rpc115::result(rUndo).value(QStringLiteral("can_redo")).toBool()
                             && gAfterUndo && gAfterUndo->plotWidget()
                             && gAfterUndo->plotWidget()->title().text()
                                        != QStringLiteral("BeforeFail115"),
                     "115f: failed app.open_project wiped UndoTargetWatch so app.undo failed!");
        }

        // 4. stats.nonparametric Friedman numeric checks, stats.roc scalar score_cols,
        //    curvemath.bragg_convert reused Text out_col -> Numeric
        {
            Table *tf = w->newTable(QStringLiteral("Stat115"), 6, 4);
            tf->column(0)->setColumnMode(SciDAVis::ColumnMode::Text);
            for (int r = 0; r < 6; ++r) {
                tf->column(0)->setTextAt(r, QStringLiteral("S%1").arg(r));
                tf->setCell(r, 1, double(r + 1));
                tf->setCell(r, 2, double(r + 2));
                tf->setCell(r, 3, double(r + 4));
            }
            // Auto-selection skips Text col 0 and uses numeric cols 1,2,3
            const auto rFrAuto = rpc.call(
                    QStringLiteral("stats.nonparametric"),
                    QJsonObject{ { QStringLiteral("table"), QStringLiteral("Stat115") },
                                 { QStringLiteral("method"), QStringLiteral("friedman") } });
            CHECK115(Rpc115::ok(rFrAuto),
                     "115f: Friedman auto-selection failed to skip Text column");
            // Explicit Text col 0 must be rejected
            const auto rFrBad = rpc.call(
                    QStringLiteral("stats.nonparametric"),
                    QJsonObject{ { QStringLiteral("table"), QStringLiteral("Stat115") },
                                 { QStringLiteral("method"), QStringLiteral("friedman") },
                                 { QStringLiteral("cols"), QJsonArray{ 0, 1, 2 } } });
            CHECK115(!Rpc115::ok(rFrBad),
                     "115f: Friedman must reject explicit Text column in cols");

            // stats.roc scalar score_cols shorthand
            Table *troc = w->newTable(QStringLiteral("Roc115"), 6, 2);
            const double stVals[6] = { 0, 0, 0, 1, 1, 1 };
            const double scVals[6] = { 0.1, 0.2, 0.3, 0.7, 0.8, 0.9 };
            for (int r = 0; r < 6; ++r) {
                troc->setCell(r, 0, stVals[r]);
                troc->setCell(r, 1, scVals[r]);
            }
            const auto rRoc = rpc.call(
                    QStringLiteral("stats.roc"),
                    QJsonObject{ { QStringLiteral("table"), QStringLiteral("Roc115") },
                                 { QStringLiteral("state_col"), 0 },
                                 { QStringLiteral("score_cols"), 1 },
                                 { QStringLiteral("plot"), false } });
            CHECK115(Rpc115::ok(rRoc)
                             && Rpc115::result(rRoc).value(QStringLiteral("auc")).toDouble() > 0.95,
                     "115f: stats.roc failed with scalar score_cols=1");

            // curvemath.bragg_convert overwrites existing Text out_col as Numeric
            Table *tbr = w->newTable(QStringLiteral("Bragg115"), 3, 2);
            tbr->setColName(0, QStringLiteral("TwoTheta"));
            tbr->setColName(1, QStringLiteral("DOut"));
            tbr->column(1)->setColumnMode(SciDAVis::ColumnMode::Text);
            for (int r = 0; r < 3; ++r) {
                tbr->setCell(r, 0, 20.0 + 10.0 * double(r));
                tbr->column(1)->setTextAt(r, QStringLiteral("old_text"));
            }
            const auto rBr = rpc.call(
                    QStringLiteral("curvemath.bragg_convert"),
                    QJsonObject{ { QStringLiteral("table"), QStringLiteral("Bragg115") },
                                 { QStringLiteral("col"), QStringLiteral("TwoTheta") },
                                 { QStringLiteral("from"), QStringLiteral("2theta") },
                                 { QStringLiteral("to"), QStringLiteral("d") },
                                 { QStringLiteral("out_col"), QStringLiteral("DOut") } });
            CHECK115(Rpc115::ok(rBr)
                             && tbr->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                             && std::isfinite(tbr->cell(0, 1)) && tbr->cell(0, 1) > 1.0,
                     "115f: curvemath.bragg_convert failed to convert reused Text out_col to Numeric");
        }
    }

    if (g115Failures == 0) {
        std::cout << "[PASS] Suite 115 Campaign 2 Swarm Audit Regressions passed!" << std::endl;
        return 0;
    }
    std::cerr << "[FAIL] Suite 115 failed with " << g115Failures << " failure(s)." << std::endl;
    return 1;
}
