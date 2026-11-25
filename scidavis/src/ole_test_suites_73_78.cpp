#include "ole_test_common.h"

// =========================================================================================
// Suite 73 (E8): "Send to PowerPoint" from the GUI. A self-written ZIP writer (no Qt private
// API) packages the graphs as ToriGraph.Graph.1 OLE objects with an EMF preview, laid out like
// the Python SDK's export_graphs_to_pptx. PowerPoint itself is never started.
// =========================================================================================
namespace {

struct Pptx73Shape
{
    QString name, descr, progId;
    qint64 x = -1, y = -1, w = -1, h = -1;
};

QString xmlError73(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd())
        r.readNext();
    return r.hasError() ? r.errorString() : QString();
}

QVector<Pptx73Shape> slideShapes73(const QByteArray &xml)
{
    QVector<Pptx73Shape> out;
    QXmlStreamReader r(xml);
    bool inFrame = false;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const QStringRef n = r.name();
            if (n == QLatin1String("graphicFrame")) {
                out.append(Pptx73Shape());
                inFrame = true;
            } else if (inFrame) {
                Pptx73Shape &s = out.last();
                if (n == QLatin1String("cNvPr") && s.name.isEmpty()) {
                    s.name = r.attributes().value(QLatin1String("name")).toString();
                    s.descr = r.attributes().value(QLatin1String("descr")).toString();
                } else if (n == QLatin1String("oleObj")) {
                    s.progId = r.attributes().value(QLatin1String("progId")).toString();
                } else if (n == QLatin1String("off") && s.x < 0) {
                    s.x = r.attributes().value(QLatin1String("x")).toLongLong();
                    s.y = r.attributes().value(QLatin1String("y")).toLongLong();
                } else if (n == QLatin1String("ext") && s.w < 0) {
                    s.w = r.attributes().value(QLatin1String("cx")).toLongLong();
                    s.h = r.attributes().value(QLatin1String("cy")).toLongLong();
                }
            }
        } else if (r.isEndElement() && r.name() == QLatin1String("graphicFrame")) {
            inFrame = false;
        }
    }
    return out;
}

} // namespace

int runSuite73SendToPowerPoint(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 73: Send to PowerPoint (.pptx writer)...\n";
    const QString tmpDir = QDir::temp().filePath(
            QStringLiteral("torigraph_suite73_%1").arg(QCoreApplication::applicationPid()));
    // restores the non-interactive flag and removes the decks also when a check fails;
    // TORIGRAPH_SUITE73_KEEP=1 keeps the decks for a manual look (python-pptx, LibreOffice, ...)
    struct S73Restore {
        QVariant prevNonInteractive;
        QString dir;
        ~S73Restore()
        {
            if (qEnvironmentVariableIsSet("TORIGRAPH_SUITE73_KEEP"))
                std::cout << "[INFO] Suite 73 decks kept in " << dir.toStdString() << "\n";
            else
                QDir(dir).removeRecursively();
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        }
    } s73Restore{ qApp->property("torigraph_noninteractive"), tmpDir };
    qApp->setProperty("torigraph_noninteractive", true);
    QDir(tmpDir).removeRecursively();
    QDir().mkpath(tmpDir);

    // ---- 73a. menu entry and Japanese text ------------------------------------------------
    {
        QAction *a = mw->findChild<QAction *>(QStringLiteral("actionSendToPowerPoint"));
        CHECK_COND(a, "73a: actionSendToPowerPoint missing");
        QAction *aSave = mw->findChild<QAction *>(QStringLiteral("actionSaveProject"));
        bool inFileMenu = false;
        for (QMenu *m : mw->findChildren<QMenu *>()) {
            if (m->actions().contains(a) && m->actions().contains(aSave))
                inFileMenu = true;
        }
        CHECK_COND(inFileMenu, "73a: Send to PowerPoint is not in the File menu");
        const QString prevLang = mw->appLanguage;
        mw->switchToLanguage("ja");
        QApplication::processEvents();
        CHECK_COND(a->text() == QString::fromUtf8("PowerPoint へ送る(&W)..."),
                   "73a: Japanese menu text: " << a->text().toStdString());
        CHECK_COND(a->toolTip().contains(QString::fromUtf8("ダブルクリック")),
                   "73a: tooltip does not explain re-editing");
        bool inRibbon = false;
        for (QAction *ra : mw->findChildren<QAction *>()) {
            if (ra != a && ra->text() == ToriGraphPptx::sendToPowerPointLabel())
                inRibbon = true;
        }
        CHECK_COND(inRibbon, "73a: ribbon 'More Export' entry missing");
        CHECK_COND(QCoreApplication::translate("ToriGraphPptx", "Graphs per slide:")
                           == QString::fromUtf8("1 枚に並べる数:"),
                   "73a: dialog strings are not translated");
        mw->switchToLanguage(prevLang.isEmpty() ? QStringLiteral("en") : prevLang);
        QApplication::processEvents();
    }

    // ---- 73b. ZipWriter round trip with ZipReader -----------------------------------------
    {
        CHECK_COND(torizip::ZipWriter::crc32(QByteArray("123456789")) == 0xCBF43926U,
                   "73b: CRC-32 check value");
        torizip::ZipWriter zw;
        QByteArray big;
        for (int i = 0; i < 2000; ++i)
            big += "ToriGraph ZIP line " + QByteArray::number(i % 7) + "\n";
        const QByteArray tiny("x");
        QString err;
        CHECK_COND(zw.addFile("a/big.txt", big, torizip::ZipWriter::Compression::Deflate, &err),
                   "73b: add big");
        CHECK_COND(zw.addFile("stored.bin", tiny, torizip::ZipWriter::Compression::Store, &err),
                   "73b: add stored");
        CHECK_COND(zw.addFile(QString::fromUtf8("日本語/名前.xml"), QByteArray("<a/>"),
                              torizip::ZipWriter::Compression::Deflate, &err),
                   "73b: add UTF-8 name");
        CHECK_COND(zw.addFile("empty.txt", QByteArray(), torizip::ZipWriter::Compression::Deflate,
                              &err),
                   "73b: add empty");
        err.clear();
        CHECK_COND(!zw.addFile("A/BIG.TXT", tiny, torizip::ZipWriter::Compression::Store, &err)
                           && !err.isEmpty(),
                   "73b: duplicate names must be refused");
        err.clear();
        CHECK_COND(!zw.addFile(QString(70000, QLatin1Char('n')), tiny,
                               torizip::ZipWriter::Compression::Store, &err)
                           && !err.isEmpty(),
                   "73b: names over 65535 bytes must be refused");
        const QByteArray zip = zw.finish();
        CHECK_COND(zip.size() < big.size(), "73b: deflate did not compress");
        torizip::ZipArchive za;
        CHECK_COND(za.openData(zip, &err), "73b: ZipReader cannot open: " << err.toStdString());
        CHECK_COND(za.entries().size() == 4, "73b: entry count");
        const QList<QByteArray> expect{ big, tiny, QByteArray("<a/>"), QByteArray() };
        for (int i = 0; i < 4; ++i) {
            QByteArray data;
            CHECK_COND(za.extract(i, data, &err), "73b: extract " << i << ": " << err.toStdString());
            CHECK_COND(data == expect.at(i), "73b: content " << i);
            CHECK_COND(za.entries().at(i).crc32 == torizip::ZipWriter::crc32(data),
                       "73b: CRC in the directory " << i);
        }
        CHECK_COND(za.entries().at(0).method == 8 && za.entries().at(1).method == 0,
                   "73b: compression methods");
        CHECK_COND(za.entries().at(2).name == QString::fromUtf8("日本語/名前.xml"),
                   "73b: UTF-8 name round trip");
    }

    // ---- set up two graphs on the lab slide preset ------------------------------------------
    ApplicationWindow *w73 = new ApplicationWindow();
    w73->applyUserSettings();
    w73->setHeadlessMode(true);
    w73->confirmCloseTable = false;
    w73->confirmClosePlot2D = false;
    w73->confirmClosePlot3D = false;
    w73->confirmCloseFolder = false;
    struct W73Guard {
        ApplicationWindow *w;
        ~W73Guard()
        {
            if (w) {
                w->savedProject();
                w->setOleEmbeddedMode(true);
                delete w;
            }
        }
    } w73Guard{ w73 };

    Table *tA = w73->newTable(6, 2, "Tensile73", "stress-strain");
    Table *tB = w73->newTable(6, 2, "Xrd73", "pattern");
    CHECK_COND(tA && tB, "73: tables");
    for (int r = 0; r < 6; ++r) {
        tA->setCell(r, 0, 0.01 * r);
        tA->setCell(r, 1, 100.0 + 40.0 * r - 3.0 * r * r);
        tB->setCell(r, 0, 20.0 + 10.0 * r);
        tB->setCell(r, 1, (r == 3) ? 900.0 : 50.0 + r);
    }
    MultiLayer *mlA =
            w73->multilayerPlot(tA, QStringList() << (tA->name() + "_2"), Graph::LineSymbols);
    MultiLayer *mlB = w73->multilayerPlot(tB, QStringList() << (tB->name() + "_2"), Graph::Line);
    CHECK_COND(mlA && mlB, "73: graphs");
    MultiLayer::GraphStylePresetSpec lab;
    CHECK_COND(MultiLayer::findStylePreset(ToriGraphPptx::labSlidePresetId(), &lab),
               "73: lab slide preset");
    mlA->applyStylePresetSpec(lab);
    mlB->applyStylePresetSpec(lab);
    QApplication::processEvents();
    const double labPct = ToriGraphPptx::presetPastePercent(ToriGraphPptx::labSlidePresetId());
    CHECK_COND(labPct == lab.slidePastePercent && labPct > 100.0,
               "73: paste factor not read from the preset: " << labPct);
    CHECK_COND(ToriGraphPptx::presetPastePercent("isij_85mm") == 100.0,
               "73: presets without a paste factor paste at 100 %");

    // ---- 73c. two graphs, one per slide --------------------------------------------------
    const QString deck = QDir(tmpDir).filePath("two_graphs.pptx");
    ToriGraphPptx::DeckOptions opt;
    opt.perSlide = 1;
    opt.scalePercent = labPct;
    const ToriGraphPptx::DeckResult res =
            ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlA << mlB, opt, deck);
    CHECK_COND(res.ok, "73c: export failed: " << res.error.toStdString());
    CHECK_COND(res.slideCount == 2 && res.newSlides == 2 && !res.appended, "73c: slide count");
    CHECK_COND(res.placements.size() == 2 && res.placements[0].slide == 1
                       && res.placements[1].slide == 2,
               "73c: placements");
    QByteArray deckBytes;
    {
        QFile f(deck);
        CHECK_COND(f.open(QIODevice::ReadOnly), "73c: pptx not written");
        deckBytes = f.readAll();
    }
    CHECK_COND(deckBytes.startsWith("PK\x03\x04"), "73c: not a ZIP file");
    torizip::ZipArchive za;
    QString err;
    CHECK_COND(za.openData(deckBytes, &err), "73c: ZipReader: " << err.toStdString());
    CHECK_COND(za.entries().first().name == "[Content_Types].xml",
               "73c: [Content_Types].xml must be the first entry");
    QHash<QString, QByteArray> parts;
    for (int i = 0; i < za.entries().size(); ++i) {
        const QString n = za.entries().at(i).name;
        QByteArray data;
        CHECK_COND(za.extract(i, data, &err),
                   "73c: extract " << n.toStdString() << ": " << err.toStdString());
        CHECK_COND(za.entries().at(i).crc32 == torizip::ZipWriter::crc32(data),
                   "73c: CRC of " << n.toStdString());
        parts.insert(n, data);
        if (n.endsWith(".xml") || n.endsWith(".rels")) {
            const QString xe = xmlError73(data);
            CHECK_COND(xe.isEmpty(),
                       "73c: " << n.toStdString() << " is not well-formed: " << xe.toStdString());
        }
    }
    for (const char *req :
         { "[Content_Types].xml", "_rels/.rels", "ppt/presentation.xml",
           "ppt/_rels/presentation.xml.rels", "ppt/slides/slide1.xml", "ppt/slides/slide2.xml",
           "ppt/slides/_rels/slide1.xml.rels", "ppt/slideLayouts/slideLayout1.xml",
           "ppt/slideMasters/slideMaster1.xml", "ppt/theme/theme1.xml",
           "ppt/embeddings/oleObject1.bin", "ppt/embeddings/oleObject2.bin",
           "ppt/media/image1.emf", "ppt/media/image2.emf", "docProps/core.xml" })
        CHECK_COND(parts.contains(QString::fromLatin1(req)), "73c: missing part " << req);
    const QString ct = QString::fromUtf8(parts.value("[Content_Types].xml"));
    CHECK_COND(ct.contains("PartName=\"/ppt/embeddings/oleObject1.bin\" "
                           "ContentType=\"application/vnd.openxmlformats-officedocument.oleObject\""),
               "73c: OLE content type");
    CHECK_COND(ct.contains("PartName=\"/ppt/media/image2.emf\" ContentType=\"image/x-emf\""),
               "73c: EMF content type");
    CHECK_COND(ct.contains("PartName=\"/ppt/slides/slide2.xml\""), "73c: slide content type");
    const QString pres = QString::fromUtf8(parts.value("ppt/presentation.xml"));
    CHECK_COND(pres.count("<p:sldId ") == 2, "73c: presentation lists 2 slides");
    CHECK_COND(pres.contains("<p:sldSz cx=\"12192000\" cy=\"6858000\"/>"), "73c: 16:9 slide size");
    CHECK_COND(pres.indexOf("<p:sldIdLst>") >= 0
                       && pres.indexOf("<p:sldIdLst>") < pres.indexOf("<p:sldSz"),
               "73c: sldIdLst must precede sldSz");
    const QString srels = QString::fromUtf8(parts.value("ppt/slides/_rels/slide1.xml.rels"));
    CHECK_COND(srels.contains("relationships/oleObject\" Target=\"../embeddings/oleObject1.bin\"")
                       && srels.contains("relationships/image\" Target=\"../media/image1.emf\"")
                       && srels.contains("relationships/slideLayout\""),
               "73c: slide relationships");
    // the same shape structure as the SDK (python/torigraph/pptx.py embed_ole_shape_on_slide)
    const QVector<Pptx73Shape> s1 = slideShapes73(parts.value("ppt/slides/slide1.xml"));
    CHECK_COND(s1.size() == 1 && s1[0].progId == "ToriGraph.Graph.1"
                       && s1[0].name == "ToriGraphOLE:" + mlA->name()
                       && s1[0].descr == "torigraph:" + mlA->name(),
               "73c: OLE frame naming/progId");
    const QString slide1 = QString::fromUtf8(parts.value("ppt/slides/slide1.xml"));
    CHECK_COND(slide1.contains("<p:embed/>") && slide1.contains("<a:blip r:embed=\"rId3\"/>")
                       && slide1.contains("uri=\"http://schemas.openxmlformats.org/presentationml/2006/ole\""),
               "73c: oleObj / preview picture");
    // size = page size of the preset x the preset's factor
    const QByteArray emf1 = parts.value("ppt/media/image1.emf");
    CHECK_COND(emf1.size() > 88 && emf1.mid(40, 4) == QByteArray(" EMF"), "73c: EMF preview");
    const qint32 *hdr = reinterpret_cast<const qint32 *>(emf1.constData());
    const qint64 frameWHm = qint64(hdr[8]) - hdr[6]; // rclFrame.right - rclFrame.left
    CHECK_COND(std::llabs(frameWHm - qRound(lab.pageWidthMm * 100.0)) <= 30,
               "73c: EMF frame is not the preset page width: " << frameWHm);
    const qint64 expectW = std::llround(double(frameWHm) * 360.0 * labPct / 100.0);
    CHECK_COND(std::llabs(s1[0].w - expectW) <= 2 && !res.placements[0].shrunk,
               "73c: width on slide " << s1[0].w << " expected " << expectW);
    CHECK_COND(std::fabs(res.placements[0].effectivePercent - labPct) < 0.1,
               "73c: effective percent " << res.placements[0].effectivePercent);
    CHECK_COND(s1[0].x > 0 && s1[0].x + s1[0].w < ToriGraphPptx::kSlideWidth16x9 && s1[0].y > 0
                       && s1[0].y + s1[0].h < ToriGraphPptx::kSlideHeight,
               "73c: graph not on the slide");
    // the embedded OLE object: CLSID, streams, and the graph with its own data
    {
        const QByteArray bin = parts.value("ppt/embeddings/oleObject1.bin");
        CHECK_COND(bin.startsWith(QByteArray::fromHex("D0CF11E0A1B11AE1")), "73c: not an OLE2 file");
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, SIZE_T(bin.size()));
        CHECK_COND(hg, "73c: GlobalAlloc");
        memcpy(GlobalLock(hg), bin.constData(), size_t(bin.size()));
        GlobalUnlock(hg);
        Microsoft::WRL::ComPtr<ILockBytes> lb;
        CHECK_COND(SUCCEEDED(CreateILockBytesOnHGlobal(hg, TRUE, &lb)), "73c: ILockBytes");
        Microsoft::WRL::ComPtr<IStorage> stg;
        CHECK_COND(SUCCEEDED(StgOpenStorageOnILockBytes(lb.Get(), nullptr,
                                                        STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr,
                                                        0, &stg)),
                   "73c: OLE storage does not open");
        CLSID clsid{};
        CHECK_COND(SUCCEEDED(ReadClassStg(stg.Get(), &clsid))
                           && IsEqualCLSID(clsid, SciDAVisOle::graph_clsid),
                   "73c: CLSID is not ToriGraph.Graph.1");
        for (const wchar_t *sn : { L"\001Ole", L"\001CompObj", L"ToriGraphProject",
                                   L"SciDAVisProject", L"SciDAVisMeta", L"SciDAVisEmf" }) {
            Microsoft::WRL::ComPtr<IStream> st;
            CHECK_COND(SUCCEEDED(stg->OpenStream(sn, nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0,
                                                 &st)),
                       "73c: OLE stream missing: " << QString::fromWCharArray(sn).toStdString());
        }
        SciDAVisOle::OleGraphPayload payload;
        CHECK_COND(SUCCEEDED(SciDAVisOle::loadPayloadFromStorage(stg.Get(), &payload)),
                   "73c: payload");
        const QString prj = QString::fromUtf8(payload.projectBytes);
        CHECK_COND(payload.targetWindow == mlA->name() && prj.contains("Tensile73")
                           && !prj.contains("Xrd73"),
                   "73c: embedded project is not the graph with its own table");
    }

    // ---- 73d. per slide 2 / 4 and fit mode --------------------------------------------------
    {
        const QString deck2 = QDir(tmpDir).filePath("one_slide.pptx");
        ToriGraphPptx::DeckOptions o2 = opt;
        o2.perSlide = 2;
        const ToriGraphPptx::DeckResult r2 =
                ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlA << mlB, o2, deck2);
        CHECK_COND(r2.ok && r2.slideCount == 1 && r2.newSlides == 1, "73d: one slide expected");
        torizip::ZipArchive z2;
        CHECK_COND(z2.open(deck2, &err), "73d: open");
        QByteArray sx;
        CHECK_COND(z2.extract(z2.findExactEntry("ppt/slides/slide1.xml"), sx, &err), "73d: slide1");
        CHECK_COND(z2.findExactEntry("ppt/slides/slide2.xml") < 0, "73d: no second slide");
        const QVector<Pptx73Shape> sh = slideShapes73(sx);
        CHECK_COND(sh.size() == 2 && sh[0].descr == "torigraph:" + mlA->name()
                           && sh[1].descr == "torigraph:" + mlB->name(),
                   "73d: two graphs on slide 1");
        CHECK_COND(sh[0].x + sh[0].w <= sh[1].x && std::llabs(sh[0].y - sh[1].y) <= 2,
                   "73d: graphs are not side by side");
        CHECK_COND(sh[1].x + sh[1].w <= ToriGraphPptx::kSlideWidth16x9,
                   "73d: right graph off the slide");
        CHECK_COND(!r2.placements[0].shrunk, "73d: the preset size fits two per 16:9 slide");

        // four per slide: the preset factor does not fit a 2 x 2 cell, so the graphs shrink
        ToriGraphPptx::DeckOptions o4 = opt;
        o4.perSlide = 4;
        const ToriGraphPptx::DeckResult r4 = ToriGraphPptx::exportGraphs(
                w73, QList<MyWidget *>() << mlA << mlB, o4, QDir(tmpDir).filePath("four.pptx"));
        qint64 cl, ctop, cw, ch;
        ToriGraphPptx::gridCell(ToriGraphPptx::kSlideWidth16x9, ToriGraphPptx::kSlideHeight, 4, 1,
                                &cl, &ctop, &cw, &ch);
        CHECK_COND(r4.ok && r4.placements.size() == 2 && r4.placements[1].shrunk
                           && r4.placements[1].effectivePercent < labPct
                           && r4.placements[1].height <= ch && r4.placements[1].left >= cl
                           && r4.placements[1].left + r4.placements[1].width <= cl + cw,
                   "73d: shrink to the 2 x 2 cell");

        // fit mode (the SDK's default without scale=): one side equals the cell
        ToriGraphPptx::DeckOptions of = opt;
        of.scalePercent = 0.0;
        const ToriGraphPptx::DeckResult rf = ToriGraphPptx::exportGraphs(
                w73, QList<MyWidget *>() << mlA, of, QDir(tmpDir).filePath("fit.pptx"));
        ToriGraphPptx::gridCell(ToriGraphPptx::kSlideWidth16x9, ToriGraphPptx::kSlideHeight, 1, 0,
                                &cl, &ctop, &cw, &ch);
        CHECK_COND(rf.ok
                           && (std::llabs(rf.placements[0].height - ch) <= 1
                               || std::llabs(rf.placements[0].width - cw) <= 1),
                   "73d: fit mode does not fill the cell");
    }

    // ---- 73e. add slides to the end of an existing deck ----------------------------------
    {
        ToriGraphPptx::DeckOptions oa = opt;
        oa.appendToExisting = true;
        const ToriGraphPptx::DeckResult ra =
                ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlB, oa, deck);
        CHECK_COND(ra.ok && ra.appended && ra.slideCount == 3 && ra.newSlides == 1
                           && ra.placements.size() == 1 && ra.placements[0].slide == 3,
                   "73e: append: " << ra.error.toStdString());
        torizip::ZipArchive z3;
        CHECK_COND(z3.open(deck, &err), "73e: reopen");
        QHash<QString, QByteArray> p3;
        for (int i = 0; i < z3.entries().size(); ++i) {
            QByteArray d;
            CHECK_COND(z3.extract(i, d, &err), "73e: extract");
            const QString n = z3.entries().at(i).name;
            p3.insert(n, d);
            if (n.endsWith(".xml") || n.endsWith(".rels"))
                CHECK_COND(xmlError73(d).isEmpty(), "73e: " << n.toStdString() << " not well-formed");
        }
        CHECK_COND(p3.value("ppt/slides/slide1.xml") == parts.value("ppt/slides/slide1.xml")
                           && p3.value("ppt/embeddings/oleObject1.bin")
                                      == parts.value("ppt/embeddings/oleObject1.bin"),
                   "73e: existing slides must be kept unchanged");
        CHECK_COND(p3.contains("ppt/slides/slide3.xml")
                           && p3.contains("ppt/embeddings/oleObject3.bin")
                           && p3.contains("ppt/media/image3.emf"),
                   "73e: new parts");
        const QString pres3 = QString::fromUtf8(p3.value("ppt/presentation.xml"));
        CHECK_COND(pres3.count("<p:sldId ") == 3 && pres3.contains("<p:sldId id=\"258\""),
                   "73e: slide list: " << pres3.toStdString());
        const QString rels3 = QString::fromUtf8(p3.value("ppt/_rels/presentation.xml.rels"));
        QSet<QString> ids;
        int nIds = 0;
        QRegularExpressionMatchIterator it =
                QRegularExpression("Id=\"([^\"]+)\"").globalMatch(rels3);
        while (it.hasNext()) {
            ids.insert(it.next().captured(1));
            ++nIds;
        }
        CHECK_COND(ids.size() == nIds && rels3.contains("Target=\"slides/slide3.xml\""),
                   "73e: presentation relationships");
        CHECK_COND(QString::fromUtf8(p3.value("[Content_Types].xml"))
                           .contains("/ppt/slides/slide3.xml"),
                   "73e: content type of the new slide");

        // a file that is not a .pptx is reported and left alone
        const QString bogus = QDir(tmpDir).filePath("not_a_deck.pptx");
        {
            QFile f(bogus);
            CHECK_COND(f.open(QIODevice::WriteOnly), "73e: bogus");
            f.write("this is not a zip file");
        }
        const ToriGraphPptx::DeckResult rb =
                ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlA, oa, bogus);
        QFile fb(bogus);
        CHECK_COND(!rb.ok && !rb.error.isEmpty() && fb.open(QIODevice::ReadOnly)
                           && fb.readAll() == QByteArray("this is not a zip file"),
                   "73e: a broken file must be reported and kept");
    }

    // ---- 73g. deck with sections (p14) and single-quoted attributes ------------------------
    auto repack73 = [](const QHash<QString, QByteArray> &src, const QString &path) {
        torizip::ZipWriter zw;
        zw.addFile("[Content_Types].xml", src.value("[Content_Types].xml"));
        for (auto it = src.constBegin(); it != src.constEnd(); ++it)
            if (it.key() != "[Content_Types].xml")
                zw.addFile(it.key(), it.value());
        QFile f(path);
        return f.open(QIODevice::WriteOnly) && f.write(zw.finish()) > 0;
    };
    {
        QHash<QString, QByteArray> sec = parts; // the two-slide deck of 73c
        QString px = QString::fromUtf8(sec.value("ppt/presentation.xml"));
        const QString rNs = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
        CHECK_COND(px.contains("xmlns:r=\"" + rNs + "\"") && px.contains("<p:sldId id=\"257\""),
                   "73g: unexpected template");
        px.replace("xmlns:r=\"" + rNs + "\"", "xmlns:r='" + rNs + "'");
        px.replace("<p:sldId id=\"257\"", "<p:sldId id='300'");
        px.replace("</p:presentation>",
                   "<p:extLst><p:ext uri=\"{521415D9-36F7-43E2-AB2F-B90AF26B5E84}\">"
                   "<p14:sectionLst xmlns:p14='http://schemas.microsoft.com/office/powerpoint/2010/main'>"
                   "<p14:section name=\"A\" id=\"{11111111-1111-1111-1111-111111111111}\">"
                   "<p14:sldIdLst><p14:sldId id=\"256\"/></p14:sldIdLst></p14:section>"
                   "<p14:section name=\"B\" id=\"{22222222-2222-2222-2222-222222222222}\">"
                   "<p14:sldIdLst><p14:sldId id='300'/></p14:sldIdLst></p14:section>"
                   "</p14:sectionLst></p:ext></p:extLst></p:presentation>");
        CHECK_COND(xmlError73(px.toUtf8()).isEmpty(), "73g: crafted presentation.xml");
        sec.insert("ppt/presentation.xml", px.toUtf8());
        const QString secDeck = QDir(tmpDir).filePath("sections.pptx");
        CHECK_COND(repack73(sec, secDeck), "73g: write the deck with sections");
        ToriGraphPptx::DeckOptions oa = opt;
        oa.appendToExisting = true;
        const ToriGraphPptx::DeckResult rs =
                ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlB, oa, secDeck);
        CHECK_COND(rs.ok && rs.slideCount == 3 && rs.placements.size() == 1
                           && rs.placements[0].slide == 3,
                   "73g: sections must not be counted as slides: slides=" << rs.slideCount << " "
                                                                         << rs.error.toStdString());
        torizip::ZipArchive zs;
        QByteArray presOut;
        CHECK_COND(zs.open(secDeck, &err)
                           && zs.extract(zs.findExactEntry("ppt/presentation.xml"), presOut, &err),
                   "73g: reopen");
        const QString po = QString::fromUtf8(presOut);
        CHECK_COND(xmlError73(presOut).isEmpty(), "73g: presentation.xml not well-formed");
        CHECK_COND(po.contains("<p:sldId id=\"301\" r:id=\""),
                   "73g: new id must follow the single-quoted id 300: " << po.toStdString());
        CHECK_COND(po.contains("<p14:sldIdLst><p14:sldId id='300'/><p14:sldId id=\"301\"/></p14:sldIdLst>"),
                   "73g: new slide not added to the last section: " << po.toStdString());
        CHECK_COND(po.contains("<p14:sldIdLst><p14:sldId id=\"256\"/></p14:sldIdLst>"),
                   "73g: first section must be unchanged");
        CHECK_COND(po.count("<p:sldId ") == 3, "73g: main slide list");

        // broken relationships: nothing is written, the reason is reported
        QHash<QString, QByteArray> bad = parts;
        bad.insert("ppt/_rels/presentation.xml.rels", QByteArray("<Relationships xmlns=\"x\"><Rel"));
        const QString badDeck = QDir(tmpDir).filePath("broken_rels.pptx");
        CHECK_COND(repack73(bad, badDeck), "73g: write the broken deck");
        QFile bf(badDeck);
        CHECK_COND(bf.open(QIODevice::ReadOnly), "73g: broken deck");
        const QByteArray before = bf.readAll();
        bf.close();
        const ToriGraphPptx::DeckResult rbad =
                ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlB, oa, badDeck);
        CHECK_COND(bf.open(QIODevice::ReadOnly) && !rbad.ok && !rbad.error.isEmpty()
                           && bf.readAll() == before,
                   "73g: broken relationships must stop the append");
    }

    // ---- 73g2. more shapes of existing decks -------------------------------------------------
    {
        ToriGraphPptx::DeckOptions oa = opt;
        oa.appendToExisting = true;
        const QString p14Ns = "http://schemas.microsoft.com/office/powerpoint/2010/main";
        const QString rNs = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
        // appends mlB to a deck made from `parts` with presentation.xml / [Content_Types].xml
        // edited by `edit`; returns the result and the new presentation.xml / content types
        auto appendCase = [&](const QString &file,
                              const std::function<void(QString &, QString &)> &edit,
                              QString *presOut, QString *ctOut, QByteArray *fileBefore) {
            QHash<QString, QByteArray> d = parts;
            QString px = QString::fromUtf8(d.value("ppt/presentation.xml"));
            QString cx = QString::fromUtf8(d.value("[Content_Types].xml"));
            edit(px, cx);
            d.insert("ppt/presentation.xml", px.toUtf8());
            d.insert("[Content_Types].xml", cx.toUtf8());
            const QString path = QDir(tmpDir).filePath(file);
            repack73(d, path);
            QFile f(path);
            if (fileBefore && f.open(QIODevice::ReadOnly)) {
                *fileBefore = f.readAll();
                f.close();
            }
            const ToriGraphPptx::DeckResult rr =
                    ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlB, oa, path);
            torizip::ZipArchive z;
            QString e;
            QByteArray pb, cb;
            if (z.open(path, &e)) {
                z.extract(z.findExactEntry("ppt/presentation.xml"), pb, &e);
                z.extract(z.findExactEntry("[Content_Types].xml"), cb, &e);
            }
            *presOut = QString::fromUtf8(pb);
            if (ctOut)
                *ctOut = QString::fromUtf8(cb);
            return rr;
        };
        const QString secHead = "<p:extLst><p:ext uri=\"{521415D9-36F7-43E2-AB2F-B90AF26B5E84}\">"
                                "<p14:sectionLst xmlns:p14=\"" + p14Ns + "\">"
                                "<p14:section name=\"A\" id=\"{11111111-1111-1111-1111-111111111111}\">"
                                "<p14:sldIdLst><p14:sldId id=\"256\"/><p14:sldId id=\"257\"/>"
                                "</p14:sldIdLst></p14:section>";
        const QString secTail = "</p14:sectionLst></p:ext></p:extLst></p:presentation>";
        QString po, co;

        // the last section is an empty element: it is expanded, the first one is untouched
        ToriGraphPptx::DeckResult rr = appendCase(
                "empty_section.pptx",
                [&](QString &px, QString &) {
                    px.replace("</p:presentation>",
                               secHead + "<p14:section name=\"Empty\" "
                                         "id=\"{33333333-3333-3333-3333-333333333333}\" />" + secTail);
                },
                &po, nullptr, nullptr);
        CHECK_COND(rr.ok && rr.slideCount == 3 && xmlError73(po.toUtf8()).isEmpty(),
                   "73g2: empty section: " << rr.error.toStdString());
        CHECK_COND(po.contains("<p14:section name=\"Empty\" id=\"{33333333-3333-3333-3333-333333333333}\">"
                               "<p14:sldIdLst><p14:sldId id=\"258\"/></p14:sldIdLst></p14:section>")
                           && po.contains("<p14:sldId id=\"257\"/></p14:sldIdLst></p14:section>"
                                          "<p14:section name=\"Empty\""),
                   "73g2: empty last section not expanded: " << po.toStdString());

        // the last section has only <p14:extLst>: sldIdLst goes in front of it (CT_Section order)
        rr = appendCase(
                "ext_section.pptx",
                [&](QString &px, QString &) {
                    px.replace("</p:presentation>",
                               secHead + "<p14:section name=\"B\" id=\"{22222222-2222-2222-2222-222222222222}\">"
                                         "<p14:extLst><p14:ext uri=\"{X}\"/></p14:extLst></p14:section>" + secTail);
                },
                &po, nullptr, nullptr);
        CHECK_COND(rr.ok && xmlError73(po.toUtf8()).isEmpty()
                           && po.contains("{22222222-2222-2222-2222-222222222222}\"><p14:sldIdLst>"
                                          "<p14:sldId id=\"258\"/></p14:sldIdLst><p14:extLst>"),
                   "73g2: sldIdLst must precede extLst: " << po.toStdString());

        // r: declared only on a child element -> declared on the root as tgr
        rr = appendCase(
                "r_on_child.pptx",
                [&](QString &px, QString &) {
                    px.replace(" xmlns:r=\"" + rNs + "\"", QString());
                    px.replace("<p:sldIdLst>", "<p:sldIdLst xmlns:r=\"" + rNs + "\">");
                    px.replace("<p:sldMasterIdLst>", "<p:sldMasterIdLst xmlns:r=\"" + rNs + "\">");
                },
                &po, nullptr, nullptr);
        const QString rootTag = po.mid(po.indexOf("<p:presentation"),
                                       po.indexOf('>', po.indexOf("<p:presentation"))
                                               - po.indexOf("<p:presentation"));
        CHECK_COND(rr.ok && xmlError73(po.toUtf8()).isEmpty()
                           && rootTag.contains("xmlns:tgr=\"" + rNs + "\"")
                           && po.contains("<p:sldId id=\"258\" tgr:id=\"rId"),
                   "73g2: r prefix must be declared on the root: " << po.toStdString());

        // Strict Open XML: refused, file unchanged
        QByteArray strictBefore;
        rr = appendCase(
                "strict.pptx",
                [&](QString &px, QString &) {
                    px.replace("http://schemas.openxmlformats.org/presentationml/2006/main",
                               "http://purl.oclc.org/ooxml/presentationml/main");
                },
                &po, nullptr, &strictBefore);
        QFile sf(QDir(tmpDir).filePath("strict.pptx"));
        CHECK_COND(!rr.ok && rr.error.contains("Strict") && sf.open(QIODevice::ReadOnly)
                           && sf.readAll() == strictBefore,
                   "73g2: Strict files must be refused and kept");

        // prefixed [Content_Types].xml with an orphan Override; an id at the ST_SlideId limit
        const QString ctNs = "http://schemas.openxmlformats.org/package/2006/content-types";
        rr = appendCase(
                "ct_prefix_maxid.pptx",
                [&](QString &px, QString &cx) {
                    px.replace("<p:sldId id=\"257\"", "<p:sldId id=\"2147483647\"");
                    cx.replace("<Types xmlns=\"" + ctNs + "\">", "<ct:Types xmlns:ct=\"" + ctNs + "\">");
                    cx.replace("<Default ", "<ct:Default ");
                    cx.replace("<Override ", "<ct:Override ");
                    cx.replace("</Types>",
                               "<ct:Override PartName=\"/ppt/slides/slide3.xml\" ContentType=\""
                               "application/vnd.openxmlformats-officedocument.presentationml.slide+xml\"/>"
                               "</ct:Types>");
                },
                &po, &co, nullptr);
        CHECK_COND(rr.ok && xmlError73(po.toUtf8()).isEmpty() && xmlError73(co.toUtf8()).isEmpty(),
                   "73g2: prefixed content types: " << rr.error.toStdString());
        CHECK_COND(po.contains("<p:sldId id=\"257\" r:id=\"") && !po.contains("<p:sldId id=\"2147483648\""),
                   "73g2: id near the limit must reuse a free id: " << po.toStdString());
        CHECK_COND(co.count("PartName=\"/ppt/slides/slide3.xml\"") == 1
                           && co.contains("<ct:Override PartName=\"/ppt/embeddings/oleObject3.bin\"")
                           && !co.contains("<Override "),
                   "73g2: content types: " << co.toStdString());
    }

    // ---- 73h. XML escaping, "%n" in names, and the picture-only fallback ----------------------
    {
        const QString odd = QString::fromUtf8("A&B <\"x\"> %2 %1 グラフ");
        ToriGraphPptx::SlideItem withOle;
        withOle.graphName = odd;
        withOle.oleBytes = parts.value("ppt/embeddings/oleObject1.bin");
        withOle.previewBytes = emf1;
        withOle.widthHm = frameWHm;
        withOle.heightHm = frameWHm;
        ToriGraphPptx::SlideItem picOnly = withOle;
        picOnly.oleBytes.clear();
        ToriGraphPptx::DeckOptions o2 = opt;
        o2.perSlide = 2;
        const QString oddDeck = QDir(tmpDir).filePath("odd_names.pptx");
        const ToriGraphPptx::DeckResult ro = ToriGraphPptx::writeDeck(
                oddDeck, QVector<ToriGraphPptx::SlideItem>() << withOle << picOnly, o2);
        CHECK_COND(ro.ok && ro.slideCount == 1, "73h: write: " << ro.error.toStdString());
        torizip::ZipArchive zo;
        QByteArray sx, rx;
        CHECK_COND(zo.open(oddDeck, &err)
                           && zo.extract(zo.findExactEntry("ppt/slides/slide1.xml"), sx, &err)
                           && zo.extract(zo.findExactEntry("ppt/slides/_rels/slide1.xml.rels"), rx, &err),
                   "73h: reopen");
        CHECK_COND(xmlError73(sx).isEmpty(), "73h: slide with odd names is not well-formed");
        const QVector<Pptx73Shape> frames = slideShapes73(sx);
        CHECK_COND(frames.size() == 1 && frames[0].descr == "torigraph:" + odd
                           && frames[0].name == "ToriGraphOLE:" + odd
                           && frames[0].progId == "ToriGraph.Graph.1",
                   "73h: OLE frame name not kept verbatim");
        // the second graph is a plain picture with the same naming
        QStringList picNames;
        int depth = 0, picDepth = -1;
        QXmlStreamReader xr(sx);
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isStartElement()) {
                ++depth;
                if (xr.name() == QLatin1String("graphicFrame"))
                    picDepth = -2; // inside a frame: its preview <p:pic> does not count
                else if (xr.name() == QLatin1String("pic") && picDepth == -1)
                    picDepth = depth;
                else if (xr.name() == QLatin1String("cNvPr") && picDepth > 0)
                    picNames << xr.attributes().value(QLatin1String("name")).toString();
            } else if (xr.isEndElement()) {
                if (xr.name() == QLatin1String("graphicFrame"))
                    picDepth = -1;
                else if (xr.name() == QLatin1String("pic") && depth == picDepth)
                    picDepth = -1;
                --depth;
            }
        }
        CHECK_COND(picNames == QStringList() << "ToriGraphOLE:" + odd,
                   "73h: picture-only shape: " << picNames.join('|').toStdString());
        const QString rels = QString::fromUtf8(rx);
        CHECK_COND(rels.count("relationships/oleObject\"") == 1
                           && rels.count("relationships/image\"") == 2,
                   "73h: relationships of the OLE + picture-only slide");
    }

    // ---- 73f. dialog: defaults read from the preset, options read back -----------------
    {
        QScopedPointer<QDialog> dlg(ToriGraphPptx::createSendDialog(w73, nullptr));
        CHECK_COND(dlg, "73f: dialog");
        auto *per = dlg->findChild<QComboBox *>("pptxPerSlide");
        CHECK_COND(per && per->count() == 5 && per->itemData(0).toInt() == 1
                           && per->itemData(1).toInt() == 2 && per->itemData(2).toInt() == 3
                           && per->itemData(3).toInt() == 4 && per->itemData(4).toInt() == 6,
                   "73f: per-slide choices 1/2/3/4/6");
        auto *size = dlg->findChild<QComboBox *>("pptxSlideSize");
        CHECK_COND(size && size->count() == 2, "73f: slide size choices");
        auto *spin = dlg->findChild<QDoubleSpinBox *>("pptxScalePercent");
        CHECK_COND(spin && std::fabs(spin->value() - labPct) < 1e-9,
                   "73f: paste factor not taken from the preset");
        auto *hint = dlg->findChild<QLabel *>("pptxScaleHint");
        CHECK_COND(hint && hint->text().contains(QString::number(lab.pageWidthMm, 'g', 6)),
                   "73f: hint does not show the preset page size");
        auto *rbSel = dlg->findChild<QRadioButton *>("pptxTargetSelected");
        auto *list = dlg->findChild<QListWidget *>("pptxGraphList");
        auto *path = dlg->findChild<QLineEdit *>("pptxPath");
        auto *rbFit = dlg->findChild<QRadioButton *>("pptxScaleFit");
        auto *rbFixed = dlg->findChild<QRadioButton *>("pptxScaleFixed");
        auto *chkPreset = dlg->findChild<QCheckBox *>("pptxApplyPreset");
        CHECK_COND(chkPreset && !chkPreset->isChecked(),
                   "73f: applying the preset must be off by default");
        CHECK_COND(rbSel && list && list->count() == 2 && path && rbFit && rbFixed,
                   "73f: widgets");
        rbFixed->setChecked(true);
        rbSel->setChecked(true);
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setCheckState(Qt::Unchecked);
        QString derr;
        CHECK_COND(!ToriGraphPptx::readSendDialog(dlg.data(), w73, nullptr, nullptr, nullptr,
                                                  nullptr, &derr)
                           && !derr.isEmpty(),
                   "73f: an empty selection must be refused");
        list->item(0)->setCheckState(Qt::Checked);
        per->setCurrentIndex(3);
        size->setCurrentIndex(1);
        path->setText(QDir::toNativeSeparators(QDir(tmpDir).filePath("from_dialog")));
        QList<MyWidget *> wins;
        ToriGraphPptx::DeckOptions od;
        QString p;
        bool preset = false;
        CHECK_COND(ToriGraphPptx::readSendDialog(dlg.data(), w73, &wins, &od, &p, &preset, &derr),
                   "73f: read: " << derr.toStdString());
        CHECK_COND(wins.size() == 1 && wins.first() == mlA && od.perSlide == 4 && !od.widescreen
                           && std::fabs(od.scalePercent - labPct) < 1e-9
                           && p.endsWith("from_dialog.pptx"),
                   "73f: options read back");
        rbFit->setChecked(true);
        CHECK_COND(ToriGraphPptx::readSendDialog(dlg.data(), w73, nullptr, &od, nullptr, nullptr,
                                                 &derr)
                           && od.scalePercent <= 0.0,
                   "73f: fit mode");
    }

    // TORIGRAPH_SUITE73_APPEND_TO=<deck.pptx>: also add a slide to a copy of a deck made by other
    // software (PowerPoint, python-pptx) for a manual check; the original is not touched.
    const QString foreignDeck = qEnvironmentVariable("TORIGRAPH_SUITE73_APPEND_TO");
    if (!foreignDeck.isEmpty() && QFileInfo::exists(foreignDeck)) {
        const QString copy = QDir(tmpDir).filePath("appended_to_foreign.pptx");
        QFile::remove(copy);
        QFile::copy(foreignDeck, copy);
        ToriGraphPptx::DeckOptions oa = opt;
        oa.appendToExisting = true;
        const ToriGraphPptx::DeckResult rx =
                ToriGraphPptx::exportGraphs(w73, QList<MyWidget *>() << mlA << mlB, oa, copy);
        std::cout << "[INFO] 73: appended to a copy of " << foreignDeck.toStdString() << ": ok="
                  << rx.ok << " slides=" << rx.slideCount << " " << rx.error.toStdString() << "\n";
    }
    std::cout << "[INFO] Suite 73 Send to PowerPoint passed!\n";
    return 0;
}

