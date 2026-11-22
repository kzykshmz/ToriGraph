/***************************************************************************
    File                 : PptxExport.cpp
    Project              : ToriGraph / SciDAVis
    Description          : "Send to PowerPoint" (E8). See PptxExport.h.

    The package layout follows python/torigraph/pptx.py (export_graphs_to_pptx):
    every graph becomes a <p:graphicFrame> holding a <p:oleObj progId="ToriGraph.Graph.1">
    whose OLE2 compound file carries the graph (and the tables it plots) and whose
    <p:pic> shows the vector EMF preview. The shapes are named "ToriGraphOLE:<graph>"
    with the description "torigraph:<graph>", so the SDK's replace=True finds them.
 ***************************************************************************/

#include "PptxExport.h"

#include "ApplicationWindow.h"
#include "EmfPaintDevice.h"
#include "Graph.h"
#include "Graph3D.h"
#include "GraphInspectorDock.h"
#include "I18nJapanese.h"
#include "MultiLayer.h"
#include "MyWidget.h"
#include "OleServer.h"
#include "ZipReader.h"
#include "ZipWriter.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QUndoStack>
#include <QTemporaryFile>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <objbase.h>
#endif

namespace ToriGraphPptx {

namespace {

QString T(const char *s)
{
    registerTranslations();
    return QCoreApplication::translate("ToriGraphPptx", s);
}

const QString kNsP = QStringLiteral("http://schemas.openxmlformats.org/presentationml/2006/main");
const QString kNsA = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/main");
const QString kNsR =
        QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
const QString kRelBase =
        QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/");
const QString kCtOle = QStringLiteral("application/vnd.openxmlformats-officedocument.oleObject");
const QString kCtSlide =
        QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.slide+xml");
const QString kProgId = QStringLiteral("ToriGraph.Graph.1");
const QString kShapePrefix = QStringLiteral("ToriGraphOLE:");
const QString kAltPrefix = QStringLiteral("torigraph:");
const char kXmlDecl[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n";

QString xmlEsc(const QString &s)
{
    QString o;
    o.reserve(s.size() + 8);
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '&': o += QStringLiteral("&amp;"); break;
        case '<': o += QStringLiteral("&lt;"); break;
        case '>': o += QStringLiteral("&gt;"); break;
        case '"': o += QStringLiteral("&quot;"); break;
        default:
            // characters XML 1.0 does not allow are dropped
            if (c.unicode() < 0x20 && c != QLatin1Char('\t') && c != QLatin1Char('\n')
                && c != QLatin1Char('\r'))
                break;
            o += c;
        }
    }
    return o;
}

QByteArray xmlDoc(const QString &body)
{
    return QByteArray(kXmlDecl) + body.toUtf8();
}

void gridDims(int perSlide, int *rows, int *cols)
{
    switch (perSlide) {
    case 2: *rows = 1; *cols = 2; break;
    case 3: *rows = 1; *cols = 3; break;
    case 4: *rows = 2; *cols = 2; break;
    case 6: *rows = 2; *cols = 3; break;
    default: *rows = 1; *cols = 1; break;
    }
}

int normalizedPerSlide(int perSlide)
{
    return (perSlide == 2 || perSlide == 3 || perSlide == 4 || perSlide == 6) ? perSlide : 1;
}

qint64 cmToEmu(double cm)
{
    return static_cast<qint64>(std::llround(cm * double(kEmuPerCm)));
}

// ---------------------------------------------------------------------------
// Package (all parts of a .pptx, in archive order)
// ---------------------------------------------------------------------------
struct Package
{
    QVector<QPair<QString, QByteArray>> parts;
    QHash<QString, int> index; // lower-case name -> position

    bool has(const QString &name) const { return index.contains(name.toLower()); }
    QByteArray get(const QString &name) const
    {
        const int i = index.value(name.toLower(), -1);
        return i >= 0 ? parts.at(i).second : QByteArray();
    }
    void set(const QString &name, const QByteArray &data)
    {
        const int i = index.value(name.toLower(), -1);
        if (i >= 0) {
            parts[i].second = data;
        } else {
            index.insert(name.toLower(), parts.size());
            parts.append(qMakePair(name, data));
        }
    }
};

struct Rel
{
    QString id, type, target, mode;
};

QVector<Rel> parseRels(const QByteArray &xml, bool *ok = nullptr)
{
    QVector<Rel> out;
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QLatin1String("Relationship")) {
            Rel rel;
            const QXmlStreamAttributes a = r.attributes();
            rel.id = a.value(QLatin1String("Id")).toString();
            rel.type = a.value(QLatin1String("Type")).toString();
            rel.target = a.value(QLatin1String("Target")).toString();
            rel.mode = a.value(QLatin1String("TargetMode")).toString();
            out.append(rel);
        }
    }
    if (ok)
        *ok = !r.hasError();
    return out;
}

QString partDir(const QString &part)
{
    const int i = part.lastIndexOf(QLatin1Char('/'));
    return i < 0 ? QString() : part.left(i);
}

QString relsPathFor(const QString &part)
{
    const int i = part.lastIndexOf(QLatin1Char('/'));
    if (i < 0)
        return QStringLiteral("_rels/") + part + QStringLiteral(".rels");
    return part.left(i) + QStringLiteral("/_rels/") + part.mid(i + 1) + QStringLiteral(".rels");
}

QString resolveTarget(const QString &baseDir, const QString &target)
{
    QString t = target;
    t.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QStringList segs;
    if (!t.startsWith(QLatin1Char('/')) && !baseDir.isEmpty())
        segs = baseDir.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &s : t.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (s == QLatin1String("."))
            continue;
        if (s == QLatin1String("..")) {
            if (!segs.isEmpty())
                segs.removeLast();
            continue;
        }
        segs << s;
    }
    return segs.join(QLatin1Char('/'));
}

QString relativeTarget(const QString &fromDir, const QString &toPart)
{
    const QStringList a = fromDir.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QStringList b = toPart.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    int common = 0;
    while (common < a.size() && common < b.size() - 1
           && a.at(common).compare(b.at(common), Qt::CaseInsensitive) == 0)
        ++common;
    QStringList out;
    for (int i = common; i < a.size(); ++i)
        out << QStringLiteral("..");
    for (int i = common; i < b.size(); ++i)
        out << b.at(i);
    return out.join(QLatin1Char('/'));
}

bool relTypeIs(const Rel &r, const char *suffix)
{
    return r.type.endsWith(QLatin1Char('/') + QLatin1String(suffix));
}

QString uniquePartName(const Package &pkg, const QString &pattern, int *counter)
{
    for (;;) {
        const QString n = pattern.arg(*counter);
        ++*counter;
        if (!pkg.has(n))
            return n;
    }
}

// ---------------------------------------------------------------------------
// Template of a new presentation (one blank layout, Arial theme)
// ---------------------------------------------------------------------------
QString emptySpTree()
{
    return QStringLiteral("<p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/>"
                          "<p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/>"
                          "<a:ext cx=\"0\" cy=\"0\"/><a:chOff x=\"0\" y=\"0\"/>"
                          "<a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>");
}

QString pmlRootAttrs()
{
    return QStringLiteral(" xmlns:a=\"%1\" xmlns:r=\"%2\" xmlns:p=\"%3\"").arg(kNsA, kNsR, kNsP);
}

QByteArray themeXml()
{
    QString s = QStringLiteral("<a:theme xmlns:a=\"%1\" name=\"ToriGraph\"><a:themeElements>"
                               "<a:clrScheme name=\"Office\">").arg(kNsA);
    s += QStringLiteral(
            "<a:dk1><a:sysClr val=\"windowText\" lastClr=\"000000\"/></a:dk1>"
            "<a:lt1><a:sysClr val=\"window\" lastClr=\"FFFFFF\"/></a:lt1>"
            "<a:dk2><a:srgbClr val=\"44546A\"/></a:dk2><a:lt2><a:srgbClr val=\"E7E6E6\"/></a:lt2>"
            "<a:accent1><a:srgbClr val=\"4472C4\"/></a:accent1>"
            "<a:accent2><a:srgbClr val=\"ED7D31\"/></a:accent2>"
            "<a:accent3><a:srgbClr val=\"A5A5A5\"/></a:accent3>"
            "<a:accent4><a:srgbClr val=\"FFC000\"/></a:accent4>"
            "<a:accent5><a:srgbClr val=\"5B9BD5\"/></a:accent5>"
            "<a:accent6><a:srgbClr val=\"70AD47\"/></a:accent6>"
            "<a:hlink><a:srgbClr val=\"0563C1\"/></a:hlink>"
            "<a:folHlink><a:srgbClr val=\"954F72\"/></a:folHlink></a:clrScheme>");
    s += QStringLiteral("<a:fontScheme name=\"ToriGraph\">"
                        "<a:majorFont><a:latin typeface=\"Arial\"/><a:ea typeface=\"\"/>"
                        "<a:cs typeface=\"\"/></a:majorFont>"
                        "<a:minorFont><a:latin typeface=\"Arial\"/><a:ea typeface=\"\"/>"
                        "<a:cs typeface=\"\"/></a:minorFont></a:fontScheme>");
    const QString solid = QStringLiteral("<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>");
    s += QStringLiteral("<a:fmtScheme name=\"Office\"><a:fillStyleLst>") + solid + solid + solid
            + QStringLiteral("</a:fillStyleLst><a:lnStyleLst>");
    for (const char *w : { "6350", "12700", "19050" })
        s += QStringLiteral("<a:ln w=\"%1\">").arg(QLatin1String(w)) + solid
                + QStringLiteral("</a:ln>");
    s += QStringLiteral("</a:lnStyleLst><a:effectStyleLst>");
    for (int i = 0; i < 3; ++i)
        s += QStringLiteral("<a:effectStyle><a:effectLst/></a:effectStyle>");
    s += QStringLiteral("</a:effectStyleLst><a:bgFillStyleLst>") + solid + solid + solid
            + QStringLiteral("</a:bgFillStyleLst></a:fmtScheme></a:themeElements>"
                             "<a:objectDefaults/><a:extraClrSchemeLst/></a:theme>");
    return xmlDoc(s);
}

