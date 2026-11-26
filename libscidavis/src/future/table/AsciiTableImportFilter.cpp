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
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTextCodec>
#include <QTextStream>
#include <QVector>

#include "I18nJapanese.h"

#include <mutex>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

QStringList AsciiTableImportFilter::fileExtensions() const
{
    return QStringList() << "txt"
                         << "csv"
                         << "tsv"
                         << "dat"
                         << "ras"
                         << "xy"
                         << "xye"
                         << "asc"
                         << "chi"
                         << "uxd"
                         << "mpt"
                         << "dta"
                         << "tab";
}

namespace {

// Encoding chosen by the most recent decodeLabTextAuto() call and the report of the most recent
// import (both only touched from the GUI thread).
QString g_lastDecodedEncoding;
int g_lastDecodeInvalidChars = 0;
QString g_dialogEncoding;
int g_dialogUnitsRow = -1;
AsciiTableImportFilter::ImportReport g_lastImportReport;

// Upper bounds of one text import, checked before the column buffers (widest line x data
// lines) are allocated. A few malformed lines with far more fields than the rest would
// otherwise allocate mostly padding and end in bad_alloc, while long two- or three-column
// instrument logs (tens of millions of lines) must still be read.
//  - kMaxAsciiImportColumns: widest line accepted (same order as XlsxImport's kMaxCols).
//  - Above kAsciiImportOutlierCells cells (1e8 doubles = 800 MB), the file is refused when its
//    width comes from outlier lines only: lines with more than kAsciiImportOutlierFactor times
//    the median number of fields that make up less than 1 % of the lines.
//  - kMaxAsciiImportCells: absolute bound (2^31 cells = 16 GiB of doubles). A rectangular file
//    that fits the int-sized text buffer never reaches it, since every cell takes at least one
//    character.
constexpr int kMaxAsciiImportColumns = 65536;
constexpr qint64 kAsciiImportOutlierCells = 100000000LL;
constexpr int kAsciiImportOutlierFactor = 16;
constexpr qint64 kMaxAsciiImportCells = qint64(1) << 31;
int g_maxImportColumns = kMaxAsciiImportColumns;
qint64 g_importOutlierCells = kAsciiImportOutlierCells;
qint64 g_maxImportCells = kMaxAsciiImportCells;

void attachStdDisplayFormat(Column *col)
{
    if (!col)
        return;
    if (auto *d2s = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
        d2s->setNumericFormat('g');
        d2s->setNumDigits(6);
    }
}

// Japanese texts of the import report and of the drop / append-import messages in
// ApplicationWindow (registered here because every import passes through this filter).
void registerImportJapaneseStrings()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("%1 scans", "%1 スキャン");
        reg("%1 rows x %2 columns", "%1 行 × %2 列");
        reg("encoding: %1", "文字コード: %1");
        reg("separator: %1", "区切り: %1");
        reg("tab", "タブ");
        reg("spaces", "空白");
        reg("unit row: yes", "単位行: あり");
        reg("unit row: no", "単位行: なし");
        reg("%n line(s) skipped", "読み飛ばした行 %n");
        reg("%n empty cell(s)", "空欄 %n 個");
        reg("%n non-numeric cell(s) marked invalid", "数値列内の非数値セル %n 個を無効値にしました");
        reg("%n column(s) imported as text", "文字列として読んだ列 %n");
        reg("%n undecodable character(s) - the text encoding may be wrong",
            "文字コードの不一致により %n 文字が正しく読めませんでした");
        reg("Scan %1", "スキャン %1");
        reg("Imported %1", "読み込みました: %1");
        reg("File Not Imported", "読み込めなかったファイル");
        reg("Excel workbooks cannot be read directly.\n"
            "In Excel, select the cells, copy them (Ctrl+C) and paste them into a "
            "worksheet here (Ctrl+V), or save the sheet as CSV and drop the CSV file.",
            "Excel のブック（.xlsx など）は直接読み込めません。\n"
            "Excel で範囲を選んでコピー（Ctrl+C）し、ここのワークシートに貼り付ける（Ctrl+V）か、"
            "CSV で保存してから CSV ファイルをドロップしてください。");
        reg("These files could not be read as data, graph or project files.\n"
            "Text tables (CSV, TXT, DAT), Rigaku .ras, images and project files can be "
            "dropped. For other formats, save the data as CSV first.",
            "次のファイルはデータ・図・プロジェクトとして読み込めませんでした。\n"
            "ドロップできるのは、文字の表（CSV・TXT・DAT）、Rigaku の .ras、画像、プロジェクトファイルです。"
            "それ以外の形式は、先に CSV で保存してください。");
        reg("Column \"%1\" of the file has a different data type (%2) than column \"%3\" of "
            "the worksheet (%4). Nothing was imported.",
            "ファイルの列「%1」（%2）とワークシートの列「%3」（%4）のデータ型が違うため、"
            "何も読み込みませんでした。");
        reg("numeric", "数値");
        reg("text/date", "文字列・日時");
        reg("These columns could not be imported from %1: %2",
            "%1 から次の列を読み込めませんでした: %2");
        reg("A line of the file has %1 fields, but at most %2 columns can be imported. Check "
            "the column separator.",
            "ファイルに %1 個の項目を持つ行がありますが、読み込める列は %2 列までです。"
            "区切り文字を確認してください。");
        reg("%1 line(s) of the file have far more fields (up to %2) than the other lines "
            "(typically %3), which would make a table of %4 rows x %2 columns. Check the column "
            "separator or remove those lines.",
            "ファイルの %1 行だけが他の行（通常 %3 項目）よりはるかに多い項目（最大 %2）を持つため、"
            "%4 行 × %2 列の表が必要になります。区切り文字を確認するか、その行を削除してください。");
        reg("The file is too large to import: %1 rows x %2 columns (at most %3 cells).",
            "ファイルが大きすぎるため読み込めません: %1 行 × %2 列（上限 %3 セル）。");
    });
}

struct LabCsvCell
{
    QString value;
    bool quoted = false;
};

bool isMissingDataTokenImpl(const QString &raw)
{
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return true;
    if (s == QLatin1String("-") || s == QLatin1String("--") || s == QLatin1String("---")
        || s == QStringLiteral("\u2014") || s == QStringLiteral("\u2013")) {
        return true;
    }
    const QString lo = s.toLower();
    return lo == QLatin1String("nan") || lo == QLatin1String("+nan") || lo == QLatin1String("-nan")
            || lo == QLatin1String("na") || lo == QLatin1String("n/a")
            || lo == QLatin1String("n.d.") || lo == QLatin1String("nd")
            || lo == QLatin1String("null") || lo == QLatin1String("#n/a")
            || lo == QLatin1String("#div/0!") || lo == QLatin1String("#value!")
            || lo == QLatin1String("#ref!") || lo == QLatin1String("#name?")
            || lo == QLatin1String("#num!") || lo == QLatin1String("#null!")
            || lo == QLatin1String("#getting_data") || lo == QLatin1String("#spill!")
            || lo == QLatin1String("#calc!");
}

//! ASCII equivalent of a full-width number character ("１", "．", "－", "＋", "Ｅ"), 0 for others.
char asciiForFullwidthNumberChar(ushort u)
{
    if (u >= 0xFF10 && u <= 0xFF19)
        return static_cast<char>('0' + (u - 0xFF10));
    switch (u) {
    case 0xFF0E:
        return '.';
    case 0xFF0D:
        return '-';
    case 0xFF0B:
        return '+';
    case 0xFF25:
        return 'E';
    case 0xFF45:
        return 'e';
    default:
        return 0;
    }
}

//! Copies a number token into `buf` as ASCII (full-width digits, point, signs and exponent
//! mapped to ASCII; a Fortran exponent "1.0D+00" / "2.5d-3" turned into 'e'). Returns the
//! length, or -1 when the token holds other non-ASCII characters or blanks.
int asciiNumberTokenToBuffer(const QChar *ptr, int len, char *buf, int bufSize,
                             bool decimalComma = false)
{
    if (!ptr || len <= 0 || len >= bufSize)
        return -1;
    int n = 0;
    int dPos = -1;
    bool seenComma = false;
    bool seenDot = false;
    for (int i = 0; i < len; ++i) {
        ushort u = ptr[i].unicode();
        if (u >= 0x80) {
            const char c = asciiForFullwidthNumberChar(u);
            if (!c)
                return -1;
            u = static_cast<uchar>(c);
        }
        if (u == ' ' || u == '\t')
            return -1;
        if (u == '.')
            seenDot = true;
        if (u == ',') {
            if (!decimalComma || seenComma || seenDot)
                return -1;
            seenComma = true;
            u = '.';
        }
        if (u == 'D' || u == 'd')
            dPos = (dPos == -1) ? n : -2;
        buf[n++] = static_cast<char>(u);
    }
    if (seenComma && seenDot)
        return -1;
    if (dPos >= 1) {
        // Fortran double-precision exponent: digits with a point on the left ("1.0D+00",
        // "1.D5", or "3,50D-02" when decimalComma is active), an integer exponent on the right.
        // Without the point it is an identifier ("2D3", "3D-1", "3D-2": sample / position names)
        // and stays text.
        bool mantissaDigits = false, mantissaPoint = false, mantissaOk = true;
        for (int i = 0; i < dPos; ++i) {
            const char c = buf[i];
            if (c >= '0' && c <= '9')
                mantissaDigits = true;
            else if (c == '.')
                mantissaPoint = true;
            else if (!((c == '+' || c == '-') && i == 0))
                mantissaOk = false;
        }
        int e = dPos + 1;
        const bool expSigned = e < n && (buf[e] == '+' || buf[e] == '-');
        if (expSigned)
            ++e;
        bool expOk = e < n;
        for (int i = e; i < n; ++i)
            expOk = expOk && buf[i] >= '0' && buf[i] <= '9';
        if (mantissaOk && mantissaDigits && expOk && mantissaPoint)
            buf[dPos] = 'e';
    }
    return n;
}

//! A plain number whose magnitude overflows a double ("1e400", "-1.0D+999"): read as a missing
//! value, like "inf", so that one such cell does not turn its column into text.
bool isOverflowNumberSpan(const QChar *ptr, int len)
{
    char buf[72];
    const int n = asciiNumberTokenToBuffer(ptr, len, buf, sizeof(buf) - 1);
    if (n <= 0)
        return false;
    bool hasExp = false;
    for (int i = 0; i < n; ++i) {
        const char c = buf[i];
        if (c == 'e' || c == 'E')
            hasExp = true;
        else if (!((c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-'))
            return false;
    }
    if (!hasExp)
        return false;
    buf[n] = '\0';
    char *end = nullptr;
    const double v = std::strtod(buf, &end);
    return end == buf + n && std::isinf(v);
}

bool isMissingNumericToken(const QString &trimmed, bool wasQuoted)
{
    if (isMissingDataTokenImpl(trimmed))
        return true;
    const QString lo = trimmed.toLower();
    if (lo == QLatin1String("none"))
        return true;
    if (!wasQuoted
        && (lo == QLatin1String("inf") || lo == QLatin1String("+inf")
            || lo == QLatin1String("-inf") || lo == QLatin1String("infinity")
            || lo == QLatin1String("+infinity") || lo == QLatin1String("-infinity")
            || isOverflowNumberSpan(trimmed.constData(), trimmed.size()))) {
        return true;
    }
    return false;
}

bool tryParseAsciiDoubleSpan(const QChar *ptr, int len, double &outVal, bool decimalComma = false)
{
    if (!ptr || len <= 0 || len > 60)
        return false;
    char buf[64];
    const int n = asciiNumberTokenToBuffer(ptr, len, buf, sizeof(buf), decimalComma);
    if (n <= 0)
        return false;
    const int startIdx = (buf[0] == '+') ? 1 : 0;
    if (startIdx >= n)
        return false;
    double val = 0.0;
    const auto res = std::from_chars(buf + startIdx, buf + n, val);
    if (res.ec == std::errc() && res.ptr == buf + n && std::isfinite(val)) {
        outVal = val;
        return true;
    }
    return false;
}

bool isMissingNumericSpan(const QChar *ptr, int len, bool wasQuoted)
{
    if (len <= 0)
        return true;
    if (len <= 3) {
        if (len == 1
            && (ptr[0] == QLatin1Char('-') || ptr[0] == QChar(0x2014) || ptr[0] == QChar(0x2013)))
            return true;
        if (len == 2 && ptr[0] == QLatin1Char('-') && ptr[1] == QLatin1Char('-'))
            return true;
        if (len == 3 && ptr[0] == QLatin1Char('-') && ptr[1] == QLatin1Char('-')
            && ptr[2] == QLatin1Char('-'))
            return true;
    }
    if (len <= 16) {
        return isMissingNumericToken(QString(ptr, len), wasQuoted);
    }
    return !wasQuoted && isOverflowNumberSpan(ptr, len);
}

bool tryParseLabDouble(const QString &trimmed, const QLocale &primaryLoc, bool useCustomLocale,
                       double &outVal)
{
    if (!useCustomLocale && tryParseAsciiDoubleSpan(trimmed.constData(), trimmed.size(), outVal))
        return true;
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
    const bool decimalComma = (primaryLoc.decimalPoint() == QLatin1Char(','));
    if (decimalComma
        && tryParseAsciiDoubleSpan(trimmed.constData(), trimmed.size(), outVal, true))
        return true;
    if (tryParseAsciiDoubleSpan(trimmed.constData(), trimmed.size(), outVal, false))
        return true;
    outVal = cLoc.toDouble(trimmed, &ok);
    return ok && std::isfinite(outVal);
}

bool tryParseLabDoubleSpan(const QChar *ptr, int len, const QLocale &primaryLoc,
                           bool useCustomLocale, double &outVal)
{
    if (!useCustomLocale && tryParseAsciiDoubleSpan(ptr, len, outVal))
        return true;
    return tryParseLabDouble(QString(ptr, len), primaryLoc, useCustomLocale, outVal);
}

bool isMissingDataTokenSpan(const QChar *ptr, int len)
{
    if (!ptr || len <= 0)
        return true;
    if (len <= 3) {
        if (len == 1
            && (ptr[0] == QLatin1Char('-') || ptr[0] == QChar(0x2014) || ptr[0] == QChar(0x2013)))
            return true;
        if (len == 2 && ptr[0] == QLatin1Char('-') && ptr[1] == QLatin1Char('-'))
            return true;
        if (len == 3 && ptr[0] == QLatin1Char('-') && ptr[1] == QLatin1Char('-')
            && ptr[2] == QLatin1Char('-'))
            return true;
    }
    if (len <= 16)
        return isMissingDataTokenImpl(QString(ptr, len));
    return false;
}

// Unified '#'-leading row classifier shared across Step 3, detectPreamble, peek, and body streaming.
// `cellGetter(k, pk, nk, kQuoted)` supplies trimmed view & quote flag for cell k in 0..rawFieldCount-1.
// When `allowAlphaHashIds` is true (after pre-data header/data), `#<alnum>` specimen IDs like
// `#A1, 520` or `#S-2, 410` with >=2 fields and trailing numeric values are treated as data rows,
// while `# comment`, `# pause,`, and `#comment,abc` (no numeric trailing values) remain comments.
template <typename CellGetter>
bool isHashCommentRecordCells(int rawFieldCount, bool whitespaceDelim, bool tableHasMultipleCols,
                              bool allowAlphaHashIds, const QLocale &primaryLoc,
                              bool useCustomLocale, CellGetter &&cellGetter)
{
    if (rawFieldCount <= 0)
        return false;
    const QChar *p0 = nullptr;
    int n0 = 0;
    bool col0Quoted = false;
    cellGetter(0, p0, n0, col0Quoted);
    // 1) Quoted cell 0, empty, or does not start with '#' -> data (false)
    if (col0Quoted || !p0 || n0 <= 0 || p0[0] != QLatin1Char('#'))
        return false;
    // 2) Missing data token (#N/A, #DIV/0!, #VALUE!, #REF!, #NAME?, #NUM!, #NULL!, #SPILL!, #CALC!, #GETTING_DATA) -> ALWAYS data (false)
    if (isMissingDataTokenSpan(p0, n0))
        return false;
    // 3) '#' alone or '# ' + whitespace (# comment, # col1, col2, # pause,, # note,,, # T=300 K, P=1 atm, # 1\t2) -> ALWAYS comment (true)
    if (n0 == 1 || p0[1].isSpace())
        return true;
    // 4) '#<non-space>' and not a missing token:
    //    - '#<digit>...' specimen identifiers (#1, #2, #01, #1-A) can be data rows.
    //    - Once past pre-data preamble (`allowAlphaHashIds`), '#<alnum>...' specimen IDs (#A1, #S-2)
    //      are also routed as data when multi-column and trailing cells hold numeric data.
    if (!p0[1].isDigit()) {
        if (!allowAlphaHashIds || rawFieldCount < 2)
            return true;
        bool anyTrailingNum = false;
        bool anyTrailingMissing = false;
        for (int k = 1; k < rawFieldCount; ++k) {
            const QChar *pk = nullptr;
            int nk = 0;
            bool kQuoted = false;
            cellGetter(k, pk, nk, kQuoted);
            if (nk <= 0)
                continue;
            if (isMissingNumericSpan(pk, nk, kQuoted)) {
                if (isMissingDataTokenSpan(pk, nk))
                    anyTrailingMissing = true;
                continue;
            }
            double dummy = 0.0;
            if (tryParseLabDoubleSpan(pk, nk, primaryLoc, useCustomLocale, dummy)) {
                anyTrailingNum = true;
            } else if (whitespaceDelim) {
                return true;
            }
        }
        return !(anyTrailingNum || anyTrailingMissing);
    }
    if (whitespaceDelim) {
        if (rawFieldCount >= 2) {
            for (int k = 1; k < rawFieldCount; ++k) {
                const QChar *pk = nullptr;
                int nk = 0;
                bool kQuoted = false;
                cellGetter(k, pk, nk, kQuoted);
                if (nk <= 0 || isMissingNumericSpan(pk, nk, kQuoted))
                    continue;
                double dummy = 0.0;
                if (!tryParseLabDoubleSpan(pk, nk, primaryLoc, useCustomLocale, dummy))
                    return true;
            }
            return false;
        }
        return tableHasMultipleCols;
    }
    if (rawFieldCount >= 2)
        return false;
    return tableHasMultipleCols;
}

bool isPureNumberString(const QString &s)
{
    const QString t = s.trimmed();
    if (t.isEmpty())
        return false;
    double v = 0.0;
    if (tryParseAsciiDoubleSpan(t.constData(), t.size(), v))
        return true;
    bool ok = false;
    QLocale cLoc = QLocale::c();
    cLoc.setNumberOptions(QLocale::RejectGroupSeparator);
    v = cLoc.toDouble(t, &ok);
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
            QStringLiteral("^((?:[pnμµucdkhmM])?m)[23]$"));
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

    // Do not treat missing-value / Excel-error sentinels as units (keep '-' / '—' for dimensionless)
    if (cand != QLatin1String("-") && cand != QStringLiteral("\u2014")
        && cand != QStringLiteral("\u2013") && isMissingDataTokenImpl(cand)) {
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
    // "1/s", "1/K", "1/min": a reciprocal unit, the "1" is its numerator.
    const bool reciprocal = parts.size() >= 2 && parts.first() == QLatin1String("1")
            && cand.startsWith(QLatin1String("1/"));
    for (int i = 0; i < parts.size(); ++i) {
        if (i == 0 && reciprocal)
            continue;
        if (!isSingleUnitAtom(parts.at(i)))
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

bool hasFallbackUnitSyntaxAscii(const QString &raw)
{
    const QString s = raw.trimmed();
    if (s.isEmpty() || s.size() > 24)
        return false;
    if ((s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')')))
        || (s.startsWith(QLatin1Char('[')) && s.endsWith(QLatin1Char(']')))) {
        return true;
    }
    for (const QChar ch : s) {
        const ushort u = ch.unicode();
        if (u == '/' || u == '^' || u == '%' || u == '*' || u == '-'
            || u == 0x2030 /*‰*/ || u == 0x00B0 /*°*/ || u == 0x2103 /*℃*/
            || u == 0x2109 /*℉*/ || u == 0x03A9 /*Ω*/ || u == 0x03BC /*μ*/
            || u == 0x00B5 /*µ*/ || u == 0x00C5 /*Å*/ || u == 0x00B7 /*·*/
            || u == 0x22C5 /*⋅*/ || u == 0x00B2 /*²*/ || u == 0x00B3 /*³*/
            || u == 0x207B /*⁻*/ || u == 0x2014 /*—*/ || u == 0x2013 /*–*/) {
            return true;
        }
    }
    return false;
}

bool isDimensionlessDashUnit(const QString &raw)
{
    const QString s = cleanPromotedUnitCell(raw);
    return s == QLatin1String("-") || s == QStringLiteral("\u2014")
            || s == QStringLiteral("\u2013")
            || s.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0;
}

int countUnquotedChar(const QString &line, QChar delim)
{
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
}

QStringList splitUnquotedSimple(const QString &line, QChar delim, QVector<bool> *outQuoted = nullptr)
{
    QStringList parts;
    if (outQuoted)
        outQuoted->clear();
    QString cur;
    bool inQuotes = false;
    bool atFieldStart = true;
    bool curQuoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
                    cur.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur.append(c);
            }
        } else if (c == delim) {
            parts.append(cur.trimmed());
            if (outQuoted)
                outQuoted->append(curQuoted);
            cur.clear();
            curQuoted = false;
            atFieldStart = true;
        } else if (c == QLatin1Char('"') && atFieldStart) {
            inQuotes = true;
            curQuoted = true;
            atFieldStart = false;
        } else {
            if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
                atFieldStart = false;
            cur.append(c);
        }
    }
    parts.append(cur.trimmed());
    if (outQuoted)
        outQuoted->append(curQuoted);
    return parts;
}

bool isDigitCommaDecimalToken(const QString &tok)
{
    static const QRegularExpression kRe(
            QStringLiteral("^[+-]?\\d+,\\d+(?:[eEdD][+-]?\\d+)?$"));
    return kRe.match(tok.trimmed()).hasMatch();
}

bool isDotDecimalToken(const QString &tok)
{
    static const QRegularExpression kRe(
            QStringLiteral("^[+-]?\\d+\\.\\d+(?:[eEdD][+-]?\\d+)?$"));
    return kRe.match(tok.trimmed()).hasMatch();
}

