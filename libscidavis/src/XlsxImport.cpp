/***************************************************************************
    File                 : XlsxImport.cpp
    Project              : ToriGraph / SciDAVis
    Description          : Excel 2007+ workbook (.xlsx / .xlsm) reader without
                           external libraries.
 ***************************************************************************/

#include "XlsxImport.h"

#include "ApplicationWindow.h"
#include "I18nJapanese.h"
#include "MyWidget.h"
#include "Table.h"
#include "Workbook.h"
#include "ZipReader.h"
#include "future/core/column/Column.h"
#include "future/core/datatypes/DateTime2StringFilter.h"
#include "future/core/datatypes/Double2StringFilter.h"
#include "future/table/AsciiTableImportFilter.h"

#include <QCoreApplication>
#include <QFile>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QStatusBar>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace XlsxImport {

namespace {

constexpr int kMaxRows = 1048576;
constexpr int kMaxCols = 16384;
constexpr qint64 kMaxTotalCells = 20000000LL;
constexpr double kMaxAbsDateSerial = 36520590.0;

QString trX(const char *s)
{
    registerTranslations();
    return QCoreApplication::translate("XlsxImport", s);
}

// ---------------------------------------------------------------------------
// Package parts
// ---------------------------------------------------------------------------
struct Relationship
{
    QString type;
    QString target; // resolved part path inside the archive
};

QString dirOfPart(const QString &part)
{
    const int slash = part.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : part.left(slash);
}

QString resolvePartPath(const QString &baseDir, const QString &target)
{
    QString t = target;
    t.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString combined;
    if (t.startsWith(QLatin1Char('/')))
        combined = t.mid(1);
    else
        combined = baseDir.isEmpty() ? t : baseDir + QLatin1Char('/') + t;
    QStringList out;
    for (const QString &seg : combined.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (seg == QLatin1String("."))
            continue;
        if (seg == QLatin1String("..")) {
            if (!out.isEmpty())
                out.removeLast();
            continue;
        }
        out << seg;
    }
    return out.join(QLatin1Char('/'));
}

QHash<QString, Relationship> parseRelationships(const QByteArray &xml, const QString &baseDir)
{
    QHash<QString, Relationship> rels;
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        if (r.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (r.name() != QLatin1String("Relationship"))
            continue;
        const QXmlStreamAttributes a = r.attributes();
        if (a.value(QLatin1String("TargetMode")).compare(QLatin1String("External"),
                                                         Qt::CaseInsensitive)
            == 0)
            continue;
        Relationship rel;
        rel.type = a.value(QLatin1String("Type")).toString();
        rel.target = resolvePartPath(baseDir, a.value(QLatin1String("Target")).toString());
        rels.insert(a.value(QLatin1String("Id")).toString(), rel);
    }
    return rels;
}

bool readPart(const torizip::ZipArchive &zip, const QString &part, QByteArray &out,
              QString *outErr, bool *outMissing = nullptr)
{
    if (outMissing)
        *outMissing = false;
    const int idx = zip.findExactEntry(part);
    if (idx < 0) {
        if (outMissing)
            *outMissing = true;
        if (outErr)
            *outErr = QStringLiteral("missing part '%1'").arg(part);
        return false;
    }
    return zip.extract(idx, out, outErr);
}

bool typeEndsWith(const QString &type, const char *suffix)
{
    return type.endsWith(QLatin1String(suffix), Qt::CaseInsensitive);
}

// Text of an <si> / <is> element: every <t>, phonetic guides (<rPh>, furigana) left out.
QString readRichText(QXmlStreamReader &r)
{
    const QString endName = r.name().toString();
    QString text;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType tt = r.readNext();
        if (tt == QXmlStreamReader::StartElement) {
            if (r.name() == QLatin1String("rPh") || r.name() == QLatin1String("phoneticPr"))
                r.skipCurrentElement();
            else if (r.name() == QLatin1String("t"))
                text += r.readElementText(QXmlStreamReader::IncludeChildElements);
        } else if (tt == QXmlStreamReader::EndElement && r.name() == endName) {
            break;
        }
    }
    return text;
}

QStringList parseSharedStrings(const QByteArray &xml)
{
    QStringList strings;
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("si"))
            strings << readRichText(r);
    }
    return strings;
}

// ---------------------------------------------------------------------------
// Number formats: which cell styles show an Excel serial number as a date/time
// ---------------------------------------------------------------------------
struct FormatKind
{
    bool isDate = false;
    bool datePart = false;
    bool timePart = false;
};

FormatKind classifyFormatCode(const QString &code)
{
    FormatKind k;
    // First section only ("pos;neg;zero;text")
    QString s;
    bool inQuote = false;
    for (int i = 0; i < code.size(); ++i) {
        const QChar c = code.at(i);
        if (c == QLatin1Char('"')) {
            inQuote = !inQuote;
            continue;
        }
        if (inQuote)
            continue;
        if (c == QLatin1Char('\\') || c == QLatin1Char('_') || c == QLatin1Char('*')) {
            ++i; // escaped / padding character
            continue;
        }
        if (c == QLatin1Char(';'))
            break;
        if (c == QLatin1Char('[')) {
            const int close = code.indexOf(QLatin1Char(']'), i + 1);
            if (close < 0)
                break;
            const QString inner = code.mid(i + 1, close - i - 1).toLower();
            static const QRegularExpression kElapsed(QStringLiteral("^(h+|m+|s+)$"));
            if (kElapsed.match(inner).hasMatch())
                return FormatKind(); // elapsed time ([h]:mm) is a duration, keep the number
            i = close;
            continue;
        }
        s += c;
    }
    s = s.toLower();
    s.remove(QStringLiteral("general"));
    static const QRegularExpression kFracSec(QStringLiteral("s\\.0+"));
    s.replace(kFracSec, QStringLiteral("s"));
    if (s.contains(QLatin1Char('0')) || s.contains(QLatin1Char('#'))
        || s.contains(QLatin1Char('?')) || s.contains(QLatin1Char('@')))
        return FormatKind();
    const bool hasH = s.contains(QLatin1Char('h'));
    const bool hasS = s.contains(QLatin1Char('s'));
    const bool hasM = s.contains(QLatin1Char('m'));
    const bool hasYD = s.contains(QLatin1Char('y')) || s.contains(QLatin1Char('d'))
            || s.contains(QLatin1Char('g')) || s.contains(QLatin1Char('e'));
    k.timePart = hasH || hasS || s.contains(QStringLiteral("am/pm"));
    k.datePart = hasYD || (hasM && !k.timePart);
    k.isDate = k.timePart || k.datePart;
    return k;
}

FormatKind classifyBuiltinFormat(int id)
{
    FormatKind k;
    if ((id >= 14 && id <= 17) || (id >= 27 && id <= 31) || (id >= 34 && id <= 36)
        || (id >= 50 && id <= 58)) {
        k.isDate = k.datePart = true;
    } else if ((id >= 18 && id <= 21) || id == 32 || id == 33 || id == 45 || id == 47) {
        k.isDate = k.timePart = true;
    } else if (id == 22) {
        k.isDate = k.datePart = k.timePart = true;
    }
    return k;
}