QByteArray relsXml(const QVector<Rel> &rels)
{
    QString s = QStringLiteral(
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
    for (const Rel &r : rels) {
        s += QStringLiteral("<Relationship Id=\"%1\" Type=\"%2\" Target=\"%3\"")
                     .arg(xmlEsc(r.id), xmlEsc(r.type), xmlEsc(r.target));
        if (!r.mode.isEmpty())
            s += QStringLiteral(" TargetMode=\"%1\"").arg(xmlEsc(r.mode));
        s += QStringLiteral("/>");
    }
    s += QStringLiteral("</Relationships>");
    return xmlDoc(s);
}

Package newPresentationTemplate(qint64 slideW, qint64 slideH)
{
    Package pkg;
    const QString ctPml = QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.");
    QString ct = QStringLiteral(
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" "
            "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"xml\" ContentType=\"application/xml\"/>");
    ct += QStringLiteral("<Override PartName=\"/ppt/presentation.xml\" ContentType=\"%1presentation.main+xml\"/>")
                  .arg(ctPml);
    ct += QStringLiteral("<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" ContentType=\"%1slideMaster+xml\"/>")
                  .arg(ctPml);
    ct += QStringLiteral("<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" ContentType=\"%1slideLayout+xml\"/>")
                  .arg(ctPml);
    ct += QStringLiteral("<Override PartName=\"/ppt/presProps.xml\" ContentType=\"%1presProps+xml\"/>")
                  .arg(ctPml);
    ct += QStringLiteral("<Override PartName=\"/ppt/viewProps.xml\" ContentType=\"%1viewProps+xml\"/>")
                  .arg(ctPml);
    ct += QStringLiteral("<Override PartName=\"/ppt/tableStyles.xml\" ContentType=\"%1tableStyles+xml\"/>")
                  .arg(ctPml);
    ct += QStringLiteral(
            "<Override PartName=\"/ppt/theme/theme1.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>"
            "<Override PartName=\"/docProps/core.xml\" "
            "ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
            "<Override PartName=\"/docProps/app.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
            "</Types>");
    pkg.set(QStringLiteral("[Content_Types].xml"), xmlDoc(ct));

    pkg.set(QStringLiteral("_rels/.rels"),
            relsXml({ { QStringLiteral("rId1"), kRelBase + QStringLiteral("officeDocument"),
                        QStringLiteral("ppt/presentation.xml"), QString() },
                      { QStringLiteral("rId2"),
                        QStringLiteral("http://schemas.openxmlformats.org/package/2006/"
                                       "relationships/metadata/core-properties"),
                        QStringLiteral("docProps/core.xml"), QString() },
                      { QStringLiteral("rId3"), kRelBase + QStringLiteral("extended-properties"),
                        QStringLiteral("docProps/app.xml"), QString() } }));

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    pkg.set(QStringLiteral("docProps/core.xml"),
            xmlDoc(QStringLiteral(
                           "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/"
                           "2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                           "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
                           "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
                           "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
                           "<dc:title>ToriGraph</dc:title><dc:creator>ToriGraph</dc:creator>"
                           "<dcterms:created xsi:type=\"dcterms:W3CDTF\">%1</dcterms:created>"
                           "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">%1</dcterms:modified>"
                           "</cp:coreProperties>")
                           .arg(now)));
    pkg.set(QStringLiteral("docProps/app.xml"),
            xmlDoc(QStringLiteral("<Properties xmlns=\"http://schemas.openxmlformats.org/"
                                  "officeDocument/2006/extended-properties\" "
                                  "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/"
                                  "2006/docPropsVTypes\"><Application>ToriGraph</Application>"
                                  "</Properties>")));

    // The slide list is added by appendSlides() (inserted in front of <p:sldSz>).
    pkg.set(QStringLiteral("ppt/presentation.xml"),
            xmlDoc(QStringLiteral("<p:presentation%1 saveSubsetFonts=\"1\">"
                                  "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/>"
                                  "</p:sldMasterIdLst><p:sldSz cx=\"%2\" cy=\"%3\"/>"
                                  "<p:notesSz cx=\"6858000\" cy=\"9144000\"/></p:presentation>")
                           .arg(pmlRootAttrs())
                           .arg(slideW)
                           .arg(slideH)));
    pkg.set(QStringLiteral("ppt/_rels/presentation.xml.rels"),
            relsXml({ { QStringLiteral("rId1"), kRelBase + QStringLiteral("slideMaster"),
                        QStringLiteral("slideMasters/slideMaster1.xml"), QString() },
                      { QStringLiteral("rId2"), kRelBase + QStringLiteral("presProps"),
                        QStringLiteral("presProps.xml"), QString() },
                      { QStringLiteral("rId3"), kRelBase + QStringLiteral("viewProps"),
                        QStringLiteral("viewProps.xml"), QString() },
                      { QStringLiteral("rId4"), kRelBase + QStringLiteral("theme"),
                        QStringLiteral("theme/theme1.xml"), QString() },
                      { QStringLiteral("rId5"), kRelBase + QStringLiteral("tableStyles"),
                        QStringLiteral("tableStyles.xml"), QString() } }));

    pkg.set(QStringLiteral("ppt/slideMasters/slideMaster1.xml"),
            xmlDoc(QStringLiteral("<p:sldMaster%1><p:cSld><p:bg><p:bgRef idx=\"1001\">"
                                  "<a:schemeClr val=\"bg1\"/></p:bgRef></p:bg>")
                           .arg(pmlRootAttrs())
                   + emptySpTree()
                   + QStringLiteral(
                           "</p:spTree></p:cSld><p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" "
                           "tx2=\"dk2\" accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" "
                           "accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" "
                           "hlink=\"hlink\" folHlink=\"folHlink\"/><p:sldLayoutIdLst>"
                           "<p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/></p:sldLayoutIdLst>"
                           "<p:txStyles><p:titleStyle><a:lvl1pPr><a:defRPr sz=\"3200\">"
                           "<a:latin typeface=\"+mj-lt\"/><a:ea typeface=\"+mj-ea\"/></a:defRPr>"
                           "</a:lvl1pPr></p:titleStyle><p:bodyStyle><a:lvl1pPr>"
                           "<a:defRPr sz=\"2000\"/></a:lvl1pPr></p:bodyStyle><p:otherStyle>"
                           "<a:lvl1pPr><a:defRPr sz=\"1800\"/></a:lvl1pPr></p:otherStyle>"
                           "</p:txStyles></p:sldMaster>")));
    pkg.set(QStringLiteral("ppt/slideMasters/_rels/slideMaster1.xml.rels"),
            relsXml({ { QStringLiteral("rId1"), kRelBase + QStringLiteral("slideLayout"),
                        QStringLiteral("../slideLayouts/slideLayout1.xml"), QString() },
                      { QStringLiteral("rId2"), kRelBase + QStringLiteral("theme"),
                        QStringLiteral("../theme/theme1.xml"), QString() } }));
    pkg.set(QStringLiteral("ppt/slideLayouts/slideLayout1.xml"),
            xmlDoc(QStringLiteral("<p:sldLayout%1 type=\"blank\" preserve=\"1\">"
                                  "<p:cSld name=\"Blank\">")
                           .arg(pmlRootAttrs())
                   + emptySpTree()
                   + QStringLiteral("</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/>"
                                    "</p:clrMapOvr></p:sldLayout>")));
    pkg.set(QStringLiteral("ppt/slideLayouts/_rels/slideLayout1.xml.rels"),
            relsXml({ { QStringLiteral("rId1"), kRelBase + QStringLiteral("slideMaster"),
                        QStringLiteral("../slideMasters/slideMaster1.xml"), QString() } }));
    pkg.set(QStringLiteral("ppt/theme/theme1.xml"), themeXml());
    pkg.set(QStringLiteral("ppt/presProps.xml"),
            xmlDoc(QStringLiteral("<p:presentationPr%1/>").arg(pmlRootAttrs())));
    pkg.set(QStringLiteral("ppt/viewProps.xml"),
            xmlDoc(QStringLiteral("<p:viewPr%1><p:gridSpacing cx=\"72008\" cy=\"72008\"/>"
                                  "</p:viewPr>")
                           .arg(pmlRootAttrs())));
    pkg.set(QStringLiteral("ppt/tableStyles.xml"),
            xmlDoc(QStringLiteral("<a:tblStyleLst xmlns:a=\"%1\" "
                                  "def=\"{5C22544A-7EE6-4342-B048-85BDC9FD1C3A}\"/>")
                           .arg(kNsA)));
    return pkg;
}

// ---------------------------------------------------------------------------
// Slides
// ---------------------------------------------------------------------------
struct ShapeOnSlide
{
    int itemIndex = -1;
    qint64 x = 0, y = 0, w = 0, h = 0;
    QString oleRid, imageRid;
};

QString graphicFrameXml(const SlideItem &item, const ShapeOnSlide &s, int shapeId)
{
    // Every template is filled by a single multi-argument arg(), so a graph name that
    // contains "%1" etc. is inserted verbatim.
    const QString name = xmlEsc(kShapePrefix + item.graphName);
    const QString alt = xmlEsc(kAltPrefix + item.graphName);
    const QString xfrm = QStringLiteral("<a:off x=\"%1\" y=\"%2\"/><a:ext cx=\"%3\" cy=\"%4\"/>")
                                 .arg(QString::number(s.x), QString::number(s.y),
                                      QString::number(s.w), QString::number(s.h));
    const bool ole = !s.oleRid.isEmpty();
    const QString pic =
            QStringLiteral("<p:pic><p:nvPicPr><p:cNvPr id=\"%1\" name=\"%2\" descr=\"%3\"/>"
                           "<p:cNvPicPr><a:picLocks noChangeAspect=\"1\"/></p:cNvPicPr><p:nvPr/>"
                           "</p:nvPicPr><p:blipFill><a:blip r:embed=\"%4\"/><a:stretch>"
                           "<a:fillRect/></a:stretch></p:blipFill><p:spPr><a:xfrm>%5</a:xfrm>"
                           "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr></p:pic>")
                    .arg(QString::number(ole ? shapeId + 1 : shapeId),
                         ole ? name + QStringLiteral("_Preview") : name, alt, s.imageRid, xfrm);
    if (!ole)
        return pic; // picture only (no OLE object could be made)
    return QStringLiteral("<p:graphicFrame><p:nvGraphicFramePr><p:cNvPr id=\"%1\" name=\"%2\" "
                          "descr=\"%3\"/><p:cNvGraphicFramePr><a:graphicFrameLocks "
                          "noChangeAspect=\"1\"/></p:cNvGraphicFramePr><p:nvPr/>"
                          "</p:nvGraphicFramePr><p:xfrm>%4</p:xfrm><a:graphic>"
                          "<a:graphicData uri=\"http://schemas.openxmlformats.org/presentationml/"
                          "2006/ole\"><p:oleObj name=\"ToriGraph Graph\" r:id=\"%5\" imgW=\"%6\" "
                          "imgH=\"%7\" progId=\"%8\"><p:embed/>%9</p:oleObj></a:graphicData>"
                          "</a:graphic></p:graphicFrame>")
            .arg(QString::number(shapeId), name, alt, xfrm, s.oleRid, QString::number(s.w),
                 QString::number(s.h), kProgId, pic);
}

Placement placeInCell(const SlideItem &item, qint64 cl, qint64 ct, qint64 cw, qint64 ch,
                      const DeckOptions &opt)
{
    Placement p;
    p.graphName = item.graphName;
    const qint64 natW = (item.widthHm > 0 ? item.widthHm : 8500) * kEmuPerHimetric;
    const qint64 natH = (item.heightHm > 0 ? item.heightHm : 8500) * kEmuPerHimetric;
    double w = 0.0, h = 0.0;
    if (opt.scalePercent > 0.0) {
        const double s = opt.scalePercent / 100.0;
        w = double(natW) * s;
        h = double(natH) * s;
        if (opt.shrinkToFit && (w > double(cw) + 0.5 || h > double(ch) + 0.5)) {
            const double f = std::min(double(cw) / w, double(ch) / h);
            w *= f;
            h *= f;
            p.shrunk = true;
        }
    } else {
        // fit the cell, keeping the aspect ratio (the SDK's default without scale=)
        const double ratio = double(natW) / double(natH);
        if (ratio >= double(cw) / double(ch)) {
            w = double(cw);
            h = double(cw) / ratio;
        } else {
            h = double(ch);
            w = double(ch) * ratio;
        }
    }
    p.width = std::max<qint64>(1, std::llround(w));
    p.height = std::max<qint64>(1, std::llround(h));
    p.left = cl + std::max<qint64>(0, (cw - p.width) / 2);
    p.top = ct + std::max<qint64>(0, (ch - p.height) / 2);
    p.effectivePercent = 100.0 * double(p.width) / double(natW);
    return p;
}

const QString kNsP14 = QStringLiteral("http://schemas.microsoft.com/office/powerpoint/2010/main");

// The slide list of presentation.xml: slides of <p:sldIdLst> only (the p14 section lists
// repeat the same ids and are not counted).
struct PresentationInfo
{
    bool ok = false;
    bool strict = false; //!< Strict Open XML (http://purl.oclc.org/ooxml/...)
    int slideCount = 0;
    quint32 maxId = 255;
    QSet<quint32> usedIds;
};

PresentationInfo scanPresentation(const QByteArray &presXml)
{
    PresentationInfo info;
    QXmlStreamReader r(presXml);
    QStringList path; // local names of the open elements
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const QString n = r.name().toString();
            if (path.isEmpty() && r.namespaceUri().startsWith(QLatin1String("http://purl.oclc.org/ooxml/")))
                info.strict = true;
            if (n == QLatin1String("sldId")) {
                const quint32 id = r.attributes().value(QLatin1String("id")).toUInt();
                info.maxId = std::max(info.maxId, id);
                info.usedIds.insert(id);
                if (r.namespaceUri() == kNsP && !path.isEmpty()
                    && path.last() == QLatin1String("sldIdLst"))
                    ++info.slideCount;
            }
            path << n;
        } else if (r.isEndElement() && !path.isEmpty()) {
            path.removeLast();
        }
    }
    info.ok = !r.hasError();
    return info;
}