bool looksLikeEuropeanSemicolonLines(const QStringList &lines)
{
    QVector<int> semiCounts;
    QStringList semiLines;
    for (const QString &raw : lines) {
        const QString ln = raw.trimmed();
        if (ln.isEmpty() || ln.startsWith(QLatin1Char('#')))
            continue;
        if (countUnquotedChar(ln, QLatin1Char('\t')) > 0)
            return false;
        const int sc = countUnquotedChar(ln, QLatin1Char(';'));
        if (sc > 0) {
            semiCounts.append(sc);
            semiLines.append(ln);
        }
    }
    if (semiLines.isEmpty())
        return false;

    // Find modal positive ';' count across lines containing ';'
    QHash<int, int> freq;
    int modeSemi = 0;
    int modeFreq = 0;
    for (int sc : semiCounts) {
        const int f = ++freq[sc];
        if (f > modeFreq) {
            modeFreq = f;
            modeSemi = sc;
        }
    }
    if (modeSemi < 1)
        return false;
    if (semiLines.size() >= 2 && modeFreq * 4 < semiLines.size() * 3)
        return false;

    int commaDec = 0;
    int dotDec = 0;
    int bareInt = 0;
    for (int i = 0; i < semiLines.size(); ++i) {
        if (semiCounts[i] != modeSemi)
            continue;
        QVector<bool> quotedFlags;
        const QStringList parts = splitUnquotedSimple(semiLines[i], QLatin1Char(';'), &quotedFlags);
        for (int c = 0; c < parts.size(); ++c) {
            if (quotedFlags.value(c))
                continue;
            const QString tok = parts[c].trimmed();
            if (tok.isEmpty() || isMissingNumericToken(tok, false))
                continue;
            if (isDigitCommaDecimalToken(tok))
                ++commaDec;
            else if (isDotDecimalToken(tok))
                ++dotDec;
            else if (isPureDigitsString(tok.startsWith(QLatin1Char('-')) || tok.startsWith(QLatin1Char('+'))
                                                ? tok.mid(1)
                                                : tok))
                ++bareInt;
        }
    }
    if (commaDec <= 0 || dotDec > 0)
        return false;
    if (modeFreq >= 2)
        return commaDec >= 2 || (commaDec >= 1 && (commaDec + bareInt) >= 2);
    return commaDec >= 2 || (commaDec >= 1 && (commaDec + bareInt) >= modeSemi + 1);
}

QString detectDelimiterFromSampleLines(const QStringList &sampleLines, const QString &filePathHint)
{
    const bool isCsvExt = filePathHint.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive);
    const bool isTsvExt = filePathHint.endsWith(QLatin1String(".tsv"), Qt::CaseInsensitive);

    if (sampleLines.isEmpty())
        return isCsvExt ? QStringLiteral(",") : QStringLiteral("\t");

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
            const int c = countUnquotedChar(ln, ds.ch);
            if (c > 0)
                ++ds.linesWithDelim;
            ds.minCnt = std::min(ds.minCnt, c);
            ds.maxCnt = std::max(ds.maxCnt, c);
            ds.totalCnt += c;
        }
    }

    if (isTsvExt && cands[0].linesWithDelim > 0)
        return QStringLiteral("\t");
    if (looksLikeEuropeanSemicolonLines(sampleLines))
        return QStringLiteral(";");
    const bool consistentSemi =
            (cands[2].linesWithDelim == nSample && cands[2].minCnt >= 1
             && cands[2].minCnt == cands[2].maxCnt && nSample >= 2);
    const bool consistentComma =
            (cands[1].linesWithDelim == nSample && cands[1].minCnt >= 1
             && cands[1].minCnt == cands[1].maxCnt);
    if (isCsvExt && cands[1].linesWithDelim > 0 && cands[0].linesWithDelim == 0
        && !(consistentSemi && !consistentComma)) {
        return QStringLiteral(",");
    }

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

future::Table *tryParseVaspTextInternal(const QString &decoded, const QString &effTableName,
                                        const QString &filePathHint, double *outFermiEV)
{
    if (outFermiEV)
        *outFermiEV = 0.0;
    if (decoded.isEmpty())
        return nullptr;

    const QString baseName = QFileInfo(filePathHint).fileName();
    const bool nameIsDoscar = baseName.startsWith(QLatin1String("DOSCAR"), Qt::CaseInsensitive);
    const bool nameIsEigenval = baseName.startsWith(QLatin1String("EIGENVAL"), Qt::CaseInsensitive);

    const QChar *buf = decoded.constData();
    const int len = decoded.size();
    int pos = 0;

    auto readLineTrimmed = [&]() -> QString {
        if (pos >= len)
            return QString();
        int lineEnd = pos;
        while (lineEnd < len && buf[lineEnd] != QLatin1Char('\r') && buf[lineEnd] != QLatin1Char('\n'))
            ++lineEnd;
        QString out(buf + pos, lineEnd - pos);
        pos = lineEnd;
        if (pos < len && buf[pos] == QLatin1Char('\r')) {
            ++pos;
            if (pos < len && buf[pos] == QLatin1Char('\n'))
                ++pos;
        } else if (pos < len && buf[pos] == QLatin1Char('\n')) {
            ++pos;
        }
        return out.trimmed();
    };

    auto splitDoubles = [](const QString &line, QVector<double> &valsOut) -> bool {
        valsOut.clear();
        const QChar *p = line.constData();
        const int n = line.size();
        int i = 0;
        while (i < n) {
            while (i < n && (p[i] == QLatin1Char(' ') || p[i] == QLatin1Char('\t')))
                ++i;
            if (i >= n)
                break;
            const int s = i;
            while (i < n && p[i] != QLatin1Char(' ') && p[i] != QLatin1Char('\t'))
                ++i;
            double v = 0.0;
            if (!tryParseAsciiDoubleSpan(p + s, i - s, v))
                return false;
            valsOut.append(v);
        }
        return !valsOut.isEmpty();
    };

    QString ln0;
    while (pos < len) {
        ln0 = readLineTrimmed();
        if (!ln0.isEmpty())
            break;
    }
    if (ln0.isEmpty())
        return nullptr;
    const QString ln1 = readLineTrimmed();
    const QString ln2 = readLineTrimmed();
    const QString ln3 = readLineTrimmed();
    const QString ln4 = readLineTrimmed(); // SYSTEM title
    const QString ln5 = readLineTrimmed();

    QVector<double> v0, v1, v2, v5;
    if (!splitDoubles(ln0, v0) || v0.size() < 4 || !splitDoubles(ln1, v1) || v1.size() < 4
        || !splitDoubles(ln2, v2) || !splitDoubles(ln5, v5)) {
        return nullptr;
    }
    const bool hasCarTag = (ln3.compare(QLatin1String("CAR"), Qt::CaseInsensitive) == 0
                            || ln3.startsWith(QLatin1String("CAR"), Qt::CaseInsensitive)
                            || ln3 == QLatin1String("0") || ln3 == QLatin1String("1"));
    if (!hasCarTag && !nameIsDoscar && !nameIsEigenval)
        return nullptr;

    // 1. VASP DOSCAR: line 5 has 5 tokens <EMAX> <EMIN> <NEDOS> <EFERMI> <WEIGHT>
    if ((v5.size() == 5 && !nameIsEigenval) || nameIsDoscar) {
        if (v5.size() < 5)
            return nullptr;
        const int nedos = static_cast<int>(std::llround(v5[2]));
        const double efermi = v5[3];
        if (nedos < 2 || nedos > 200000)
            return nullptr;
        if (outFermiEV)
            *outFermiEV = efermi;

        QVector<QVector<double>> totalRows;
        totalRows.reserve(nedos);
        int totCols = 0;
        for (int r = 0; r < nedos && pos < len; ++r) {
            const QString dl = readLineTrimmed();
            if (dl.isEmpty()) {
                --r;
                continue;
            }
            QVector<double> rowVals;
            if (!splitDoubles(dl, rowVals) || rowVals.size() < 3)
                break;
            totCols = std::max(totCols, rowVals.size());
            totalRows.append(std::move(rowVals));
        }
        const int nActual = totalRows.size();
        if (nActual < 2)
            return nullptr;
        const bool spinPol = (totCols >= 5);

        // Optional site-projected PDOS blocks (summed over ions into s, p, d channels)
        bool hasPdos = false;
        QVector<double> sUp(nActual, 0.0), pUp(nActual, 0.0), dUp(nActual, 0.0);
        QVector<double> sDw(nActual, 0.0), pDw(nActual, 0.0), dDw(nActual, 0.0);
        while (pos < len) {
            const QString hdr = readLineTrimmed();
            if (hdr.isEmpty())
                continue;
            QVector<double> hv;
            if (!splitDoubles(hdr, hv) || hv.size() != 5
                || static_cast<int>(std::llround(hv[2])) != nedos) {
                break;
            }
            for (int r = 0; r < nedos && pos < len; ++r) {
                const QString pl = readLineTrimmed();
                QVector<double> pv;
                if (!splitDoubles(pl, pv) || r >= nActual)
                    continue;
                if (!spinPol) {
                    if (pv.size() == 4) {
                        sUp[r] += pv[1];
                        pUp[r] += pv[2];
                        dUp[r] += pv[3];
                        hasPdos = true;
                    } else if (pv.size() >= 10) {
                        sUp[r] += pv[1];
                        pUp[r] += pv[2] + pv[3] + pv[4];
                        for (int k = 5; k < std::min(10, pv.size()); ++k)
                            dUp[r] += pv[k];
                        hasPdos = true;
                    }
                } else {
                    if (pv.size() == 7) {
                        sUp[r] += pv[1];
                        sDw[r] += std::fabs(pv[2]);
                        pUp[r] += pv[3];
                        pDw[r] += std::fabs(pv[4]);
                        dUp[r] += pv[5];
                        dDw[r] += std::fabs(pv[6]);
                        hasPdos = true;
                    } else if (pv.size() >= 19) {
                        sUp[r] += pv[1];
                        sDw[r] += std::fabs(pv[2]);
                        pUp[r] += pv[3] + pv[5] + pv[7];
                        pDw[r] += std::fabs(pv[4]) + std::fabs(pv[6]) + std::fabs(pv[8]);
                        for (int k = 9; k + 1 < std::min(19, pv.size()); k += 2) {
                            dUp[r] += pv[k];
                            dDw[r] += std::fabs(pv[k + 1]);
                        }
                        hasPdos = true;
                    }
                }
            }
        }

        auto makeCol = [&](const QString &shortNm, const QString &longNm, const QString &unit,
                           SciDAVis::PlotDesignation pd, const QVector<double> &src) -> Column * {
            std::unique_ptr<QVector<qreal>> vec(new QVector<qreal>(nActual, 0.0));
            for (int r = 0; r < nActual; ++r)
                (*vec)[r] = src[r];
            Column *c = new Column(shortNm, std::move(vec), IntervalAttribute<bool>());
            c->setLongName(longNm);
            c->setUnits(unit);
            c->setPlotDesignation(pd);
            attachStdDisplayFormat(c);
            return c;
        };

        QVector<double> eRaw(nActual), dos1(nActual), dos2(nActual);
        for (int r = 0; r < nActual; ++r) {
            eRaw[r] = totalRows[r][0];
            dos1[r] = totalRows[r][1];
            if (spinPol && totalRows[r].size() >= 3)
                dos2[r] = std::fabs(totalRows[r][2]);
        }

        QList<Column *> cols;
        Column *cE = makeCol(QStringLiteral("Energy"), QStringLiteral("Energy, E"),
                             QStringLiteral("eV"), SciDAVis::X, eRaw);
        cE->setComment(QStringLiteral("VASP_EFERMI=%1").arg(efermi, 0, 'g', 10));
        cols << cE;

        if (!spinPol) {
            cols << makeCol(QStringLiteral("DOS_Total"), QStringLiteral("Total DOS"),
                            QStringLiteral("states/eV"), SciDAVis::Y, dos1);
            if (hasPdos) {
                cols << makeCol(QStringLiteral("PDOS_s"), QStringLiteral("s-PDOS"),
                                QStringLiteral("states/eV"), SciDAVis::Y, sUp);
                cols << makeCol(QStringLiteral("PDOS_p"), QStringLiteral("p-PDOS"),
                                QStringLiteral("states/eV"), SciDAVis::Y, pUp);
                cols << makeCol(QStringLiteral("PDOS_d"), QStringLiteral("d-PDOS"),
                                QStringLiteral("states/eV"), SciDAVis::Y, dUp);
            }
        } else {
            cols << makeCol(QStringLiteral("DOS_up"), QStringLiteral("Total DOS (Spin \u2191)"),
                            QStringLiteral("states/eV"), SciDAVis::Y, dos1);
            cols << makeCol(QStringLiteral("DOS_down"), QStringLiteral("Total DOS (Spin \u2193)"),
                            QStringLiteral("states/eV"), SciDAVis::Y, dos2);
            if (hasPdos) {
                cols << makeCol(QStringLiteral("PDOS_s_up"), QStringLiteral("s (Spin \u2191)"),
                                QStringLiteral("states/eV"), SciDAVis::Y, sUp);
                cols << makeCol(QStringLiteral("PDOS_p_up"), QStringLiteral("p (Spin \u2191)"),
                                QStringLiteral("states/eV"), SciDAVis::Y, pUp);
                cols << makeCol(QStringLiteral("PDOS_d_up"), QStringLiteral("d (Spin \u2191)"),
                                QStringLiteral("states/eV"), SciDAVis::Y, dUp);
                cols << makeCol(QStringLiteral("PDOS_s_dw"), QStringLiteral("s (Spin \u2193)"),
                                QStringLiteral("states/eV"), SciDAVis::Y, sDw);
                cols << makeCol(QStringLiteral("PDOS_p_dw"), QStringLiteral("p (Spin \u2193)"),
                                QStringLiteral("states/eV"), SciDAVis::Y, pDw);
                cols << makeCol(QStringLiteral("PDOS_d_dw"), QStringLiteral("d (Spin \u2193)"),
                                QStringLiteral("states/eV"), SciDAVis::Y, dDw);
            }
        }

        future::Table *result = new future::Table(0, 0, effTableName);
        if (!ln4.isEmpty())
            result->setComment(ln4);
        result->appendColumns(cols);
        return result;
    }

    // 2. VASP EIGENVAL: line 5 has 3 tokens <NELECT> <NKPTS> <NBANDS>
    if ((v5.size() == 3 && !nameIsDoscar) || nameIsEigenval) {
        if (v5.size() < 3)
            return nullptr;
        const double nelect = v5[0];
        const int nkpts = static_cast<int>(std::llround(v5[1]));
        const int nbands = static_cast<int>(std::llround(v5[2]));
        const int ispin = std::clamp(static_cast<int>(std::llround(v0[3])), 1, 2);
        if (nkpts < 1 || nkpts > 50000 || nbands < 1 || nbands > 2048)
            return nullptr;

        QVector<double> kx(nkpts, 0.0), ky(nkpts, 0.0), kz(nkpts, 0.0), kDist(nkpts, 0.0);
        QVector<QVector<double>> bandsUp(nbands, QVector<double>(nkpts, 0.0));
        QVector<QVector<double>> bandsDw(ispin == 2 ? nbands : 0, QVector<double>(nkpts, 0.0));

        bool hasOcc = false;
        double maxOccEnergy = -std::numeric_limits<double>::infinity();
        int actualK = 0;

        for (int k = 0; k < nkpts && pos < len; ++k) {
            QString khdr;
            while (pos < len) {
                khdr = readLineTrimmed();
                if (!khdr.isEmpty())
                    break;
            }
            QVector<double> kv;
            if (!splitDoubles(khdr, kv) || kv.size() < 4)
                break;
            kx[k] = kv[0];
            ky[k] = kv[1];
            kz[k] = kv[2];
            if (k > 0) {
                const double dx = kx[k] - kx[k - 1];
                const double dy = ky[k] - ky[k - 1];
                const double dz = kz[k] - kz[k - 1];
                const double dk = std::sqrt(dx * dx + dy * dy + dz * dz);
                kDist[k] = kDist[k - 1] + ((dk > 0.5) ? 0.0 : dk);
            }
            for (int b = 0; b < nbands && pos < len; ++b) {
                const QString bl = readLineTrimmed();
                if (bl.isEmpty()) {
                    --b;
                    continue;
                }
                QVector<double> bv;
                if (!splitDoubles(bl, bv) || bv.size() < 2)
                    break;
                bandsUp[b][k] = bv[1];
                if (ispin == 2 && bv.size() >= 3) {
                    bandsDw[b][k] = bv[2];
                    if (bv.size() >= 5) {
                        hasOcc = true;
                        if (bv[3] >= 0.5)
                            maxOccEnergy = std::max(maxOccEnergy, bv[1]);
                        if (bv[4] >= 0.5)
                            maxOccEnergy = std::max(maxOccEnergy, bv[2]);
                    }
                } else if (ispin == 1 && bv.size() >= 3) {
                    hasOcc = true;
                    if (bv[2] >= 0.5)
                        maxOccEnergy = std::max(maxOccEnergy, bv[1]);
                }
            }
            ++actualK;
        }
        if (actualK < 1)
            return nullptr;

        double estFermi = 0.0;
        if (hasOcc && std::isfinite(maxOccEnergy)) {
            estFermi = maxOccEnergy;
        } else {
            const int occBandIdx = std::clamp(static_cast<int>(std::floor(nelect / 2.0)) - 1, 0,
                                              nbands - 1);
            double vbm = -std::numeric_limits<double>::infinity();
            for (int k = 0; k < actualK; ++k)
                vbm = std::max(vbm, bandsUp[occBandIdx][k]);
            if (std::isfinite(vbm))
                estFermi = vbm;
        }
        if (outFermiEV)
            *outFermiEV = estFermi;

        QList<Column *> cols;
        auto appendVecCol = [&](const QString &nm, const QString &lnm, const QString &u,
                                SciDAVis::PlotDesignation pd, const QVector<double> &src) -> Column * {
            std::unique_ptr<QVector<qreal>> vec(new QVector<qreal>(actualK, 0.0));
            for (int k = 0; k < actualK; ++k)
                (*vec)[k] = src[k];
            Column *c = new Column(nm, std::move(vec), IntervalAttribute<bool>());
            c->setLongName(lnm);
            c->setUnits(u);
            c->setPlotDesignation(pd);
            attachStdDisplayFormat(c);
            cols << c;
            return c;
        };

        Column *cK = appendVecCol(QStringLiteral("k_dist"), QStringLiteral("Wavevector, k"),
                                  QString(), SciDAVis::X, kDist);
        cK->setComment(QStringLiteral("VASP_EFERMI=%1").arg(estFermi, 0, 'g', 10));

        for (int b = 0; b < nbands; ++b) {
            appendVecCol(QStringLiteral("Band_%1").arg(b + 1),
                         QStringLiteral("Band %1").arg(b + 1), QStringLiteral("eV"),
                         SciDAVis::Y, bandsUp[b]);
        }
        if (ispin == 2) {
            for (int b = 0; b < nbands; ++b) {
                appendVecCol(QStringLiteral("Band_%1_dw").arg(b + 1),
                             QStringLiteral("Band %1 (\u2193)").arg(b + 1), QStringLiteral("eV"),
                             SciDAVis::Y, bandsDw[b]);
            }
        }

        future::Table *result = new future::Table(0, 0, effTableName);
        if (!ln4.isEmpty())
            result->setComment(ln4);
        result->appendColumns(cols);
        return result;
    }

    return nullptr;
}

struct ZeroCopyCellSpan
{
    const QChar *ptr = nullptr;
    int len = 0;
    QString unescaped;
    bool quoted = false;
    bool usesUnescaped = false;

    QString toQString(bool trimSpaces, bool simplifySpaces) const
    {
        if (usesUnescaped)
            return unescaped;
        if (!ptr || len <= 0)
            return QString();
        QString s(ptr, len);
        if (!quoted) {
            if (simplifySpaces)
                return s.simplified();
            if (trimSpaces)
                return s.trimmed();
        }
        return s;
    }

    void trimmedView(const QChar *&outPtr, int &outLen) const
    {
        if (usesUnescaped) {
            outPtr = unescaped.constData();
            outLen = unescaped.size();
        } else {
            outPtr = ptr;
            outLen = len;
        }
        if (!outPtr || outLen <= 0) {
            outLen = 0;
            return;
        }
        int s = 0;
        int e = outLen;
        while (s < e && outPtr[s].isSpace())
            ++s;
        while (e > s && outPtr[e - 1].isSpace())
            --e;
        outPtr += s;
        outLen = e - s;
    }
};