QVector<FormatKind> parseStyles(const QByteArray &xml)
{
    QHash<int, QString> customFormats;
    QVector<int> xfFormatIds;
    QXmlStreamReader r(xml);
    bool inCellXfs = false;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType tt = r.readNext();
        if (tt == QXmlStreamReader::StartElement) {
            if (r.name() == QLatin1String("numFmt")) {
                const QXmlStreamAttributes a = r.attributes();
                customFormats.insert(a.value(QLatin1String("numFmtId")).toInt(),
                                     a.value(QLatin1String("formatCode")).toString());
            } else if (r.name() == QLatin1String("cellXfs")) {
                inCellXfs = true;
            } else if (inCellXfs && r.name() == QLatin1String("xf")) {
                xfFormatIds << r.attributes().value(QLatin1String("numFmtId")).toInt();
            }
        } else if (tt == QXmlStreamReader::EndElement && r.name() == QLatin1String("cellXfs")) {
            inCellXfs = false;
        }
    }
    QVector<FormatKind> kinds;
    kinds.reserve(xfFormatIds.size());
    for (int id : xfFormatIds) {
        auto it = customFormats.constFind(id);
        kinds << ((it != customFormats.constEnd()) ? classifyFormatCode(it.value())
                                                   : classifyBuiltinFormat(id));
    }
    return kinds;
}

// ---------------------------------------------------------------------------
// Worksheet
// ---------------------------------------------------------------------------
// "B12" -> col 1, row 11. Returns false for malformed references.
bool parseCellRef(const QStringRef &ref, int &col, int &row)
{
    int i = 0;
    int c = 0;
    while (i < ref.size() && ref.at(i).isLetter()) {
        const int ch = ref.at(i).toUpper().unicode();
        if (ch < 'A' || ch > 'Z')
            return false;
        c = c * 26 + (ch - 'A' + 1);
        if (c > kMaxCols)
            return false;
        ++i;
    }
    if (i == 0 || i >= ref.size())
        return false;
    bool ok = false;
    const int rr = ref.mid(i).toInt(&ok);
    if (!ok || rr < 1 || rr > kMaxRows)
        return false;
    col = c - 1;
    row = rr - 1;
    return true;
}

double dateTimeToSerial(const QDateTime &dt, bool date1904)
{
    QDate base;
    if (date1904)
        base = QDate(1904, 1, 1);
    else if (dt.date() >= QDate(1900, 1, 1) && dt.date() < QDate(1900, 3, 1))
        base = QDate(1899, 12, 31);
    else
        base = QDate(1899, 12, 30);
    const qint64 days = base.daysTo(dt.date());
    return double(days) + double(dt.time().msecsSinceStartOfDay()) / 86400000.0;
}

bool parseWorksheet(const QByteArray &xml, const QStringList &sharedStrings,
                    const QVector<FormatKind> &styles, bool date1904, Sheet &sheet,
                    QString *outErr)
{
    QXmlStreamReader r(xml);
    int curRow = -1;
    int curCol = -1;
    qint64 allocatedCells = 0;
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType tt = r.readNext();
        if (tt != QXmlStreamReader::StartElement)
            continue;
        if (r.name() == QLatin1String("row")) {
            bool ok = false;
            const int rr = r.attributes().value(QLatin1String("r")).toInt(&ok);
            curRow = (ok && rr >= 1 && rr <= kMaxRows) ? rr - 1 : curRow + 1;
            curCol = -1;
            continue;
        }
        if (r.name() != QLatin1String("c"))
            continue;

        const QXmlStreamAttributes a = r.attributes();
        int col = curCol + 1;
        int row = std::max(0, curRow);
        if (a.hasAttribute(QLatin1String("r"))) {
            int pc = 0, pr = 0;
            if (parseCellRef(a.value(QLatin1String("r")), pc, pr)) {
                col = pc;
                row = pr;
            }
        }
        curCol = col;
        if (curRow < 0)
            curRow = row;
        const QString type = a.value(QLatin1String("t")).toString();
        bool styleOk = false;
        const int styleIdx = a.value(QLatin1String("s")).toInt(&styleOk);

        QString valueText;
        QString inlineText;
        bool hasValue = false;
        bool hasInline = false;
        while (!r.atEnd()) {
            const QXmlStreamReader::TokenType ct = r.readNext();
            if (ct == QXmlStreamReader::StartElement) {
                if (r.name() == QLatin1String("v")) {
                    valueText = r.readElementText(QXmlStreamReader::IncludeChildElements);
                    hasValue = true;
                } else if (r.name() == QLatin1String("is")) {
                    inlineText = readRichText(r);
                    hasInline = true;
                } else {
                    r.skipCurrentElement(); // <f>, <extLst>
                }
            } else if (ct == QXmlStreamReader::EndElement && r.name() == QLatin1String("c")) {
                break;
            }
        }

        Cell cell;
        if (type == QLatin1String("s")) {
            bool ok = false;
            const int idx = valueText.trimmed().toInt(&ok);
            if (ok && idx >= 0 && idx < sharedStrings.size()) {
                cell.type = Cell::Text;
                cell.text = sharedStrings.at(idx);
            }
        } else if (type == QLatin1String("inlineStr")) {
            if (hasInline || hasValue) {
                cell.type = Cell::Text;
                cell.text = hasInline ? inlineText : valueText;
            }
        } else if (type == QLatin1String("str")) {
            if (hasValue) {
                cell.type = Cell::Text;
                cell.text = valueText;
            }
        } else if (type == QLatin1String("b")) {
            if (hasValue) {
                cell.type = Cell::Bool;
                cell.number = (valueText.trimmed() == QLatin1String("1")) ? 1.0 : 0.0;
            }
        } else if (type == QLatin1String("e")) {
            cell.type = Cell::Error;
            cell.text = valueText;
        } else if (type == QLatin1String("d")) {
            QDateTime dt = QDateTime::fromString(valueText.trimmed(), Qt::ISODate);
            if (dt.isValid()) {
                cell.type = Cell::Number;
                cell.number = dateTimeToSerial(dt, date1904);
                cell.isDate = true;
                cell.hasDatePart = true;
                cell.hasTimePart = dt.time().msecsSinceStartOfDay() != 0;
            } else if (!valueText.isEmpty()) {
                cell.type = Cell::Text;
                cell.text = valueText;
            }
        } else if (hasValue && !valueText.trimmed().isEmpty()) {
            bool ok = false;
            const double v = QLocale::c().toDouble(valueText.trimmed(), &ok);
            if (ok && std::isfinite(v)) {
                cell.type = Cell::Number;
                cell.number = v;
                if (styleOk && styleIdx >= 0 && styleIdx < styles.size()
                    && styles.at(styleIdx).isDate) {
                    cell.isDate = true;
                    cell.hasDatePart = styles.at(styleIdx).datePart;
                    cell.hasTimePart = styles.at(styleIdx).timePart;
                }
            } else if (ok) {
                cell.type = Cell::Error;
                cell.text = valueText;
            } else {
                cell.type = Cell::Text;
                cell.text = valueText;
            }
        }
        if (cell.type == Cell::Empty)
            continue;

        if (row >= sheet.rows.size())
            sheet.rows.resize(row + 1);
        QVector<Cell> &rowCells = sheet.rows[row];
        if (col >= rowCells.size()) {
            const qint64 delta = qint64(col + 1) - qint64(rowCells.size());
            if (allocatedCells + delta > kMaxTotalCells * 2) {
                if (outErr) {
                    *outErr = trX("Worksheet \"%1\" exceeds the maximum import size "
                                  "(20,000,000 cells).")
                                      .arg(sheet.name);
                }
                return false;
            }
            allocatedCells += delta;
            rowCells.resize(col + 1);
        }
        rowCells[col] = cell;
        sheet.columnCount = std::max(sheet.columnCount, col + 1);
    }
    if (r.hasError()) {
        if (outErr)
            *outErr = QStringLiteral("XML error in sheet '%1' (line %2): %3")
                              .arg(sheet.name)
                              .arg(r.lineNumber())
                              .arg(r.errorString());
        return false;
    }
    return true;
}