// Ids for new slides: after the largest id, or (near the ST_SlideId limit 2147483647) the
// lowest unused ids from 256. Returns false when there is no room.
bool allocateSlideIds(const PresentationInfo &info, int count, QVector<quint32> *out)
{
    constexpr quint64 kMaxSlideId = 2147483647ULL;
    out->clear();
    if (quint64(info.maxId) + quint64(count) <= kMaxSlideId) {
        for (int i = 0; i < count; ++i)
            out->append(info.maxId + 1 + quint32(i));
        return true;
    }
    for (quint64 id = 256; id <= kMaxSlideId && out->size() < count; ++id) {
        if (!info.usedIds.contains(quint32(id)))
            out->append(quint32(id));
    }
    return out->size() == count;
}

// Inserts the new <p:sldId> entries into presentation.xml, and into the last section of a
// presentation that uses sections (PowerPoint 2010+), so that every slide is in a section.
// The document is walked with an XML reader; the edits are made at the character offsets of
// the elements it reports, so the rest of the file is kept byte for byte.
bool insertSlideIds(QByteArray &presXml, const QVector<QPair<quint32, QString>> &ids,
                    QString *err)
{
    const QString s = QString::fromUtf8(presXml);
    struct Tag
    {
        int start = -1; //!< '<' of the start tag
        int startEnd = -1; //!< after the start tag
        int endStart = -1; //!< '<' of the end tag (== startEnd for an empty element)
        bool empty = false;
        QString prefix;
    };
    Tag root, mainList, sldSz, notesSz, lastSection, sectionList, sectionExt;
    QString rootQName;
    QXmlStreamReader r(s);
    QVector<QPair<QString, QString>> stack; // (namespace, local name)
    QVector<Tag *> open; // Tag objects of the open elements (or nullptr)
    qint64 before = 0;
    while (!r.atEnd()) {
        r.readNext();
        // whitespace before an element is not always reported as its own token (prolog)
        const int tokStart = s.indexOf(QLatin1Char('<'), int(before));
        const int tokEnd = int(r.characterOffset());
        before = r.characterOffset();
        if (r.isStartElement()) {
            const QString ns = r.namespaceUri().toString();
            const QString n = r.name().toString();
            Tag t;
            t.start = tokStart;
            t.startEnd = tokEnd;
            t.empty = s.midRef(tokStart, tokEnd - tokStart).trimmed().endsWith(QLatin1String("/>"));
            t.prefix = r.prefix().toString();
            Tag *target = nullptr;
            const int depth = stack.size();
            const QString parentNs = depth ? stack.last().first : QString();
            const QString parent = depth ? stack.last().second : QString();
            if (depth == 0) {
                root = t;
                rootQName = r.qualifiedName().toString();
                target = &root;
            } else if (depth == 1 && ns == kNsP) {
                if (n == QLatin1String("sldIdLst"))
                    target = &mainList;
                else if (n == QLatin1String("sldSz") && sldSz.start < 0)
                    target = &sldSz;
                else if (n == QLatin1String("notesSz") && notesSz.start < 0)
                    target = &notesSz;
            } else if (ns == kNsP14 && n == QLatin1String("section")) {
                lastSection = t;
                sectionList = Tag();
                sectionExt = Tag();
                target = &lastSection;
            } else if (ns == kNsP14 && parentNs == kNsP14 && parent == QLatin1String("section")) {
                if (n == QLatin1String("sldIdLst"))
                    target = &sectionList;
                else if (n == QLatin1String("extLst") && sectionExt.start < 0)
                    target = &sectionExt;
            }
            if (target)
                *target = t;
            stack.append(qMakePair(ns, n));
            open.append(target);
        } else if (r.isEndElement() && !stack.isEmpty()) {
            if (Tag *t = open.last())
                t->endStart = t->empty ? t->startEnd : tokStart;
            stack.removeLast();
            open.removeLast();
        }
    }
    if (r.hasError() || root.start < 0) {
        *err = T("The presentation part of the existing file could not be read.");
        return false;
    }

    struct Edit
    {
        int pos, removeLen;
        QString text;
    };
    QVector<Edit> edits;

    // r: prefix declared on the root element only (a declaration further down does not reach
    // the slide list)
    const QString rootTag = s.mid(root.start, root.startEnd - root.start);
    const QRegularExpression rNsRx(QStringLiteral("xmlns:([A-Za-z_][\\w.-]*)\\s*=\\s*([\"'])%1\\2")
                                           .arg(QRegularExpression::escape(kNsR)));
    const QRegularExpressionMatch nm = rNsRx.match(rootTag);
    QString rPrefix;
    if (nm.hasMatch()) {
        rPrefix = nm.captured(1);
    } else {
        rPrefix = QStringLiteral("tgr");
        edits.append({ root.start + 1 + rootQName.size(), 0,
                       QStringLiteral(" xmlns:tgr=\"%1\"").arg(kNsR) });
    }

    auto qn = [](const QString &prefix, const char *local) {
        return prefix.isEmpty() ? QString::fromLatin1(local)
                                : prefix + QLatin1Char(':') + QLatin1String(local);
    };
    const QString pp = mainList.start >= 0 ? mainList.prefix : root.prefix;
    QString entries;
    for (const auto &id : ids)
        entries += QStringLiteral("<%1 id=\"%2\" %3:id=\"%4\"/>")
                           .arg(qn(pp, "sldId"), QString::number(id.first), rPrefix, id.second);
    if (mainList.start >= 0) {
        if (mainList.empty)
            edits.append({ mainList.start, mainList.startEnd - mainList.start,
                           QStringLiteral("<%1>%2</%1>").arg(qn(pp, "sldIdLst"), entries) });
        else
            edits.append({ mainList.endStart, 0, entries });
    } else {
        const int at = sldSz.start >= 0 ? sldSz.start : notesSz.start;
        if (at < 0) {
            *err = T("The presentation part of the existing file could not be read.");
            return false;
        }
        edits.append({ at, 0, QStringLiteral("<%1>%2</%1>").arg(qn(pp, "sldIdLst"), entries) });
    }

    if (lastSection.start >= 0) {
        const QString sp = lastSection.prefix;
        QString secEntries;
        for (const auto &id : ids)
            secEntries += QStringLiteral("<%1 id=\"%2\"/>").arg(qn(sp, "sldId"),
                                                               QString::number(id.first));
        const QString newList = QStringLiteral("<%1>%2</%1>").arg(qn(sp, "sldIdLst"), secEntries);
        if (lastSection.empty) {
            // <p14:section .../>  ->  <p14:section ...><p14:sldIdLst>...</p14:sldIdLst></p14:section>
            QString openTag = s.mid(lastSection.start, lastSection.startEnd - lastSection.start);
            openTag = openTag.trimmed();
            openTag.chop(2);
            while (openTag.endsWith(QLatin1Char(' ')) || openTag.endsWith(QLatin1Char('\t'))
                   || openTag.endsWith(QLatin1Char('\r')) || openTag.endsWith(QLatin1Char('\n')))
                openTag.chop(1);
            edits.append({ lastSection.start, lastSection.startEnd - lastSection.start,
                           openTag + QLatin1Char('>') + newList
                                   + QStringLiteral("</%1>").arg(qn(sp, "section")) });
        } else if (sectionList.start >= 0) {
            if (sectionList.empty)
                edits.append({ sectionList.start, sectionList.startEnd - sectionList.start,
                               newList });
            else
                edits.append({ sectionList.endStart, 0, secEntries });
        } else {
            // CT_Section: (sldIdLst?, extLst?)
            edits.append({ sectionExt.start >= 0 ? sectionExt.start : lastSection.endStart, 0,
                           newList });
        }
    }

    std::sort(edits.begin(), edits.end(),
              [](const Edit &a, const Edit &b) { return a.pos > b.pos; });
    QString out = s;
    for (const Edit &e : edits)
        out.replace(e.pos, e.removeLen, e.text);
    presXml = out.toUtf8();
    return true;
}

void slideSize(const QByteArray &presXml, qint64 *w, qint64 *h)
{
    QXmlStreamReader r(presXml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QLatin1String("sldSz")) {
            const qint64 cx = r.attributes().value(QLatin1String("cx")).toLongLong();
            const qint64 cy = r.attributes().value(QLatin1String("cy")).toLongLong();
            if (cx > 0 && cy > 0) {
                *w = cx;
                *h = cy;
            }
            return;
        }
    }
}