// Reads one logical CSV/whitespace record starting at `ioPos`, advancing `ioPos` to the next line.
// Returns false at EOF (`ioPos >= len`). Sets `outSkipRow = true` for blank or '#'-comment lines.
bool readNextRecordSpans(const QChar *buf, int len, int &ioPos, const QString &effSep,
                         bool whitespaceDelim, bool trimSpaces, bool simplifySpaces,
                         QVector<ZeroCopyCellSpan> &outRow, bool &outSkipRow)
{
    outRow.resize(0);
    outSkipRow = false;
    if (ioPos >= len)
        return false;

    if (whitespaceDelim) {
        int lineEnd = ioPos;
        while (lineEnd < len && buf[lineEnd] != QLatin1Char('\r')
               && buf[lineEnd] != QLatin1Char('\n')) {
            ++lineEnd;
        }
        int p = ioPos;
        while (p < lineEnd && (buf[p] == QLatin1Char(' ') || buf[p] == QLatin1Char('\t')))
            ++p;
        if (p >= lineEnd) {
            outSkipRow = true;
        } else {
            while (p < lineEnd) {
                while (p < lineEnd && (buf[p] == QLatin1Char(' ') || buf[p] == QLatin1Char('\t')))
                    ++p;
                if (p >= lineEnd)
                    break;
                ZeroCopyCellSpan cell;
                if (buf[p] == QLatin1Char('"')) {
                    cell.quoted = true;
                    ++p;
                    const int qStart = p;
                    bool hasEsc = false;
                    while (p < lineEnd) {
                        if (buf[p] == QLatin1Char('"')) {
                            if (p + 1 < lineEnd && buf[p + 1] == QLatin1Char('"')) {
                                hasEsc = true;
                                p += 2;
                            } else {
                                break;
                            }
                        } else {
                            ++p;
                        }
                    }
                    const int qEnd = p;
                    if (p < lineEnd && buf[p] == QLatin1Char('"'))
                        ++p;
                    if (!hasEsc) {
                        cell.ptr = buf + qStart;
                        cell.len = qEnd - qStart;
                    } else {
                        cell.usesUnescaped = true;
                        cell.unescaped.reserve(qEnd - qStart);
                        for (int k = qStart; k < qEnd; ++k) {
                            if (buf[k] == QLatin1Char('"') && k + 1 < qEnd
                                && buf[k + 1] == QLatin1Char('"')) {
                                cell.unescaped.append(QLatin1Char('"'));
                                ++k;
                            } else {
                                cell.unescaped.append(buf[k]);
                            }
                        }
                    }
                } else {
                    const int tokStart = p;
                    while (p < lineEnd && buf[p] != QLatin1Char(' ') && buf[p] != QLatin1Char('\t'))
                        ++p;
                    cell.ptr = buf + tokStart;
                    cell.len = p - tokStart;
                }
                outRow.append(std::move(cell));
            }
            if (outRow.isEmpty())
                outSkipRow = true;
        }
        ioPos = lineEnd;
        if (ioPos < len && buf[ioPos] == QLatin1Char('\r')) {
            ++ioPos;
            if (ioPos < len && buf[ioPos] == QLatin1Char('\n'))
                ++ioPos;
        } else if (ioPos < len && buf[ioPos] == QLatin1Char('\n')) {
            ++ioPos;
        }
        return true;
    }

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

    int pos = ioPos;
    while (true) {
        ZeroCopyCellSpan cell;
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

        // A quote that is still open at EOF does not start a multi-line field: like
        // scanLogicalLineEnd(), the record then ends at the physical line end and the cell is
        // read as plain text (otherwise every following line is swallowed into this one cell).
        bool quotedField = (scanPos < len && buf[scanPos] == QLatin1Char('"'));
        bool hasEsc = false;
        int quoteEnd = scanPos + 1;
        if (quotedField) {
            bool crossesLine = false;
            while (quoteEnd < len) {
                const QChar ch = buf[quoteEnd];
                if (ch == QLatin1Char('"')) {
                    if (quoteEnd + 1 < len && buf[quoteEnd + 1] == QLatin1Char('"')) {
                        hasEsc = true;
                        quoteEnd += 2;
                    } else {
                        break;
                    }
                } else {
                    if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n'))
                        crossesLine = true;
                    ++quoteEnd;
                }
            }
            if (quoteEnd >= len && crossesLine)
                quotedField = false;
        }

        if (quotedField) {
            cell.quoted = true;
            const int qStart = scanPos + 1;
            pos = quoteEnd;
            const int qEnd = pos;
            if (pos < len && buf[pos] == QLatin1Char('"'))
                ++pos;
            bool hasTrailing = false;
            int trailStart = pos;
            while (pos < len && buf[pos] != QLatin1Char('\r') && buf[pos] != QLatin1Char('\n')
                   && !matchesSepAt(pos)) {
                if (buf[pos] != QLatin1Char(' ') && buf[pos] != QLatin1Char('\t'))
                    hasTrailing = true;
                ++pos;
            }
            if (!hasEsc && !hasTrailing) {
                cell.ptr = buf + qStart;
                cell.len = qEnd - qStart;
            } else {
                cell.usesUnescaped = true;
                cell.unescaped.reserve((qEnd - qStart) + (pos - trailStart));
                for (int k = qStart; k < qEnd; ++k) {
                    if (buf[k] == QLatin1Char('"') && k + 1 < qEnd
                        && buf[k + 1] == QLatin1Char('"')) {
                        cell.unescaped.append(QLatin1Char('"'));
                        ++k;
                    } else {
                        cell.unescaped.append(buf[k]);
                    }
                }
                for (int k = trailStart; k < pos; ++k) {
                    if (buf[k] != QLatin1Char(' ') && buf[k] != QLatin1Char('\t'))
                        cell.unescaped.append(buf[k]);
                }
            }
        } else {
            pos = fieldStart;
            int rawStart = pos;
            while (pos < len && buf[pos] != QLatin1Char('\r') && buf[pos] != QLatin1Char('\n')
                   && !matchesSepAt(pos)) {
                ++pos;
            }
            int rawEnd = pos;
            if (trimSpaces || simplifySpaces) {
                while (rawStart < rawEnd && buf[rawStart].isSpace())
                    ++rawStart;
                while (rawEnd > rawStart && buf[rawEnd - 1].isSpace())
                    --rawEnd;
            }
            cell.ptr = buf + rawStart;
            cell.len = rawEnd - rawStart;
        }
        outRow.append(std::move(cell));

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

    ioPos = pos;
    if (outRow.isEmpty()) {
        outSkipRow = true;
    } else if (!outRow[0].quoted) {
        const QChar *p0 = nullptr;
        int n0 = 0;
        outRow[0].trimmedView(p0, n0);
        if (outRow.size() == 1 && n0 == 0)
            outSkipRow = true;
    }
    return true;
}

// Parse synchrotron SPEC scan files (#F / #S / #N / #L) into separate column groups per scan.
future::Table *tryParseSpecTextInternal(const QString &decoded, const QString &effTableName,
                                        bool convertToNumeric, const QLocale &numericLocale,
                                        bool firstRowNamesColumns)
{
    if (!decoded.contains(QLatin1String("#S")))
        return nullptr;
    if (!decoded.contains(QLatin1String("#L"))
        && !(decoded.contains(QLatin1String("#F")) && decoded.contains(QLatin1String("#N")))) {
        return nullptr;
    }

    const QStringList rawLines = decoded.split(QRegularExpression(QStringLiteral("\\r\\n|\\r|\\n")));
    int sCount = 0;
    int lCount = 0;
    bool hasFOrN = false;
    for (const QString &ln : rawLines) {
        const QString t = ln.trimmed();
        if (t.startsWith(QLatin1String("#S")) && (t.size() == 2 || t.at(2).isSpace()))
            ++sCount;
        else if (t.startsWith(QLatin1String("#L")) && (t.size() == 2 || t.at(2).isSpace()))
            ++lCount;
        else if ((t.startsWith(QLatin1String("#F")) || t.startsWith(QLatin1String("#N")))
                 && (t.size() == 2 || t.at(2).isSpace()))
            hasFOrN = true;
    }
    if (sCount == 0 || (lCount == 0 && !hasFOrN))
        return nullptr;

    struct SpecScan
    {
        QString scanTitle;
        QString labelBody;
        int declaredCols = 0;
        QVector<QStringList> rows;
    };

    QStringList preamble;
    QVector<SpecScan> scans;
    bool inScan = false;
    for (const QString &rawLn : rawLines) {
        const QString t = rawLn.trimmed();
        if (t.isEmpty())
            continue;
        if (t.startsWith(QLatin1String("#S")) && (t.size() == 2 || t.at(2).isSpace())) {
            SpecScan sc;
            sc.scanTitle = t.mid(2).trimmed();
            scans.append(sc);
            inScan = true;
            continue;
        }
        if (!inScan) {
            preamble.append(t);
            continue;
        }
        SpecScan &cur = scans.last();
        if (t.startsWith(QLatin1String("#L")) && (t.size() == 2 || t.at(2).isSpace())) {
            cur.labelBody = t.mid(2).trimmed();
        } else if (t.startsWith(QLatin1String("#N")) && (t.size() == 2 || t.at(2).isSpace())) {
            bool okN = false;
            const int n = t.mid(2).trimmed().section(QRegularExpression(QStringLiteral("\\s+")), 0, 0).toInt(&okN);
            if (okN && n > 0)
                cur.declaredCols = n;
        } else if (t.startsWith(QLatin1Char('#'))) {
            // other SPEC directives (#D, #T, #M, #P0, #C, #@...)
        } else {
            static const QRegularExpression kWsSep(QStringLiteral("\\s+"));
            QStringList fields = t.split(kWsSep, Qt::SkipEmptyParts);
            if (!fields.isEmpty())
                cur.rows.append(std::move(fields));
        }
    }

    // Keep only scans that actually hold data rows
    QVector<SpecScan> nonEmptyScans;
    for (SpecScan &sc : scans) {
        if (!sc.rows.isEmpty())
            nonEmptyScans.append(std::move(sc));
    }
    if (nonEmptyScans.isEmpty())
        return nullptr;

    auto splitSpecLabels = [](const QString &body, int expectedCols) -> QStringList {
        if (body.isEmpty())
            return {};
        if (body.contains(QLatin1Char('\t'))) {
            QStringList p = body.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
            for (QString &s : p)
                s = s.trimmed();
            return p;
        }
        static const QRegularExpression kTwoSpace(QStringLiteral("\\s{2,}"));
        static const QRegularExpression kOneSpace(QStringLiteral("\\s+"));
        QStringList p2 = body.split(kTwoSpace, Qt::SkipEmptyParts);
        if (expectedCols > 0 && p2.size() == expectedCols)
            return p2;
        QStringList p1 = body.split(kOneSpace, Qt::SkipEmptyParts);
        if (expectedCols > 0 && p1.size() == expectedCols)
            return p1;
        return (p2.size() >= 2) ? p2 : p1;
    };

    const bool useCustomLoc = (numericLocale.decimalPoint() != QLatin1Char('.'));
    // Same as the main path: never read the group separator, or German "12.345" becomes 12345.
    QLocale specLoc = numericLocale;
    specLoc.setNumberOptions(QLocale::RejectGroupSeparator);
    QSet<QString> usedShortNames;
    auto makeUniqueShort = [&usedShortNames](const QString &candidate, int idx) -> QString {
        QString base = sanitizeShortColumnIdentifier(candidate);
        if (base.isEmpty())
            base = QStringLiteral("Col%1").arg(idx + 1);
        QString name = base;
        int suffix = 2;
        while (usedShortNames.contains(name.toLower()))
            name = QStringLiteral("%1_%2").arg(base).arg(suffix++);
        usedShortNames.insert(name.toLower());
        return name;
    };

    QList<Column *> cols;
    int maxDataRows = 0;
    int missingCells = 0;
    int textCols = 0;
    for (int sIdx = 0; sIdx < nonEmptyScans.size(); ++sIdx) {
        const SpecScan &sc = nonEmptyScans.at(sIdx);
        int nCols = sc.declaredCols;
        for (const QStringList &r : sc.rows)
            nCols = std::max(nCols, int(r.size()));
        if (nCols <= 0)
            continue;
        const QStringList labels =
                firstRowNamesColumns ? splitSpecLabels(sc.labelBody, nCols) : QStringList();
        const int nRows = sc.rows.size();
        maxDataRows = std::max(maxDataRows, nRows);
        const QString scanSuffix = (nonEmptyScans.size() > 1)
                ? QStringLiteral("_S%1").arg(
                        sc.scanTitle.section(QRegularExpression(QStringLiteral("\\s+")), 0, 0).isEmpty()
                                ? QString::number(sIdx + 1)
                                : sc.scanTitle.section(QRegularExpression(QStringLiteral("\\s+")), 0, 0))
                : QString();

        for (int c = 0; c < nCols; ++c) {
            bool allNumeric = convertToNumeric;
            if (allNumeric) {
                for (int r = 0; r < nRows; ++r) {
                    const QString cell = (c < sc.rows[r].size()) ? sc.rows[r][c] : QString();
                    if (cell.isEmpty() || isMissingNumericToken(cell, false))
                        continue;
                    double dv = 0.0;
                    if (!tryParseLabDouble(cell, specLoc, useCustomLoc, dv)) {
                        allNumeric = false;
                        break;
                    }
                }
            }

            QString rawHdr = (c < labels.size()) ? labels.at(c) : QString();
            QString shortNm, longNm, units;
            if (!rawHdr.isEmpty())
                AsciiTableImportFilter::splitHeaderNameAndUnits(rawHdr, shortNm, longNm, units);
            if (shortNm.isEmpty())
                shortNm = (c == 0) ? QStringLiteral("X") : QStringLiteral("Y%1").arg(c);
            const QString uniqueShort = makeUniqueShort(shortNm + scanSuffix, cols.size());

            Column *col = nullptr;
            if (allNumeric) {
                std::unique_ptr<QVector<qreal>> vec(new QVector<qreal>(nRows, 0.0));
                IntervalAttribute<bool> inv;
                for (int r = 0; r < nRows; ++r) {
                    const QString cell = (c < sc.rows[r].size()) ? sc.rows[r][c] : QString();
                    double dv = 0.0;
                    if (!cell.isEmpty() && !isMissingNumericToken(cell, false)
                        && tryParseLabDouble(cell, specLoc, useCustomLoc, dv)
                        && std::isfinite(dv)) {
                        (*vec)[r] = dv;
                    } else {
                        inv.setValue(r, true);
                        ++missingCells;
                    }
                }
                col = new Column(uniqueShort, std::move(vec), inv);
                attachStdDisplayFormat(col);
            } else {
                std::unique_ptr<QStringList> vec(new QStringList());
                vec->reserve(nRows);
                for (int r = 0; r < nRows; ++r)
                    vec->append((c < sc.rows[r].size()) ? sc.rows[r][c] : QString());
                col = new Column(uniqueShort, std::move(vec), IntervalAttribute<bool>());
                ++textCols;
            }
            if (!longNm.isEmpty())
                col->setLongName(longNm);
            if (!units.isEmpty())
                col->setUnits(units);
            col->setPlotDesignation((c == 0) ? SciDAVis::X : SciDAVis::Y);
            if (!sc.scanTitle.isEmpty())
                col->setComment(QObject::tr("Scan %1").arg(sc.scanTitle));
            cols << col;
        }
    }

    if (cols.isEmpty())
        return nullptr;

    g_lastImportReport.separator = QStringLiteral(" ");
    g_lastImportReport.rasBlocks = nonEmptyScans.size();
    g_lastImportReport.dataRows = maxDataRows;
    g_lastImportReport.columns = cols.size();
    g_lastImportReport.missingCells = missingCells;
    g_lastImportReport.textColumns = textCols;
    g_lastImportReport.headerLines = (lCount > 0 && firstRowNamesColumns) ? 1 : 0;
    g_lastImportReport.preamble = preamble.mid(0, 200).join(QLatin1Char('\n'));

    future::Table *result = new future::Table(0, 0, effTableName);
    result->appendColumns(cols);
    if (!g_lastImportReport.preamble.isEmpty())
        result->setComment(g_lastImportReport.preamble);
    return result;
}

future::Table *importLabTextDataInternal(const QByteArray &rawBytes, const QString &tableName,
                                         const QString &filePathHint, const QString &separatorHint,
                                         const QString &encodingHint, int unitsRowHint,
                                         int ignoredLines, bool firstRowNamesColumns,
                                         bool trimSpaces, bool simplifySpaces,
                                         bool convertToNumeric, const QLocale &numericLocale,
                                         bool autoFixTabOnCsv, bool detectPreamble = false,
                                         bool preambleProbeOnly = false)
{
    g_lastImportReport = AsciiTableImportFilter::ImportReport();
    g_lastDecodedEncoding.clear();
    g_lastDecodeInvalidChars = 0;
    const QString effTableName =
            tableName.trimmed().isEmpty() ? QObject::tr("Table") : tableName.trimmed();
    if (rawBytes.isEmpty())
        return new future::Table(0, 0, effTableName);

    // Reject binary image / archive / raster magic headers (e.g. Sun Raster 0x59A66A95) & non-UTF16 NUL-byte blobs
    {
        const int nBytes = rawBytes.size();
        const unsigned char *u = reinterpret_cast<const unsigned char *>(rawBytes.constData());
        if (nBytes >= 4) {
            if ((u[0] == 0x59 && u[1] == 0xA6 && u[2] == 0x6A && u[3] == 0x95) // Sun Raster BE
                || (u[0] == 0x95 && u[1] == 0x6A && u[2] == 0xA6 && u[3] == 0x59) // Sun Raster LE
                || (u[0] == 0x89 && u[1] == 'P' && u[2] == 'N' && u[3] == 'G')
                || (u[0] == '%' && u[1] == 'P' && u[2] == 'D' && u[3] == 'F')
                || (u[0] == 'P' && u[1] == 'K' && u[2] == 0x03 && u[3] == 0x04)
                || (u[0] == 0xD0 && u[1] == 0xCF && u[2] == 0x11 && u[3] == 0xE0)
                || (u[0] == 'C' && u[1] == 'P' && u[2] == 'Y' && (u[3] == 'A' || u[3] == 'U'))) {
                return nullptr;
            }
        }
        const bool hasUtf16Bom = (nBytes >= 2
                                  && ((u[0] == 0xFF && u[1] == 0xFE)
                                      || (u[0] == 0xFE && u[1] == 0xFF)));
        if (!hasUtf16Bom) {
            const int checkBytes = std::min(nBytes, 4096);
            int nulCount = 0;
            for (int i = 0; i < checkBytes; ++i) {
                if (u[i] == 0)
                    ++nulCount;
            }
            if (nulCount > 0) {
                bool validBomlessUtf16 = false;
                if (nBytes >= 4 && (nBytes % 2 == 0)) {
                    const int samplePairs = std::min(nBytes / 2, 256);
                    int evenZeros = 0, oddZeros = 0;
                    for (int p = 0; p < samplePairs; ++p) {
                        if (u[2 * p] == 0)
                            ++evenZeros;
                        if (u[2 * p + 1] == 0)
                            ++oddZeros;
                    }
                    validBomlessUtf16 =
                            (oddZeros > samplePairs / 2 && evenZeros == 0)
                            || (evenZeros > samplePairs / 2 && oddZeros == 0);
                }
                if (!validBomlessUtf16)
                    return nullptr;
            }
        }
    }

    const QString decoded = AsciiTableImportFilter::decodeLabTextAuto(rawBytes, encodingHint);
    g_lastImportReport.encoding = g_lastDecodedEncoding;
    g_lastImportReport.invalidChars = g_lastDecodeInvalidChars;
    if (decoded.isEmpty())
        return new future::Table(0, 0, effTableName);

    const bool isRasExt = filePathHint.endsWith(QLatin1String(".ras"), Qt::CaseInsensitive);
    const QString trimmedStart = decoded.left(512).trimmed();

    // 1. Specialized Rigaku SmartLab .ras XRD / XRR parser
    if ((isRasExt || trimmedStart.startsWith(QLatin1String("*RAS_")))
        && decoded.contains(QLatin1String("*RAS_INT_START"))) {
        static const QRegularExpression kUnitXRe(
                QStringLiteral("\\*MEAS_SCAN_UNIT_X\\s+\"([^\"]*)\""));
        static const QRegularExpression kUnitYRe(
                QStringLiteral("\\*MEAS_SCAN_UNIT_Y\\s+\"([^\"]*)\""));
        // Unit of the scan header nearest above `before` (each scan of a multi-scan file has
        // its own *RAS_HEADER block), falling back to the default.
        auto unitBefore = [&decoded](const QRegularExpression &re, int before,
                                     const QString &fallback) -> QString {
            QString found = fallback;
            QRegularExpressionMatchIterator it = re.globalMatch(decoded);
            while (it.hasNext()) {
                const QRegularExpressionMatch m = it.next();
                if (m.capturedStart() > before)
                    break;
                if (!m.captured(1).trimmed().isEmpty())
                    found = m.captured(1).trimmed();
            }
            return found;
        };

        QLocale cLoc = QLocale::c();
        cLoc.setNumberOptions(QLocale::RejectGroupSeparator);

        // Every *RAS_INT_START ... *RAS_INT_END block is imported (multi-scan files used to be cut
        // at the first *RAS_INT_END). Cells that are missing or not numbers become empty cells;
        // rows are never dropped and never padded with 0.
        QList<Column *> cols;
        int searchFrom = 0;
        int blockIdx = 0;
        int missingCells = 0;
        int totalRows = 0;
        int maxBlockCols = 0;
        while (true) {
            const int intStart = decoded.indexOf(QLatin1String("*RAS_INT_START"), searchFrom);
            if (intStart < 0)
                break;
            int bodyStart = decoded.indexOf(QLatin1Char('\n'), intStart);
            bodyStart = (bodyStart >= 0) ? (bodyStart + 1) : (intStart + 14);
            int intEnd = decoded.indexOf(QLatin1String("*RAS_INT_END"), bodyStart);
            const int nextStart = decoded.indexOf(QLatin1String("*RAS_INT_START"), bodyStart);
            if (intEnd < 0 || (nextStart >= 0 && nextStart < intEnd))
                intEnd = (nextStart >= 0) ? nextStart : decoded.size();
            searchFrom = std::max(intEnd, bodyStart);

            const QString unitX = unitBefore(kUnitXRe, intStart, QStringLiteral("deg."));
            const QString unitY = unitBefore(kUnitYRe, intStart, QStringLiteral("cps"));

            const QString body = decoded.mid(bodyStart, std::max(0, intEnd - bodyStart));
            QVector<QVector<LabCsvCell>> rasRows;
            int rasCols = 0;
            parseDelimitedRecordsFast(body, 0, QStringLiteral(" "), true, true, true, rasRows,
                                      rasCols);
            // Drop '*' metadata lines that may sit inside a block.
            for (int r = rasRows.size() - 1; r >= 0; --r) {
                if (!rasRows[r].isEmpty() && rasRows[r][0].value.trimmed().startsWith(QLatin1Char('*')))
                    rasRows.remove(r);
            }
            const int nRows = rasRows.size();
            if (nRows == 0 && blockIdx > 0)
                continue;
            rasCols = std::max(2, rasCols);
            maxBlockCols = std::max(maxBlockCols, rasCols);
            totalRows = std::max(totalRows, nRows);
            const QString suffix =
                    (blockIdx == 0) ? QString() : QStringLiteral("_%1").arg(blockIdx + 1);

            for (int c = 0; c < rasCols; ++c) {
                std::unique_ptr<QVector<qreal>> vec(new QVector<qreal>(nRows, 0.0));
                IntervalAttribute<bool> inv;
                for (int r = 0; r < nRows; ++r) {
                    const QString cellTxt =
                            (c < rasRows[r].size()) ? rasRows[r][c].value.trimmed() : QString();
                    bool ok = false;
                    const double v = cLoc.toDouble(cellTxt, &ok);
                    if (ok && std::isfinite(v)) {
                        double scaled = v;
                        if (c == 1 && 2 < rasRows[r].size()) {
                            bool fOk = false;
                            const double fv = cLoc.toDouble(rasRows[r][2].value.trimmed(), &fOk);
                            if (fOk && std::isfinite(fv) && fv > 0.0)
                                scaled = v * fv;
                        }
                        (*vec)[r] = scaled;
                    } else {
                        inv.setValue(r, true);
                        ++missingCells;
                    }
                }
                QString colShort = (c == 0) ? QStringLiteral("TwoTheta")
                        : (c == 1)          ? QStringLiteral("Intensity")
                        : (c == 2)          ? QStringLiteral("Factor")
                                            : QStringLiteral("Col%1").arg(c + 1);
                Column *col = new Column(colShort + suffix, std::move(vec), inv);
                if (c == 0) {
                    col->setLongName(QStringLiteral("2θ"));
                    col->setUnits(unitX);
                    col->setPlotDesignation(SciDAVis::X);
                } else if (c == 1) {
                    col->setLongName(QStringLiteral("Intensity"));
                    col->setUnits(unitY);
                    col->setPlotDesignation(SciDAVis::Y);
                } else {
                    col->setPlotDesignation(SciDAVis::noDesignation);
                }
                if (blockIdx > 0)
                    col->setComment(QObject::tr("Scan %1").arg(blockIdx + 1));
                if (auto *d2s = qobject_cast<Double2StringFilter *>(col->outputFilter())) {
                    d2s->setNumericFormat('g');
                    d2s->setNumDigits(6);
                }
                cols << col;
            }
            ++blockIdx;
        }

        g_lastImportReport.separator = QStringLiteral(" ");
        g_lastImportReport.rasBlocks = blockIdx;
        g_lastImportReport.dataRows = totalRows;
        g_lastImportReport.columns = cols.size();
        g_lastImportReport.missingCells = missingCells;
        g_lastImportReport.headerLines = 0;

        future::Table *result = new future::Table(0, 0, effTableName);
        result->appendColumns(cols);
        return result;
    }

    // 1b. Specialized VASP DOSCAR & EIGENVAL parser
    if (ignoredLines == 0) {
        if (future::Table *vaspTbl =
                    tryParseVaspTextInternal(decoded, effTableName, filePathHint, nullptr)) {
            return vaspTbl;
        }
        // 1c. Synchrotron SPEC (#F / #S / #N / #L) parser
        if (future::Table *specTbl = tryParseSpecTextInternal(
                    decoded, effTableName, convertToNumeric, numericLocale, firstRowNamesColumns)) {
            return specTbl;
        }
    }

    // 2. Advance past explicit ignoredLines
    const QChar *buf = decoded.constData();
    const int len = decoded.size();
    int pos = 0;
    int skippedNonBlankLines = 0;
    for (int skipped = 0; skipped < ignoredLines && pos < len; ++skipped) {
        ++skippedNonBlankLines;
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

    auto scanLogicalLineEnd = [&](int startIdx) -> int {
        int e = startIdx;
        bool inQuotes = false;
        bool atFieldStart = true;
        while (e < len) {
            const QChar ch = buf[e];
            if (inQuotes) {
                if (ch == QLatin1Char('"')) {
                    if (e + 1 < len && buf[e + 1] == QLatin1Char('"')) {
                        e += 2;
                        continue;
                    }
                    inQuotes = false;
                }
                ++e;
            } else {
                if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n'))
                    break;
                if (ch == QLatin1Char('"') && atFieldStart) {
                    inQuotes = true;
                    atFieldStart = false;
                    ++e;
                } else if (ch == QLatin1Char(',') || ch == QLatin1Char('\t')
                           || ch == QLatin1Char(';')) {
                    atFieldStart = true;
                    ++e;
                } else if (ch == QLatin1Char(' ')) {
                    ++e;
                } else {
                    atFieldStart = false;
                    ++e;
                }
            }
        }
        if (inQuotes) {
            e = startIdx;
            while (e < len && buf[e] != QLatin1Char('\r') && buf[e] != QLatin1Char('\n'))
                ++e;
        }
        return e;
    };

    QLocale effectiveLocale = numericLocale;
    bool europeanSemicolonDetected = false;
    {
        QString normHintSep = separatorHint;
        if (normHintSep == QLatin1String("\\t"))
            normHintSep = QStringLiteral("\t");
        else if (normHintSep == QLatin1String("\\s"))
            normHintSep = QStringLiteral(" ");
        const bool sepAllowsSemicolon = normHintSep.isEmpty()
                || normHintSep.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0
                || normHintSep == QLatin1String(";")
                || (autoFixTabOnCsv && normHintSep == QLatin1String("\t"));
        if (convertToNumeric && sepAllowsSemicolon
            && effectiveLocale.decimalPoint() == QLatin1Char('.')) {
            QStringList preLines;
            QStringList preHashFallback;
            int pScan = pos;
            while (pScan < len && preLines.size() < 24) {
                const int e = scanLogicalLineEnd(pScan);
                const QString ln = QString(buf + pScan, e - pScan).trimmed();
                if (!ln.isEmpty()) {
                    if (!ln.startsWith(QLatin1Char('#')))
                        preLines.append(ln);
                    else if (preHashFallback.size() < 8 && ln.contains(QLatin1Char(';')))
                        preHashFallback.append(ln);
                }
                pScan = e;
                if (pScan < len && buf[pScan] == QLatin1Char('\r')) {
                    ++pScan;
                    if (pScan < len && buf[pScan] == QLatin1Char('\n'))
                        ++pScan;
                } else if (pScan < len && buf[pScan] == QLatin1Char('\n')) {
                    ++pScan;
                }
            }
            if (preLines.isEmpty())
                preLines = preHashFallback;
            if (looksLikeEuropeanSemicolonLines(preLines)) {
                effectiveLocale = QLocale(QLocale::German);
                europeanSemicolonDetected = true;
            }
        }
    }

    const bool useCustomLoc =
            (effectiveLocale.decimalPoint() != QLatin1Char('.') && effectiveLocale != QLocale::c());
    const bool hasDistinctNumericLocale = (effectiveLocale != QLocale::c());
    QLocale primaryLoc = effectiveLocale;
    primaryLoc.setNumberOptions(QLocale::RejectGroupSeparator);

    // 3. Skip leading blank lines & '#'-prefixed comment preamble lines
    //    (preserving a final '# col1, col2' or '#x,y' header line if data starts immediately with numbers!)
    QString lastHashCommentBody;
    bool lastHashHadSpaceAfterHash = false;
    while (pos < len) {
        int lineStart = pos;
        while (lineStart < len
               && (buf[lineStart] == QLatin1Char(' ') || buf[lineStart] == QLatin1Char('\t')))
            ++lineStart;
        int lineEnd = lineStart;
        while (lineEnd < len && buf[lineEnd] != QLatin1Char('\r')
               && buf[lineEnd] != QLatin1Char('\n'))
            ++lineEnd;

        int nextPos = lineEnd;
        if (nextPos < len && buf[nextPos] == QLatin1Char('\r')) {
            ++nextPos;
            if (nextPos < len && buf[nextPos] == QLatin1Char('\n'))
                ++nextPos;
        } else if (nextPos < len && buf[nextPos] == QLatin1Char('\n')) {
            ++nextPos;
        }

        int trimEnd = lineEnd;
        while (trimEnd > lineStart
               && (buf[trimEnd - 1] == QLatin1Char(' ') || buf[trimEnd - 1] == QLatin1Char('\t')))
            --trimEnd;

        if (lineStart == trimEnd) {
            pos = nextPos;
            continue;
        }
        if (buf[lineStart] == QLatin1Char('#')) {
            bool isCommentAtTop = true;
            if (lineStart + 1 < trimEnd && !buf[lineStart + 1].isSpace()) {
                int tokEnd = lineStart;
                while (tokEnd < trimEnd && buf[tokEnd] != QLatin1Char(',')
                       && buf[tokEnd] != QLatin1Char('\t') && buf[tokEnd] != QLatin1Char(';')
                       && !buf[tokEnd].isSpace()) {
                    ++tokEnd;
                }
                if (isMissingDataTokenSpan(buf + lineStart, tokEnd - lineStart)) {
                    isCommentAtTop = false;
                } else {
                    bool inQ = false;
                    bool atFS = true;
                    QChar foundExplicitSep;
                    bool hasWsOutsideQuotes = false;
                    for (int k = lineStart; k < trimEnd; ++k) {
                        const QChar ch = buf[k];
                        if (inQ) {
                            if (ch == QLatin1Char('"')) {
                                if (k + 1 < trimEnd && buf[k + 1] == QLatin1Char('"'))
                                    ++k;
                                else
                                    inQ = false;
                            }
                        } else if (ch == QLatin1Char('"') && atFS) {
                            inQ = true;
                            atFS = false;
                        } else if (ch == QLatin1Char(',') || ch == QLatin1Char('\t')
                                   || ch == QLatin1Char(';')) {
                            if (foundExplicitSep.isNull())
                                foundExplicitSep = ch;
                            atFS = true;
                        } else if (ch.isSpace()) {
                            hasWsOutsideQuotes = true;
                        } else {
                            atFS = false;
                        }
                    }

                    if (!foundExplicitSep.isNull()) {
                        // Multi-field explicit delimiter line: '#<digit>...' ('#1,520', '#2,') is data,
                        // whereas '#x,y' or '#comment,abc' is a comment!
                        isCommentAtTop = !buf[lineStart + 1].isDigit();
                    } else if (hasWsOutsideQuotes) {
                        int tmpP = lineStart;
                        QVector<ZeroCopyCellSpan> wsSpans;
                        bool tmpSkip = false;
                        readNextRecordSpans(buf, trimEnd, tmpP, QStringLiteral(" "), true, true,
                                            true, wsSpans, tmpSkip);
                        isCommentAtTop = isHashCommentRecordCells(
                                wsSpans.size(), true /*whitespaceDelim*/, true,
                                false /*allowAlphaHashIds*/, primaryLoc, hasDistinctNumericLocale,
                                [&](int idx, const QChar *&pk, int &nk, bool &kQuoted) {
                                    wsSpans[idx].trimmedView(pk, nk);
                                    kQuoted = wsSpans[idx].quoted;
                                });
                    } else {
                        bool subsequentHasMultiCol = false;
                        if (buf[lineStart + 1].isDigit()) {
                            int pPeek = nextPos;
                            int checkedSubsequent = 0;
                            while (pPeek < len && checkedSubsequent < 10) {
                                int ls = pPeek;
                                while (ls < len
                                       && (buf[ls] == QLatin1Char(' ')
                                           || buf[ls] == QLatin1Char('\t')))
                                    ++ls;
                                int le = ls;
                                while (le < len && buf[le] != QLatin1Char('\r')
                                       && buf[le] != QLatin1Char('\n'))
                                    ++le;
                                int np = le;
                                if (np < len && buf[np] == QLatin1Char('\r')) {
                                    ++np;
                                    if (np < len && buf[np] == QLatin1Char('\n'))
                                        ++np;
                                } else if (np < len && buf[np] == QLatin1Char('\n')) {
                                    ++np;
                                }
                                pPeek = np;
                                while (le > ls
                                       && (buf[le - 1] == QLatin1Char(' ')
                                           || buf[le - 1] == QLatin1Char('\t')))
                                    --le;
                                if (ls == le)
                                    continue;
                                if (buf[ls] == QLatin1Char('#')
                                    && (ls + 1 == le || buf[ls + 1].isSpace())) {
                                    continue;
                                }
                                bool sq = false;
                                bool sfs = true;
                                bool lineHasSep = false;
                                bool lineHasWs = false;
                                for (int k = ls; k < le; ++k) {
                                    const QChar ch = buf[k];
                                    if (sq) {
                                        if (ch == QLatin1Char('"')) {
                                            if (k + 1 < le && buf[k + 1] == QLatin1Char('"'))
                                                ++k;
                                            else
                                                sq = false;
                                        }
                                    } else if (ch == QLatin1Char('"') && sfs) {
                                        sq = true;
                                        sfs = false;
                                    } else if (ch == QLatin1Char(',') || ch == QLatin1Char('\t')
                                               || ch == QLatin1Char(';')) {
                                        lineHasSep = true;
                                        break;
                                    } else if (ch.isSpace()) {
                                        lineHasWs = true;
                                    } else {
                                        sfs = false;
                                    }
                                }
                                if (lineHasSep) {
                                    subsequentHasMultiCol = true;
                                    break;
                                }
                                if (lineHasWs) {
                                    if (!(buf[ls] == QLatin1Char('#') && ls + 1 < le
                                          && !buf[ls + 1].isDigit())) {
                                        subsequentHasMultiCol = true;
                                        break;
                                    }
                                }
                                ++checkedSubsequent;
                            }
                        }
                        const bool multiColForSingle =
                                (!buf[lineStart + 1].isDigit() || subsequentHasMultiCol);
                        isCommentAtTop = isHashCommentRecordCells(
                                1, false, multiColForSingle, false /*allowAlphaHashIds*/,
                                primaryLoc, hasDistinctNumericLocale,
                                [&](int, const QChar *&pk, int &nk, bool &kQuoted) {
                                    pk = buf + lineStart;
                                    nk = trimEnd - lineStart;
                                    kQuoted = false;
                                });
                    }
                }
            }
            if (isCommentAtTop) {
                lastHashHadSpaceAfterHash =
                        (lineStart + 1 < trimEnd && buf[lineStart + 1].isSpace());
                lastHashCommentBody =
                        QString(buf + lineStart + 1, trimEnd - (lineStart + 1)).trimmed();
                ++skippedNonBlankLines;
                pos = nextPos;
                continue;
            }
        }
        break;
    }

    // 3b. Drag & drop: find the numeric block below an instrument metadata preamble
    //     (BioLogic .mpt, Gamry .dta, many .asc/.csv exports) and the name/unit rows right
    //     above it. Everything above is kept as the table comment instead of being guessed at.
    QString preambleSeparator;
    int forcedUnitsRow = -1;
    int forcedCommentsRow = -1;
    if (detectPreamble && ignoredLines == 0 && pos < len) {
        struct LineSpan
        {
            int start;
            int end;
        };
        QVector<LineSpan> lines;
        {
            int p = pos;
            while (p < len && lines.size() < 4000) {
                const int e = scanLogicalLineEnd(p);
                if (!QString(buf + p, e - p).trimmed().isEmpty())
                    lines.append({ p, e });
                p = e;
                if (p < len && buf[p] == QLatin1Char('\r')) {
                    ++p;
                    if (p < len && buf[p] == QLatin1Char('\n'))
                        ++p;
                } else if (p < len && buf[p] == QLatin1Char('\n')) {
                    ++p;
                }
            }
        }
        struct SplitLineResult
        {
            QStringList parts;
            QVector<QString> rawParts;
            QVector<bool> rawQuoted;
            int rawFieldCount = 0;
            bool col0Quoted = false;
        };
        auto splitLineEx = [&](int idx, QChar sep) -> SplitLineResult {
            SplitLineResult res;
            const QString ln = QString(buf + lines[idx].start, lines[idx].end - lines[idx].start);
            if (sep == QLatin1Char(' ')) {
                int p = 0;
                const int n = ln.size();
                while (p < n) {
                    while (p < n && ln.at(p).isSpace())
                        ++p;
                    if (p >= n)
                        break;
                    if (ln.at(p) == QLatin1Char('"')) {
                        ++p;
                        QString val;
                        while (p < n) {
                            if (ln.at(p) == QLatin1Char('"')) {
                                if (p + 1 < n && ln.at(p + 1) == QLatin1Char('"')) {
                                    val.append(QLatin1Char('"'));
                                    p += 2;
                                } else {
                                    ++p;
                                    break;
                                }
                            } else {
                                val.append(ln.at(p));
                                ++p;
                            }
                        }
                        res.rawParts.append(val.trimmed());
                        res.rawQuoted.append(true);
                    } else {
                        const int ts = p;
                        while (p < n && !ln.at(p).isSpace())
                            ++p;
                        res.rawParts.append(ln.mid(ts, p - ts).trimmed());
                        res.rawQuoted.append(false);
                    }
                }
            } else {
                QString cur;
                bool inQuotes = false;
                bool atFieldStart = true;
                bool curQuoted = false;
                for (int k = 0; k < ln.size(); ++k) {
                    const QChar ch = ln.at(k);
                    if (inQuotes) {
                        if (ch == QLatin1Char('"')) {
                            if (k + 1 < ln.size() && ln.at(k + 1) == QLatin1Char('"')) {
                                cur.append(QLatin1Char('"'));
                                ++k;
                            } else {
                                inQuotes = false;
                            }
                        } else if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n')) {
                            if (!cur.endsWith(QLatin1Char(' ')))
                                cur.append(QLatin1Char(' '));
                        } else {
                            cur.append(ch);
                        }
                    } else {
                        if (ch == sep) {
                            res.rawParts.append(cur.trimmed());
                            res.rawQuoted.append(curQuoted);
                            cur.clear();
                            curQuoted = false;
                            atFieldStart = true;
                        } else if (ch == QLatin1Char('"') && atFieldStart) {
                            inQuotes = true;
                            curQuoted = true;
                            atFieldStart = false;
                        } else {
                            if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t'))
                                atFieldStart = false;
                            cur.append(ch);
                        }
                    }
                }
                res.rawParts.append(cur.trimmed());
                res.rawQuoted.append(curQuoted);
            }
            res.rawFieldCount = res.rawParts.size();
            res.col0Quoted = !res.rawQuoted.isEmpty() && res.rawQuoted[0];
            res.parts.reserve(res.rawParts.size());
            for (const QString &rp : res.rawParts)
                res.parts.append(rp);
            // a trailing separator ("1,2,") must not change the effective field count
            while (res.parts.size() > 1 && res.parts.last().isEmpty()
                   && !res.rawQuoted[res.parts.size() - 1]) {
                res.parts.removeLast();
            }
            return res;
        };
        auto splitLine = [&](int idx, QChar sep) -> QStringList {
            return splitLineEx(idx, sep).parts;
        };
        auto rowStats = [&](const QStringList &parts, int &outNumeric, int &outText) {
            outNumeric = 0;
            outText = 0;
            for (const QString &f : parts) {
                if (f.isEmpty() || isMissingNumericToken(f, false))
                    continue;
                double dv = 0.0;
                if (tryParseLabDouble(f, primaryLoc, hasDistinctNumericLocale, dv))
                    ++outNumeric;
                else
                    ++outText;
            }
        };
        // Missing cells (empty, "NaN", "-", "N/A", "inf", ...) of a split row, counting the empty
        // fields that splitLineEx dropped at the end ("0," -> one missing cell).
        auto missingCells = [&](const SplitLineResult &sr) -> int {
            int n = std::max(0, sr.rawFieldCount - int(sr.parts.size()));
            for (const QString &f : sr.parts) {
                if (f.isEmpty() || isMissingNumericToken(f, false))
                    ++n;
            }
            return n;
        };
        // Numeric data row for the block search: at least two numbers, or (missing cells count as
        // number fields) one number plus missing cells and no text ("0,", "0,NaN", ",1.5").
        auto isNumericBlockRow = [&](const SplitLineResult &sr, int num, int txt) -> bool {
            if (num >= 2 && num >= txt && sr.parts.size() >= 2)
                return true;
            return num >= 1 && txt == 0 && sr.rawFieldCount >= 2 && num + missingCells(sr) >= 2;
        };
        // ';' / '!' comment line inside a ','/tab separated file ("; pause", "! note").
        auto isBangSemicolonComment = [&](int idx, QChar sep) -> bool {
            if (sep == QLatin1Char(';') || idx < 0 || idx >= lines.size())
                return false;
            int s = lines[idx].start;
            while (s < lines[idx].end && buf[s].isSpace())
                ++s;
            return s < lines[idx].end
                    && (buf[s] == QLatin1Char(';') || buf[s] == QLatin1Char('!'));
        };
        auto isCommentLineForSep = [&](int idx, QChar sep, bool allowAlphaHashIds) -> bool {
            if (idx < 0 || idx >= lines.size())
                return false;
            const QChar *ls = buf + lines[idx].start;
            int ll = lines[idx].end - lines[idx].start;
            while (ll > 0 && ls[0].isSpace()) {
                ++ls;
                --ll;
            }
            if (ll <= 0 || ls[0] != QLatin1Char('#'))
                return false;
            const SplitLineResult sr = splitLineEx(idx, sep);
            return isHashCommentRecordCells(
                    sr.rawFieldCount, sep == QLatin1Char(' '), true /*tableHasMultipleCols*/,
                    allowAlphaHashIds, primaryLoc, hasDistinctNumericLocale,
                    [&](int k, const QChar *&pk, int &nk, bool &kQuoted) {
                        if (k >= 0 && k < sr.rawParts.size()) {
                            pk = sr.rawParts[k].constData();
                            nk = sr.rawParts[k].size();
                            kQuoted = sr.rawQuoted[k];
                        } else {
                            pk = nullptr;
                            nk = 0;
                            kQuoted = false;
                        }
                    });
        };
        // 1 = numeric-dominant row, 0 = pure text row, -1 = mixed
        auto rowKind = [&](const QStringList &parts) -> int {
            int numeric = 0, text = 0;
            rowStats(parts, numeric, text);
            if (numeric >= 2 && numeric >= text)
                return 1;
            if (numeric == 0 && text > 0)
                return 0;
            return -1;
        };
        const QChar sepsC[4] = { QLatin1Char('\t'), QLatin1Char(','), QLatin1Char(';'),
                                 QLatin1Char(' ') };
        const QChar sepsEuro[4] = { QLatin1Char('\t'), QLatin1Char(';'), QLatin1Char(','),
                                    QLatin1Char(' ') };
        const QChar(&seps)[4] = (europeanSemicolonDetected || effectiveLocale.decimalPoint() == QLatin1Char(','))
                ? sepsEuro
                : sepsC;
        // Every line holds the same number of TABs (one or more): the TAB is the separator of the
        // whole file, there is no preamble of another shape above the table. A ',' / ';' found
        // inside the cells is then a decimal comma ("0,50<TAB>1,234"), not the separator: splitting
        // at it would cut the values apart ("0" / "50<TAB>1" / "234") and push the header into a
        // preamble.
        bool tabSeparatedFile = false;
        {
            int tabsPerLine = -1;
            bool same = !lines.isEmpty();
            for (int i = 0; same && i < lines.size(); ++i) {
                int tabs = 0;
                for (int k = lines[i].start; k < lines[i].end; ++k) {
                    if (buf[k] == QLatin1Char('\t'))
                        ++tabs;
                }
                if (tabsPerLine < 0)
                    tabsPerLine = tabs;
                same = tabs == tabsPerLine;
            }
            tabSeparatedFile = same && tabsPerLine >= 1;
        }
        int bestStart = -1, bestFields = 0, bestRawFields = 0;
        bool bestRelaxed = false;
        QChar bestSep;
        for (const QChar sep : seps) {
            if (europeanSemicolonDetected && sep == QLatin1Char(','))
                continue;
            if (tabSeparatedFile && (sep == QLatin1Char(',') || sep == QLatin1Char(';')))
                continue;
            for (int i = 0; i < lines.size(); ++i) {
                if (bestStart >= 0 && i > bestStart)
                    break; // an earlier block was already found with another separator
                if (isCommentLineForSep(i, sep, false) || isBangSemicolonComment(i, sep))
                    continue;
                const SplitLineResult sr0 = splitLineEx(i, sep);
                const QStringList &p0 = sr0.parts;
                const int p0RawCount = sr0.rawFieldCount;
                int p0Num = 0, p0Txt = 0;
                rowStats(p0, p0Num, p0Txt);
                if (!isNumericBlockRow(sr0, p0Num, p0Txt))
                    continue;
                // Accepted only through its missing cells ("0,", "0,NaN"): the rows below must
                // exist and hold numbers and missing cells only, so that a sparse row further down
                // a table with text columns is not taken for the start of the data.
                const bool relaxedStart = !(p0.size() >= 2 && p0Num >= 2 && p0Num >= p0Txt);
                // A missing value at the end of the first data row ("0,", "300<TAB>1.5<TAB>")
                // shortens its split; the block is as wide as the widest of its first rows.
                int blockWidth = (p0.size() >= 2) ? int(p0.size()) : p0RawCount;
                int blockRawWidth = p0RawCount;
                // P1-D4(b): a mixed text+numeric header row (e.g. "Time (s), 300, 400, 500")
                // sitting right above pure-numeric data rows is the header, not the first data row.
                int nextDataIdx = i + 1;
                while (nextDataIdx < lines.size() && isCommentLineForSep(nextDataIdx, sep, false))
                    ++nextDataIdx;
                if (p0Txt > 0 && nextDataIdx < lines.size()
                    && !isCommentLineForSep(nextDataIdx, sep, false)) {
                    const QStringList pNext = splitLine(nextDataIdx, sep);
                    int nextNum = 0, nextTxt = 0;
                    rowStats(pNext, nextNum, nextTxt);
                    if (pNext.size() == p0.size() && nextNum >= 2 && nextTxt == 0)
                        continue;
                }
                int checkedFollow = 0;
                bool ok = true;
                for (int k = i + 1; k < lines.size() && checkedFollow < 2 && ok; ++k) {
                    if (isCommentLineForSep(k, sep, true) || isBangSemicolonComment(k, sep))
                        continue;
                    // following rows may lack trailing values ("2\t" -> one field) or be all-missing ("#N/A,#N/A", "#DIV/0!,") or specimen+missing ("#2,")
                    const SplitLineResult srk = splitLineEx(k, sep);
                    const QStringList &pk = srk.parts;
                    int num = 0, txt = 0;
                    rowStats(pk, num, txt);
                    if (srk.rawFieldCount >= 2 && srk.rawFieldCount <= p0RawCount
                        && ((num == 0 && txt == 0)
                            || (pk.size() < srk.rawFieldCount && num == 0 && txt == 1))) {
                        continue;
                    }
                    ok = (pk.size() <= std::max<int>(p0.size(), p0RawCount) && num >= 1
                          && num >= txt && (!relaxedStart || txt == 0));
                    if (ok) {
                        blockWidth = std::max<int>(blockWidth, pk.size());
                        blockRawWidth = std::max(blockRawWidth, srk.rawFieldCount);
                    }
                    ++checkedFollow;
                }
                if (!ok || (relaxedStart && checkedFollow == 0))
                    continue;
                if (bestStart < 0 || i < bestStart || (i == bestStart && blockWidth > bestFields)) {
                    bestStart = i;
                    bestFields = blockWidth;
                    bestRawFields = blockRawWidth;
                    bestRelaxed = relaxedStart;
                    bestSep = sep;
                }
                break;
            }
        }
        if (bestStart >= 0) {
            bool bestStartAllNumeric = false;
            int hdrStart = bestStart;
            auto matchesBestWidth = [&](int idx) -> bool {
                if (idx < 0 || isCommentLineForSep(idx, bestSep, false))
                    return false;
                const SplitLineResult sr = splitLineEx(idx, bestSep);
                if (bestSep == QLatin1Char(' '))
                    return sr.parts.size() == bestFields;
                // "x,y,z" above "1,2," rows (an empty last column): the raw width counts too
                return sr.rawFieldCount == bestFields || sr.parts.size() == bestFields
                        || (bestRawFields > bestFields && sr.rawFieldCount == bestRawFields);
            };
            // A start row accepted only through its missing cells ("2,", ",352") under rows of the
            // same width that hold numbers ("1,heated", "S2,350"): the table has a text column that
            // is empty further down, and those rows are its first data rows, not a header and a
            // preamble ("Time,Note / 0,start / 1,heated / 2, / 3,"). The block starts at the
            // first of them when a row without numbers (the header) or the file start is above
            // them, or when there are several. A single such row under a line of another width
            // (a title) stays a header candidate ("Temp,300" above "0," / "1,2.5").
            // Each of those rows must have the shape of the data below, column by column: text
            // only in a column that holds no number from the start row down (the text column
            // that is empty further down), numbers only in a column that holds numbers there.
            // "Month,2024" / "Time (s),300" / "Rate,10" above "1," / "0," (text over the numeric
            // first column) are a header or a preamble and stop the chain.
            if (bestRelaxed) {
                auto prevDataIdx = [&](int fromIdx) -> int {
                    int cur = fromIdx;
                    while (cur >= 0
                           && (isCommentLineForSep(cur, bestSep, false)
                               || isBangSemicolonComment(cur, bestSep)))
                        --cur;
                    return cur;
                };
                // 0 = missing, 1 = number, 2 = text
                auto cellKind = [&](const QString &f) -> int {
                    if (f.isEmpty() || isMissingNumericToken(f, false))
                        return 0;
                    double dv = 0.0;
                    return tryParseLabDouble(f, primaryLoc, hasDistinctNumericLocale, dv) ? 1 : 2;
                };
                const int profCols = std::max(bestFields, bestRawFields);
                QVector<bool> colHasNum(profCols, false);
                {
                    int scanned = 0;
                    for (int k = bestStart; k < lines.size() && scanned < 32; ++k) {
                        if (isCommentLineForSep(k, bestSep, true)
                            || isBangSemicolonComment(k, bestSep))
                            continue;
                        const SplitLineResult srk = splitLineEx(k, bestSep);
                        for (int c = 0; c < srk.rawParts.size() && c < profCols; ++c) {
                            if (cellKind(srk.rawParts[c]) == 1)
                                colHasNum[c] = true;
                        }
                        ++scanned;
                    }
                }
                auto chainRowFits = [&](const SplitLineResult &sr) -> bool {
                    for (int c = 0; c < sr.rawParts.size(); ++c) {
                        const int kind = cellKind(sr.rawParts[c]);
                        if (kind == 0)
                            continue;
                        const bool numBelow = c < profCols && colHasNum[c];
                        if ((kind == 1) != numBelow)
                            return false;
                    }
                    return true;
                };
                int top = bestStart;
                int chain = 0;
                int up = prevDataIdx(top - 1);
                while (up >= 0 && matchesBestWidth(up)) {
                    const SplitLineResult srUp = splitLineEx(up, bestSep);
                    int num = 0, txt = 0;
                    rowStats(srUp.parts, num, txt);
                    if (num < 1 || !chainRowFits(srUp))
                        break;
                    top = up;
                    ++chain;
                    up = prevDataIdx(up - 1);
                }
                if (chain > 0 && (chain >= 2 || up < 0 || matchesBestWidth(up))) {
                    bestStart = top;
                    hdrStart = top;
                }
            }
            {
                int bsNum = 0, bsTxt = 0;
                const SplitLineResult srBest = splitLineEx(bestStart, bestSep);
                rowStats(srBest.parts, bsNum, bsTxt);
                bestStartAllNumeric = bsTxt == 0 && isNumericBlockRow(srBest, bsNum, bsTxt);
            }
            auto isPureTextRowWithFields = [&](int idx) -> bool {
                if (!matchesBestWidth(idx))
                    return false;
                const SplitLineResult sr = splitLineEx(idx, bestSep);
                return rowKind(sr.parts) == 0;
            };
            auto isHeaderCandidateWithFields = [&](int idx) -> bool {
                if (!matchesBestWidth(idx))
                    return false;
                const SplitLineResult sr = splitLineEx(idx, bestSep);
                int num = 0, txt = 0;
                rowStats(sr.parts, num, txt);
                if (txt >= 1 && (num == 0 || bestStartAllNumeric))
                    return true;
                // A top-line header exported with an auto-numbered column (e.g. "colA,colB,colC,4")
                // above mixed text+numeric data has majority text labels over numeric data columns.
                if (idx == 0 && txt > num && bestStart > 0) {
                    const SplitLineResult srBest = splitLineEx(bestStart, bestSep);
                    const int nCheck = std::min(sr.rawParts.size(), srBest.rawParts.size());
                    for (int c = 0; c < nCheck; ++c) {
                        const QString &hc = sr.rawParts[c];
                        const QString &dc = srBest.rawParts[c];
                        if (hc.isEmpty() || dc.isEmpty() || isMissingNumericToken(hc, false)
                            || isMissingNumericToken(dc, false))
                            continue;
                        double d1 = 0.0, d2 = 0.0;
                        const bool hNum = tryParseLabDouble(hc, primaryLoc,
                                                            hasDistinctNumericLocale, d1);
                        const bool dNum = tryParseLabDouble(dc, primaryLoc,
                                                            hasDistinctNumericLocale, d2);
                        if (!hNum && dNum)
                            return true;
                    }
                }
                return false;
            };
            auto prevNonCommentIdx = [&](int fromIdx) -> int {
                int cur = fromIdx;
                while (cur >= 0 && isCommentLineForSep(cur, bestSep, false))
                    --cur;
                return cur;
            };
            auto analyzePreUnitRow = [&](int unitIdx, bool allowAllEmptyUnits, int &outNamedUnits,
                                         bool &outAllEmpty) -> bool {
                outNamedUnits = 0;
                outAllEmpty = false;
                if (!matchesBestWidth(unitIdx))
                    return false;
                const SplitLineResult srU = splitLineEx(unitIdx, bestSep);
                int nonEmpty = 0;
                bool allNonNumeric = true;
                bool allRecognized = true;
                bool allFallback = true;
                bool shortCells = true;
                bool hasInlineHeaderUnit = false;
                for (const QString &raw : srU.rawParts) {
                    const QString t = raw.trimmed();
                    if (t.isEmpty())
                        continue;
                    ++nonEmpty;
                    double dummy = 0.0;
                    if (tryParseLabDouble(t, primaryLoc, hasDistinctNumericLocale, dummy)) {
                        allNonNumeric = false;
                        break;
                    }
                    QString sn, ln, un;
                    AsciiTableImportFilter::splitHeaderNameAndUnits(t, sn, ln, un);
                    if (!ln.isEmpty() && !un.isEmpty() && ln != t)
                        hasInlineHeaderUnit = true;
                    const QString cleaned = cleanPromotedUnitCell(t);
                    if (cleaned.size() > 32)
                        shortCells = false;
                    if (isLikelyScientificUnit(cleaned)) {
                        if (!isDimensionlessDashUnit(cleaned))
                            ++outNamedUnits;
                    } else {
                        allRecognized = false;
                        if (!hasFallbackUnitSyntaxAscii(t))
                            allFallback = false;
                    }
                }
                if (!allNonNumeric || hasInlineHeaderUnit)
                    return false;
                if (nonEmpty == 0) {
                    outAllEmpty = true;
                    return allowAllEmptyUnits && bestSep != QLatin1Char(' ');
                }
                return shortCells && (allRecognized || allFallback);
            };
            auto nonEmptyCellCount = [&](int idx) -> int {
                const SplitLineResult sr = splitLineEx(idx, bestSep);
                int n = 0;
                for (const QString &rp : sr.rawParts) {
                    if (!rp.trimmed().isEmpty())
                        ++n;
                }
                return n;
            };

            // If numeric data rows right above bestStart contain a non-numeric outlier
            // (e.g. "0, <0.01" or "0, OVER") directly below an already width-matched pure-text
            // header/unit/comment row (e.g. "Idx, Val"), include them in the data block instead of
            // misclassifying the lowest one as a mixed header and discarding the real header and
            // the rows above into the preamble. Several such rows may follow each other
            // ("t_h,H_ppm / 0,<0.01 / 1,<0.01 / 2,0.05", "Slice,Vf / 1,N.A. / 2,N.A. / 3,0.52",
            // "Time,Counts / 0,"12,345" / 1,"5,432" / 3,987"): every one of them must be such an
            // outlier row, and the row above the topmost one the pure-text header.
            auto isOutlierDataRow = [&](int idx) -> bool {
                if (idx < 0 || !isHeaderCandidateWithFields(idx))
                    return false;
                const SplitLineResult srRow = splitLineEx(idx, bestSep);
                int rowNum = 0, rowTxt = 0;
                rowStats(srRow.parts, rowNum, rowTxt);
                // The outlier sits in a value column ("0, <0.01", "0, OVER"). A row that
                // starts with a name over a first column that holds only numbers further
                // down ("Month,2024" under "Operator,Tanaka", above "1,100" or "1,") is a
                // mixed header with a preamble line above it, not a data row.
                auto firstCellKind = [&](const SplitLineResult &sr) -> int {
                    if (sr.rawParts.isEmpty() || sr.rawParts[0].isEmpty()
                        || isMissingNumericToken(sr.rawParts[0], false))
                        return 0;
                    double dv = 0.0;
                    return tryParseLabDouble(sr.rawParts[0], primaryLoc, hasDistinctNumericLocale,
                                             dv)
                            ? 1
                            : 2;
                };
                bool firstText = firstCellKind(srRow) == 2;
                if (firstText) {
                    bool firstColNumbers = false;
                    int scanned = 0;
                    for (int k = bestStart; k < lines.size() && scanned < 32; ++k) {
                        if (isCommentLineForSep(k, bestSep, true)
                            || isBangSemicolonComment(k, bestSep))
                            continue;
                        const int kind = firstCellKind(splitLineEx(k, bestSep));
                        if (kind == 2) {
                            firstColNumbers = false;
                            break;
                        }
                        if (kind == 1)
                            firstColNumbers = true;
                        ++scanned;
                    }
                    firstText = firstColNumbers;
                }
                return rowNum >= 1 && rowTxt >= 1 && !firstText;
            };
            {
                int top = -1;
                int up = prevNonCommentIdx(bestStart - 1);
                while (up >= 0 && isOutlierDataRow(up)) {
                    top = up;
                    up = prevNonCommentIdx(up - 1);
                }
                if (top >= 0 && up >= 0 && isPureTextRowWithFields(up)
                    && nonEmptyCellCount(up) >= bestFields) {
                    bestStart = top;
                    hdrStart = top;
                }
            }

            const int cand1 = prevNonCommentIdx(bestStart - 1);
            const int cand2 = (cand1 >= 0) ? prevNonCommentIdx(cand1 - 1) : -1;
            const int cand3 = (cand2 >= 0) ? prevNonCommentIdx(cand2 - 1) : -1;

            int namedUnits2 = 0, namedUnits1 = 0;
            bool emptyUnits2 = false, emptyUnits1 = false;
            const bool cand2IsUnit = (cand2 >= 0 && cand3 >= 0)
                    && analyzePreUnitRow(cand2, true, namedUnits2, emptyUnits2);
            const bool cand1IsUnit = (cand1 >= 0 && cand2 >= 0)
                    && analyzePreUnitRow(cand1, false, namedUnits1, emptyUnits1);

            bool matched3RowOrigin = false;
            if (cand3 >= 0 && cand2 >= 0 && cand1 >= 0 && isHeaderCandidateWithFields(cand3)
                && nonEmptyCellCount(cand3) >= (bestFields >= 2 ? 2 : 1)
                && isPureTextRowWithFields(cand1) && nonEmptyCellCount(cand1) >= 1 && cand2IsUnit
                && (!cand1IsUnit || namedUnits2 > namedUnits1
                    || (emptyUnits2 && namedUnits1 == 0))) {
                hdrStart = cand3;
                forcedUnitsRow = 1;
                forcedCommentsRow = 2;
                matched3RowOrigin = true;
            }
            if (!matched3RowOrigin) {
                if (cand1 >= 0 && isPureTextRowWithFields(cand1)) {
                    hdrStart = cand1;
                    if (cand2 >= 0 && isHeaderCandidateWithFields(cand2)) {
                        bool shortCells = true;
                        for (const QString &u : splitLine(cand1, bestSep))
                            shortCells = shortCells && u.size() <= 24;
                        if (shortCells) {
                            hdrStart = cand2;
                            forcedUnitsRow = 1;
                        }
                    }
                } else if (cand1 >= 0 && isHeaderCandidateWithFields(cand1)) {
                    hdrStart = cand1;
                }
            }
            int preambleEnd = hdrStart;
            if (hdrStart == bestStart) {
                while (preambleEnd > 0 && isCommentLineForSep(preambleEnd - 1, bestSep, false))
                    --preambleEnd;
            }
            QStringList pre;
            for (int i = 0; i < preambleEnd; ++i)
                pre << QString(buf + lines[i].start, lines[i].end - lines[i].start).trimmed();
            // A row of numbers above the block can be a data row the block search skipped: the
            // file has no preamble then and is read from its first line, as app.import_csv reads
            // it (hasDataPreamble() runs this same code). That is so for a row of two numbers or
            // more that is
            //  - narrower than the block and joined to it without a text row in between (a table
            //    whose rows get wider further down: "t,T1 / 0,20 / 1,21 / 2,22,32",
            //    "a,b / 1,2 / 3,4 / 5,6,7 / 8,9,10,11", "1,2 / 3,4,5"), or
            //  - in an earlier block of the same table, under a copy of the header right above
            //    the block (blocks separated by blank lines: "x y / 1 10 / 2 20 / x y / 1 11").
            // A metadata preamble may hold numbers too and stays one: a summary row as wide as
            // the data under its own header ("Specimen,MaxForce,Elong / 1,1234.5,12.3 /
            // Time,Force,Disp / 0,0,0"), "Points,1001", a single "1001".
            bool numberRowsAreData = false;
            if (hdrStart > 0) {
                auto sameCells = [&](int a, int b) {
                    const SplitLineResult sra = splitLineEx(a, bestSep);
                    int na = 0, ta = 0;
                    rowStats(sra.parts, na, ta);
                    if (ta == 0)
                        return false;
                    const SplitLineResult srb = splitLineEx(b, bestSep);
                    return sra.parts == srb.parts;
                };
                QVector<int> nums(bestStart, 0), txts(bestStart, 0), widths(bestStart, 0);
                QVector<bool> firstCellNum(bestStart, false);
                for (int i = 0; i < bestStart; ++i) {
                    const SplitLineResult sri = splitLineEx(i, bestSep);
                    rowStats(sri.parts, nums[i], txts[i]);
                    widths[i] = sri.parts.size();
                    if (!sri.rawParts.isEmpty() && !sri.rawParts[0].isEmpty()
                        && !isMissingNumericToken(sri.rawParts[0], false)) {
                        double dv = 0.0;
                        firstCellNum[i] = tryParseLabDouble(sri.rawParts[0], primaryLoc,
                                                            hasDistinctNumericLocale, dv);
                    }
                }
                auto numberRow = [&](int i) { return nums[i] >= 1 && txts[i] == 0; };
                for (int i = 0; i < hdrStart && !numberRowsAreData; ++i) {
                    const bool pureNumRow = (nums[i] >= 2 && txts[i] == 0);
                    const bool mixedNumRow = (nums[i] >= 2 && nums[i] >= txts[i])
                            && (!bestStartAllNumeric
                                || (i > 0 && nums[i - 1] == 0 && txts[i - 1] == widths[i]
                                    && firstCellNum[i]));
                    if (!pureNumRow && !mixedNumRow)
                        continue;
                    if (widths[i] < bestFields) {
                        bool joined = true;
                        for (int k = i + 1; k < bestStart && joined; ++k)
                            joined = !(nums[k] == 0 && txts[k] > 0);
                        if (joined) {
                            numberRowsAreData = true;
                            continue;
                        }
                    }
                    if (!pureNumRow)
                        continue;
                    int top = i, bottom = i;
                    while (top > 0 && numberRow(top - 1))
                        --top;
                    while (bottom + 1 < hdrStart && numberRow(bottom + 1))
                        ++bottom;
                    if (bottom + 1 == hdrStart && hdrStart < bestStart && top > 0
                        && sameCells(top - 1, hdrStart))
                        numberRowsAreData = true;
                }
            }
            if (numberRowsAreData) {
                hdrStart = 0;
                forcedUnitsRow = -1;
                forcedCommentsRow = -1;
            }
            if (hdrStart > 0) {
                skippedNonBlankLines += preambleEnd;
                preambleSeparator = QString(bestSep);
                // Skip any '#'-comment lines immediately above numeric data (preserving '# col1, col2' header)
                for (int ci = preambleEnd; ci < hdrStart; ++ci) {
                    int lineStart = lines[ci].start;
                    const int lineEnd = lines[ci].end;
                    while (lineStart < lineEnd
                           && (buf[lineStart] == QLatin1Char(' ')
                               || buf[lineStart] == QLatin1Char('\t'))) {
                        ++lineStart;
                    }
                    if (lineStart < lineEnd && buf[lineStart] == QLatin1Char('#')) {
                        lastHashHadSpaceAfterHash =
                                (lineStart + 1 < lineEnd && buf[lineStart + 1].isSpace());
                        lastHashCommentBody =
                                QString(buf + lineStart + 1, lineEnd - (lineStart + 1)).trimmed();
                        pre << QString(buf + lineStart, lineEnd - lineStart).trimmed();
                        ++skippedNonBlankLines;
                    }
                }
                pos = lines[hdrStart].start;
                g_lastImportReport.preamble = pre.mid(0, 200).join(QLatin1Char('\n'));
            }
        }
    }
    // hasDataPreamble(): only the preamble decision is wanted, not the table
    if (preambleProbeOnly) {
        g_lastImportReport.separator = preambleSeparator;
        return nullptr;
    }

    // Gather up to 15 sample lines for separator auto-detection
    QStringList sampleLines;
    QStringList hashDelimSampleFallback;
    {
        int sPos = pos;
        while (sPos < len && sampleLines.size() < 15) {
            const int sEnd = scanLogicalLineEnd(sPos);
            const QString ln = QString(buf + sPos, sEnd - sPos).trimmed();
            if (!ln.isEmpty()) {
                if (!ln.startsWith(QLatin1Char('#'))) {
                    sampleLines.append(ln);
                } else if (hashDelimSampleFallback.size() < 8
                           && (ln.contains(QLatin1Char(',')) || ln.contains(QLatin1Char('\t'))
                               || ln.contains(QLatin1Char(';'))
                               || (ln.size() >= 2 && ln.at(1).isDigit()
                                   && ln.contains(QLatin1Char(' '))))) {
                    hashDelimSampleFallback.append(ln);
                }
            }
            sPos = sEnd;
            if (sPos < len && buf[sPos] == QLatin1Char('\r')) {
                ++sPos;
                if (sPos < len && buf[sPos] == QLatin1Char('\n'))
                    ++sPos;
            } else if (sPos < len && buf[sPos] == QLatin1Char('\n')) {
                ++sPos;
            }
        }
        if (sampleLines.isEmpty() && !hashDelimSampleFallback.isEmpty())
            sampleLines = hashDelimSampleFallback;
    }

    QString effSep = separatorHint;
    if (effSep == QLatin1String("\\t"))
        effSep = QStringLiteral("\t");
    else if (effSep == QLatin1String("\\s"))
        effSep = QStringLiteral(" ");

    const bool isXrdAsciiExt =
            filePathHint.endsWith(QLatin1String(".xy"), Qt::CaseInsensitive)
            || filePathHint.endsWith(QLatin1String(".xye"), Qt::CaseInsensitive)
            || filePathHint.endsWith(QLatin1String(".asc"), Qt::CaseInsensitive)
            || filePathHint.endsWith(QLatin1String(".chi"), Qt::CaseInsensitive)
            || filePathHint.endsWith(QLatin1String(".uxd"), Qt::CaseInsensitive);

    bool whitespaceDelim = false;
    if (!preambleSeparator.isEmpty()
        && (effSep.isEmpty() || effSep.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0)) {
        effSep = preambleSeparator;
        if (effSep == QLatin1String(" "))
            whitespaceDelim = true;
    } else if (effSep.isEmpty() || effSep.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0) {
        effSep = detectDelimiterFromSampleLines(sampleLines, filePathHint);
        if (effSep == QLatin1String(" "))
            whitespaceDelim = true;
    } else if (effSep == QLatin1String(" ") && (simplifySpaces || autoFixTabOnCsv || isXrdAsciiExt)) {
        whitespaceDelim = true;
    } else if (autoFixTabOnCsv && effSep == QLatin1String("\t") && !sampleLines.isEmpty()) {
        // Auto-recover when GUI default "\t" is used on comma/semicolon/whitespace files with 0 tabs
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
                if (isCsvExt || sampleLines.size() >= 2 || europeanSemicolonDetected)
                    effSep = detected;
            } else if (detected == QLatin1String(" ")) {
                effSep = detected;
                whitespaceDelim = true;
            }
        }
    }
    const QString normSep = effSep.isEmpty() ? QStringLiteral(",") : effSep;

    // Determine whether this table has >= 2 columns so '# ...' comment lines inside a
    // multi-column CSV/TSV/whitespace file (including '# pause,', '# note,,', '# T=300 K, P=1 atm')
    // are skipped as comments while '#1, 520' / '#2,' specimen rows, mid-file '#A1, 520' IDs,
    // '#N/A,#N/A' missing rows, and 1-column '#1\n#2' tables are kept.
    auto isSpanHashComment = [&](const QVector<ZeroCopyCellSpan> &spans, bool multiCols,
                                 bool allowAlphaHashIds) -> bool {
        return isHashCommentRecordCells(
                spans.size(), whitespaceDelim, multiCols, allowAlphaHashIds, primaryLoc,
                hasDistinctNumericLocale,
                [&](int k, const QChar *&pk, int &nk, bool &kQuoted) {
                    if (k >= 0 && k < spans.size()) {
                        spans[k].trimmedView(pk, nk);
                        kQuoted = spans[k].quoted;
                    } else {
                        pk = nullptr;
                        nk = 0;
                        kQuoted = false;
                    }
                });
    };

    auto rowHasRealNumericData = [&](const QVector<ZeroCopyCellSpan> &spans) -> bool {
        if (spans.isEmpty())
            return false;
        if (!convertToNumeric)
            return true;
        for (int c = 0; c < spans.size(); ++c) {
            const QChar *p = nullptr;
            int n = 0;
            spans[c].trimmedView(p, n);
            if (spans[c].quoted || !p || n <= 0 || isMissingNumericSpan(p, n, false))
                continue;
            double dv = 0.0;
            if (tryParseLabDoubleSpan(p, n, primaryLoc, hasDistinctNumericLocale, dv))
                return true;
        }
        return false;
    };

    QVector<ZeroCopyCellSpan> rowSpans;
    rowSpans.reserve(16);
    bool tableHasMultipleCols = !preambleSeparator.isEmpty();
    if (!tableHasMultipleCols) {
        auto peekEffectiveCols = [](const QVector<ZeroCopyCellSpan> &spans) -> int {
            if (!spans.isEmpty() && !spans[0].quoted) {
                const QChar *p0 = nullptr;
                int n0 = 0;
                spans[0].trimmedView(p0, n0);
                if (n0 > 0 && p0[0] == QLatin1Char('#'))
                    return spans.size(); // preserve '#2,' / '#DIV/0!,' two-column intent
            }
            int n = spans.size();
            while (n > 1 && !spans[n - 1].quoted) {
                const QChar *p = nullptr;
                int l = 0;
                spans[n - 1].trimmedView(p, l);
                if (l > 0)
                    break;
                --n;
            }
            return n;
        };
        int peekPos = pos;
        int checkedRows = 0;
        bool peekAlphaHashMulti = false;
        while (peekPos < len && checkedRows < 24) {
            bool skipPeek = false;
            if (!readNextRecordSpans(buf, len, peekPos, normSep, whitespaceDelim, trimSpaces,
                                     simplifySpaces, rowSpans, skipPeek)) {
                break;
            }
            if (skipPeek)
                continue;
            if (isSpanHashComment(rowSpans, true /*multiCols*/, false /*allowAlphaHashIds*/)) {
                if (!isSpanHashComment(rowSpans, true, true) && peekEffectiveCols(rowSpans) >= 2)
                    peekAlphaHashMulti = true;
                continue;
            }
            if (peekEffectiveCols(rowSpans) >= 2) {
                tableHasMultipleCols = true;
                break;
            }
            ++checkedRows;
        }
        if (!tableHasMultipleCols && peekAlphaHashMulti)
            tableHasMultipleCols = true;
    }

    auto isBodyHashCommentRow = [&](const QVector<ZeroCopyCellSpan> &spans,
                                    bool allowAlphaHashIds) -> bool {
        return isSpanHashComment(spans, tableHasMultipleCols, allowAlphaHashIds);
    };

    // 4. Parse up to 16 preview rows (zero spike even on 200,000-row CSVs!) for header/unit discovery
    QVector<QVector<LabCsvCell>> previewRecords;
    previewRecords.reserve(16);
    QVector<int> previewLineStartOffsets;
    previewLineStartOffsets.reserve(17);
    QVector<int> previewSkippedCommentOffsets;
    int scanPos = pos;
    int maxCols = 0;
    bool anyNumericPreviewRow = false;
    bool sawSkippedAlphaHashWithNum = false;
    QSet<int> skippedAlphaHashColCounts;
    bool step4SeenData = false;

    auto runPreviewScan = [&](bool pass2AllowAlpha, bool pass1HadNonHashRows) {
        previewRecords.clear();
        previewLineStartOffsets.clear();
        previewSkippedCommentOffsets.clear();
        scanPos = pos;
        maxCols = 0;
        anyNumericPreviewRow = false;
        sawSkippedAlphaHashWithNum = false;
        skippedAlphaHashColCounts.clear();
        step4SeenData = false;
        bool seenRealData = false;

        while (scanPos < len && previewRecords.size() < 16) {
            const int rowStartOffset = scanPos;
            bool skipRow = false;
            if (!readNextRecordSpans(buf, len, scanPos, normSep, whitespaceDelim, trimSpaces,
                                     simplifySpaces, rowSpans, skipRow)) {
                break;
            }
            if (skipRow)
                continue;
            const bool allowAlphaNow = seenRealData
                    || (pass2AllowAlpha
                        && (pass1HadNonHashRows ? !previewRecords.isEmpty() : true));
            if (isBodyHashCommentRow(rowSpans, allowAlphaNow)) {
                if (!allowAlphaNow && !isBodyHashCommentRow(rowSpans, true)) {
                    sawSkippedAlphaHashWithNum = true;
                    skippedAlphaHashColCounts.insert(rowSpans.size());
                }
                previewSkippedCommentOffsets.append(rowStartOffset);
                continue;
            }
            if (rowHasRealNumericData(rowSpans)) {
                seenRealData = true;
                anyNumericPreviewRow = true;
                step4SeenData = true;
            }
            previewLineStartOffsets.append(rowStartOffset);
            QVector<LabCsvCell> rec;
            rec.reserve(rowSpans.size());
            for (const ZeroCopyCellSpan &sp : rowSpans)
                rec.append({ sp.toQString(trimSpaces, simplifySpaces), sp.quoted });
            maxCols = std::max(maxCols, rec.size());
            previewRecords.append(std::move(rec));
        }
        previewLineStartOffsets.append(scanPos);
    };

    runPreviewScan(false, false);
    bool usedAlphaHashPass2 = false;
    if (sawSkippedAlphaHashWithNum
        && (previewRecords.isEmpty() || !anyNumericPreviewRow
            || skippedAlphaHashColCounts.contains(maxCols))) {
        usedAlphaHashPass2 = true;
        runPreviewScan(true, !previewRecords.isEmpty());
    }

    if (previewRecords.isEmpty())
        return new future::Table(0, 0, effTableName);

    // Check if '# col1, col2' right above numeric row 0 was actually the column header line
    bool prependedHashHeader = false;
    QVector<LabCsvCell> headerRow;
    if (firstRowNamesColumns && !lastHashCommentBody.isEmpty() && !previewRecords.isEmpty()
        && maxCols >= 2) {
        bool row0AllNumeric = true;
        for (const LabCsvCell &c0 : previewRecords[0]) {
            double dummy = 0.0;
            if (c0.quoted
                || !tryParseLabDouble(c0.value, primaryLoc, hasDistinctNumericLocale, dummy)) {
                row0AllNumeric = false;
                break;
            }
        }
        if (row0AllNumeric) {
            QString hashHdrBody = lastHashCommentBody;
            bool strippedSpecL = false;
            if (!lastHashHadSpaceAfterHash && hashHdrBody.size() >= 2
                && hashHdrBody.at(0) == QLatin1Char('L') && hashHdrBody.at(1).isSpace()) {
                hashHdrBody = hashHdrBody.mid(1).trimmed();
                strippedSpecL = true;
            }
            QVector<QVector<LabCsvCell>> hashRows;
            int hashCols = 0;
            if (strippedSpecL && whitespaceDelim && hashHdrBody.contains(QLatin1String("  "))) {
                static const QRegularExpression kTwoSpace(QStringLiteral("\\s{2,}"));
                const QStringList dblParts = hashHdrBody.split(kTwoSpace, Qt::SkipEmptyParts);
                if (dblParts.size() == previewRecords[0].size()) {
                    QVector<LabCsvCell> one;
                    for (const QString &p : dblParts)
                        one.append({ p.trimmed(), false });
                    hashRows.append(one);
                    hashCols = dblParts.size();
                }
            }
            if (hashRows.isEmpty()) {
                parseDelimitedRecordsFast(hashHdrBody, 0, normSep, whitespaceDelim, true,
                                          simplifySpaces, hashRows, hashCols);
            }
            if (hashRows.size() == 1 && hashRows[0].size() == previewRecords[0].size()) {
                bool anyNonNumericHdr = false;
                bool anyPureNumHdr = false;
                for (const LabCsvCell &hc : hashRows[0]) {
                    if (!hc.value.trimmed().isEmpty()) {
                        double dummy = 0.0;
                        if (tryParseLabDouble(hc.value, primaryLoc, hasDistinctNumericLocale, dummy))
                            anyPureNumHdr = true;
                        else
                            anyNonNumericHdr = true;
                    }
                }
                const bool isSpecDirective =
                        (!strippedSpecL && whitespaceDelim && !lastHashHadSpaceAfterHash
                         && anyPureNumHdr);
                if (anyNonNumericHdr && !isSpecDirective) {
                    headerRow = hashRows[0];
                    prependedHashHeader = true;
                }
            }
        }
    }

    // Auto-detect headerless numeric files (e.g. .xy/.asc XRD or numeric tables where row 0 is pure float data)
    bool effectiveFirstRowNames = firstRowNamesColumns;
    int fileHeaderLinesConsumed = 0;

    if (firstRowNamesColumns && !prependedHashHeader && !previewRecords.isEmpty()) {
        bool treatRow0AsData = false;
        // Classify a cell of a candidate data row: real number, missing token (empty / NaN /
        // N/A / "-"), or anything else (=> text, i.e. the row is a header).
        auto classifyCell = [&](const LabCsvCell &cell, bool &isNumber, bool &isMissingToken,
                                bool &isFloatMarker) -> bool {
            const QString t = cell.value.trimmed();
            isNumber = false;
            isMissingToken = false;
            isFloatMarker = false;
            double dv = 0.0;
            if (cell.quoted)
                return false;
            if (!t.isEmpty() && tryParseLabDouble(t, primaryLoc, useCustomLoc, dv)) {
                isNumber = true;
                isFloatMarker = t.contains(QLatin1Char('.')) || t.contains(QLatin1Char('e'))
                        || t.contains(QLatin1Char('E')) || t.startsWith(QLatin1Char('-'));
                return true;
            }
            if (isMissingNumericToken(t, false)) {
                isMissingToken = !t.isEmpty();
                return true;
            }
            return false;
        };
        // (the second row may be wider: a table whose rows get wider further down, "1,2 / 3,4,5",
        // as app.import_csv reads it)
        if (previewRecords.size() >= 2 && previewRecords[0].size() >= 2
            && previewRecords[0].size() <= previewRecords[1].size()) {
            bool allRow0Num = true;
            bool allRow1Num = true;
            bool anyFloatMarkerInRow0 = false;
            bool anyFloatMarkerInRow1 = false;
            bool row0IsOneBasedSeq = (previewRecords[0].size() >= 3);
            bool row0AnyNumber = false;
            bool row0AnyMissingToken = false;
            bool row1AnyNumber = false;
            for (int c = 0; c < previewRecords[0].size(); ++c) {
                bool isNum = false, isTok = false, isFloat = false;
                if (!classifyCell(previewRecords[0][c], isNum, isTok, isFloat)) {
                    allRow0Num = false;
                    row0IsOneBasedSeq = false;
                    break;
                }
                row0AnyNumber = row0AnyNumber || isNum;
                row0AnyMissingToken = row0AnyMissingToken || isTok;
                if (!isNum) {
                    row0IsOneBasedSeq = false;
                    continue;
                }
                const QString t = previewRecords[0][c].value.trimmed();
                if (isFloat) {
                    anyFloatMarkerInRow0 = true;
                    row0IsOneBasedSeq = false;
                } else if (t != QString::number(c + 1)) {
                    row0IsOneBasedSeq = false;
                }
            }
            // A first row with missing cells (NaN / blank) next to numbers is still data
            if (allRow0Num && !row0AnyNumber && !row0AnyMissingToken)
                allRow0Num = false;
            if (allRow0Num) {
                for (const LabCsvCell &c1 : previewRecords[1]) {
                    bool isNum = false, isTok = false, isFloat = false;
                    if (!classifyCell(c1, isNum, isTok, isFloat)) {
                        allRow1Num = false;
                        break;
                    }
                    row1AnyNumber = row1AnyNumber || isNum;
                    if (isFloat && isNum)
                        anyFloatMarkerInRow1 = true;
                }
                if (!row1AnyNumber)
                    allRow1Num = false;
            }
            if (allRow0Num && allRow1Num) {
                const bool likelySeqColHeader = (previewRecords[0].size() >= 3
                                                 && row0IsOneBasedSeq && !anyFloatMarkerInRow0
                                                 && anyFloatMarkerInRow1);
                if (!likelySeqColHeader)
                    treatRow0AsData = true;
            }
            if (!treatRow0AsData && allRow0Num && !previewRecords[1][0].quoted
                && previewRecords[1][0].value.trimmed().startsWith(QLatin1Char('#'))) {
                bool row1TrailingOk = true;
                bool row1TrailingAnyNum = false;
                for (int c = 1; c < previewRecords[1].size(); ++c) {
                    bool isNum = false, isTok = false, isFloat = false;
                    if (!classifyCell(previewRecords[1][c], isNum, isTok, isFloat)) {
                        row1TrailingOk = false;
                        break;
                    }
                    row1TrailingAnyNum = row1TrailingAnyNum || isNum;
                    if (isFloat && isNum)
                        anyFloatMarkerInRow1 = true;
                }
                const bool likelySeqColHeader = (previewRecords[0].size() >= 3
                                                 && row0IsOneBasedSeq && !anyFloatMarkerInRow0
                                                 && anyFloatMarkerInRow1);
                if (row1TrailingOk && row1TrailingAnyNum && !likelySeqColHeader)
                    treatRow0AsData = true;
            }
        }
        // One column of numbers, or a file with a single data row: the first line is data, it
        // must not disappear into the column name.
        if (!treatRow0AsData && !previewRecords.isEmpty()) {
            bool uniformWidth = true;
            const int w = previewRecords[0].size();
            for (const auto &rec : previewRecords) {
                if (rec.size() != w) {
                    uniformWidth = false;
                    break;
                }
            }
            if (uniformWidth && w >= 1 && (w == 1 || previewRecords.size() == 1)) {
                bool allDataLike = true;
                bool anyNumber = false;
                bool row0Number = false;
                for (int r = 0; r < previewRecords.size() && allDataLike; ++r) {
                    for (int c = 0; c < w; ++c) {
                        bool isNum = false, isTok = false, isFloat = false;
                        if (!classifyCell(previewRecords[r][c], isNum, isTok, isFloat)) {
                            allDataLike = false;
                            break;
                        }
                        anyNumber = anyNumber || isNum;
                        if (r == 0 && isNum)
                            row0Number = true;
                    }
                }
                if (allDataLike && anyNumber && (row0Number || previewRecords.size() >= 2))
                    treatRow0AsData = true;
            }
        }        if (!treatRow0AsData && previewRecords[0].size() >= 2 && !previewRecords[0][0].quoted
            && previewRecords[0][0].value.trimmed().startsWith(QLatin1Char('#'))) {
            for (int c = 1; c < previewRecords[0].size(); ++c) {
                const LabCsvCell &cell = previewRecords[0][c];
                const QString tc = cell.value.trimmed();
                double dv = 0.0;
                if (isMissingNumericToken(tc, cell.quoted)
                    || (!cell.quoted && !tc.isEmpty()
                        && tryParseLabDouble(tc, primaryLoc, useCustomLoc, dv))) {
                    treatRow0AsData = true;
                    break;
                }
            }
        } else if (!treatRow0AsData && previewRecords.size() >= 2
                   && previewRecords[0].size() == 1 && previewRecords[1].size() == 1
                   && !previewRecords[0][0].quoted && !previewRecords[1][0].quoted) {
            const QString t0 = previewRecords[0][0].value.trimmed();
            const QString t1 = previewRecords[1][0].value.trimmed();
            const bool r0HashData = (t0.size() >= 2 && t0.at(0) == QLatin1Char('#')
                                     && (t0.at(1).isDigit()
                                         || isMissingDataTokenSpan(t0.constData(), t0.size())));
            const bool r1HashData = (t1.size() >= 2 && t1.at(0) == QLatin1Char('#')
                                     && (t1.at(1).isDigit()
                                         || isMissingDataTokenSpan(t1.constData(), t1.size())));
            if (r0HashData && r1HashData)
                treatRow0AsData = true;
        }
        if (treatRow0AsData) {
            effectiveFirstRowNames = false;
            fileHeaderLinesConsumed = 0;
        } else {
            headerRow = previewRecords[0];
            fileHeaderLinesConsumed = 1;
        }
    }

    QVector<LabCsvCell> unitsRow;
    QVector<LabCsvCell> commentsRow;
    // 2-Row [Name + Units] & 3-Row Origin [Long Name + Units + Comments] auto-detection
    const int effUnitsRowHint = (unitsRowHint < 0 && forcedUnitsRow == 1) ? 1 : unitsRowHint;
    if (effectiveFirstRowNames && unitsRowHint < 0 && forcedUnitsRow == 1 && forcedCommentsRow == 2
        && previewRecords.size() >= 4) {
        unitsRow = previewRecords[1];
        commentsRow = previewRecords[2];
        fileHeaderLinesConsumed = 3;
    } else if (effectiveFirstRowNames && effUnitsRowHint != 0) {
        const int candUnitIdx = fileHeaderLinesConsumed;
        if (effUnitsRowHint == 1 && previewRecords.size() > candUnitIdx) {
            unitsRow = previewRecords[candUnitIdx];
            ++fileHeaderLinesConsumed;
        } else if (effUnitsRowHint < 0 && previewRecords.size() > candUnitIdx + 1) {
            const int checkEnd = std::min(previewRecords.size(), 15);
            auto isNumericColumnFromRow = [&](int c, int dataStartRow) -> bool {
                int colFinite = 0;
                int colText = 0;
                for (int r = dataStartRow; r < checkEnd; ++r) {
                    const QString s = (c < previewRecords[r].size())
                            ? previewRecords[r][c].value.trimmed()
                            : QString();
                    const bool q =
                            (c < previewRecords[r].size()) ? previewRecords[r][c].quoted : false;
                    if (isMissingNumericToken(s, q))
                        continue;
                    double dv = 0.0;
                    if (!q && tryParseLabDouble(s, primaryLoc, useCustomLoc, dv))
                        ++colFinite;
                    else
                        ++colText;
                }
                return colFinite >= 1
                        && (colText == 0
                            || (colFinite >= 5
                                && qint64(colFinite) * 10
                                        >= qint64(colFinite + colText) * 9));
            };
            auto inspectUnitCandidate = [&](int unitRowIdx, int dataStartRow,
                                            bool allowAllEmptyUnits, int &outNamedUnits,
                                            bool &outAllEmpty) -> bool {
                outNamedUnits = 0;
                outAllEmpty = false;
                if (unitRowIdx < 0 || unitRowIdx >= previewRecords.size()
                    || dataStartRow >= previewRecords.size()) {
                    return false;
                }
                const QVector<LabCsvCell> &candUnits = previewRecords[unitRowIdx];
                if (!candUnits.isEmpty() && !candUnits[0].quoted
                    && candUnits[0].value.trimmed().startsWith(QLatin1Char('#'))) {
                    return false;
                }
                bool rowAllNonNumeric = true;
                int nonEmptyAnywhere = 0;
                for (const LabCsvCell &uc : candUnits) {
                    const QString u = uc.value.trimmed();
                    if (u.isEmpty())
                        continue;
                    ++nonEmptyAnywhere;
                    double dummy = 0.0;
                    if (tryParseLabDouble(u, primaryLoc, useCustomLoc, dummy)) {
                        rowAllNonNumeric = false;
                        break;
                    }
                }
                if (nonEmptyAnywhere == 0) {
                    int anyNumCols = 0;
                    for (int c = 0; c < maxCols; ++c) {
                        if (isNumericColumnFromRow(c, dataStartRow))
                            ++anyNumCols;
                    }
                    outAllEmpty = true;
                    return allowAllEmptyUnits && !whitespaceDelim && anyNumCols >= 1;
                }

                int numericDataCols = 0;
                bool hasNumberInNumericCol = false;
                bool allValidUnitsInNumericCols = true;
                bool allFallbackInNumericCols = true;
                int recognizedUnitTokens = 0;
                int nonEmptyUnitCellsInNumericCols = 0;
                bool shortCells = true;
                bool hasInlineHeaderUnit = false;

                for (int c = 0; c < maxCols; ++c) {
                    if (!isNumericColumnFromRow(c, dataStartRow))
                        continue;
                    ++numericDataCols;
                    const QString uRaw =
                            (c < candUnits.size()) ? candUnits[c].value.trimmed() : QString();
                    if (!uRaw.isEmpty()) {
                        double dummy = 0.0;
                        if (tryParseLabDouble(uRaw, primaryLoc, useCustomLoc, dummy)) {
                            hasNumberInNumericCol = true;
                            break;
                        }
                        QString sn, ln, un;
                        AsciiTableImportFilter::splitHeaderNameAndUnits(uRaw, sn, ln, un);
                        if (!ln.isEmpty() && !un.isEmpty() && ln != uRaw)
                            hasInlineHeaderUnit = true;
                        const QString uClean = cleanPromotedUnitCell(uRaw);
                        ++nonEmptyUnitCellsInNumericCols;
                        if (uClean.size() > 32)
                            shortCells = false;
                        if (isLikelyScientificUnit(uClean)) {
                            if (!isDimensionlessDashUnit(uClean))
                                ++outNamedUnits;
                            ++recognizedUnitTokens;
                        } else {
                            allValidUnitsInNumericCols = false;
                            if (!hasFallbackUnitSyntaxAscii(uRaw))
                                allFallbackInNumericCols = false;
                        }
                    }
                }
                if (numericDataCols < 1 || hasNumberInNumericCol || hasInlineHeaderUnit)
                    return false;
                if (allValidUnitsInNumericCols && recognizedUnitTokens >= 1)
                    return true;
                // Units outside the built-in dictionary (N/mm2, kgf/mm^2, 'mm/min', ...): the
                // rows below are numbers and the unit row has no number at all.
                return rowAllNonNumeric && nonEmptyUnitCellsInNumericCols >= 1 && shortCells
                        && allFallbackInNumericCols;
            };

            bool matched3RowOrigin = false;
            if (previewRecords.size() > candUnitIdx + 2) {
                const int candCommentsIdx = candUnitIdx + 1;
                const int dataStart3 = candUnitIdx + 2;
                const QVector<LabCsvCell> &candComments = previewRecords[candCommentsIdx];
                bool row2HasNumInNumericCol = false;
                bool row2HasTextInNumericCol = false;
                int numericColsFrom3 = 0;
                for (int c = 0; c < maxCols; ++c) {
                    if (!isNumericColumnFromRow(c, dataStart3))
                        continue;
                    ++numericColsFrom3;
                    const QString cm = (c < candComments.size())
                            ? candComments[c].value.trimmed()
                            : QString();
                    const bool q = (c < candComments.size()) ? candComments[c].quoted : false;
                    if (cm.isEmpty() || isMissingNumericToken(cm, q))
                        continue;
                    double dummy = 0.0;
                    if (!q && tryParseLabDouble(cm, primaryLoc, useCustomLoc, dummy)) {
                        row2HasNumInNumericCol = true;
                        break;
                    }
                    row2HasTextInNumericCol = true;
                }
                if (numericColsFrom3 >= 1 && row2HasTextInNumericCol && !row2HasNumInNumericCol) {
                    int namedUnits1 = 0, namedUnits2 = 0;
                    bool emptyUnits1 = false, emptyUnits2 = false;
                    const bool unit1Matched = inspectUnitCandidate(
                            candUnitIdx, dataStart3, true /*allowAllEmpty*/, namedUnits1,
                            emptyUnits1);
                    const bool unit2Matched = inspectUnitCandidate(
                            candCommentsIdx, dataStart3, false /*allowAllEmpty*/, namedUnits2,
                            emptyUnits2);
                    if (unit1Matched
                        && (namedUnits1 > namedUnits2 || (emptyUnits1 && namedUnits2 == 0)
                            || !unit2Matched)) {
                        unitsRow = previewRecords[candUnitIdx];
                        commentsRow = previewRecords[candCommentsIdx];
                        fileHeaderLinesConsumed += 2;
                        matched3RowOrigin = true;
                    }
                }
            }
            if (!matched3RowOrigin) {
                int namedUnits2 = 0;
                bool emptyUnits2 = false;
                if (inspectUnitCandidate(candUnitIdx, candUnitIdx + 1, false /*allowAllEmpty*/,
                                         namedUnits2, emptyUnits2)) {
                    unitsRow = previewRecords[candUnitIdx];
                    ++fileHeaderLinesConsumed;
                }
            }
        }
    }

    // Determine exact character offset of first data row and count remaining rows without allocating QStrings
    const int dataStartPos = previewLineStartOffsets[std::min(
            fileHeaderLinesConsumed, previewLineStartOffsets.size() - 1)];
    const bool allowAlphaAtDataStart = usedAlphaHashPass2
            || (fileHeaderLinesConsumed < previewRecords.size()
                && !previewRecords[fileHeaderLinesConsumed].isEmpty()
                && !previewRecords[fileHeaderLinesConsumed][0].quoted
                && previewRecords[fileHeaderLinesConsumed][0].value.trimmed().startsWith(
                        QLatin1Char('#')));
    for (int off : previewSkippedCommentOffsets) {
        if (off < dataStartPos)
            ++skippedNonBlankLines;
    }
    int dataRowCount = std::max(0, previewRecords.size() - fileHeaderLinesConsumed);
    // Number of fields per line (header and data lines that make up maxCols), for the outlier
    // test below: narrow widths in a plain array (the hot path), the rare wide ones in a hash.
    constexpr int kWidthHistSmall = 256;
    QVector<qint64> widthHistSmall(kWidthHistSmall + 1, 0);
    QHash<int, qint64> widthHistLarge;
    qint64 *const widthHist = widthHistSmall.data();
    // Every line ending in a separator ("x,y,", "1,2,") with nothing after it, the header line
    // included: the separator is a line terminator, not an empty last column.
    int trailingSepWidth = -1;
    bool allLinesEndWithSep = !whitespaceDelim;
    auto noteTrailingSep = [&](int w, bool lastEmptyUnquoted) {
        if (!allLinesEndWithSep)
            return;
        if (w < 2 || !lastEmptyUnquoted || (trailingSepWidth >= 0 && w != trailingSepWidth))
            allLinesEndWithSep = false;
        trailingSepWidth = w;
    };
    for (const QVector<LabCsvCell> &rec : previewRecords) {
        if (rec.size() <= kWidthHistSmall)
            ++widthHist[rec.size()];
        else
            ++widthHistLarge[rec.size()];
        noteTrailingSep(rec.size(),
                        !rec.isEmpty() && !rec.last().quoted && rec.last().value.trimmed().isEmpty());
    }
    {
        int countPos = scanPos;
        bool seenCountData = step4SeenData || allowAlphaAtDataStart;
        while (countPos < len) {
            bool skipRow = false;
            if (!readNextRecordSpans(buf, len, countPos, normSep, whitespaceDelim, trimSpaces,
                                     simplifySpaces, rowSpans, skipRow)) {
                break;
            }
            if (skipRow || isBodyHashCommentRow(rowSpans, seenCountData))
                continue;
            if (!seenCountData && rowHasRealNumericData(rowSpans))
                seenCountData = true;
            const int w = rowSpans.size();
            maxCols = std::max(maxCols, w);
            if (w <= kWidthHistSmall)
                ++widthHist[w];
            else
                ++widthHistLarge[w];
            ++dataRowCount;
            if (allLinesEndWithSep) {
                const QChar *lp = nullptr;
                int ll = 0;
                if (w > 0)
                    rowSpans[w - 1].trimmedView(lp, ll);
                noteTrailingSep(w, w > 0 && !rowSpans[w - 1].quoted && ll == 0);
            }
        }
    }
    if (allLinesEndWithSep && trailingSepWidth >= 2 && trailingSepWidth == maxCols)
        --maxCols;
    maxCols = std::max(1, maxCols);

    // One malformed line with a huge number of fields would make maxCols x dataRowCount buffers
    // (and as many Column objects) below and end in bad_alloc. Refuse such a file before any
    // allocation (see kAsciiImportOutlierCells); the reason goes to the import report.
    {
        const qint64 cells = qint64(maxCols) * qint64(std::max(1, dataRowCount));
        QString refusal;
        if (maxCols > g_maxImportColumns) {
            refusal = QObject::tr("A line of the file has %1 fields, but at most %2 columns can "
                                  "be imported. Check the column separator.")
                              .arg(maxCols)
                              .arg(g_maxImportColumns);
        } else if (cells > g_importOutlierCells) {
            // widths in ascending order with their line counts
            QVector<QPair<int, qint64>> widths;
            for (int w = 0; w <= kWidthHistSmall; ++w) {
                if (widthHist[w] > 0)
                    widths.append(qMakePair(w, widthHist[w]));
            }
            QList<int> largeWidths = widthHistLarge.keys();
            std::sort(largeWidths.begin(), largeWidths.end());
            for (int w : largeWidths)
                widths.append(qMakePair(w, widthHistLarge.value(w)));
            qint64 lines = 0;
            for (const auto &wc : widths)
                lines += wc.second;
            int medianWidth = maxCols;
            qint64 cumulative = 0;
            for (const auto &wc : widths) {
                cumulative += wc.second;
                if (2 * cumulative >= lines) {
                    medianWidth = wc.first;
                    break;
                }
            }
            const qint64 outlierWidth =
                    qint64(std::max(1, medianWidth)) * kAsciiImportOutlierFactor;
            qint64 outlierLines = 0;
            for (const auto &wc : widths) {
                if (wc.first > outlierWidth)
                    outlierLines += wc.second;
            }
            if (lines > 0 && maxCols > outlierWidth && outlierLines * 100 < lines) {
                refusal = QObject::tr("%1 line(s) of the file have far more fields (up to %2) "
                                      "than the other lines (typically %3), which would make a "
                                      "table of %4 rows x %2 columns. Check the column separator "
                                      "or remove those lines.")
                                  .arg(outlierLines)
                                  .arg(maxCols)
                                  .arg(medianWidth)
                                  .arg(dataRowCount);
            }
        }
        if (refusal.isEmpty() && cells > g_maxImportCells) {
            refusal = QObject::tr("The file is too large to import: %1 rows x %2 columns (at most "
                                  "%3 cells).")
                              .arg(dataRowCount)
                              .arg(maxCols)
                              .arg(g_maxImportCells);
        }
        if (!refusal.isEmpty()) {
            qWarning("ASCII import refused: %d columns x %d rows: %s", maxCols, dataRowCount,
                     qPrintable(refusal));
            g_lastImportReport.refusal = refusal;
            return nullptr;
        }
    }

    // 5. Stream directly into pre-allocated column buffers (0 intermediate Cell matrix!)
    std::vector<std::unique_ptr<QVector<qreal>>> numVecs(static_cast<size_t>(maxCols));
    QVector<IntervalAttribute<bool>> invalidAttrs(maxCols);
    QVector<int> runInvalidStart(maxCols, -1);
    QVector<bool> useText(maxCols, !convertToNumeric);
    QVector<bool> needsTextBackfill(maxCols, false);
    QVector<bool> anyQuoted(maxCols, false);
    QVector<bool> anyFiniteNumber(maxCols, false);
    QVector<int> validNumCount(maxCols, 0);
    QVector<int> strayNonNumCount(maxCols, 0);
    std::vector<std::unique_ptr<QStringList>> strVecs(static_cast<size_t>(maxCols));

    for (int c = 0; c < maxCols; ++c) {
        if (!useText[c]) {
            numVecs[static_cast<size_t>(c)].reset(
                    new QVector<qreal>(dataRowCount, std::numeric_limits<double>::quiet_NaN()));
        } else {
            strVecs[static_cast<size_t>(c)].reset(new QStringList());
            strVecs[static_cast<size_t>(c)]->reserve(dataRowCount);
        }
    }

    {
        int streamPos = dataStartPos;
        int r = 0;
        bool seenStreamData = allowAlphaAtDataStart;
        while (streamPos < len && r < dataRowCount) {
            bool skipRow = false;
            if (!readNextRecordSpans(buf, len, streamPos, normSep, whitespaceDelim, trimSpaces,
                                     simplifySpaces, rowSpans, skipRow)) {
                break;
            }
            if (skipRow || isBodyHashCommentRow(rowSpans, seenStreamData)) {
                if (!skipRow)
                    ++skippedNonBlankLines;
                continue;
            }
            if (!seenStreamData && rowHasRealNumericData(rowSpans))
                seenStreamData = true;

            const int recCols = rowSpans.size();
            for (int c = 0; c < maxCols; ++c) {
                if (useText[c]) {
                    if (!needsTextBackfill[c] && strVecs[static_cast<size_t>(c)]) {
                        strVecs[static_cast<size_t>(c)]->append(
                                (c < recCols) ? rowSpans[c].toQString(trimSpaces, simplifySpaces)
                                              : QString());
                    }
                    continue;
                }

                const bool hasCell = (c < recCols);
                const bool wasQuoted = hasCell ? rowSpans[c].quoted : false;
                if (wasQuoted)
                    anyQuoted[c] = true;

                const QChar *tPtr = nullptr;
                int tLen = 0;
                if (hasCell)
                    rowSpans[c].trimmedView(tPtr, tLen);

                if (!hasCell || isMissingNumericSpan(tPtr, tLen, wasQuoted)) {
                    if (runInvalidStart[c] < 0)
                        runInvalidStart[c] = r;
                    continue;
                }
                if (runInvalidStart[c] >= 0) {
                    invalidAttrs[c].setValue(Interval<int>(runInvalidStart[c], r - 1), true);
                    runInvalidStart[c] = -1;
                }

                double numVal = 0.0;
                if (tryParseLabDoubleSpan(tPtr, tLen, primaryLoc, useCustomLoc, numVal)) {
                    (*numVecs[static_cast<size_t>(c)])[r] = numVal;
                    anyFiniteNumber[c] = true;
                    ++validNumCount[c];
                } else {
                    ++strayNonNumCount[c];
                    const int maxRemainingNum = validNumCount[c] + (dataRowCount - r - 1);
                    if (maxRemainingNum < 5
                        || qint64(maxRemainingNum) < qint64(strayNonNumCount[c]) * 9) {
                        useText[c] = true;
                        needsTextBackfill[c] = true;
                        numVecs[static_cast<size_t>(c)].reset();
                    } else if (runInvalidStart[c] < 0) {
                        runInvalidStart[c] = r;
                    }
                }
            }
            ++r;
        }
        // Also count any '#' comment lines trailing after the final data row
        while (streamPos < len) {
            bool skipRow = false;
            if (!readNextRecordSpans(buf, len, streamPos, normSep, whitespaceDelim, trimSpaces,
                                     simplifySpaces, rowSpans, skipRow)) {
                break;
            }
            if (!skipRow && isBodyHashCommentRow(rowSpans, seenStreamData))
                ++skippedNonBlankLines;
        }
    }

    bool anyBackfill = false;
    for (int c = 0; c < maxCols; ++c) {
        if (!useText[c]) {
            const bool enoughForNumeric = (strayNonNumCount[c] == 0)
                    || (validNumCount[c] >= 5
                        && qint64(validNumCount[c]) * 10
                                >= qint64(validNumCount[c] + strayNonNumCount[c]) * 9);
            if (!enoughForNumeric) {
                useText[c] = true;
                needsTextBackfill[c] = true;
                numVecs[static_cast<size_t>(c)].reset();
                strayNonNumCount[c] = 0;
            } else {
                if (runInvalidStart[c] >= 0) {
                    invalidAttrs[c].setValue(Interval<int>(runInvalidStart[c], dataRowCount - 1),
                                             true);
                }
                if (anyQuoted[c] && !anyFiniteNumber[c]) {
                    useText[c] = true;
                    needsTextBackfill[c] = true;
                    numVecs[static_cast<size_t>(c)].reset();
                    strayNonNumCount[c] = 0;
                }
            }
        } else {
            strayNonNumCount[c] = 0;
        }
        if (needsTextBackfill[c]) {
            anyBackfill = true;
            strVecs[static_cast<size_t>(c)].reset(new QStringList());
            strVecs[static_cast<size_t>(c)]->reserve(dataRowCount);
        }
    }

    if (anyBackfill) {
        int streamPos = dataStartPos;
        int r = 0;
        bool seenBackfillData = allowAlphaAtDataStart;
        while (streamPos < len && r < dataRowCount) {
            bool skipRow = false;
            if (!readNextRecordSpans(buf, len, streamPos, normSep, whitespaceDelim, trimSpaces,
                                     simplifySpaces, rowSpans, skipRow)) {
                break;
            }
            if (skipRow || isBodyHashCommentRow(rowSpans, seenBackfillData))
                continue;
            if (!seenBackfillData && rowHasRealNumericData(rowSpans))
                seenBackfillData = true;
            const int recCols = rowSpans.size();
            for (int c = 0; c < maxCols; ++c) {
                if (!needsTextBackfill[c])
                    continue;
                strVecs[static_cast<size_t>(c)]->append(
                        (c < recCols) ? rowSpans[c].toQString(trimSpaces, simplifySpaces)
                                      : QString());
            }
            ++r;
        }
    }

    // 6. Build typed Columns with metadata & 'g' (6-digit) display formatting
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

        if (effectiveFirstRowNames && !headerRow.isEmpty()) {
            const QString rawHdr = (c < headerRow.size()) ? headerRow[c].value : QString();
            AsciiTableImportFilter::splitHeaderNameAndUnits(rawHdr, shortName, longName, units);
            if (longName.isEmpty() && !shortName.isEmpty()
                && (!unitsRow.isEmpty() || !commentsRow.isEmpty())) {
                longName = shortName;
            }
        } else if (!effectiveFirstRowNames) {
            if (isXrdAsciiExt) {
                if (c == 0) {
                    shortName = QStringLiteral("TwoTheta");
                    longName = QStringLiteral("2\u03B8");
                    units = QStringLiteral("deg");
                } else if (c == 1) {
                    shortName = QStringLiteral("Intensity");
                    longName = QStringLiteral("Intensity");
                    units = QStringLiteral("counts");
                } else if (c == 2) {
                    shortName = QStringLiteral("Error");
                    longName = QStringLiteral("\u03C3_I");
                    units = QStringLiteral("counts");
                } else {
                    shortName = QStringLiteral("Y%1").arg(c);
                }
            } else if (maxCols >= 2) {
                if (c == 0)
                    shortName = QStringLiteral("X");
                else if (c == 1)
                    shortName = QStringLiteral("Y");
                else
                    shortName = QStringLiteral("Y%1").arg(c);
            }
        }
        if (!unitsRow.isEmpty() && c < unitsRow.size()) {
            const QString u2 = cleanPromotedUnitCell(unitsRow[c].value);
            if (!u2.isEmpty()) {
                if (units.isEmpty())
                    units = u2;
            }
        }
        shortName = makeUniqueShort(shortName, c);

        Column *col = nullptr;
        if (useText[c]) {
            if (!strVecs[static_cast<size_t>(c)])
                strVecs[static_cast<size_t>(c)].reset(new QStringList());
            col = new Column(shortName, std::move(strVecs[static_cast<size_t>(c)]),
                             IntervalAttribute<bool>());
            colIsNumeric[c] = false;
        } else {
            col = new Column(shortName, std::move(numVecs[static_cast<size_t>(c)]),
                             invalidAttrs[c]);
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
        if (!commentsRow.isEmpty() && c < commentsRow.size()) {
            const QString cm = commentsRow[c].value.trimmed();
            if (!cm.isEmpty())
                col->setComment(cm);
        }

        cols << col;
    }

    // Assign sensible X / Y / yErr plot designations
    if (!effectiveFirstRowNames && isXrdAsciiExt && maxCols >= 3 && colIsNumeric[0]
        && colIsNumeric[1] && colIsNumeric[2]) {
        cols[0]->setPlotDesignation(SciDAVis::X);
        cols[1]->setPlotDesignation(SciDAVis::Y);
        cols[2]->setPlotDesignation(SciDAVis::yErr);
        for (int c = 3; c < maxCols; ++c)
            cols[c]->setPlotDesignation(SciDAVis::Y);
    } else if (maxCols >= 3 && !colIsNumeric[0] && colIsNumeric[1] && colIsNumeric[2]) {
        cols[0]->setPlotDesignation(SciDAVis::noDesignation);
        cols[1]->setPlotDesignation(SciDAVis::X);
        for (int c = 2; c < maxCols; ++c)
            cols[c]->setPlotDesignation(SciDAVis::Y);
    } else {
        for (int c = 0; c < maxCols; ++c)
            cols[c]->setPlotDesignation((c == 0) ? SciDAVis::X : SciDAVis::Y);
    }

    AsciiTableImportFilter::ImportReport &rep = g_lastImportReport;
    rep.separator = whitespaceDelim ? QStringLiteral(" ") : normSep;
    rep.skippedLines = skippedNonBlankLines - (prependedHashHeader ? 1 : 0);
    rep.headerLines = fileHeaderLinesConsumed + (prependedHashHeader ? 1 : 0);
    rep.unitsRowDetected = !unitsRow.isEmpty();
    rep.dataRows = dataRowCount;
    rep.columns = maxCols;
    for (int c = 0; c < maxCols; ++c) {
        if (!colIsNumeric[c]) {
            ++rep.textColumns;
            continue;
        }
        int invalidCount = 0;
        for (const Interval<int> &iv : invalidAttrs[c].intervals())
            invalidCount += iv.end() - iv.start() + 1;
        const int stray = strayNonNumCount[c];
        rep.nonNumericCells += stray;
        rep.missingCells += std::max(0, invalidCount - stray);
    }

    future::Table *result = new future::Table(0, 0, effTableName);
    result->appendColumns(cols);
    if (!rep.preamble.isEmpty())
        result->setComment(rep.preamble);
    return result;
}

} // namespace