// Suite 75: copying / exporting a graph must not move anything on it. Copy Page (copyAllLayers,
// the PowerPoint copy), copyPageAsImage, the OLE metafile, every file export and the layer
// copies run five times each at 100 % and 200 % view; the legend, a text annotation, an arrow
// and a picture must keep their data coordinates and pixel boxes exactly, and the axis ranges
// and layer geometry must stay as they were.
static QStringList suite75GraphState(MultiLayer *ml)
{
    QStringList s;
    auto num = [](double v) { return QString::number(v, 'g', 17); };
    auto rectStr = [](const QRect &r) {
        return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    };
    s << QStringLiteral("zoom %1 window %2x%3").arg(num(ml->pageZoom())).arg(ml->width()).arg(ml->height());
    const QList<Graph *> layers = ml->layersList();
    for (int li = 0; li < layers.size(); ++li) {
        Graph *g = layers.at(li);
        if (!g || !g->plotWidget())
            continue;
        Plot *pl = g->plotWidget();
        const QString L = QStringLiteral("L%1 ").arg(li + 1);
        s << L + QStringLiteral("geometry ") + rectStr(g->geometry()) + QStringLiteral(" plot ")
                        + rectStr(pl->geometry()) + QStringLiteral(" canvas ")
                        + rectStr(pl->canvas()->geometry());
        for (int a = 0; a < QwtPlot::axisCnt; ++a) {
            const QwtScaleDiv *d = pl->axisScaleDiv(a);
            if (d)
                s << L + QStringLiteral("axis%1 %2 .. %3").arg(a).arg(num(d->lowerBound())).arg(num(d->upperBound()));
        }
        const QVector<int> texts = g->textMarkerKeys();
        for (int id : texts) {
            Legend *m = g->textMarker(id);
            if (!m)
                continue;
            s << L + QStringLiteral("text '%1' data (%2, %3) px %4")
                             .arg(m->text().left(12))
                             .arg(num(m->xValue()))
                             .arg(num(m->yValue()))
                             .arg(rectStr(m->rect()));
        }
        const QVector<int> lines = g->lineMarkerKeys();
        for (int id : lines) {
            ArrowMarker *m = g->arrow(id);
            if (!m)
                continue;
            const QwtDoubleRect b = m->boundingRect();
            s << L + QStringLiteral("arrow data (%1, %2)-(%3, %4) px %5")
                             .arg(num(m->startPointCoord().x()))
                             .arg(num(m->startPointCoord().y()))
                             .arg(num(m->endPointCoord().x()))
                             .arg(num(m->endPointCoord().y()))
                             .arg(rectStr(QRect(m->startPoint(), m->endPoint())));
            Q_UNUSED(b);
        }
        const QVector<int> images = g->imageMarkerKeys();
        for (int id : images) {
            ImageMarker *m = g->imageMarker(id);
            if (!m)
                continue;
            const QwtDoubleRect b = m->boundingRect();
            s << L + QStringLiteral("image data (%1, %2) %3x%4 px %5")
                             .arg(num(b.left()))
                             .arg(num(b.top()))
                             .arg(num(b.width()))
                             .arg(num(b.height()))
                             .arg(rectStr(m->rect()));
        }
    }
    return s;
}