// Picks the layout new slides are based on: a "blank" layout of the first master, else the
// one with the fewest shapes.
QString chooseLayout(const Package &pkg, const QString &presPart, const QVector<Rel> &presRels)
{
    const QString presDir = partDir(presPart);
    QStringList layouts;
    for (const Rel &r : presRels) {
        if (!relTypeIs(r, "slideMaster"))
            continue;
        const QString master = resolveTarget(presDir, r.target);
        bool ok = true;
        const QVector<Rel> masterRels = parseRels(pkg.get(relsPathFor(master)), &ok);
        if (!ok)
            return QString(); // reported as "no slide layout"
        for (const Rel &mr : masterRels) {
            if (relTypeIs(mr, "slideLayout")) {
                const QString lp = resolveTarget(partDir(master), mr.target);
                if (pkg.has(lp))
                    layouts << lp;
            }
        }
        if (!layouts.isEmpty())
            break;
    }
    QString best;
    int bestShapes = INT_MAX;
    for (const QString &lp : layouts) {
        const QByteArray x = pkg.get(lp);
        QXmlStreamReader r(x);
        bool blank = false;
        int shapes = 0;
        while (!r.atEnd()) {
            r.readNext();
            if (!r.isStartElement())
                continue;
            if (r.name() == QLatin1String("sldLayout")
                && r.attributes().value(QLatin1String("type")) == QLatin1String("blank"))
                blank = true;
            if (r.name() == QLatin1String("sp") || r.name() == QLatin1String("pic")
                || r.name() == QLatin1String("graphicFrame"))
                ++shapes;
        }
        if (blank)
            return lp;
        if (shapes < bestShapes) {
            bestShapes = shapes;
            best = lp;
        }
    }
    return best;
}

// Adds <Override> entries to [Content_Types].xml, skipping part names that already have one
// (an existing deck may carry Overrides for parts that are gone), with the root's prefix.
bool addContentTypeOverrides(QByteArray &ctXml, const QVector<QPair<QString, QString>> &add)
{
    const QString s = QString::fromUtf8(ctXml);
    QXmlStreamReader r(s);
    QSet<QString> existing;
    QString prefix;
    int rootEndTag = -1, depth = 0;
    qint64 before = 0;
    while (!r.atEnd()) {
        r.readNext();
        const int tokStart = s.indexOf(QLatin1Char('<'), int(before));
        before = r.characterOffset();
        if (r.isStartElement()) {
            if (depth == 0)
                prefix = r.prefix().toString();
            if (r.name() == QLatin1String("Override"))
                existing.insert(
                        r.attributes().value(QLatin1String("PartName")).toString().toLower());
            ++depth;
        } else if (r.isEndElement()) {
            --depth;
            if (depth == 0)
                rootEndTag = tokStart;
        }
    }
    if (r.hasError() || rootEndTag < 0)
        return false;
    const QString el = prefix.isEmpty() ? QStringLiteral("Override")
                                        : prefix + QStringLiteral(":Override");
    QString text;
    for (const auto &a : add) {
        const QString pn = QLatin1Char('/') + a.first;
        if (existing.contains(pn.toLower()))
            continue;
        existing.insert(pn.toLower());
        text += QStringLiteral("<%1 PartName=\"%2\" ContentType=\"%3\"/>")
                        .arg(el, xmlEsc(pn), xmlEsc(a.second));
    }
    QString out = s;
    out.insert(rootEndTag, text);
    ctXml = out.toUtf8();
    return true;
}