void AsciiTableImportFilter::setDialogImportDefaults(const QString &encoding, int unitsRow)
{
    g_dialogEncoding = encoding;
    g_dialogUnitsRow = unitsRow;
}

QString AsciiTableImportFilter::dialogDefaultEncoding()
{
    return g_dialogEncoding;
}

int AsciiTableImportFilter::dialogDefaultUnitsRow()
{
    return g_dialogUnitsRow;
}

namespace {
// Length of the run of non-ASCII characters that contains each character (0 for ASCII).
QVector<int> nonAsciiRunLengths(const QString &s)
{
    QVector<int> run(s.size(), 0);
    int i = 0;
    while (i < s.size()) {
        if (s.at(i).unicode() < 0x80) {
            ++i;
            continue;
        }
        int j = i;
        while (j < s.size() && s.at(j).unicode() >= 0x80)
            ++j;
        for (int k = i; k < j; ++k)
            run[k] = j - i;
        i = j;
    }
    return run;
}

// How much a decoded text looks like Japanese (Shift-JIS / EUC-JP candidates). Kana, kanji next to
// other Japanese characters, full-width forms and JIS symbols count for it; a lone half-width
// katakana (what Latin-1 "°", "µ" become in Shift-JIS) and private-use characters count against.
int japaneseTextScore(const QString &s)
{
    const QVector<int> run = nonAsciiRunLengths(s);
    int score = 0;
    for (int i = 0; i < s.size(); ++i) {
        const ushort u = s.at(i).unicode();
        if (u < 0x80)
            continue;
        if ((u >= 0x3000 && u <= 0x30FF) || (u >= 0xFF01 && u <= 0xFF5E) || u == 0x2103
            || u == 0x203B || u == 0x00B0 || u == 0x00B1 || u == 0x00D7 || u == 0x00F7
            || (u >= 0x2460 && u <= 0x24FF) || (u >= 0x0391 && u <= 0x03C9)
            || (u >= 0x2190 && u <= 0x22FF) || (u >= 0x2500 && u <= 0x257F)) {
            score += 2;
        } else if (u >= 0x4E00 && u <= 0x9FFF) {
            score += (run[i] >= 2) ? 2 : 1;
        } else if (u >= 0xFF61 && u <= 0xFF9F) {
            score += (run[i] >= 2) ? 1 : -2;
        } else if ((u >= 0xE000 && u <= 0xFAFF) || u == 0xFFFD) {
            score -= 3;
        }
    }
    return score;
}

// How much a Windows-1252 decoded text looks like Western text: single accented letters next to
// ASCII letters and single unit symbols ("°", "µ", "±", "²") count for it; runs of 3+ high
// characters (what multi-byte Japanese becomes) and C1 control codes count against.
int westernTextScore(const QString &s)
{
    const QVector<int> run = nonAsciiRunLengths(s);
    static const QString kUnitSymbols = QStringLiteral("°µ±²³·‰€£§¹¼½¾×÷");
    static const QString kPunct = QStringLiteral("–—‘’“”…•©®™");
    auto isAsciiLower = [](QChar c) { return c.unicode() < 0x80 && c.isLower(); };
    auto isAsciiLetter = [](QChar c) { return c.unicode() < 0x80 && c.isLetter(); };
    int score = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        const ushort u = ch.unicode();
        if (u < 0x80)
            continue;
        if (u <= 0x9F || u == 0xFFFD) {
            score -= 3;
            continue;
        }
        if (run[i] >= 3) {
            score -= 1;
            continue;
        }
        if (run[i] == 2)
            continue;
        const QChar prev = (i > 0) ? s.at(i - 1) : QLatin1Char(' ');
        const QChar next = (i + 1 < s.size()) ? s.at(i + 1) : QLatin1Char(' ');
        if (kUnitSymbols.contains(ch)) {
            score += 2;
        } else if (kPunct.contains(ch)) {
            score += 1;
        } else if (ch.isLetter()) {
            if (isAsciiLower(prev) || isAsciiLower(next)
                || (ch.isUpper() && (isAsciiLetter(prev) || isAsciiLetter(next))))
                score += 2;
        }
    }
    return score;
}