int runSuite75CopyAndExportKeepGraphState(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 75: copy / export leave legend, annotations and axes in place...\n";
    CHECK_COND(mw != nullptr, "ApplicationWindow is null in Suite 75");
    qApp->setProperty("torigraph_noninteractive", true);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir tmp75;
    CHECK_COND(tmp75.isValid(), "75: QTemporaryDir failed");

    ApplicationWindow *w75 = new ApplicationWindow();
    w75->applyUserSettings();
    MultiLayer::setNewGraphStylePresetId(QString());
    Table *t = w75->newTable(QStringLiteral("Drift75"), 10, 3);
    CHECK_COND(t != nullptr, "75: table");
    t->setColName(0, "x");
    t->setColName(1, "y1");
    t->setColName(2, "y2");
    for (int r = 0; r < 10; ++r) {
        t->setCell(r, 0, 0.02 * (r + 1));
        t->setCell(r, 1, 2.0 + 1.7 * r);
        t->setCell(r, 2, 1.0 + 1.1 * r);
    }
    MultiLayer *ml = w75->multilayerPlot(
            t, QStringList() << (t->name() + "_y1") << (t->name() + "_y2"), Graph::LineSymbols);
    CHECK_COND(ml && ml->activeGraph(), "75: graph");
    w75->show();
    ml->show();
    for (int k = 0; k < 5; ++k)
        QApplication::processEvents();
    // The lab slide preset opens the small page at a 160 % view, so the markers are placed while
    // the view is zoomed (the case seen in practice).
    CHECK_COND(ml->applyStylePresetById(QStringLiteral("lab_slide_83mm")), "75: apply lab preset");
    QApplication::processEvents();
    Graph *g = ml->activeGraph();
    CHECK_COND(g && g->legend(), "75: legend");

    // Fix the legend at a position chosen by the user (no automatic anchor any more), and add a
    // text annotation, an arrow and a picture at data positions.
    g->plotWidget()->setProperty("_toriLegendAnchor", QVariant());
    Legend *note = g->newLegend(QStringLiteral("annealed 473 K"));
    CHECK_COND(note != nullptr, "75: text annotation");
    {
        ArrowMarker arr;
        arr.setStartPoint(0.10, 8.0);
        arr.setEndPoint(0.14, 11.0);
        arr.setColor(Qt::black);
        arr.setWidth(1);
        arr.drawEndArrow(true);
        g->addArrow(&arr);
    }
    CHECK_COND(g->lineMarkerKeys().size() == 1, "75: arrow");
    ArrowMarker *arrow = g->arrow(g->lineMarkerKeys().first());
    const QString pngPath = tmp75.filePath(QStringLiteral("inset75.png"));
    {
        QImage inset(40, 30, QImage::Format_ARGB32);
        inset.fill(QColor(30, 120, 200));
        CHECK_COND(inset.save(pngPath, "PNG"), "75: picture");
    }
    ImageMarker *img = g->addImage(pngPath);
    CHECK_COND(img != nullptr && arrow != nullptr, "75: add picture");

    // Places every marker while the view is at placeZoom (their pixel caches are made there),
    // then shows the page at viewZoom.
    auto placeMarkers = [&](double placeZoom, double viewZoom) {
        ml->setPageZoom(placeZoom);
        for (int k = 0; k < 3; ++k)
            QApplication::processEvents();
        // A different value first: setOriginCoord / setBoundingRect skip equal values.
        g->legend()->setOriginCoord(0.05, 10.0);
        g->legend()->setOriginCoord(0.035, 17.5);
        note->setOriginCoord(0.05, 10.0);
        // "annealed 473 K" spans >50% of the 83 mm canvas width, so placing it at x = 0.06 stays
        // clear of Legend::drawGeometry()'s right-edge clamp (now reflected by Legend::rect()).
        note->setOriginCoord(0.06, 6.0);
        arrow->setBoundingRect(0.10, 8.0, 0.14, 11.0);
        img->setBoundingRect(0.10, 10.0, 0.12, 8.0);
        img->setBoundingRect(0.16, 15.0, 0.19, 12.0);
        g->replot();
        ml->setPageZoom(viewZoom);
        for (int k = 0; k < 3; ++k)
            QApplication::processEvents();
    };
    auto placedExactly = [&]() -> bool {
        return g->legend()->xValue() == 0.035 && g->legend()->yValue() == 17.5 && note->xValue() == 0.06
                && note->yValue() == 6.0 && img->xValue() == 0.16 && img->yValue() == 15.0
                && img->right() == 0.19 && img->bottom() == 12.0 && arrow->startPointCoord().x() == 0.10
                && arrow->endPointCoord().y() == 11.0;
    };

    struct Op {
        QString name;
        std::function<void()> run;
    };
    int fileNo = 0;
    auto out = [&](const QString &ext) {
        return tmp75.filePath(QStringLiteral("out75_%1.%2").arg(++fileNo).arg(ext));
    };
    const QList<Op> ops = {
        { QStringLiteral("copyAllLayers"), [&]() { ml->copyAllLayers(); } },
        { QStringLiteral("copyGraphForPowerPoint"),
          [&]() { (void)SciDAVisOle::copyGraphForPowerPoint(w75, ml, nullptr); } },
        { QStringLiteral("copyGraphForPowerPoint(layer)"),
          [&]() { (void)SciDAVisOle::copyGraphForPowerPoint(w75, ml, g); } },
        { QStringLiteral("copyPageAsImage"), [&]() { (void)ml->copyPageAsImage(300); } },
        { QStringLiteral("renderMultiLayer"), [&]() { (void)SciDAVisEmf::renderMultiLayer(ml, nullptr, false); } },
        { QStringLiteral("renderMultiLayer(layer)"), [&]() { (void)SciDAVisEmf::renderMultiLayer(ml, g, true); } },
        // The refreshes the OLE server and the Python automation server run before drawing.
        { QStringLiteral("Graph::updatePlot"), [&]() { g->updatePlot(); } },
        { QStringLiteral("Graph::updateScale"), [&]() { g->updateScale(); } },
        { QStringLiteral("exportImage png"), [&]() { ml->exportImage(out(QStringLiteral("png")), 100, false, 150); } },
        { QStringLiteral("exportImage emf"), [&]() { ml->exportImage(out(QStringLiteral("emf"))); } },
        { QStringLiteral("exportToFile emf"), [&]() { ml->exportToFile(out(QStringLiteral("emf"))); } },
        { QStringLiteral("exportVector pdf"), [&]() { ml->exportVector(out(QStringLiteral("pdf"))); } },
        { QStringLiteral("exportPDF"), [&]() { ml->exportPDF(out(QStringLiteral("pdf"))); } },
        { QStringLiteral("exportSVG"), [&]() { ml->exportSVG(out(QStringLiteral("svg"))); } },
        { QStringLiteral("layer exportImage"), [&]() { g->exportImage(out(QStringLiteral("png")), 100, false, 150); } },
        { QStringLiteral("layer exportSVG"), [&]() { g->exportSVG(out(QStringLiteral("svg"))); } },
        { QStringLiteral("layer exportVector"), [&]() { g->exportVector(out(QStringLiteral("pdf"))); } },
        { QStringLiteral("layer exportToFile emf"), [&]() { g->exportToFile(out(QStringLiteral("emf"))); } },
        { QStringLiteral("layer copyImage"), [&]() { g->copyImage(); } },
    };

    int failures = 0;
    for (const double zoom : { 1.0, 2.0 }) {
        for (const Op &op : ops) {
            // Every operation starts from markers placed at the 160 % view of the preset.
            placeMarkers(1.6, zoom);
            CHECK_COND(std::fabs(ml->pageZoom() - zoom) < 1e-9, "75: view zoom");
            CHECK_COND(placedExactly(), "75: markers not at their data positions after placing");
            const QStringList before = suite75GraphState(ml);
            for (int rep = 1; rep <= 5; ++rep) {
                op.run();
                QApplication::processEvents();
                const QStringList after = suite75GraphState(ml);
                if (after != before || !placedExactly()) {
                    ++failures;
                    std::cerr << "[FAIL] 75: " << op.name.toStdString() << " at " << zoom * 100.0
                              << " % changed the graph (repeat " << rep << "):\n";
                    for (int i = 0; i < qMax(before.size(), after.size()); ++i) {
                        if (before.value(i) != after.value(i)) {
                            std::cerr << "    before: " << before.value(i).toStdString() << "\n"
                                      << "    after:  " << after.value(i).toStdString() << "\n";
                        }
                    }
                    break;
                }
            }
        }
    }
    std::cout << "[INFO] 75 state at 200 %:\n";
    for (const QString &ln : suite75GraphState(ml))
        std::cout << "    " << ln.toStdString() << "\n";
    CHECK_COND(failures == 0, "75: copy / export moved markers or changed the axes (see above)");

    // The copied picture shows the markers where the page shows them: the same picture whether
    // the markers were placed at 100 % or at 160 %, at 100 % and 200 % view, before and after
    // five PowerPoint copies.
    {
        placeMarkers(1.0, 1.0);
        const QImage ref = ml->copyPageAsImage(150).convertToFormat(QImage::Format_ARGB32);
        CHECK_COND(!ref.isNull(), "75: reference page image");
        placeMarkers(1.6, 1.0);
        const QImage stale100 = ml->copyPageAsImage(150).convertToFormat(QImage::Format_ARGB32);
        for (int rep = 0; rep < 5; ++rep)
            ml->copyAllLayers();
        const QImage after100 = ml->copyPageAsImage(150).convertToFormat(QImage::Format_ARGB32);
        placeMarkers(1.6, 2.0);
        for (int rep = 0; rep < 5; ++rep)
            ml->copyAllLayers();
        const QImage after200 = ml->copyPageAsImage(150).convertToFormat(QImage::Format_ARGB32);
        std::cout << "[INFO] 75 page image " << ref.width() << "x" << ref.height()
                  << ": placed at 160 % / after copies at 100 % / at 200 % equal the reference: "
                  << (stale100 == ref) << " / " << (after100 == ref) << " / " << (after200 == ref) << "\n";
        CHECK_COND(stale100 == ref && after100 == ref && after200 == ref,
                   "75: the copied page image differs from the reference drawing");
    }

    // A real rescale still keeps text markers and pictures where they are drawn on the canvas,
    // also when they were placed at another view zoom.
    {
        // setScale() on a page with a fixed aspect ratio re-arranges the layer, so the canvas may
        // change size: compare positions as fractions of the canvas.
        placeMarkers(1.6, 2.0);
        const QSize cv1 = g->plotWidget()->canvas()->size();
        const QRect legPx = g->legend()->rect();
        const QRect notePx = note->rect();
        const QRect imgPx = img->rect();
        g->setScale(QwtPlot::xBottom, 0.0, 0.5);
        QApplication::processEvents();
        const QSize cv2 = g->plotWidget()->canvas()->size();
        const QRect legPx2 = g->legend()->rect();
        const QRect notePx2 = note->rect();
        const QRect imgPx2 = img->rect();
        // Within 1.5 px of the same place on the canvas (pixel rounding of the cached origin).
        auto samePlace = [&](const QRect &a, const QRect &b, bool withSize) {
            const double tx = 1.5 / cv2.width(), ty = 1.5 / cv2.height();
            bool ok = std::fabs(double(a.x()) / cv1.width() - double(b.x()) / cv2.width()) <= tx
                    && std::fabs(double(a.y()) / cv1.height() - double(b.y()) / cv2.height()) <= ty;
            if (withSize) {
                ok = ok && std::fabs(double(a.width()) / cv1.width() - double(b.width()) / cv2.width()) <= tx
                        && std::fabs(double(a.height()) / cv1.height() - double(b.height()) / cv2.height()) <= ty;
            }
            return ok;
        };
        std::cout << "[INFO] 75 rescale x 0..0.25 -> 0..0.5 at 200 % (canvas " << cv1.width() << "x" << cv1.height()
                  << " -> " << cv2.width() << "x" << cv2.height() << "): legend " << legPx.x() << "," << legPx.y()
                  << " -> " << legPx2.x() << "," << legPx2.y() << ", note " << notePx.x() << "," << notePx.y()
                  << " -> " << notePx2.x() << "," << notePx2.y() << ", picture " << imgPx.x() << ","
                  << imgPx.y() << " " << imgPx.width() << "x" << imgPx.height() << " -> " << imgPx2.x() << ","
                  << imgPx2.y() << " " << imgPx2.width() << "x" << imgPx2.height() << "\n";
        CHECK_COND(samePlace(legPx, legPx2, false) && samePlace(notePx, notePx2, false)
                           && samePlace(imgPx, imgPx2, true),
                   "75: a rescale moved the legend, a text or a picture on the canvas");
    }

    // ---- 75s: scale changes keep the legend, texts and pictures on their place on the canvas
    // (also through the paths that do not replot between setAxisScale() and updatePlot(), the
    // full autoscale, a linear <-> log switch, and after a view zoom); arrows follow the data ----
    {
        auto resetAxes = [&]() {
            g->setScale(QwtPlot::xBottom, 0.0, 0.25);
            g->setScale(QwtPlot::yLeft, 0.0, 20.0);
            QApplication::processEvents();
        };
        // Runs act and checks that the text markers and the picture stay on the canvas where
        // they were (as fractions of the canvas, within 1.5 px; with absolutePx at the same pixel,
        // within 1 px), the picture keeps its size, and the arrow keeps its data coordinates.
        auto staysOnCanvas = [&](const char *what, const std::function<void()> &act,
                                 bool absolutePx = false) -> bool {
            const QSize cv1 = g->plotWidget()->canvas()->size();
            const QRect leg1 = g->legend()->rect(), note1 = note->rect(), img1 = img->rect();
            act();
            QApplication::processEvents();
            const QSize cv2 = g->plotWidget()->canvas()->size();
            const QRect leg2 = g->legend()->rect(), note2 = note->rect(), img2 = img->rect();
            auto samePlace = [&](const QRect &a, const QRect &b, bool withSize) {
                if (absolutePx) {
                    return std::abs(a.x() - b.x()) <= 1 && std::abs(a.y() - b.y()) <= 1
                            && (!withSize
                                || (std::abs(a.width() - b.width()) <= 1
                                    && std::abs(a.height() - b.height()) <= 1));
                }
                const double tx = 1.5 / cv2.width(), ty = 1.5 / cv2.height();
                bool ok = std::fabs(double(a.x()) / cv1.width() - double(b.x()) / cv2.width()) <= tx
                        && std::fabs(double(a.y()) / cv1.height() - double(b.y()) / cv2.height()) <= ty;
                if (withSize) {
                    ok = ok && std::fabs(double(a.width()) / cv1.width() - double(b.width()) / cv2.width()) <= tx
                            && std::fabs(double(a.height()) / cv1.height() - double(b.height()) / cv2.height())
                                    <= ty;
                }
                return ok;
            };
            const bool arrowKept = arrow->startPointCoord() == QwtDoublePoint(0.10, 8.0)
                    && arrow->endPointCoord() == QwtDoublePoint(0.14, 11.0);
            const bool ok = samePlace(leg1, leg2, false) && samePlace(note1, note2, false)
                    && samePlace(img1, img2, true) && img2.width() > 0 && img2.height() > 0 && arrowKept;
            std::cout << "[INFO] 75s " << what << ": canvas " << cv1.width() << "x" << cv1.height() << " -> "
                      << cv2.width() << "x" << cv2.height() << ", legend " << leg1.x() << "," << leg1.y() << " -> "
                      << leg2.x() << "," << leg2.y() << ", note " << note1.x() << "," << note1.y() << " -> "
                      << note2.x() << "," << note2.y() << ", picture " << img1.x() << "," << img1.y() << " "
                      << img1.width() << "x" << img1.height() << " -> " << img2.x() << "," << img2.y() << " "
                      << img2.width() << "x" << img2.height() << ", arrow kept " << arrowKept
                      << (ok ? "" : "  <-- FAIL") << "\n";
            return ok;
        };
        // An arrow follows the data: its pixels are those of its data points on the new scale.
        auto arrowFollowsData = [&]() {
            const QwtScaleMap xm = g->plotWidget()->canvasMap(arrow->xAxis());
            const QwtScaleMap ym = g->plotWidget()->canvasMap(arrow->yAxis());
            return arrow->startPoint() == QPoint(xm.transform(0.10), ym.transform(8.0))
                    && arrow->endPoint() == QPoint(xm.transform(0.14), ym.transform(11.0));
        };

        // Qwt's setAxisScale() leaves the scale division invalid until the next replot; the
        // previous scale then comes from the scale widget.
        resetAxes();
        placeMarkers(1.6, 1.0);
        CHECK_COND(staysOnCanvas("setAxisScale + updatePlot without replot", [&]() {
                       g->plotWidget()->setAxisScale(QwtPlot::xBottom, 0.0, 0.5);
                       g->updatePlot();
                   }),
                   "75s: setAxisScale() followed by updatePlot() moved a marker or squashed the picture");
        CHECK_COND(arrowFollowsData(), "75s: the arrow must follow the data after updatePlot");

        resetAxes();
        placeMarkers(1.0, 2.0);
        CHECK_COND(staysOnCanvas("at 200 % (picture placed at 100 %): setAxisScale + updatePlot", [&]() {
                       g->plotWidget()->setAxisScale(QwtPlot::xBottom, 0.0, 0.5);
                       g->updatePlot();
                   }),
                   "75s: after a view zoom the picture / texts must keep their place (pixel caches)");
        ml->setPageZoom(1.0);
        QApplication::processEvents();

        resetAxes();
        g->setScale(QwtPlot::xBottom, 0.0, 0.5);
        g->setScale(QwtPlot::yLeft, 0.0, 40.0);
        placeMarkers(1.6, 1.0);
        CHECK_COND(staysOnCanvas("full autoscale", [&]() { g->setAutoScale(); }),
                   "75s: autoscale of all axes moved a marker on the canvas");
        CHECK_COND(arrowFollowsData(), "75s: the arrow must follow the data after the autoscale");

        resetAxes();
        g->setScale(QwtPlot::xBottom, 0.0, 0.5);
        placeMarkers(1.6, 1.0);
        CHECK_COND(staysOnCanvas("autoscale of the x axis only", [&]() { g->setAutoScaleAxis(QwtPlot::xBottom); }),
                   "75s: autoscale of one axis moved a marker on the canvas");

        resetAxes();
        placeMarkers(1.6, 1.0);
        CHECK_COND(staysOnCanvas("y linear -> log", [&]() { g->setScale(QwtPlot::yLeft, 1.0, 20.0, 0.0, 5, 8, 1); }),
                   "75s: linear -> log moved a marker on the canvas");
        CHECK_COND(arrowFollowsData(), "75s: the arrow must follow the data on the log axis");
        CHECK_COND(staysOnCanvas("y log -> linear", [&]() { g->setScale(QwtPlot::yLeft, 0.0, 20.0); }),
                   "75s: log -> linear moved a marker on the canvas");

        // A resized layer (pixel caches refreshed by Graph::resizeEvent), then setAxisScale()
        // and updatePlot().
        resetAxes();
        placeMarkers(1.0, 1.0);
        {
            const QRect geo = g->geometry();
            g->resize(geo.width() + 60, geo.height() + 40);
            QApplication::processEvents();
            CHECK_COND(staysOnCanvas("resized layer, then setAxisScale + updatePlot", [&]() {
                           g->plotWidget()->setAxisScale(QwtPlot::xBottom, 0.0, 0.5);
                           g->updatePlot();
                       }),
                       "75s: after a layer resize, setAxisScale + updatePlot moved a marker");
            g->resize(geo.size());
            QApplication::processEvents();
        }
        // Tick labels of another width (0..20 -> 0..20000) move the canvas edge: the markers
        // must be placed with the new layout.
        resetAxes();
        placeMarkers(1.0, 1.0);
        CHECK_COND(staysOnCanvas("y 0..20 -> 0..20000 (wider tick labels)",
                                 [&]() { g->setScale(QwtPlot::yLeft, 0.0, 20000.0); }),
                   "75s: a range change with wider tick labels shifted a marker");
        // The same through the zoom history (Graph::applyAxisViewportState sets the scale
        // divisions without a replot).
        resetAxes();
        g->setScale(QwtPlot::yLeft, 0.0, 20000.0);
        QApplication::processEvents();
        g->recordZoomHistorySnapshot();
        g->setScale(QwtPlot::yLeft, 0.0, 20.0);
        QApplication::processEvents();
        placeMarkers(1.0, 1.0);
        bool stepped = false;
        CHECK_COND(staysOnCanvas("zoom history back to y 0..20000 (wider tick labels)",
                                 [&]() { stepped = g->stepZoomHistory(-1); }),
                   "75s: a zoom history step with wider tick labels shifted a marker");
        CHECK_COND(stepped, "75s: zoom history step");
        CHECK_COND(std::fabs(g->plotWidget()->axisScaleDiv(QwtPlot::yLeft)->upperBound() - 20000.0) < 1e-9,
                   "75s: the zoom history step must restore y 0..20000");

        resetAxes();
        placeMarkers(1.0, 1.0);
    }

    // ---- 75z: view zoom works from the 100 % layer rectangles (no rounding drift, no stuck
    // 60 px minimum) ----
    {
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        Graph *gSmall = ml->addLayer(30, 30, 100, 80);
        QApplication::processEvents();
        CHECK_COND(gSmall != nullptr, "75z: small layer");
        auto layerGeoms = [&]() {
            QStringList s;
            for (Graph *gr : ml->layersList()) {
                if (gr && gr->plotWidget()) {
                    const QRect r = gr->geometry();
                    const QSize p = gr->plotWidget()->size();
                    s << QStringLiteral("%1,%2 %3x%4 plot %5x%6")
                                    .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())
                                    .arg(p.width()).arg(p.height());
                }
            }
            const QSize lc = ml->layerCanvasSize();
            s << QStringLiteral("layer canvas %1x%2").arg(lc.width()).arg(lc.height());
            return s;
        };
        auto report = [&](const char *what, const QStringList &a, const QStringList &b) {
            if (a == b)
                return true;
            std::cerr << "[FAIL] 75z: " << what << " changed the layer geometry:\n";
            for (int i = 0; i < qMax(a.size(), b.size()); ++i)
                if (a.value(i) != b.value(i))
                    std::cerr << "    " << a.value(i).toStdString() << "  ->  " << b.value(i).toStdString() << "\n";
            return false;
        };
        const QStringList geo100 = layerGeoms();
        std::cout << "[INFO] 75z layers at 100 %: " << geo100.join(QStringLiteral(" | ")).toStdString() << "\n";
        CHECK_COND(gSmall->geometry() == QRect(30, 30, 100, 80), "75z: small layer must be 100x80 at 100 %");

        ml->setPageZoom(0.5);
        QApplication::processEvents();
        std::cout << "[INFO] 75z small layer at 50 %: " << gSmall->width() << "x" << gSmall->height() << "\n";
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        CHECK_COND(report("1.0 -> 0.5 -> 1.0", geo100, layerGeoms()), "75z: zoom 50 % and back");
        CHECK_COND(gSmall->geometry() == QRect(30, 30, 100, 80), "75z: the 100x80 layer must come back from 50 %");

        for (int i = 0; i < 9; ++i) {
            QWheelEvent we(QPointF(20, 20), ml->mapToGlobal(QPoint(20, 20)), QPoint(), QPoint(0, 120),
                           Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(ml, &we);
        }
        QApplication::processEvents();
        const double zoomUp = ml->pageZoom();
        for (int i = 0; i < 9; ++i) {
            QWheelEvent we(QPointF(20, 20), ml->mapToGlobal(QPoint(20, 20)), QPoint(), QPoint(0, -120),
                           Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(ml, &we);
        }
        QApplication::processEvents();
        std::cout << "[INFO] 75z Ctrl+wheel 9 steps: zoom " << zoomUp << " and back to " << ml->pageZoom() << "\n";
        CHECK_COND(zoomUp > 2.0 && std::fabs(ml->pageZoom() - 1.0) < 1e-6, "75z: Ctrl+wheel round trip zoom");
        CHECK_COND(report("Ctrl+wheel 9 steps there and back", geo100, layerGeoms()), "75z: Ctrl+wheel round trip");

        ml->setPageZoom(2.0);
        QApplication::processEvents();
        const QStringList geo200 = layerGeoms();
        for (int rep = 0; rep < 5; ++rep) {
            ml->exportImage(tmp75.filePath(QStringLiteral("z75_%1.png").arg(rep)), 100, false, 150);
            ml->exportSVG(tmp75.filePath(QStringLiteral("z75_%1.svg").arg(rep)));
            (void)ml->copyPageAsImage(150);
            (void)SciDAVisEmf::renderMultiLayer(ml, nullptr, false);
            ml->copyAllLayers();
            QApplication::processEvents();
            CHECK_COND(report("an export at 200 %", geo200, layerGeoms()), "75z: export at 200 %");
        }
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        CHECK_COND(report("five exports at 200 % and back to 100 %", geo100, layerGeoms()), "75z: exports at 200 %");

        // A layer moved while zoomed takes its 100 % rectangle from the new place.
        ml->setPageZoom(2.0);
        QApplication::processEvents();
        ml->setActiveGraph(gSmall);
        ml->setGraphGeometry(100, 120, gSmall->width(), gSmall->height());
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        std::cout << "[INFO] 75z layer moved to 100,120 at 200 %: at 100 % " << gSmall->x() << "," << gSmall->y()
                  << " " << gSmall->width() << "x" << gSmall->height() << "\n";
        CHECK_COND(gSmall->geometry() == QRect(50, 60, 100, 80),
                   "75z: a layer moved at 200 % must be at half the place at 100 %, same size");
        // Moved at 100 %, then zoomed.
        ml->setGraphGeometry(40, 44, 100, 80);
        ml->setPageZoom(0.5);
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        CHECK_COND(gSmall->geometry() == QRect(40, 44, 100, 80), "75z: a layer moved at 100 % must keep its place");
        // Moved (not resized) at 50 %, where the view shows it at the 60 px minimum: it keeps its
        // 100 % size.
        ml->setPageZoom(0.5);
        QApplication::processEvents();
        CHECK_COND(gSmall->geometry() == QRect(20, 22, 60, 60), "75z: small layer at 50 % shows 60x60");
        ml->setGraphGeometry(30, 35, gSmall->width(), gSmall->height());
        ml->setPageZoom(1.0);
        QApplication::processEvents();
        std::cout << "[INFO] 75z small layer moved to 30,35 at 50 %: at 100 % " << gSmall->x() << ","
                  << gSmall->y() << " " << gSmall->width() << "x" << gSmall->height() << "\n";
        CHECK_COND(gSmall->geometry() == QRect(60, 70, 100, 80),
                   "75z: a small layer moved at 50 % must keep its 100x80 size at 100 %");
        ml->setActiveGraph(g);
    }

    ml->askOnCloseEvent(false);
    w75->savedProject();
    w75->setOleEmbeddedMode(true);
    delete w75;
    QApplication::processEvents();
    std::cout << "[PASS] Suite 75 passed.\n";
    return 0;
}

// =============================================================================
// Suite 76: Review Polish & Edge-Case Regression Gate
// =============================================================================
#include "TextDialog.h"
#include "ColorButton.h"

namespace {
struct Suite76AxesDialogProbe : public AxesDialog
{
    double titlePtBoxValue() const { return boxTitleFontSize ? boxTitleFontSize->value() : -1.0; }
    double numbersPtBoxValue() const { return boxAxesFontSize ? boxAxesFontSize->value() : -1.0; }
    void editSelectedRefLineColor(const QColor &c)
    {
        if (refLinesPage)
            generalDialog.setCurrentWidget(refLinesPage);
        if (refLinesList && refLinesList->count() > 0 && refLinesList->currentRow() < 0)
            refLinesList->setCurrentRow(0);
        if (refColorBtn)
            refColorBtn->setColor(c);
        syncSelectedRefLineFromUi();
    }
    void acceptDialog() { accept(); }
};

struct Suite76TextDialogProbe : public TextDialog
{
    explicit Suite76TextDialogProbe(TextType type) : TextDialog(type) {}
    double fontPtBoxValue() const { return boxFontSize ? boxFontSize->value() : -1.0; }
    void applyDialog()
    {
        if (buttonApply)
            buttonApply->click();
    }
};

// Puts the run's settings file back as it was when the suite started. saveSettings() stores
// the active graph of the saving window as the "last graph" defaults (LastGraphSettings: page
// size, zoom and unit format; the graph fonts, tick lengths and legend of /2DPlots), and every
// ApplicationWindow created later in the same run reads them for its new graphs. Suite 76 saves
// with a lab_slide_83mm graph active (76a), so without this the later suites ran against that
// graph's defaults instead of the initial ones (Suite 102j: a two-curve lab_slide_83mm graph
// then got no legend headroom, so no <ToriAxisTickStep>, only in the full run).
struct SettingsFileRestore76
{
    QList<QPair<QString, QVariant>> before;
    SettingsFileRestore76()
    {
        QSettings &st = ApplicationWindow::getSettings();
        st.sync();
        for (const QString &k : st.allKeys())
            before << qMakePair(k, st.value(k));
    }
    ~SettingsFileRestore76()
    {
        QSettings &st = ApplicationWindow::getSettings();
        st.clear();
        for (const auto &kv : before)
            st.setValue(kv.first, kv.second);
        st.sync();
    }
    SettingsFileRestore76(const SettingsFileRestore76 &) = delete;
    SettingsFileRestore76 &operator=(const SettingsFileRestore76 &) = delete;
};
} // namespace

int runSuite76ReviewPolishAndEdgeCases(ApplicationWindow *mw)
{
    std::cout << "[INFO] Running Suite 76: review polish, fractional pt/px, workbook undo/redo, "
                 "pole-figure presets, xlsx N/A & paste disambiguation, and ref-line/colorbar persistence...\n";
    CHECK_COND(mw != nullptr, "76: ApplicationWindow is null");
    // Declared before every window of this suite, so it restores the file after they are gone.
    const SettingsFileRestore76 settingsRestore76;
    qApp->setProperty("torigraph_noninteractive", true);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir tmp76;
    CHECK_COND(tmp76.isValid(), "76: QTemporaryDir failed");

    auto storedPt76 = [](const QFont &f) -> double {
        return (f.pointSizeF() > 0.0) ? f.pointSizeF() : double(f.pointSize());
    };

    ApplicationWindow *w76 = new ApplicationWindow();
    w76->applyUserSettings();
    MultiLayer::setNewGraphStylePresetId(QString());

    // -------------------------------------------------------------------------
    // 76a: Fractional font sizes (12.9 pt / 11.5 pt) across AxesDialog,
    //      TextDialog, GraphMiniToolbar (+1 -> -1), saveSettings/readSettings,
    //      and fractional line/symbol pen widths (1.5 px / 0.5 px) across
    //      .sciprj round-trip and MultiLayer Undo/Redo.
    // -------------------------------------------------------------------------
    {
        Table *t76a = w76->newTable(QStringLiteral("Tbl76a"), 6, 2);
        CHECK_COND(t76a != nullptr, "76a: table creation failed");
        t76a->setColName(0, QStringLiteral("Strain"));
        t76a->setColName(1, QStringLiteral("Stress"));
        for (int r = 0; r < 6; ++r) {
            t76a->setCell(r, 0, 0.05 * (r + 1));
            t76a->setCell(r, 1, 100.0 * (r + 1));
        }
        MultiLayer *ml76a = w76->multilayerPlot(
                t76a, QStringList() << (t76a->name() + QStringLiteral("_Stress")),
                Graph::LineSymbols);
        CHECK_COND(ml76a && ml76a->activeGraph(), "76a: graph creation failed");
        CHECK_COND(ml76a->applyStylePresetById(QStringLiteral("lab_slide_83mm")),
                   "76a: apply lab_slide_83mm preset");
        Graph *g76a = ml76a->activeGraph();
        Legend *leg76a = g76a->legend();
        CHECK_COND(leg76a != nullptr, "76a: legend missing");
        CHECK_COND(std::fabs(storedPt76(g76a->axisTitleFont(QwtPlot::xBottom)) - 12.9) < 1e-6
                           && std::fabs(storedPt76(g76a->axisFont(QwtPlot::xBottom)) - 12.9) < 1e-6
                           && std::fabs(storedPt76(leg76a->font()) - 11.5) < 1e-6,
                   "76a: preset initial fractional pt mismatch");

        // AxesDialog preserves 12.9 pt without rounding to 13 (and setGraph initializes scale boxes)
        {
            Suite76AxesDialogProbe axDlg;
            axDlg.setGraph(g76a);
            axDlg.showAxesPage();
            CHECK_COND(std::fabs(axDlg.titlePtBoxValue() - 12.9) < 1e-6
                               && std::fabs(axDlg.numbersPtBoxValue() - 12.9) < 1e-6,
                       "76a: AxesDialog spinboxes rounded 12.9 pt");
            axDlg.acceptDialog();
            CHECK_COND(std::fabs(storedPt76(g76a->axisTitleFont(QwtPlot::xBottom)) - 12.9) < 1e-6
                               && std::fabs(storedPt76(g76a->axisFont(QwtPlot::xBottom)) - 12.9) < 1e-6,
                       "76a: AxesDialog::accept rounded 12.9 pt");
        }

        // TextDialog preserves 11.5 pt without rounding to 12
        {
            Suite76TextDialogProbe txtDlg(TextDialog::TextMarker);
            txtDlg.setFont(leg76a->font());
            txtDlg.setText(leg76a->text());
            CHECK_COND(std::fabs(txtDlg.fontPtBoxValue() - 11.5) < 1e-6,
                       "76a: TextDialog spinbox rounded 11.5 pt");
            QFont emittedFont;
            bool gotSignal = false;
            QObject::connect(&txtDlg, &TextDialog::values, &txtDlg,
                             [&](const QString &, int, int, const QFont &fnt, const QColor &,
                                 const QColor &) {
                                 emittedFont = fnt;
                                 gotSignal = true;
                             });
            txtDlg.applyDialog();
            CHECK_COND(gotSignal && std::fabs(storedPt76(emittedFont) - 11.5) < 1e-6,
                       "76a: TextDialog::apply rounded 11.5 pt");
        }

        // GraphMiniToolbar (+1 -> -1) keeps 12.9 pt and 11.5 pt fractional parts intact
        {
            struct ScopedInteractiveForMtb {
                QVariant prevProp;
                bool prevEnabled;
                ScopedInteractiveForMtb()
                    : prevProp(qApp->property("torigraph_noninteractive")),
                      prevEnabled(GraphMiniToolbar::isMiniToolbarEnabled())
                {
                    qApp->setProperty("torigraph_noninteractive", QVariant());
                    GraphMiniToolbar::setMiniToolbarEnabled(true);
                }
                ~ScopedInteractiveForMtb()
                {
                    GraphMiniToolbar::instance()->cancelScheduledAndHide();
                    GraphMiniToolbar::setMiniToolbarEnabled(prevEnabled);
                    qApp->setProperty("torigraph_noninteractive", prevProp);
                }
            } mtbGuard;

            GraphMiniToolbar *mtb = GraphMiniToolbar::instance();
            mtb->popupForAxis(g76a, QwtPlot::xBottom, QPoint(120, 120));
            CHECK_COND(QMetaObject::invokeMethod(mtb, "onAxisFontInc"), "76a: onAxisFontInc invoke");
            CHECK_COND(std::fabs(storedPt76(g76a->axisTitleFont(QwtPlot::xBottom)) - 13.9) < 1e-6
                               && std::fabs(storedPt76(g76a->axisFont(QwtPlot::xBottom)) - 13.9) < 1e-6,
                       "76a: MiniToolbar axis +1 did not preserve .9 fractional part");
            CHECK_COND(QMetaObject::invokeMethod(mtb, "onAxisFontDec"), "76a: onAxisFontDec invoke");
            CHECK_COND(std::fabs(storedPt76(g76a->axisTitleFont(QwtPlot::xBottom)) - 12.9) < 1e-6
                               && std::fabs(storedPt76(g76a->axisFont(QwtPlot::xBottom)) - 12.9) < 1e-6,
                       "76a: MiniToolbar axis +1 -> -1 lost 12.9 pt");

            mtb->popupForLegend(g76a, leg76a, QPoint(120, 120));
            CHECK_COND(QMetaObject::invokeMethod(mtb, "onLegendFontInc"), "76a: onLegendFontInc invoke");
            CHECK_COND(std::fabs(storedPt76(leg76a->font()) - 12.5) < 1e-6,
                       "76a: MiniToolbar legend +1 did not preserve .5 fractional part");
            CHECK_COND(QMetaObject::invokeMethod(mtb, "onLegendFontDec"), "76a: onLegendFontDec invoke");
            CHECK_COND(std::fabs(storedPt76(leg76a->font()) - 11.5) < 1e-6,
                       "76a: MiniToolbar legend +1 -> -1 lost 11.5 pt");
        }

        // saveSettings / readSettings preserves fractional pt
        {
            const QFont prevAx = w76->plotAxesFont;
            const QFont prevNum = w76->plotNumbersFont;
            const QFont prevLeg = w76->plotLegendFont;
            const QFont prevTit = w76->plotTitleFont;

            w76->plotAxesFont.setPointSizeF(12.9);
            w76->plotNumbersFont.setPointSizeF(12.9);
            w76->plotLegendFont.setPointSizeF(11.5);
            w76->plotTitleFont.setPointSizeF(12.9);
            w76->saveSettings();

            w76->plotAxesFont.setPointSize(9);
            w76->plotNumbersFont.setPointSize(9);
            w76->plotLegendFont.setPointSize(9);
            w76->plotTitleFont.setPointSize(9);
            w76->readSettings();

            const bool settingsOk = std::fabs(storedPt76(w76->plotAxesFont) - 12.9) < 1e-6
                    && std::fabs(storedPt76(w76->plotNumbersFont) - 12.9) < 1e-6
                    && std::fabs(storedPt76(w76->plotLegendFont) - 11.5) < 1e-6
                    && std::fabs(storedPt76(w76->plotTitleFont) - 12.9) < 1e-6;

            w76->plotAxesFont = prevAx;
            w76->plotNumbersFont = prevNum;
            w76->plotLegendFont = prevLeg;
            w76->plotTitleFont = prevTit;
            w76->saveSettings();
            CHECK_COND(settingsOk, "76a: saveSettings/readSettings rounded fractional font sizes");
        }

        // Fractional line width (1.5 px) and symbol border width (0.5 px) across Undo/Redo & .sciprj
        QwtPlotCurve *cv0 = g76a->curve(0);
        CHECK_COND(cv0 != nullptr, "76a: curve(0) null");
        {
            QPen initPen = cv0->pen();
            initPen.setWidthF(2.5);
            cv0->setPen(initPen);
            QwtSymbol initSym = cv0->symbol();
            QPen initSymPen = initSym.pen();
            initSymPen.setWidthF(1.25);
            initSym.setPen(initSymPen);
            cv0->setSymbol(initSym);
        }
        const QString snapBeforeWidths = ml76a->captureUndoSnapshot();
        {
            QPen p = cv0->pen();
            p.setWidthF(1.5);
            cv0->setPen(p);
            QwtSymbol sym = cv0->symbol();
            QPen sp = sym.pen();
            sp.setWidthF(0.5);
            sym.setPen(sp);
            cv0->setSymbol(sym);
            g76a->replot();
        }
        ml76a->pushUndoSnapshot(QStringLiteral("Set 1.5/0.5 px widths"), snapBeforeWidths);
        CHECK_COND(ml76a->undoStack() && ml76a->undoStack()->canUndo(), "76a: undoStack cannot undo");
        ml76a->undoStack()->undo();
        QApplication::processEvents();
        CHECK_COND(ml76a->activeGraph() && ml76a->activeGraph()->curve(0)
                           && std::fabs(ml76a->activeGraph()->curve(0)->pen().widthF() - 2.5) < 1e-6
                           && std::fabs(ml76a->activeGraph()->curve(0)->symbol().pen().widthF() - 1.25) < 1e-6,
                   "76a: MultiLayer undo lost fractional 2.5 / 1.25 px widths");
        ml76a->undoStack()->redo();
        QApplication::processEvents();
        CHECK_COND(ml76a->activeGraph() && ml76a->activeGraph()->curve(0)
                           && std::fabs(ml76a->activeGraph()->curve(0)->pen().widthF() - 1.5) < 1e-6
                           && std::fabs(ml76a->activeGraph()->curve(0)->symbol().pen().widthF() - 0.5) < 1e-6,
                   "76a: MultiLayer redo lost fractional 1.5 / 0.5 px widths");

        const QString ml76aName = ml76a->name();
        MultiLayer *ml76Spline = w76->multilayerPlot(
                t76a, QStringList() << (t76a->name() + QStringLiteral("_Stress")), Graph::Spline);
        CHECK_COND(ml76Spline && ml76Spline->activeGraph() && ml76Spline->activeGraph()->curve(0),
                   "76a: spline graph creation failed");
        {
            QwtPlotCurve *cvSp = ml76Spline->activeGraph()->curve(0);
            QPen p = cvSp->pen();
            p.setWidthF(1.0);
            cvSp->setPen(p);
            QwtSymbol sym = cvSp->symbol();
            sym.setStyle(QwtSymbol::Ellipse);
            QPen sp = sym.pen();
            sp.setWidthF(2.5);
            sym.setPen(sp);
            cvSp->setSymbol(sym);
        }
        const QString ml76SpName = ml76Spline->name();
        const QString prj76a = tmp76.filePath(QStringLiteral("suite76a.sciprj"));
        {
            QFile pf(prj76a);
            CHECK_COND(pf.open(QIODevice::WriteOnly), "76a: write .sciprj");
            pf.write(w76->saveProjectToByteArray());
            pf.close();
        }
        ApplicationWindow *reader76a = new ApplicationWindow();
        reader76a->applyUserSettings();
        CHECK_COND(reader76a->loadProject(prj76a), "76a: loadProject failed");
        MultiLayer *ml76aRel = qobject_cast<MultiLayer *>(reader76a->window(ml76aName));
        CHECK_COND(ml76aRel && ml76aRel->activeGraph() && ml76aRel->activeGraph()->curve(0),
                   "76a: reloaded graph missing");
        QwtPlotCurve *cvRel = ml76aRel->activeGraph()->curve(0);
        CHECK_COND(std::fabs(cvRel->pen().widthF() - 1.5) < 1e-6
                           && std::fabs(cvRel->symbol().pen().widthF() - 0.5) < 1e-6,
                   "76a: .sciprj round-trip lost fractional 1.5 / 0.5 px widths");
        MultiLayer *ml76SpRel = qobject_cast<MultiLayer *>(reader76a->window(ml76SpName));
        CHECK_COND(ml76SpRel && ml76SpRel->activeGraph() && ml76SpRel->activeGraph()->curve(0),
                   "76a: reloaded spline graph missing");
        QwtPlotCurve *cvSpRel = ml76SpRel->activeGraph()->curve(0);
        CHECK_COND(std::fabs(cvSpRel->pen().widthF() - 1.0) < 1e-6
                           && std::fabs(cvSpRel->symbol().pen().widthF() - 2.5) < 1e-6,
                   "76a: Spline curve symbol border 2.5 px lost across .sciprj round-trip");
        // appendProject into a project that already owns Tbl76a (table rename changes curve title)
        ApplicationWindow *appendHost76a = new ApplicationWindow();
        appendHost76a->applyUserSettings();
        Table *clashTbl = appendHost76a->newTable(t76a->name(), 2, 2);
        CHECK_COND(clashTbl != nullptr, "76a: clash table");
        clashTbl->setName(t76a->name());
        appendHost76a->appendProject(prj76a);
        Folder *appendedFolder = appendHost76a->projectFolder()
                        && !appendHost76a->projectFolder()->folders().isEmpty()
                ? appendHost76a->projectFolder()->folders().last()
                : nullptr;
        CHECK_COND(appendedFolder != nullptr, "76a: appendProject failed");
        bool foundAppended15 = false;
        bool foundAppended25 = false;
        for (MyWidget *mwItem : appendedFolder->windowsList()) {
            if (MultiLayer *mlApp = qobject_cast<MultiLayer *>(mwItem)) {
                if (mlApp->activeGraph() && mlApp->activeGraph()->curve(0)) {
                    QwtPlotCurve *cApp = mlApp->activeGraph()->curve(0);
                    if (std::fabs(cApp->pen().widthF() - 1.5) < 1e-6
                        && std::fabs(cApp->symbol().pen().widthF() - 0.5) < 1e-6)
                        foundAppended15 = true;
                    if (std::fabs(cApp->pen().widthF() - 1.0) < 1e-6
                        && std::fabs(cApp->symbol().pen().widthF() - 2.5) < 1e-6)
                        foundAppended25 = true;
                }
            }
        }
        appendHost76a->savedProject();
        appendHost76a->setOleEmbeddedMode(true);
        delete appendHost76a;
        CHECK_COND(foundAppended15 && foundAppended25,
                   "76a: appendProject with table rename lost fractional curve/symbol widths");
        reader76a->savedProject();
        reader76a->setOleEmbeddedMode(true);
        delete reader76a;
    }

    // -------------------------------------------------------------------------
    // 76b: ApplicationWindow::window("NoSuchWindowName_76") returns nullptr
    // -------------------------------------------------------------------------
    CHECK_COND(w76->window(QStringLiteral("NoSuchWindowName_76")) == nullptr
                       && mw->window(QStringLiteral("NoSuchWindowName_76")) == nullptr,
               "76b: ApplicationWindow::window() must return nullptr for non-existent name");

    // -------------------------------------------------------------------------
    // 76c: WorkbookManager addSheet -> undo() -> redo() synchronization &
    //      saveWindowsToByteArray visibility promotion for background sheet
    // -------------------------------------------------------------------------
    {
        WorkbookManager *wm = WorkbookManager::instance(w76);
        CHECK_COND(wm != nullptr, "76c: WorkbookManager::instance null");
        QString wbErr;
        const QString bk = wm->createBook(QStringLiteral("Bk76"), 2, &wbErr);
        CHECK_COND(!bk.isEmpty() && wm->sheets(bk).size() == 2, "76c: createBook failed");
        Table *sh0 = wm->sheets(bk)[0];
        sh0->setColName(0, QStringLiteral("X"));
        sh0->setColName(1, QStringLiteral("Y"));
        for (int r = 0; r < 4; ++r) {
            sh0->setCell(r, 0, double(r + 1));
            sh0->setCell(r, 1, double((r + 1) * 10));
        }

        Table *sh2 = wm->addSheet(bk, QStringLiteral("S3"), -1, true, &wbErr);
        CHECK_COND(sh2 != nullptr && wm->sheets(bk).size() == 3 && wm->currentSheet(bk) == sh2,
                   "76c: addSheet failed");
        QApplication::processEvents();

        w76->activateSubWindow(sh2);
        w76->undo();
        QApplication::processEvents();
        CHECK_COND(wm->sheets(bk).size() == 2 && wm->bookOf(sh2).isEmpty()
                           && wm->currentSheet(bk) != nullptr
                           && wm->sheets(bk).contains(wm->currentSheet(bk)),
                   "76c: undo() after addSheet left dangling sheet or desynced workbook");

        w76->redo();
        QApplication::processEvents();
        CHECK_COND(wm->sheets(bk).size() == 3 && wm->bookOf(sh2) == bk
                           && wm->currentSheet(bk) == sh2,
                   "76c: redo() after addSheet failed to resync sheet and workbook");

        CHECK_COND(wm->setCurrentSheet(sh2, true), "76c: setCurrentSheet(sh2)");
        CHECK_COND(w76->hidden(sh0), "76c: background sheet sh0 expected hidden while sh2 is active");
        MultiLayer *mlBg = w76->multilayerPlot(
                sh0, QStringList() << (sh0->name() + QStringLiteral("_Y")), Graph::LineSymbols);
        CHECK_COND(mlBg != nullptr, "76c: plot from background sheet failed");
        CHECK_COND(wm->setCurrentSheet(sh2, true) && w76->hidden(sh0),
                   "76c: sh0 must remain background-hidden before mini-project export");

        const QByteArray miniBytes =
                w76->saveWindowsToByteArray(QList<MyWidget *>() << sh0 << mlBg);
        CHECK_COND(!miniBytes.isEmpty(), "76c: saveWindowsToByteArray returned empty");
        const QString miniPath = tmp76.filePath(QStringLiteral("suite76c_mini.sciprj"));
        {
            QFile mf(miniPath);
            CHECK_COND(mf.open(QIODevice::WriteOnly), "76c: open mini .sciprj");
            mf.write(miniBytes);
            mf.close();
        }
        ApplicationWindow *miniReader = new ApplicationWindow();
        miniReader->applyUserSettings();
        CHECK_COND(miniReader->loadProject(miniPath), "76c: load mini .sciprj failed");
        Table *sh0Rel = miniReader->table(sh0->name());
        CHECK_COND(sh0Rel != nullptr && !miniReader->hidden(sh0Rel)
                           && sh0Rel->status() != MyWidget::Hidden,
                   "76c: background source sheet was not promoted to visible in OLE mini-project");
        miniReader->savedProject();
        miniReader->setOleEmbeddedMode(true);
        delete miniReader;
    }

    // -------------------------------------------------------------------------
    // 76d: PoleFigure keeps isPoleFigurePlot() and suppresses Cartesian axes
    //      across isij_85mm and lab_slide_83mm presets (and clears when spectrogram removed)
    // -------------------------------------------------------------------------
    {
        const int nA = 16, nB = 73;
        Table *tp76 = w76->newTable(QStringLiteral("Pole76"), nA * nB, 3);
        CHECK_COND(tp76 != nullptr, "76d: pole table creation failed");
        tp76->setHeader(QStringList() << QStringLiteral("alpha") << QStringLiteral("beta")
                                      << QStringLiteral("I"));
        QVector<qreal> ca(nA * nB), cb(nA * nB), ci(nA * nB);
        int k = 0;
        for (int i = 0; i < nA; ++i) {
            for (int j = 0; j < nB; ++j, ++k) {
                ca[k] = 5.0 * i;
                cb[k] = 5.0 * j;
                ci[k] = suite67PoleIntensity(5.0 * i, 5.0 * j);
            }
        }
        tp76->column(0)->replaceValues(0, ca);
        tp76->column(1)->replaceValues(0, cb);
        tp76->column(2)->replaceValues(0, ci);

        ExtraPlotTypes::PoleFigureOptions po76;
        po76.label = QStringLiteral("(111)");
        const ExtraPlotTypes::PoleFigureResult pf76 =
                ExtraPlotTypes::plotPoleFigure(w76, tp76, QStringLiteral("alpha"),
                                               QStringLiteral("beta"), QStringLiteral("I"), po76);
        CHECK_COND(pf76.valid && pf76.plot && pf76.plot->activeGraph(), "76d: plotPoleFigure failed");
        Graph *gPf = pf76.plot->activeGraph();
        CHECK_COND(gPf->isPoleFigurePlot(), "76d: initial isPoleFigurePlot() false");

        for (const QString &presetId :
             { QStringLiteral("isij_85mm"), QStringLiteral("lab_slide_83mm") }) {
            CHECK_COND(pf76.plot->applyStylePresetById(presetId),
                       "76d: applyStylePresetById failed on pole figure");
            CHECK_COND(gPf->isPoleFigurePlot()
                               && !gPf->plotWidget()->axisEnabled(QwtPlot::xBottom)
                               && !gPf->plotWidget()->axisEnabled(QwtPlot::yLeft)
                               && !gPf->plotWidget()->axisEnabled(QwtPlot::xTop),
                       "76d: preset re-enabled Cartesian axes or broke isPoleFigurePlot()");
        }
    }

    // -------------------------------------------------------------------------
    // 76e: .xlsx with mid-column "N/A" string cell keeps all numeric rows above
    //      it, and Table::pasteText distinguishes 1-col thousands ("985\n1,012\n1,030")
    //      from 2-col CSV ("1,250\n2,375").
    // -------------------------------------------------------------------------
    {
        const QByteArray contentTypes =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"/>";
        const QByteArray rootRels =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
                "</Relationships>";
        const QByteArray workbook =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
                "<sheets><sheet name=\"MidNA\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";
        const QByteArray wbRels =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
                "</Relationships>";
        const QByteArray sheet1 =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>"
                "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Strain</t></is></c>"
                "<c r=\"B1\" t=\"inlineStr\"><is><t>Stress</t></is></c></row>"
                "<row r=\"2\"><c r=\"A2\"><v>0.1</v></c><c r=\"B2\"><v>120</v></c></row>"
                "<row r=\"3\"><c r=\"A3\"><v>0.2</v></c><c r=\"B3\"><v>240</v></c></row>"
                "<row r=\"4\"><c r=\"A4\"><v>0.3</v></c><c r=\"B4\"><v>350</v></c></row>"
                "<row r=\"5\"><c r=\"A5\"><v>0.4</v></c><c r=\"B5\" t=\"inlineStr\"><is><t>N/A</t></is></c></row>"
                "<row r=\"6\"><c r=\"A6\"><v>0.5</v></c><c r=\"B6\"><v>480</v></c></row>"
                "<row r=\"7\"><c r=\"A7\"><v>0.6</v></c><c r=\"B7\"><v>510</v></c></row>"
                "</sheetData></worksheet>";
        const QByteArray xlsxBytes = suite64BuildZip({
                { "[Content_Types].xml", contentTypes, true },
                { "_rels/.rels", rootRels, true },
                { "xl/workbook.xml", workbook, true },
                { "xl/_rels/workbook.xml.rels", wbRels, true },
                { "xl/worksheets/sheet1.xml", sheet1, true },
        });
        const QString xlsxPath = tmp76.filePath(QStringLiteral("mid_na_76.xlsx"));
        {
            QFile xf(xlsxPath);
            CHECK_COND(xf.open(QIODevice::WriteOnly), "76e: open mid_na_76.xlsx");
            xf.write(xlsxBytes);
            xf.close();
        }
        QString xlsxErr;
        XlsxImport::ImportResult xlsxRes;
        CHECK_COND(XlsxImport::importIntoProject(w76, xlsxPath, QString(), &xlsxErr, &xlsxRes, false)
                           && xlsxRes.tables.size() == 1,
                   "76e: XlsxImport::importIntoProject failed on mid-N/A workbook");
        Table *tXlsx = w76->table(xlsxRes.tables[0]);
        CHECK_COND(tXlsx != nullptr && tXlsx->numCols() == 2 && tXlsx->numRows() == 6
                           && tXlsx->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && tXlsx->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && tXlsx->column(1)->isInvalid(3),
                   "76e: mid-column 'N/A' discarded upper numeric rows or coerced column to Text!");
        CHECK_COND(std::fabs(tXlsx->cell(0, 0) - 0.1) < 1e-9
                           && std::fabs(tXlsx->cell(0, 1) - 120.0) < 1e-9
                           && std::fabs(tXlsx->cell(3, 0) - 0.4) < 1e-9
                           && std::fabs(tXlsx->cell(5, 0) - 0.6) < 1e-9
                           && std::fabs(tXlsx->cell(5, 1) - 510.0) < 1e-9,
                   "76e: imported values from mid-N/A .xlsx mismatch");

        // Preamble "Width | 10" + header row + mid "N/A" must keep header and numeric columns
        {
            const QByteArray sheetPreamble =
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                    "<sheetData>"
                    "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Width</t></is></c><c r=\"B1\"><v>10</v></c></row>"
                    "<row r=\"2\"><c r=\"A2\" t=\"inlineStr\"><is><t>Strain</t></is></c>"
                    "<c r=\"B2\" t=\"inlineStr\"><is><t>Stress</t></is></c></row>"
                    "<row r=\"3\"><c r=\"A3\"><v>0.1</v></c><c r=\"B3\"><v>120</v></c></row>"
                    "<row r=\"4\"><c r=\"A4\"><v>0.2</v></c><c r=\"B4\" t=\"inlineStr\"><is><t>N/A</t></is></c></row>"
                    "<row r=\"5\"><c r=\"A5\"><v>0.3</v></c><c r=\"B5\"><v>360</v></c></row>"
                    "</sheetData></worksheet>";
            const QByteArray preBytes = suite64BuildZip({
                    { "[Content_Types].xml", contentTypes, true },
                    { "_rels/.rels", rootRels, true },
                    { "xl/workbook.xml", workbook, true },
                    { "xl/_rels/workbook.xml.rels", wbRels, true },
                    { "xl/worksheets/sheet1.xml", sheetPreamble, true },
            });
            const QString prePath = tmp76.filePath(QStringLiteral("preamble_na_76.xlsx"));
            QFile pf(prePath);
            CHECK_COND(pf.open(QIODevice::WriteOnly), "76e: open preamble_na_76.xlsx");
            pf.write(preBytes);
            pf.close();
            XlsxImport::ImportResult preRes;
            CHECK_COND(XlsxImport::importIntoProject(w76, prePath, QString(), &xlsxErr, &preRes, false)
                               && preRes.tables.size() == 1,
                       "76e: preamble + mid-N/A xlsx import failed");
            Table *tPre = w76->table(preRes.tables[0]);
            CHECK_COND(tPre != nullptr && tPre->numCols() == 2 && tPre->numRows() == 3
                               && tPre->colLabel(0) == QStringLiteral("Strain")
                               && tPre->colLabel(1) == QStringLiteral("Stress")
                               && std::fabs(tPre->cell(0, 0) - 0.1) < 1e-9
                               && std::fabs(tPre->cell(2, 1) - 360.0) < 1e-9,
                       "76e: 'Width | 10' preamble caused header loss or text-column coercion");
        }

        Table *tPaste1 = w76->newTable(QStringLiteral("Paste1Col76"), 3, 2);
        CHECK_COND(tPaste1 != nullptr, "76e: Paste1Col76 table");
        tPaste1->clearCell(0, 1);
        tPaste1->clearCell(1, 1);
        tPaste1->clearCell(2, 1);
        tPaste1->setCellsSelected(0, 0, 0, 0, true);
        CHECK_COND(tPaste1->pasteText(QStringLiteral("985\n1,012\n1,030")),
                   "76e: pasteText 1-col thousands failed");
        CHECK_COND(std::fabs(tPaste1->cell(0, 0) - 985.0) < 1e-9
                           && std::fabs(tPaste1->cell(1, 0) - 1012.0) < 1e-9
                           && std::fabs(tPaste1->cell(2, 0) - 1030.0) < 1e-9
                           && tPaste1->column(1)->isInvalid(1),
                   "76e: '985\\n1,012\\n1,030' was not parsed as 1-column thousands-separated numbers");

        Table *tPaste2 = w76->newTable(QStringLiteral("Paste2Col76"), 2, 2);
        CHECK_COND(tPaste2 != nullptr, "76e: Paste2Col76 table");
        tPaste2->setCellsSelected(0, 0, 0, 0, true);
        CHECK_COND(tPaste2->pasteText(QStringLiteral("X,Y\n1,250\n2,375")),
                   "76e: pasteText 2-col CSV failed");
        CHECK_COND(std::fabs(tPaste2->cell(0, 0) - 1.0) < 1e-9
                           && std::fabs(tPaste2->cell(0, 1) - 250.0) < 1e-9
                           && std::fabs(tPaste2->cell(1, 0) - 2.0) < 1e-9
                           && std::fabs(tPaste2->cell(1, 1) - 375.0) < 1e-9,
                   "76e: 'X,Y\\n1,250\\n2,375' was misclassified instead of 2-column CSV");
    }

    // -------------------------------------------------------------------------
    // 76f: ReferenceLineSpec::visible=false & PointColorMap::showColorBar=false
    //      persisted across .sciprj save/reload and syncPointColorBar()
    // -------------------------------------------------------------------------
    {
        Table *t76f = w76->newTable(QStringLiteral("Tbl76f"), 5, 3);
        CHECK_COND(t76f != nullptr, "76f: table creation failed");
        t76f->setColName(0, QStringLiteral("X"));
        t76f->setColName(1, QStringLiteral("Y"));
        t76f->setColName(2, QStringLiteral("Z"));
        for (int r = 0; r < 5; ++r) {
            t76f->setCell(r, 0, double(r + 1));
            t76f->setCell(r, 1, double((r + 1) * 3));
            t76f->setCell(r, 2, double(100 + r * 25));
        }
        MultiLayer *ml76f = w76->multilayerPlot(
                t76f, QStringList() << (t76f->name() + QStringLiteral("_Y")), Graph::Scatter);
        CHECK_COND(ml76f && ml76f->activeGraph(), "76f: scatter graph failed");
        Graph *g76f = ml76f->activeGraph();
        DataCurve *dc76f = dynamic_cast<DataCurve *>(g76f->curve(0));
        CHECK_COND(dc76f != nullptr, "76f: DataCurve null");

        DataCurve::PointColorMap pcm;
        pcm.enabled = true;
        pcm.zColumn = t76f->name() + QStringLiteral("_Z");
        pcm.showColorBar = true;
        dc76f->setPointColorMap(pcm);
        g76f->syncPointColorBar();
        CHECK_COND(g76f->hasColorScaleOnAxis(QwtPlot::yRight),
                   "76f: color bar should be enabled when showColorBar=true");

        pcm.showColorBar = false;
        dc76f->setPointColorMap(pcm);
        g76f->syncPointColorBar();
        CHECK_COND(!g76f->hasColorScaleOnAxis(QwtPlot::yRight)
                           && !g76f->plotWidget()->axisWidget(QwtPlot::yRight)->isColorBarEnabled(),
                   "76f: syncPointColorBar() did not hide color bar when showColorBar=false");

        Graph::ReferenceLineSpec rlHidden;
        rlHidden.orientation = 0;
        rlHidden.value = 6.0;
        rlHidden.label = QStringLiteral("HiddenThreshold");
        rlHidden.visible = false;
        g76f->addReferenceLine(rlHidden);

        Graph::ReferenceLineSpec rlShown;
        rlShown.orientation = 1;
        rlShown.value = 3.0;
        rlShown.label = QStringLiteral("ShownMarker");
        rlShown.visible = true;
        g76f->addReferenceLine(rlShown);

        // Editing a hidden reference line in AxesDialog must preserve visible=false
        {
            Suite76AxesDialogProbe axRefDlg;
            axRefDlg.setGraph(g76f);
            axRefDlg.editSelectedRefLineColor(QColor(200, 40, 40));
            axRefDlg.acceptDialog();
            CHECK_COND(g76f->referenceLines().size() == 2
                               && !g76f->referenceLines()[0].visible
                               && g76f->referenceLines()[0].color == QColor(200, 40, 40),
                       "76f: AxesDialog reference-line edit inadvertently re-enabled hidden line");
        }

        const QString ml76fName = ml76f->name();
        const QString prj76f = tmp76.filePath(QStringLiteral("suite76f.sciprj"));
        {
            QFile pf(prj76f);
            CHECK_COND(pf.open(QIODevice::WriteOnly), "76f: open .sciprj");
            pf.write(w76->saveProjectToByteArray());
            pf.close();
        }
        ApplicationWindow *reader76f = new ApplicationWindow();
        reader76f->applyUserSettings();
        CHECK_COND(reader76f->loadProject(prj76f), "76f: loadProject failed");
        MultiLayer *ml76fRel = qobject_cast<MultiLayer *>(reader76f->window(ml76fName));
        CHECK_COND(ml76fRel && ml76fRel->activeGraph(), "76f: reloaded graph missing");
        Graph *g76fRel = ml76fRel->activeGraph();
        CHECK_COND(g76fRel->referenceLines().size() == 2
                           && !g76fRel->referenceLines()[0].visible
                           && g76fRel->referenceLines()[0].label == QStringLiteral("HiddenThreshold")
                           && g76fRel->referenceLines()[1].visible
                           && g76fRel->referenceLines()[1].label == QStringLiteral("ShownMarker"),
                   "76f: ReferenceLineSpec::visible or label lost across .sciprj round-trip");

        DataCurve *dc76fRel = dynamic_cast<DataCurve *>(g76fRel->curve(0));
        CHECK_COND(dc76fRel && dc76fRel->hasPointColorMap()
                           && !dc76fRel->pointColorMap().showColorBar,
                   "76f: PointColorMap::showColorBar=false lost across .sciprj round-trip");
        g76fRel->syncPointColorBar();
        CHECK_COND(!dc76fRel->pointColorMap().showColorBar
                           && !g76fRel->hasColorScaleOnAxis(QwtPlot::yRight)
                           && !g76fRel->plotWidget()->axisWidget(QwtPlot::yRight)->isColorBarEnabled(),
                   "76f: color bar re-appeared after reload / syncPointColorBar()");

        reader76f->savedProject();
        reader76f->setOleEmbeddedMode(true);
        delete reader76f;
    }

    w76->savedProject();
    w76->setOleEmbeddedMode(true);
    delete w76;
    QApplication::processEvents();
    std::cout << "[PASS] Suite 76 passed.\n";
    return 0;
}

// =============================================================================
// Suite 77: Nov 22 Night Review Regressions (P0-1..7, P1-D/N/G/S/O/R/U, P2)
// =============================================================================
int runSuite77Nov22NightReviewRegressions(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 77: Nov 22 night review security, Lorentz/ExpGrowth CI, "
                 "CSV/paste/xlsx, dotted export, zoom legend, graph undo remap, 200-sheet workbook & .gz safety...\n";

    struct Suite77EnvGuard
    {
        QVariant prevNonInteractive;
        bool prevTestMode = false;
        Suite77EnvGuard()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevTestMode(QStandardPaths::isTestModeEnabled())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QStandardPaths::setTestModeEnabled(true);
        }
        ~Suite77EnvGuard()
        {
            QStandardPaths::setTestModeEnabled(prevTestMode);
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        }
    } envGuard77;

    struct AppWinGuard
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard()
        {
            reset();
            QApplication::processEvents();
        }
        AppWinGuard(const AppWinGuard &) = delete;
        AppWinGuard &operator=(const AppWinGuard &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
        }
        ApplicationWindow *get() const { return win; }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    QTemporaryDir tmp77;
    CHECK_COND(tmp77.isValid(), "77: temp dir failed");

    AppWinGuard w77(new ApplicationWindow());
    w77->applyUserSettings();

    // -------------------------------------------------------------------------
    // 77a: P0-1 (no unsolicited TCP listener on ApplicationWindow ctor) &
    //      P0-2 / P1-R3 (AutoUpdater URL allowlist, System32 PowerShell, SHA256
    //      re-verification & full DLL rollback script)
    // -------------------------------------------------------------------------
    {
        PythonAutomationServer *py = w77->pythonAutomationServer();
        CHECK_COND(py != nullptr, "77a: pythonAutomationServer null");
        CHECK_COND(!py->isTcpListening() && !py->isLocalPipeListening(),
                   "77a: ApplicationWindow constructor must NOT open an unrequested RPC listener");

        CHECK_COND(!AutoUpdater::isAllowedUpdateUrl(QUrl::fromLocalFile(tmp77.filePath("evil.exe"))),
                   "77a: file:// URL must be rejected by AutoUpdater::isAllowedUpdateUrl");
        CHECK_COND(!AutoUpdater::isAllowedUpdateUrl(QUrl("http://github.com/kzykshmz/ToriGraph/releases")),
                   "77a: non-HTTPS external URL must be rejected");
        CHECK_COND(!AutoUpdater::isAllowedUpdateUrl(QUrl("https://evil.example.com/update.exe")),
                   "77a: untrusted HTTPS host must be rejected");
        CHECK_COND(AutoUpdater::isAllowedUpdateUrl(
                           QUrl("https://api.github.com/repos/kzykshmz/ToriGraph/releases/latest"))
                           && AutoUpdater::isAllowedUpdateUrl(QUrl(
                                   "https://objects.githubusercontent.com/github-production-release-asset/x.zip"))
                           && AutoUpdater::isAllowedUpdateUrl(QUrl("http://127.0.0.1:8080/mock")),
                   "77a: GitHub HTTPS and loopback test URLs must be accepted");

        const QString psExe = QDir::fromNativeSeparators(AutoUpdater::resolvePowerShellExecutable()).toLower();
        CHECK_COND(psExe.endsWith("/system32/windowspowershell/v1.0/powershell.exe")
                           && QFileInfo::exists(AutoUpdater::resolvePowerShellExecutable()),
                   "77a: resolvePowerShellExecutable() must resolve inside System32, not PATH");

        const QString dummyZip = tmp77.filePath("ToriGraph-9.9.0-win64-portable.zip");
        {
            QFile f(dummyZip);
            CHECK_COND(f.open(QIODevice::WriteOnly), "77a: dummy zip");
            f.write("PK-dummy");
            f.close();
        }
        const QString fakeSha(64, QLatin1Char('b'));
        QString psErr;
        const QString psPath = AutoUpdater::writeWindowsStagingUpdaterScript(
                dummyZip, tmp77.filePath("inst"), "ToriGraph.exe", 999999, false, &psErr, 2, fakeSha);
        CHECK_COND(!psPath.isEmpty() && QFileInfo::exists(psPath), "77a: staging script generation failed");
        QFile sf(psPath);
        CHECK_COND(sf.open(QIODevice::ReadOnly), "77a: open staging script");
        const QString psBody = QString::fromUtf8(sf.readAll());
        sf.close();
        CHECK_COND(psBody.contains(fakeSha) && psBody.contains("Get-FileHash")
                           && psBody.contains("exit 4") && psBody.contains("$BackupDir"),
                   "77a: staging script missing elevated SHA-256 re-verification or full directory rollback");
    }

    // -------------------------------------------------------------------------
    // 77b: P0-3 (MultiPeakFit Lorentz area SE & 95% CI parity with peak table),
    //      M1 (ExpGrowthFit confidence band parity with NonLinearFit),
    //      & P2 (Weighted fit SE consistency between parameters and peak summary)
    // -------------------------------------------------------------------------
    {
        const int nPts = 81;
        Table *tLor = w77->newTable(QStringLiteral("Lor77"), nPts, 3);
        CHECK_COND(tLor != nullptr, "77b: Lor77 table");
        tLor->setColName(0, QStringLiteral("X"));
        tLor->setColName(1, QStringLiteral("Y"));
        tLor->setColName(2, QStringLiteral("Err"));
        tLor->setColPlotDesignation(2, SciDAVis::yErr);
        for (int i = 0; i < nPts; ++i) {
            const double x = -10.0 + 0.25 * double(i);
            const double p1 = (2.0 / M_PI) * 100.0 * 2.0 / (4.0 * (x + 3.0) * (x + 3.0) + 4.0);
            const double p2 = (2.0 / M_PI) * 60.0 * 1.6 / (4.0 * (x - 3.5) * (x - 3.5) + 2.56);
            const double noise = 0.35 * std::sin(1.7 * double(i)) + 0.15 * std::cos(2.9 * double(i));
            tLor->setCell(i, 0, x);
            tLor->setCell(i, 1, 2.0 + p1 + p2 + noise);
            tLor->setCell(i, 2, 0.8);
        }
        MultiLayer *mlLor = w77->multilayerPlot(
                tLor, QStringList() << (tLor->name() + QStringLiteral("_Y")), Graph::Scatter);
        CHECK_COND(mlLor && mlLor->activeGraph(), "77b: Lorentz graph");
        Graph *gLor = mlLor->activeGraph();

        MultiPeakFit fitLor(w77, gLor, MultiPeakFit::Lorentz, 2);
        fitLor.setDataFromCurve(tLor->name() + QStringLiteral("_Y"));
        fitLor.enableConfidenceBand(true, 0.95);
        double initVals[7] = { 95.0, -3.0, 2.0, 58.0, 3.5, 1.6, 2.0 };
        fitLor.setInitialGuesses(initVals);
        fitLor.fit();

        const std::vector<double> &res = fitLor.results();
        const std::vector<double> &errs = fitLor.errors();
        const auto rows = fitLor.computePeakSummary(res);
        CHECK_COND(res.size() >= 7 && errs.size() >= 7 && rows.size() == 2,
                   "77b: Lorentz 2-peak fit did not produce 2 summary rows");
        const double relDiff0 = std::fabs(errs[0] - rows[0].areaSE) / std::max(1e-12, rows[0].areaSE);
        const double relDiff1 = std::fabs(errs[3] - rows[1].areaSE) / std::max(1e-12, rows[1].areaSE);
        std::cout << "[INFO] 77b Lorentz Area SE: param0=" << errs[0] << " summary0=" << rows[0].areaSE
                  << ", param1=" << errs[3] << " summary1=" << rows[1].areaSE << "\n";
        CHECK_COND(relDiff0 < 1e-4 && relDiff1 < 1e-4
                           && std::fabs(fitLor.parameterStdError(0) - rows[0].areaSE) < 1e-4,
                   "77b: Lorentz parameter Area SE diverged from peak summary Area SE (2/pi bug)");

        //Weighted fit with scaleErrors=false: peak summary SE must match parameter SE
        MultiPeakFit fitWeighted(w77, gLor, MultiPeakFit::Gauss, 2);
        fitWeighted.setDataFromCurve(tLor->name() + QStringLiteral("_Y"));
        CHECK_COND(fitWeighted.setWeightingMethod(Fit::WeightDirect, tLor->name() + QStringLiteral("_Err")),
                   "77b: setWeightingMethod failed");
        fitWeighted.scaleErrors(false);
        double gInit[7] = { 95.0, -3.0, 2.0, 58.0, 3.5, 1.6, 2.0 };
        fitWeighted.setInitialGuesses(gInit);
        fitWeighted.fit();
        const auto wErrs = fitWeighted.errors();
        const auto wRows = fitWeighted.computePeakSummary(fitWeighted.results());
        CHECK_COND(wErrs.size() >= 7 && wRows.size() == 2, "77b: weighted Gauss fit failed");
        const double wRelDiff = std::fabs(wErrs[0] - wRows[0].areaSE) / std::max(1e-12, wRows[0].areaSE);
        CHECK_COND(wRelDiff < 1e-4,
                   "77b: weighted fit (!scaleErrors) scaled peak summary SE differently from parameter SE");

        // M1 regression gate: ExponentialFit (expGrowth=true: internal lambda < 0, stored t = -1/lambda > 0)
        // confidence band must match NonLinearFit("y0+A*exp(x/t)") without off-diagonal covariance sign corruption
        const int nExp = 31;
        Table *tExp = w77->newTable(QStringLiteral("ExpGr77"), nExp, 2);
        CHECK_COND(tExp != nullptr, "77b: ExpGr77 table");
        tExp->setColName(0, QStringLiteral("X"));
        tExp->setColName(1, QStringLiteral("Y"));
        for (int i = 0; i < nExp; ++i) {
            const double x = 0.2 * double(i);
            const double noise = 0.45 * std::sin(1.9 * double(i)) - 0.25 * std::cos(2.7 * double(i));
            tExp->setCell(i, 0, x);
            tExp->setCell(i, 1, 5.0 + 2.5 * std::exp(x / 2.2) + noise);
        }
        const QString expCurveTitle = tExp->name() + QStringLiteral("_Y");
        MultiLayer *mlExp1 = w77->multilayerPlot(tExp, QStringList() << expCurveTitle, Graph::Scatter);
        MultiLayer *mlExp2 = w77->multilayerPlot(tExp, QStringList() << expCurveTitle, Graph::Scatter);
        CHECK_COND(mlExp1 && mlExp1->activeGraph() && mlExp2 && mlExp2->activeGraph(),
                   "77b: ExpGr77 graphs");
        Graph *gExp1 = mlExp1->activeGraph();
        Graph *gExp2 = mlExp2->activeGraph();

        ExponentialFit expGrFit(w77, gExp1, expCurveTitle, true /*expGrowth*/);
        expGrFit.generateFunction(false);
        expGrFit.enableConfidenceBand(true, 0.95);
        double expInitInternal[3] = { 2.4, -1.0 / 2.1, 4.8 };
        expGrFit.setInitialGuesses(expInitInternal);
        expGrFit.fit();
        CHECK_COND(expGrFit.results().size() >= 3 && expGrFit.results()[1] > 1.5,
                   "77b: ExpGrowth fit failed");

        NonLinearFit nlfExp(w77, gExp2, expCurveTitle);
        nlfExp.setParametersList(QStringList() << "A" << "t" << "y0");
        nlfExp.setFormula("y0+A*exp(x/t)");
        nlfExp.generateFunction(false);
        nlfExp.enableConfidenceBand(true, 0.95);
        double nlfInit[3] = { 2.4, 2.1, 4.8 };
        nlfExp.setInitialGuesses(nlfInit);
        nlfExp.fit();

        const int idxUp1 = gExp1->curveIndex(expCurveTitle + QStringLiteral("_ConfUpper"));
        const int idxLo1 = gExp1->curveIndex(expCurveTitle + QStringLiteral("_ConfLower"));
        const int idxUp2 = gExp2->curveIndex(expCurveTitle + QStringLiteral("_ConfUpper"));
        const int idxLo2 = gExp2->curveIndex(expCurveTitle + QStringLiteral("_ConfLower"));
        CHECK_COND(idxUp1 >= 0 && idxLo1 >= 0 && idxUp2 >= 0 && idxLo2 >= 0,
                   "77b: ExpGrowth or NonLinearFit did not insert _ConfUpper/_ConfLower band curves");
        const QwtPlotCurve *cUp1 = gExp1->curve(idxUp1);
        const QwtPlotCurve *cLo1 = gExp1->curve(idxLo1);
        const QwtPlotCurve *cUp2 = gExp2->curve(idxUp2);
        const QwtPlotCurve *cLo2 = gExp2->curve(idxLo2);
        CHECK_COND(cUp1 && cLo1 && cUp2 && cLo2 && int(cUp1->dataSize()) == nExp
                           && int(cUp2->dataSize()) == nExp,
                   "77b: ExpGrowth confidence band point count mismatch");
        double maxBandRelDiff = 0.0;
        for (int k = 0; k < nExp; ++k) {
            const double w1 = cUp1->y(k) - cLo1->y(k);
            const double w2 = cUp2->y(k) - cLo2->y(k);
            CHECK_COND(w1 > 0.0 && w2 > 0.0 && std::isfinite(w1) && std::isfinite(w2),
                       "77b: non-positive or NaN confidence band width on ExpGrowth");
            const double rel = std::fabs(w1 - w2) / std::max(1e-9, w2);
            maxBandRelDiff = std::max(maxBandRelDiff, rel);
        }
        std::cout << "[INFO] 77b ExpGrowth vs NonLinearFit 95% CI band max rel diff = "
                  << maxBandRelDiff << "\n";
        CHECK_COND(maxBandRelDiff < 5e-3,
                   "77b: M1 ExpGrowth confidence band diverged from NonLinearFit (flipped Jacobian sign bug): "
                           << maxBandRelDiff);
    }

    // -------------------------------------------------------------------------
    // 77c: P0-4 (# specimen rows), M3 (mid-file single-cell '# sensor note'),
    //      P0-5 & M2 (thousands single-col vs explicit 2-col CSV paste),
    //      M4 (short column empty-cell EditRole), P1-D1..D8, P2 Rigaku .ras
    // -------------------------------------------------------------------------
    {
        // P0-4: '#' comment in preamble skipped, '#1,520' specimen rows in body preserved
        const QByteArray hashCsv =
                "# Tensile summary log\n"
                "Specimen,UTS_MPa\n"
                "#1,520\n"
                "#2,515\n"
                "#3,528\n";
        AsciiTableImportFilter::ImportReport repHash;
        std::unique_ptr<future::Table> ftHash(AsciiTableImportFilter::importDroppedTextData(
                hashCsv, QStringLiteral("HashSpec"), QStringLiteral("spec.csv"), &repHash));
        CHECK_COND(ftHash && ftHash->columnCount() == 2 && ftHash->rowCount() == 3
                           && ftHash->column(0)->textAt(0) == QStringLiteral("#1")
                           && std::fabs(ftHash->column(1)->valueAt(0) - 520.0) < 1e-9
                           && std::fabs(ftHash->column(1)->valueAt(2) - 528.0) < 1e-9,
                   "77c: P0-4 mid-file '#1,520' specimen rows were silently dropped as comments!");

        // M3: single-cell '# sensor note' (and header-to-data '# comment') in multi-column CSV
        // must be skipped as comments without turning numeric columns into Text!
        const QByteArray midNoteCsv =
                "x,y\n"
                "# header-to-data comment\n"
                "1.5,10.25\n"
                "# sensor note: recalibrated\n"
                "2.5,20.50\n";
        std::unique_ptr<future::Table> ftMidNote(AsciiTableImportFilter::importDroppedTextData(
                midNoteCsv, QStringLiteral("MidNote77"), QStringLiteral("midnote.csv"), nullptr));
        CHECK_COND(ftMidNote && ftMidNote->columnCount() == 2 && ftMidNote->rowCount() == 2
                           && ftMidNote->column(0)->name() == QStringLiteral("x")
                           && ftMidNote->column(1)->name() == QStringLiteral("y")
                           && ftMidNote->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && ftMidNote->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && std::fabs(ftMidNote->column(0)->valueAt(0) - 1.5) < 1e-9
                           && std::fabs(ftMidNote->column(1)->valueAt(1) - 20.5) < 1e-9,
                   "77c: M3 mid-file single-cell '# sensor note' corrupted numeric CSV columns!");

        // P1-D4 (a): all-integer CSV must NOT consume row 0 as column headers
        const QByteArray intCsv = "1,100\n2,200\n3,300\n";
        std::unique_ptr<future::Table> ftInt(AsciiTableImportFilter::importDroppedTextData(
                intCsv, QStringLiteral("IntCsv"), QStringLiteral("ints.csv"), nullptr));
        CHECK_COND(ftInt && ftInt->columnCount() == 2 && ftInt->rowCount() == 3
                           && std::fabs(ftInt->column(0)->valueAt(0) - 1.0) < 1e-9
                           && std::fabs(ftInt->column(1)->valueAt(0) - 100.0) < 1e-9,
                   "77c: P1-D4 all-integer CSV consumed first data row as header!");

        // Track 2 M2: Excel trailing-comma '# pause,' / '# note,,' and multi-cell text
        // '# T=300 K, P=1 atm' comments skipped while '#1,520' specimen rows are preserved
        const QByteArray excelHashMixCsv =
                "Specimen,UTS_MPa\n"
                "# pause,\n"
                "# T=300 K, P=1 atm\n"
                "#1,520\n"
                "# note,,\n"
                "#2,515\n"
                "# end of test, operator=A\n";
        AsciiTableImportFilter::ImportReport repExcelHash;
        std::unique_ptr<future::Table> ftExcelHash(AsciiTableImportFilter::importDroppedTextData(
                excelHashMixCsv, QStringLiteral("ExcelHashMix77"), QStringLiteral("excel_hash.csv"),
                &repExcelHash));
        CHECK_COND(ftExcelHash && ftExcelHash->columnCount() == 2 && ftExcelHash->rowCount() == 2
                           && ftExcelHash->column(0)->textAt(0) == QStringLiteral("#1")
                           && ftExcelHash->column(0)->textAt(1) == QStringLiteral("#2")
                           && ftExcelHash->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && std::fabs(ftExcelHash->column(1)->valueAt(0) - 520.0) < 1e-9
                           && std::fabs(ftExcelHash->column(1)->valueAt(1) - 515.0) < 1e-9
                           && repExcelHash.skippedLines == 4,
                   "77c: M2 mixed '# pause,' / '# T=300 K, P=1 atm' vs '#1,520' CSV failed (rows="
                           << (ftExcelHash ? ftExcelHash->rowCount() : -1)
                           << ", skipped=" << repExcelHash.skippedLines << ")!");

        // Unified '#'-leading row rules across Step 3, detectPreamble, peek & body streaming:
        // (1) All-missing '#N/A,#N/A' row preserved as missing data row (never comment / preamble break)
        {
            AsciiTableImportFilter::ImportReport repNa;
            std::unique_ptr<future::Table> ftNa(AsciiTableImportFilter::importDroppedTextData(
                    QByteArray("x,y\n1,2\n#N/A,#N/A\n3,4\n"), QStringLiteral("NaRow77"),
                    QStringLiteral("na_row.csv"), &repNa));
            CHECK_COND(ftNa && ftNa->columnCount() == 2 && ftNa->rowCount() == 3
                               && ftNa->column(0)->name() == QStringLiteral("x")
                               && ftNa->column(1)->name() == QStringLiteral("y")
                               && ftNa->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && ftNa->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && std::fabs(ftNa->column(0)->valueAt(0) - 1.0) < 1e-9
                               && ftNa->column(0)->isInvalid(1) && ftNa->column(1)->isInvalid(1)
                               && std::fabs(ftNa->column(1)->valueAt(2) - 4.0) < 1e-9
                               && repNa.skippedLines == 0,
                       "77c: 'x,y\\n1,2\\n#N/A,#N/A\\n3,4' dropped or misclassified #N/A,#N/A row!");
        }
        // (2) '#DIV/0!,' trailing-empty error row preserved as missing data row
        {
            AsciiTableImportFilter::ImportReport repDiv;
            std::unique_ptr<future::Table> ftDiv(AsciiTableImportFilter::importDroppedTextData(
                    QByteArray("x,y\n1,2\n#DIV/0!,\n3,4\n"), QStringLiteral("DivRow77"),
                    QStringLiteral("div_row.csv"), &repDiv));
            CHECK_COND(ftDiv && ftDiv->columnCount() == 2 && ftDiv->rowCount() == 3
                               && ftDiv->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && ftDiv->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && std::fabs(ftDiv->column(0)->valueAt(0) - 1.0) < 1e-9
                               && ftDiv->column(0)->isInvalid(1) && ftDiv->column(1)->isInvalid(1)
                               && std::fabs(ftDiv->column(1)->valueAt(2) - 4.0) < 1e-9
                               && repDiv.skippedLines == 0,
                       "77c: 'x,y\\n1,2\\n#DIV/0!,\\n3,4' dropped or misclassified #DIV/0!, row!");
        }
        // (3) 'ID,Load\n#1,520\n#2,\n#3,540' preserves '#2,' specimen row with missing Load
        {
            AsciiTableImportFilter::ImportReport repId;
            std::unique_ptr<future::Table> ftId(AsciiTableImportFilter::importDroppedTextData(
                    QByteArray("ID,Load\n#1,520\n#2,\n#3,540\n"), QStringLiteral("IdLoad77"),
                    QStringLiteral("id_load.csv"), &repId));
            CHECK_COND(ftId && ftId->columnCount() == 2 && ftId->rowCount() == 3
                               && ftId->column(0)->name() == QStringLiteral("ID")
                               && ftId->column(1)->name() == QStringLiteral("Load")
                               && ftId->column(0)->textAt(0) == QStringLiteral("#1")
                               && ftId->column(0)->textAt(1) == QStringLiteral("#2")
                               && ftId->column(0)->textAt(2) == QStringLiteral("#3")
                               && ftId->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && std::fabs(ftId->column(1)->valueAt(0) - 520.0) < 1e-9
                               && ftId->column(1)->isInvalid(1)
                               && std::fabs(ftId->column(1)->valueAt(2) - 540.0) < 1e-9
                               && repId.skippedLines == 0,
                       "77c: 'ID,Load\\n#1,520\\n#2,\\n#3,540' dropped '#2,' specimen row!");
        }
        // (4) 'x,y\n1,510\n#2,520\n3,530' via w77->open(csvPath) (detectPreamble = true)
        {
            const QString openMidHashPath = tmp77.filePath(QStringLiteral("OpenMidHash77.csv"));
            QFile omf(openMidHashPath);
            CHECK_COND(omf.open(QIODevice::WriteOnly), "77c: write OpenMidHash77.csv");
            omf.write("x,y\n1,510\n#2,520\n3,530\n");
            omf.close();
            CHECK_COND(w77->open(openMidHashPath) == w77, "77c: w77->open(OpenMidHash77.csv) failed");
            Table *tOpenMid = w77->table(QStringLiteral("OpenMidHash771"));
            if (!tOpenMid)
                tOpenMid = w77->table(QStringLiteral("OpenMidHash77"));
            CHECK_COND(tOpenMid && tOpenMid->numCols() == 2 && tOpenMid->numRows() == 3
                               && tOpenMid->column(0)->textAt(0) == QStringLiteral("1")
                               && tOpenMid->column(0)->textAt(1) == QStringLiteral("#2")
                               && tOpenMid->column(0)->textAt(2) == QStringLiteral("3")
                               && tOpenMid->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && std::fabs(tOpenMid->cell(0, 1) - 510.0) < 1e-9
                               && std::fabs(tOpenMid->cell(1, 1) - 520.0) < 1e-9
                               && std::fabs(tOpenMid->cell(2, 1) - 530.0) < 1e-9,
                       "77c: w77->open('x,y\\n1,510\\n#2,520\\n3,530') ate rows in detectPreamble!");
        }
        // (4b) '#x,y\n1,2\n#comment,abc\n3,4' promotes 'x','y' headers and skips mid-file '#comment,abc'
        {
            AsciiTableImportFilter::ImportReport repNoSpcHdr;
            std::unique_ptr<future::Table> ftNoSpcHdr(
                    AsciiTableImportFilter::importDroppedTextData(
                            QByteArray("#x,y\n1,2\n#comment,abc\n3,4\n"),
                            QStringLiteral("HashNoSpcHdr77"), QStringLiteral("nospc_hdr.csv"),
                            &repNoSpcHdr));
            CHECK_COND(ftNoSpcHdr && ftNoSpcHdr->columnCount() == 2
                               && ftNoSpcHdr->rowCount() == 2
                               && ftNoSpcHdr->column(0)->name() == QStringLiteral("x")
                               && ftNoSpcHdr->column(1)->name() == QStringLiteral("y")
                               && ftNoSpcHdr->column(0)->columnMode()
                                       == SciDAVis::ColumnMode::Numeric
                               && ftNoSpcHdr->column(1)->columnMode()
                                       == SciDAVis::ColumnMode::Numeric
                               && std::fabs(ftNoSpcHdr->column(0)->valueAt(0) - 1.0) < 1e-9
                               && std::fabs(ftNoSpcHdr->column(1)->valueAt(1) - 4.0) < 1e-9
                               && repNoSpcHdr.headerLines == 1 && repNoSpcHdr.skippedLines == 1,
                       "77c: '#x,y\\n1,2\\n#comment,abc\\n3,4' failed header promotion or mid-file comment skip!");
        }
        // (4c) 1-column whitespace '#1\n#2\n#3' keeps all 3 specimen rows
        {
            std::unique_ptr<future::Table> ftOneColHash(
                    AsciiTableImportFilter::importDroppedTextData(
                            QByteArray("#1\n#2\n#3\n"), QStringLiteral("OneColHash77"),
                            QStringLiteral("one_col_hash.dat"), nullptr));
            CHECK_COND(ftOneColHash && ftOneColHash->columnCount() == 1
                               && ftOneColHash->rowCount() == 3
                               && ftOneColHash->column(0)->textAt(0) == QStringLiteral("#1")
                               && ftOneColHash->column(0)->textAt(1) == QStringLiteral("#2")
                               && ftOneColHash->column(0)->textAt(2) == QStringLiteral("#3"),
                       "77c: 1-column '#1\\n#2\\n#3' dropped '#<digit>' rows!");
        }
        // (5) Headerless '#1,520\n#2,530' keeps both rows as data (neither skipped nor consumed as header)
        {
            std::unique_ptr<future::Table> ftHdrlessHash(
                    AsciiTableImportFilter::importDroppedTextData(
                            QByteArray("#1,520\n#2,530\n"), QStringLiteral("HdrlessHash77"),
                            QStringLiteral("hdrless_hash.csv"), nullptr));
            CHECK_COND(ftHdrlessHash && ftHdrlessHash->columnCount() == 2
                               && ftHdrlessHash->rowCount() == 2
                               && ftHdrlessHash->column(0)->textAt(0) == QStringLiteral("#1")
                               && ftHdrlessHash->column(0)->textAt(1) == QStringLiteral("#2")
                               && ftHdrlessHash->column(1)->columnMode()
                                       == SciDAVis::ColumnMode::Numeric
                               && std::fabs(ftHdrlessHash->column(1)->valueAt(0) - 520.0) < 1e-9
                               && std::fabs(ftHdrlessHash->column(1)->valueAt(1) - 530.0) < 1e-9,
                       "77c: headerless '#1,520\\n#2,530' dropped row 0 as comment or column header!");
        }
        // (6) TSV '# 1\t2\n3\t4\n5\t6' skips '# 1\t2' as comment and keeps 2 numeric rows
        {
            AsciiTableImportFilter::ImportReport repTsv;
            std::unique_ptr<future::Table> ftTsv(AsciiTableImportFilter::importDroppedTextData(
                    QByteArray("# 1\t2\n3\t4\n5\t6\n"), QStringLiteral("HashSpaceTsv77"),
                    QStringLiteral("hash_space.tsv"), &repTsv));
            CHECK_COND(ftTsv && ftTsv->columnCount() == 2 && ftTsv->rowCount() == 2
                               && ftTsv->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && ftTsv->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && std::fabs(ftTsv->column(0)->valueAt(0) - 3.0) < 1e-9
                               && std::fabs(ftTsv->column(1)->valueAt(1) - 6.0) < 1e-9
                               && repTsv.skippedLines == 1,
                       "77c: TSV '# 1\\t2\\n3\\t4\\n5\\t6' failed to skip '# 1\\t2' as comment!");
        }
        // (7) Whitespace SPEC '#S 2 ascan th 10 20\n#N 2\n10 100\n20 200' skips #S & #N directives
        {
            AsciiTableImportFilter::ImportReport repSpec;
            std::unique_ptr<future::Table> ftSpec(AsciiTableImportFilter::importDroppedTextData(
                    QByteArray("#S 2 ascan th 10 20\n#N 2\n10 100\n20 200\n"),
                    QStringLiteral("SpecScan77"), QStringLiteral("spec_scan.dat"), &repSpec));
            CHECK_COND(ftSpec && ftSpec->columnCount() == 2 && ftSpec->rowCount() == 2
                               && ftSpec->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && ftSpec->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                               && std::fabs(ftSpec->column(0)->valueAt(0) - 10.0) < 1e-9
                               && std::fabs(ftSpec->column(1)->valueAt(0) - 100.0) < 1e-9
                               && std::fabs(ftSpec->column(0)->valueAt(1) - 20.0) < 1e-9
                               && std::fabs(ftSpec->column(1)->valueAt(1) - 200.0) < 1e-9
                               && repSpec.skippedLines == 2,
                       "77c: SPEC '#S 2 ascan...\\n#N 2\\n10 100\\n20 200' misparsed directives!");
        }

        // Track 2 M3: 2-column '1,2\n1.5,2.8\n2,3.1' must keep all 3 rows as numeric data
        const QByteArray twoColSeqCsv = "1,2\n1.5,2.8\n2,3.1\n";
        std::unique_ptr<future::Table> ftTwoColSeq(AsciiTableImportFilter::importDroppedTextData(
                twoColSeqCsv, QStringLiteral("TwoColSeq77"), QStringLiteral("seq2.csv"), nullptr));
        CHECK_COND(ftTwoColSeq && ftTwoColSeq->columnCount() == 2 && ftTwoColSeq->rowCount() == 3
                           && std::fabs(ftTwoColSeq->column(0)->valueAt(0) - 1.0) < 1e-9
                           && std::fabs(ftTwoColSeq->column(1)->valueAt(0) - 2.0) < 1e-9
                           && std::fabs(ftTwoColSeq->column(0)->valueAt(1) - 1.5) < 1e-9
                           && std::fabs(ftTwoColSeq->column(1)->valueAt(2) - 3.1) < 1e-9,
                   "77c: M3 2-column '1,2\\n1.5,2.8\\n2,3.1' dropped first data row as seq header!");

        // Track 2 M3 (b): ignoredLines > 0 above '0,1000\n0.1,998.5' must NOT drop row 0
        {
            const QString skipZeroPath = tmp77.filePath(QStringLiteral("skip_zero77.csv"));
            QFile szf(skipZeroPath);
            CHECK_COND(szf.open(QIODevice::WriteOnly), "77c: write skip_zero77.csv");
            szf.write("Instrument metadata line\n0,1000\n0.1,998.5\n");
            szf.close();
            AsciiTableImportFilter skipFlt;
            skipFlt.set_ignored_lines(1);
            skipFlt.set_separator(QStringLiteral(","));
            skipFlt.set_first_row_names_columns(true);
            skipFlt.set_trim_whitespace(true);
            skipFlt.set_convert_to_numeric(true);
            skipFlt.set_numeric_locale(QLocale::c());
            QFile szIn(skipZeroPath);
            CHECK_COND(szIn.open(QIODevice::ReadOnly), "77c: open skip_zero77.csv");
            std::unique_ptr<AbstractAspect> szAsp(skipFlt.importAspect(szIn));
            szIn.close();
            future::Table *ftSz = dynamic_cast<future::Table *>(szAsp.get());
            CHECK_COND(ftSz && ftSz->columnCount() == 2 && ftSz->rowCount() == 2
                               && std::fabs(ftSz->column(0)->valueAt(0) - 0.0) < 1e-9
                               && std::fabs(ftSz->column(1)->valueAt(0) - 1000.0) < 1e-9,
                       "77c: M3 ignoredLines > 0 dropped '0,1000' initial data row!");
        }

        // P1-D4 (b): mixed text + numeric header ("Time (s),300,400")
        const QByteArray mixCsv = "Time (s),300,400\n0,1.2,2.4\n10,1.5,3.1\n";
        std::unique_ptr<future::Table> ftMix(AsciiTableImportFilter::importDroppedTextData(
                mixCsv, QStringLiteral("MixHdr"), QStringLiteral("mix.csv"), nullptr));
        CHECK_COND(ftMix && ftMix->columnCount() == 3 && ftMix->rowCount() == 2
                           && ftMix->column(1)->name() == QStringLiteral("300")
                           && ftMix->column(2)->name() == QStringLiteral("400")
                           && ftMix->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && std::fabs(ftMix->column(2)->valueAt(1) - 3.1) < 1e-9,
                   "77c: P1-D4 mixed text+numeric header ('Time (s),300,400') failed!");

        // P2 Rigaku .ras 3rd column Factor multiplies Intensity while keeping 3 columns
        const QByteArray rasData =
                "*RAS_DATA_START\n"
                "*RAS_HEADER_START\n"
                "*RAS_HEADER_END\n"
                "*RAS_INT_START\n"
                "38.50 120.0 10.0000\n"
                "38.52 250.0 1.0000\n"
                "*RAS_INT_END\n"
                "*RAS_DATA_END\n";
        std::unique_ptr<future::Table> ftRas(AsciiTableImportFilter::importDroppedTextData(
                rasData, QStringLiteral("RasFactor"), QStringLiteral("al.ras"), nullptr));
        CHECK_COND(ftRas && ftRas->columnCount() == 3 && ftRas->rowCount() == 2
                           && std::fabs(ftRas->column(1)->valueAt(0) - 1200.0) < 1e-9
                           && std::fabs(ftRas->column(1)->valueAt(1) - 250.0) < 1e-9,
                   "77c: Rigaku .ras attenuator Factor was not applied to Intensity!");

        // P0-5: all-thousands single column ("12,345\n23,456") when 1 cell/column selected
        Table *tAllThou = w77->newTable(QStringLiteral("AllThou77"), 3, 2);
        tAllThou->clearCell(0, 1);
        tAllThou->clearCell(1, 1);
        tAllThou->setCellsSelected(0, 0, 0, 0, true);
        CHECK_COND(tAllThou->pasteText(QStringLiteral("12,345\n23,456")),
                   "77c: pasteText '12,345\\n23,456' failed");
        CHECK_COND(std::fabs(tAllThou->cell(0, 0) - 12345.0) < 1e-9
                           && std::fabs(tAllThou->cell(1, 0) - 23456.0) < 1e-9
                           && tAllThou->column(1)->isInvalid(0),
                   "77c: P0-5 '12,345\\n23,456' split into 2 columns instead of 1 thousands-grouped column!");

        // M2: explicit 2-column selection must split multi-row "10,200\n20,400" into 2 CSV columns
        Table *tTwoColCsv = w77->newTable(QStringLiteral("TwoColCsv77"), 3, 2);
        tTwoColCsv->setCellsSelected(0, 0, 1, 1, true);
        CHECK_COND(tTwoColCsv->pasteText(QStringLiteral("10,200\n20,400")),
                   "77c: pasteText '10,200\\n20,400' with 2 selected columns failed");
        CHECK_COND(std::fabs(tTwoColCsv->cell(0, 0) - 10.0) < 1e-9
                           && std::fabs(tTwoColCsv->cell(0, 1) - 200.0) < 1e-9
                           && std::fabs(tTwoColCsv->cell(1, 0) - 20.0) < 1e-9
                           && std::fabs(tTwoColCsv->cell(1, 1) - 400.0) < 1e-9,
                   "77c: M2 multi-row '10,200\\n20,400' with >=2 selected columns failed to split into 2 columns!");

        // P1-D8 + P1-D2: Excel Alt+Enter quoted multiline headers + #N/A / #DIV/0! / N.D. missing markers
        Table *tExcel = w77->newTable(QStringLiteral("ExcelPaste77"), 4, 2);
        tExcel->setCellsSelected(0, 0, 0, 0, true);
        CHECK_COND(tExcel->pasteText(QStringLiteral(
                           "\"Nominal stress\n(MPa)\"\t\"Strain\n(%)\"\n"
                           "120.5\t0.15\n"
                           "#N/A\t#DIV/0!\n"
                           "245.0\tN.D.")),
                   "77c: pasteText multiline header + Excel errors failed");
        CHECK_COND(tExcel->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && tExcel->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && std::fabs(tExcel->cell(0, 0) - 120.5) < 1e-9
                           && tExcel->column(0)->isInvalid(1)
                           && tExcel->column(1)->isInvalid(1)
                           && std::fabs(tExcel->cell(2, 0) - 245.0) < 1e-9
                           && tExcel->column(1)->isInvalid(2),
                   "77c: P1-D2 / P1-D8 multiline header or #N/A/#DIV/0!/N.D. broke numeric column paste!");

        // P1-D6 + M4: EditRole preserves 12-digit precision AND returns empty QVariant beyond short column rowCount()
        tExcel->column(0)->setValueAt(0, 1.234567890123);
        tExcel->column(1)->removeRows(2, tExcel->column(1)->rowCount() - 2);
        CHECK_COND(tExcel->column(1)->rowCount() == 2 && tExcel->numRows() >= 4,
                   "77c: setup short column rowCount() == 2 failed");
        if (QWidget *vw = tExcel->d_future_table->view()) {
            QTableView *tv = vw->findChild<QTableView *>();
            QAbstractItemModel *mdl = tv ? tv->model() : nullptr;
            CHECK_COND(mdl != nullptr, "77c: TableModel null");
            const QModelIndex idx00 = mdl->index(0, 0);
            const QVariant editVal = mdl->data(idx00, Qt::EditRole);
            mdl->setData(idx00, editVal, Qt::EditRole);
            CHECK_COND(std::fabs(tExcel->column(0)->valueAt(0) - 1.234567890123) < 1e-12,
                       "77c: P1-D6 EditRole rounded cell value to 6 significant digits!");

            const QModelIndex idxBelowShortCol = mdl->index(3, 1);
            const QVariant emptyBelow = mdl->data(idxBelowShortCol, Qt::EditRole);
            CHECK_COND(!emptyBelow.isValid(),
                       "77c: M4 EditRole below short column rowCount() returned '"
                               << emptyBelow.toString().toStdString() << "' instead of empty QVariant!");
            CHECK_COND(!mdl->setData(idxBelowShortCol, QString(), Qt::EditRole)
                               && tExcel->column(1)->rowCount() == 2,
                       "77c: M4 empty setData below short column rowCount() mutated column!");
        }

        // P1-D5: sorting column containing NaNs places finite values in strict order and NaNs at end
        Table *tSort = w77->newTable(QStringLiteral("NaNSort77"), 5, 1);
        tSort->column(0)->setValueAt(0, 30.0);
        tSort->column(0)->setInvalid(1, true);
        tSort->column(0)->setValueAt(2, 10.0);
        tSort->column(0)->setInvalid(3, true);
        tSort->column(0)->setValueAt(4, 20.0);
        tSort->d_future_table->sortColumns(tSort->column(0), QList<Column *>() << tSort->column(0), true);
        CHECK_COND(std::fabs(tSort->column(0)->valueAt(0) - 10.0) < 1e-9
                           && std::fabs(tSort->column(0)->valueAt(1) - 20.0) < 1e-9
                           && std::fabs(tSort->column(0)->valueAt(2) - 30.0) < 1e-9
                           && tSort->column(0)->isInvalid(3)
                           && tSort->column(0)->isInvalid(4),
                   "77c: P1-D5 ascending sort with NaNs failed strict order or NaN-last placement!");

        // P1-D7: Overwrite import shrinks trailing old rows when new file has fewer rows
        const QString shortCsvPath = tmp77.filePath(QStringLiteral("short_overwrite.csv"));
        {
            QFile cf(shortCsvPath);
            CHECK_COND(cf.open(QIODevice::WriteOnly), "77c: short_overwrite.csv");
            cf.write("A,B\n10,100\n20,200\n");
            cf.close();
        }
        Table *tOvr = w77->newTable(QStringLiteral("Ovr77"), 6, 2);
        for (int r = 0; r < 6; ++r) {
            tOvr->setCell(r, 0, double(r + 1));
            tOvr->setCell(r, 1, double((r + 1) * 10));
        }
        w77->activateSubWindow(tOvr);
        w77->importASCII(QStringList() << shortCsvPath, ImportASCIIDialog::Overwrite,
                         QStringLiteral(","), 0, true, true, false, true, QLocale::c());
        CHECK_COND(tOvr->numRows() == 2
                           && std::fabs(tOvr->cell(0, 1) - 100.0) < 1e-9
                           && std::fabs(tOvr->cell(1, 1) - 200.0) < 1e-9,
                   "77c: P1-D7 Overwrite import left trailing old rows behind!");
    }

    // -------------------------------------------------------------------------
    // 77d: P0-6 (Dotted alloy filename actual PNG export & non-truncating probe)
    // -------------------------------------------------------------------------
    {
        const QString baseAlloy = tmp77.filePath(QStringLiteral("Al-5.5Zn-2.5Mg"));
        {
            QFile existingNoExt(baseAlloy);
            CHECK_COND(existingNoExt.open(QIODevice::WriteOnly), "77d: seed existing file");
            existingNoExt.write("PRECIOUS_EXISTING_DATA");
            existingNoExt.close();
        }
        bool appended = false;
        const QString resolvedPng = ApplicationWindow::resolveImageExportFileName(
                baseAlloy, QStringLiteral("PNG (*.png)"), &appended);
        CHECK_COND(appended && resolvedPng.endsWith(QStringLiteral("Al-5.5Zn-2.5Mg.png")),
                   "77d: P0-6 dotted alloy filename did not append .png suffix!");
        CHECK_COND(ApplicationWindow::probeExportWritableNoTruncate(baseAlloy),
                   "77d: probeExportWritableNoTruncate returned false on writable file");
        QFile checkExisting(baseAlloy);
        CHECK_COND(checkExisting.open(QIODevice::ReadOnly)
                           && checkExisting.readAll() == QByteArray("PRECIOUS_EXISTING_DATA"),
                   "77d: P0-6 writability probe truncated existing file!");
        checkExisting.close();

        Table *tAlloy = w77->newTable(QStringLiteral("Alloy77"), 5, 2);
        for (int r = 0; r < 5; ++r) {
            tAlloy->setCell(r, 0, double(r));
            tAlloy->setCell(r, 1, double(100 + 25 * r));
        }
        MultiLayer *mlAlloy = w77->multilayerPlot(
                tAlloy, QStringList() << (tAlloy->name() + QStringLiteral("_2")), Graph::LineSymbols);
        CHECK_COND(mlAlloy != nullptr, "77d: mlAlloy null");
        mlAlloy->exportImage(resolvedPng, 100, false, 150);
        const QImage exportedAlloyImg(resolvedPng);
        CHECK_COND(!exportedAlloyImg.isNull() && exportedAlloyImg.width() > 50
                           && exportedAlloyImg.height() > 50,
                   "77d: ml->exportImage failed to write valid PNG for dotted filename Al-5.5Zn-2.5Mg.png!");

        // Track 2 M1: safeExportWriteTransaction overwrites existing Al-5.5Zn-2.5Mg.png via short
        // '.tgexp_<8hex>.png' temp file, and preserves original file intact on callback/lock failure
        bool sawShortTmpName = false;
        const bool ovrOk = ApplicationWindow::safeExportWriteTransaction(
                resolvedPng, QStringLiteral("png"), [&](const QString &writePath) {
                    const QString fn = QFileInfo(writePath).fileName();
                    sawShortTmpName = fn.startsWith(QStringLiteral(".tgexp_"))
                            && fn.endsWith(QStringLiteral(".png")) && fn.size() <= 20;
                    mlAlloy->exportImage(writePath, 100, false, 210);
                    return true;
                });
        const QImage ovrAlloyImg(resolvedPng);
        CHECK_COND(ovrOk && sawShortTmpName && !ovrAlloyImg.isNull()
                           && ovrAlloyImg.width() > exportedAlloyImg.width(),
                   "77d: M1 safe overwrite of existing Al-5.5Zn-2.5Mg.png failed!");
        QByteArray savedGoodPngBytes;
        {
            QFile gf(resolvedPng);
            CHECK_COND(gf.open(QIODevice::ReadOnly), "77d: read overwritten PNG");
            savedGoodPngBytes = gf.readAll();
            gf.close();
            CHECK_COND(!savedGoodPngBytes.isEmpty(), "77d: empty overwritten PNG");
        }

        // Callback returning false or producing 0-byte temp file must leave existing PNG untouched
        const bool failCb1 = ApplicationWindow::safeExportWriteTransaction(
                resolvedPng, QStringLiteral("png"),
                [&](const QString &) { return false; });
        const bool failCb2 = ApplicationWindow::safeExportWriteTransaction(
                resolvedPng, QStringLiteral("png"), [&](const QString &writePath) {
                    QFile zf(writePath);
                    if (zf.open(QIODevice::WriteOnly))
                        zf.close();
                    return true;
                });
        {
            QFile gf(resolvedPng);
            CHECK_COND(!failCb1 && !failCb2 && gf.open(QIODevice::ReadOnly)
                               && gf.readAll() == savedGoodPngBytes,
                       "77d: M1 failed export callback clobbered existing Al-5.5Zn-2.5Mg.png!");
        }
#ifdef Q_OS_WIN
        // Exclusive/read-locked destination file (no FILE_SHARE_DELETE) must NOT delete original or temp data
        {
            const std::wstring wLocked = QDir::toNativeSeparators(resolvedPng).toStdWString();
            HANDLE hLock = CreateFileW(wLocked.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            CHECK_COND(hLock != INVALID_HANDLE_VALUE, "77d: failed to open read lock on PNG");
            const bool failLocked = ApplicationWindow::safeExportWriteTransaction(
                    resolvedPng, QStringLiteral("png"), [&](const QString &writePath) {
                        QFile tf(writePath);
                        if (!tf.open(QIODevice::WriteOnly))
                            return false;
                        tf.write("NEW_PNG_BYTES_WHILE_LOCKED");
                        tf.close();
                        return true;
                    });
            CloseHandle(hLock);
            QFile gf(resolvedPng);
            CHECK_COND(!failLocked && gf.open(QIODevice::ReadOnly)
                               && gf.readAll() == savedGoodPngBytes,
                       "77d: M1 export onto locked Al-5.5Zn-2.5Mg.png corrupted or deleted original file!");
        }
#endif
        CHECK_COND(QDir(tmp77.path())
                           .entryList(QStringList() << QStringLiteral(".tgexp_*")
                                                    << QStringLiteral("*.tg_bak*"),
                                      QDir::Files | QDir::Hidden)
                           .isEmpty(),
                   "77d: M1 left temporary .tgexp_* or .tg_bak* files behind!");

        // RPC graph.export stale-file guard: pre-existing stale destination must NOT yield false ok:true
        if (PythonAutomationServer *pySrv = w77->pythonAutomationServer()) {
            const QString staleRpcPng = tmp77.filePath(QStringLiteral("stale_rpc77.png"));
            {
                QFile sf(staleRpcPng);
                CHECK_COND(sf.open(QIODevice::WriteOnly), "77d: seed stale_rpc77.png");
                sf.write("STALE_PREEXISTING_PNG_BYTES");
                sf.close();
            }
            QJsonObject badParams;
            badParams[QStringLiteral("graph")] = mlAlloy->name();
            badParams[QStringLiteral("path")] = staleRpcPng;
            badParams[QStringLiteral("format")] = QStringLiteral("unsupported_bogus_fmt");
            QJsonObject badReq;
            badReq[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            badReq[QStringLiteral("id")] = 7701;
            badReq[QStringLiteral("method")] = QStringLiteral("graph.export");
            badReq[QStringLiteral("params")] = badParams;
            const QJsonObject badResp = pySrv->handleRpcRequest(badReq);
            QFile chkStale(staleRpcPng);
            CHECK_COND(badResp.contains(QStringLiteral("error"))
                               && chkStale.open(QIODevice::ReadOnly)
                               && chkStale.readAll() == QByteArray("STALE_PREEXISTING_PNG_BYTES"),
                       "77d: RPC graph.export returned false ok:true on stale file or clobbered it!");
            chkStale.close();

            const QString psOut = tmp77.filePath(QStringLiteral("rpc_vec77.ps"));
            QJsonObject psParams;
            psParams[QStringLiteral("graph")] = mlAlloy->name();
            psParams[QStringLiteral("path")] = psOut;
            QJsonObject psReq;
            psReq[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            psReq[QStringLiteral("id")] = 7702;
            psReq[QStringLiteral("method")] = QStringLiteral("graph.export");
            psReq[QStringLiteral("params")] = psParams;
            const QJsonObject psResp = pySrv->handleRpcRequest(psReq);
            const QJsonObject psRes = psResp.value(QStringLiteral("result")).toObject();
            CHECK_COND(psRes.value(QStringLiteral("ok")).toBool()
                               && QFileInfo(psOut + QStringLiteral(".pdf")).size() > 0,
                       "77d: RPC graph.export .ps -> .ps.pdf fallback via temp file failed!");
            CHECK_COND(QDir(tmp77.path())
                               .entryList(QStringList() << QStringLiteral(".tgrpc_*"),
                                          QDir::Files | QDir::Hidden)
                               .isEmpty(),
                       "77d: RPC graph.export left temporary .tgrpc_* files behind!");
        }
    }

    // -------------------------------------------------------------------------
    // 77e: P0-7 & P1-G1 (Legend placement, window size & stored font invariant under zoom)
    // -------------------------------------------------------------------------
    {
        Table *tZ = w77->newTable(QStringLiteral("ZoomLeg77"), 10, 2);
        for (int r = 0; r < 10; ++r) {
            tZ->setCell(r, 0, double(r));
            tZ->setCell(r, 1, double(r * r));
        }
        MultiLayer *mlZ = w77->multilayerPlot(
                tZ, QStringList() << (tZ->name() + QStringLiteral("_2")), Graph::LineSymbols);
        CHECK_COND(mlZ && mlZ->activeGraph(), "77e: ZoomLeg77 graph");
        CHECK_COND(mlZ->applyStylePresetById(QStringLiteral("lab_slide_83mm")),
                   "77e: apply lab_slide_83mm failed");
        Graph *gZ = mlZ->activeGraph();
        CHECK_COND(std::fabs(mlZ->pageZoom() - 1.6) < 1e-4, "77e: expected 160% view zoom");
        const double ptBefore = gZ->legend()->font().pointSizeF();
        mlZ->resize(mlZ->width() + 42, mlZ->height() + 28);
        mlZ->move(95, 75);
        const QSize customWinSz = mlZ->size();
        const QPoint customWinPos = mlZ->pos();

        gZ->placeLegendAnchor(Graph::LegendOutsideRight);
        CHECK_COND(mlZ->size() == customWinSz && mlZ->pos() == customWinPos,
                   "77e: placeLegendAnchor under zoom reset manually resized/moved MultiLayer geometry!");
        CHECK_COND(!mlZ->testAttribute(Qt::WA_UpdatesDisabled)
                           && (!mlZ->widget() || !mlZ->widget()->testAttribute(Qt::WA_UpdatesDisabled)),
                   "77e: ScopedUnzoomForExport left WA_UpdatesDisabled stuck on MultiLayer or its child widget!");
        const QString exp160Png = tmp77.filePath(QStringLiteral("zoom160_exp.png"));
        const QString exp100Png = tmp77.filePath(QStringLiteral("zoom100_exp.png"));
        mlZ->exportImage(exp160Png, 100, false, 300);
        CHECK_COND(!mlZ->testAttribute(Qt::WA_UpdatesDisabled)
                           && (!mlZ->widget() || !mlZ->widget()->testAttribute(Qt::WA_UpdatesDisabled)),
                   "77e: exportImage under 160% zoom left WA_UpdatesDisabled stuck on MultiLayer!");
        const int marginFrom160 = gZ->plotWidget()->property("_outsideRightMargin").toInt();
        mlZ->setPageZoom(1.0);
        mlZ->exportImage(exp100Png, 100, false, 300);
        CHECK_COND(QImage(exp160Png) == QImage(exp100Png),
                   "77e: 160%-zoomed exportImage differed at pixel level from 100%-zoomed exportImage!");
        gZ->placeLegendAnchor(Graph::LegendOutsideRight);
        const int marginFrom100 = gZ->plotWidget()->property("_outsideRightMargin").toInt();
        CHECK_COND(std::abs(marginFrom160 - marginFrom100) <= 2,
                   "77e: P0-7 LegendOutsideRight _outsideRightMargin inflated by 160% view zoom ("
                           << marginFrom160 << " vs " << marginFrom100 << ")");

        mlZ->setPageZoom(0.75);
        mlZ->move(95, 75);
        const QPoint posAt75 = mlZ->pos();
        gZ->placeLegendAnchor(Graph::LegendTopRight);
        CHECK_COND(mlZ->pos() == posAt75,
                   "77e: placeLegendAnchor under 75% zoom shifted MultiLayer window position!");
        CHECK_COND(std::fabs(gZ->legend()->font().pointSizeF() - ptBefore) < 1e-6,
                   "77e: P0-7 placeLegendAnchor under 75% zoom permanently shrank stored legend pt!");
    }

    // -------------------------------------------------------------------------
    // 77f: P1-S2 (Graph Undo/Redo + Legend remap after table & column rename),
    //      P1-S4 (200-sheet workbook > 4500 chars),
    //      P2 (multi-member .gz, zero-padded .gz, truncated .gz & appendProject dirty)
    // -------------------------------------------------------------------------
    {
        Table *tRen = w77->newTable(QStringLiteral("OrigSheet77"), 6, 3);
        tRen->setColName(0, QStringLiteral("Strain"));
        tRen->setColName(1, QStringLiteral("Stress"));
        tRen->setColName(2, QStringLiteral("Err"));
        tRen->setColPlotDesignation(2, SciDAVis::yErr);
        for (int r = 0; r < 6; ++r) {
            tRen->setCell(r, 0, 0.1 * double(r));
            tRen->setCell(r, 1, 100.0 * double(r));
            tRen->setCell(r, 2, 5.0);
        }
        MultiLayer *mlRen = w77->multilayerPlot(
                tRen, QStringList() << (tRen->name() + QStringLiteral("_Stress")), Graph::LineSymbols);
        CHECK_COND(mlRen && mlRen->activeGraph(), "77f: mlRen null");
        Graph *gRen = mlRen->activeGraph();
        gRen->addErrorBars(tRen->name() + QStringLiteral("_Stress"), tRen,
                           tRen->name() + QStringLiteral("_Err"));
        CHECK_COND(gRen->curves() == 2, "77f: expected data curve + error bar");

        if (gRen->legend()) {
            gRen->legend()->setText(gRen->legend()->text()
                                    + QStringLiteral("\n\\l(1) OrigSheet77_Stress (fit)"
                                                     "\nModel vs OrigSheet77_Other"
                                                     "\nModel vs Stress"));
        }
        const QString snapBefore = mlRen->saveToString(QString());
        gRen->setAxisTitle(QwtPlot::xBottom, QStringLiteral("True Strain (-)"));
        mlRen->pushUndoSnapshot(QStringLiteral("Edit X Title"), snapBefore);
        CHECK_COND(mlRen->undoStack() && mlRen->undoStack()->canUndo(), "77f: undoStack not populated");

        CHECK_COND(w77->renameWindow(tRen, QStringLiteral("RenamedSheet77")),
                   "77f: renameWindow failed");
        tRen->setColName(1, QStringLiteral("FlowStress"));
        QApplication::processEvents();

        mlRen->undoStack()->undo();
        CHECK_COND(mlRen->activeGraph() && mlRen->activeGraph()->curves() == 2,
                   "77f: P1-S2 Graph Undo after sheet+column rename deleted the curve!");
        const QString legAfterUndo =
                mlRen->activeGraph()->legend() ? mlRen->activeGraph()->legend()->text() : QString();
        CHECK_COND(legAfterUndo.contains(QStringLiteral("\\l(1) RenamedSheet77_FlowStress (fit)"))
                           && legAfterUndo.contains(QStringLiteral("Model vs OrigSheet77_Other"))
                           && legAfterUndo.contains(QStringLiteral("Model vs Stress")),
                   "77f: Graph Undo legend remap mismatch: got '"
                           << legAfterUndo.toStdString() << "'");
        mlRen->undoStack()->redo();
        CHECK_COND(mlRen->activeGraph() && mlRen->activeGraph()->curves() == 2,
                   "77f: P1-S2 Graph Redo after sheet+column rename deleted the curve!");

        // P1-S4: 200-sheet workbook (> 4500 chars on <workbook> line, strictly > 4096) survives save & loadProject
        const QString bigPrj = tmp77.filePath(QStringLiteral("big200.sciprj"));
        QString wbErr;
        QString bigBook;
        {
            AppWinGuard wBigSrc(new ApplicationWindow());
            wBigSrc->applyUserSettings();
            WorkbookManager *wm = WorkbookManager::instance(wBigSrc);
            bigBook = wm->createBook(QStringLiteral("BigBook77"), 200, &wbErr);
            CHECK_COND(!bigBook.isEmpty() && wm->sheets(bigBook).size() == 200,
                       "77f: createBook(200) failed: " << wbErr.toStdString());
            CHECK_COND(wBigSrc->writeProjectFile(wBigSrc->projectFolder(), bigPrj),
                       "77f: save big200.sciprj");
        }
        {
            QFile chkWb(bigPrj);
            CHECK_COND(chkWb.open(QIODevice::ReadOnly), "77f: read big200.sciprj");
            int maxWorkbookLineLen = 0;
            for (const QByteArray &ln : chkWb.readAll().split('\n')) {
                if (ln.startsWith("<workbook>\t"))
                    maxWorkbookLineLen = std::max(maxWorkbookLineLen, ln.size());
            }
            chkWb.close();
            std::cout << "[INFO] 77f 200-sheet <workbook> line length = " << maxWorkbookLineLen
                      << " bytes\n";
            CHECK_COND(maxWorkbookLineLen > 4500,
                       "77f: expected <workbook> line > 4500 bytes, got " << maxWorkbookLineLen);
        }

        AppWinGuard reBig(w77->openProject(bigPrj));
        CHECK_COND(reBig != nullptr, "77f: reopen big200.sciprj failed");
        WorkbookManager *wmReBig = WorkbookManager::instance(reBig);
        CHECK_COND(wmReBig && wmReBig->sheets(bigBook).size() == 200,
                   "77f: P1-S4 200-sheet workbook truncated by 4096-char readLine limit (got "
                           << (wmReBig ? wmReBig->sheets(bigBook).size() : -1) << ")");
        Table *sheet151 = wmReBig->sheets(bigBook).at(150);
        CHECK_COND(sheet151 && reBig->hidden(sheet151),
                   "77f: sheet 151 expected initially hidden after reopening big200.sciprj");
        CHECK_COND(wmReBig->setCurrentSheet(sheet151, true, &wbErr)
                           && wmReBig->currentSheet(bigBook) == sheet151
                           && !reBig->hidden(sheet151) && !sheet151->isHidden(),
                   "77f: showing hidden sheet 151 after 200-sheet load failed: "
                           << wbErr.toStdString());

        // P1-S5: Editing Sheet 1, switching to Sheet 151, and pressing Ctrl+Z (undo()) automatically
        // reveals Sheet 1 so a hidden sheet is never modified invisibly in the background!
        Table *sheet1 = wmReBig->sheets(bigBook).at(0);
        CHECK_COND(wmReBig->setCurrentSheet(sheet1, true, &wbErr), "77f: switch to sheet 1");
        sheet1->setCell(0, 0, 777.5);
        CHECK_COND(wmReBig->setCurrentSheet(sheet151, true, &wbErr)
                           && wmReBig->currentSheet(bigBook) == sheet151,
                   "77f: switch back to sheet 151");
        reBig->undo();
        CHECK_COND(wmReBig->currentSheet(bigBook) == sheet1 && !reBig->hidden(sheet1)
                           && std::fabs(sheet1->cell(0, 0) - 777.5) > 1e-9,
                   "77f: P1-S5 undo() from another sheet did not reveal the modified sheet 1!");
        CHECK_COND(wmReBig->setCurrentSheet(sheet151, true, &wbErr), "77f: switch to sheet 151 before redo");
        reBig->redo();
        CHECK_COND(wmReBig->currentSheet(bigBook) == sheet1
                           && std::fabs(sheet1->cell(0, 0) - 777.5) < 1e-9,
                   "77f: P1-S5 redo() from another sheet did not reveal the modified sheet 1!");

        // P2: appendProject marks destination project modified (!saved)
        reBig->savedProject();
        CHECK_COND(reBig->saved, "77f: expected clean state before appendProject");
        const QString smallPrj = tmp77.filePath(QStringLiteral("small_append.sciprj"));
        const QString gzPath = tmp77.filePath(QStringLiteral("trunc77.sciprj.gz"));
        {
            AppWinGuard wSmall(new ApplicationWindow());
            wSmall->applyUserSettings();
            Note *padNote = wSmall->newNote(QStringLiteral("PadNote77"));
            CHECK_COND(padNote != nullptr, "77f: create PadNote77");
            padNote->setText(QString(16600, QLatin1Char('P')));
            Table *ts = wSmall->newTable(QStringLiteral("SmallT77"), 2, 2);
            ts->setCell(0, 0, 42.0);
            ts->setCell(1, 1, 84.0);
            CHECK_COND(wSmall->writeProjectFile(wSmall->projectFolder(), smallPrj),
                       "77f: write small_append.sciprj");
            CHECK_COND(wSmall->writeProjectFile(wSmall->projectFolder(), gzPath),
                       "77f: write trunc77.sciprj.gz from lightweight smallPrj");
        }
        reBig->appendProject(smallPrj);
        CHECK_COND(!reBig->saved,
                   "77f: P2 appendProject left project marked saved (no close prompt)!");
        reBig.reset();

        // Helper to build an RFC-1952 valid gzip member using stored DEFLATE blocks
        auto makeStoredGzipMember = [](const QByteArray &raw) -> QByteArray {
            quint32 crc = 0xFFFFFFFFu;
            for (unsigned char b : raw) {
                crc ^= b;
                for (int bit = 0; bit < 8; ++bit)
                    crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
            }
            crc ^= 0xFFFFFFFFu;

            QByteArray out;
            out.reserve(raw.size() + 32);
            const char hdr[10] = { '\x1f', '\x8b', '\x08', '\x00', '\x00',
                                   '\x00', '\x00', '\x00', '\x00', '\x03' };
            out.append(hdr, 10);

            int offset = 0;
            do {
                const int blkLen = std::min(32768, raw.size() - offset);
                const bool isFinal = (offset + blkLen >= raw.size());
                out.append(char(isFinal ? 0x01 : 0x00));
                const quint16 len16 = quint16(blkLen);
                const quint16 nlen16 = quint16(~len16);
                out.append(char(len16 & 0xFF));
                out.append(char((len16 >> 8) & 0xFF));
                out.append(char(nlen16 & 0xFF));
                out.append(char((nlen16 >> 8) & 0xFF));
                if (blkLen > 0)
                    out.append(raw.constData() + offset, blkLen);
                offset += blkLen;
            } while (offset < raw.size());

            const quint32 isize = quint32(raw.size());
            for (int s = 0; s < 32; s += 8)
                out.append(char((crc >> s) & 0xFF));
            for (int s = 0; s < 32; s += 8)
                out.append(char((isize >> s) & 0xFF));
            return out;
        };

        // Multi-member .sciprj.gz (including exact 16 KB chunk boundary: gzMember1 == 16384 & 16383)
        QByteArray rawSmallBytes;
        {
            QFile sf(smallPrj);
            CHECK_COND(sf.open(QIODevice::ReadOnly), "77f: read small_append.sciprj");
            rawSmallBytes = sf.readAll();
            sf.close();
            CHECK_COND(rawSmallBytes.size() > 16800,
                       "77f: small_append.sciprj expected > 16800 bytes for 16KB boundary test");
        }
        const int storedGzOverhead = makeStoredGzipMember(QByteArray()).size();
        for (int targetGz1Size : { 16384, 16383, int(rawSmallBytes.size() / 2) }) {
            const int raw1Len = (targetGz1Size >= 16383)
                    ? (targetGz1Size - storedGzOverhead)
                    : targetGz1Size;
            const QByteArray gzMember1 = makeStoredGzipMember(rawSmallBytes.left(raw1Len));
            const QByteArray gzMember2 = makeStoredGzipMember(rawSmallBytes.mid(raw1Len));
            if (targetGz1Size >= 16383) {
                CHECK_COND(gzMember1.size() == targetGz1Size,
                           "77f: expected gzMember1.size() == " << targetGz1Size
                                                                << ", got " << gzMember1.size());
            }
            const QString multiGzPath = tmp77.filePath(
                    QStringLiteral("multi77_%1.sciprj.gz").arg(targetGz1Size));
            {
                QFile mf(multiGzPath);
                CHECK_COND(mf.open(QIODevice::WriteOnly), "77f: write multi77.sciprj.gz");
                mf.write(gzMember1);
                mf.write(gzMember2);
                mf.close();
            }
            AppWinGuard reMultiGz(w77->openProject(multiGzPath));
            CHECK_COND(reMultiGz != nullptr && reMultiGz->table(QStringLiteral("SmallT77")) != nullptr
                               && std::fabs(reMultiGz->table(QStringLiteral("SmallT77"))->cell(1, 1)
                                            - 84.0)
                                       < 1e-9,
                       "77f: multi-member .sciprj.gz failed at gzMember1 size " << gzMember1.size());
        }

        // Multi-member .sciprj.gz with truncated 2nd member must be rejected
        {
            const int halfIdx = rawSmallBytes.size() / 2;
            const QByteArray gzMember1 = makeStoredGzipMember(rawSmallBytes.left(halfIdx));
            const QByteArray gzMember2 = makeStoredGzipMember(rawSmallBytes.mid(halfIdx));
            const QString truncMultiGzPath =
                    tmp77.filePath(QStringLiteral("trunc_multi77.sciprj.gz"));
            QFile tmf(truncMultiGzPath);
            CHECK_COND(tmf.open(QIODevice::WriteOnly), "77f: write trunc_multi77.sciprj.gz");
            tmf.write(gzMember1);
            tmf.write(gzMember2.left(gzMember2.size() / 2));
            tmf.close();
            AppWinGuard reTruncMulti(w77->openProject(truncMultiGzPath));
            CHECK_COND(reTruncMulti == nullptr,
                       "77f: multi-member .sciprj.gz with truncated 2nd member was not rejected!");
        }

        // Zero-padded .sciprj.gz (trailing NUL block padding) must open cleanly, while truncated .gz is rejected
        QByteArray fullGz;
        {
            QFile gf(gzPath);
            CHECK_COND(gf.open(QIODevice::ReadOnly), "77f: read gz");
            fullGz = gf.readAll();
            gf.close();
            CHECK_COND(fullGz.size() > 40, "77f: gz too small");
        }
        const QString paddedGzPath = tmp77.filePath(QStringLiteral("padded77.sciprj.gz"));
        {
            QFile pf(paddedGzPath);
            CHECK_COND(pf.open(QIODevice::WriteOnly), "77f: write padded77.sciprj.gz");
            pf.write(fullGz);
            pf.write(QByteArray(512, '\0'));
            pf.close();
        }
        AppWinGuard rePadGz(w77->openProject(paddedGzPath));
        CHECK_COND(rePadGz != nullptr
                           && rePadGz->table(QStringLiteral("SmallT77")) != nullptr,
                   "77f: trailing zero-padded .sciprj.gz was falsely rejected!");
        rePadGz.reset();

        {
            QFile gf(gzPath);
            CHECK_COND(gf.open(QIODevice::WriteOnly | QIODevice::Truncate), "77f: truncate gz");
            gf.write(fullGz.left(fullGz.size() / 2));
            gf.close();
        }
        AppWinGuard reTrunc(w77->openProject(gzPath));
        CHECK_COND(reTrunc == nullptr,
                   "77f: P2 truncated .sciprj.gz was silently partially loaded without Z_STREAM_END!");
    }

    // -------------------------------------------------------------------------
    // 77g: Wave R7 Regressions:
    //      (1) Axis Title (xBottom & yLeft) arrow-key nudging at 100% & 160% zoom
    //          (0 px canvasRect/geometry/MultiLayer::size change, PNG pixel diff,
    //           160%-vs-100% PNG equality, Undo/Redo & .sciprj round-trip)
    //      (2) Plot Title arrow-key nudging via titleLabel() inside MultiLayerCanvasScroll
    //          at 160% zoom (0 px layout change, Undo/Redo & .sciprj round-trip)
    //      (3) Outside-Right Legend (_outsideRightMargin > 0) + TextAnnotation &
    //          LegendBox arrow-key nudging without wiping _outsideRightMargin
    //      (4) Stale MultiLayer::d_layers_selector clearance on Graph selection
    //      (5) No misleading "状態図" / "phase diagram" wording on Ternary Plot
    // -------------------------------------------------------------------------
    {
        // (5) Verify Ternary Plot UI action & RPC catalog contain no "状態図" / "phase diagram"
        QAction *actTernary = w77->findChild<QAction *>(QStringLiteral("actionPlotTernary"));
        CHECK_COND(actTernary != nullptr, "77g: actionPlotTernary not found");
        const QString prevLang77g = w77->appLanguage;
        for (const QString &langCode : { QStringLiteral("en"), QStringLiteral("ja") }) {
            w77->switchToLanguage(langCode);
            CHECK_COND(!actTernary->text().contains(QString::fromUtf8("状態図"))
                               && !actTernary->text().contains(QStringLiteral("phase diagram"),
                                                               Qt::CaseInsensitive),
                       "77g: actionPlotTernary text contained phase-diagram wording in '"
                               << langCode.toStdString()
                               << "': " << actTernary->text().toStdString());
            CHECK_COND(!actTernary->toolTip().contains(QString::fromUtf8("状態図"))
                               && !actTernary->toolTip().contains(QStringLiteral("phase diagram"),
                                                                  Qt::CaseInsensitive),
                       "77g: actionPlotTernary toolTip contained phase-diagram wording in '"
                               << langCode.toStdString()
                               << "': " << actTernary->toolTip().toStdString());
            if (langCode == QStringLiteral("ja")) {
                CHECK_COND(actTernary->text().contains(QString::fromUtf8("三角図")),
                           "77g: JA actionPlotTernary text missing '三角図': "
                                   << actTernary->text().toStdString());
            }
        }
        w77->switchToLanguage(prevLang77g.isEmpty() ? QStringLiteral("en") : prevLang77g);

        if (PythonAutomationServer *pySrv = w77->pythonAutomationServer()) {
            QJsonObject descParams;
            descParams[QStringLiteral("method")] = QStringLiteral("plot.ternary");
            QJsonObject descReq;
            descReq[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            descReq[QStringLiteral("id")] = 7707;
            descReq[QStringLiteral("method")] = QStringLiteral("app.describe_api");
            descReq[QStringLiteral("params")] = descParams;
            const QJsonObject descResp = pySrv->handleRpcRequest(descReq);
            const QString descJson = QString::fromUtf8(
                    QJsonDocument(descResp).toJson(QJsonDocument::Compact));
            CHECK_COND(!descResp.contains(QStringLiteral("error"))
                               && descJson.contains(QStringLiteral("plot.ternary"))
                               && !descJson.contains(QStringLiteral("phase diagram"),
                                                     Qt::CaseInsensitive)
                               && !descJson.contains(QString::fromUtf8("状態図")),
                       "77g: RPC app.describe_api(plot.ternary) still mentions phase diagram: "
                               << descJson.toStdString());
        }

        // Dedicated visible window so focus, MultiLayerCanvasScroll, and MultiLayer margin click
        // (cand->isVisible()) behave identically to interactive GUI usage
        AppWinGuard w77g(new ApplicationWindow());
        w77g->applyUserSettings();
        Table *tNudge = w77g->newTable(QStringLiteral("NudgeTbl77g"), 8, 2);
        tNudge->setColName(0, QStringLiteral("Strain"));
        tNudge->setColName(1, QStringLiteral("Stress"));
        for (int r = 0; r < 8; ++r) {
            tNudge->setCell(r, 0, 0.5 * double(r));
            tNudge->setCell(r, 1, 120.0 + 35.0 * double(r));
        }
        MultiLayer *ml = w77g->multilayerPlot(
                tNudge, QStringList() << (tNudge->name() + QStringLiteral("_Stress")),
                Graph::LineSymbols);
        CHECK_COND(ml && ml->activeGraph(), "77g: multilayerPlot failed");
        w77g->show();
        ml->show();
        for (int k = 0; k < 5; ++k)
            QApplication::processEvents();

        CHECK_COND(ml->applyStylePresetById(QStringLiteral("lab_slide_83mm")),
                   "77g: applyStylePresetById(lab_slide_83mm) failed");
        Graph *g = ml->activeGraph();
        Plot *plt = g ? g->plotWidget() : nullptr;
        CHECK_COND(g && plt && plt->canvas(), "77g: activeGraph / plotWidget null");
        g->setXAxisTitle(QStringLiteral("Strain / %"));
        g->setYAxisTitle(QStringLiteral("Stress / MPa"));
        g->setTitle(QStringLiteral("Al-Zn-Mg Tensile Curve"));
        plt->updateLayout();
        plt->replot();
        QApplication::processEvents();

        auto sendKey = [](QObject *receiver, Qt::Key key,
                          Qt::KeyboardModifiers mods = Qt::NoModifier) {
            QKeyEvent kePress(QEvent::KeyPress, key, mods);
            QApplication::sendEvent(receiver, &kePress);
            QKeyEvent keRel(QEvent::KeyRelease, key, mods);
            QApplication::sendEvent(receiver, &keRel);
        };

        // ---------------------------------------------------------------------
        // (1a) Axis Title (xBottom & yLeft) + Axis scale nudging at 100% zoom
        // ---------------------------------------------------------------------
        ml->setPageZoom(1.0);
        plt->updateLayout();
        plt->replot();
        QApplication::processEvents();
        CHECK_COND(std::fabs(ml->pageZoom() - 1.0) < 1e-4, "77g: expected 100% pageZoom");

        const QSize origMlSize100 = ml->size();
        const QRect origGraphGeo100 = g->geometry();
        const QRect origCanvasRect100 = plt->plotLayout()->canvasRect();
        auto assertZeroLayoutDelta100 = [&](const char *stepTag) -> bool {
            return ml->size() == origMlSize100 && g->geometry() == origGraphGeo100
                    && plt->plotLayout()->canvasRect() == origCanvasRect100 && stepTag != nullptr;
        };

        QwtScaleWidget *swX = plt->axisWidget(QwtPlot::xBottom);
        QwtScaleWidget *swY = plt->axisWidget(QwtPlot::yLeft);
        CHECK_COND(swX && swY, "77g: xBottom / yLeft axisWidget null");

        g->setCurrentSelection({ Graph::GraphSelection::AxisTitle, QwtPlot::xBottom, -1 });
        swX->setFocus();
        sendKey(swX, Qt::Key_Down, Qt::NoModifier); // +2 Y
        CHECK_COND(assertZeroLayoutDelta100("xBottom Key_Down 100%"),
                   "77g: xBottom AxisTitle Key_Down at 100% mutated canvasRect/geometry/ml->size!");
        sendKey(swX, Qt::Key_Up, Qt::ShiftModifier); // -10 Y -> net -8 Y
        CHECK_COND(assertZeroLayoutDelta100("xBottom Shift+Key_Up 100%"),
                   "77g: xBottom AxisTitle Shift+Key_Up at 100% mutated canvasRect/geometry!");
        plt->canvas()->setFocus();
        sendKey(plt->canvas(), Qt::Key_Right, Qt::NoModifier); // +2 X (also tests canvas() routing)
        CHECK_COND(assertZeroLayoutDelta100("xBottom Key_Right 100%"),
                   "77g: xBottom AxisTitle Key_Right on canvas() mutated canvasRect/geometry!");
        sendKey(swX, Qt::Key_Left, Qt::ControlModifier); // -1 X -> net +1 X
        CHECK_COND(assertZeroLayoutDelta100("xBottom Ctrl+Key_Left 100%"),
                   "77g: xBottom AxisTitle Ctrl+Key_Left at 100% mutated canvasRect/geometry!");
        const QPointF xShift100 = swX->property("_toriAxisTitleShift").toPointF();
        CHECK_COND(std::fabs(xShift100.x() - 1.0) < 1e-3
                           && std::fabs(xShift100.y() - (-8.0)) < 1e-3,
                   "77g: xBottom _toriAxisTitleShift at 100% expected (1, -8), got ("
                           << xShift100.x() << ", " << xShift100.y() << ")");

        g->setCurrentSelection({ Graph::GraphSelection::AxisTitle, QwtPlot::yLeft, -1 });
        swY->setFocus();
        sendKey(swY, Qt::Key_Left, Qt::ShiftModifier); // -10 X
        CHECK_COND(assertZeroLayoutDelta100("yLeft Shift+Key_Left 100%"),
                   "77g: yLeft AxisTitle Shift+Key_Left at 100% mutated canvasRect/geometry!");
        sendKey(swY, Qt::Key_Right, Qt::NoModifier); // +2 X -> net -8 X
        CHECK_COND(assertZeroLayoutDelta100("yLeft Key_Right 100%"),
                   "77g: yLeft AxisTitle Key_Right at 100% mutated canvasRect/geometry!");
        sendKey(swY, Qt::Key_Up, Qt::ShiftModifier); // -10 Y
        CHECK_COND(assertZeroLayoutDelta100("yLeft Shift+Key_Up 100%"),
                   "77g: yLeft AxisTitle Shift+Key_Up at 100% mutated canvasRect/geometry!");
        sendKey(swY, Qt::Key_Down, Qt::ControlModifier); // +1 Y -> net -9 Y
        CHECK_COND(assertZeroLayoutDelta100("yLeft Ctrl+Key_Down 100%"),
                   "77g: yLeft AxisTitle Ctrl+Key_Down at 100% mutated canvasRect/geometry!");
        const QPointF yShift100 = swY->property("_toriAxisTitleShift").toPointF();
        CHECK_COND(std::fabs(yShift100.x() - (-8.0)) < 1e-3
                           && std::fabs(yShift100.y() - (-9.0)) < 1e-3,
                   "77g: yLeft _toriAxisTitleShift at 100% expected (-8, -9), got ("
                           << yShift100.x() << ", " << yShift100.y() << ")");

        // Also verify Axis scale selection nudging keeps canvasRect() 100% unchanged
        g->setCurrentSelection({ Graph::GraphSelection::Axis, QwtPlot::xBottom, -1 });
        swX->setFocus();
        sendKey(swX, Qt::Key_Down, Qt::ShiftModifier);
        sendKey(swX, Qt::Key_Right, Qt::NoModifier);
        CHECK_COND(assertZeroLayoutDelta100("xBottom Axis scale nudge 100%"),
                   "77g: xBottom Axis scale nudge mutated canvasRect/geometry!");

        // Reset shifts cleanly before 160%-zoom + PNG pixel + Undo/Redo + .sciprj test
        swX->setProperty("_toriAxisTitleShift", QPointF(0.0, 0.0));
        swY->setProperty("_toriAxisTitleShift", QPointF(0.0, 0.0));
        swX->setProperty("_toriAxisNormalShift", 0.0);
        swX->setProperty("_toriAxisParallelShift", 0.0);
        plt->setProperty("_toriPlotTitleShift", QPointF(0.0, 0.0));

        // ---------------------------------------------------------------------
        // (1b) Axis Title nudging at 160% zoom + PNG pixel diff + 160%==100% PNG + Undo/Redo
        // ---------------------------------------------------------------------
        ml->setPageZoom(1.6);
        plt->updateLayout();
        plt->replot();
        QApplication::processEvents();
        CHECK_COND(std::fabs(ml->pageZoom() - 1.6) < 1e-4, "77g: expected 160% pageZoom");

        const QString pngBefore = tmp77.filePath(QStringLiteral("nudge77g_before.png"));
        const QString pngAfter160 = tmp77.filePath(QStringLiteral("nudge77g_after160.png"));
        const QString pngAfter100 = tmp77.filePath(QStringLiteral("nudge77g_after100.png"));
        const QString pngUndo = tmp77.filePath(QStringLiteral("nudge77g_undo.png"));
        ml->exportImage(pngBefore, 100, false, 300);
        const QImage imgBefore(pngBefore);
        CHECK_COND(!imgBefore.isNull() && imgBefore.width() > 200, "77g: imgBefore export failed");

        CHECK_COND(ml->undoStack() != nullptr, "77g: ml->undoStack() null");
        ml->undoStack()->clear();

        const QSize origMlSize160 = ml->size();
        const QRect origGraphGeo160 = g->geometry();
        const QRect origCanvasRect160 = plt->plotLayout()->canvasRect();
        auto assertZeroLayoutDelta160 = [&]() -> bool {
            return ml->size() == origMlSize160 && g->geometry() == origGraphGeo160
                    && plt->plotLayout()->canvasRect() == origCanvasRect160;
        };

        // At 160% zoom:
        //   3 * Shift+Right (+30 screen px) + 1 * Right (+2 screen px) = +32 / 1.6 = +20.0 logical px X
        //   2 * Shift+Up   (-20 screen px) + 2 * Up    (-4 screen px) = -24 / 1.6 = -15.0 logical px Y
        g->setCurrentSelection({ Graph::GraphSelection::AxisTitle, QwtPlot::xBottom, -1 });
        swX->setFocus();
        for (int i = 0; i < 3; ++i) {
            sendKey(swX, Qt::Key_Right, Qt::ShiftModifier);
            CHECK_COND(assertZeroLayoutDelta160(),
                       "77g: xBottom Shift+Key_Right at 160% mutated canvasRect/geometry/ml->size!");
        }
        sendKey(swX, Qt::Key_Right, Qt::NoModifier);
        CHECK_COND(assertZeroLayoutDelta160(),
                   "77g: xBottom Key_Right at 160% mutated canvasRect/geometry/ml->size!");
        for (int i = 0; i < 2; ++i) {
            sendKey(swX, Qt::Key_Up, Qt::ShiftModifier);
            CHECK_COND(assertZeroLayoutDelta160(),
                       "77g: xBottom Shift+Key_Up at 160% mutated canvasRect/geometry/ml->size!");
        }
        for (int i = 0; i < 2; ++i) {
            sendKey(swX, Qt::Key_Up, Qt::NoModifier);
            CHECK_COND(assertZeroLayoutDelta160(),
                       "77g: xBottom Key_Up at 160% mutated canvasRect/geometry/ml->size!");
        }

        // Also test yLeft at 160% zoom (+0 net after round-trip or record exact shift, then restore yLeft to 0
        // before checking imgUndo vs imgBefore? No — undoStack->clear() was before xBottom, so let's verify
        // xBottom (+20, -15) PNG & Undo/Redo first, then also verify yLeft at 160%!)
        const QPointF xShift160 = swX->property("_toriAxisTitleShift").toPointF();
        CHECK_COND(std::fabs(xShift160.x() - 20.0) < 1e-3
                           && std::fabs(xShift160.y() - (-15.0)) < 1e-3,
                   "77g: xBottom _toriAxisTitleShift at 160% expected (20, -15), got ("
                           << xShift160.x() << ", " << xShift160.y() << ")");

        ml->exportImage(pngAfter160, 100, false, 300);
        const QImage img160(pngAfter160);
        CHECK_COND(!img160.isNull() && img160.size() == imgBefore.size(),
                   "77g: img160 invalid or size changed");
        CHECK_COND(imgBefore != img160,
                   "77g: exported PNG pixels did not change after nudging X-axis title by (+20, -15)!");

        ml->setPageZoom(1.0);
        ml->exportImage(pngAfter100, 100, false, 300);
        const QImage img100(pngAfter100);
        CHECK_COND(img160 == img100,
                   "77g: 160%-zoom export PNG differed from 100%-zoom export PNG after axis title nudge!");

        // Undo restores _toriAxisTitleShift to (0, 0) and imgUndo == imgBefore
        CHECK_COND(ml->undoStack()->canUndo(),
                   "77g: expected undoStack()->canUndo() after axis title nudge");
        while (ml->undoStack()->canUndo())
            ml->undoStack()->undo();
        QApplication::processEvents();
        g = ml->activeGraph();
        plt = g ? g->plotWidget() : nullptr;
        CHECK_COND(g && plt && plt->axisWidget(QwtPlot::xBottom),
                   "77g: activeGraph null after undo()");
        const QPointF xShiftAfterUndo =
                plt->axisWidget(QwtPlot::xBottom)->property("_toriAxisTitleShift").toPointF();
        CHECK_COND(std::hypot(xShiftAfterUndo.x(), xShiftAfterUndo.y()) < 1e-3,
                   "77g: undo() did not restore _toriAxisTitleShift to (0, 0), got ("
                           << xShiftAfterUndo.x() << ", " << xShiftAfterUndo.y() << ")");
        CHECK_COND(plt->plotLayout()->canvasRect() == origCanvasRect160,
                   "77g: undo() mutated 160%-zoom canvasRect!");
        ml->exportImage(pngUndo, 100, false, 300);
        const QImage imgUndo(pngUndo);
        CHECK_COND(imgUndo == imgBefore,
                   "77g: exported PNG after undo() differed from imgBefore!");

        // Redo restores nudged _toriAxisTitleShift (+20, -15) and exact exported PNG
        CHECK_COND(ml->undoStack()->canRedo(), "77g: expected undoStack()->canRedo()");
        while (ml->undoStack()->canRedo())
            ml->undoStack()->redo();
        QApplication::processEvents();
        g = ml->activeGraph();
        plt = g ? g->plotWidget() : nullptr;
        CHECK_COND(g && plt && plt->axisWidget(QwtPlot::xBottom) && plt->axisWidget(QwtPlot::yLeft),
                   "77g: activeGraph null after redo()");
        const QPointF xShiftAfterRedo =
                plt->axisWidget(QwtPlot::xBottom)->property("_toriAxisTitleShift").toPointF();
        CHECK_COND(std::fabs(xShiftAfterRedo.x() - 20.0) < 1e-3
                           && std::fabs(xShiftAfterRedo.y() - (-15.0)) < 1e-3,
                   "77g: redo() did not restore _toriAxisTitleShift (+20, -15), got ("
                           << xShiftAfterRedo.x() << ", " << xShiftAfterRedo.y() << ")");
        CHECK_COND(plt->plotLayout()->canvasRect() == origCanvasRect160,
                   "77g: redo() mutated 160%-zoom canvasRect!");
        const QString pngRedo = tmp77.filePath(QStringLiteral("nudge77g_redo.png"));
        ml->exportImage(pngRedo, 100, false, 300);
        const QImage imgRedo(pngRedo);
        CHECK_COND(imgRedo == img160,
                   "77g: exported PNG after redo() differed from img160!");

        // Also verify yLeft AxisTitle nudging at 160% zoom with 0 px layout delta
        ml->setPageZoom(1.6);
        plt->updateLayout();
        plt->replot();
        QApplication::processEvents();
        const QSize origMlSizeY160 = ml->size();
        const QRect origGraphGeoY160 = g->geometry();
        const QRect origCanvasRectY160 = plt->plotLayout()->canvasRect();
        swY = plt->axisWidget(QwtPlot::yLeft);
        g->setCurrentSelection({ Graph::GraphSelection::AxisTitle, QwtPlot::yLeft, -1 });
        swY->setFocus();
        sendKey(swY, Qt::Key_Left, Qt::ShiftModifier);   // -10 / 1.6 = -6.25 X
        sendKey(swY, Qt::Key_Right, Qt::ControlModifier); // +1 / 1.6 = +0.625 X -> -5.625 X
        sendKey(swY, Qt::Key_Up, Qt::NoModifier);        // -2 / 1.6 = -1.25 Y
        sendKey(swY, Qt::Key_Down, Qt::ShiftModifier);   // +10 / 1.6 = +6.25 Y -> +5.0 Y
        CHECK_COND(ml->size() == origMlSizeY160 && g->geometry() == origGraphGeoY160
                           && plt->plotLayout()->canvasRect() == origCanvasRectY160,
                   "77g: yLeft AxisTitle arrow nudging at 160% zoom mutated canvasRect/geometry!");
        const QPointF yShift160 = swY->property("_toriAxisTitleShift").toPointF();
        CHECK_COND(std::fabs(yShift160.x() - (-5.625)) < 1e-3
                           && std::fabs(yShift160.y() - 5.0) < 1e-3,
                   "77g: yLeft _toriAxisTitleShift at 160% expected (-5.625, 5.0), got ("
                           << yShift160.x() << ", " << yShift160.y() << ")");

        // ---------------------------------------------------------------------
        // (2) Plot Title arrow-key nudging via titleLabel() inside MultiLayerCanvasScroll at 160% zoom
        // ---------------------------------------------------------------------
        // Push a non-703 separator snapshot so PlotTitle undo does not coalesce with yLeft nudge
        {
            const QString sepSnap = ml->captureUndoSnapshot();
            g->setAxisTitleColor(QwtPlot::xBottom, QColor(12, 24, 36));
            ml->pushUndoSnapshot(QStringLiteral("Color X Axis Title"), sepSnap);
        }
        const int undoIdxBeforePlotTitle = ml->undoStack()->index();
        const QSize origMlSizeTitle = ml->size();
        const QRect origGraphGeoTitle = g->geometry();
        const QRect origCanvasRectTitle = plt->plotLayout()->canvasRect();
        QScrollArea *scrollHost = ml->findChild<QScrollArea *>();
        const int hScrollBefore = (scrollHost && scrollHost->horizontalScrollBar())
                ? scrollHost->horizontalScrollBar()->value()
                : 0;
        const int vScrollBefore = (scrollHost && scrollHost->verticalScrollBar())
                ? scrollHost->verticalScrollBar()->value()
                : 0;

        QwtTextLabel *tl = plt->titleLabel();
        CHECK_COND(tl != nullptr, "77g: plt->titleLabel() null");
        g->setCurrentSelection({ Graph::GraphSelection::PlotTitle, 0, -1 });
        tl->setFocus();
        sendKey(tl, Qt::Key_Right, Qt::ShiftModifier); // +10 / 1.6 = +6.25 X
        CHECK_COND(ml->size() == origMlSizeTitle && g->geometry() == origGraphGeoTitle
                           && plt->plotLayout()->canvasRect() == origCanvasRectTitle,
                   "77g: PlotTitle Shift+Key_Right via titleLabel() mutated canvasRect/geometry!");
        sendKey(tl, Qt::Key_Down, Qt::NoModifier); // +2 / 1.6 = +1.25 Y
        CHECK_COND(ml->size() == origMlSizeTitle && g->geometry() == origGraphGeoTitle
                           && plt->plotLayout()->canvasRect() == origCanvasRectTitle,
                   "77g: PlotTitle Key_Down via titleLabel() mutated canvasRect/geometry!");
        if (scrollHost && scrollHost->horizontalScrollBar() && scrollHost->verticalScrollBar()) {
            CHECK_COND(scrollHost->horizontalScrollBar()->value() == hScrollBefore
                               && scrollHost->verticalScrollBar()->value() == vScrollBefore,
                       "77g: MultiLayerCanvasScroll QScrollArea swallowed arrow keys on titleLabel()!");
        }

        const QPointF ptShift160 = plt->property("_toriPlotTitleShift").toPointF();
        CHECK_COND(std::fabs(ptShift160.x() - 6.25) < 1e-3
                           && std::fabs(ptShift160.y() - 1.25) < 1e-3,
                   "77g: _toriPlotTitleShift at 160% expected (6.25, 1.25), got ("
                           << ptShift160.x() << ", " << ptShift160.y() << ")");

        // Undo & Redo PlotTitle shift
        CHECK_COND(ml->undoStack()->index() > undoIdxBeforePlotTitle,
                   "77g: PlotTitle nudge did not push onto undoStack");
        while (ml->undoStack()->index() > undoIdxBeforePlotTitle)
            ml->undoStack()->undo();
        QApplication::processEvents();
        g = ml->activeGraph();
        plt = g ? g->plotWidget() : nullptr;
        CHECK_COND(g && plt, "77g: activeGraph null after PlotTitle undo()");
        const QPointF ptShiftUndo = plt->property("_toriPlotTitleShift").toPointF();
        CHECK_COND(std::hypot(ptShiftUndo.x(), ptShiftUndo.y()) < 1e-3,
                   "77g: PlotTitle undo() did not restore _toriPlotTitleShift to (0, 0)!");
        while (ml->undoStack()->canRedo())
            ml->undoStack()->redo();
        QApplication::processEvents();
        g = ml->activeGraph();
        plt = g ? g->plotWidget() : nullptr;
        CHECK_COND(g && plt, "77g: activeGraph null after PlotTitle redo()");
        const QPointF ptShiftRedo = plt->property("_toriPlotTitleShift").toPointF();
        CHECK_COND(std::fabs(ptShiftRedo.x() - 6.25) < 1e-3
                           && std::fabs(ptShiftRedo.y() - 1.25) < 1e-3,
                   "77g: PlotTitle redo() did not restore _toriPlotTitleShift (6.25, 1.25)!");

        // .sciprj save + w77g->open(...) reload preserves _toriAxisTitleShift & _toriPlotTitleShift
        const QString mlName77g = ml->name();
        const QString sciprj77g = tmp77.filePath(QStringLiteral("label_shifts_77g.sciprj"));
        CHECK_COND(w77g->writeProjectFile(w77g->projectFolder(), sciprj77g),
                   "77g: writeProjectFile(label_shifts_77g.sciprj) failed");
        {
            AppWinGuard re77g(w77g->open(sciprj77g));
            CHECK_COND(re77g != nullptr, "77g: open(label_shifts_77g.sciprj) returned null");
            MultiLayer *reMl = nullptr;
            for (MyWidget *mwItem : re77g->windowsList()) {
                if (MultiLayer *cand = qobject_cast<MultiLayer *>(mwItem)) {
                    if (cand->name() == mlName77g) {
                        reMl = cand;
                        break;
                    }
                }
            }
            CHECK_COND(reMl && reMl->activeGraph() && reMl->activeGraph()->plotWidget(),
                       "77g: reopened MultiLayer missing after .sciprj reload");
            Plot *rePlt = reMl->activeGraph()->plotWidget();
            const QPointF reXShift =
                    rePlt->axisWidget(QwtPlot::xBottom)->property("_toriAxisTitleShift").toPointF();
            const QPointF reYShift =
                    rePlt->axisWidget(QwtPlot::yLeft)->property("_toriAxisTitleShift").toPointF();
            const QPointF reTitleShift = rePlt->property("_toriPlotTitleShift").toPointF();
            CHECK_COND(std::fabs(reXShift.x() - 20.0) < 1e-2
                               && std::fabs(reXShift.y() - (-15.0)) < 1e-2,
                       "77g: .sciprj reload lost xBottom _toriAxisTitleShift: got ("
                               << reXShift.x() << ", " << reXShift.y() << ")");
            CHECK_COND(std::fabs(reYShift.x() - (-5.625)) < 1e-2
                               && std::fabs(reYShift.y() - 5.0) < 1e-2,
                       "77g: .sciprj reload lost yLeft _toriAxisTitleShift: got ("
                               << reYShift.x() << ", " << reYShift.y() << ")");
            CHECK_COND(std::fabs(reTitleShift.x() - 6.25) < 1e-2
                               && std::fabs(reTitleShift.y() - 1.25) < 1e-2,
                       "77g: .sciprj reload lost _toriPlotTitleShift: got ("
                               << reTitleShift.x() << ", " << reTitleShift.y() << ")");
        }

        // ---------------------------------------------------------------------
        // (3) Outside-Right Legend (_outsideRightMargin > 0) + TextAnnotation & LegendBox arrow nudging
        // ---------------------------------------------------------------------
        ml->setPageZoom(1.0);
        g->placeLegendAnchor(Graph::LegendOutsideRight);
        plt->updateLayout();
        plt->replot();
        QApplication::processEvents();

        const int origRightMargin = plt->property("_outsideRightMargin").toInt();
        CHECK_COND(origRightMargin > 0,
                   "77g: expected _outsideRightMargin > 0 after placeLegendAnchor(LegendOutsideRight)");
        const QRect origCanvasRectOut = plt->plotLayout()->canvasRect();
        const QRect origGraphGeoOut = g->geometry();
        const QSize origMlSizeOut = ml->size();

        // (3a) Add TextAnnotation inside canvas, select via SelectionMoveResizer, nudge Left & Up
        Legend *txt = g->newLegend(QStringLiteral("Al"));
        CHECK_COND(txt != nullptr, "77g: newLegend('Al') failed");
        const long txtId = g->selectedMarkerKey();
        txt->setOrigin(QPoint(8, 20));
        plt->replot();
        QApplication::processEvents();
        CHECK_COND(txt->rect().right() <= plt->canvas()->width(),
                   "77g: TextAnnotation expected inside canvas()->width() (right="
                           << txt->rect().right() << ", cvW=" << plt->canvas()->width() << ")");
        g->setSelectedMarker(txtId, false);
        g->setCurrentSelection({ Graph::GraphSelection::TextAnnotation, int(txtId), -1 });
        SelectionMoveResizer *txtResizer = plt->canvas()->findChild<SelectionMoveResizer *>();
        CHECK_COND(txtResizer && txtResizer->contains(txt),
                   "77g: SelectionMoveResizer not active on TextAnnotation");
        const QRect txtRectBefore = txt->rect();
        txtResizer->setFocus();
        sendKey(txtResizer, Qt::Key_Left, Qt::NoModifier); // -2 X
        sendKey(txtResizer, Qt::Key_Up, Qt::NoModifier);   // -2 Y
        CHECK_COND(plt->property("_outsideRightMargin").toInt() == origRightMargin,
                   "77g: arrow-key nudging TextAnnotation wiped _outsideRightMargin!");
        CHECK_COND(plt->plotLayout()->canvasRect() == origCanvasRectOut
                           && g->geometry() == origGraphGeoOut && ml->size() == origMlSizeOut,
                   "77g: arrow-key nudging TextAnnotation changed canvasRect()!");
        CHECK_COND(txt->rect().topLeft() == txtRectBefore.topLeft() + QPoint(-2, -2),
                   "77g: TextAnnotation rect did not move by (-2, -2)!");

        // (3b) Select outside-right LegendBox via real mouse click (no manual setFocus!) and nudge
        //      left with Shift+Key_Left via QApplication::focusWidget() until inside canvas()->width(),
        //      then also test SelectionMoveResizer nudging on LegendBox inside canvas!
        Legend *leg = g->legend();
        CHECK_COND(leg != nullptr, "77g: g->legend() null");
        int legId = -1;
        for (int id : g->textMarkerKeys()) {
            if (g->textMarker(id) == leg) {
                legId = id;
                break;
            }
        }
        CHECK_COND(legId >= 0, "77g: failed to locate legendMarkerID");
        CHECK_COND(leg->rect().right() > plt->canvas()->width(),
                   "77g: outside-right legend expected initially right of canvas()->width()");

        // Path 1: real mouse click on outside-right LegendBox -> focus stays on plt->canvas()
        const QPoint legClickInPlot = plt->canvas()->pos() + leg->rect().center();
        {
            QMouseEvent mePress(QEvent::MouseButtonPress, legClickInPlot,
                                plt->mapToGlobal(legClickInPlot), Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier);
            QApplication::sendEvent(plt, &mePress);
            QMouseEvent meRel(QEvent::MouseButtonRelease, legClickInPlot,
                              plt->mapToGlobal(legClickInPlot), Qt::LeftButton, Qt::NoButton,
                              Qt::NoModifier);
            QApplication::sendEvent(plt, &meRel);
            QApplication::processEvents();
        }
        CHECK_COND(g->currentSelection().kind == Graph::GraphSelection::LegendBox,
                   "77g: mouse click on outside-right LegendBox did not select LegendBox");
        QWidget *focusedAfterOuterClick = QApplication::focusWidget();
        CHECK_COND(focusedAfterOuterClick == plt->canvas(),
                   "77g: mouse click on outside-right LegendBox lost focus from plt->canvas()");
        int canvasLegSteps = 0;
        while (leg->rect().right() > plt->canvas()->width() && canvasLegSteps < 80) {
            sendKey(focusedAfterOuterClick, Qt::Key_Left, Qt::ShiftModifier);
            ++canvasLegSteps;
            CHECK_COND(plt->property("_outsideRightMargin").toInt() == origRightMargin
                               && plt->plotLayout()->canvasRect() == origCanvasRectOut,
                       "77g: Shift+Key_Left on outside-right LegendBox wiped _outsideRightMargin or changed canvasRect() at step "
                               << canvasLegSteps);
        }
        CHECK_COND(leg->rect().right() <= plt->canvas()->width(),
                   "77g: outside-right LegendBox did not cross <= canvas()->width() after Shift+Key_Left nudges");

        // Path 2: inside-canvas LegendBox via SelectionMoveResizer while _outsideRightMargin > 0
        leg->setText(QStringLiteral("S1"));
        leg->setOrigin(QPoint(4, 16));
        plt->replot();
        plt->canvas()->repaint();
        QApplication::processEvents();
        g->setSelectedMarker(legId, false);
        g->setCurrentSelection({ Graph::GraphSelection::LegendBox, legId, -1 });
        SelectionMoveResizer *legResizer = plt->canvas()->findChild<SelectionMoveResizer *>();
        CHECK_COND(legResizer && legResizer->contains(leg),
                   "77g: SelectionMoveResizer not active on inside-canvas LegendBox");
        legResizer->setFocus();
        for (int extra = 0; extra < 2; ++extra) {
            sendKey(legResizer, Qt::Key_Left, Qt::NoModifier);
            CHECK_COND(plt->property("_outsideRightMargin").toInt() == origRightMargin
                               && plt->plotLayout()->canvasRect() == origCanvasRectOut,
                       "77g: Key_Left on LegendBox (SelectionMoveResizer) wiped _outsideRightMargin or changed canvasRect()!");
        }

        // ---------------------------------------------------------------------
        // (4) Stale MultiLayer::d_layers_selector clearance on AxisTitle / TextAnnotation selection
        // ---------------------------------------------------------------------
        auto findLayerResizerForGraph = [&](Graph *targetG) -> SelectionMoveResizer * {
            for (SelectionMoveResizer *sr : ml->findChildren<SelectionMoveResizer *>()) {
                if (sr && sr->contains(static_cast<QWidget *>(targetG)))
                    return sr;
            }
            return nullptr;
        };
        auto clickMultiLayerMarginToSelectLayer = [&]() {
            if (QScrollArea *sa = ml->findChild<QScrollArea *>(
                        QStringLiteral("MultiLayerCanvasScroll")))
                sa->ensureVisible(2, 2);
            QWidget *layerHost = g->parentWidget();
            const QPoint hostPt = g->frameGeometry().topLeft() + QPoint(2, 2);
            const QPoint mlPt = layerHost ? layerHost->mapTo(ml, hostPt) : QPoint(10, 35);
            QMouseEvent mePress(QEvent::MouseButtonPress, mlPt, Qt::LeftButton, Qt::LeftButton,
                                Qt::NoModifier);
            QApplication::sendEvent(ml, &mePress);
            QMouseEvent meRel(QEvent::MouseButtonRelease, mlPt, Qt::LeftButton, Qt::NoButton,
                              Qt::NoModifier);
            QApplication::sendEvent(ml, &meRel);
        };

        const QRect origLayerGeo4 = g->geometry();
        const QRect origCanvasRect4 = plt->plotLayout()->canvasRect();

        // Case A: margin click creates d_layers_selector wrapping g -> selecting AxisTitle deletes it
        clickMultiLayerMarginToSelectLayer();
        QPointer<SelectionMoveResizer> staleLayerSelA(findLayerResizerForGraph(g));
        CHECK_COND(!staleLayerSelA.isNull(),
                   "77g: simulated MultiLayer margin click did not create d_layers_selector on g");
        g->setCurrentSelection({ Graph::GraphSelection::AxisTitle, QwtPlot::xBottom, -1 });
        CHECK_COND(findLayerResizerForGraph(g) == nullptr,
                   "77g: selecting AxisTitle did not detach stale MultiLayer::d_layers_selector!");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CHECK_COND(staleLayerSelA.isNull(),
                   "77g: selecting AxisTitle did not deleteLater stale MultiLayer::d_layers_selector!");
        plt->axisWidget(QwtPlot::xBottom)->setFocus();
        sendKey(plt->axisWidget(QwtPlot::xBottom), Qt::Key_Down, Qt::ShiftModifier);
        CHECK_COND(g->geometry() == origLayerGeo4
                           && plt->plotLayout()->canvasRect() == origCanvasRect4,
                   "77g: arrow key after clearing d_layers_selector (AxisTitle) changed g->geometry()!");

        // Case B: margin click creates d_layers_selector wrapping g -> selecting TextAnnotation deletes it
        clickMultiLayerMarginToSelectLayer();
        QPointer<SelectionMoveResizer> staleLayerSelB(findLayerResizerForGraph(g));
        CHECK_COND(!staleLayerSelB.isNull(),
                   "77g: 2nd simulated MultiLayer margin click did not create d_layers_selector on g");
        g->setSelectedMarker(txtId, false);
        g->setCurrentSelection({ Graph::GraphSelection::TextAnnotation, int(txtId), -1 });
        CHECK_COND(findLayerResizerForGraph(g) == nullptr,
                   "77g: selecting TextAnnotation did not detach stale MultiLayer::d_layers_selector!");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CHECK_COND(staleLayerSelB.isNull(),
                   "77g: selecting TextAnnotation did not deleteLater stale MultiLayer::d_layers_selector!");
        if (SelectionMoveResizer *tr = plt->canvas()->findChild<SelectionMoveResizer *>()) {
            const QPoint txtBefore4 = txt->rect().topLeft();
            sendKey(tr, Qt::Key_Right, Qt::ShiftModifier);
            CHECK_COND(g->geometry() == origLayerGeo4
                               && plt->plotLayout()->canvasRect() == origCanvasRect4
                               && txt->rect().topLeft() == txtBefore4 + QPoint(10, 0),
                       "77g: arrow key after clearing d_layers_selector (TextAnnotation) changed g->geometry()!");
        }
    }

    w77.reset();
    std::cout << "[PASS] Suite 77 passed.\n";
    return 0;
}

#include "future/matrix/future_Matrix.h"
#include <qwt_layout_metrics.h>
#include <qwt_painter.h>

namespace {
struct Suite78AxesDialogProbe : public AxesDialog
{
    QListWidget *scaleAxisList() const { return axesList; }
    QListWidget *titleAxisList() const { return axesTitlesList; }
    QListWidget *breakAxisList() const { return axisBreakAxesList; }
};
} // namespace

int runSuite78ZeroBaseUxAndQualityGate(ApplicationWindow *mw)
{
    Q_UNUSED(mw);
    std::cout << "[INFO] Running Suite 78: Zero-Base UX & Quality Revamp Comprehensive Gate...\n";

    struct Suite78EnvGuard
    {
        QVariant prevNonInteractive;
        bool prevTestMode = false;
        Suite78EnvGuard()
            : prevNonInteractive(qApp->property("torigraph_noninteractive")),
              prevTestMode(QStandardPaths::isTestModeEnabled())
        {
            qApp->setProperty("torigraph_noninteractive", true);
            QStandardPaths::setTestModeEnabled(true);
        }
        ~Suite78EnvGuard()
        {
            QStandardPaths::setTestModeEnabled(prevTestMode);
            qApp->setProperty("torigraph_noninteractive", prevNonInteractive);
        }
    } envGuard78;

    struct AppWinGuard78
    {
        ApplicationWindow *win = nullptr;
        explicit AppWinGuard78(ApplicationWindow *w = nullptr) : win(w) {}
        ~AppWinGuard78()
        {
            reset();
            QApplication::processEvents();
        }
        AppWinGuard78(const AppWinGuard78 &) = delete;
        AppWinGuard78 &operator=(const AppWinGuard78 &) = delete;
        void reset(ApplicationWindow *next = nullptr)
        {
            if (win) {
                win->savedProject();
                win->setOleEmbeddedMode(true);
                delete win;
            }
            win = next;
        }
        ApplicationWindow *get() const { return win; }
        ApplicationWindow *operator->() const { return win; }
        operator ApplicationWindow *() const { return win; }
    };

    QTemporaryDir tmp78;
    CHECK_COND(tmp78.isValid(), "78: temp dir creation failed");

    AppWinGuard78 w78(new ApplicationWindow());
    w78->applyUserSettings();
    w78->resize(1440, 860);
    w78->show();
    w78->switchToLanguage("en");
    QCoreApplication::processEvents();

    // -------------------------------------------------------------------------
    // 78a: Table/Matrix close -> DeferredDelete -> Undo -> Redo -> savedProject()
    // -------------------------------------------------------------------------
    {
        QUndoStack *projUndo = w78->project() ? w78->project()->undoStack() : nullptr;
        CHECK_COND(projUndo != nullptr, "78a: project undoStack null");
        projUndo->clear();

        Table *t = w78->newTable(4, 2, "T78Undo", "UndoTable");
        Matrix *m = w78->newMatrix("M78Undo", 3, 3);
        CHECK_COND(t != nullptr && m != nullptr, "78a: failed to create Table/Matrix");
        t->setCell(0, 0, 1.0);
        t->setCell(0, 1, 2.0);
        m->setCell(0, 0, 9.5);
        t->showNormal();
        m->showNormal();
        QCoreApplication::processEvents();

        QPointer<Table> pTbl(t);
        QPointer<Matrix> pMtx(m);
        t->askOnCloseEvent(false);
        m->askOnCloseEvent(false);
        t->close();
        m->close();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        CHECK_COND(w78->table("T78Undo") == nullptr && w78->matrix("M78Undo") == nullptr,
                   "78a: Table/Matrix still registered after close");
        CHECK_COND(projUndo->canUndo(), "78a: project undoStack empty after closing Table & Matrix");

        projUndo->undo();
        projUndo->undo();
        QCoreApplication::processEvents();
        Table *restoredT = w78->table("T78Undo");
        Matrix *restoredM = w78->matrix("M78Undo");
        CHECK_COND(restoredT != nullptr && restoredM != nullptr,
                   "78a: project undo failed to restore closed Table/Matrix");
        CHECK_COND(std::fabs(restoredT->cell(0, 1) - 2.0) < 1e-9
                           && std::fabs(restoredM->cell(0, 0) - 9.5) < 1e-9,
                   "78a: restored Table/Matrix lost cell contents");

        QPointer<Table> pRestT(restoredT);
        QPointer<Matrix> pRestM(restoredM);
        projUndo->redo();
        projUndo->redo();
        w78->savedProject();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        CHECK_COND(pTbl.isNull() && pMtx.isNull() && pRestT.isNull() && pRestM.isNull(),
                   "78a: closed Table/Matrix survived redo + savedProject() + DeferredDelete!");
        CHECK_COND(w78->table("T78Undo") == nullptr && w78->matrix("M78Undo") == nullptr,
                   "78a: redo did not remove restored Table/Matrix cleanly");
    }

    // -------------------------------------------------------------------------
    // 78b: ModernUiTheme active subwindow preservation & language toggle idempotence
    // -------------------------------------------------------------------------
    MultiLayer *mlActiveKeep = w78->newGraph("G78Active");
    CHECK_COND(mlActiveKeep != nullptr, "78b: failed to create mlActiveKeep");
    Table *tblOther = w78->newTable(3, 2, "T78Other", "Other");
    CHECK_COND(tblOther != nullptr, "78b: failed to create tblOther");
    w78->d_workspace.setActiveSubWindow(mlActiveKeep);
    QCoreApplication::processEvents();
    CHECK_COND(w78->activeWindow() == mlActiveKeep, "78b: failed to activate mlActiveKeep");

    ModernUiTheme::applyModernTheme(qApp);
    QCoreApplication::processEvents();
    CHECK_COND(w78->activeWindow() == mlActiveKeep,
               "78b: ModernUiTheme::applyModernTheme altered active MDI subwindow!");

    CHECK_COND(w78->stylePresetsMenu != nullptr, "78b: stylePresetsMenu null");
    const int baseSubMenus = w78->stylePresetsMenu
                                     ->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly)
                                     .size();
    const int basePresetActions = w78->stylePresetsMenu->actions().size();
    for (int i = 0; i < 3; ++i) {
        w78->switchToLanguage("ja");
        QCoreApplication::processEvents();
        w78->switchToLanguage("en");
        QCoreApplication::processEvents();
    }
    w78->toggleLanguage();
    QCoreApplication::processEvents();
    w78->toggleLanguage();
    QCoreApplication::processEvents();
    CHECK_COND(w78->stylePresetsMenu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly).size()
                               == baseSubMenus
                       && w78->stylePresetsMenu->actions().size() == basePresetActions,
               "78b: language switching leaked submenus/actions in stylePresetsMenu!");

    {
        AppWinGuard78 w78Twin(new ApplicationWindow());
        w78Twin->applyUserSettings();
        w78Twin->switchToLanguage("ja");
        QCoreApplication::processEvents();
        w78->switchToLanguage("ja");
        QCoreApplication::processEvents();
        w78->switchToLanguage("en");
        QCoreApplication::processEvents();
        PlotDialog pdEn(false, w78);
        CHECK_COND(!pdEn.windowTitle().contains(QChar(0x30B0)) && pdEn.windowTitle() == QStringLiteral("Plot details"),
                   "78b: stale JA translator leaked into PlotDialog after multi-window en switch!");
        w78Twin.reset();
    }

    // -------------------------------------------------------------------------
    // 78c: 300-DPI PNG zero ClearType chromatic leak & Table::clearCell on empty/Text columns
    // -------------------------------------------------------------------------
    {
        Table *tClr = w78->newTable(5, 2, "T78ClearEmpty", "Clear");
        CHECK_COND(tClr != nullptr && tClr->column(1) != nullptr, "78c: T78ClearEmpty creation failed");
        CHECK_COND(tClr->column(1)->rowCount() == 0, "78c: expected fresh column rowCount() == 0");
        tClr->clearCell(1, 1);
        CHECK_COND(tClr->column(1)->isInvalid(1),
                   "78c: Table::clearCell(1, 1) on empty column (rowCount==0) failed to mark cell invalid!");

        Table *tClrTxt = w78->newTable(4, 1, "T78ClearText", "ClearText");
        CHECK_COND(tClrTxt != nullptr && tClrTxt->column(0) != nullptr, "78c: T78ClearText creation failed");
        tClrTxt->setColumnType(0, SciDAVis::ColumnMode::Text);
        tClrTxt->setText(1, 0, QStringLiteral("SampleNote"));
        CHECK_COND(tClrTxt->text(1, 0) == QStringLiteral("SampleNote"), "78c: failed to populate text cell");
        tClrTxt->clearCell(1, 0);
        CHECK_COND(!tClrTxt->column(0)->isInvalid(1) && tClrTxt->text(1, 0).isEmpty(),
                   "78c: Table::clearCell(1, 0) on Text column must leave isInvalid==false and blank text!");

        for (int r = 0; r < 5; ++r) {
            tClr->setCell(r, 0, double(r));
            tClr->setCell(r, 1, double(r) * 2.0 + 1.0);
        }
        MultiLayer *mlPng = w78->multilayerPlot(tClr, QStringList() << tClr->colName(1), Graph::Line);
        CHECK_COND(mlPng != nullptr && mlPng->activeGraph() != nullptr, "78c: mlPng failed");
        Graph *gPng = mlPng->activeGraph();
        QwtPlotCurve *cPng = gPng->curve(0);
        CHECK_COND(cPng != nullptr, "78c: expected curve 0 on mlPng");
        cPng->setPen(QPen(Qt::black, 2.0));
        gPng->setTitle(QStringLiteral("Publication Black Text Check"));
        gPng->setAxisTitle(QwtPlot::xBottom, QStringLiteral("Strain"));
        gPng->setAxisTitle(QwtPlot::yLeft, QStringLiteral("Stress / MPa"));
        gPng->replot();
        QCoreApplication::processEvents();

        auto countChromaLeaks = [](const QImage &img) -> int {
            int leaks = 0;
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const QRgb px = img.pixel(x, y);
                    const int r = qRed(px), g = qGreen(px), b = qBlue(px);
                    if (std::max({ r, g, b }) - std::min({ r, g, b }) >= 35)
                        ++leaks;
                }
            }
            return leaks;
        };

        // Positive control: prove ClearType detector flags synthetic chromatic subpixel fringes.
        QImage synthFringe(16, 16, QImage::Format_ARGB32);
        synthFringe.fill(Qt::white);
        synthFringe.setPixel(4, 4, qRgb(255, 80, 20));
        synthFringe.setPixel(5, 4, qRgb(20, 120, 255));
        CHECK_COND(countChromaLeaks(synthFringe) == 2,
                   "78c: ClearType detector failed positive control on synthetic chromatic fringes!");

        const QString png300 = tmp78.filePath("clearType_check_300dpi.png");
        CHECK_COND(mlPng->exportImageChecked(png300, -1, false, 300), "78c: exportImageChecked failed");
        QImage pngImg(png300);
        CHECK_COND(!pngImg.isNull() && pngImg.width() > 300, "78c: exported PNG unreadable");
        const int chromaLeaks = countChromaLeaks(pngImg);
        CHECK_COND(chromaLeaks == 0,
                   "78c: 300-DPI PNG still exhibits ClearType RGB chromatic fringes (" << chromaLeaks << ")");
    }

    // -------------------------------------------------------------------------
    // 78d: Truncated .sciprj EOF detection & "~" backup creation on save
    // -------------------------------------------------------------------------
    {
        AppWinGuard78 wBak(new ApplicationWindow());
        wBak->applyUserSettings();
        Table *tBak = wBak->newTable(3, 2, "T78Bak", "Backup");
        tBak->setCell(0, 1, 12.5);
        const QString prjBak = tmp78.filePath("backup_test_78.sciprj");
        CHECK_COND(wBak->writeProjectFile(wBak->projectFolder(), prjBak), "78d: 1st writeProjectFile failed");
        tBak->setCell(0, 1, 99.5);
        CHECK_COND(wBak->writeProjectFile(wBak->projectFolder(), prjBak), "78d: 2nd writeProjectFile failed");
        const QString tildePath = prjBak + QLatin1Char('~');
        CHECK_COND(QFileInfo::exists(tildePath), "78d: '~' backup file was not created on 2nd save");

        AppWinGuard78 reTilde(wBak->openProject(tildePath));
        CHECK_COND(reTilde != nullptr && reTilde->table("T78Bak") != nullptr
                           && std::fabs(reTilde->table("T78Bak")->cell(0, 1) - 12.5) < 1e-9,
                   "78d: '~' backup did not retain previous saved state (expected 12.5)");
        reTilde.reset();

        QByteArray fullBytes;
        {
            QFile pf(prjBak);
            CHECK_COND(pf.open(QIODevice::ReadOnly), "78d: read prjBak failed");
            fullBytes = pf.readAll();
            pf.close();
        }
        CHECK_COND(fullBytes.size() > 200, "78d: prjBak unexpectedly tiny");
        const QString truncPath = tmp78.filePath("truncated_78.sciprj");
        {
            QFile tf(truncPath);
            CHECK_COND(tf.open(QIODevice::WriteOnly | QIODevice::Truncate), "78d: write truncPath failed");
            tf.write(fullBytes.left(fullBytes.size() / 2));
            tf.close();
        }
        AppWinGuard78 reTrunc(wBak->openProject(truncPath));
        CHECK_COND(reTrunc == nullptr, "78d: truncated .sciprj was not rejected on openProject!");
    }

    // -------------------------------------------------------------------------
    // 78e: Non-table safety guards on addColToTable() & cutSelection()
    // -------------------------------------------------------------------------
    {
        const int colsBefore = tblOther->numCols();
        w78->d_workspace.setActiveSubWindow(mlActiveKeep);
        QCoreApplication::processEvents();
        w78->addColToTable();
        w78->cutSelection();
        QCoreApplication::processEvents();
        CHECK_COND(tblOther->numCols() == colsBefore,
                   "78e: addColToTable() mutated non-active table while graph window was active");
    }

    // -------------------------------------------------------------------------
    // 78f: AxesDialog::setCurrentQwtAxis()
    // -------------------------------------------------------------------------
    {
        Suite78AxesDialogProbe axDlg;
        axDlg.setGraph(mlActiveKeep->activeGraph());
        struct AxisCase { int qwtId; int expectedRow; };
        const AxisCase cases[] = {
            { QwtPlot::xBottom, 0 },
            { QwtPlot::yLeft, 1 },
            { QwtPlot::xTop, 2 },
            { QwtPlot::yRight, 3 },
        };
        for (const AxisCase &c : cases) {
            axDlg.setCurrentQwtAxis(c.qwtId);
            CHECK_COND(axDlg.scaleAxisList() && axDlg.scaleAxisList()->currentRow() == c.expectedRow,
                       "78f: scaleAxisList row mismatch for qwtAxis " << c.qwtId);
            CHECK_COND(axDlg.titleAxisList() && axDlg.titleAxisList()->currentRow() == c.expectedRow,
                       "78f: titleAxisList row mismatch for qwtAxis " << c.qwtId);
            CHECK_COND(axDlg.breakAxisList() && axDlg.breakAxisList()->currentRow() == c.expectedRow,
                       "78f: breakAxisList row mismatch for qwtAxis " << c.qwtId);
        }
    }

    // -------------------------------------------------------------------------
    // 78g: BoxCurve::Tukey15IQR & Column::UnitFormat::Bracket XML round-trip
    // -------------------------------------------------------------------------
    {
        Table *tBox = w78->newTable(7, 2, "T78BoxUnit", "BoxUnit");
        const double vals[7] = { 10.0, 12.0, 14.0, 15.0, 16.0, 18.0, 100.0 };
        for (int r = 0; r < 7; ++r) {
            tBox->setCell(r, 0, double(r + 1));
            tBox->setCell(r, 1, vals[r]);
        }
        MultiLayer *mlBox = w78->multilayerPlot(tBox, QStringList() << tBox->colName(1), Graph::Box);
        mlBox->setName("G78BoxUnit");
        Graph *gBox = mlBox->activeGraph();
        CHECK_COND(gBox != nullptr, "78g: gBox null");
        BoxCurve *bc = dynamic_cast<BoxCurve *>(gBox->curve(0));
        CHECK_COND(bc != nullptr, "78g: curve 0 is not BoxCurve");
        bc->setWhiskersRange(BoxCurve::Tukey15IQR, 1.5);
        gBox->setAxisUnitFormat(Column::UnitFormat::Bracket);
        gBox->replot();

        const QString boxPrj = tmp78.filePath("box_unit_78.sciprj");
        CHECK_COND(w78->writeProjectFile(w78->projectFolder(), boxPrj), "78g: write boxPrj failed");
        AppWinGuard78 reBoxWin(w78->openProject(boxPrj));
        CHECK_COND(reBoxWin != nullptr, "78g: reload boxPrj failed");
        MultiLayer *reMlBox = qobject_cast<MultiLayer *>(reBoxWin->window("G78BoxUnit"));
        CHECK_COND(reMlBox != nullptr && reMlBox->activeGraph() != nullptr, "78g: reloaded G78BoxUnit missing");
        BoxCurve *reBc = dynamic_cast<BoxCurve *>(reMlBox->activeGraph()->curve(0));
        CHECK_COND(reBc != nullptr && reBc->whiskersRangeType() == BoxCurve::Tukey15IQR,
                   "78g: BoxCurve::Tukey15IQR lost across .sciprj XML round-trip!");
        CHECK_COND(reMlBox->activeGraph()->axisUnitFormat() == Column::UnitFormat::Bracket,
                   "78g: Column::UnitFormat::Bracket lost across .sciprj XML round-trip!");
    }

    // -------------------------------------------------------------------------
    // 78h: RecalcManager XML round-trip, rename tracking, & non-destructive failure
    // -------------------------------------------------------------------------
    {
        Table *tRec = w78->newTable(6, 2, "RecSrc78", "Recalc");
        tRec->setColName(0, "X");
        tRec->setColPlotDesignation(0, SciDAVis::X);
        tRec->setColName(1, "Y");
        tRec->setColPlotDesignation(1, SciDAVis::Y);
        for (int r = 0; r < 6; ++r) {
            tRec->setCell(r, 0, double(r));
            tRec->setCell(r, 1, 3.0 * double(r) + 2.0);
        }
        MultiLayer *mlRec = w78->multilayerPlot(tRec, QStringList() << "RecSrc78_Y", Graph::Scatter);
        Graph *gRec = mlRec->activeGraph();
        RecalcManager *rm = RecalcManager::instance(w78);
        CHECK_COND(rm != nullptr && gRec != nullptr, "78h: RecalcManager/gRec null");
        rm->setRecalcMode(RecalcManager::Auto);

        const int lid = rm->registerLink(w78, gRec, tRec, "RecSrc78_Y", "Rec78Fit", RecalcManager::OpLinearFit);
        CHECK_COND(lid > 0 && rm->activeLinkCount(gRec) == 1 && gRec->curve("Rec78Fit") != nullptr,
                   "78h: initial RecalcManager link registration failed");

        const QString xml = rm->saveLinksXmlForGraph(gRec);
        CHECK_COND(!xml.isEmpty() && xml.contains("RecSrc78_Y"), "78h: saveLinksXmlForGraph returned empty/invalid XML");

        rm->removeLinksForGraph(gRec);
        CHECK_COND(rm->activeLinkCount(gRec) == 0, "78h: removeLinksForGraph failed");
        rm->restoreLinkXmlForGraph(w78, gRec, xml);
        CHECK_COND(rm->activeLinkCount(gRec) == 1, "78h: restoreLinkXmlForGraph did not restore link");

        w78->renameWindow(tRec, "RecSrc78Ren");
        tRec->setColName(1, "Y2");
        rm->notifyTableRenamed("RecSrc78", "RecSrc78Ren");
        rm->notifyColumnRenamed("RecSrc78Ren", "Y", "Y2");
        QCoreApplication::processEvents();
        const int updated = rm->recalculateAll(gRec);
        CHECK_COND(updated >= 1 && rm->lastError(gRec).isEmpty() && gRec->curve("Rec78Fit") != nullptr,
                   "78h: RecalcManager failed after rename tracking!");

        // Corrupt source data -> recalculate must fail non-destructively and record lastError(gRec)
        const int ptsBefore = gRec->curve("Rec78Fit")->dataSize();
        for (int r = 0; r < 6; ++r)
            tRec->clearCell(r, 1);
        QCoreApplication::processEvents();
        rm->recalculateAll(gRec);
        CHECK_COND(!rm->lastError(gRec).isEmpty(), "78h: expected non-empty lastError after degenerate source data");
        CHECK_COND(gRec->curve("Rec78Fit") != nullptr && gRec->curve("Rec78Fit")->dataSize() == ptsBefore,
                   "78h: failed recalculation destroyed previous output curve!");
        rm->removeLinksForGraph(gRec);
    }

    // -------------------------------------------------------------------------
    // 78i & 78j: lab_slide_83mm XML round-trip & fitPageZoomToViewport() at 1440x860 / 911x512
    // -------------------------------------------------------------------------
    MultiLayer *mlLab = w78->newGraph("G78LabSlide");
    CHECK_COND(mlLab != nullptr, "78i: mlLab null");
    const QString labWinName = mlLab->name();
    CHECK_COND(mlLab->applyStylePresetById(QStringLiteral("lab_slide_83mm")),
               "78i: applyStylePresetById('lab_slide_83mm') failed");
    CHECK_COND(mlLab->appliedStylePresetId() == QStringLiteral("lab_slide_83mm"),
               "78i: appliedStylePresetId() != lab_slide_83mm");
    const QString savedStr = mlLab->saveToString(QString());
    CHECK_COND(savedStr.contains(QStringLiteral("<ToriAppliedStylePreset id=\"lab_slide_83mm\"/>")),
               "78i: saveToString missing <ToriAppliedStylePreset id=\"lab_slide_83mm\"/>");

    const QString labPrj = tmp78.filePath("lab_slide_78.sciprj");
    CHECK_COND(w78->writeProjectFile(w78->projectFolder(), labPrj), "78i: write labPrj failed");
    {
        AppWinGuard78 reLab(w78->openProject(labPrj));
        CHECK_COND(reLab != nullptr, "78i: openProject(labPrj) returned null!");
        MultiLayer *reMl = qobject_cast<MultiLayer *>(reLab->window(labWinName));
        CHECK_COND(reMl != nullptr, "78i: reloaded project missing " << labWinName.toStdString());
        CHECK_COND(reMl->appliedStylePresetId() == QStringLiteral("lab_slide_83mm"),
                   "78i: lab_slide_83mm appliedStylePresetId lost on .sciprj reload (got '"
                           << reMl->appliedStylePresetId().toStdString() << "')!");
    }

    for (const QSize &vpSz : { QSize(1440, 860), QSize(911, 512) }) {
        w78->resize(vpSz);
        mlLab->showNormal();
        QCoreApplication::processEvents();
        mlLab->fitPageZoomToViewport();
        QCoreApplication::processEvents();
        const double zf = mlLab->fitPageZoomFactor();
        CHECK_COND(zf >= 0.35 && zf <= 1.6, "78j: fitPageZoomFactor out of bounds");
        QScrollArea *cs = mlLab->widget()
                ? mlLab->widget()->findChild<QScrollArea *>(QStringLiteral("MultiLayerCanvasScroll"))
                : nullptr;
        CHECK_COND(cs != nullptr && cs->viewport() != nullptr && mlLab->canvasWidget() != nullptr,
                   "78j: MultiLayerCanvasScroll / viewport / canvasWidget missing at size "
                           << vpSz.width() << "x" << vpSz.height());
        CHECK_COND(mlLab->canvasWidget()->width() <= cs->viewport()->width() + 2,
                   "78j: page canvas clipped horizontally at size " << vpSz.width() << "x" << vpSz.height());
    }
    w78->resize(1440, 860);
    QCoreApplication::processEvents();

    // -------------------------------------------------------------------------
    // 78k: EmptyTableHint JA/LanguageChange & safe teardown; FocusTableDrawer opaque card
    // -------------------------------------------------------------------------
    {
        Table *tHint = w78->newTable(4, 2, "T78EmptyHint", "Hint");
        w78->attachEmptyTableHint(tHint);
        tHint->showNormal();
        QCoreApplication::processEvents();
        QLabel *hintLbl = tHint->findChild<QLabel *>(QStringLiteral("ToriEmptyTableHintLabel"));
        CHECK_COND(hintLbl != nullptr, "78k: ToriEmptyTableHintLabel not attached");
        const QString enHintText = hintLbl->text();
        w78->switchToLanguage("ja");
        QCoreApplication::processEvents();
        CHECK_COND(hintLbl->text() != enHintText && !hintLbl->text().isEmpty(),
                   "78k: EmptyTableHint did not retranslate on LanguageChange");
        tHint->askOnCloseEvent(false);
        tHint->close();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        w78->switchToLanguage("en");
        QCoreApplication::processEvents();

        ModernRibbonBar *ribbon78 = w78->modernRibbonBar();
        CHECK_COND(ribbon78 != nullptr, "78k: w78->modernRibbonBar() null");
        FocusTableDrawer *drawer = ribbon78->focusTableDrawer();
        CHECK_COND(drawer != nullptr, "78k: focusTableDrawer() null");
        CHECK_COND(drawer->styleSheet().contains(QStringLiteral("#FFFFFF")),
                   "78k: FocusTableDrawer should use solid opaque #FFFFFF card background");
    }

    // -------------------------------------------------------------------------
    // 78l: GraphInspectorDock empty state, E5-2 flush on switch, E5-5 disconnect, E5-6 no-op FocusOut
    // -------------------------------------------------------------------------
    {
        GraphInspectorDock *insp = w78->graphInspectorDock();
        CHECK_COND(insp != nullptr, "78l: graphInspectorDock null");
        insp->setTargetMultiLayer(nullptr);
        QCoreApplication::processEvents();
        CHECK_COND(insp->isShowingQuickStart(), "78l: inspector should show QuickStart when target is null");
        insp->setCollapsedRail(true);
        QCoreApplication::processEvents();
        CHECK_COND(insp->isCollapsedRail(), "78l: failed to collapse inspector rail with null graph");
        insp->setCollapsedRail(false);
        QCoreApplication::processEvents();

        MultiLayer *ml1 = w78->newGraph("G78Insp1");
        MultiLayer *ml2 = w78->newGraph("G78Insp2");
        CHECK_COND(ml1 && ml1->activeGraph() && ml2 && ml2->activeGraph(), "78l: ml1/ml2 creation failed");
        ml1->activeGraph()->setAxisTitle(QwtPlot::xBottom, QStringLiteral("InitX1"));
        ml2->activeGraph()->setAxisTitle(QwtPlot::xBottom, QStringLiteral("InitX2"));

        insp->setTargetMultiLayer(ml1, true);
        insp->switchToInspectorPage(GraphInspectorDock::PageTabAxis);
        QCoreApplication::processEvents();

        QWidget *axisPage = insp->propertyStack()->widget(GraphInspectorDock::PageTabAxis);
        CHECK_COND(axisPage != nullptr, "78l: inspector Axis page null");
        QLineEdit *titleEdit = nullptr;
        QLineEdit *minEdit = nullptr;
        for (QLineEdit *le : axisPage->findChildren<QLineEdit *>()) {
            if (!le->placeholderText().isEmpty() && !titleEdit)
                titleEdit = le;
            else if (le->placeholderText().isEmpty() && le->property("_syncedText").isValid()
                     && le->isEnabled() && !minEdit)
                minEdit = le;
        }
        CHECK_COND(titleEdit != nullptr && minEdit != nullptr, "78l: failed to locate axis title/min edits");

        // E5-6: no-op editingFinished on minEdit & titleEdit must NOT push onto Undo stack
        const int undoBeforeMin = ml1->undoStack() ? ml1->undoStack()->count() : 0;
        QMetaObject::invokeMethod(minEdit, "editingFinished", Qt::DirectConnection);
        QMetaObject::invokeMethod(titleEdit, "editingFinished", Qt::DirectConnection);
        QCoreApplication::processEvents();
        const int undoAfterMin = ml1->undoStack() ? ml1->undoStack()->count() : 0;
        CHECK_COND(undoAfterMin == undoBeforeMin,
                   "78l: no-op focus-out on Axis Min/Title pushed spurious Undo entry!");

        // E5-2 & E5-5: pending debounced title edit flushes cleanly onto ml1 before rebinding to ml2
        titleEdit->setText(QStringLiteral("FlushedX1"));
        emit titleEdit->textEdited(QStringLiteral("FlushedX1"));
        insp->setTargetMultiLayer(ml2);
        QCoreApplication::processEvents();
        CHECK_COND(ml1->activeGraph()->axisTitle(QwtPlot::xBottom) == QStringLiteral("FlushedX1"),
                   "78l: pending debounced axis title edit did not flush to original ml1 on switch!");
        CHECK_COND(ml2->activeGraph()->axisTitle(QwtPlot::xBottom) == QStringLiteral("InitX2"),
                   "78l: flushed axis title leaked into new target ml2!");
    }

    // -------------------------------------------------------------------------
    // 78m: RPC allow-list security, malformed JSON, & Graph3D::exportImageChecked
    // -------------------------------------------------------------------------
    {
        PythonAutomationServer *srv = w78->pythonAutomationServer();
        CHECK_COND(srv != nullptr, "78m: pythonAutomationServer null");

        QJsonObject badActReq;
        badActReq["jsonrpc"] = "2.0";
        badActReq["id"] = 7801;
        badActReq["method"] = "app.trigger_action";
        badActReq["params"] = QJsonObject{ { "name", "deleteWindow" } };
        const QJsonObject badActResp = srv->handleRpcRequest(badActReq);
        CHECK_COND(badActResp.contains("error"),
                   "78m: app.trigger_action allowed non-allowlisted target!");

        for (const char *blockedSlot : { "closeActiveWindow", "deleteSelectedItems" }) {
            QJsonObject slotReq;
            slotReq["jsonrpc"] = "2.0";
            slotReq["id"] = 7802;
            slotReq["method"] = "app.trigger_action";
            slotReq["params"] = QJsonObject{ { "name", QString::fromLatin1(blockedSlot) } };
            const QJsonObject slotResp = srv->handleRpcRequest(slotReq);
            const QString errMsg = slotResp.value("error").toObject().value("message").toString();
            CHECK_COND(slotResp.contains("error") && errMsg.contains(QStringLiteral("allow-list"), Qt::CaseInsensitive),
                       "78m: destructive slot '" << blockedSlot << "' error must reference allow-list (got: "
                                                 << errMsg.toStdString() << ")");
        }

        for (const char *allowedTarget : { "actionNextWindow", "actionOpenPythonConsole" }) {
            QJsonObject okActReq;
            okActReq["jsonrpc"] = "2.0";
            okActReq["id"] = 7803;
            okActReq["method"] = "app.trigger_action";
            okActReq["params"] = QJsonObject{ { "name", QString::fromLatin1(allowedTarget) } };
            const QJsonObject okActResp = srv->handleRpcRequest(okActReq);
            CHECK_COND(!okActResp.contains("error") && okActResp.value("result").toObject().value("ok").toBool(),
                       "78m: allowed target '" << allowedTarget << "' unexpectedly failed: "
                                               << QString::fromUtf8(QJsonDocument(okActResp).toJson(QJsonDocument::Compact)).toStdString());
        }

        const QByteArray parseRespRaw = srv->handleRpcLine(QByteArray("{not-valid-json\n"));
        const QJsonObject parseResp = QJsonDocument::fromJson(parseRespRaw).object();
        CHECK_COND(parseResp.contains("error") && parseResp.value("error").toObject().value("code").toInt() == -32700,
                   "78m: malformed JSON line did not return JSON-RPC -32700 parse error!");

        Graph3D *g3d = w78->newPlot3D();
        CHECK_COND(g3d != nullptr, "78m: newPlot3D failed");
        QCoreApplication::processEvents();
        QString errBad;
        CHECK_COND(!g3d->exportImageChecked(tmp78.filePath("missing_dir_78/out.png"), 100, false, &errBad)
                           && !errBad.isEmpty(),
                   "78m: Graph3D::exportImageChecked failed to report error on unwritable path!");
        QString errOk;
        const QString ok3dPath = tmp78.filePath("ok_3d_78.png");
        CHECK_COND(g3d->exportImageChecked(ok3dPath, 100, false, &errOk) && QFileInfo::exists(ok3dPath),
                   "78m: Graph3D::exportImageChecked failed on valid path: " << errOk.toStdString());
    }

    // -------------------------------------------------------------------------
    // 78n: Standalone future::Table/Matrix setView(nullptr) teardown safety
    // -------------------------------------------------------------------------
    {
        Table *tNv = w78->newTable(3, 2, "T78NullView", "NullView");
        Matrix *mNv = w78->newMatrix("M78NullView", 3, 3);
        CHECK_COND(tNv != nullptr && tNv->d_future_table != nullptr
                           && mNv != nullptr && mNv->d_future_matrix != nullptr,
                   "78n: future Table/Matrix missing");
        tNv->d_future_table->setView(nullptr);
        mNv->d_future_matrix->setView(nullptr);
        QCoreApplication::processEvents();
    }

    // -------------------------------------------------------------------------
    // 78o: Table::reimportFromSource() preserves Numeric columns & linked Graph curves
    // -------------------------------------------------------------------------
    {
        const QString reimpCsvPath = tmp78.filePath(QStringLiteral("reimport_78o.csv"));
        {
            QFile cf(reimpCsvPath);
            CHECK_COND(cf.open(QIODevice::WriteOnly), "78o: write initial CSV");
            cf.write("X,Y\n1,10\n2,20\n3,30\n4,40\n");
            cf.close();
        }
        QSet<Table *> beforeTbls;
        if (QList<MyWidget *> *tl = w78->tableList()) {
            for (MyWidget *mwItem : *tl) {
                if (Table *tp = qobject_cast<Table *>(mwItem))
                    beforeTbls.insert(tp);
            }
        }
        w78->importASCII(QStringList() << reimpCsvPath, ImportASCIIDialog::NewTables,
                         QStringLiteral(","), 0, true, true, false, true, QLocale::c());
        Table *tRe = nullptr;
        if (QList<MyWidget *> *tl = w78->tableList()) {
            for (MyWidget *mwItem : *tl) {
                if (Table *tp = qobject_cast<Table *>(mwItem)) {
                    if (!beforeTbls.contains(tp))
                        tRe = tp;
                }
            }
        }
        CHECK_COND(tRe != nullptr && tRe->numCols() >= 2 && tRe->numRows() == 4,
                   "78o: importASCII failed to create imported table");
        CHECK_COND(tRe->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && tRe->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric,
                   "78o: initial imported columns must be Numeric");

        MultiLayer *mlRe = w78->multilayerPlot(tRe, QStringList() << tRe->colName(1), Graph::Line);
        CHECK_COND(mlRe != nullptr && mlRe->activeGraph() != nullptr, "78o: multilayerPlot failed");
        Graph *gRe = mlRe->activeGraph();
        CHECK_COND(gRe->curves() == 1, "78o: expected 1 curve linked to imported table");

        {
            QFile cf(reimpCsvPath);
            CHECK_COND(cf.open(QIODevice::WriteOnly | QIODevice::Truncate), "78o: overwrite CSV");
            cf.write("X,Y\n1,15\n2,25\n3,35\n4,45\n5,55\n");
            cf.close();
        }
        CHECK_COND(tRe->reimportFromSource(), "78o: Table::reimportFromSource() failed");
        QCoreApplication::processEvents();
        CHECK_COND(tRe->column(0)->columnMode() == SciDAVis::ColumnMode::Numeric
                           && tRe->column(1)->columnMode() == SciDAVis::ColumnMode::Numeric,
                   "78o: reimportFromSource converted Numeric columns to non-Numeric!");
        CHECK_COND(tRe->numRows() == 5 && std::fabs(tRe->cell(1, 1) - 25.0) < 1e-9,
                   "78o: reimportFromSource table values not updated");
        CHECK_COND(gRe->curves() == 1,
                   "78o: reimportFromSource dropped linked curve on Graph!");
        DataCurve *dcRe = dynamic_cast<DataCurve *>(gRe->curve(0));
        CHECK_COND(dcRe != nullptr && dcRe->dataSize() == 5 && std::fabs(dcRe->y(4) - 55.0) < 1e-9,
                   "78o: linked DataCurve not refreshed after reimportFromSource!");

        // Swapped columns ([X, Y] -> [Y, X]) must rename cleanly via two-pass rename without '-1' collision,
        // and stale formulas/masks must be cleared.
        tRe->column(0)->setFormula(0, QStringLiteral("col(\"Y\")"));
        tRe->column(0)->setMasked(0, true);
        {
            QFile cf(reimpCsvPath);
            CHECK_COND(cf.open(QIODevice::WriteOnly | QIODevice::Truncate), "78o: overwrite swapped CSV");
            cf.write("Y,X\n100,1\n200,2\n");
            cf.close();
        }
        CHECK_COND(tRe->reimportFromSource(), "78o: swapped reimportFromSource failed");
        QCoreApplication::processEvents();
        CHECK_COND(tRe->column(0)->name() == QStringLiteral("Y")
                           && tRe->column(1)->name() == QStringLiteral("X"),
                   "78o: swapped reimportFromSource collided column names!");
        CHECK_COND(tRe->column(0)->formula(0).isEmpty() && !tRe->column(0)->isMasked(0),
                   "78o: reimportFromSource retained stale formula or mask on reused column!");
    }

    // -------------------------------------------------------------------------
    // 78p: Zero-DOF / degenerate fit standard errors must be NaN (never 0.0)
    // -------------------------------------------------------------------------
    {
        // 1) MultiPeakFit (1 Gauss peak + constant baseline = 4 params, n = 4 points -> dof = 0)
        Table *tPk = w78->newTable(4, 2, "T78PeakNan", "PeakNan");
        CHECK_COND(tPk != nullptr, "78p: T78PeakNan failed");
        const double pkX[4] = { 0.0, 1.0, 2.0, 3.0 };
        const double pkY[4] = { 1.0, 5.0, 5.0, 1.0 };
        for (int r = 0; r < 4; ++r) {
            tPk->setCell(r, 0, pkX[r]);
            tPk->setCell(r, 1, pkY[r]);
        }
        MultiLayer *mlPk = w78->multilayerPlot(
                tPk, QStringList() << (QString(tPk->name()) + QStringLiteral("_2")), Graph::Scatter);
        CHECK_COND(mlPk != nullptr && mlPk->activeGraph() != nullptr, "78p: mlPk failed");
        MultiPeakFit mpf(w78, mlPk->activeGraph(), MultiPeakFit::Gauss, 1);
        mpf.setSilent(true);
        mpf.enablePeakCurves(false);
        CHECK_COND(mpf.setDataFromCurve(QString(tPk->name()) + QStringLiteral("_2")),
                   "78p: MultiPeakFit::setDataFromCurve failed");
        mpf.fit();
        const std::vector<MultiPeakFit::PeakSummaryRow> pkRows = mpf.computePeakSummary(mpf.results());
        CHECK_COND(pkRows.size() == 1, "78p: expected 1 peak summary row");
        CHECK_COND(std::isnan(pkRows[0].centerSE) && std::isnan(pkRows[0].fwhmSE)
                           && std::isnan(pkRows[0].heightSE) && std::isnan(pkRows[0].areaSE),
                   "78p: MultiPeakFit zero-DOF SE should be NaN, not 0.0");

        // 2) LinearFit::fitWithXYErrors (York) with 2 points (dof = 0)
        Table *tLin = w78->newTable(2, 2, "T78LinNan", "LinNan");
        CHECK_COND(tLin != nullptr, "78p: T78LinNan failed");
        tLin->setCell(0, 0, 1.0);
        tLin->setCell(0, 1, 2.0);
        tLin->setCell(1, 0, 3.0);
        tLin->setCell(1, 1, 6.0);
        MultiLayer *mlLin = w78->multilayerPlot(
                tLin, QStringList() << (QString(tLin->name()) + QStringLiteral("_2")), Graph::Scatter);
        CHECK_COND(mlLin != nullptr && mlLin->activeGraph() != nullptr, "78p: mlLin failed");
        LinearFit lf(w78, mlLin->activeGraph());
        lf.setSilent(true);
        CHECK_COND(lf.setDataFromCurve(QString(tLin->name()) + QStringLiteral("_2")),
                   "78p: LinearFit::setDataFromCurve failed");
        CHECK_COND(lf.fitWithXYErrors(), "78p: LinearFit::fitWithXYErrors failed on 2-point dataset");
        CHECK_COND(std::isnan(lf.yorkSlopeError()) && std::isnan(lf.yorkInterceptError()),
                   "78p: LinearFit zero-DOF York SE should be NaN, not 0.0");

        // 3) GraphStatsAndSurfaceFit zero-DOF Poly2DQuadratic (6 points, p=6) & degenerate Plane2D
        Table *tSurf = w78->newTable(6, 3, "T78SurfNan", "SurfNan");
        CHECK_COND(tSurf != nullptr, "78p: T78SurfNan failed");
        tSurf->setColName(0, QStringLiteral("X"));
        tSurf->setColName(1, QStringLiteral("Y"));
        tSurf->setColName(2, QStringLiteral("Z"));
        const double sx[6] = { 0.0, 1.0, 0.0, 1.0, 2.0, 0.0 };
        const double sy[6] = { 0.0, 0.0, 1.0, 1.0, 0.0, 2.0 };
        const double sz[6] = { 1.0, 2.0, 3.0, 5.0, 4.0, 6.0 };
        for (int r = 0; r < 6; ++r) {
            tSurf->setCell(r, 0, sx[r]);
            tSurf->setCell(r, 1, sy[r]);
            tSurf->setCell(r, 2, sz[r]);
        }
        const GraphStatsAndSurfaceFit::SurfaceFitResult sfZero = GraphStatsAndSurfaceFit::fitSurfaceXYZ(
                w78, tSurf, QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
                GraphStatsAndSurfaceFit::Poly2DQuadratic);
        CHECK_COND(sfZero.errors.size() == 6, "78p: Poly2DQuadratic errors size != 6");
        for (int j = 0; j < sfZero.errors.size(); ++j) {
            CHECK_COND(std::isnan(sfZero.errors[j]),
                       "78p: Poly2DQuadratic zero-DOF error[" << j << "] should be NaN, not "
                                                              << sfZero.errors[j]);
        }

        Table *tPlane = w78->newTable(5, 3, "T78PlaneDegen", "PlaneDegen");
        CHECK_COND(tPlane != nullptr, "78p: T78PlaneDegen failed");
        tPlane->setColName(0, QStringLiteral("X"));
        tPlane->setColName(1, QStringLiteral("Y"));
        tPlane->setColName(2, QStringLiteral("Z"));
        for (int r = 0; r < 5; ++r) {
            tPlane->setCell(r, 0, double(r));
            tPlane->setCell(r, 1, 1.0); // constant Y -> unidentifiable ay slope
            tPlane->setCell(r, 2, 1.0 + double(r));
        }
        const GraphStatsAndSurfaceFit::SurfaceFitResult sfDegen = GraphStatsAndSurfaceFit::fitSurfaceXYZ(
                w78, tPlane, QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
                GraphStatsAndSurfaceFit::Plane2D);
        CHECK_COND(sfDegen.errors.size() == 3 && std::isnan(sfDegen.errors[2]),
                   "78p: degenerate Plane2D ay error should be NaN, not 0.0");
    }

    // -------------------------------------------------------------------------
    // 78q: Updater empty-SHA block, RPC stop/restart token lifecycle, 600-DPI Legend columns
    // -------------------------------------------------------------------------
    {
        // 1) AutoUpdater::downloadAndVerifyBlocking rejects empty SHA-256 unless AllowUnverifiedInstall
        QSettings &st78 = ApplicationWindow::getSettings();
        const QString allowKey = QStringLiteral("AutoUpdate/AllowUnverifiedInstall");
        const QVariant prevAllow = st78.value(allowKey);
        st78.remove(allowKey);
        CHECK_COND(!AutoUpdater::allowUnverifiedInstall(),
                   "78q: allowUnverifiedInstall must default to false");

        QTcpServer mockDl;
        CHECK_COND(mockDl.listen(QHostAddress::LocalHost, 0), "78q: mockDl.listen failed");
        QObject::connect(&mockDl, &QTcpServer::newConnection, &mockDl, [&]() {
            while (QTcpSocket *sock = mockDl.nextPendingConnection()) {
                QObject::connect(sock, &QTcpSocket::readyRead, sock, [sock]() {
                    if (!sock->canReadLine())
                        return;
                    sock->readAll();
                    const QByteArray body("DUMMY_UPDATE_ARCHIVE_BYTES");
                    const QByteArray hdr = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                            + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n";
                    sock->write(hdr);
                    sock->write(body);
                    sock->flush();
                    sock->disconnectFromHost();
                });
                QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
            }
        });
        const QUrl emptyShaUrl(
                QStringLiteral("http://127.0.0.1:%1/ToriGraph-empty-sha.zip").arg(mockDl.serverPort()));
        const QString emptyShaDest = tmp78.filePath(QStringLiteral("empty_sha_78q.zip"));
        QString dlErr;
        const bool okEmptySha = AutoUpdater::downloadAndVerifyBlocking(
                emptyShaUrl, emptyShaDest, QString(), &dlErr, 5000);
        CHECK_COND(!okEmptySha && !dlErr.isEmpty() && !QFile::exists(emptyShaDest)
                           && !QFile::exists(emptyShaDest + QStringLiteral(".part")),
                   "78q: downloadAndVerifyBlocking accepted empty SHA-256 without allowUnverifiedInstall!");
        if (prevAllow.isValid())
            st78.setValue(allowKey, prevAllow);
        else
            st78.remove(allowKey);

        // 2) PythonAutomationServer::stop() clears tcpAuthToken(); restart generates fresh token; empty/stale rejected
        PythonAutomationServer srv78(w78);
        CHECK_COND(srv78.startTcp(0, QHostAddress::LocalHost), "78q: startTcp #1 failed");
        const QString tok1 = srv78.tcpAuthToken();
        CHECK_COND(!tok1.isEmpty(), "78q: startTcp #1 returned empty tcpAuthToken");
        srv78.stop();
        CHECK_COND(srv78.tcpAuthToken().isEmpty() && !srv78.isTcpListening(),
                   "78q: stop() failed to clear tcpAuthToken()");

        CHECK_COND(srv78.startTcp(0, QHostAddress::LocalHost), "78q: startTcp #2 failed");
        const quint16 port2 = srv78.tcpPort();
        const QString tok2 = srv78.tcpAuthToken();
        CHECK_COND(!tok2.isEmpty() && tok2 != tok1,
                   "78q: restarting TCP did not regenerate fresh tcpAuthToken()");

        auto tryTcpAuth = [&](const QString &candidateToken) -> QJsonObject {
            QTcpSocket sock;
            sock.connectToHost(QHostAddress::LocalHost, port2);
            QElapsedTimer tmr;
            tmr.start();
            while (sock.state() != QAbstractSocket::ConnectedState && tmr.elapsed() < 3000)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 15);
            if (sock.state() != QAbstractSocket::ConnectedState)
                return QJsonObject();
            QJsonObject req;
            req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            req[QStringLiteral("id")] = 7810;
            req[QStringLiteral("method")] = QStringLiteral("rpc.auth");
            req[QStringLiteral("params")] = QJsonObject{ { QStringLiteral("token"), candidateToken } };
            sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
            sock.flush();
            tmr.restart();
            while (!sock.canReadLine() && tmr.elapsed() < 3000)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 15);
            if (!sock.canReadLine())
                return QJsonObject();
            return QJsonDocument::fromJson(sock.readLine().trimmed()).object();
        };

        const QJsonObject emptyTokResp = tryTcpAuth(QString());
        CHECK_COND(emptyTokResp.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt() == -32001,
                   "78q: empty rpc.auth token was not rejected with -32001");
        const QJsonObject staleTokResp = tryTcpAuth(tok1);
        CHECK_COND(staleTokResp.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt() == -32001,
                   "78q: stale rpc.auth token from before stop() was not rejected with -32001");
        const QJsonObject freshTokResp = tryTcpAuth(tok2);
        CHECK_COND(freshTokResp.value(QStringLiteral("result")).toObject().value(QStringLiteral("authenticated")).toBool(),
                   "78q: fresh rpc.auth token failed after TCP restart");
        srv78.stop();
        CHECK_COND(srv78.tcpAuthToken().isEmpty(), "78q: second stop() did not clear tcpAuthToken");

        // 3) Legend::draw / computeCellsLayout under non-identity 600-DPI QwtMetricsMap (2-column layout)
        Table *tLeg = w78->newTable(3, 3, "T78Legend600", "Legend600");
        CHECK_COND(tLeg != nullptr, "78q: T78Legend600 creation failed");
        tLeg->setColName(1, QStringLiteral("AlphaSeries"));
        tLeg->setColName(2, QStringLiteral("BetaSeries"));
        for (int r = 0; r < 3; ++r) {
            tLeg->setCell(r, 0, double(r));
            tLeg->setCell(r, 1, double(r + 1));
            tLeg->setCell(r, 2, double(r + 2));
        }
        MultiLayer *mlLeg = w78->multilayerPlot(
                tLeg,
                QStringList() << (QString(tLeg->name()) + QStringLiteral("_AlphaSeries"))
                              << (QString(tLeg->name()) + QStringLiteral("_BetaSeries")),
                Graph::Line);
        CHECK_COND(mlLeg != nullptr && mlLeg->activeGraph() != nullptr, "78q: mlLeg failed");
        Graph *gLeg = mlLeg->activeGraph();
        CHECK_COND(gLeg->curves() == 2, "78q: expected 2 curves for Legend test");
        gLeg->curve(0)->setPen(QPen(Qt::red, 3.0));
        gLeg->curve(1)->setPen(QPen(Qt::blue, 3.0));
        Legend *leg = gLeg->legend() ? gLeg->legend() : gLeg->newLegend();
        CHECK_COND(leg != nullptr, "78q: Legend creation failed");
        leg->setText(QStringLiteral("\\c{1}AlphaSeries\n\\c{2}BetaSeries"));
        leg->setNumColumns(2);
        leg->setFrameStyle(Legend::None);
        leg->setTextColor(Qt::black);
        leg->setBackgroundColor(Qt::white);
        leg->setOriginCoord(0.5, 8.0);

        QImage dev600(3200, 800, QImage::Format_ARGB32);
        const int dpm600 = qRound(600.0 / 0.0254);
        dev600.setDotsPerMeterX(dpm600);
        dev600.setDotsPerMeterY(dpm600);
        dev600.fill(Qt::white);

        QwtMetricsMap mm600;
        mm600.setMetrics(&dev600, &dev600);
        CHECK_COND(mm600.isIdentity() && mm600.screenToLayoutX(100) > 400,
                   "78q: 600-DPI equal-device QwtMetricsMap scale check failed");

        QwtScaleMap xMap600, yMap600;
        xMap600.setScaleInterval(0.0, 10.0);
        xMap600.setPaintInterval(0.0, double(dev600.width()));
        yMap600.setScaleInterval(0.0, 10.0);
        yMap600.setPaintInterval(double(dev600.height()), 0.0);
        {
            QPainter p600(&dev600);
            leg->draw(&p600, xMap600, yMap600, dev600.rect());
        }
        CHECK_COND(QwtPainter::metricsMap().isIdentity(),
                   "78q: Legend::draw leaked non-identity QwtMetricsMap after returning");

        int minRedX = dev600.width(), maxRedX = -1;
        int minBlueX = dev600.width(), maxBlueX = -1;
        for (int y = 0; y < dev600.height(); ++y) {
            for (int x = 0; x < dev600.width(); ++x) {
                const QRgb px = dev600.pixel(x, y);
                const int r = qRed(px), g = qGreen(px), b = qBlue(px);
                if (r > 180 && g < 80 && b < 80) {
                    minRedX = std::min(minRedX, x);
                    maxRedX = std::max(maxRedX, x);
                } else if (b > 180 && r < 80 && g < 80) {
                    minBlueX = std::min(minBlueX, x);
                    maxBlueX = std::max(maxBlueX, x);
                }
            }
        }
        CHECK_COND(maxRedX > minRedX && minBlueX > maxRedX && maxBlueX > minBlueX,
                   "78q: 600-DPI 2-column Legend symbols overlapped or failed to render (red=["
                           << minRedX << "," << maxRedX << "] blue=[" << minBlueX << "," << maxBlueX << "])");

        int minDark0X = dev600.width(), maxDark0X = -1;
        int minDark1X = dev600.width(), maxDark1X = -1;
        int darkOverlapSwatch = 0;
        for (int y = 0; y < dev600.height(); ++y) {
            for (int x = 0; x < dev600.width(); ++x) {
                const QRgb px = dev600.pixel(x, y);
                const int r = qRed(px), g = qGreen(px), b = qBlue(px);
                if (std::max({ r, g, b }) < 110 && (std::max({ r, g, b }) - std::min({ r, g, b })) < 40) {
                    if ((x >= minRedX && x <= maxRedX) || (x >= minBlueX && x <= maxBlueX))
                        ++darkOverlapSwatch;
                    else if (x > maxRedX && x < minBlueX) {
                        minDark0X = std::min(minDark0X, x);
                        maxDark0X = std::max(maxDark0X, x);
                    } else if (x > maxBlueX) {
                        minDark1X = std::min(minDark1X, x);
                        maxDark1X = std::max(maxDark1X, x);
                    }
                }
            }
        }
        CHECK_COND(darkOverlapSwatch == 0 && maxDark0X > minDark0X && maxDark1X > minDark1X,
                   "78q: 600-DPI 2-column Legend text overlapped swatches/columns (overlap="
                           << darkOverlapSwatch << " col0=[" << minDark0X << "," << maxDark0X
                           << "] col1=[" << minDark1X << "," << maxDark1X << "])");
    }

    w78->switchToLanguage("en");
    QCoreApplication::processEvents();
    w78.reset();
    std::cout << "[PASS] Suite 78 passed.\n";
    return 0;
}