bool appendSlides(Package &pkg, const QVector<SlideItem> &items, const DeckOptions &opt,
                  DeckResult &res)
{
    // main part
    QString presPart = QStringLiteral("ppt/presentation.xml");
    bool relsOk = true;
    for (const Rel &r : parseRels(pkg.get(QStringLiteral("_rels/.rels")), &relsOk)) {
        if (relTypeIs(r, "officeDocument")) {
            presPart = resolveTarget(QString(), r.target);
            break;
        }
    }
    QByteArray presXml = pkg.get(presPart);
    QByteArray ctXml = pkg.get(QStringLiteral("[Content_Types].xml"));
    if (!relsOk || presXml.isEmpty() || ctXml.isEmpty()) {
        res.error = T("The file is not a PowerPoint presentation (.pptx).");
        return false;
    }
    const QString presRelsPart = relsPathFor(presPart);
    QVector<Rel> presRels = parseRels(pkg.get(presRelsPart), &relsOk);
    const PresentationInfo info = scanPresentation(presXml);
    if (info.strict
        || pkg.get(QStringLiteral("_rels/.rels")).contains("http://purl.oclc.org/ooxml/")) {
        // mixing Transitional parts into a Strict package would damage it
        res.error = T("Slides cannot be added to a file saved in the Strict Open XML format. "
                      "Save it again in PowerPoint as a normal PowerPoint Presentation (.pptx).");
        return false;
    }
    if (!relsOk || !info.ok) {
        res.error = T("The presentation part of the existing file could not be read.");
        return false;
    }
    const QString layoutPart = chooseLayout(pkg, presPart, presRels);
    if (layoutPart.isEmpty()) {
        res.error = T("The existing presentation has no slide layout to add slides to.");
        return false;
    }

    qint64 slideW = kSlideWidth16x9, slideH = kSlideHeight;
    slideSize(presXml, &slideW, &slideH);
    res.slideWidth = slideW;
    res.slideHeight = slideH;

    int maxRid = 0;
    for (const Rel &r : presRels) {
        if (r.id.startsWith(QLatin1String("rId")))
            maxRid = std::max(maxRid, r.id.mid(3).toInt());
    }
    auto ridTaken = [&presRels](const QString &id) {
        for (const Rel &r : presRels)
            if (r.id == id)
                return true;
        return false;
    };
    const int perSlideN = normalizedPerSlide(opt.perSlide);
    QVector<quint32> freeIds;
    if (!allocateSlideIds(info, (items.size() + perSlideN - 1) / perSlideN, &freeIds)) {
        res.error = T("The presentation part of the existing file could not be read.");
        return false;
    }
    int nextIdIndex = 0;

    const int perSlide = normalizedPerSlide(opt.perSlide);
    int rows = 1, cols = 1;
    gridDims(perSlide, &rows, &cols);
    int slideCounter = 1, oleCounter = 1, mediaCounter = 1;
    QVector<QPair<QString, QString>> ctAdd; // (part name, content type)
    QVector<QPair<quint32, QString>> newIds;
    const QString slideDir = QStringLiteral("ppt/slides");
    for (int start = 0; start < items.size(); start += perSlide) {
        const QString slidePart =
                uniquePartName(pkg, slideDir + QStringLiteral("/slide%1.xml"), &slideCounter);
        QVector<Rel> slideRels;
        slideRels.append({ QStringLiteral("rId1"), kRelBase + QStringLiteral("slideLayout"),
                           relativeTarget(slideDir, layoutPart), QString() });
        QString shapes;
        int shapeId = 2;
        for (int k = 0; k < perSlide && start + k < items.size(); ++k) {
            const SlideItem &it = items.at(start + k);
            qint64 cl, ct, cw, ch;
            gridCell(slideW, slideH, perSlide, k, &cl, &ct, &cw, &ch);
            Placement pl = placeInCell(it, cl, ct, cw, ch, opt);
            ShapeOnSlide s;
            s.x = pl.left;
            s.y = pl.top;
            s.w = pl.width;
            s.h = pl.height;
            const QString ext = it.previewExtension.toLower() == QLatin1String("png")
                    ? QStringLiteral("png")
                    : QStringLiteral("emf");
            const QString mediaPart = uniquePartName(
                    pkg, QStringLiteral("ppt/media/image%1.") + ext, &mediaCounter);
            pkg.set(mediaPart, it.previewBytes);
            ctAdd.append(qMakePair(mediaPart, ext == QLatin1String("png")
                                                      ? QStringLiteral("image/png")
                                                      : QStringLiteral("image/x-emf")));
            if (!it.oleBytes.isEmpty()) {
                const QString olePart = uniquePartName(
                        pkg, QStringLiteral("ppt/embeddings/oleObject%1.bin"), &oleCounter);
                pkg.set(olePart, it.oleBytes);
                ctAdd.append(qMakePair(olePart, kCtOle));
                s.oleRid = QStringLiteral("rId%1").arg(slideRels.size() + 1);
                slideRels.append({ s.oleRid, kRelBase + QStringLiteral("oleObject"),
                                   relativeTarget(slideDir, olePart), QString() });
            }
            s.imageRid = QStringLiteral("rId%1").arg(slideRels.size() + 1);
            slideRels.append({ s.imageRid, kRelBase + QStringLiteral("image"),
                               relativeTarget(slideDir, mediaPart), QString() });
            shapes += graphicFrameXml(it, s, shapeId);
            shapeId += 2;
            const double tol = double(kEmuPerCm) / 10.0;
            if (pl.left < -tol || pl.top < -tol || pl.left + pl.width > slideW + tol
                || pl.top + pl.height > slideH + tol)
                res.warnings << T("%1 (%2 x %3 cm) extends beyond the slide.")
                                        .arg(it.graphName,
                                             QString::number(pl.width / double(kEmuPerCm), 'f', 1),
                                             QString::number(pl.height / double(kEmuPerCm), 'f', 1));
            res.placements.append(pl);
        }
        pkg.set(slidePart,
                xmlDoc(QStringLiteral("<p:sld%1><p:cSld>").arg(pmlRootAttrs()) + emptySpTree()
                       + shapes
                       + QStringLiteral("</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/>"
                                        "</p:clrMapOvr></p:sld>")));
        pkg.set(relsPathFor(slidePart), relsXml(slideRels));
        ctAdd.append(qMakePair(slidePart, kCtSlide));

        QString rid;
        do {
            rid = QStringLiteral("rId%1").arg(++maxRid);
        } while (ridTaken(rid));
        presRels.append({ rid, kRelBase + QStringLiteral("slide"),
                          relativeTarget(partDir(presPart), slidePart), QString() });
        newIds.append(qMakePair(freeIds.at(nextIdIndex++), rid));
        ++res.newSlides;
    }

    QString err;
    if (!insertSlideIds(presXml, newIds, &err)) {
        res.error = err;
        return false;
    }
    if (!addContentTypeOverrides(ctXml, ctAdd)) {
        res.error = T("The file is not a PowerPoint presentation (.pptx).");
        return false;
    }
    pkg.set(QStringLiteral("[Content_Types].xml"), ctXml);
    pkg.set(presPart, presXml);
    pkg.set(presRelsPart, relsXml(presRels));
    res.slideCount = info.slideCount + res.newSlides;
    for (int i = 0; i < res.placements.size(); ++i)
        res.placements[i].slide = info.slideCount + 1 + i / perSlide;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void registerTranslations()
{
    static bool s_done = false;
    if (s_done)
        return;
    s_done = true;
    auto reg = [](const char *en, const char *ja) {
        // context-bound, so that generic words ("Open", "File", ...) do not change other screens
        ToriGraphFullJapaneseTranslator::registerContextTranslation(
                QStringLiteral("ToriGraphPptx"), QString::fromUtf8(en), QString::fromUtf8(ja));
    };
    reg("Send to Po&werPoint...", "PowerPoint へ送る(&W)...");
    reg("Send to PowerPoint", "PowerPoint へ送る");
    reg("Write graphs into a PowerPoint file (.pptx) as editable objects. Double-click a graph "
        "on the slide to edit it in ToriGraph.",
        "グラフを PowerPoint のファイル (.pptx) に、編集できるオブジェクトとして書き出します。"
        "スライド上のグラフをダブルクリックすると ToriGraph で編集できます。");
    reg("The file is not a PowerPoint presentation (.pptx).",
        "このファイルは PowerPoint のプレゼンテーション (.pptx) ではありません。");
    reg("The presentation part of the existing file could not be read.",
        "既存のファイルのスライド一覧を読み取れませんでした。");
    reg("The existing presentation has no slide layout to add slides to.",
        "既存のファイルに、スライドを追加するためのレイアウトがありません。");
    reg("%1 (%2 x %3 cm) extends beyond the slide.",
        "%1 (%2 × %3 cm) がスライドからはみ出しています。");
    reg("The existing file could not be opened as a PowerPoint file (it may be damaged or "
        "protected with a password).\n\nDetails: %1",
        "既存のファイルを PowerPoint のファイルとして開けませんでした (壊れているか、"
        "パスワードで保護されている可能性があります)。\n\n詳細: %1");
    reg("Could not write %1. If the file is open in PowerPoint, close it and try again.\n\n"
        "Details: %2",
        "%1 に書き込めませんでした。PowerPoint で開いている場合は閉じてから、もう一度"
        "試してください。\n\n詳細: %2");
    reg("No graph to send.", "送るグラフがありません。");
    reg("%1 could not be drawn: %2", "%1 を描けませんでした: %2");
    reg("The graph could not be drawn as a vector picture (EMF).",
        "グラフをベクター画像 (EMF) として描けませんでした。");
    reg("The editable object could not be created; a picture was placed instead.",
        "編集できるオブジェクトを作れなかったため、画像として貼りました。");
    reg("Sending graphs to PowerPoint needs Windows.",
        "PowerPoint へ送る機能は Windows でだけ使えます。");
    reg("Slides cannot be added to a file saved in the Strict Open XML format. Save it again in "
        "PowerPoint as a normal PowerPoint Presentation (.pptx).",
        "Strict Open XML 形式で保存されたファイルには追記できません。PowerPoint で通常の"
        "「PowerPoint プレゼンテーション (.pptx)」として保存し直してください。");
    reg("The 3D graph could not be captured. Restore its window (not minimized) and try again.",
        "3D グラフを取り込めませんでした。ウィンドウを最小化から戻して、もう一度試してください。");
    // dialog
    reg("Graphs to send", "送るグラフ");
    reg("Active graph (%1)", "作業中のグラフ (%1)");
    reg("All graphs (%1)", "すべてのグラフ (%1 個)");
    reg("Selected graphs:", "選んだグラフ:");
    reg("Select All", "すべて選択");
    reg("Deselect All", "すべて解除");
    reg("Slide layout", "スライドの配置");
    reg("Graphs per slide:", "1 枚に並べる数:");
    reg("1 per slide", "1 個");
    reg("2 per slide (side by side)", "2 個 (横に並べる)");
    reg("4 per slide (2 x 2)", "4 個 (2 × 2)");
    reg("6 per slide (2 x 3)", "6 個 (2 × 3)");
    reg("Slide size:", "スライドの大きさ:");
    reg("Widescreen 16:9 (33.9 x 19.1 cm)", "ワイド画面 16:9 (33.9 × 19.1 cm)");
    reg("Standard 4:3 (25.4 x 19.1 cm)", "標準 4:3 (25.4 × 19.1 cm)");
    reg("Size on the slide", "スライド上の大きさ");
    reg("Paste at", "貼り付け倍率");
    reg("Fit the graphs to the slide", "スライドの枠に合わせて拡大・縮小する");
    reg("Shrink graphs that do not fit", "入りきらないグラフは縮めて入れる");
    reg("Lab slide preset \"%1\": a %2 mm page pasted at %3 % shows %4 pt tick labels on the "
        "slide, like an Origin graph.",
        "研究室スライドのプリセット「%1」: %2 mm のページを %3 % で貼ると、目盛りの数字は"
        "スライド上で %4 pt になり、Origin のグラフと同じ見え方になります。");
    reg("Apply the lab slide preset to the graphs first (can be undone)",
        "書き出す前に、研究室スライドの体裁をグラフに当てる (元に戻せます)");
    reg("File", "ファイル");
    reg("Save to:", "保存先:");
    reg("Browse...", "参照...");
    reg("If the file already exists, you will be asked whether to add slides at the end or to "
        "replace it.",
        "既存のファイルを選ぶと、末尾にスライドを追加するか上書きするかを尋ねます。");
    reg("Send Graphs", "書き出す");
    reg("PowerPoint Files (*.pptx)", "PowerPoint ファイル (*.pptx)");
    reg("Choose a file name.", "保存先のファイル名を指定してください。");
    reg("Choose at least one graph.", "グラフを 1 つ以上選んでください。");
    reg("%1 already exists.\nAdd the graphs as new slides at the end, or replace the file?",
        "%1 は既にあります。\nグラフを末尾の新しいスライドとして追加しますか、それとも"
        "ファイルを置き換えますか?");
    reg("Add Slides at End", "末尾にスライドを追加");
    reg("Replace File", "上書きする");
    reg("Sent %1 graph(s) to %2 slide(s).", "%1 個のグラフを %2 枚のスライドに書き出しました。");
    reg("Added %1 graph(s) on %2 new slide(s) at the end.",
        "%1 個のグラフを、末尾の %2 枚の新しいスライドに追加しました。");
    reg("Double-click a graph on the slide to edit it in ToriGraph.",
        "スライド上のグラフをダブルクリックすると ToriGraph で編集できます。");
    reg("Size on the slide: %1 % of the graph page.", "スライド上の大きさ: グラフのページの %1 %。");
    reg("%1 did not fit at %2 % and was shrunk to %3 %.",
        "%1 は %2 % では入りきらないため、%3 % に縮めました。");
    reg("The PowerPoint link of ToriGraph is not registered, so double-clicking will not open "
        "ToriGraph. Use Help > Repair PowerPoint Link.",
        "ToriGraph の PowerPoint 連携が登録されていないため、ダブルクリックしても ToriGraph は"
        "開きません。ヘルプの「PowerPoint 連携を修復」を使ってください。");
    reg("Open", "開く");
    reg("Close", "閉じる");
    reg("Could not send the graphs to PowerPoint.", "PowerPoint へ送れませんでした。");
    auto regAny = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    regAny("The ZIP archive is already finished.", "ZIP の書き出しは既に終わっています。");
    regAny("A ZIP entry needs a name.", "ZIP の項目に名前がありません。");
    regAny("Duplicate ZIP entry: %1", "ZIP の項目が重複しています: %1");
    regAny("Too many ZIP entries.", "ZIP の項目が多すぎます。");
    regAny("A ZIP entry name is too long.", "ZIP の項目名が長すぎます。");
    regAny("The ZIP archive would be too large.", "ZIP が大きくなりすぎます。");
}

QString sendToPowerPointMenuText()
{
    return T("Send to Po&werPoint...");
}

QString sendToPowerPointLabel()
{
    return T("Send to PowerPoint");
}

QString sendToPowerPointToolTip()
{
    return T("Write graphs into a PowerPoint file (.pptx) as editable objects. Double-click a "
             "graph on the slide to edit it in ToriGraph.");
}

QString labSlidePresetId()
{
    return QStringLiteral("lab_slide_83mm");
}

double presetPastePercent(const QString &presetId)
{
    MultiLayer::GraphStylePresetSpec spec;
    if (MultiLayer::findStylePreset(presetId, &spec) && spec.slidePastePercent > 0.0)
        return spec.slidePastePercent;
    return 100.0;
}

void gridCell(qint64 slideW, qint64 slideH, int perSlide, int cellIndex, qint64 *left,
              qint64 *top, qint64 *width, qint64 *height)
{
    int rows = 1, cols = 1;
    gridDims(normalizedPerSlide(perSlide), &rows, &cols);
    const qint64 marginX = cmToEmu(1.35);
    const qint64 marginTop = cmToEmu(1.25);
    const qint64 marginBottom = cmToEmu(1.15);
    const qint64 gapX = cols > 1 ? cmToEmu(0.85) : 0;
    const qint64 gapY = rows > 1 ? cmToEmu(0.80) : 0;
    const qint64 usableW = std::max<qint64>(kEmuPerCm, slideW - 2 * marginX - (cols - 1) * gapX);
    const qint64 usableH =
            std::max<qint64>(kEmuPerCm, slideH - marginTop - marginBottom - (rows - 1) * gapY);
    const qint64 cw = usableW / cols;
    const qint64 ch = usableH / rows;
    const int r = cellIndex / cols;
    const int c = cellIndex % cols;
    *left = marginX + c * (cw + gapX);
    *top = marginTop + r * (ch + gapY);
    *width = cw;
    *height = ch;
}

DeckResult writeDeck(const QString &path, const QVector<SlideItem> &items, const DeckOptions &opt)
{
    DeckResult res;
    res.path = QFileInfo(path).absoluteFilePath();
    if (items.isEmpty()) {
        res.error = T("No graph to send.");
        return res;
    }
    Package pkg;
    if (opt.appendToExisting && QFileInfo::exists(path)) {
        torizip::ZipArchive zip;
        QString zerr;
        if (!zip.open(path, &zerr)) {
            res.error = T("The existing file could not be opened as a PowerPoint file (it may be "
                          "damaged or protected with a password).\n\nDetails: %1")
                                .arg(zerr);
            return res;
        }
        for (int i = 0; i < zip.entries().size(); ++i) {
            QByteArray data;
            if (!zip.extract(i, data, &zerr)) {
                res.error = T("The existing file could not be opened as a PowerPoint file (it "
                              "may be damaged or protected with a password).\n\nDetails: %1")
                                    .arg(zerr);
                return res;
            }
            pkg.set(zip.entries().at(i).name, data);
        }
        res.appended = true;
    } else {
        pkg = newPresentationTemplate(opt.widescreen ? kSlideWidth16x9 : kSlideWidth4x3,
                                      kSlideHeight);
    }

    if (!appendSlides(pkg, items, opt, res))
        return res;

    torizip::ZipWriter zw;
    QString zerr;
    // [Content_Types].xml first, as Office writes it
    if (!zw.addFile(QStringLiteral("[Content_Types].xml"),
                    pkg.get(QStringLiteral("[Content_Types].xml")),
                    torizip::ZipWriter::Compression::Deflate, &zerr)) {
        res.error = zerr;
        return res;
    }
    for (const auto &part : pkg.parts) {
        if (part.first.compare(QLatin1String("[Content_Types].xml"), Qt::CaseInsensitive) == 0)
            continue;
        if (part.first.endsWith(QLatin1Char('/')))
            continue; // directory entries are not parts
        if (!zw.addFile(part.first, part.second, torizip::ZipWriter::Compression::Deflate, &zerr)) {
            res.error = zerr;
            return res;
        }
    }
    const QByteArray zipBytes = zw.finish();

    const QFileInfo fi(res.path);
    if (!fi.dir().exists())
        QDir().mkpath(fi.dir().absolutePath());
    QSaveFile f(res.path);
    if (!f.open(QIODevice::WriteOnly) || f.write(zipBytes) != zipBytes.size() || !f.commit()) {
        res.error = T("Could not write %1. If the file is open in PowerPoint, close it and try "
                      "again.\n\nDetails: %2")
                            .arg(QDir::toNativeSeparators(res.path), f.errorString());
        return res;
    }
    res.ok = true;
    return res;
}

QList<MyWidget *> graphWindows(ApplicationWindow *app)
{
    QList<MyWidget *> out;
    if (!app)
        return out;
    for (MyWidget *w : app->windowsList()) {
        if (w && (w->inherits("MultiLayer") || w->inherits("Graph3D")))
            out << w;
    }
    return out;
}

#ifdef Q_OS_WIN
namespace {

HRESULT writeExtraStream(IStorage *stg, const wchar_t *name, const QByteArray &bytes)
{
    IStream *stm = nullptr;
    HRESULT hr = stg->CreateStream(name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0,
                                   0, &stm);
    if (FAILED(hr) || !stm)
        return FAILED(hr) ? hr : E_FAIL;
    ULONG written = 0;
    if (!bytes.isEmpty())
        hr = stm->Write(bytes.constData(), static_cast<ULONG>(bytes.size()), &written);
    stm->Release();
    if (SUCCEEDED(hr) && written != static_cast<ULONG>(bytes.size()))
        hr = STG_E_WRITEFAULT;
    return hr;
}

// The OLE2 compound file of one embedded graph (same streams as the Python SDK).
QByteArray buildOleCompound(const SciDAVisOle::OleGraphPayload &payload)
{
    QByteArray out;
    ILockBytes *lockBytes = nullptr;
    if (FAILED(CreateILockBytesOnHGlobal(nullptr, TRUE, &lockBytes)) || !lockBytes)
        return out;
    IStorage *stg = nullptr;
    HRESULT hr = StgCreateDocfileOnILockBytes(
            lockBytes, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    if (SUCCEEDED(hr) && stg) {
        hr = SciDAVisOle::savePayloadToStorage(stg, payload);
        if (SUCCEEDED(hr)) {
            // \001Ole: OLE 2.0 object header (version 0x02000001, no flags, no link)
            QByteArray ole10(20, '\0');
            ole10[0] = char(0x01);
            ole10[3] = char(0x02);
            hr = writeExtraStream(stg, L"\001Ole", ole10);
        }
        if (SUCCEEDED(hr))
            hr = writeExtraStream(stg, L"ToriGraphProject", payload.projectBytes);
        if (SUCCEEDED(hr))
            hr = stg->Commit(STGC_DEFAULT);
        stg->Release();
        STATSTG stat{};
        if (SUCCEEDED(hr) && SUCCEEDED(lockBytes->Stat(&stat, STATFLAG_NONAME))
            && stat.cbSize.QuadPart > 0 && stat.cbSize.QuadPart < 256000000ULL) {
            HGLOBAL hg = nullptr;
            if (SUCCEEDED(GetHGlobalFromILockBytes(lockBytes, &hg)) && hg) {
                if (const void *raw = GlobalLock(hg)) {
                    out = QByteArray(static_cast<const char *>(raw),
                                     static_cast<int>(stat.cbSize.QuadPart));
                    GlobalUnlock(hg);
                }
            }
        }
    }
    lockBytes->Release();
    return out;
}

QImage captureGraph3D(Graph3D *g3d)
{
    QImage fb;
    if (!g3d)
        return fb;
    g3d->update();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (g3d->sp) {
        g3d->sp->makeCurrent();
        fb = g3d->sp->grabFramebuffer();
    }
    if (fb.isNull()) {
        QTemporaryFile tmpPng(QDir::temp().filePath(QStringLiteral("torigraph_pptx3d_XXXXXX.png")));
        if (tmpPng.open()) {
            const QString tmpPath = tmpPng.fileName();
            tmpPng.close();
            g3d->exportImage(tmpPath, 100, false);
            fb.load(tmpPath);
            QFile::remove(tmpPath);
        }
    }
    return fb;
}

} // namespace
#endif

bool buildSlideItem(ApplicationWindow *app, MyWidget *window, SlideItem *out, QString *outErr)
{
    if (!window || !out)
        return false;
#ifdef Q_OS_WIN
    SciDAVisOle::OleGraphPayload payload;
    payload.targetWindow = window->name();
    payload.targetLayer = 0;
    OleRenderedGraph rg;
    if (window->inherits("MultiLayer")) {
        MultiLayer *ml = static_cast<MultiLayer *>(window);
        for (Graph *lg : ml->layersList()) {
            if (lg) {
                lg->updatePlot();
                lg->replot();
            }
        }
        rg = SciDAVisEmf::renderMultiLayer(ml, nullptr, false);
    } else if (window->inherits("Graph3D")) {
        const QImage img = captureGraph3D(static_cast<Graph3D *>(window));
        if (img.isNull()) {
            // renderFromImage() would return a blank 500 x 375 page
            if (outErr)
                *outErr = T("The 3D graph could not be captured. Restore its window (not "
                            "minimized) and try again.");
            return false;
        }
        rg = SciDAVisEmf::renderFromImage(img);
    }
    if (rg.emfBytes.isEmpty()) {
        if (outErr)
            *outErr = T("The graph could not be drawn as a vector picture (EMF).");
        return false;
    }
    payload.emfBytes = rg.emfBytes;
    payload.dibBytes = rg.dibBytes;
    payload.extentHm = rg.extentHm;
    payload.pixelSize = rg.pixelSize;
    payload.projectBytes = SciDAVisOle::buildEmbeddedProjectBytes(
            app, window, SciDAVisOle::embedScopeSetting(), nullptr);

    out->graphName = window->name();
    out->previewBytes = rg.emfBytes;
    out->previewExtension = QStringLiteral("emf");
    out->widthHm = rg.extentHm.cx;
    out->heightHm = rg.extentHm.cy;
    out->oleBytes = payload.projectBytes.isEmpty() ? QByteArray() : buildOleCompound(payload);
    if (out->oleBytes.isEmpty() && outErr)
        *outErr = T("The editable object could not be created; a picture was placed instead.");
    return true;
#else
    Q_UNUSED(app);
    if (outErr)
        *outErr = T("Sending graphs to PowerPoint needs Windows.");
    return false;
#endif
}

DeckResult exportGraphs(ApplicationWindow *app, const QList<MyWidget *> &windows,
                        const DeckOptions &opt, const QString &path)
{
    QVector<SlideItem> items;
    QStringList warnings;
    QString firstError;
    for (MyWidget *w : windows) {
        SlideItem it;
        QString err;
        if (buildSlideItem(app, w, &it, &err)) {
            items.append(it);
            if (!err.isEmpty())
                warnings << QStringLiteral("%1: %2").arg(w->name(), err);
        } else {
            const QString msg = T("%1 could not be drawn: %2").arg(w ? w->name() : QString(), err);
            warnings << msg;
            if (firstError.isEmpty())
                firstError = msg;
        }
    }
    if (items.isEmpty()) {
        DeckResult res;
        res.path = QFileInfo(path).absoluteFilePath();
        res.error = firstError.isEmpty() ? T("No graph to send.") : firstError;
        res.warnings = warnings;
        return res;
    }
    DeckResult res = writeDeck(path, items, opt);
    res.warnings = warnings + res.warnings;
    return res;
}

// ---------------------------------------------------------------------------
// Dialog and menu command
// ---------------------------------------------------------------------------
namespace {

const char kKeyPath[] = "/PptxExport/LastPath";
const char kKeyPerSlide[] = "/PptxExport/PerSlide";
const char kKeyWide[] = "/PptxExport/Widescreen";
const char kKeyFit[] = "/PptxExport/FitToSlide";
const char kKeyShrink[] = "/PptxExport/ShrinkToFit";

QString defaultDeckPath(ApplicationWindow *app)
{
    const QString last = ApplicationWindow::getSettings().value(QLatin1String(kKeyPath)).toString();
    if (!last.trimmed().isEmpty())
        return last;
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    QString base = QStringLiteral("ToriGraph_graphs");
    if (app && !app->projectname.isEmpty() && app->projectname != QLatin1String("untitled")) {
        const QString b = QFileInfo(app->projectname).completeBaseName();
        if (!b.isEmpty())
            base = b;
    }
    return QDir(dir).filePath(base + QStringLiteral(".pptx"));
}

QString fmtNumber(double v)
{
    // 83.3, 140, 15.7 (one decimal at most, no trailing zeros)
    return QString::number(std::round(v * 10.0) / 10.0, 'g', 6);
}

} // namespace

QDialog *createSendDialog(ApplicationWindow *app, QWidget *parent)
{
    registerTranslations();
    QSettings &st = ApplicationWindow::getSettings();
    auto *dlg = new QDialog(parent ? parent : static_cast<QWidget *>(app));
    dlg->setObjectName(QStringLiteral("SendToPowerPointDialog"));
    dlg->setWindowTitle(sendToPowerPointLabel());

    const QList<MyWidget *> graphs = graphWindows(app);
    MyWidget *active = app ? app->activeWindow() : nullptr;
    if (active && !graphs.contains(active))
        active = nullptr;
    QList<QPointer<MyWidget>> safeGraphs;
    safeGraphs.reserve(graphs.size());
    for (MyWidget *w : graphs)
        safeGraphs.append(QPointer<MyWidget>(w));
    QPointer<MyWidget> safeActive(active);

    auto *top = new QVBoxLayout(dlg);

    // --- graphs ---
    auto *gbGraphs = new QGroupBox(T("Graphs to send"), dlg);
    auto *vg = new QVBoxLayout(gbGraphs);
    auto *rbActive = new QRadioButton(
            T("Active graph (%1)").arg(active ? active->name() : QStringLiteral("-")), gbGraphs);
    rbActive->setObjectName(QStringLiteral("pptxTargetActive"));
    rbActive->setEnabled(active != nullptr);
    auto *rbAll = new QRadioButton(T("All graphs (%1)").arg(graphs.size()), gbGraphs);
    rbAll->setObjectName(QStringLiteral("pptxTargetAll"));
    auto *selHeaderRow = new QHBoxLayout();
    auto *rbSel = new QRadioButton(T("Selected graphs:"), gbGraphs);
    rbSel->setObjectName(QStringLiteral("pptxTargetSelected"));
    auto *btnSelectAll = new QPushButton(T("Select All"), gbGraphs);
    btnSelectAll->setObjectName(QStringLiteral("pptxSelectAll"));
    btnSelectAll->setEnabled(false);
    auto *btnDeselectAll = new QPushButton(T("Deselect All"), gbGraphs);
    btnDeselectAll->setObjectName(QStringLiteral("pptxDeselectAll"));
    btnDeselectAll->setEnabled(false);
    selHeaderRow->addWidget(rbSel);
    selHeaderRow->addStretch(1);
    selHeaderRow->addWidget(btnSelectAll);
    selHeaderRow->addWidget(btnDeselectAll);
    auto *list = new QListWidget(gbGraphs);
    list->setObjectName(QStringLiteral("pptxGraphList"));
    for (MyWidget *w : graphs) {
        auto *item = new QListWidgetItem(w->name(), list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(w == active ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<QObject *>(w)));
        item->setData(Qt::UserRole + 1, w->name());
    }
    list->setMaximumHeight(120);
    list->setEnabled(false);
    QObject::connect(rbSel, &QRadioButton::toggled, list, &QWidget::setEnabled);
    QObject::connect(rbSel, &QRadioButton::toggled, btnSelectAll, &QWidget::setEnabled);
    QObject::connect(rbSel, &QRadioButton::toggled, btnDeselectAll, &QWidget::setEnabled);
    QObject::connect(btnSelectAll, &QPushButton::clicked, list, [list]() {
        const QSignalBlocker sb(list);
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setCheckState(Qt::Checked);
        emit list->itemSelectionChanged();
    });
    QObject::connect(btnDeselectAll, &QPushButton::clicked, list, [list]() {
        const QSignalBlocker sb(list);
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setCheckState(Qt::Unchecked);
        emit list->itemSelectionChanged();
    });
    (active ? rbActive : rbAll)->setChecked(true);
    vg->addWidget(rbActive);
    vg->addWidget(rbAll);
    vg->addLayout(selHeaderRow);
    vg->addWidget(list);
    top->addWidget(gbGraphs);

    // --- layout ---
    auto *gbLayout = new QGroupBox(T("Slide layout"), dlg);
    auto *fl = new QFormLayout(gbLayout);
    auto *cbPer = new QComboBox(gbLayout);
    cbPer->setObjectName(QStringLiteral("pptxPerSlide"));
    cbPer->addItem(T("1 per slide"), 1);
    cbPer->addItem(T("2 per slide (side by side)"), 2);
    cbPer->addItem(T("4 per slide (2 x 2)"), 4);
    cbPer->addItem(T("6 per slide (2 x 3)"), 6);
    const int perIdx = cbPer->findData(st.value(QLatin1String(kKeyPerSlide), 1).toInt());
    cbPer->setCurrentIndex(perIdx >= 0 ? perIdx : 0);
    fl->addRow(T("Graphs per slide:"), cbPer);
    auto *cbSize = new QComboBox(gbLayout);
    cbSize->setObjectName(QStringLiteral("pptxSlideSize"));
    cbSize->addItem(T("Widescreen 16:9 (33.9 x 19.1 cm)"), true);
    cbSize->addItem(T("Standard 4:3 (25.4 x 19.1 cm)"), false);
    cbSize->setCurrentIndex(st.value(QLatin1String(kKeyWide), true).toBool() ? 0 : 1);
    fl->addRow(T("Slide size:"), cbSize);
    top->addWidget(gbLayout);

    // --- size on the slide (auto-linked to the target graph's preset or lab preset checkbox) ---
    MultiLayer::GraphStylePresetSpec lab;
    const bool haveLab = MultiLayer::findStylePreset(labSlidePresetId(), &lab);
    const double labPct = presetPastePercent(labSlidePresetId());
    auto computeTargetPastePct = [safeGraphs, safeActive, rbActive, rbSel, list,
                                  labPct](bool applyLabFirst) -> double {
        if (applyLabFirst)
            return labPct;
        QList<MyWidget *> targets;
        if (rbActive && rbActive->isChecked()) {
            if (safeActive)
                targets << safeActive.data();
        } else if (rbSel && rbSel->isChecked() && list) {
            for (int i = 0; i < list->count(); ++i) {
                if (list->item(i)->checkState() != Qt::Checked)
                    continue;
                if (auto *mw = qobject_cast<MyWidget *>(
                            list->item(i)->data(Qt::UserRole).value<QObject *>())) {
                    targets << mw;
                }
            }
        } else {
            for (const QPointer<MyWidget> &gp : safeGraphs) {
                if (gp)
                    targets << gp.data();
            }
        }
        if (targets.isEmpty()) {
            if (safeActive)
                targets << safeActive.data();
            else {
                for (const QPointer<MyWidget> &gp : safeGraphs) {
                    if (gp)
                        targets << gp.data();
                }
            }
        }
        bool anyMl = false;
        bool allSamePreset = true;
        QString commonPreset;
        for (MyWidget *w : targets) {
            if (!w || !w->inherits("MultiLayer"))
                continue;
            const QString pid = w->property("toriAppliedStylePreset").toString().trimmed();
            if (!anyMl) {
                anyMl = true;
                commonPreset = pid;
            } else if (pid != commonPreset) {
                allSamePreset = false;
            }
        }
        if (anyMl && allSamePreset && !commonPreset.isEmpty())
            return presetPastePercent(commonPreset);
        return 100.0;
    };

    auto *gbScale = new QGroupBox(T("Size on the slide"), dlg);
    auto *vs = new QVBoxLayout(gbScale);
    auto *hs = new QHBoxLayout();
    auto *rbScale = new QRadioButton(T("Paste at"), gbScale);
    rbScale->setObjectName(QStringLiteral("pptxScaleFixed"));
    auto *spin = new QDoubleSpinBox(gbScale);
    spin->setObjectName(QStringLiteral("pptxScalePercent"));
    spin->setRange(10.0, 400.0);
    spin->setDecimals(0);
    spin->setSingleStep(10.0);
    spin->setSuffix(QStringLiteral(" %"));
    spin->setValue(computeTargetPastePct(false));
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), dlg, [spin]() {
        spin->setProperty("_tg_user_edited_scale", true);
    });
    hs->addWidget(rbScale);
    hs->addWidget(spin);
    hs->addStretch(1);
    vs->addLayout(hs);
    auto *rbFit = new QRadioButton(T("Fit the graphs to the slide"), gbScale);
    rbFit->setObjectName(QStringLiteral("pptxScaleFit"));
    vs->addWidget(rbFit);
    (st.value(QLatin1String(kKeyFit), false).toBool() ? rbFit : rbScale)->setChecked(true);
    spin->setEnabled(rbScale->isChecked());
    QObject::connect(rbScale, &QRadioButton::toggled, spin, &QWidget::setEnabled);
    auto *chkShrink = new QCheckBox(T("Shrink graphs that do not fit"), gbScale);
    chkShrink->setObjectName(QStringLiteral("pptxShrinkToFit"));
    chkShrink->setChecked(st.value(QLatin1String(kKeyShrink), true).toBool());
    chkShrink->setEnabled(rbScale->isChecked());
    QObject::connect(rbScale, &QRadioButton::toggled, chkShrink, &QWidget::setEnabled);
    vs->addWidget(chkShrink);
    QCheckBox *chkPreset = nullptr;
    if (haveLab) {
        auto *hint = new QLabel(
                T("Lab slide preset \"%1\": a %2 mm page pasted at %3 % shows %4 pt tick labels "
                  "on the slide, like an Origin graph.")
                        .arg(QCoreApplication::translate("MultiLayer",
                                                         lab.name.toUtf8().constData()),
                             fmtNumber(lab.pageWidthMm), fmtNumber(labPct),
                             fmtNumber(lab.tickLabelPt * labPct / 100.0)),
                gbScale);
        hint->setObjectName(QStringLiteral("pptxScaleHint"));
        hint->setWordWrap(true);
        vs->addWidget(hint);
        chkPreset = new QCheckBox(
                T("Apply the lab slide preset to the graphs first (can be undone)"), gbScale);
        chkPreset->setObjectName(QStringLiteral("pptxApplyPreset"));
        chkPreset->setChecked(false); // changes the graphs: never on by default
        vs->addWidget(chkPreset);
    }
    auto syncScaleFromTarget = [spin, chkPreset, computeTargetPastePct](bool forceOverride) {
        if (!forceOverride && spin->property("_tg_user_edited_scale").toBool())
            return;
        const bool applyLab = chkPreset && chkPreset->isChecked();
        const QSignalBlocker sb(spin);
        spin->setValue(computeTargetPastePct(applyLab));
        if (forceOverride)
            spin->setProperty("_tg_user_edited_scale", false);
    };
    if (chkPreset) {
        QObject::connect(chkPreset, &QCheckBox::toggled, dlg,
                         [syncScaleFromTarget](bool) { syncScaleFromTarget(true); });
    }
    QObject::connect(rbActive, &QRadioButton::toggled, dlg,
                     [syncScaleFromTarget](bool on) { if (on) syncScaleFromTarget(false); });
    QObject::connect(rbAll, &QRadioButton::toggled, dlg,
                     [syncScaleFromTarget](bool on) { if (on) syncScaleFromTarget(false); });
    QObject::connect(rbSel, &QRadioButton::toggled, dlg,
                     [syncScaleFromTarget](bool on) { if (on) syncScaleFromTarget(false); });
    QObject::connect(list, &QListWidget::itemChanged, dlg,
                     [syncScaleFromTarget](QListWidgetItem *) { syncScaleFromTarget(false); });
    QObject::connect(list, &QListWidget::itemSelectionChanged, dlg,
                     [syncScaleFromTarget]() { syncScaleFromTarget(false); });
    top->addWidget(gbScale);

    // --- file ---
    auto *gbFile = new QGroupBox(T("File"), dlg);
    auto *vf = new QVBoxLayout(gbFile);
    auto *hf = new QHBoxLayout();
    hf->addWidget(new QLabel(T("Save to:"), gbFile));
    auto *edPath = new QLineEdit(QDir::toNativeSeparators(defaultDeckPath(app)), gbFile);
    edPath->setObjectName(QStringLiteral("pptxPath"));
    edPath->setMinimumWidth(320);
    hf->addWidget(edPath, 1);
    auto *btnBrowse = new QPushButton(T("Browse..."), gbFile);
    btnBrowse->setObjectName(QStringLiteral("pptxBrowse"));
    hf->addWidget(btnBrowse);
    vf->addLayout(hf);
    auto *note = new QLabel(T("If the file already exists, you will be asked whether to add "
                              "slides at the end or to replace it."),
                            gbFile);
    note->setWordWrap(true);
    vf->addWidget(note);
    top->addWidget(gbFile);
    QObject::connect(btnBrowse, &QPushButton::clicked, dlg, [dlg, edPath]() {
        const QString f = QFileDialog::getSaveFileName(
                dlg, sendToPowerPointLabel(), QDir::fromNativeSeparators(edPath->text()),
                T("PowerPoint Files (*.pptx)"), nullptr, QFileDialog::DontConfirmOverwrite);
        if (!f.isEmpty())
            edPath->setText(QDir::toNativeSeparators(f));
    });

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->setObjectName(QStringLiteral("pptxButtons"));
    bb->button(QDialogButtonBox::Ok)->setText(T("Send Graphs"));
    QObject::connect(bb, &QDialogButtonBox::accepted, dlg, [dlg, app]() {
        QString err;
        if (!readSendDialog(dlg, app, nullptr, nullptr, nullptr, nullptr, &err)) {
            QMessageBox::warning(dlg, sendToPowerPointLabel(), err);
            return;
        }
        dlg->accept();
    });
    QObject::connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    top->addWidget(bb);
    return dlg;
}