QString decodeCountingInvalid(QTextCodec *codec, const QByteArray &rawBytes, int &outInvalid)
{
    QTextCodec::ConverterState state;
    QString s = codec->toUnicode(rawBytes.constData(), rawBytes.size(), &state);
    outInvalid = state.invalidChars;
    if (state.remainingChars > 0) {
        s.append(QChar::ReplacementCharacter);
        ++outInvalid;
    }
    return s;
}
} // namespace

QString AsciiTableImportFilter::decodeLabTextAuto(const QByteArray &rawBytes,
                                                  const QString &encodingHint)
{
    g_lastDecodeInvalidChars = 0;
    if (rawBytes.isEmpty())
        return QString();

    auto decodeWithCodec = [](QTextCodec *codec, const char *data, int size) -> QString {
        QTextCodec::ConverterState state;
        QString s = codec->toUnicode(data, size, &state);
        if (state.remainingChars > 0) {
            s.append(QChar::ReplacementCharacter);
            g_lastDecodeInvalidChars = state.invalidChars + 1;
        } else {
            g_lastDecodeInvalidChars = state.invalidChars;
        }
        if (!s.isEmpty() && s.at(0) == QChar(0xFEFF))
            s.remove(0, 1);
        return s;
    };

    // 1. Check explicit BOMs first
    if (rawBytes.size() >= 3 && static_cast<unsigned char>(rawBytes[0]) == 0xEF
        && static_cast<unsigned char>(rawBytes[1]) == 0xBB
        && static_cast<unsigned char>(rawBytes[2]) == 0xBF) {
        g_lastDecodedEncoding = QStringLiteral("UTF-8 (BOM)");
        if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8"))
            return decodeWithCodec(utf8, rawBytes.constData() + 3, rawBytes.size() - 3);
        return QString::fromUtf8(rawBytes.constData() + 3, rawBytes.size() - 3);
    }
    if (rawBytes.size() >= 2 && static_cast<unsigned char>(rawBytes[0]) == 0xFF
        && static_cast<unsigned char>(rawBytes[1]) == 0xFE) {
        g_lastDecodedEncoding = QStringLiteral("UTF-16LE (BOM)");
        if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16LE"))
            return decodeWithCodec(c16, rawBytes.constData() + 2, rawBytes.size() - 2);
    }
    if (rawBytes.size() >= 2 && static_cast<unsigned char>(rawBytes[0]) == 0xFE
        && static_cast<unsigned char>(rawBytes[1]) == 0xFF) {
        g_lastDecodedEncoding = QStringLiteral("UTF-16BE (BOM)");
        if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16BE"))
            return decodeWithCodec(c16, rawBytes.constData() + 2, rawBytes.size() - 2);
    }

    // 2. Honor explicit encodingHint when specified and not "auto"
    const QString hint = encodingHint.trimmed();
    if (!hint.isEmpty() && hint.compare(QLatin1String("auto"), Qt::CaseInsensitive) != 0) {
        g_lastDecodedEncoding = hint;
        const QString lo = hint.toLower();
        QByteArray codecName = hint.toLatin1();
        if (lo == QLatin1String("utf8") || lo == QLatin1String("utf-8")
            || lo == QLatin1String("utf-8-sig") || lo == QLatin1String("utf_8")) {
            if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8"))
                return decodeWithCodec(utf8, rawBytes.constData(), rawBytes.size());
            QString s = QString::fromUtf8(rawBytes);
            if (!s.isEmpty() && s.at(0) == QChar(0xFEFF))
                s.remove(0, 1);
            return s;
        }
        if (lo == QLatin1String("sjis") || lo == QLatin1String("shift_jis")
            || lo == QLatin1String("shift-jis") || lo == QLatin1String("cp932")
            || lo == QLatin1String("ms932") || lo == QLatin1String("windows-31j")) {
            QTextCodec *cp = QTextCodec::codecForName("Windows-31J");
            if (!cp)
                cp = QTextCodec::codecForName("CP932");
            if (!cp)
                cp = QTextCodec::codecForName("Shift-JIS");
            if (cp)
                return decodeWithCodec(cp, rawBytes.constData(), rawBytes.size());
            codecName = QByteArrayLiteral("Shift-JIS");
        } else if (lo == QLatin1String("latin1") || lo == QLatin1String("latin-1")
                   || lo == QLatin1String("iso-8859-1")) {
            return QString::fromLatin1(rawBytes);
        }
        if (QTextCodec *codec = QTextCodec::codecForName(codecName))
            return decodeWithCodec(codec, rawBytes.constData(), rawBytes.size());
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
    if (pureAscii) {
        g_lastDecodedEncoding = QStringLiteral("ASCII");
        return QString::fromLatin1(rawBytes);
    }

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
            g_lastDecodedEncoding = QStringLiteral("UTF-16LE");
            if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16LE"))
                return decodeWithCodec(c16, rawBytes.constData(), rawBytes.size());
        }
        if (evenZeros > samplePairs / 2 && oddZeros == 0) {
            g_lastDecodedEncoding = QStringLiteral("UTF-16BE");
            if (QTextCodec *c16 = QTextCodec::codecForName("UTF-16BE"))
                return decodeWithCodec(c16, rawBytes.constData(), rawBytes.size());
        }
    }

    // 5. UTF-8 / Shift-JIS decision. A single stray byte must neither silently flip a UTF-8
    //    file into Shift-JIS (mojibake) nor hide damage: compare the invalid-character counts
    //    of both candidates and report what was decoded with errors.
    QString utf8Decoded;
    int utf8Invalid = -1;
    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        QTextCodec::ConverterState state;
        utf8Decoded = utf8->toUnicode(rawBytes.constData(), rawBytes.size(), &state);
        if (state.remainingChars > 0) {
            utf8Decoded.append(QChar::ReplacementCharacter);
            utf8Invalid = state.invalidChars + 1;
        } else {
            utf8Invalid = state.invalidChars;
        }
        if (utf8Invalid == 0) {
            g_lastDecodedEncoding = QStringLiteral("UTF-8");
            if (!utf8Decoded.isEmpty() && utf8Decoded.at(0) == QChar(0xFEFF))
                utf8Decoded.remove(0, 1);
            return utf8Decoded;
        }
    }

    // 6. Shift-JIS / CP932 (Japanese lab instruments: Shimadzu, Rigaku, Keyence, JEOL), weighed
    //    against EUC-JP and Windows-1252 / Latin-1: a Latin-1 "°C" or "µm" is also valid
    //    Shift-JIS (half-width katakana), so validity alone does not decide. Shift-JIS stays the
    //    choice unless another candidate is clearly more plausible.
    QTextCodec *sjis = QTextCodec::codecForName("Windows-31J");
    if (!sjis)
        sjis = QTextCodec::codecForName("CP932");
    if (!sjis)
        sjis = QTextCodec::codecForName("Shift-JIS");
    if (sjis) {
        int sjisInvalid = 0;
        QString decoded = decodeCountingInvalid(sjis, rawBytes, sjisInvalid);
        const int sjisScore = (sjisInvalid == 0) ? japaneseTextScore(decoded) : 0;
        int eucInvalid = -1;
        QString eucDecoded;
        if (QTextCodec *euc = QTextCodec::codecForName("EUC-JP"))
            eucDecoded = decodeCountingInvalid(euc, rawBytes, eucInvalid);
        const int eucScore = (eucInvalid == 0) ? japaneseTextScore(eucDecoded) : 0;
        int westInvalid = -1;
        QString westDecoded;
        if (QTextCodec *w1252 = QTextCodec::codecForName("windows-1252"))
            westDecoded = decodeCountingInvalid(w1252, rawBytes, westInvalid);
        const int westScore = (westInvalid == 0) ? westernTextScore(westDecoded) : 0;
        // Mostly valid UTF-8 ("°C", "µm", "é" decoded) with a few damaged bytes, which Shift-JIS
        // reads worse ("Ü", "à" are not Shift-JIS): UTF-8 with a warning, before Windows-1252 /
        // EUC-JP, which read any byte and would turn every UTF-8 character into two
        // ("°C" -> "Â°C") without a warning. The Shift-JIS codec puts U+FFFD for a byte it cannot
        // read without counting it as invalid, so the replacement characters are counted here.
        int sjisBad = sjisInvalid;
        if (utf8Invalid > 0)
            sjisBad = std::max<int>(sjisBad, int(decoded.count(QChar::ReplacementCharacter)));
        if (utf8Invalid > 0 && utf8Invalid < sjisBad) {
            int utf8Chars = 0;
            for (const QChar ch : utf8Decoded) {
                if (ch.unicode() >= 0x80 && ch != QChar::ReplacementCharacter
                    && ch != QChar(0xFEFF))
                    ++utf8Chars;
            }
            if (utf8Chars > utf8Invalid) {
                g_lastDecodedEncoding = QStringLiteral("UTF-8");
                g_lastDecodeInvalidChars = utf8Invalid;
                if (!utf8Decoded.isEmpty() && utf8Decoded.at(0) == QChar(0xFEFF))
                    utf8Decoded.remove(0, 1);
                return utf8Decoded;
            }
        }
        const bool eucBetter = eucInvalid == 0 && eucScore > 0
                && (sjisInvalid > 0 || eucScore > sjisScore);
        const bool westBetter = westInvalid == 0 && westScore > 0
                && (sjisInvalid > 0 || westScore > sjisScore)
                && (!eucBetter || westScore > eucScore);
        if (westBetter) {
            g_lastDecodedEncoding = QStringLiteral("Windows-1252");
            g_lastDecodeInvalidChars = 0;
            return westDecoded;
        }
        if (eucBetter) {
            g_lastDecodedEncoding = QStringLiteral("EUC-JP");
            g_lastDecodeInvalidChars = 0;
            return eucDecoded;
        }
        if (sjisInvalid == 0) {
            g_lastDecodedEncoding = QStringLiteral("Shift-JIS");
            return decoded;
        }
        if (utf8Invalid >= 0 && utf8Invalid < sjisInvalid) {
            // Mostly valid UTF-8 with a few damaged bytes: keep UTF-8 and warn.
            g_lastDecodedEncoding = QStringLiteral("UTF-8");
            g_lastDecodeInvalidChars = utf8Invalid;
            if (!utf8Decoded.isEmpty() && utf8Decoded.at(0) == QChar(0xFEFF))
                utf8Decoded.remove(0, 1);
            return utf8Decoded;
        }
        g_lastDecodedEncoding = QStringLiteral("Shift-JIS");
        g_lastDecodeInvalidChars = sjisInvalid;
        return decoded;
    }
    g_lastDecodedEncoding = QStringLiteral("Latin-1");
    return QString::fromLatin1(rawBytes);
}