bool looksLikeEncryptedOoxml(const QByteArray &data)
{
    // An encrypted (password-to-open) xlsx is an OLE compound file holding the streams
    // "EncryptionInfo" and "EncryptedPackage" (names stored as UTF-16LE).
    static const QByteArray kName = []() {
        QByteArray n;
        for (char ch : QByteArray("EncryptedPackage")) {
            n.append(ch);
            n.append('\0');
        }
        return n;
    }();
    return data.contains(kName);
}

QString damagedMessage(const QString &detail)
{
    return trX("The file is damaged or is not an Excel workbook (.xlsx).\n"
               "Open it in Excel and save it again as an Excel Workbook (.xlsx), or save the "
               "data as CSV and import that file.\n\nDetails: %1")
            .arg(detail);
}

// ---------------------------------------------------------------------------
// Sheet -> columns
// ---------------------------------------------------------------------------
QString excelColumnLetters(int col)
{
    QString s;
    int c = col + 1;
    while (c > 0) {
        const int rem = (c - 1) % 26;
        s.prepend(QChar('A' + rem));
        c = (c - 1) / 26;
    }
    return s;
}

const Cell &cellAt(const Sheet &sheet, int row, int col)
{
    static const Cell kEmpty;
    if (row < 0 || row >= sheet.rows.size())
        return kEmpty;
    const QVector<Cell> &cells = sheet.rows.at(row);
    return (col >= 0 && col < cells.size()) ? cells.at(col) : kEmpty;
}

bool isBlank(const Cell &c)
{
    return c.type == Cell::Empty || (c.type == Cell::Text && c.text.trimmed().isEmpty());
}

bool numericValue(const Cell &c, double &out)
{
    if (c.type == Cell::Number || c.type == Cell::Bool) {
        if (!std::isfinite(c.number))
            return false;
        out = c.number;
        return true;
    }
    if (c.type == Cell::Text) {
        bool ok = false;
        out = QLocale::c().toDouble(c.text.trimmed(), &ok);
        return ok && std::isfinite(out);
    }
    return false;
}

bool isNumericCell(const Cell &c)
{
    double v = 0.0;
    return numericValue(c, v);
}

bool isTextualCell(const Cell &c)
{
    return c.type == Cell::Text && !isBlank(c) && !isNumericCell(c);
}

QString cellDisplayText(const Cell &c, bool date1904)
{
    switch (c.type) {
    case Cell::Number:
        if (c.isDate) {
            const QDateTime dt = serialToDateTime(c.number, date1904);
            if (dt.isValid()) {
                if (c.hasDatePart && !c.hasTimePart)
                    return dt.date().toString(Qt::ISODate);
                if (!c.hasDatePart)
                    return dt.time().toString(QStringLiteral("HH:mm:ss"));
                return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            }
        }
        return std::isfinite(c.number) ? QString::number(c.number, 'g', 15) : QString();
    case Cell::Bool:
        return c.number != 0.0 ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    case Cell::Text:
    case Cell::Error:
        return c.text;
    case Cell::Empty:
        break;
    }
    return QString();
}

QString sanitizeWindowName(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (const QChar ch : raw) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-'))
            out += ch;
        else
            out += QLatin1Char('_');
    }
    static const QRegularExpression kRuns(QStringLiteral("_+"));
    out.replace(kRuns, QStringLiteral("_"));
    while (out.startsWith(QLatin1Char('_')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('_')))
        out.chop(1);
    return out;
}

Table *findBlankInitialTable(ApplicationWindow *app)
{
    if (!app || app->projectname != QLatin1String("untitled"))
        return nullptr;
    const QList<MyWidget *> allWins = app->windowsList();
    if (allWins.size() != 1)
        return nullptr;
    Table *cand = qobject_cast<Table *>(allWins.first());
    if (!cand || !cand->d_future_table || !cand->windowLabel().isEmpty()
        || !app->dependingPlots(cand->name()).isEmpty() || cand->columnCount() > 2)
        return nullptr;
    for (int c = 0; c < cand->columnCount(); ++c) {
        Column *col = cand->column(c);
        if (!col || !col->longName().isEmpty() || !col->units().isEmpty()
            || !col->comment().isEmpty() || !col->formulaIntervals().isEmpty())
            return nullptr;
        for (int r = 0; r < col->rowCount(); ++r) {
            const bool empty = (col->columnMode() == SciDAVis::ColumnMode::Numeric)
                    ? col->isInvalid(r)
                    : col->textAt(r).isEmpty();
            if (!empty)
                return nullptr;
        }
    }
    return cand;
}