bool readSendDialog(QDialog *dialog, ApplicationWindow *app, QList<MyWidget *> *windows,
                    DeckOptions *options, QString *path, bool *applyLabPreset, QString *outErr)
{
    if (!dialog)
        return false;
    auto fail = [outErr](const QString &m) {
        if (outErr)
            *outErr = m;
        return false;
    };
    const QList<MyWidget *> graphs = graphWindows(app);
    QList<MyWidget *> chosen;
    auto *rbActive = dialog->findChild<QRadioButton *>(QStringLiteral("pptxTargetActive"));
    auto *rbSel = dialog->findChild<QRadioButton *>(QStringLiteral("pptxTargetSelected"));
    auto *list = dialog->findChild<QListWidget *>(QStringLiteral("pptxGraphList"));
    if (rbActive && rbActive->isChecked()) {
        MyWidget *a = app ? app->activeWindow() : nullptr;
        if (a && graphs.contains(a))
            chosen << a;
    } else if (rbSel && rbSel->isChecked() && list) {
        QStringList names;
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->checkState() != Qt::Checked)
                continue;
            if (auto *mw = qobject_cast<MyWidget *>(
                        list->item(i)->data(Qt::UserRole).value<QObject *>())) {
                if (graphs.contains(mw) && !chosen.contains(mw))
                    chosen << mw;
                continue;
            }
            const QString fallbackName = list->item(i)->data(Qt::UserRole + 1).isValid()
                    ? list->item(i)->data(Qt::UserRole + 1).toString()
                    : list->item(i)->data(Qt::UserRole).toString();
            if (!fallbackName.isEmpty())
                names << fallbackName;
        }
        for (MyWidget *w : graphs)
            if (names.contains(w->name()) && !chosen.contains(w))
                chosen << w;
    } else {
        chosen = graphs;
    }
    if (chosen.isEmpty())
        return fail(T("Choose at least one graph."));

    DeckOptions o;
    if (auto *cb = dialog->findChild<QComboBox *>(QStringLiteral("pptxPerSlide")))
        o.perSlide = cb->currentData().toInt();
    if (auto *cb = dialog->findChild<QComboBox *>(QStringLiteral("pptxSlideSize")))
        o.widescreen = cb->currentData().toBool();
    auto *rbFit = dialog->findChild<QRadioButton *>(QStringLiteral("pptxScaleFit"));
    auto *spin = dialog->findChild<QDoubleSpinBox *>(QStringLiteral("pptxScalePercent"));
    o.scalePercent = (rbFit && rbFit->isChecked()) ? 0.0 : (spin ? spin->value() : 100.0);
    if (auto *chk = dialog->findChild<QCheckBox *>(QStringLiteral("pptxShrinkToFit")))
        o.shrinkToFit = chk->isChecked();
    bool preset = false;
    if (auto *chk = dialog->findChild<QCheckBox *>(QStringLiteral("pptxApplyPreset")))
        preset = chk->isChecked();

    QString p;
    if (auto *ed = dialog->findChild<QLineEdit *>(QStringLiteral("pptxPath")))
        p = QDir::fromNativeSeparators(ed->text().trimmed());
    if (p.isEmpty() || QFileInfo(p).fileName().isEmpty())
        return fail(T("Choose a file name."));
    if (!p.endsWith(QLatin1String(".pptx"), Qt::CaseInsensitive))
        p += QStringLiteral(".pptx");

    if (windows)
        *windows = chosen;
    if (options)
        *options = o;
    if (path)
        *path = p;
    if (applyLabPreset)
        *applyLabPreset = preset;
    return true;
}