bool AsciiTableImportFilter::looksLikeUnitCell(const QString &cell, QString *outCleaned)
{
    const QString cleaned = cleanPromotedUnitCell(cell);
    if (outCleaned)
        *outCleaned = cleaned;
    return !cleaned.isEmpty() && isLikelyScientificUnit(cleaned);
}

bool AsciiTableImportFilter::isMissingDataToken(const QString &raw)
{
    return isMissingDataTokenImpl(raw);
}

bool AsciiTableImportFilter::parseNumberToken(const QString &trimmed, double &outValue)
{
    return tryParseAsciiDoubleSpan(trimmed.constData(), trimmed.size(), outValue);
}

bool AsciiTableImportFilter::isOverflowNumberToken(const QString &trimmed)
{
    return isOverflowNumberSpan(trimmed.constData(), trimmed.size());
}

QString AsciiTableImportFilter::detectSeparator(const QStringList &sampleLines,
                                                const QString &filePathHint)
{
    return detectDelimiterFromSampleLines(sampleLines, filePathHint);
}

bool AsciiTableImportFilter::looksLikeEuropeanDecimalComma(const QStringList &lines)
{
    return looksLikeEuropeanSemicolonLines(lines);
}

bool AsciiTableImportFilter::isHashCommentRow(const QStringList &cells, const QVector<bool> &quoted,
                                              bool whitespaceDelim, bool tableHasMultipleCols,
                                              bool allowAlphaHashIds, const QLocale &numericLocale)
{
    QLocale primaryLoc = numericLocale;
    primaryLoc.setNumberOptions(QLocale::RejectGroupSeparator);
    const bool useCustomLocale = (numericLocale != QLocale::c());
    // trimmed copies stay alive for the whole classification (the getter hands out pointers)
    QStringList trimmedCells;
    trimmedCells.reserve(cells.size());
    for (const QString &c : cells)
        trimmedCells.append(c.trimmed());
    return isHashCommentRecordCells(trimmedCells.size(), whitespaceDelim, tableHasMultipleCols,
                                    allowAlphaHashIds, primaryLoc, useCustomLocale,
                                    [&](int k, const QChar *&pk, int &nk, bool &kQuoted) {
                                        if (k >= 0 && k < trimmedCells.size()) {
                                            pk = trimmedCells.at(k).constData();
                                            nk = trimmedCells.at(k).size();
                                            kQuoted = quoted.value(k, false);
                                        } else {
                                            pk = nullptr;
                                            nk = 0;
                                            kQuoted = false;
                                        }
                                    });
}