QString uniqueWindowName(ApplicationWindow *app, const QString &base)
{
    QSet<QString> used;
    for (MyWidget *w : app->windowsList()) {
        if (w)
            used.insert(w->name());
    }
    if (!used.contains(base))
        return base;
    for (int i = 2;; ++i) {
        const QString cand = base + QStringLiteral("_%1").arg(i);
        if (!used.contains(cand))
            return cand;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
QDateTime serialToDateTime(double serial, bool date1904)
{
    if (!std::isfinite(serial) || std::fabs(serial) > kMaxAbsDateSerial)
        return QDateTime();
    QDate base;
    if (date1904)
        base = QDate(1904, 1, 1);
    else if (serial < 60.0)
        base = QDate(1899, 12, 31); // before Excel's fictitious 1900-02-29 (serial 60)
    else
        base = QDate(1899, 12, 30);
    const double scaledMs = serial * 86400000.0;
    if (!std::isfinite(scaledMs)
        || std::fabs(scaledMs) > double(std::numeric_limits<qint64>::max() / 4))
        return QDateTime();
    qint64 totalMs = static_cast<qint64>(std::llround(scaledMs));
    qint64 days = totalMs / 86400000LL;
    qint64 ms = totalMs % 86400000LL;
    if (ms < 0) {
        ms += 86400000LL;
        --days;
    }
    const QDate outDate = base.addDays(days);
    if (!outDate.isValid())
        return QDateTime();
    return QDateTime(outDate, QTime::fromMSecsSinceStartOfDay(static_cast<int>(ms)));
}

bool readWorkbookData(const QByteArray &data, Workbook &out, QString *outError,
                      const QString &onlySheet)
{
    out = Workbook();
    auto fail = [outError](const QString &msg) {
        if (outError)
            *outError = msg;
        return false;
    };

    if (data.isEmpty())
        return fail(damagedMessage(QStringLiteral("empty file")));
    if (data.startsWith(QByteArray::fromHex("D0CF11E0A1B11AE1"))) {
        if (looksLikeEncryptedOoxml(data)) {
            return fail(trX("This workbook is protected with a password (encrypted), so it "
                            "cannot be read.\nIn Excel, open it, remove the password (File > "
                            "Info > Protect Workbook > Encrypt with Password, clear the box), "
                            "save it and import it again."));
        }
        return fail(trX("This is an old Excel 97-2003 workbook (.xls), which cannot be read "
                        "directly.\nIn Excel, save it as an Excel Workbook (.xlsx) or as CSV and "
                        "import that file."));
    }

    torizip::ZipArchive zip;
    QString err;
    if (!zip.openData(data, &err))
        return fail(damagedMessage(err));

    // Workbook part from the package relationships (normally xl/workbook.xml)
    QString workbookPart = QStringLiteral("xl/workbook.xml");
    {
        QByteArray relsXml;
        if (readPart(zip, QStringLiteral("_rels/.rels"), relsXml, nullptr)) {
            const QHash<QString, Relationship> rels = parseRelationships(relsXml, QString());
            for (const Relationship &rel : rels) {
                if (typeEndsWith(rel.type, "/officeDocument") && !rel.target.isEmpty()) {
                    workbookPart = rel.target;
                    break;
                }
            }
        }
    }
    QByteArray workbookXml;
    bool missing = false;
    if (!readPart(zip, workbookPart, workbookXml, &err, &missing)) {
        if (missing && zip.findExactEntry(QStringLiteral("content.xml")) >= 0
            && zip.findExactEntry(QStringLiteral("mimetype")) >= 0) {
            return fail(trX("This is an OpenDocument spreadsheet (.ods), which cannot be read "
                            "directly.\nSave it as an Excel Workbook (.xlsx) or as CSV and import "
                            "that file."));
        }
        bool anyEncrypted = false;
        for (const torizip::ZipEntry &e : zip.entries())
            anyEncrypted = anyEncrypted || e.isEncrypted();
        if (anyEncrypted) {
            return fail(trX("This workbook is protected with a password (encrypted), so it "
                            "cannot be read.\nIn Excel, open it, remove the password (File > "
                            "Info > Protect Workbook > Encrypt with Password, clear the box), "
                            "save it and import it again."));
        }
        return fail(damagedMessage(err));
    }
    const QString workbookDir = dirOfPart(workbookPart);

    QHash<QString, Relationship> wbRels;
    {
        const QString relsPart = (workbookDir.isEmpty() ? QString() : workbookDir + QLatin1Char('/'))
                + QStringLiteral("_rels/") + workbookPart.section(QLatin1Char('/'), -1)
                + QStringLiteral(".rels");
        QByteArray relsXml;
        if (readPart(zip, relsPart, relsXml, nullptr))
            wbRels = parseRelationships(relsXml, workbookDir);
    }

    // Shared strings and styles
    QString sstPart, stylesPart;
    for (const Relationship &rel : wbRels) {
        if (typeEndsWith(rel.type, "/sharedStrings"))
            sstPart = rel.target;
        else if (typeEndsWith(rel.type, "/styles"))
            stylesPart = rel.target;
    }
    if (sstPart.isEmpty())
        sstPart = resolvePartPath(workbookDir, QStringLiteral("sharedStrings.xml"));
    if (stylesPart.isEmpty())
        stylesPart = resolvePartPath(workbookDir, QStringLiteral("styles.xml"));

    QStringList sharedStrings;
    {
        QByteArray xml;
        if (readPart(zip, sstPart, xml, &err, &missing))
            sharedStrings = parseSharedStrings(xml);
        else if (!missing)
            return fail(damagedMessage(err));
    }
    QVector<FormatKind> styles;
    {
        QByteArray xml;
        if (readPart(zip, stylesPart, xml, &err, &missing))
            styles = parseStyles(xml);
        else if (!missing)
            return fail(damagedMessage(err));
    }

    // Sheet list in tab order
    struct SheetRef
    {
        QString name, state, relId;
    };
    QVector<SheetRef> sheetRefs;
    {
        QXmlStreamReader r(workbookXml);
        while (!r.atEnd()) {
            if (r.readNext() != QXmlStreamReader::StartElement)
                continue;
            if (r.name() == QLatin1String("workbookPr")) {
                const QStringRef v = r.attributes().value(QLatin1String("date1904"));
                out.date1904 = (v == QLatin1String("1") || v == QLatin1String("true"));
            } else if (r.name() == QLatin1String("sheet")) {
                SheetRef ref;
                for (const QXmlStreamAttribute &attr : r.attributes()) {
                    if (attr.name() == QLatin1String("name") && attr.namespaceUri().isEmpty())
                        ref.name = attr.value().toString();
                    else if (attr.name() == QLatin1String("state"))
                        ref.state = attr.value().toString();
                    else if (attr.name() == QLatin1String("id") && !attr.namespaceUri().isEmpty())
                        ref.relId = attr.value().toString();
                }
                sheetRefs << ref;
            }
        }
        if (r.hasError())
            return fail(damagedMessage(QStringLiteral("workbook.xml: %1").arg(r.errorString())));
    }

    bool foundRequested = onlySheet.isEmpty();
    QStringList allNames;
    for (int si = 0; si < sheetRefs.size(); ++si) {
        const SheetRef &ref = sheetRefs.at(si);
        allNames << ref.name;
        auto relIt = wbRels.constFind(ref.relId);
        QString part;
        if (relIt != wbRels.constEnd()) {
            if (!typeEndsWith(relIt->type, "/worksheet")) {
                if (!onlySheet.isEmpty()
                    && ref.name.compare(onlySheet, Qt::CaseInsensitive) == 0) {
                    return fail(trX("Sheet \"%1\" is a chart or macro sheet and contains no "
                                    "worksheet table data.")
                                        .arg(ref.name));
                }
                ++out.skippedNonWorksheets; // chart sheet, dialog sheet, macro sheet
                continue;
            }
            part = relIt->target;
        } else {
            part = resolvePartPath(workbookDir,
                                   QStringLiteral("worksheets/sheet%1.xml").arg(si + 1));
        }
        if (!onlySheet.isEmpty() && ref.name.compare(onlySheet, Qt::CaseInsensitive) != 0)
            continue;
        foundRequested = true;

        QByteArray xml;
        if (!readPart(zip, part, xml, &err))
            return fail(damagedMessage(err));
        Sheet sheet;
        sheet.name = ref.name;
        sheet.state = ref.state;
        if (!parseWorksheet(xml, sharedStrings, styles, out.date1904, sheet, &err)) {
            if (err.contains(QStringLiteral("20,000,000")))
                return fail(err);
            return fail(damagedMessage(err));
        }
        out.sheets << sheet;
    }
    if (!foundRequested) {
        return fail(trX("Sheet \"%1\" was not found in the workbook.\nSheets: %2")
                            .arg(onlySheet, allNames.join(QStringLiteral(", "))));
    }
    if (sheetRefs.isEmpty())
        return fail(damagedMessage(QStringLiteral("the workbook lists no sheets")));
    return true;
}

bool readWorkbook(const QString &path, Workbook &out, QString *outError, const QString &onlySheet)
{
    QFile f(path);
    if (!f.exists()) {
        if (outError)
            *outError = trX("File not found: %1").arg(path);
        return false;
    }
    if (f.size() > torizip::kMaxArchiveBytes) {
        if (outError)
            *outError = trX("The file is too large to import (over 512 MB): %1").arg(path);
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (outError)
            *outError = trX("Cannot open %1: %2").arg(path, f.errorString());
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();
    return readWorkbookData(data, out, outError, onlySheet);
}

QList<Column *> sheetToColumns(const Sheet &sheet, bool date1904, SheetLayout *outLayout)
{
    SheetLayout layout;
    QList<Column *> cols;

    // Used range & non-empty columns (avoids 131 GB OOM when a stray cell sits at e.g. XFD1048576)
    int firstRow = -1, lastRow = -1, firstCol = std::numeric_limits<int>::max(), lastCol = -1;
    QSet<int> nonEmptyColSet;
    for (int r = 0; r < sheet.rows.size(); ++r) {
        const QVector<Cell> &cells = sheet.rows.at(r);
        for (int c = 0; c < cells.size(); ++c) {
            if (isBlank(cells.at(c)))
                continue;
            if (firstRow < 0)
                firstRow = r;
            lastRow = r;
            firstCol = std::min(firstCol, c);
            lastCol = std::max(lastCol, c);
            nonEmptyColSet.insert(c);
        }
    }
    if (firstRow < 0 || nonEmptyColSet.isEmpty()) {
        if (outLayout)
            *outLayout = layout;
        return cols;
    }

    const int spanCols = lastCol - firstCol + 1;
    const qint64 rawSpanCells = qint64(lastRow - firstRow + 1) * qint64(spanCols);
    QVector<int> targetCols;
    if (spanCols <= 64 && rawSpanCells <= kMaxTotalCells) {
        targetCols.reserve(spanCols);
        for (int c = firstCol; c <= lastCol; ++c)
            targetCols.append(c);
    } else {
        targetCols.reserve(nonEmptyColSet.size());
        for (int c : nonEmptyColSet)
            targetCols.append(c);
        std::sort(targetCols.begin(), targetCols.end());
    }

    auto rowHasNumeric = [&](int r) {
        for (int c : targetCols)
            if (isNumericCell(cellAt(sheet, r, c)))
                return true;
        return false;
    };
    auto rowHasTextual = [&](int r) {
        for (int c : targetCols)
            if (isTextualCell(cellAt(sheet, r, c)))
                return true;
        return false;
    };
    auto countRowNonBlank = [&](int r) {
        int cnt = 0;
        for (int c : targetCols)
            if (!isBlank(cellAt(sheet, r, c)))
                ++cnt;
        return cnt;
    };
    // Every non-blank cell of rows [from, lastRow] in column c is a number, and there is one.
    auto columnNumericFrom = [&](int c, int from) {
        bool any = false;
        for (int r = from; r <= lastRow; ++r) {
            const Cell &cell = cellAt(sheet, r, c);
            if (isBlank(cell) || cell.type == Cell::Error)
                continue;
            if (!isNumericCell(cell))
                return false;
            any = true;
        }
        return any;
    };

    auto columnMostlyNumericFrom = [&](int c, int from) {
        int nums = 0, texts = 0;
        for (int r = from; r <= lastRow; ++r) {
            const Cell &cell = cellAt(sheet, r, c);
            if (isBlank(cell) || cell.type == Cell::Error)
                continue;
            if (isNumericCell(cell))
                ++nums;
            else
                ++texts;
        }
        return nums > 0 && nums >= texts;
    };

    int firstNumericRow = -1;
    for (int r = firstRow; r <= lastRow; ++r) {
        if (rowHasNumeric(r)) {
            firstNumericRow = r;
            break;
        }
    }

    // Detect data start row even when row 1+ contains title / comment preamble lines.
    int detectedDataStart = -1;
    if (firstNumericRow >= 0) {
        for (int r = firstNumericRow; r <= lastRow; ++r) {
            if (!rowHasNumeric(r))
                continue;
            bool hasCleanNumericCol = false;
            for (int c : targetCols) {
                if (columnNumericFrom(c, r)) {
                    hasCleanNumericCol = true;
                    break;
                }
            }
            if (hasCleanNumericCol) {
                detectedDataStart = r;
                break;
            }
        }
        if (detectedDataStart < 0) {
            detectedDataStart = firstNumericRow;
        } else if (detectedDataStart > firstNumericRow) {
            for (int r = firstNumericRow; r < detectedDataStart; ++r) {
                int nonBlankCnt = 0;
                int numericCnt = 0;
                for (int c : targetCols) {
                    const Cell &cell = cellAt(sheet, r, c);
                    if (isBlank(cell))
                        continue;
                    ++nonBlankCnt;
                    if (isNumericCell(cell))
                        ++numericCnt;
                }
                if (nonBlankCnt > 0 && numericCnt * 2 >= nonBlankCnt) {
                    detectedDataStart = firstNumericRow;
                    break;
                }
            }
        }
    }

    bool hasHeader = false;
    bool hasUnits = false;
    int headerRowIdx = firstRow;
    int unitsRowIdx = -1;
    int dataStart = firstRow;

    if (detectedDataStart > firstRow) {
        dataStart = detectedDataStart;
        QVector<int> candRows;
        for (int r = firstRow; r < dataStart; ++r) {
            if (countRowNonBlank(r) > 0)
                candRows.append(r);
        }
        if (candRows.size() == 1) {
            const int rHdr = candRows.first();
            if (rowHasTextual(rHdr) && !rowHasNumeric(rHdr)) {
                hasHeader = true;
                headerRowIdx = rHdr;
            }
        } else if (candRows.size() >= 2) {
            const int rLast = candRows.at(candRows.size() - 1);
            const int rPrev = candRows.at(candRows.size() - 2);
            const bool lastTextOnly = rowHasTextual(rLast) && !rowHasNumeric(rLast);
            const bool prevTextOnly = rowHasTextual(rPrev) && !rowHasNumeric(rPrev);
            bool lastIsUnits = false;
            if (lastTextOnly && prevTextOnly) {
                auto scanUnitRowCols = [&](bool allowMissingMarkers) {
                    int numericCols = 0, nonEmptyUnits = 0, recognized = 0;
                    bool allRecognized = true, shortCells = true, hasInlineHeaderUnit = false;
                    for (int c : targetCols) {
                        const bool okCol = allowMissingMarkers
                                ? columnMostlyNumericFrom(c, dataStart)
                                : columnNumericFrom(c, dataStart);
                        if (!okCol)
                            continue;
                        ++numericCols;
                        const Cell &uc = cellAt(sheet, rLast, c);
                        if (isBlank(uc))
                            continue;
                        ++nonEmptyUnits;
                        const QString rawTxt = cellDisplayText(uc, date1904).trimmed();
                        QString sn, ln, un;
                        AsciiTableImportFilter::splitHeaderNameAndUnits(rawTxt, sn, ln, un);
                        if (!ln.isEmpty() && !un.isEmpty() && ln != rawTxt)
                            hasInlineHeaderUnit = true;
                        QString cleaned;
                        if (AsciiTableImportFilter::looksLikeUnitCell(rawTxt, &cleaned))
                            ++recognized;
                        else
                            allRecognized = false;
                        if (cleaned.size() > 32)
                            shortCells = false;
                    }
                    const int nonBlankPrev = countRowNonBlank(rPrev);
                    const int nonBlankLast = countRowNonBlank(rLast);
                    const bool isUnits = !hasInlineHeaderUnit && numericCols >= 1
                            && ((allRecognized && recognized >= 1)
                                || (nonEmptyUnits >= 1 && shortCells && nonBlankPrev >= nonBlankLast
                                    && !(nonBlankPrev == 1 && targetCols.size() >= 2)));
                    return std::make_pair(numericCols, isUnits);
                };
                std::pair<int, bool> unitScan = scanUnitRowCols(false);
                if (unitScan.first == 0)
                    unitScan = scanUnitRowCols(true);
                lastIsUnits = unitScan.second;
            }
            if (lastIsUnits) {
                hasHeader = true;
                headerRowIdx = rPrev;
                hasUnits = true;
                unitsRowIdx = rLast;
            } else if (lastTextOnly) {
                hasHeader = true;
                headerRowIdx = rLast;
            } else if (prevTextOnly) {
                hasHeader = true;
                headerRowIdx = rPrev;
            }
        }
    }

    const int nRows = std::max(0, lastRow - dataStart + 1);
    layout.headerRow = hasHeader;
    layout.unitsRow = hasUnits;
    layout.dataRows = nRows;

    if (qint64(nRows) * qint64(targetCols.size()) > kMaxTotalCells) {
        layout.dataRows = -1;
        if (outLayout)
            *outLayout = layout;
        return cols;
    }

    QSet<QString> usedNames;
    QVector<bool> colIsNumeric;
    for (int c : targetCols) {
        int nums = 0, dates = 0, texts = 0;
        bool datePart = false, timePart = false;
        for (int r = dataStart; r <= lastRow; ++r) {
            const Cell &cell = cellAt(sheet, r, c);
            if (isBlank(cell) || cell.type == Cell::Error)
                continue;
            if (isNumericCell(cell)) {
                ++nums;
                if (cell.type == Cell::Number && cell.isDate) {
                    ++dates;
                    datePart = datePart || cell.hasDatePart;
                    timePart = timePart || cell.hasTimePart;
                }
            } else {
                ++texts;
            }
        }
        const SciDAVis::ColumnMode mode = (texts > 0)
                ? SciDAVis::ColumnMode::Text
                : ((nums > 0 && dates == nums) ? SciDAVis::ColumnMode::DateTime
                                               : SciDAVis::ColumnMode::Numeric);

        // Names and units
        QString shortName, longName, units;
        if (hasHeader) {
            const Cell &hc = cellAt(sheet, headerRowIdx, c);
            const QString raw = isBlank(hc) ? QString() : cellDisplayText(hc, date1904).trimmed();
            if (!raw.isEmpty()) {
                AsciiTableImportFilter::splitHeaderNameAndUnits(raw, shortName, longName, units);
                if (longName.isEmpty())
                    longName = units.isEmpty() ? raw : shortName;
            }
        }
        if (hasUnits && unitsRowIdx >= 0) {
            const Cell &uc = cellAt(sheet, unitsRowIdx, c);
            if (!isBlank(uc)) {
                QString cleaned;
                AsciiTableImportFilter::looksLikeUnitCell(cellDisplayText(uc, date1904), &cleaned);
                if (!cleaned.isEmpty())
                    units = cleaned;
            }
        }
        if (shortName.trimmed().isEmpty())
            shortName = excelColumnLetters(c);
        shortName.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
        if (usedNames.contains(shortName)) {
            int k = 2;
            while (usedNames.contains(shortName + QStringLiteral("_%1").arg(k)))
                ++k;
            shortName += QStringLiteral("_%1").arg(k);
        }
        usedNames.insert(shortName);

        Column *col = nullptr;
        if (mode == SciDAVis::ColumnMode::Numeric) {
            std::unique_ptr<QVector<qreal>> values(
                    new QVector<qreal>(nRows, std::numeric_limits<double>::quiet_NaN()));
            IntervalAttribute<bool> invalid;
            int runStart = -1;
            for (int i = 0; i < nRows; ++i) {
                double v = 0.0;
                const Cell &cell = cellAt(sheet, dataStart + i, c);
                const bool ok = !isBlank(cell) && numericValue(cell, v);
                if (ok) {
                    (*values)[i] = v;
                    if (runStart >= 0) {
                        invalid.setValue(Interval<int>(runStart, i - 1), true);
                        runStart = -1;
                    }
                } else if (runStart < 0) {
                    runStart = i;
                }
            }
            if (runStart >= 0 && nRows > 0)
                invalid.setValue(Interval<int>(runStart, nRows - 1), true);
            col = new Column(shortName, std::move(values), invalid);
            if (auto *d2s = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
                d2s->setNumericFormat('g');
                d2s->setNumDigits(6);
            }
            ++layout.numericColumns;
        } else if (mode == SciDAVis::ColumnMode::DateTime) {
            std::unique_ptr<QList<QDateTime>> values(new QList<QDateTime>());
            IntervalAttribute<bool> invalid;
            for (int i = 0; i < nRows; ++i) {
                const Cell &cell = cellAt(sheet, dataStart + i, c);
                if (cell.type == Cell::Number && cell.isDate) {
                    const QDateTime dt = serialToDateTime(cell.number, date1904);
                    values->append(dt);
                    if (!dt.isValid())
                        invalid.setValue(Interval<int>(i, i), true);
                } else {
                    values->append(QDateTime());
                    invalid.setValue(Interval<int>(i, i), true);
                }
            }
            col = new Column(shortName, std::move(values), invalid);
            QString fmt;
            if (datePart && timePart)
                fmt = QStringLiteral("yyyy-MM-dd hh:mm:ss");
            else if (timePart)
                fmt = QStringLiteral("hh:mm:ss");
            else
                fmt = QStringLiteral("yyyy-MM-dd");
            if (auto *dt2s = qobject_cast<DateTime2StringFilter *>(col->outputFilter()))
                dt2s->setFormat(fmt);
            ++layout.dateColumns;
        } else {
            std::unique_ptr<QStringList> values(new QStringList());
            values->reserve(nRows);
            for (int i = 0; i < nRows; ++i) {
                const Cell &cell = cellAt(sheet, dataStart + i, c);
                values->append(isBlank(cell) ? QString() : cellDisplayText(cell, date1904));
            }
            col = new Column(shortName, std::move(values), IntervalAttribute<bool>());
            ++layout.textColumns;
        }
        if (!longName.isEmpty())
            col->setLongName(longName);
        if (!units.isEmpty())
            col->setUnits(units);
        colIsNumeric << (mode != SciDAVis::ColumnMode::Text);
        cols << col;
    }

    // Plot designations as for CSV files: a text first column followed by numbers keeps no
    // designation and the next column becomes X.
    const int n = cols.size();
    if (n >= 3 && !colIsNumeric[0] && colIsNumeric[1] && colIsNumeric[2]) {
        cols[0]->setPlotDesignation(SciDAVis::noDesignation);
        cols[1]->setPlotDesignation(SciDAVis::X);
        for (int c = 2; c < n; ++c)
            cols[c]->setPlotDesignation(SciDAVis::Y);
    } else {
        for (int c = 0; c < n; ++c)
            cols[c]->setPlotDesignation(c == 0 ? SciDAVis::X : SciDAVis::Y);
    }

    if (outLayout)
        *outLayout = layout;
    return cols;
}

QString tableBaseName(const QString &bookBaseName, const QString &sheetName)
{
    QString book = sanitizeWindowName(bookBaseName);
    QString sheet = sanitizeWindowName(sheetName);
    if (book.isEmpty())
        book = QStringLiteral("Book");
    if (sheet.isEmpty())
        return book;
    return book + QLatin1Char('_') + sheet;
}

bool importAsWorkbookSetting()
{
    return ApplicationWindow::getSettings()
            .value(QStringLiteral("/Import/XlsxIntoWorkbook"), true)
            .toBool();
}

void setImportAsWorkbookSetting(bool on)
{
    ApplicationWindow::getSettings().setValue(QStringLiteral("/Import/XlsxIntoWorkbook"), on);
}

bool importIntoProject(ApplicationWindow *app, const QString &path, const QString &sheet,
                       QString *outError, ImportResult *outResult, bool asWorkbook)
{
    ImportResult result;
    if (!app) {
        if (outError)
            *outError = QStringLiteral("ApplicationWindow is null.");
        return false;
    }
    Workbook wb;
    if (!readWorkbook(path, wb, outError, sheet))
        return false;

    const QFileInfo fi(path);
    Table *blankInitial = findBlankInitialTable(app);
    QStringList parts;
    QString oversizedSheetName;
    for (const Sheet &sh : wb.sheets) {
        SheetLayout layout;
        QList<Column *> cols = sheetToColumns(sh, wb.date1904, &layout);
        if (cols.isEmpty()) {
            if (layout.dataRows < 0 && oversizedSheetName.isEmpty())
                oversizedSheetName = sh.name;
            else
                result.emptySheets << sh.name;
            continue;
        }
        if (blankInitial) {
            blankInitial->askOnCloseEvent(false);
            if (blankInitial->d_future_table)
                blankInitial->d_future_table->remove();
            blankInitial = nullptr;
        }
        const QString name =
                uniqueWindowName(app, tableBaseName(fi.completeBaseName(), sh.name));
        Table *t = app->newTable(name, QStringLiteral("%1 [%2]").arg(fi.fileName(), sh.name),
                                 cols);
        if (!t) {
            qDeleteAll(cols);
            continue;
        }
        result.tables << t->name();
        QString part = trX("%1 (%2 rows x %3 columns").arg(sh.name).arg(layout.dataRows).arg(
                cols.size());
        if (layout.unitsRow)
            part += trX(", unit row");
        if (layout.dateColumns > 0)
            part += trX(", %1 date column(s)").arg(layout.dateColumns);
        if (layout.textColumns > 0)
            part += trX(", %1 text column(s)").arg(layout.textColumns);
        part += QLatin1Char(')');
        parts << part;
    }

    if (asWorkbook
        && (result.tables.size() >= 2 || (result.tables.size() == 1 && sheet.isEmpty()))) {
        // D8: one workbook, one sheet tab per worksheet (first sheet shown; 1-sheet books also
        // get the [+] sheet tab bar when importing a whole file)
        QList<Table *> sheets;
        for (const QString &n : result.tables)
            for (MyWidget *w : app->windowsList())
                if (Table *t = qobject_cast<Table *>(w))
                    if (t->name() == n) {
                        sheets << t;
                        break;
                    }
        if (sheets.size() == result.tables.size())
            result.workbook = WorkbookManager::instance(app)->createBookFromTables(
                    tableBaseName(fi.completeBaseName(), QString()), sheets, sheets.first());
    }

    if (result.tables.isEmpty()) {
        if (outError) {
            if (!oversizedSheetName.isEmpty()) {
                *outError = trX("Worksheet \"%1\" exceeds the maximum import size "
                                "(20,000,000 cells).")
                                    .arg(oversizedSheetName);
            } else if (wb.sheets.isEmpty() && wb.skippedNonWorksheets > 0) {
                *outError = trX("This workbook contains only chart or macro sheets and no "
                                "worksheet table data: %1")
                                    .arg(fi.fileName());
            } else {
                *outError = trX("The workbook contains no data (all sheets are empty): %1")
                                    .arg(fi.fileName());
            }
        }
        if (outResult)
            *outResult = result;
        return false;
    }
    result.summary = trX("%1: %2 sheet(s) imported - %3")
                             .arg(fi.fileName())
                             .arg(result.tables.size())
                             .arg(parts.join(QStringLiteral(", ")));
    if (!result.emptySheets.isEmpty())
        result.summary += trX(" / empty sheets skipped: %1")
                                  .arg(result.emptySheets.join(QStringLiteral(", ")));
    app->modifiedProject();
    if (outResult)
        *outResult = result;
    return true;
}

bool importDroppedFile(ApplicationWindow *app, const QString &path, QString *outSummary)
{
    QString err;
    ImportResult res;
    const bool ok = importIntoProject(app, path, QString(), &err, &res, importAsWorkbookSetting());
    if (ok) {
        if (outSummary)
            *outSummary = res.summary;
        app->setProperty("torigraph_last_xlsx_import_error", QString());
        if (!res.tables.isEmpty()) {
            Table *firstTbl = nullptr;
            for (MyWidget *w : app->windowsList()) {
                if (w && w->name() == res.tables.first()) {
                    firstTbl = qobject_cast<Table *>(w);
                    break;
                }
            }
            if (firstTbl && !ApplicationWindow::isNonInteractiveContext(app)) {
                if (!firstTbl->isMaximized() && !firstTbl->isMinimized()
                    && app->d_workspace.viewMode() == QMdiArea::SubWindowView) {
                    const QSize vp = app->d_workspace.viewport()
                            ? app->d_workspace.viewport()->size()
                            : QSize();
                    const int maxW = (vp.width() >= 360) ? qMax(360, vp.width() - 12) : 960;
                    const int maxH = (vp.height() >= 260) ? qMax(260, vp.height() - 12) : 720;
                    const int wantW = qMin(maxW, qMax(firstTbl->width(), 640));
                    const int wantH = qMin(maxH, qMax(firstTbl->height(), 400));
                    if (wantW > firstTbl->width() || wantH > firstTbl->height())
                        firstTbl->resize(wantW, wantH);
                }
                app->offerPlotAfterImport(firstTbl);
            }
        }
        return true;
    }
    const QString msg = err + QStringLiteral("\n\n") + QFileInfo(path).fileName();
    app->setProperty("torigraph_last_xlsx_import_error", err);
    app->setProperty("torigraph_last_drop_unsupported_message", msg);
    if (!ApplicationWindow::isNonInteractiveContext(app))
        QMessageBox::warning(app, trX("Excel Import"), msg);
    else
        app->statusBar()->showMessage(err.section(QLatin1Char('\n'), 0, 0), 15000);
    return false;
}

void importWithDialog(ApplicationWindow *app)
{
    if (!app || ApplicationWindow::isNonInteractiveContext(app))
        return;
    // Qt's own file dialog, so that the workbook choice can sit right in it (D8)
    QFileDialog dlg(app, trX("Import Excel Workbook"), app->asciiDirPath,
                    trX("Excel workbook (*.xlsx *.xlsm)") + QStringLiteral(";;")
                            + trX("All files (*)"));
    dlg.setFileMode(QFileDialog::ExistingFiles);
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    auto *asBook = new QCheckBox(trX("Put the sheets of a file into one workbook (sheet tabs)"),
                                 &dlg);
    asBook->setObjectName(QStringLiteral("torigraph_xlsx_as_workbook"));
    asBook->setToolTip(trX("Off: every sheet becomes a separate worksheet window."));
    asBook->setChecked(importAsWorkbookSetting());
    if (auto *grid = qobject_cast<QGridLayout *>(dlg.layout()))
        grid->addWidget(asBook, grid->rowCount(), 0, 1, grid->columnCount());
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QStringList files = dlg.selectedFiles();
    if (files.isEmpty())
        return;
    setImportAsWorkbookSetting(asBook->isChecked());
    app->asciiDirPath = QFileInfo(files.first()).absolutePath();
    QStringList summaries;
    for (const QString &f : files) {
        QString summary;
        if (importDroppedFile(app, f, &summary))
            summaries << summary;
    }
    if (!summaries.isEmpty())
        app->statusBar()->showMessage(summaries.join(QStringLiteral(" / ")), 15000);
}

void registerTranslations()
{
    static bool s_registered = false;
    if (s_registered)
        return;
    s_registered = true;
    auto reg = [](const char *en, const char *ja) {
        ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                  QString::fromUtf8(ja));
    };
    reg("The file is damaged or is not an Excel workbook (.xlsx).\n"
        "Open it in Excel and save it again as an Excel Workbook (.xlsx), or save the data as "
        "CSV and import that file.\n\nDetails: %1",
        "ファイルが壊れているか、Excel ブック (.xlsx) ではありません。\n"
        "Excel で開いて「Excel ブック (.xlsx)」として保存し直すか、データを CSV で保存して"
        "そのファイルを読み込んでください。\n\n詳細: %1");
    reg("This workbook is protected with a password (encrypted), so it cannot be read.\n"
        "In Excel, open it, remove the password (File > Info > Protect Workbook > Encrypt "
        "with Password, clear the box), save it and import it again.",
        "このブックはパスワードで保護(暗号化)されているため読み込めません。\n"
        "Excel で開き、[ファイル] > [情報] > [ブックの保護] > [パスワードを使用して暗号化] "
        "でパスワードを消して保存してから、もう一度読み込んでください。");
    reg("This is an old Excel 97-2003 workbook (.xls), which cannot be read directly.\n"
        "In Excel, save it as an Excel Workbook (.xlsx) or as CSV and import that file.",
        "これは古い Excel 97-2003 形式のブック (.xls) のため、直接は読み込めません。\n"
        "Excel で「Excel ブック (.xlsx)」または CSV として保存し直し、そのファイルを読み込んで"
        "ください。");
    reg("This is an OpenDocument spreadsheet (.ods), which cannot be read directly.\n"
        "Save it as an Excel Workbook (.xlsx) or as CSV and import that file.",
        "これは OpenDocument 形式の表 (.ods) のため、直接は読み込めません。\n"
        "「Excel ブック (.xlsx)」または CSV として保存し直し、そのファイルを読み込んでください。");
    reg("Sheet \"%1\" was not found in the workbook.\nSheets: %2",
        "シート「%1」がブックにありません。\nシート: %2");
    reg("Sheet \"%1\" is a chart or macro sheet and contains no worksheet table data.",
        "シート「%1」はグラフシートまたはマクロシートのため、表データがありません。");
    reg("This workbook contains only chart or macro sheets and no worksheet table data: %1",
        "このブックにはグラフシートまたはマクロシートしかなく、表データがありません: %1");
    reg("Worksheet \"%1\" exceeds the maximum import size (20,000,000 cells).",
        "ワークシート「%1」のセル数が上限(20,000,000セル)を超えているため読み込めません。");
    reg("File not found: %1", "ファイルが見つかりません: %1");
    reg("The file is too large to import (over 512 MB): %1",
        "ファイルが大きすぎて読み込めません (512 MB 超): %1");
    reg("Cannot open %1: %2", "%1 を開けません: %2");
    reg("The workbook contains no data (all sheets are empty): %1",
        "ブックにデータがありません (すべてのシートが空です): %1");
    reg("%1 (%2 rows x %3 columns", "%1 (%2 行 x %3 列");
    reg(", unit row", "、単位行あり");
    reg(", %1 date column(s)", "、日時 %1 列");
    reg(", %1 text column(s)", "、文字列 %1 列");
    reg("%1: %2 sheet(s) imported - %3", "%1: %2 シートを読み込みました - %3");
    reg(" / empty sheets skipped: %1", " / 空のシートは読み込みませんでした: %1");
    reg("Excel Import", "Excel の読み込み");
    reg("Import Excel Workbook", "Excel ブックを読み込む");
    reg("Excel workbook (*.xlsx *.xlsm)", "Excel ブック (*.xlsx *.xlsm)");
    reg("All files (*)", "すべてのファイル (*)");
    reg("Put the sheets of a file into one workbook (sheet tabs)",
        "1 つのファイルのシートを 1 つのブックにまとめる (シートのタブ)");
    reg("Off: every sheet becomes a separate worksheet window.",
        "オフ: シートごとに別々のワークシートのウィンドウになります。");
    reg("Import &Excel Workbook (.xlsx)...", "Excel ブックを読み込む (.xlsx)(&E)...");
    reg("Import Excel Workbook (.xlsx)...", "Excel ブックを読み込む (.xlsx)...");
    reg("Import every sheet of an Excel workbook (.xlsx) as worksheets",
        "Excel ブック (.xlsx) の各シートをワークシートとして読み込む");
    reg("Old Excel (.xls) and OpenDocument (.ods) files cannot be read directly.\n"
        "Save them as an Excel Workbook (.xlsx) or as CSV and drop that file, or copy the "
        "cells in Excel (Ctrl+C) and paste them into a worksheet here (Ctrl+V).",
        "古い Excel 形式 (.xls) と OpenDocument 形式 (.ods) は直接読み込めません。\n"
        "「Excel ブック (.xlsx)」または CSV として保存し直してドロップするか、Excel でセルを"
        "コピー (Ctrl+C) してワークシートに貼り付けて (Ctrl+V) ください。");
}

} // namespace XlsxImport