void sendGraphsToPowerPoint(ApplicationWindow *app)
{
    if (!app)
        return;
    registerTranslations();
    const QString title = sendToPowerPointLabel();
    const bool interactive = !ApplicationWindow::isNonInteractiveContext(app);
    if (graphWindows(app).isEmpty()) {
        if (app->statusBar())
            app->statusBar()->showMessage(T("No graph to send."), 8000);
        if (interactive)
            QMessageBox::information(app, title, T("No graph to send."));
        return;
    }
    if (!interactive)
        return;

    QScopedPointer<QDialog> dlg(createSendDialog(app, app));
    if (dlg->exec() != QDialog::Accepted)
        return;
    QList<MyWidget *> rawWindows;
    DeckOptions opt;
    QString path, err;
    bool applyPreset = false;
    if (!readSendDialog(dlg.data(), app, &rawWindows, &opt, &path, &applyPreset, &err)) {
        QMessageBox::warning(app, title, err);
        return;
    }
    dlg.reset();
    QList<QPointer<MyWidget>> safeWindows;
    safeWindows.reserve(rawWindows.size());
    for (MyWidget *w : rawWindows)
        safeWindows.append(QPointer<MyWidget>(w));

    if (QFileInfo::exists(path)) {
        QMessageBox box(QMessageBox::Question, title,
                        T("%1 already exists.\nAdd the graphs as new slides at the end, or "
                          "replace the file?")
                                .arg(QDir::toNativeSeparators(path)),
                        QMessageBox::NoButton, app);
        QPushButton *btnAdd = box.addButton(T("Add Slides at End"), QMessageBox::AcceptRole);
        QPushButton *btnReplace = box.addButton(T("Replace File"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(btnAdd);
        box.exec();
        if (box.clickedButton() == btnAdd)
            opt.appendToExisting = true;
        else if (box.clickedButton() == btnReplace)
            opt.appendToExisting = false;
        else
            return;
    }

    QList<MyWidget *> windows;
    for (const QPointer<MyWidget> &wp : safeWindows) {
        if (wp)
            windows << wp.data();
    }
    if (windows.isEmpty()) {
        QMessageBox::warning(app, title, T("No graph to send."));
        return;
    }

    QSettings &st = ApplicationWindow::getSettings();
    st.setValue(QLatin1String(kKeyPath), path);
    st.setValue(QLatin1String(kKeyPerSlide), opt.perSlide);
    st.setValue(QLatin1String(kKeyWide), opt.widescreen);
    st.setValue(QLatin1String(kKeyFit), opt.scalePercent <= 0.0);
    st.setValue(QLatin1String(kKeyShrink), opt.shrinkToFit);

    struct PresetRollbackState {
        QPointer<MultiLayer> ml;
        QString snapshot;
        QVariant prevPresetProp;
        int prevUndoCount = 0;
    };
    QVector<PresetRollbackState> rollbackStates;
    const bool wasProjectSaved = app->saved;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (applyPreset) {
        MultiLayer::GraphStylePresetSpec spec;
        if (MultiLayer::findStylePreset(labSlidePresetId(), &spec)) {
            for (const QPointer<MyWidget> &wp : safeWindows) {
                if (!wp || !wp->inherits("MultiLayer"))
                    continue;
                MultiLayer *ml = static_cast<MultiLayer *>(wp.data());
                PresetRollbackState rb;
                rb.ml = ml;
                rb.snapshot = ml->captureUndoSnapshot();
                rb.prevPresetProp = ml->property("toriAppliedStylePreset");
                rb.prevUndoCount = ml->undoStack() ? ml->undoStack()->count() : 0;
                rollbackStates.append(rb);
                ml->applyStylePresetSpec(spec);
            }
            app->modifiedProject();
            if (GraphInspectorDock *insp = app->graphInspectorDock())
                insp->refreshAll();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }
    windows.clear();
    for (const QPointer<MyWidget> &wp : safeWindows) {
        if (wp)
            windows << wp.data();
    }
    const DeckResult res = exportGraphs(app, windows, opt, path);
    if (!res.ok && !rollbackStates.isEmpty()) {
        for (const PresetRollbackState &rb : rollbackStates) {
            if (!rb.ml)
                continue;
            if (QUndoStack *us = rb.ml->undoStack()) {
                if (rb.prevUndoCount == 0) {
                    us->clear();
                } else {
                    while (us->count() > rb.prevUndoCount && us->canUndo())
                        us->undo();
                }
            }
            if (!rb.snapshot.isEmpty())
                rb.ml->restoreSnapshotInPlace(app, rb.snapshot);
            rb.ml->setProperty("toriAppliedStylePreset", rb.prevPresetProp);
        }
        if (wasProjectSaved)
            app->markProjectSaved();
        if (GraphInspectorDock *insp = app->graphInspectorDock())
            insp->refreshAll();
    }
    QApplication::restoreOverrideCursor();

    if (!res.ok) {
        QString msg = T("Could not send the graphs to PowerPoint.") + QStringLiteral("\n\n")
                + res.error;
        if (!res.warnings.isEmpty())
            msg += QStringLiteral("\n\n") + res.warnings.join(QLatin1Char('\n'));
        QMessageBox::warning(app, title, msg);
        return;
    }

    QStringList lines;
    lines << (res.appended ? T("Added %1 graph(s) on %2 new slide(s) at the end.")
                           : T("Sent %1 graph(s) to %2 slide(s)."))
                     .arg(res.placements.size())
                     .arg(res.newSlides);
    lines << QDir::toNativeSeparators(res.path);
    bool allSame = !res.placements.isEmpty();
    for (const Placement &pl : res.placements)
        if (std::fabs(pl.effectivePercent - res.placements.first().effectivePercent) > 0.5)
            allSame = false;
    if (allSame)
        lines << T("Size on the slide: %1 % of the graph page.")
                         .arg(fmtNumber(res.placements.first().effectivePercent));
    for (const Placement &pl : res.placements) {
        if (pl.shrunk)
            lines << T("%1 did not fit at %2 % and was shrunk to %3 %.")
                             .arg(pl.graphName, fmtNumber(opt.scalePercent),
                                  fmtNumber(pl.effectivePercent));
    }
    lines << T("Double-click a graph on the slide to edit it in ToriGraph.");
#ifdef Q_OS_WIN
    if (SciDAVisOle::queryRegistration().state != SciDAVisOle::RegistrationInfo::Valid)
        lines << T("The PowerPoint link of ToriGraph is not registered, so double-clicking will "
                   "not open ToriGraph. Use Help > Repair PowerPoint Link.");
#endif
    for (const QString &w : res.warnings)
        lines << w;
    if (app->statusBar())
        app->statusBar()->showMessage(lines.at(0) + QStringLiteral(" ") + lines.at(1), 10000);

    QMessageBox done(QMessageBox::Information, title, lines.join(QLatin1Char('\n')),
                     QMessageBox::NoButton, app);
    QPushButton *btnOpen = done.addButton(T("Open"), QMessageBox::AcceptRole);
    done.addButton(T("Close"), QMessageBox::RejectRole);
    done.setDefaultButton(btnOpen);
    done.exec();
    if (done.clickedButton() == btnOpen)
        QDesktopServices::openUrl(QUrl::fromLocalFile(res.path));
}

} // namespace ToriGraphPptx