future::Table *AsciiTableImportFilter::importSpecTextData(const QString &decodedText,
                                                          const QString &tableName,
                                                          bool convertToNumeric,
                                                          bool firstRowNamesColumns)
{
    return tryParseSpecTextInternal(decodedText, tableName, convertToNumeric, QLocale::c(),
                                    firstRowNamesColumns);
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
                                     QLocale::c(), true /*autoFixTabOnCsv*/,
                                     true /*detectPreamble*/);
}

future::Table *AsciiTableImportFilter::importVaspTextData(const QByteArray &rawBytes,
                                                          const QString &tableName,
                                                          const QString &filePathHint,
                                                          double *outFermiEV)
{
    if (outFermiEV)
        *outFermiEV = 0.0;
    const QString effTableName =
            tableName.trimmed().isEmpty() ? QStringLiteral("VASPData") : tableName.trimmed();
    const QString decoded = decodeLabTextAuto(rawBytes, QString());
    return tryParseVaspTextInternal(decoded, effTableName, filePathHint, outFermiEV);
}

AsciiTableImportFilter::ImportReport AsciiTableImportFilter::lastImportReport()
{
    return g_lastImportReport;
}

void AsciiTableImportFilter::setImportSizeLimitsForTesting(int maxColumns, qint64 outlierCells,
                                                           qint64 maxCells)
{
    g_maxImportColumns = maxColumns > 0 ? maxColumns : kMaxAsciiImportColumns;
    g_importOutlierCells = outlierCells > 0 ? outlierCells : kAsciiImportOutlierCells;
    g_maxImportCells = maxCells > 0 ? maxCells : kMaxAsciiImportCells;
}

