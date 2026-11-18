/***************************************************************************
    File                 : AsciiTableImportFilter.cpp
    Project              : SciDAVis / ToriGraph
    --------------------------------------------------------------------
    Copyright            : (C) 2008-2009 Knut Franke
    Email (use @ for *)  : Knut.Franke*gmx.net
    Description          : Shared Lab CSV / TSV / Rigaku .ras Table Importer.

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/

#include "table/AsciiTableImportFilter.h"
#include "table/future_Table.h"
#include "lib/IntervalAttribute.h"
#include "core/column/Column.h"
#include "core/datatypes/Double2StringFilter.h"
#include "core/datatypes/String2DoubleFilter.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTextCodec>
#include <QTextStream>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

QStringList AsciiTableImportFilter::fileExtensions() const
{
    return QStringList() << "txt"
                         << "csv"
                         << "tsv"
                         << "dat"
                         << "ras";
}

namespace {

struct LabCsvCell
{
    QString value;
    bool quoted = false;
};

bool isMissingNumericToken(const QString &trimmed, bool wasQuoted)
{
    if (trimmed.isEmpty())
        return true;
    if (trimmed == QLatin1String("-") || trimmed == QLatin1String("--")
        || trimmed == QLatin1String("---") || trimmed == QStringLiteral("\u2014")
        || trimmed == QStringLiteral("\u2013")) {
        return true;
    }
    const QString lo = trimmed.toLower();
    if (lo == QLatin1String("nan") || lo == QLatin1String("+nan") || lo == QLatin1String("-nan")
        || lo == QLatin1String("n/a") || lo == QLatin1String("na") || lo == QLatin1String("#n/a")
        || lo == QLatin1String("null") || lo == QLatin1String("none")) {
        return true;
    }
    if (!wasQuoted
        && (lo == QLatin1String("inf") || lo == QLatin1String("+inf")
            || lo == QLatin1String("-inf"))) {
        return true;
    }
    return false;
}

bool tryParseLabDouble(const QString &trimmed, const QLocale &primaryLoc, bool useCustomLocale,
                       double &outVal)
{
    QLocale cLoc = QLocale::c();
    cLoc.setNumberOptions(QLocale::RejectGroupSeparator);
    bool ok = false;
    if (!useCustomLocale) {
        outVal = cLoc.toDouble(trimmed, &ok);
        return ok && std::isfinite(outVal);
    }
    outVal = primaryLoc.toDouble(trimmed, &ok);
    if (ok && std::isfinite(outVal))
        return true;
    outVal = cLoc.toDouble(trimmed, &ok);
    return ok && std::isfinite(outVal);
}

bool isPureNumberString(const QString &s)
{
    const QString t = s.trimmed();
    if (t.isEmpty())
        return false;
    bool ok = false;
    QLocale cLoc = QLocale::c();
    cLoc.setNumberOptions(QLocale::RejectGroupSeparator);
    const double v = cLoc.toDouble(t, &ok);
    return ok && std::isfinite(v);
}

bool isPureDigitsString(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (QChar ch : s) {
        if (!ch.isDigit())
            return false;
    }
    return true;
}

bool isSingleUnitAtom(const QString &atomRaw)
{
    QString atom = atomRaw.trimmed();
    if (atom.isEmpty() || atom.size() > 24)
        return false;

    // Strip trailing superscripts / exponents like m^2, s^-1, cm-1, mm2, 10^-3, m^{1/2}
    static const QRegularExpression kScalePrefixRe(
            QStringLiteral("^10(?:\\^[+-]?\\d+|[⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻]+)$"));
    if (kScalePrefixRe.match(atom).hasMatch())
        return true;

    static const QSet<QString> kExactUnits = {
        QStringLiteral("%"),          QStringLiteral("‰"),
        QStringLiteral("-"),          QStringLiteral("\u2014"),
        QStringLiteral("\u2013"),     QStringLiteral("none"),
        QStringLiteral("ppm"),        QStringLiteral("ppb"),
        QStringLiteral("wt%"),        QStringLiteral("wt.%"),
        QStringLiteral("at%"),        QStringLiteral("at.%"),
        QStringLiteral("vol%"),       QStringLiteral("mol%"),
        QStringLiteral("mass%"),      QStringLiteral("wt.ppm"),
        QStringLiteral("at.ppm"),     QStringLiteral("a.u."),
        QStringLiteral("au"),         QStringLiteral("arb.u."),
        QStringLiteral("counts"),     QStringLiteral("cnts"),
        QStringLiteral("cps"),        QStringLiteral("cpm"),
        QStringLiteral("deg"),        QStringLiteral("deg."),
        QStringLiteral("rad"),        QStringLiteral("mrad"),
        QStringLiteral("°"),          QStringLiteral("°C"),
        QStringLiteral("°F"),         QStringLiteral("℃"),
        QStringLiteral("℉"),          QStringLiteral("degC"),
        QStringLiteral("degF"),       QStringLiteral("Å"),
        QStringLiteral("angstrom"),   QStringLiteral("sec"),
        QStringLiteral("min"),        QStringLiteral("hr"),
        QStringLiteral("hrs"),        QStringLiteral("day"),
        QStringLiteral("days"),       QStringLiteral("rpm"),
        QStringLiteral("bar"),        QStringLiteral("mbar"),
        QStringLiteral("atm"),        QStringLiteral("Torr"),
        QStringLiteral("mTorr"),      QStringLiteral("psi"),
        QStringLiteral("ksi"),        QStringLiteral("gf"),
        QStringLiteral("kgf"),        QStringLiteral("tf"),
        QStringLiteral("HV"),         QStringLiteral("HRC"),
        QStringLiteral("HRB"),        QStringLiteral("HB"),
        QStringLiteral("cal"),        QStringLiteral("kcal"),
        QStringLiteral("Wh"),         QStringLiteral("kWh"),
        QStringLiteral("mAh"),        QStringLiteral("Ah"),
        QStringLiteral("ohm"),        QStringLiteral("Ohm"),
        QStringLiteral("Oe"),         QStringLiteral("emu"),
        QStringLiteral("dB"),         QStringLiteral("dBm"),
        QStringLiteral("px"),         QStringLiteral("dpi"),
        QStringLiteral("in"),         QStringLiteral("ft"),
        QStringLiteral("mol"),        QStringLiteral("Da"),
        QStringLiteral("um"),         QStringLiteral("us"),
        QStringLiteral("uA"),         QStringLiteral("uV"),
        QStringLiteral("uF"),         QStringLiteral("ug"),
        QStringLiteral("uL"),         QStringLiteral("ul"),
        QStringLiteral("uJ"),         QStringLiteral("uW"),
        QStringLiteral("uN"),         QStringLiteral("uPa"),
        QStringLiteral("uS"),         QStringLiteral("uH"),
        QStringLiteral("uT"),         QStringLiteral("uM"),
        QStringLiteral("uC"),         QStringLiteral("umol"),
        QStringLiteral("ubar"),       QStringLiteral("uohm"),
        QStringLiteral("uOhm")
    };
    if (kExactUnits.contains(atom))
        return true;

    // Strip trailing power suffix like ^-1, ^2, ^0.5, ^(1/2), ⁻¹, ², ³, negative integer -1..-4, or area/volume [23] on length units
    QString core = atom;
    static const QRegularExpression kTrailingExpRe(
            QStringLiteral("(?:\\^(?:\\([^)]+\\)|\\{[^}]+\\}|[+-]?\\d+(?:\\.\\d+)?)|[⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻]+|-[1234])$"));
    core.remove(kTrailingExpRe);
    static const QRegularExpression kLengthAreaVolRe(
            QStringLiteral("^((?:[pnμµucdkhM])?m)[23]$"));
    const auto mLen = kLengthAreaVolRe.match(core);
    if (mLen.hasMatch())
        core = mLen.captured(1);
    if (core.isEmpty())
        return false;
    if (kExactUnits.contains(core))
        return true;

    // SI prefix + standard SI / MatSci unit symbol
    static const QSet<QString> kSiBases = {
        QStringLiteral("m"),   QStringLiteral("g"),   QStringLiteral("s"),
        QStringLiteral("A"),   QStringLiteral("K"),   QStringLiteral("mol"),
        QStringLiteral("cd"),  QStringLiteral("Hz"),  QStringLiteral("N"),
        QStringLiteral("Pa"),  QStringLiteral("J"),   QStringLiteral("W"),
        QStringLiteral("C"),   QStringLiteral("V"),   QStringLiteral("F"),
        QStringLiteral("Ω"),   QStringLiteral("S"),   QStringLiteral("Wb"),
        QStringLiteral("T"),   QStringLiteral("H"),   QStringLiteral("lm"),
        QStringLiteral("lx"),  QStringLiteral("Bq"),  QStringLiteral("Gy"),
        QStringLiteral("Sv"),  QStringLiteral("L"),   QStringLiteral("l"),
        QStringLiteral("eV"),  QStringLiteral("bar"), QStringLiteral("Torr"),
        QStringLiteral("M"),   QStringLiteral("G"),   QStringLiteral("h")
    };
    if (kSiBases.contains(core))
        return true;

    if (core.size() >= 2) {
        const QChar pfx = core.at(0);
        static const QString kSiPrefixes = QStringLiteral("YZEPTGMkhdcfpnμµm");
        if (kSiPrefixes.contains(pfx) && kSiBases.contains(core.mid(1)))
            return true;
        if (core.startsWith(QLatin1String("da")) && kSiBases.contains(core.mid(2)))
            return true;
    }
    return false;
}

bool isLikelyScientificUnit(const QString &rawCandidate)
{
    QString cand = rawCandidate.trimmed();
    if (cand.isEmpty() || cand.size() > 36)
        return false;

    const QString lo = cand.toLower();
    if (lo == QLatin1String("arb. units") || lo == QLatin1String("arb. unit")
        || lo == QLatin1String("arbitrary units") || lo == QLatin1String("atomic %")
        || lo == QLatin1String("weight %") || lo == QLatin1String("mass %")
        || lo == QLatin1String("vol. %") || lo == QLatin1String("at. %")
        || lo == QLatin1String("wt. %")) {
        return true;
    }

    // Do not treat missing-value sentinels as units
    if (lo == QLatin1String("nan") || lo == QLatin1String("n/a") || lo == QLatin1String("na")
        || lo == QLatin1String("null") || lo == QLatin1String("#n/a")) {
        return false;
    }

    if (isSingleUnitAtom(cand))
        return true;

    // Check compound unit expressions split by '/', '*', '·', or whitespace (e.g. "kJ/mol", "wt.ppm/s", "m^2/s", "MPa m^0.5", "J/(mol·K)")
    QString cleaned = cand;
    cleaned.replace(QLatin1Char('('), QLatin1Char(' '));
    cleaned.replace(QLatin1Char(')'), QLatin1Char(' '));
    cleaned.replace(QLatin1Char('/'), QLatin1Char(' '));
    cleaned.replace(QLatin1Char('*'), QLatin1Char(' '));
    cleaned.replace(QChar(0x00B7), QLatin1Char(' ')); // ·
    cleaned.replace(QChar(0x22C5), QLatin1Char(' ')); // ⋅

    const QStringList parts = cleaned.split(QRegularExpression(QStringLiteral("\\s+")),
                                            Qt::SkipEmptyParts);
    if (parts.isEmpty() || parts.size() > 6)
        return false;
    for (const QString &part : parts) {
        if (!isSingleUnitAtom(part))
            return false;
    }
    return true;
}

bool isCleanColumnIdentifier(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (QChar ch : s) {
        if (ch.isSpace())
            return false;
        const ushort u = ch.unicode();
        if (u == ',' || u == ';' || u == '(' || u == ')' || u == '[' || u == ']' || u == '/'
            || u == '\\' || u == ':' || u == '"' || u == '\'') {
            return false;
        }
    }
    return true;
}

QString sanitizeShortColumnIdentifier(const QString &baseName)
{
    QString working = baseName.trimmed();
    if (working.isEmpty())
        return QString();

    // If baseName has a trailing ", symbol" like "Nominal stress, σ", keep the descriptive stem if >= 2 letters
    const int commaPos = working.indexOf(QLatin1Char(','));
    if (commaPos > 0) {
        const QString stem = working.left(commaPos).trimmed();
        int letterCount = 0;
        for (QChar ch : stem) {
            if (ch.isLetter())
                ++letterCount;
        }
        if (letterCount >= 2)
            working = stem;
    }

    QString out;
    out.reserve(working.size());
    bool prevUnderscore = false;
    for (QChar ch : working) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_')) {
            if (ch == QLatin1Char('_')) {
                if (!out.isEmpty() && !prevUnderscore) {
                    out.append(QLatin1Char('_'));
                    prevUnderscore = true;
                }
            } else {
                out.append(ch);
                prevUnderscore = false;
            }
        } else {
            if (!out.isEmpty() && !prevUnderscore) {
                out.append(QLatin1Char('_'));
                prevUnderscore = true;
            }
        }
    }
    while (out.endsWith(QLatin1Char('_')) || out.endsWith(QLatin1Char('-')))
        out.chop(1);
    while (out.startsWith(QLatin1Char('_')) || out.startsWith(QLatin1Char('-')))
        out.remove(0, 1);
    return out;
}

QString cleanPromotedUnitCell(const QString &rawCell)
{
    QString u = rawCell.trimmed();
    if (u.size() >= 2
        && ((u.startsWith(QLatin1Char('(')) && u.endsWith(QLatin1Char(')')))
            || (u.startsWith(QLatin1Char('[')) && u.endsWith(QLatin1Char(']'))))) {
        u = u.mid(1, u.size() - 2).trimmed();
    }
    return u;
}

QString detectDelimiterFromSampleLines(const QStringList &sampleLines, const QString &filePathHint)
{
    const bool isCsvExt = filePathHint.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive);
    const bool isTsvExt = filePathHint.endsWith(QLatin1String(".tsv"), Qt::CaseInsensitive);

    if (sampleLines.isEmpty())
        return isCsvExt ? QStringLiteral(",") : QStringLiteral("\t");

    auto countUnquoted = [](const QString &line, QChar delim) -> int {
        bool inQuotes = false;
        int cnt = 0;
        for (int i = 0; i < line.size(); ++i) {
            const QChar c = line[i];
            if (c == QLatin1Char('"')) {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == QLatin1Char('"'))
                    ++i;
                else
                    inQuotes = !inQuotes;
            } else if (!inQuotes && c == delim) {
                ++cnt;
            }
        }
        return cnt;
    };

    struct DelimStats
    {
        QChar ch;
        int linesWithDelim = 0;
        int minCnt = std::numeric_limits<int>::max();
        int maxCnt = 0;
        int totalCnt = 0;
    };

    DelimStats cands[3] = { { QLatin1Char('\t') }, { QLatin1Char(',') }, { QLatin1Char(';') } };
    const int nSample = sampleLines.size();
    for (const QString &ln : sampleLines) {
        for (DelimStats &ds : cands) {
            const int c = countUnquoted(ln, ds.ch);
            if (c > 0)
                ++ds.linesWithDelim;
            ds.minCnt = std::min(ds.minCnt, c);
            ds.maxCnt = std::max(ds.maxCnt, c);
            ds.totalCnt += c;
        }
    }

    if (isTsvExt && cands[0].linesWithDelim > 0)
        return QStringLiteral("\t");
    if (isCsvExt && cands[1].linesWithDelim > 0 && cands[0].linesWithDelim == 0)
        return QStringLiteral(",");

    // Prefer delimiter present on all sample lines with consistent positive count
    int bestIdx = -1;
    int bestScore = -1;
    for (int i = 0; i < 3; ++i) {
        const DelimStats &ds = cands[i];
        if (ds.linesWithDelim == 0)
            continue;
        int score = ds.linesWithDelim * 100 + std::min(ds.totalCnt, 99);
        if (ds.minCnt >= 1 && ds.minCnt == ds.maxCnt && nSample >= 2)
            score += 500;
        if (isCsvExt && ds.ch == QLatin1Char(','))
            score += 50;
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0)
        return QString(cands[bestIdx].ch);

    // Check whitespace-separated columns (e.g. .dat / .txt numeric tables)
    int multiSpaceLines = 0;
    for (const QString &ln : sampleLines) {
        const QString s = ln.simplified();
        if (s.contains(QLatin1Char(' ')))
            ++multiSpaceLines;
    }
    if (multiSpaceLines > 0 && multiSpaceLines >= (nSample + 1) / 2)
        return QStringLiteral(" ");

    return isCsvExt ? QStringLiteral(",") : QStringLiteral("\t");
}

void parseDelimitedRecordsFast(const QString &text, int startPos, const QString &sep,
                               bool whitespaceDelim, bool trimSpaces, bool simplifySpaces,
                               QVector<QVector<LabCsvCell>> &outRecords, int &outMaxCols)
{
    outRecords.clear();
    outMaxCols = 0;

    const QChar *buf = text.constData();
    const int len = text.size();
    int pos = std::max(0, startPos);

    if (whitespaceDelim) {
        while (pos < len) {
            int lineEnd = pos;
            while (lineEnd < len && buf[lineEnd] != QLatin1Char('\r')
                   && buf[lineEnd] != QLatin1Char('\n')) {
                ++lineEnd;
            }
            QVector<LabCsvCell> row;
            int p = pos;
            while (p < lineEnd) {
                while (p < lineEnd && (buf[p] == QLatin1Char(' ') || buf[p] == QLatin1Char('\t')))
                    ++p;
                if (p >= lineEnd)
                    break;
                LabCsvCell cell;
                if (buf[p] == QLatin1Char('"')) {
                    cell.quoted = true;
                    ++p;
                    while (p < lineEnd) {
                        if (buf[p] == QLatin1Char('"')) {
                            if (p + 1 < lineEnd && buf[p + 1] == QLatin1Char('"')) {
                                cell.value.append(QLatin1Char('"'));
                                p += 2;
                            } else {
                                ++p;
                                break;
                            }
                        } else {
                            cell.value.append(buf[p++]);
                        }
                    }
                } else {
                    const int tokStart = p;
                    while (p < lineEnd && buf[p] != QLatin1Char(' ') && buf[p] != QLatin1Char('\t'))
                        ++p;
                    cell.value = QString(buf + tokStart, p - tokStart);
                }
                row.append(std::move(cell));
            }
            if (!row.isEmpty()) {
                outMaxCols = std::max(outMaxCols, row.size());
                outRecords.append(std::move(row));
            }
            pos = lineEnd;
            if (pos < len && buf[pos] == QLatin1Char('\r')) {
                ++pos;
                if (pos < len && buf[pos] == QLatin1Char('\n'))
                    ++pos;
            } else if (pos < len && buf[pos] == QLatin1Char('\n')) {
                ++pos;
            }
        }
        return;
    }

    const QString effSep = sep.isEmpty() ? QStringLiteral(",") : sep;
    const int sepLen = effSep.size();
    const QChar sepFirst = effSep.at(0);

    auto matchesSepAt = [&](int idx) -> bool {
        if (idx + sepLen > len || buf[idx] != sepFirst)
            return false;
        for (int k = 1; k < sepLen; ++k) {
            if (buf[idx + k] != effSep.at(k))
                return false;
        }
        return true;
    };

    while (pos < len) {
        QVector<LabCsvCell> row;
        while (true) {
            LabCsvCell cell;
            const int fieldStart = pos;
            int scanPos = pos;
            if (trimSpaces || simplifySpaces) {
                while (scanPos < len
                       && (buf[scanPos] == QLatin1Char(' ')
                           || (effSep != QLatin1String("\t") && buf[scanPos] == QLatin1Char('\t')))
                       && !matchesSepAt(scanPos)) {
                    ++scanPos;
                }
            }

            if (scanPos < len && buf[scanPos] == QLatin1Char('"')) {
                cell.quoted = true;
                pos = scanPos + 1; // consume opening '"'
                while (pos < len) {
                    const QChar ch = buf[pos];
                    if (ch == QLatin1Char('"')) {
                        if (pos + 1 < len && buf[pos + 1] == QLatin1Char('"')) {
                            cell.value.append(QLatin1Char('"'));
                            pos += 2;
                        } else {
                            ++pos; // consume closing '"'
                            break;
                        }
                    } else {
                        cell.value.append(ch);
                        ++pos;
                    }
                }
                while (pos < len && buf[pos] != QLatin1Char('\r') && buf[pos] != QLatin1Char('\n')
                       && !matchesSepAt(pos)) {
                    if (buf[pos] != QLatin1Char(' ') && buf[pos] != QLatin1Char('\t'))
                        cell.value.append(buf[pos]);
                    ++pos;
                }
            } else {
                pos = fieldStart;
                const int rawStart = pos;
                while (pos < len && buf[pos] != QLatin1Char('\r') && buf[pos] != QLatin1Char('\n')
                       && !matchesSepAt(pos)) {
                    ++pos;
                }
                QString rawField(buf + rawStart, pos - rawStart);
                if (simplifySpaces)
                    rawField = rawField.simplified();
                else if (trimSpaces)
                    rawField = rawField.trimmed();
                cell.value = std::move(rawField);
            }
            row.append(std::move(cell));

            if (pos < len && matchesSepAt(pos)) {
                pos += sepLen;
                continue;
            }
            if (pos < len && buf[pos] == QLatin1Char('\r')) {
                ++pos;
                if (pos < len && buf[pos] == QLatin1Char('\n'))
                    ++pos;
            } else if (pos < len && buf[pos] == QLatin1Char('\n')) {
                ++pos;
            }
            break;
        }
        outMaxCols = std::max(outMaxCols, row.size());
        outRecords.append(std::move(row));
    }
}

future::Table *importLabTextDataInternal(const QByteArray &rawBytes, const QString &tableName,
                                         const QString &filePathHint, const QString &separatorHint,
                                         const QString &encodingHint, int unitsRowHint,
                                         int ignoredLines, bool firstRowNamesColumns,
                                         bool trimSpaces, bool simplifySpaces,
                                         bool convertToNumeric, const QLocale &numericLocale,
                                         bool autoFixTabOnCsv)
{
    const QString effTableName =
            tableName.trimmed().isEmpty() ? QObject::tr("Table") : tableName.trimmed();
    const QString decoded = AsciiTableImportFilter::decodeLabTextAuto(rawBytes, encodingHint);
    if (decoded.isEmpty())
        return new future::Table(0, 0, effTableName);

    const bool isRasExt = filePathHint.endsWith(QLatin1String(".ras"), Qt::CaseInsensitive);
    const QString trimmedStart = decoded.left(512).trimmed();

    // 1. Specialized Rigaku SmartLab .ras XRD / XRR parser
    if ((isRasExt || trimmedStart.startsWith(QLatin1String("*RAS_")))
        && decoded.contains(QLatin1String("*RAS_INT_START"))) {
        QString unitX = QStringLiteral("deg.");
        QString unitY = QStringLiteral("cps");

        static const QRegularExpression kUnitXRe(
                QStringLiteral("\\*MEAS_SCAN_UNIT_X\\s+\"([^\"]*)\""));
        static const QRegularExpression kUnitYRe(
                QStringLiteral("\\*MEAS_SCAN_UNIT_Y\\s+\"([^\"]*)\""));
        const auto mx = kUnitXRe.match(decoded);
        if (mx.hasMatch() && !mx.captured(1).trimmed().isEmpty())
            unitX = mx.captured(1).trimmed();
        const auto my = kUnitYRe.match(decoded);
        if (my.hasMatch() && !my.captured(1).trimmed().isEmpty())
            unitY = my.captured(1).trimmed();

        const int intStart = decoded.indexOf(QLatin1String("*RAS_INT_START"));
        int bodyStart = decoded.indexOf(QLatin1Char('\n'), intStart);
        bodyStart = (bodyStart >= 0) ? (bodyStart + 1) : (intStart + 14);
        int intEnd = decoded.indexOf(QLatin1String("*RAS_INT_END"), bodyStart);
        if (intEnd < 0)
            intEnd = decoded.size();

        const QString body = decoded.mid(bodyStart, std::max(0, intEnd - bodyStart));
        QVector<QVector<LabCsvCell>> rasRows;
        int rasCols = 0;
        parseDelimitedRecordsFast(body, 0, QStringLiteral(" "), true, true, true, rasRows, rasCols);
        rasCols = std::max(2, rasCols);
        const int nRows = rasRows.size();

        QLocale cLoc = QLocale::c();
        cLoc.setNumberOptions(QLocale::RejectGroupSeparator);

        QList<Column *> cols;
        for (int c = 0; c < rasCols; ++c) {
            std::unique_ptr<QVector<qreal>> vec(new QVector<qreal>(nRows, 0.0));
            IntervalAttribute<bool> inv;
            for (int r = 0; r < nRows; ++r) {
                const QString cellTxt =
                        (c < rasRows[r].size()) ? rasRows[r][c].value.trimmed() : QString();
                bool ok = false;
                const double v = cLoc.toDouble(cellTxt, &ok);
                if (ok && std::isfinite(v))
                    (*vec)[r] = v;
                else
                    inv.setValue(r, true);
            }
            QString colShort = (c == 0) ? QStringLiteral("TwoTheta")
                    : (c == 1)          ? QStringLiteral("Intensity")
                    : (c == 2)          ? QStringLiteral("Factor")
                                        : QStringLiteral("Col%1").arg(c + 1);
            Column *col = new Column(colShort, std::move(vec), inv);
            if (c == 0) {
                col->setLongName(QStringLiteral("2\u03B8"));
                col->setUnits(unitX);
                col->setPlotDesignation(SciDAVis::X);
            } else if (c == 1) {
                col->setLongName(QStringLiteral("Intensity"));
                col->setUnits(unitY);
                col->setPlotDesignation(SciDAVis::Y);
            } else {
                col->setPlotDesignation(SciDAVis::noDesignation);
            }
            if (auto *d2s = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
                d2s->setNumericFormat('g');
                d2s->setNumDigits(6);
            }
            cols << col;
        }

        future::Table *result = new future::Table(0, 0, effTableName);
        result->appendColumns(cols);
        return result;
    }

    // 2. Advance past explicit ignoredLines
    const QChar *buf = decoded.constData();
    const int len = decoded.size();
    int pos = 0;
    for (int skipped = 0; skipped < ignoredLines && pos < len; ++skipped) {
        while (pos < len && buf[pos] != QLatin1Char('\r') && buf[pos] != QLatin1Char('\n'))
            ++pos;
        if (pos < len && buf[pos] == QLatin1Char('\r')) {
            ++pos;
            if (pos < len && buf[pos] == QLatin1Char('\n'))
                ++pos;
        } else if (pos < len && buf[pos] == QLatin1Char('\n')) {
            ++pos;
        }
    }

    // 3. Skip leading blank lines & '#'-prefixed comment preamble lines
    //    (preserving a final '# col1, col2' header line if data starts immediately with numbers!)
    QString lastHashCommentBody;
    while (pos < len) {
        int lineStart = pos;
        while (lineStart < len && (buf[lineStart] == QLatin1Char(' ') || buf[lineStart] == QLatin1Char('\t')))
            ++lineStart;
        int lineEnd = lineStart;
        while (lineEnd < len && buf[lineEnd] != QLatin1Char('\r') && buf[lineEnd] != QLatin1Char('\n'))
            ++lineEnd;

        int nextPos = lineEnd;
        if (nextPos < len && buf[nextPos] == QLatin1Char('\r')) {
            ++nextPos;
            if (nextPos < len && buf[nextPos] == QLatin1Char('\n'))
                ++nextPos;
        } else if (nextPos < len && buf[nextPos] == QLatin1Char('\n')) {
            ++nextPos;
        }

        if (lineStart == lineEnd) {
            pos = nextPos;
            continue;
        }
        if (buf[lineStart] == QLatin1Char('#')) {
            lastHashCommentBody = QString(buf + lineStart + 1, lineEnd - (lineStart + 1)).trimmed();
            pos = nextPos;
            continue;
        }
        break;
    }

    // Gather up to 15 sample lines for separator auto-detection
    QStringList sampleLines;
    {
        int sPos = pos;
        while (sPos < len && sampleLines.size() < 15) {
            int sEnd = sPos;
            while (sEnd < len && buf[sEnd] != QLatin1Char('\r') && buf[sEnd] != QLatin1Char('\n'))
                ++sEnd;
            const QString ln = QString(buf + sPos, sEnd - sPos).trimmed();
            if (!ln.isEmpty() && !ln.startsWith(QLatin1Char('#')))
                sampleLines.append(ln);
            sPos = sEnd;
            if (sPos < len && buf[sPos] == QLatin1Char('\r')) {
                ++sPos;
                if (sPos < len && buf[sPos] == QLatin1Char('\n'))
                    ++sPos;
            } else if (sPos < len && buf[sPos] == QLatin1Char('\n')) {
                ++sPos;
            }
        }
    }

    QString effSep = separatorHint;
    if (effSep == QLatin1String("\\t"))
        effSep = QStringLiteral("\t");
    else if (effSep == QLatin1String("\\s"))
        effSep = QStringLiteral(" ");

    bool whitespaceDelim = false;
    if (effSep.isEmpty() || effSep.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0) {
        effSep = detectDelimiterFromSampleLines(sampleLines, filePathHint);
        if (effSep == QLatin1String(" "))
            whitespaceDelim = true;
    } else if (effSep == QLatin1String(" ") && simplifySpaces) {
        whitespaceDelim = true;
    } else if (autoFixTabOnCsv && effSep == QLatin1String("\t") && !sampleLines.isEmpty()) {
        // Auto-recover when GUI default "\t" is used on a comma/semicolon CSV file with 0 tabs
        bool anyTab = false;
        for (const QString &ln : sampleLines) {
            if (ln.contains(QLatin1Char('\t'))) {
                anyTab = true;
                break;
            }
        }
        if (!anyTab) {
            const bool isCsvExt = filePathHint.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive);
            const QString detected = detectDelimiterFromSampleLines(sampleLines, filePathHint);
            if (detected == QLatin1String(",") || detected == QLatin1String(";")) {
                if (isCsvExt || sampleLines.size() >= 2)
                    effSep = detected;
            }
        }
    }

    QVector<QVector<LabCsvCell>> records;
    int maxCols = 0;
    parseDelimitedRecordsFast(decoded, pos, effSep, whitespaceDelim, trimSpaces, simplifySpaces,
                              records, maxCols);

    // Strip trailing empty lines or mid-file '#' comment lines
    for (int r = records.size() - 1; r >= 0; --r) {
        const auto &row = records[r];
        if (row.isEmpty()) {
            records.removeAt(r);
            continue;
        }
        if (row.size() == 1 && !row[0].quoted && row[0].value.trimmed().isEmpty()) {
            records.removeAt(r);
            continue;
        }
        const QString firstTrim = row[0].value.trimmed();
        if (!row[0].quoted && firstTrim.startsWith(QLatin1Char('#'))) {
            records.removeAt(r);
            continue;
        }
    }

    maxCols = 0;
    for (const auto &row : records)
        maxCols = std::max(maxCols, row.size());
    maxCols = std::max(1, maxCols);

    // Check if '# col1, col2' right above numeric row 0 was actually the column header line
    if (firstRowNamesColumns && !lastHashCommentBody.isEmpty() && !records.isEmpty()
        && maxCols >= 2) {
        bool row0AllNumeric = true;
        for (const LabCsvCell &c0 : records[0]) {
            if (!isPureNumberString(c0.value)) {
                row0AllNumeric = false;
                break;
            }
        }
        if (row0AllNumeric) {
            QVector<QVector<LabCsvCell>> hashRows;
            int hashCols = 0;
            parseDelimitedRecordsFast(lastHashCommentBody, 0, effSep, whitespaceDelim, true,
                                      simplifySpaces, hashRows, hashCols);
            if (hashRows.size() == 1 && hashRows[0].size() == records[0].size()) {
                bool anyNonNumericHdr = false;
                for (const LabCsvCell &hc : hashRows[0]) {
                    if (!hc.value.trimmed().isEmpty() && !isPureNumberString(hc.value))
                        anyNonNumericHdr = true;
                }
                if (anyNonNumericHdr)
                    records.prepend(hashRows[0]);
            }
        }
    }

    const bool useCustomLoc =
            (numericLocale.decimalPoint() != QLatin1Char('.') && numericLocale != QLocale::c());
    QLocale primaryLoc = numericLocale;
    primaryLoc.setNumberOptions(QLocale::RejectGroupSeparator);

    QVector<LabCsvCell> headerRow;
    QVector<LabCsvCell> unitsRow;
    int dataStartIdx = 0;

    if (firstRowNamesColumns && !records.isEmpty()) {
        // Guard: if firstRowNamesColumns is true, verify row 0 isn't purely numeric data when user didn't provide headers?
        // Note: SciDAVis standard honors firstRowNamesColumns=true explicitly when set.
        headerRow = records[0];
        dataStartIdx = 1;
    }

    // 4. 2-Row [Name row + Units row] auto-detection (Instron / Shimadzu / Zwick CSVs)
    if (firstRowNamesColumns && dataStartIdx == 1 && unitsRowHint != 0) {
        if (unitsRowHint == 1 && records.size() >= 2) {
            unitsRow = records[1];
            dataStartIdx = 2;
        } else if (unitsRowHint < 0 && records.size() >= 3) {
            const QVector<LabCsvCell> &candUnits = records[1];
            const int checkEnd = std::min(records.size(), 14);
            int numericDataCols = 0;
            bool row2HasNumberInNumericCol = false;
            bool row2AllValidUnitsInNumericCols = true;
            int recognizedUnitTokens = 0;

            for (int c = 0; c < maxCols; ++c) {
                bool colHasFinite = false;
                bool colAllNumeric = true;
                for (int r = 2; r < checkEnd; ++r) {
                    const QString s =
                            (c < records[r].size()) ? records[r][c].value.trimmed() : QString();
                    const bool q = (c < records[r].size()) ? records[r][c].quoted : false;
                    if (isMissingNumericToken(s, q))
                        continue;
                    double dv = 0.0;
                    if (tryParseLabDouble(s, primaryLoc, useCustomLoc, dv)) {
                        colHasFinite = true;
                    } else {
                        colAllNumeric = false;
                        break;
                    }
                }
                if (colHasFinite && colAllNumeric) {
                    ++numericDataCols;
                    const QString uRaw =
                            (c < candUnits.size()) ? candUnits[c].value.trimmed() : QString();
                    if (!uRaw.isEmpty()) {
                        double dummy = 0.0;
                        if (tryParseLabDouble(uRaw, primaryLoc, useCustomLoc, dummy)) {
                            row2HasNumberInNumericCol = true;
                            break;
                        }
                        const QString uClean = cleanPromotedUnitCell(uRaw);
                        if (isLikelyScientificUnit(uClean))
                            ++recognizedUnitTokens;
                        else
                            row2AllValidUnitsInNumericCols = false;
                    }
                }
            }

            if (numericDataCols >= 1 && !row2HasNumberInNumericCol
                && row2AllValidUnitsInNumericCols && recognizedUnitTokens >= 1) {
                unitsRow = candUnits;
                dataStartIdx = 2;
            }
        }
    }

    const int dataRowCount = std::max(0, records.size() - dataStartIdx);

    // 5. Build typed Columns with metadata & 'g' (6-digit) display formatting
    QSet<QString> usedShortNames;
    auto makeUniqueShort = [&](const QString &desired, int colIdx) -> QString {
        QString base = desired.trimmed();
        if (base.isEmpty())
            base = QString::number(colIdx + 1);
        QString cand = base;
        int suffix = 2;
        while (usedShortNames.contains(cand)) {
            cand = QStringLiteral("%1_%2").arg(base).arg(suffix++);
        }
        usedShortNames.insert(cand);
        return cand;
    };

    QVector<bool> colIsNumeric(maxCols, false);
    QList<Column *> cols;
    cols.reserve(maxCols);

    for (int c = 0; c < maxCols; ++c) {
        QString shortName;
        QString longName;
        QString units;

        if (firstRowNamesColumns && !headerRow.isEmpty()) {
            const QString rawHdr = (c < headerRow.size()) ? headerRow[c].value : QString();
            AsciiTableImportFilter::splitHeaderNameAndUnits(rawHdr, shortName, longName, units);
        }
        if (!unitsRow.isEmpty() && c < unitsRow.size()) {
            const QString u2 = cleanPromotedUnitCell(unitsRow[c].value);
            if (!u2.isEmpty()) {
                if (units.isEmpty())
                    units = u2;
            }
        }
        shortName = makeUniqueShort(shortName, c);

        bool useText = !convertToNumeric;
        bool anyQuoted = false;
        bool anyFiniteNumber = false;
        std::unique_ptr<QVector<qreal>> numVec(
                new QVector<qreal>(dataRowCount, std::numeric_limits<double>::quiet_NaN()));
        IntervalAttribute<bool> invalidAttr;

        if (convertToNumeric) {
            int runInvalidStart = -1;
            for (int r = 0; r < dataRowCount; ++r) {
                const QVector<LabCsvCell> &rec = records[dataStartIdx + r];
                const QString valStr = (c < rec.size()) ? rec[c].value : QString();
                const bool wasQuoted = (c < rec.size()) ? rec[c].quoted : false;
                if (wasQuoted)
                    anyQuoted = true;
                const QString trimmed = valStr.trimmed();
                if (isMissingNumericToken(trimmed, wasQuoted)) {
                    if (runInvalidStart < 0)
                        runInvalidStart = r;
                    continue;
                }
                if (runInvalidStart >= 0) {
                    invalidAttr.setValue(Interval<int>(runInvalidStart, r - 1), true);
                    runInvalidStart = -1;
                }
                double numVal = 0.0;
                if (tryParseLabDouble(trimmed, primaryLoc, useCustomLoc, numVal)) {
                    (*numVec)[r] = numVal;
                    anyFiniteNumber = true;
                } else {
                    useText = true;
                    break;
                }
            }
            if (!useText && runInvalidStart >= 0)
                invalidAttr.setValue(Interval<int>(runInvalidStart, dataRowCount - 1), true);
            if (!useText && anyQuoted && !anyFiniteNumber)
                useText = true;
        }

        Column *col = nullptr;
        if (useText) {
            std::unique_ptr<QStringList> strVec(new QStringList());
            strVec->reserve(dataRowCount);
            for (int r = 0; r < dataRowCount; ++r) {
                const QVector<LabCsvCell> &rec = records[dataStartIdx + r];
                strVec->append((c < rec.size()) ? rec[c].value : QString());
            }
            col = new Column(shortName, std::move(strVec), IntervalAttribute<bool>());
            colIsNumeric[c] = false;
        } else {
            col = new Column(shortName, std::move(numVec), invalidAttr);
            if (auto *d2s = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
                d2s->setNumericFormat('g');
                d2s->setNumDigits(6);
            }
            colIsNumeric[c] = true;
        }

        if (!longName.isEmpty())
            col->setLongName(longName);
        if (!units.isEmpty())
            col->setUnits(units);

        cols << col;
    }

    // Assign sensible X / Y plot designations (if col 0 is Text Specimen IDs in a >=3-col table, make col 1 X!)
    if (maxCols >= 3 && !colIsNumeric[0] && colIsNumeric[1] && colIsNumeric[2]) {
        cols[0]->setPlotDesignation(SciDAVis::noDesignation);
        cols[1]->setPlotDesignation(SciDAVis::X);
        for (int c = 2; c < maxCols; ++c)
            cols[c]->setPlotDesignation(SciDAVis::Y);
    } else {
        for (int c = 0; c < maxCols; ++c)
            cols[c]->setPlotDesignation((c == 0) ? SciDAVis::X : SciDAVis::Y);
    }

    future::Table *result = new future::Table(0, 0, effTableName);
    result->appendColumns(cols);
    return result;
}

} // namespace

QString AsciiTableImportFilter::decodeLabTextAuto(const QByteArray &rawBytes,
                                                  const QString &encodingHint)
{
    if (rawBytes.isEmpty())
        return QString();

    // 1. Check explicit BOMs first
    if (rawBytes.size() >= 3 && static_cast<unsigned char>(rawBytes[0]) == 0xEF
        && static_cast<unsigned char>(rawBytes[1]) == 0xBB
        && static_cast<unsigned char>(rawBytes[2]) == 0xBF) {
        return QString::fromUtf8(rawBytes.constData() + 3, rawBytes.size() - 3);
    }
    if (rawBytes.size() >= 2 && static_cast<unsigned char>(rawBytes[0]) == 0xFF
        && static_cast<unsigned char>(rawBytes[1]) == 0xFE) {
        if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16LE"))
            return c16->toUnicode(rawBytes.constData() + 2, rawBytes.size() - 2);
    }
    if (rawBytes.size() >= 2 && static_cast<unsigned char>(rawBytes[0]) == 0xFE
        && static_cast<unsigned char>(rawBytes[1]) == 0xFF) {
        if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16BE"))
            return c16->toUnicode(rawBytes.constData() + 2, rawBytes.size() - 2);
    }

    // 2. Honor explicit encodingHint when specified and not "auto"
    const QString hint = encodingHint.trimmed();
    if (!hint.isEmpty() && hint.compare(QLatin1String("auto"), Qt::CaseInsensitive) != 0) {
        const QString lo = hint.toLower();
        QByteArray codecName = hint.toLatin1();
        if (lo == QLatin1String("utf8") || lo == QLatin1String("utf-8")
            || lo == QLatin1String("utf-8-sig") || lo == QLatin1String("utf_8")) {
            QString s = QString::fromUtf8(rawBytes);
            if (!s.isEmpty() && s.at(0) == QChar(0xFEFF))
                s.remove(0, 1);
            return s;
        }
        if (lo == QLatin1String("sjis") || lo == QLatin1String("shift_jis")
            || lo == QLatin1String("shift-jis") || lo == QLatin1String("cp932")
            || lo == QLatin1String("ms932") || lo == QLatin1String("windows-31j")) {
            codecName = QByteArrayLiteral("Shift-JIS");
        } else if (lo == QLatin1String("latin1") || lo == QLatin1String("latin-1")
                   || lo == QLatin1String("iso-8859-1")) {
            return QString::fromLatin1(rawBytes);
        }
        if (QTextCodec *codec = QTextCodec::codecForName(codecName)) {
            QString s = codec->toUnicode(rawBytes);
            if (!s.isEmpty() && s.at(0) == QChar(0xFEFF))
                s.remove(0, 1);
            return s;
        }
    }

    // 3. Fast path for pure 7-bit ASCII
    bool pureAscii = true;
    bool hasNullByte = false;
    const int nBytes = rawBytes.size();
    const unsigned char *uptr = reinterpret_cast<const unsigned char *>(rawBytes.constData());
    for (int i = 0; i < nBytes; ++i) {
        if (uptr[i] == 0) {
            hasNullByte = true;
            pureAscii = false;
            break;
        }
        if (uptr[i] >= 0x80)
            pureAscii = false;
    }
    if (pureAscii)
        return QString::fromLatin1(rawBytes);

    // 4. BOM-less UTF-16 heuristic
    if (hasNullByte && nBytes >= 4 && (nBytes % 2 == 0)) {
        int evenZeros = 0, oddZeros = 0;
        const int samplePairs = std::min(nBytes / 2, 256);
        for (int p = 0; p < samplePairs; ++p) {
            if (uptr[2 * p] == 0)
                ++evenZeros;
            if (uptr[2 * p + 1] == 0)
                ++oddZeros;
        }
        if (oddZeros > samplePairs / 2 && evenZeros == 0) {
            if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16LE"))
                return c16->toUnicode(rawBytes);
        }
        if (evenZeros > samplePairs / 2 && oddZeros == 0) {
            if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16BE"))
                return c16->toUnicode(rawBytes);
        }
    }

    // 5. Strict UTF-8 validation
    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        QTextCodec::ConverterState state;
        QString decoded = utf8->toUnicode(rawBytes.constData(), rawBytes.size(), &state);
        if (state.invalidChars == 0) {
            if (!decoded.isEmpty() && decoded.at(0) == QChar(0xFEFF))
                decoded.remove(0, 1);
            return decoded;
        }
    }

    // 6. Shift-JIS / CP932 fallback (Japanese lab instruments: Shimadzu, Rigaku, Keyence, JEOL)
    QTextCodec *sjis = QTextCodec::codecForName("Shift-JIS");
    if (!sjis)
        sjis = QTextCodec::codecForName("Windows-31J");
    if (!sjis)
        sjis = QTextCodec::codecForName("CP932");
    if (sjis) {
        QTextCodec::ConverterState sjisState;
        QString decoded = sjis->toUnicode(rawBytes.constData(), rawBytes.size(), &sjisState);
        if (sjisState.invalidChars == 0)
            return decoded;
        return decoded;
    }

    return QString::fromLatin1(rawBytes);
}

void AsciiTableImportFilter::splitHeaderNameAndUnits(const QString &rawHeader,
                                                     QString &outShortName,
                                                     QString &outLongName,
                                                     QString &outUnits)
{
    outShortName.clear();
    outLongName.clear();
    outUnits.clear();

    const QString trimmed = rawHeader.trimmed();
    if (trimmed.isEmpty())
        return;

    QString base = trimmed;
    QString unit;

    // 1. Check trailing [unit], e.g. "Time [s]", "Strain [%]"
    if (trimmed.endsWith(QLatin1Char(']'))) {
        int depth = 0;
        int openIdx = -1;
        for (int i = trimmed.size() - 1; i >= 0; --i) {
            if (trimmed[i] == QLatin1Char(']'))
                ++depth;
            else if (trimmed[i] == QLatin1Char('[')) {
                --depth;
                if (depth == 0) {
                    openIdx = i;
                    break;
                }
            }
        }
        if (openIdx > 0) {
            const QString lhs = trimmed.left(openIdx).trimmed();
            const QString cand = trimmed.mid(openIdx + 1, trimmed.size() - openIdx - 2).trimmed();
            if (!lhs.isEmpty() && !cand.isEmpty() && cand.size() <= 32
                && !isPureDigitsString(cand)) {
                base = lhs;
                unit = cand;
            }
        }
    }

    // 2. Check trailing (unit), e.g. "Nominal stress, σ (MPa)", "Strain (%)", "ひずみ (%)"
    if (unit.isEmpty() && trimmed.endsWith(QLatin1Char(')'))) {
        int depth = 0;
        int openIdx = -1;
        for (int i = trimmed.size() - 1; i >= 0; --i) {
            if (trimmed[i] == QLatin1Char(')'))
                ++depth;
            else if (trimmed[i] == QLatin1Char('(')) {
                --depth;
                if (depth == 0) {
                    openIdx = i;
                    break;
                }
            }
        }
        if (openIdx > 0) {
            const QString lhs = trimmed.left(openIdx).trimmed();
            const QString cand = trimmed.mid(openIdx + 1, trimmed.size() - openIdx - 2).trimmed();
            if (!lhs.isEmpty() && isLikelyScientificUnit(cand)) {
                base = lhs;
                unit = cand;
            }
        }
    }

    // 3. Check ISIJ-style "Quantity / Unit", e.g. "Temperature / K", "Nominal stress, σ / MPa"
    if (unit.isEmpty()) {
        const int slashSpacePos = trimmed.lastIndexOf(QLatin1String(" / "));
        if (slashSpacePos > 0) {
            const QString lhs = trimmed.left(slashSpacePos).trimmed();
            const QString cand = trimmed.mid(slashSpacePos + 3).trimmed();
            if (!lhs.isEmpty() && !isPureNumberString(lhs) && isLikelyScientificUnit(cand)) {
                base = lhs;
                unit = cand;
            }
        }
    }

    outUnits = unit;

    if (isCleanColumnIdentifier(base)) {
        outShortName = base;
        outLongName = unit.isEmpty() ? QString() : base;
    } else {
        outLongName = base;
        outShortName = sanitizeShortColumnIdentifier(base);
        if (outShortName.isEmpty())
            outShortName = base;
    }
}

future::Table *AsciiTableImportFilter::importLabTextData(const QByteArray &rawBytes,
                                                         const QString &tableName,
                                                         const QString &filePathHint,
                                                         const QString &separatorHint,
                                                         const QString &encodingHint,
                                                         int unitsRowHint)
{
    return importLabTextDataInternal(rawBytes, tableName, filePathHint, separatorHint,
                                     encodingHint, unitsRowHint, 0 /*ignoredLines*/,
                                     true /*firstRowNamesColumns*/, true /*trimSpaces*/,
                                     false /*simplifySpaces*/, true /*convertToNumeric*/,
                                     QLocale::c(), true /*autoFixTabOnCsv*/);
}

AbstractAspect *AsciiTableImportFilter::importAspect(QIODevice &input)
{
    QString filePathHint;
    if (const QFile *qf = qobject_cast<const QFile *>(&input))
        filePathHint = qf->fileName();

    const QByteArray rawBytes = input.readAll();
    const bool trimEffective = d_trim_whitespace || d_simplify_whitespace || d_convert_to_numeric;

    return importLabTextDataInternal(rawBytes, tr("Table"), filePathHint, d_separator, QString(),
                                     -1 /*auto units row*/, d_ignored_lines,
                                     d_first_row_names_columns, trimEffective,
                                     d_simplify_whitespace, d_convert_to_numeric, d_numeric_locale,
                                     true /*autoFixTabOnCsv*/);
}