QString AsciiTableImportFilter::describeImportReport(const ImportReport &r)
{
    registerImportJapaneseStrings();
    QStringList parts;
    if (r.rasBlocks > 1)
        parts << QObject::tr("%1 scans").arg(r.rasBlocks);
    parts << QObject::tr("%1 rows x %2 columns").arg(r.dataRows).arg(r.columns);
    if (!r.encoding.isEmpty())
        parts << QObject::tr("encoding: %1").arg(r.encoding);
    if (r.invalidChars > 0)
        parts << QObject::tr("%n undecodable character(s) - the text encoding may be wrong", "", r.invalidChars);
    if (r.rasBlocks == 0) {
        QString sepName;
        if (r.separator == QLatin1String("\t"))
            sepName = QObject::tr("tab");
        else if (r.separator == QLatin1String(" "))
            sepName = QObject::tr("spaces");
        else
            sepName = r.separator;
        if (!sepName.isEmpty())
            parts << QObject::tr("separator: %1").arg(sepName);
        parts << (r.unitsRowDetected ? QObject::tr("unit row: yes") : QObject::tr("unit row: no"));
    }
    if (r.skippedLines > 0)
        parts << QObject::tr("%n line(s) skipped", "", r.skippedLines);
    if (r.nonNumericCells > 0)
        parts << QObject::tr("%n non-numeric cell(s) marked invalid", "", r.nonNumericCells);
    if (r.missingCells > 0)
        parts << QObject::tr("%n empty cell(s)", "", r.missingCells);
    if (r.textColumns > 0)
        parts << QObject::tr("%n column(s) imported as text", "", r.textColumns);
    return parts.join(QStringLiteral(", "));
}

future::Table *AsciiTableImportFilter::importDroppedTextData(const QByteArray &rawBytes,
                                                             const QString &tableName,
                                                             const QString &filePathHint,
                                                             ImportReport *outReport)
{
    registerImportJapaneseStrings();
    future::Table *t = importLabTextDataInternal(
            rawBytes, tableName, filePathHint, QString() /*auto separator*/,
            QString() /*auto encoding*/, -1 /*auto units row*/, 0 /*ignoredLines*/,
            true /*firstRowNamesColumns*/, true /*trimSpaces*/, false /*simplifySpaces*/,
            true /*convertToNumeric*/, QLocale::c(), true /*autoFixTabOnCsv*/,
            true /*detectPreamble*/);
    if (outReport)
        *outReport = g_lastImportReport;
    return t;
}

bool AsciiTableImportFilter::hasDataPreamble(const QByteArray &rawBytes,
                                             const QString &filePathHint,
                                             const QString &encodingHint)
{
    // Only the head of the file (1000 lines, 128 KiB): a preamble with its header and the first
    // data rows fits there, and a large or very wide file is not read twice.
    // UTF-16 text is not cut (a '\n' byte is not a line end there).
    QByteArray head = rawBytes;
    if (!rawBytes.contains('\0')) {
        int lines = 0;
        int cut = -1;
        const int limit = std::min(rawBytes.size(), 128 * 1024);
        for (int i = 0; i < limit; ++i) {
            if (rawBytes.at(i) == '\n' && ++lines >= 1000) {
                cut = i + 1;
                break;
            }
        }
        if (cut < 0 && rawBytes.size() > limit) {
            cut = rawBytes.lastIndexOf('\n', limit - 1) + 1;
            if (cut <= 0)
                cut = limit;
        }
        if (cut > 0)
            head = rawBytes.left(cut);
    }
    const ImportReport savedReport = g_lastImportReport;
    const QString savedEncoding = g_lastDecodedEncoding;
    const int savedInvalid = g_lastDecodeInvalidChars;
    std::unique_ptr<future::Table> probe(importLabTextDataInternal(
            head, QStringLiteral("PreambleProbe"), filePathHint, QString() /*auto separator*/,
            encodingHint, -1 /*auto units row*/, 0 /*ignoredLines*/,
            true /*firstRowNamesColumns*/, true /*trimSpaces*/, false /*simplifySpaces*/,
            true /*convertToNumeric*/, QLocale::c(), true /*autoFixTabOnCsv*/,
            true /*detectPreamble*/, true /*preambleProbeOnly*/));
    // (a table comes back only from the special formats -- .ras, VASP, SPEC -- which have no
    // preamble in this sense)
    // Rows of numbers above the block that are data rows the block search skipped (a table whose
    // rows get wider further down, blocks separated by blank lines) leave no preamble: the probe
    // runs the same decision as the import (importLabTextDataInternal).
    const bool found = !probe && !g_lastImportReport.preamble.isEmpty();
    g_lastImportReport = savedReport;
    g_lastDecodedEncoding = savedEncoding;
    g_lastDecodeInvalidChars = savedInvalid;
    return found;
}

AbstractAspect *AsciiTableImportFilter::importAspect(QIODevice &input)
{
    registerImportJapaneseStrings();
    QString filePathHint = d_file_path_hint;
    if (filePathHint.isEmpty()) {
        if (const QFile *qf = qobject_cast<const QFile *>(&input))
            filePathHint = qf->fileName();
    }

    const QByteArray rawBytes = input.readAll();
    const bool trimEffective = d_trim_whitespace || d_simplify_whitespace || d_convert_to_numeric;

    return importLabTextDataInternal(rawBytes, tr("Table"), filePathHint, d_separator, d_encoding,
                                     d_units_row, d_ignored_lines,
                                     d_first_row_names_columns, trimEffective,
                                     d_simplify_whitespace, d_convert_to_numeric, d_numeric_locale,
                                     true /*autoFixTabOnCsv*/,
                                     d_ignored_lines == 0 /*detectPreamble*/);
}
