/***************************************************************************
    File                 : ImportOPJU.cpp
    Project              : ToriGraph / SciDAVis
    Description          : Native Origin 2018+ Unicode ZIP-based (.opju) importer
 ***************************************************************************/

#include "ImportOPJU.h"

#include "ApplicationWindow.h"
#include "ColorButton.h"
#include "Folder.h"
#include "Graph.h"
#include "Grid.h"
#include "I18nJapanese.h"
#include "Matrix.h"
#include "MultiLayer.h"
#include "Note.h"
#include "QwtBarCurve.h"
#include "QwtHistogram.h"
#include "Table.h"
#include "ZipReader.h"
#include "future/core/column/Column.h"
#include "future/core/datatypes/DateTime2StringFilter.h"
#include "future/core/datatypes/Double2StringFilter.h"

#include <qwt_plot_curve.h>
#include <qwt_symbol.h>

#include <zlib.h>

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMessageBox>
#include <QPen>
#include <QRegularExpression>
#include <QSet>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTextCodec>
#include <QVector>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>

namespace {

void registerOpjuTranslations()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        auto reg = [](const char *en, const char *ja) {
            ToriGraphFullJapaneseTranslator::registerExtraTranslation(QString::fromUtf8(en),
                                                                      QString::fromUtf8(ja));
        };
        reg("Origin .opju Import", "Origin .opju の読み込み");
        reg("%1 graph window(s) were skipped (unsupported binary CPYUA/graph layout)",
            "%1 件のグラフウィンドウは形式上の制約によりスキップされました");
        reg("%1 matrix window(s) were skipped",
            "%1 件の行列ウィンドウはスキップされました");
        reg("%1 dataset(s) could not be decoded and were skipped",
            "%1 件のデータセットはデコードできずスキップされました");
    });
}

using torizip::inflateBuffer;
using torizip::kMaxArchiveBytes;
using torizip::kMaxEntryUncompressedBytes;
using torizip::ZipArchive;

constexpr int kObjectXOffset = 200;

// ---------------------------------------------------------------------------
// Little-endian binary readers with strict bounds checking
// ---------------------------------------------------------------------------
inline bool readU16LE(const QByteArray &buf, qint64 pos, quint16 &out)
{
    if (pos < 0 || pos + 2 > buf.size())
        return false;
    const auto *p = reinterpret_cast<const unsigned char *>(buf.constData() + pos);
    out = static_cast<quint16>(p[0]) | (static_cast<quint16>(p[1]) << 8);
    return true;
}

inline bool readU32LE(const QByteArray &buf, qint64 pos, quint32 &out)
{
    if (pos < 0 || pos + 4 > buf.size())
        return false;
    const auto *p = reinterpret_cast<const unsigned char *>(buf.constData() + pos);
    out = static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
            | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24);
    return true;
}

inline bool readU64LE(const QByteArray &buf, qint64 pos, quint64 &out)
{
    quint32 lo = 0, hi = 0;
    if (!readU32LE(buf, pos, lo) || !readU32LE(buf, pos + 4, hi))
        return false;
    out = static_cast<quint64>(lo) | (static_cast<quint64>(hi) << 32);
    return true;
}

inline bool readF32LE(const QByteArray &buf, qint64 pos, float &out)
{
    quint32 bits = 0;
    if (!readU32LE(buf, pos, bits))
        return false;
    std::memcpy(&out, &bits, sizeof(float));
    return true;
}

inline bool readF64LE(const QByteArray &buf, qint64 pos, double &out)
{
    quint64 bits = 0;
    if (!readU64LE(buf, pos, bits))
        return false;
    std::memcpy(&out, &bits, sizeof(double));
    return true;
}

// ---------------------------------------------------------------------------
// Origin missing-value sentinel detection & string decoding
// ---------------------------------------------------------------------------
inline bool isOriginMissingDouble(double v)
{
    if (!std::isfinite(v))
        return true;
    // Origin NANUM / _ONAN sentinels:
    //   -1.23456789e-300 (liborigin _ONAN / Origin C NANUM)
    //   <= -1.0e300 (large negative sentinel used in some Origin builds)
    if (v <= -1.0e300)
        return true;
    if (std::fabs(v - (-1.23456789e-300)) <= 1.0e-305)
        return true;
    return false;
}

inline bool isOriginMissingNumericText(const QString &s)
{
    const QString t = s.trimmed();
    if (t.isEmpty() || t == QLatin1String("--") || t == QLatin1String("-"))
        return true;
    if (t.compare(QLatin1String("NANUM"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("NaN"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("Inf"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("-Inf"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("null"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    return false;
}

QString decodeTextBytes(const QByteArray &bytes, const QString &encodingHint = QString())
{
    if (bytes.isEmpty())
        return QString();

    const QString hint = encodingHint.trimmed().toLower();
    if (hint == QLatin1String("utf-16") || hint == QLatin1String("utf-16le")
        || hint == QLatin1String("utf16") || hint == QLatin1String("utf16le")) {
        int start = 0;
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF
            && static_cast<unsigned char>(bytes[1]) == 0xFE) {
            start = 2;
        }
        const int u16Len = (bytes.size() - start) / 2;
        return QString::fromUtf16(reinterpret_cast<const ushort *>(bytes.constData() + start),
                                  u16Len);
    }

    // BOM detection
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF
        && static_cast<unsigned char>(bytes[1]) == 0xBB
        && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return QString::fromUtf8(bytes.constData() + 3, bytes.size() - 3);
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF
        && static_cast<unsigned char>(bytes[1]) == 0xFE) {
        return QString::fromUtf16(reinterpret_cast<const ushort *>(bytes.constData() + 2),
                                  (bytes.size() - 2) / 2);
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE
        && static_cast<unsigned char>(bytes[1]) == 0xFF) {
        if (QTextCodec *be = QTextCodec::codecForName("UTF-16BE"))
            return be->toUnicode(bytes.constData() + 2, bytes.size() - 2);
    }

    if (!hint.isEmpty()) {
        if (QTextCodec *c = QTextCodec::codecForName(hint.toUtf8()))
            return c->toUnicode(bytes);
    }

    bool pureAscii = true;
    for (char ch : bytes) {
        if (static_cast<unsigned char>(ch) >= 0x80) {
            pureAscii = false;
            break;
        }
    }
    if (pureAscii)
        return QString::fromLatin1(bytes);

    if (QTextCodec *utf8 = QTextCodec::codecForName("UTF-8")) {
        QTextCodec::ConverterState state;
        const QString s = utf8->toUnicode(bytes.constData(), bytes.size(), &state);
        if (state.invalidChars == 0)
            return s;
    }
    if (QTextCodec *sjis = QTextCodec::codecForName("Shift-JIS")) {
        QTextCodec::ConverterState state;
        const QString s = sjis->toUnicode(bytes.constData(), bytes.size(), &state);
        if (state.invalidChars == 0)
            return s;
    }
    return QString::fromLocal8Bit(bytes);
}

bool maybeDecompressInnerStream(const QByteArray &raw, bool forceZlib, QByteArray &out,
                                QString *outErr)
{
    if (raw.size() < 2) {
        out = raw;
        return !forceZlib;
    }
    const unsigned char b0 = static_cast<unsigned char>(raw[0]);
    const unsigned char b1 = static_cast<unsigned char>(raw[1]);
    const bool looksGzip = (b0 == 0x1F && b1 == 0x8B);
    const bool looksZlib = (b0 == 0x78 && (b1 == 0x01 || b1 == 0x5E || b1 == 0x9C || b1 == 0xDA));

    if (looksGzip || looksZlib || forceZlib) {
        QByteArray inflated;
        QString err;
        // MAX_WBITS + 32 enables automatic zlib/gzip header detection.
        if (inflateBuffer(raw.constData(), static_cast<quint64>(raw.size()), MAX_WBITS + 32,
                          kMaxEntryUncompressedBytes, inflated, &err)) {
            out = inflated;
            return true;
        }
        if (forceZlib) {
            // Try raw deflate fallback if explicitly marked compressed
            if (inflateBuffer(raw.constData(), static_cast<quint64>(raw.size()), -MAX_WBITS,
                              kMaxEntryUncompressedBytes, inflated, &err)) {
                out = inflated;
                return true;
            }
            if (outErr)
                *outErr = err;
            return false;
        }
        // Auto-detect false positive (raw binary bytes happened to start with 0x78 0x9C): keep raw
    }

    out = raw;
    return true;
}

// ---------------------------------------------------------------------------
// Intermediate Representation for parsed OPJU project
// ---------------------------------------------------------------------------
enum class OpjuDataType {
    Auto,
    Double,
    Float,
    Int32,
    UInt32,
    Int16,
    UInt16,
    Int8,
    UInt8,
    Text,
    TextNumeric,
    Date,
    Time,
    DateTime
};

struct OpjuColumn
{
    QString shortName;
    QString longName;
    QString units;
    QString comment;
    QString formula;
    SciDAVis::PlotDesignation designation = SciDAVis::Y;
    OpjuDataType dataType = OpjuDataType::Auto;
    int widthPixels = 0;
    int decimalPlaces = -1;
    int numericFormat = 0; // 0=auto('g'), 1=decimal('f'), 2=scientific('e')

    // External or inline binary/text data descriptor
    QString dataRef;
    QString baseDir;
    qint64 dataOffset = 0;
    qint64 dataLength = -1;
    int rowCountHint = -1;
    int stringStride = 0;
    bool compressed = false;
    QString encoding;
    QString stringFormat; // "", "nullTerminated", "lengthPrefixed", "fixed"

    // Inline parsed values
    QVector<double> numericValues;
    QVector<QString> textValues;
    bool hasNumeric = false;
    bool hasText = false;
    QByteArray inlineBinary;
};

struct OpjuWorksheet
{
    QString workbookName;
    QString sheetName;
    QString resolvedName;
    QString label;
    QString folderPath;
    bool hidden = false;
    QVector<OpjuColumn> columns;
};

struct OpjuMatrix
{
    QString name;
    QString sheetName;
    QString label;
    QString folderPath;
    QString formula;
    bool hidden = false;
    int rows = 0;
    int cols = 0;
    bool hasCoords = false;
    double xStart = 1.0, xEnd = 10.0, yStart = 1.0, yEnd = 10.0;
    OpjuDataType dataType = OpjuDataType::Double;
    QString dataRef;
    QString baseDir;
    qint64 dataOffset = 0;
    qint64 dataLength = -1;
    bool compressed = false;
    QVector<double> values;
};

struct OpjuNote
{
    QString name;
    QString label;
    QString folderPath;
    QString text;
    QString dataRef;
    QString baseDir;
    bool hidden = false;
};

struct OpjuCurve
{
    QString tableName;
    QString sheetName;
    QString xColName;
    QString yColName;
    QString errColName;
    int plotStyle = Graph::Line;
    QColor lineColor;
    double lineWidth = 1.5;
    int lineStyle = 0; // 0=Solid, 1=Dash, 2=Dot, 3=DashDot, 4=DashDotDot
    int connectType = QwtPlotCurve::Lines;
    bool hasExplicitConnect = false;
    int symbolShape = 0; // SciDAVis SymbolBox index (0=NoSymbol, 1=Ellipse, 2=Rect, ...)
    double symbolSize = 7.0;
    QColor symbolColor;
    QColor symbolFillColor;
    bool symbolFilled = true;
    bool fillArea = false;
    QColor fillColor;
    QString legendText;
};

struct OpjuAxis
{
    QString title;
    bool hasRange = false;
    double minVal = 0.0;
    double maxVal = 1.0;
    double step = 0.0;
    int majorTicks = 5;
    int minorTicks = 5;
    int scaleType = 0; // 0=Linear, 1=Log10
    bool inverted = false;
    bool showMajorGrid = false;
    bool showMinorGrid = false;
    bool hidden = false;
    bool showLabels = true;
    bool configured = false;
};

struct OpjuLayer
{
    QString title;
    QVector<OpjuCurve> curves;
    OpjuAxis xAxis;
    OpjuAxis yAxis;
    OpjuAxis x2Axis;
    OpjuAxis y2Axis;
    QString legendText;
    QStringList annotations;
};

struct OpjuGraph
{
    QString name;
    QString label;
    QString folderPath;
    bool hidden = false;
    QVector<OpjuLayer> layers;
};

struct OpjuFolderNode
{
    QString name;
    QString fullPath;
    QVector<OpjuFolderNode> children;
    QVector<QPair<QString, QString>> windowRefs; // (type, name)
};

struct OpjuProject
{
    QString rootFolderName;
    QVector<OpjuFolderNode> rootSubfolders;
    QVector<OpjuWorksheet> worksheets;
    QVector<OpjuMatrix> matrices;
    QVector<OpjuNote> notes;
    QVector<OpjuGraph> graphs;
    int skippedDatasets = 0;
    int skippedGraphs = 0;
    int skippedMatrices = 0;
    bool isCpyuaContainer = false;
};

// ---------------------------------------------------------------------------
// Parsing helpers for attributes, colors, enums, and Origin text tags
// ---------------------------------------------------------------------------
QString attrCI(const QXmlStreamAttributes &attrs, std::initializer_list<const char *> names)
{
    for (const char *n : names) {
        const QLatin1String target(n);
        for (const QXmlStreamAttribute &a : attrs) {
            if (a.name().compare(target, Qt::CaseInsensitive) == 0)
                return a.value().toString();
        }
    }
    return QString();
}

bool parseBoolAttr(const QString &s, bool defaultVal = false)
{
    const QString t = s.trimmed().toLower();
    if (t.isEmpty())
        return defaultVal;
    return (t == QLatin1String("1") || t == QLatin1String("true") || t == QLatin1String("yes")
            || t == QLatin1String("on"));
}

OpjuDataType parseDataType(const QString &raw)
{
    const QString t = raw.trimmed().toLower();
    if (t.isEmpty())
        return OpjuDataType::Auto;
    if (t == QLatin1String("double") || t == QLatin1String("float64") || t == QLatin1String("f64")
        || t == QLatin1String("numeric") || t == QLatin1String("real") || t == QLatin1String("0"))
        return OpjuDataType::Double;
    if (t == QLatin1String("float") || t == QLatin1String("float32") || t == QLatin1String("f32")
        || t == QLatin1String("single"))
        return OpjuDataType::Float;
    if (t == QLatin1String("int32") || t == QLatin1String("int") || t == QLatin1String("i32")
        || t == QLatin1String("long"))
        return OpjuDataType::Int32;
    if (t == QLatin1String("uint32") || t == QLatin1String("uint") || t == QLatin1String("u32")
        || t == QLatin1String("ulong"))
        return OpjuDataType::UInt32;
    if (t == QLatin1String("int16") || t == QLatin1String("short") || t == QLatin1String("i16"))
        return OpjuDataType::Int16;
    if (t == QLatin1String("uint16") || t == QLatin1String("ushort") || t == QLatin1String("u16"))
        return OpjuDataType::UInt16;
    if (t == QLatin1String("int8") || t == QLatin1String("i8") || t == QLatin1String("sbyte"))
        return OpjuDataType::Int8;
    if (t == QLatin1String("uint8") || t == QLatin1String("u8") || t == QLatin1String("byte"))
        return OpjuDataType::UInt8;
    if (t == QLatin1String("text") || t == QLatin1String("string") || t == QLatin1String("str")
        || t == QLatin1String("utf8") || t == QLatin1String("utf-8")
        || t == QLatin1String("utf16") || t == QLatin1String("utf-16")
        || t == QLatin1String("utf16le") || t == QLatin1String("utf-16le")
        || t == QLatin1String("1"))
        return OpjuDataType::Text;
    if (t == QLatin1String("textnumeric") || t == QLatin1String("text_numeric")
        || t == QLatin1String("mixed") || t == QLatin1String("9"))
        return OpjuDataType::TextNumeric;
    if (t == QLatin1String("date") || t == QLatin1String("2"))
        return OpjuDataType::Date;
    if (t == QLatin1String("time") || t == QLatin1String("3"))
        return OpjuDataType::Time;
    if (t == QLatin1String("datetime"))
        return OpjuDataType::DateTime;
    return OpjuDataType::Auto;
}

SciDAVis::PlotDesignation parsePlotDesignation(const QString &raw, int colIndex)
{
    const QString t = raw.trimmed().toLower();
    if (t.isEmpty())
        return (colIndex == 0) ? SciDAVis::X : SciDAVis::Y;
    if (t == QLatin1String("x") || t == QLatin1String("3"))
        return SciDAVis::X;
    if (t == QLatin1String("y") || t == QLatin1String("0"))
        return SciDAVis::Y;
    if (t == QLatin1String("z") || t == QLatin1String("5"))
        return SciDAVis::Z;
    if (t == QLatin1String("xerr") || t == QLatin1String("xerror") || t == QLatin1String("x_err")
        || t == QLatin1String("6"))
        return SciDAVis::xErr;
    if (t == QLatin1String("yerr") || t == QLatin1String("yerror") || t == QLatin1String("y_err")
        || t == QLatin1String("err") || t == QLatin1String("2"))
        return SciDAVis::yErr;
    if (t == QLatin1String("label") || t == QLatin1String("l") || t == QLatin1String("none")
        || t == QLatin1String("n") || t == QLatin1String("disregard") || t == QLatin1String("1")
        || t == QLatin1String("4"))
        return SciDAVis::noDesignation;
    return SciDAVis::Y;
}

QColor parseColor(const QString &raw)
{
    const QString t = raw.trimmed();
    if (t.isEmpty() || t.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("automatic"), Qt::CaseInsensitive) == 0)
        return QColor();

    // rgb(r,g,b) or rgba(r,g,b,a)
    static const QRegularExpression kRgbRe(
            QStringLiteral("^rgba?\\s*\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)(?:\\s*,\\s*(\\d+))?\\s*\\)$"),
            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = kRgbRe.match(t);
    if (m.hasMatch()) {
        const int r = std::clamp(m.captured(1).toInt(), 0, 255);
        const int g = std::clamp(m.captured(2).toInt(), 0, 255);
        const int b = std::clamp(m.captured(3).toInt(), 0, 255);
        const int a = m.captured(4).isEmpty() ? 255 : std::clamp(m.captured(4).toInt(), 0, 255);
        return QColor(r, g, b, a);
    }

    // Numeric Origin palette index (0..23) or packed 0x00BBGGRR
    bool ok = false;
    const int idx = t.toInt(&ok);
    if (ok && idx >= 0) {
        if (idx < static_cast<int>(ColorButton::colors_count))
            return ColorButton::color(static_cast<unsigned int>(idx));
        return QColor(idx & 0xFF, (idx >> 8) & 0xFF, (idx >> 16) & 0xFF);
    }

    QColor c(t);
    if (c.isValid())
        return c;
    return QColor();
}

int parseLineStyle(const QString &raw)
{
    const QString t = raw.trimmed().toLower();
    if (t == QLatin1String("dash") || t == QLatin1String("dashed") || t == QLatin1String("shortdash")
        || t == QLatin1String("1"))
        return 1;
    if (t == QLatin1String("dot") || t == QLatin1String("dotted") || t == QLatin1String("shortdot")
        || t == QLatin1String("2"))
        return 2;
    if (t == QLatin1String("dashdot") || t == QLatin1String("shortdashdot")
        || t == QLatin1String("3"))
        return 3;
    if (t == QLatin1String("dashdotdot") || t == QLatin1String("4"))
        return 4;
    return 0; // Solid
}

int parsePlotStyle(const QString &raw)
{
    const QString t = raw.trimmed().toLower();
    if (t == QLatin1String("scatter") || t == QLatin1String("symbol")
        || t == QLatin1String("symbols") || t == QLatin1String("dot") || t == QLatin1String("201"))
        return Graph::Scatter;
    if (t == QLatin1String("linesymbol") || t == QLatin1String("linesymbols")
        || t == QLatin1String("line+symbol") || t == QLatin1String("202"))
        return Graph::LineSymbols;
    if (t == QLatin1String("area") || t == QLatin1String("areastack") || t == QLatin1String("204"))
        return Graph::Area;
    if (t == QLatin1String("column") || t == QLatin1String("bar")
        || t == QLatin1String("verticalbars") || t == QLatin1String("vbar")
        || t == QLatin1String("203"))
        return Graph::VerticalBars;
    if (t == QLatin1String("hbar") || t == QLatin1String("horizontalbars")
        || t == QLatin1String("215"))
        return Graph::HorizontalBars;
    if (t == QLatin1String("histogram") || t == QLatin1String("hist") || t == QLatin1String("219"))
        return Graph::Histogram;
    if (t == QLatin1String("box") || t == QLatin1String("boxplot"))
        return Graph::Box;
    if (t == QLatin1String("pie"))
        return Graph::Pie;
    if (t == QLatin1String("errorbar") || t == QLatin1String("errorbars")
        || t == QLatin1String("yerr") || t == QLatin1String("xerr"))
        return Graph::ErrorBars;
    return Graph::Line;
}

int parseSymbolShape(const QString &raw)
{
    const QString t = raw.trimmed().toLower();
    if (t.isEmpty() || t == QLatin1String("none") || t == QLatin1String("0"))
        return 0;
    if (t == QLatin1String("circle") || t == QLatin1String("ellipse")
        || t == QLatin1String("sphere") || t == QLatin1String("2"))
        return 1; // Ellipse
    if (t == QLatin1String("square") || t == QLatin1String("rect")
        || t == QLatin1String("rectangle") || t == QLatin1String("1"))
        return 2; // Rect
    if (t == QLatin1String("diamond") || t == QLatin1String("5"))
        return 3; // Diamond
    if (t == QLatin1String("dtriangle") || t == QLatin1String("downtriangle")
        || t == QLatin1String("4"))
        return 5; // DTriangle
    if (t == QLatin1String("utriangle") || t == QLatin1String("triangle")
        || t == QLatin1String("uptriangle") || t == QLatin1String("3"))
        return 6; // UTriangle
    if (t == QLatin1String("ltriangle") || t == QLatin1String("lefttriangle")
        || t == QLatin1String("15"))
        return 7;
    if (t == QLatin1String("rtriangle") || t == QLatin1String("righttriangle")
        || t == QLatin1String("16"))
        return 8;
    if (t == QLatin1String("cross") || t == QLatin1String("plus") || t == QLatin1String("+")
        || t == QLatin1String("6"))
        return 9;
    if (t == QLatin1String("xcross") || t == QLatin1String("x") || t == QLatin1String("7"))
        return 10;
    if (t == QLatin1String("hline") || t == QLatin1String("horizontal") || t == QLatin1String("9"))
        return 11;
    if (t == QLatin1String("vline") || t == QLatin1String("vertical") || t == QLatin1String("10"))
        return 12;
    if (t == QLatin1String("star") || t == QLatin1String("18"))
        return 14;
    if (t == QLatin1String("hexagon") || t == QLatin1String("pentagon") || t == QLatin1String("17"))
        return 15;
    return 1; // default circle
}

int parseScaleType(const QString &raw)
{
    const QString t = raw.trimmed().toLower();
    if (t == QLatin1String("log") || t == QLatin1String("log10") || t == QLatin1String("logarithmic")
        || t == QLatin1String("1"))
        return 1;
    return 0; // Linear
}

QString parseOriginRichText(const QString &raw)
{
    if (raw.isEmpty())
        return raw;
    QString out = raw;
    // Convert \l(k) curve symbol tags -> \c{k}
    static const QRegularExpression kLineTag(QStringLiteral("\\\\\\s*l\\s*\\(\\s*(\\d+)\\s*\\)"));
    out.replace(kLineTag, QStringLiteral("\\c{\\1}"));
    // Convert basic Origin inline formatting tags: \b(...), \i(...), \u(...), \+(...), \-(...)
    static const QRegularExpression kBold(QStringLiteral("\\\\b\\(([^\\(\\)]*)\\)"));
    static const QRegularExpression kItalic(QStringLiteral("\\\\i\\(([^\\(\\)]*)\\)"));
    static const QRegularExpression kUnder(QStringLiteral("\\\\u\\(([^\\(\\)]*)\\)"));
    static const QRegularExpression kSup(QStringLiteral("\\\\\\+\\(([^\\(\\)]*)\\)"));
    static const QRegularExpression kSub(QStringLiteral("\\\\\\-\\(([^\\(\\)]*)\\)"));
    out.replace(kBold, QStringLiteral("<b>\\1</b>"));
    out.replace(kItalic, QStringLiteral("<i>\\1</i>"));
    out.replace(kUnder, QStringLiteral("<u>\\1</u>"));
    out.replace(kSup, QStringLiteral("<sup>\\1</sup>"));
    out.replace(kSub, QStringLiteral("<sub>\\1</sub>"));
    return out;
}

// Parse Origin dataset reference like "[Book1]Sheet1!A", "T_Book1", or "Book1_A"
void splitDatasetSpec(const QString &spec, QString &outTable, QString &outSheet, QString &outCol)
{
    const QString s = spec.trimmed();
    if (s.isEmpty())
        return;

    // [Book1]Sheet1!ColA or [Book1]Sheet1!Col(A)
    static const QRegularExpression kOriginRangeRe(
            QStringLiteral("^\\[([^\\]]+)\\]([^!]*)!(?:Col\\(([^\\)]+)\\)|(.+))$"),
            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = kOriginRangeRe.match(s);
    if (m.hasMatch()) {
        outTable = m.captured(1).trimmed();
        outSheet = m.captured(2).trimmed();
        outCol = m.captured(3).isEmpty() ? m.captured(4).trimmed() : m.captured(3).trimmed();
        return;
    }

    // Col(A)
    static const QRegularExpression kColParenRe(QStringLiteral("^Col\\(([^\\)]+)\\)$"),
                                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch cm = kColParenRe.match(s);
    if (cm.hasMatch()) {
        outCol = cm.captured(1).trimmed();
        return;
    }

    if (s.startsWith(QLatin1String("T_")) || s.startsWith(QLatin1String("E_"))) {
        outTable = s.mid(2);
        return;
    }

    // Table_Col (strip known table prefix or split on '_' when table is not yet set)
    if (!outTable.isEmpty() && s.size() > outTable.size() + 1
        && s.startsWith(outTable + QLatin1Char('_'), Qt::CaseInsensitive)) {
        outCol = s.mid(outTable.size() + 1);
        return;
    }
    if (outTable.isEmpty() && s.contains(QLatin1Char('_'))) {
        const int idx = s.lastIndexOf(QLatin1Char('_'));
        if (idx > 0 && idx + 1 < s.size()) {
            outTable = s.left(idx);
            outCol = s.mid(idx + 1);
            return;
        }
    }

    outCol = s;
}

// ---------------------------------------------------------------------------
// Binary & inline data decoding for Columns and Matrices
// ---------------------------------------------------------------------------
bool decodeBinaryIntoColumn(const QByteArray &rawBytes, OpjuColumn &col, QString *outErr)
{
    QByteArray payload;
    qint64 off = std::max<qint64>(0, col.dataOffset);
    if (off > rawBytes.size()) {
        if (outErr)
            *outErr = QStringLiteral("Binary column offset exceeds stream length.");
        return false;
    }
    qint64 len = (col.dataLength >= 0) ? std::min<qint64>(col.dataLength, rawBytes.size() - off)
                                       : (rawBytes.size() - off);
    const QByteArray slice = rawBytes.mid(static_cast<int>(off), static_cast<int>(len));

    if (!maybeDecompressInnerStream(slice, col.compressed, payload, outErr))
        return false;

    const OpjuDataType dt = (col.dataType == OpjuDataType::Auto) ? OpjuDataType::Double : col.dataType;

    if (dt == OpjuDataType::Text) {
        col.hasText = true;
        const QString enc = col.encoding.trimmed().toLower();
        const bool isUtf16 = (enc == QLatin1String("utf-16") || enc == QLatin1String("utf-16le")
                              || enc == QLatin1String("utf16") || enc == QLatin1String("utf16le"));

        if (col.stringStride > 0) {
            const int count = payload.size() / col.stringStride;
            const int n = (col.rowCountHint > 0) ? std::min(count, col.rowCountHint) : count;
            col.textValues.reserve(n);
            for (int i = 0; i < n; ++i) {
                QByteArray cell = payload.mid(i * col.stringStride, col.stringStride);
                if (isUtf16) {
                    int endBytes = cell.size();
                    for (int k = 0; k + 1 < cell.size(); k += 2) {
                        if (cell[k] == '\0' && cell[k + 1] == '\0') {
                            endBytes = k;
                            break;
                        }
                    }
                    col.textValues.append(decodeTextBytes(cell.left(endBytes), QStringLiteral("utf-16le")));
                } else {
                    const int nul = cell.indexOf('\0');
                    if (nul >= 0)
                        cell.truncate(nul);
                    col.textValues.append(decodeTextBytes(cell, col.encoding));
                }
            }
            return true;
        }

        if (col.stringFormat.compare(QLatin1String("lengthPrefixed"), Qt::CaseInsensitive) == 0) {
            qint64 p = 0;
            while (p + 4 <= payload.size()) {
                quint32 byteLen = 0;
                if (!readU32LE(payload, p, byteLen))
                    break;
                p += 4;
                if (p + static_cast<qint64>(byteLen) > payload.size())
                    break;
                col.textValues.append(
                        decodeTextBytes(payload.mid(static_cast<int>(p), static_cast<int>(byteLen)),
                                        col.encoding));
                p += static_cast<qint64>(byteLen);
                if (col.rowCountHint > 0 && col.textValues.size() >= col.rowCountHint)
                    break;
            }
            return true;
        }

        if (isUtf16) {
            int p = 0;
            if (payload.size() >= 2 && static_cast<unsigned char>(payload[0]) == 0xFF
                && static_cast<unsigned char>(payload[1]) == 0xFE) {
                p = 2;
            }
            while (p + 1 < payload.size()) {
                int start = p;
                while (p + 1 < payload.size() && !(payload[p] == '\0' && payload[p + 1] == '\0'))
                    p += 2;
                col.textValues.append(decodeTextBytes(payload.mid(start, p - start),
                                                      QStringLiteral("utf-16le")));
                p += 2; // skip null terminator
                if (col.rowCountHint > 0 && col.textValues.size() >= col.rowCountHint)
                    break;
            }
            return true;
        }

        // Null-terminated or newline-delimited UTF-8 / Shift-JIS buffer
        if (payload.contains('\0')) {
            int p = 0;
            while (p < payload.size()) {
                int nul = payload.indexOf('\0', p);
                if (nul < 0)
                    nul = payload.size();
                col.textValues.append(decodeTextBytes(payload.mid(p, nul - p), col.encoding));
                p = nul + 1;
                if (col.rowCountHint > 0 && col.textValues.size() >= col.rowCountHint)
                    break;
            }
        } else {
            const QString all = decodeTextBytes(payload, col.encoding);
            const QStringList lines = all.split(QRegularExpression(QStringLiteral("\\r?\\n")));
            for (const QString &ln : lines) {
                col.textValues.append(ln);
                if (col.rowCountHint > 0 && col.textValues.size() >= col.rowCountHint)
                    break;
            }
        }
        return true;
    }

    // Numeric binary formats
    col.hasNumeric = true;
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    int elemSize = 8;
    switch (dt) {
    case OpjuDataType::Double:
    case OpjuDataType::Date:
    case OpjuDataType::Time:
    case OpjuDataType::DateTime:
    case OpjuDataType::TextNumeric:
    case OpjuDataType::Auto:
        elemSize = 8;
        break;
    case OpjuDataType::Float:
    case OpjuDataType::Int32:
    case OpjuDataType::UInt32:
        elemSize = 4;
        break;
    case OpjuDataType::Int16:
    case OpjuDataType::UInt16:
        elemSize = 2;
        break;
    case OpjuDataType::Int8:
    case OpjuDataType::UInt8:
        elemSize = 1;
        break;
    default:
        elemSize = 8;
        break;
    }

    const int availCount = payload.size() / elemSize;
    const int n = (col.rowCountHint > 0) ? std::min(availCount, col.rowCountHint) : availCount;
    col.numericValues.reserve(n);

    for (int i = 0; i < n; ++i) {
        const qint64 p = static_cast<qint64>(i) * elemSize;
        double v = qnan;
        switch (dt) {
        case OpjuDataType::Float: {
            float fv = 0.0f;
            if (readF32LE(payload, p, fv))
                v = static_cast<double>(fv);
            break;
        }
        case OpjuDataType::Int32: {
            quint32 u = 0;
            if (readU32LE(payload, p, u))
                v = static_cast<double>(static_cast<qint32>(u));
            break;
        }
        case OpjuDataType::UInt32: {
            quint32 u = 0;
            if (readU32LE(payload, p, u))
                v = static_cast<double>(u);
            break;
        }
        case OpjuDataType::Int16: {
            quint16 u = 0;
            if (readU16LE(payload, p, u))
                v = static_cast<double>(static_cast<qint16>(u));
            break;
        }
        case OpjuDataType::UInt16: {
            quint16 u = 0;
            if (readU16LE(payload, p, u))
                v = static_cast<double>(u);
            break;
        }
        case OpjuDataType::Int8:
            v = static_cast<double>(static_cast<qint8>(payload[static_cast<int>(p)]));
            break;
        case OpjuDataType::UInt8:
            v = static_cast<double>(static_cast<quint8>(payload[static_cast<int>(p)]));
            break;
        default: {
            double dv = qnan;
            if (readF64LE(payload, p, dv))
                v = dv;
            break;
        }
        }
        col.numericValues.append(isOriginMissingDouble(v) ? qnan : v);
    }
    return true;
}

void parseInlineTextTokensIntoColumn(const QString &text, OpjuColumn &col)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;

    const bool hasNewline = trimmed.contains(QLatin1Char('\n'));
    const bool hasComma = trimmed.contains(QLatin1Char(','));
    const bool hasTab = trimmed.contains(QLatin1Char('\t'));

    QStringList tokens;
    if (hasNewline) {
        tokens = trimmed.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::KeepEmptyParts);
    } else if (hasTab) {
        tokens = trimmed.split(QLatin1Char('\t'), Qt::KeepEmptyParts);
    } else if (hasComma) {
        tokens = trimmed.split(QLatin1Char(','), Qt::KeepEmptyParts);
    } else {
        tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    }

    const double qnan = std::numeric_limits<double>::quiet_NaN();
    const bool forceText = (col.dataType == OpjuDataType::Text);

    if (forceText) {
        col.hasText = true;
        for (const QString &tok : tokens)
            col.textValues.append(tok.trimmed());
        return;
    }

    // Try numeric parsing; if any non-empty, non-missing token is non-numeric, retain text too
    int numericCount = 0;
    int nonNumericCount = 0;
    QVector<double> nums;
    QVector<QString> strs;
    nums.reserve(tokens.size());
    strs.reserve(tokens.size());

    for (const QString &rawTok : tokens) {
        const QString tok = rawTok.trimmed();
        strs.append(tok);
        if (isOriginMissingNumericText(tok)) {
            nums.append(qnan);
            continue;
        }
        bool ok = false;
        const double v = tok.toDouble(&ok);
        if (ok && !isOriginMissingDouble(v)) {
            nums.append(v);
            ++numericCount;
        } else {
            nums.append(qnan);
            ++nonNumericCount;
        }
    }

    if (nonNumericCount > 0 && (col.dataType == OpjuDataType::Auto || numericCount == 0)) {
        col.hasText = true;
        col.textValues.append(strs);
    } else {
        col.hasNumeric = true;
        col.numericValues.append(nums);
    }
}

bool decodeBinaryIntoMatrix(const QByteArray &rawBytes, OpjuMatrix &mat, QString *outErr)
{
    QByteArray payload;
    qint64 off = std::max<qint64>(0, mat.dataOffset);
    if (off > rawBytes.size()) {
        if (outErr)
            *outErr = QStringLiteral("Binary matrix offset exceeds stream length.");
        return false;
    }
    qint64 len = (mat.dataLength >= 0) ? std::min<qint64>(mat.dataLength, rawBytes.size() - off)
                                       : (rawBytes.size() - off);
    const QByteArray slice = rawBytes.mid(static_cast<int>(off), static_cast<int>(len));

    if (!maybeDecompressInnerStream(slice, mat.compressed, payload, outErr))
        return false;

    const OpjuDataType dt = (mat.dataType == OpjuDataType::Auto) ? OpjuDataType::Double : mat.dataType;
    const int elemSize = (dt == OpjuDataType::Float || dt == OpjuDataType::Int32
                          || dt == OpjuDataType::UInt32)
            ? 4
            : (dt == OpjuDataType::Int16 || dt == OpjuDataType::UInt16)
            ? 2
            : (dt == OpjuDataType::Int8 || dt == OpjuDataType::UInt8) ? 1 : 8;

    const int avail = payload.size() / elemSize;
    const int expected = (mat.rows > 0 && mat.cols > 0) ? (mat.rows * mat.cols) : avail;
    const int count = std::min(avail, expected);
    mat.values.reserve(count);

    for (int i = 0; i < count; ++i) {
        const qint64 p = static_cast<qint64>(i) * elemSize;
        double v = 0.0;
        switch (dt) {
        case OpjuDataType::Float: {
            float fv = 0.0f;
            if (readF32LE(payload, p, fv))
                v = static_cast<double>(fv);
            break;
        }
        case OpjuDataType::Int32: {
            quint32 u = 0;
            if (readU32LE(payload, p, u))
                v = static_cast<double>(static_cast<qint32>(u));
            break;
        }
        case OpjuDataType::UInt32: {
            quint32 u = 0;
            if (readU32LE(payload, p, u))
                v = static_cast<double>(u);
            break;
        }
        case OpjuDataType::Int16: {
            quint16 u = 0;
            if (readU16LE(payload, p, u))
                v = static_cast<double>(static_cast<qint16>(u));
            break;
        }
        case OpjuDataType::UInt16: {
            quint16 u = 0;
            if (readU16LE(payload, p, u))
                v = static_cast<double>(u);
            break;
        }
        case OpjuDataType::Int8:
            v = static_cast<double>(static_cast<qint8>(payload[static_cast<int>(p)]));
            break;
        case OpjuDataType::UInt8:
            v = static_cast<double>(static_cast<quint8>(payload[static_cast<int>(p)]));
            break;
        default:
            readF64LE(payload, p, v);
            break;
        }
        mat.values.append(isOriginMissingDouble(v) ? 0.0 : v);
    }
    return true;
}

// ---------------------------------------------------------------------------
// XML Parser for OriginProj / *.xml inside the ZIP archive
// ---------------------------------------------------------------------------
class OpjuXmlParser
{
public:
    OpjuXmlParser(const ZipArchive &zip, OpjuProject &proj)
        : d_zip(zip), d_proj(proj)
    {
    }

    bool parseArchive(QString *outErr)
    {
        // 1. Identify candidate root XML entries
        static const QStringList kRootCandidates = {
            QStringLiteral("OriginProj.xml"),
            QStringLiteral("OriginProj"),
            QStringLiteral("Project.xml"),
            QStringLiteral("OriginProject.xml"),
            QStringLiteral("project.xml"),
            QStringLiteral("manifest.xml")
        };

        int rootIdx = -1;
        for (const QString &cand : kRootCandidates) {
            rootIdx = d_zip.findEntry(cand);
            if (rootIdx >= 0)
                break;
        }

        if (rootIdx >= 0) {
            if (!parseXmlEntry(rootIdx, QString(), outErr))
                return false;
        }

        // 2. Parse any remaining unparsed XML entries in deterministic order
        for (int i = 0; i < d_zip.entries().size(); ++i) {
            if (d_parsedEntries.contains(i))
                continue;
            const QString lower = d_zip.entries()[i].name.toLower();
            if (lower.endsWith(QLatin1String(".xml")) || lower == QLatin1String("originproj")) {
                QString subErr;
                if (!parseXmlEntry(i, QString(), &subErr)) {
                    // Only fail overall if no root XML was parsed and this was our only XML
                    if (rootIdx < 0 && d_parsedXmlCount == 0) {
                        if (outErr)
                            *outErr = subErr;
                        return false;
                    }
                }
            }
        }

        if (d_parsedXmlCount == 0) {
            if (outErr)
                *outErr = QStringLiteral(
                        "Invalid .opju archive: no Origin XML project metadata found inside ZIP.");
            return false;
        }

        // 3. Resolve external binary/text streams referenced by columns, matrices, notes
        resolveExternalStreams();
        return true;
    }

private:
    bool parseXmlEntry(int entryIdx, const QString &folderContext, QString *outErr)
    {
        if (d_parsedEntries.contains(entryIdx))
            return true;
        d_parsedEntries.insert(entryIdx);

        QByteArray raw;
        if (!d_zip.extract(entryIdx, raw, outErr))
            return false;

        QByteArray xmlBytes;
        if (!maybeDecompressInnerStream(raw, false, xmlBytes, outErr))
            return false;

        const QString entryName = d_zip.entries()[entryIdx].name;
        const int slashPos = entryName.lastIndexOf(QLatin1Char('/'));
        const QString baseDir = (slashPos >= 0) ? entryName.left(slashPos) : QString();

        const QString xmlText = decodeTextBytes(xmlBytes);
        QXmlStreamReader xr(xmlText);

        bool sawStartElement = false;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isStartElement()) {
                sawStartElement = true;
                handleTopElement(xr, baseDir, folderContext);
            }
        }

        if (xr.hasError()) {
            if (outErr)
                *outErr = QStringLiteral("XML parse error in '%1' (line %2): %3")
                                  .arg(entryName)
                                  .arg(xr.lineNumber())
                                  .arg(xr.errorString());
            return false;
        }
        if (!sawStartElement) {
            if (outErr)
                *outErr = QStringLiteral("Empty XML document in '%1'.").arg(entryName);
            return false;
        }

        ++d_parsedXmlCount;
        return true;
    }

    void handleTopElement(QXmlStreamReader &xr, const QString &baseDir,
                          const QString &folderContext)
    {
        const QString tag = xr.name().toString().toLower();
        if (tag == QLatin1String("originproject") || tag == QLatin1String("originproj")
            || tag == QLatin1String("project") || tag == QLatin1String("originstorage")) {
            const QString rootName = attrCI(xr.attributes(), { "name", "rootName", "title" });
            if (!rootName.isEmpty() && d_proj.rootFolderName.isEmpty())
                d_proj.rootFolderName = rootName;
            parseContainerChildren(xr, baseDir, folderContext, nullptr);
        } else if (tag == QLatin1String("folder") || tag == QLatin1String("projectfolder")) {
            OpjuFolderNode fn = parseFolderElement(xr, baseDir, folderContext);
            d_proj.rootSubfolders.append(fn);
        } else if (tag == QLatin1String("workbook") || tag == QLatin1String("spreadsheet")
                   || tag == QLatin1String("table") || tag == QLatin1String("worksheet")
                   || tag == QLatin1String("sheet")) {
            parseWorkbookOrSheet(xr, baseDir, folderContext, QString());
        } else if (tag == QLatin1String("matrix") || tag == QLatin1String("matrixbook")
                   || tag == QLatin1String("matrixsheet")) {
            parseMatrixElement(xr, baseDir, folderContext);
        } else if (tag == QLatin1String("note") || tag == QLatin1String("notes")
                   || tag == QLatin1String("textwindow")) {
            parseNoteElement(xr, baseDir, folderContext);
        } else if (tag == QLatin1String("graph") || tag == QLatin1String("graphpage")
                   || tag == QLatin1String("multilayer") || tag == QLatin1String("plotpage")) {
            parseGraphElement(xr, baseDir, folderContext);
        } else {
            // Walk children in case of wrapper tags (e.g. <Workbooks>, <Graphs>, <Pages>)
            parseContainerChildren(xr, baseDir, folderContext, nullptr);
        }
    }

    void parseContainerChildren(QXmlStreamReader &xr, const QString &baseDir,
                                const QString &folderPath, OpjuFolderNode *currentFolder)
    {
        while (!xr.atEnd()) {
            const QXmlStreamReader::TokenType tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;

            const QString tag = xr.name().toString().toLower();
            const QXmlStreamAttributes attrs = xr.attributes();

            // External XML file reference inside project tree
            const QString extRef = attrCI(attrs, { "src", "href", "file", "xml", "path" });
            if (!extRef.isEmpty() && extRef.toLower().endsWith(QLatin1String(".xml"))
                && tag != QLatin1String("column") && tag != QLatin1String("data")) {
                const int idx = d_zip.findEntry(extRef, baseDir);
                if (idx >= 0) {
                    QString ignored;
                    parseXmlEntry(idx, folderPath, &ignored);
                }
                xr.skipCurrentElement();
                continue;
            }

            if (tag == QLatin1String("folder") || tag == QLatin1String("projectfolder")
                || tag == QLatin1String("dir")) {
                OpjuFolderNode child = parseFolderElement(xr, baseDir, folderPath);
                if (currentFolder)
                    currentFolder->children.append(child);
                else
                    d_proj.rootSubfolders.append(child);
            } else if (tag == QLatin1String("workbook") || tag == QLatin1String("spreadsheet")
                       || tag == QLatin1String("table") || tag == QLatin1String("worksheet")
                       || tag == QLatin1String("sheet")) {
                const int before = d_proj.worksheets.size();
                parseWorkbookOrSheet(xr, baseDir, folderPath, QString());
                if (currentFolder) {
                    for (int i = before; i < d_proj.worksheets.size(); ++i) {
                        currentFolder->windowRefs.append(
                                qMakePair(QStringLiteral("Table"), d_proj.worksheets[i].resolvedName));
                    }
                }
            } else if (tag == QLatin1String("matrix") || tag == QLatin1String("matrixbook")
                       || tag == QLatin1String("matrixsheet")) {
                const int before = d_proj.matrices.size();
                parseMatrixElement(xr, baseDir, folderPath);
                if (currentFolder) {
                    for (int i = before; i < d_proj.matrices.size(); ++i) {
                        currentFolder->windowRefs.append(
                                qMakePair(QStringLiteral("Matrix"), d_proj.matrices[i].name));
                    }
                }
            } else if (tag == QLatin1String("note") || tag == QLatin1String("textwindow")) {
                const int before = d_proj.notes.size();
                parseNoteElement(xr, baseDir, folderPath);
                if (currentFolder) {
                    for (int i = before; i < d_proj.notes.size(); ++i) {
                        currentFolder->windowRefs.append(
                                qMakePair(QStringLiteral("Note"), d_proj.notes[i].name));
                    }
                }
            } else if (tag == QLatin1String("graph") || tag == QLatin1String("graphpage")
                       || tag == QLatin1String("multilayer") || tag == QLatin1String("plotpage")) {
                const int before = d_proj.graphs.size();
                parseGraphElement(xr, baseDir, folderPath);
                if (currentFolder) {
                    for (int i = before; i < d_proj.graphs.size(); ++i) {
                        currentFolder->windowRefs.append(
                                qMakePair(QStringLiteral("MultiLayer"), d_proj.graphs[i].name));
                    }
                }
            } else if (tag == QLatin1String("window") || tag == QLatin1String("page")
                       || tag == QLatin1String("item")) {
                const QString wType = attrCI(attrs, { "type", "kind", "class" }).toLower();
                const QString wName = attrCI(attrs, { "name", "id", "ref" });
                if (currentFolder && !wName.isEmpty()) {
                    QString scidavisType = QStringLiteral("Table");
                    if (wType.contains(QLatin1String("matrix")))
                        scidavisType = QStringLiteral("Matrix");
                    else if (wType.contains(QLatin1String("graph"))
                             || wType.contains(QLatin1String("plot"))
                             || wType.contains(QLatin1String("multilayer")))
                        scidavisType = QStringLiteral("MultiLayer");
                    else if (wType.contains(QLatin1String("note")))
                        scidavisType = QStringLiteral("Note");
                    currentFolder->windowRefs.append(qMakePair(scidavisType, wName));
                }
                xr.skipCurrentElement();
            } else if (tag == QLatin1String("workbooks") || tag == QLatin1String("tables")
                       || tag == QLatin1String("spreadsheets") || tag == QLatin1String("matrices")
                       || tag == QLatin1String("graphs") || tag == QLatin1String("notes")
                       || tag == QLatin1String("folders") || tag == QLatin1String("pages")
                       || tag == QLatin1String("projecttree") || tag == QLatin1String("tree")) {
                parseContainerChildren(xr, baseDir, folderPath, currentFolder);
            } else {
                xr.skipCurrentElement();
            }
        }
    }

    OpjuFolderNode parseFolderElement(QXmlStreamReader &xr, const QString &baseDir,
                                      const QString &parentPath)
    {
        OpjuFolderNode node;
        node.name = attrCI(xr.attributes(), { "name", "title", "id" }).trimmed();
        if (node.name.isEmpty())
            node.name = QStringLiteral("Folder");
        node.fullPath = parentPath.isEmpty() ? (QLatin1Char('/') + node.name)
                                             : (parentPath + QLatin1Char('/') + node.name);
        parseContainerChildren(xr, baseDir, node.fullPath, &node);
        return node;
    }

    void parseWorkbookOrSheet(QXmlStreamReader &xr, const QString &baseDir,
                              const QString &folderPath, const QString &parentBookName)
    {
        const QString tag = xr.name().toString().toLower();
        const QXmlStreamAttributes attrs = xr.attributes();
        const QString elemName = attrCI(attrs, { "name", "shortName", "id" }).trimmed();
        const QString label = attrCI(attrs, { "label", "longName", "title" }).trimmed();
        const bool hidden = parseBoolAttr(attrCI(attrs, { "hidden" }), false);
        const QString attrFolder = attrCI(attrs, { "folder", "folderPath" }).trimmed();
        const QString effFolder = attrFolder.isEmpty() ? folderPath : attrFolder;

        const bool isWorkbookTag = (tag == QLatin1String("workbook"));
        const QString bookName = !parentBookName.isEmpty()
                ? parentBookName
                : (!elemName.isEmpty() ? elemName
                                       : QStringLiteral("Book%1").arg(d_proj.worksheets.size() + 1));

        OpjuWorksheet directSheet;
        directSheet.workbookName = bookName;
        directSheet.sheetName = isWorkbookTag ? QStringLiteral("Sheet1")
                                              : (!elemName.isEmpty() ? elemName : QStringLiteral("Sheet1"));
        directSheet.label = label;
        directSheet.folderPath = effFolder;
        directSheet.hidden = hidden;

        int childSheetsAdded = 0;
        const int startIdx = d_proj.worksheets.size();

        while (!xr.atEnd()) {
            const QXmlStreamReader::TokenType tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;

            const QString ctag = xr.name().toString().toLower();
            if (ctag == QLatin1String("worksheet") || ctag == QLatin1String("sheet")) {
                parseWorkbookOrSheet(xr, baseDir, effFolder, bookName);
                ++childSheetsAdded;
            } else if (ctag == QLatin1String("column") || ctag == QLatin1String("col")) {
                OpjuColumn col = parseColumnElement(xr, baseDir, directSheet.columns.size());
                directSheet.columns.append(col);
            } else if (ctag == QLatin1String("columns")) {
                while (!xr.atEnd()) {
                    const auto t2 = xr.readNext();
                    if (t2 == QXmlStreamReader::EndElement)
                        break;
                    if (t2 == QXmlStreamReader::StartElement) {
                        const QString t2Name = xr.name().toString().toLower();
                        if (t2Name == QLatin1String("column") || t2Name == QLatin1String("col")) {
                            directSheet.columns.append(
                                    parseColumnElement(xr, baseDir, directSheet.columns.size()));
                        } else {
                            xr.skipCurrentElement();
                        }
                    }
                }
            } else if (ctag == QLatin1String("rows") || ctag == QLatin1String("data")) {
                parseRowOrientedData(xr, directSheet);
            } else if (ctag == QLatin1String("label") || ctag == QLatin1String("longname")) {
                directSheet.label = xr.readElementText().trimmed();
            } else {
                xr.skipCurrentElement();
            }
        }

        if (childSheetsAdded > 0) {
            // Assign resolved names for multi-sheet workbooks
            for (int i = startIdx; i < d_proj.worksheets.size(); ++i) {
                OpjuWorksheet &ws = d_proj.worksheets[i];
                if (ws.label.isEmpty())
                    ws.label = label;
                if (i == startIdx && childSheetsAdded == 1) {
                    ws.resolvedName = ws.workbookName;
                } else {
                    const QString sn = ws.sheetName.isEmpty()
                            ? QStringLiteral("Sheet%1").arg(i - startIdx + 1)
                            : ws.sheetName;
                    ws.resolvedName = (i == startIdx)
                            ? ws.workbookName
                            : QStringLiteral("%1@%2").arg(ws.workbookName, sn);
                }
            }
        } else if (!directSheet.columns.isEmpty()) {
            directSheet.resolvedName = parentBookName.isEmpty()
                    ? directSheet.workbookName
                    : QStringLiteral("%1@%2").arg(parentBookName, directSheet.sheetName);
            d_proj.worksheets.append(directSheet);
        }
    }

    OpjuColumn parseColumnElement(QXmlStreamReader &xr, const QString &baseDir, int colIndex)
    {
        OpjuColumn col;
        col.baseDir = baseDir;
        const QXmlStreamAttributes attrs = xr.attributes();

        col.shortName = attrCI(attrs, { "name", "shortName", "id" }).trimmed();
        col.longName = attrCI(attrs, { "longName", "label", "title" }).trimmed();
        col.units = attrCI(attrs, { "units", "unit" }).trimmed();
        col.comment = attrCI(attrs, { "comment", "comments", "description" }).trimmed();
        col.formula = attrCI(attrs, { "formula", "command" }).trimmed();
        col.designation = parsePlotDesignation(
                attrCI(attrs, { "designation", "plotDesignation", "type", "role" }), colIndex);
        col.dataType = parseDataType(
                attrCI(attrs, { "dataType", "valueType", "binaryType", "storageType", "format" }));
        if (col.dataType == OpjuDataType::Auto) {
            const QString typeAttr = attrCI(attrs, { "type" }).trimmed().toLower();
            if (typeAttr == QLatin1String("date") || typeAttr == QLatin1String("time")
                || typeAttr == QLatin1String("datetime") || typeAttr == QLatin1String("text")
                || typeAttr == QLatin1String("string") || typeAttr == QLatin1String("str")) {
                col.dataType = parseDataType(typeAttr);
            }
        }
        col.dataRef = attrCI(attrs, { "dataFile", "dataRef", "stream", "src", "file", "binary" })
                              .trimmed();
        col.encoding = attrCI(attrs, { "encoding", "charset" }).trimmed();
        col.stringFormat = attrCI(attrs, { "stringFormat", "strFormat" }).trimmed();

        const QString offStr = attrCI(attrs, { "offset", "dataOffset" });
        if (!offStr.isEmpty())
            col.dataOffset = offStr.toLongLong();
        const QString lenStr = attrCI(attrs, { "length", "size", "dataLength", "byteLength" });
        if (!lenStr.isEmpty())
            col.dataLength = lenStr.toLongLong();
        const QString rowsStr = attrCI(attrs, { "rows", "rowCount", "count" });
        if (!rowsStr.isEmpty())
            col.rowCountHint = rowsStr.toInt();
        const QString strideStr = attrCI(attrs, { "stride", "recordSize", "itemSize" });
        if (!strideStr.isEmpty())
            col.stringStride = strideStr.toInt();
        const QString widthStr = attrCI(attrs, { "width" });
        if (!widthStr.isEmpty()) {
            const int w = widthStr.toInt();
            col.widthPixels = (w > 0 && w <= 40) ? (w * 10) : w;
        }
        const QString decStr = attrCI(attrs, { "decimalPlaces", "decimals", "precision" });
        if (!decStr.isEmpty())
            col.decimalPlaces = decStr.toInt();

        const QString compStr = attrCI(attrs, { "compressed", "compression" }).trimmed().toLower();
        if (compStr == QLatin1String("zlib") || compStr == QLatin1String("gzip")
            || compStr == QLatin1String("deflate") || parseBoolAttr(compStr, false)) {
            col.compressed = true;
        }

        while (!xr.atEnd()) {
            const QXmlStreamReader::TokenType tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;

            const QString ctag = xr.name().toString().toLower();
            const QXmlStreamAttributes cattrs = xr.attributes();
            if (ctag == QLatin1String("shortname") || ctag == QLatin1String("name")) {
                col.shortName = xr.readElementText().trimmed();
            } else if (ctag == QLatin1String("longname") || ctag == QLatin1String("label")) {
                col.longName = xr.readElementText().trimmed();
            } else if (ctag == QLatin1String("units") || ctag == QLatin1String("unit")) {
                col.units = xr.readElementText().trimmed();
            } else if (ctag == QLatin1String("comment") || ctag == QLatin1String("comments")) {
                col.comment = xr.readElementText().trimmed();
            } else if (ctag == QLatin1String("formula") || ctag == QLatin1String("command")) {
                col.formula = xr.readElementText().trimmed();
            } else if (ctag == QLatin1String("designation")
                       || ctag == QLatin1String("plotdesignation")) {
                col.designation = parsePlotDesignation(xr.readElementText(), colIndex);
            } else if (ctag == QLatin1String("data") || ctag == QLatin1String("values")) {
                parseColumnDataElement(xr, col);
            } else if (ctag == QLatin1String("cell") || ctag == QLatin1String("v")
                       || ctag == QLatin1String("value") || ctag == QLatin1String("item")) {
                const QString cellTxt = xr.readElementText();
                appendSingleCell(col, cellTxt);
            } else {
                xr.skipCurrentElement();
            }
        }

        if (col.shortName.isEmpty()) {
            // Default Origin column naming: A, B, C, ..., Z, AA, AB, ...
            int n = colIndex;
            QString s;
            do {
                s.prepend(QChar('A' + (n % 26)));
                n = n / 26 - 1;
            } while (n >= 0);
            col.shortName = s;
        }
        return col;
    }

    void appendSingleCell(OpjuColumn &col, const QString &rawText)
    {
        const double qnan = std::numeric_limits<double>::quiet_NaN();
        if (col.dataType == OpjuDataType::Text) {
            col.hasText = true;
            col.textValues.append(rawText);
            return;
        }
        const QString t = rawText.trimmed();
        if (isOriginMissingNumericText(t)) {
            col.hasNumeric = true;
            col.numericValues.append(qnan);
            if (col.hasText)
                col.textValues.append(QString());
            return;
        }
        bool ok = false;
        const double v = t.toDouble(&ok);
        if (ok && !isOriginMissingDouble(v) && !col.hasText) {
            col.hasNumeric = true;
            col.numericValues.append(v);
        } else {
            // Promote previously collected numeric values to text if mixed
            if (!col.hasText && !col.numericValues.isEmpty()) {
                for (double prev : col.numericValues) {
                    col.textValues.append(std::isfinite(prev) ? QString::number(prev, 'g', 15)
                                                              : QString());
                }
            }
            col.hasText = true;
            col.textValues.append(rawText);
        }
    }

    void parseColumnDataElement(QXmlStreamReader &xr, OpjuColumn &col)
    {
        const QXmlStreamAttributes attrs = xr.attributes();
        const QString ref = attrCI(attrs, { "src", "file", "dataFile", "stream", "ref" }).trimmed();
        if (!ref.isEmpty())
            col.dataRef = ref;
        const QString dt = attrCI(attrs, { "dataType", "type", "binaryType", "format" });
        if (!dt.isEmpty() && col.dataType == OpjuDataType::Auto)
            col.dataType = parseDataType(dt);
        const QString offStr = attrCI(attrs, { "offset" });
        if (!offStr.isEmpty())
            col.dataOffset = offStr.toLongLong();
        const QString lenStr = attrCI(attrs, { "length", "size", "byteLength" });
        if (!lenStr.isEmpty())
            col.dataLength = lenStr.toLongLong();
        const QString rowsStr = attrCI(attrs, { "rows", "count" });
        if (!rowsStr.isEmpty())
            col.rowCountHint = rowsStr.toInt();
        const QString compStr = attrCI(attrs, { "compressed", "compression" }).trimmed().toLower();
        if (compStr == QLatin1String("zlib") || compStr == QLatin1String("gzip")
            || compStr == QLatin1String("deflate") || parseBoolAttr(compStr, false)) {
            col.compressed = true;
        }
        const QString enc = attrCI(attrs, { "encoding" }).trimmed().toLower();
        if (!enc.isEmpty() && enc != QLatin1String("base64"))
            col.encoding = enc;

        QString collectedText;
        bool hadChildCells = false;
        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok == QXmlStreamReader::Characters) {
                collectedText.append(xr.text().toString());
            } else if (tok == QXmlStreamReader::StartElement) {
                const QString ctag = xr.name().toString().toLower();
                if (ctag == QLatin1String("cell") || ctag == QLatin1String("v")
                    || ctag == QLatin1String("value") || ctag == QLatin1String("row")
                    || ctag == QLatin1String("item")) {
                    hadChildCells = true;
                    appendSingleCell(col, xr.readElementText());
                } else {
                    xr.skipCurrentElement();
                }
            }
        }

        if (!hadChildCells && !collectedText.trimmed().isEmpty()) {
            if (enc == QLatin1String("base64")) {
                col.inlineBinary = QByteArray::fromBase64(collectedText.toLatin1());
            } else {
                parseInlineTextTokensIntoColumn(collectedText, col);
            }
        }
    }

    void parseRowOrientedData(QXmlStreamReader &xr, OpjuWorksheet &ws)
    {
        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;
            const QString tag = xr.name().toString().toLower();
            if (tag == QLatin1String("row") || tag == QLatin1String("r")) {
                int cIdx = 0;
                while (!xr.atEnd()) {
                    const auto t2 = xr.readNext();
                    if (t2 == QXmlStreamReader::EndElement)
                        break;
                    if (t2 != QXmlStreamReader::StartElement)
                        continue;
                    const QString ctag = xr.name().toString().toLower();
                    if (ctag == QLatin1String("cell") || ctag == QLatin1String("c")
                        || ctag == QLatin1String("v")) {
                        const QString idxAttr = attrCI(xr.attributes(), { "col", "index" });
                        if (!idxAttr.isEmpty()) {
                            bool ok = false;
                            const int parsed = idxAttr.toInt(&ok);
                            if (ok && parsed >= 0)
                                cIdx = parsed;
                        }
                        const QString cellTxt = xr.readElementText();
                        while (ws.columns.size() <= cIdx) {
                            OpjuColumn nc;
                            int n = ws.columns.size();
                            QString s;
                            do {
                                s.prepend(QChar('A' + (n % 26)));
                                n = n / 26 - 1;
                            } while (n >= 0);
                            nc.shortName = s;
                            nc.designation = (ws.columns.isEmpty()) ? SciDAVis::X : SciDAVis::Y;
                            ws.columns.append(nc);
                        }
                        appendSingleCell(ws.columns[cIdx], cellTxt);
                        ++cIdx;
                    } else {
                        xr.skipCurrentElement();
                    }
                }
            } else {
                xr.skipCurrentElement();
            }
        }
    }

    void parseMatrixElement(QXmlStreamReader &xr, const QString &baseDir,
                            const QString &folderPath)
    {
        OpjuMatrix mat;
        mat.baseDir = baseDir;
        const QXmlStreamAttributes attrs = xr.attributes();
        mat.name = attrCI(attrs, { "name", "shortName", "id" }).trimmed();
        if (mat.name.isEmpty())
            mat.name = QStringLiteral("MBook%1").arg(d_proj.matrices.size() + 1);
        mat.label = attrCI(attrs, { "label", "longName", "title" }).trimmed();
        const QString attrFolder = attrCI(attrs, { "folder", "folderPath" }).trimmed();
        mat.folderPath = attrFolder.isEmpty() ? folderPath : attrFolder;
        mat.formula = attrCI(attrs, { "formula", "command" }).trimmed();
        mat.hidden = parseBoolAttr(attrCI(attrs, { "hidden" }), false);
        mat.rows = attrCI(attrs, { "rows", "rowCount" }).toInt();
        mat.cols = attrCI(attrs, { "cols", "colCount", "columns" }).toInt();
        mat.dataType = parseDataType(attrCI(attrs, { "dataType", "valueType", "binaryType" }));
        mat.dataRef = attrCI(attrs, { "dataFile", "dataRef", "stream", "src", "file" }).trimmed();

        const QString offStr = attrCI(attrs, { "offset" });
        if (!offStr.isEmpty())
            mat.dataOffset = offStr.toLongLong();
        const QString lenStr = attrCI(attrs, { "length", "size" });
        if (!lenStr.isEmpty())
            mat.dataLength = lenStr.toLongLong();
        const QString compStr = attrCI(attrs, { "compressed", "compression" }).trimmed().toLower();
        if (compStr == QLatin1String("zlib") || compStr == QLatin1String("gzip")
            || parseBoolAttr(compStr, false))
            mat.compressed = true;

        const QString xMinS = attrCI(attrs, { "xMin", "xStart", "x1" });
        const QString xMaxS = attrCI(attrs, { "xMax", "xEnd", "x2" });
        const QString yMinS = attrCI(attrs, { "yMin", "yStart", "y1" });
        const QString yMaxS = attrCI(attrs, { "yMax", "yEnd", "y2" });
        if (!xMinS.isEmpty() && !xMaxS.isEmpty() && !yMinS.isEmpty() && !yMaxS.isEmpty()) {
            mat.xStart = xMinS.toDouble();
            mat.xEnd = xMaxS.toDouble();
            mat.yStart = yMinS.toDouble();
            mat.yEnd = yMaxS.toDouble();
            mat.hasCoords = true;
        }

        int parsedRowCount = 0;
        int parsedMaxCols = 0;

        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;
            const QString ctag = xr.name().toString().toLower();
            const QXmlStreamAttributes cattrs = xr.attributes();
            if (ctag == QLatin1String("matrixsheet") || ctag == QLatin1String("sheet")) {
                parseMatrixElement(xr, baseDir, mat.folderPath);
            } else if (ctag == QLatin1String("formula") || ctag == QLatin1String("command")) {
                mat.formula = xr.readElementText().trimmed();
            } else if (ctag == QLatin1String("data") || ctag == QLatin1String("values")) {
                const QString ref = attrCI(cattrs, { "src", "file", "dataFile", "stream" }).trimmed();
                if (!ref.isEmpty())
                    mat.dataRef = ref;
                const QString dt = attrCI(cattrs, { "dataType", "type" });
                if (!dt.isEmpty())
                    mat.dataType = parseDataType(dt);
                const QString enc = attrCI(cattrs, { "encoding" }).trimmed().toLower();
                const QString txt = xr.readElementText().trimmed();
                if (!txt.isEmpty()) {
                    if (enc == QLatin1String("base64")) {
                        QString ignored;
                        decodeBinaryIntoMatrix(QByteArray::fromBase64(txt.toLatin1()), mat, &ignored);
                    } else {
                        const QStringList lines = txt.split(
                                QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts);
                        for (const QString &ln : lines) {
                            const QStringList parts = ln.trimmed().split(
                                    QRegularExpression(QStringLiteral("[,\\t\\s]+")),
                                    Qt::SkipEmptyParts);
                            if (parts.isEmpty())
                                continue;
                            ++parsedRowCount;
                            parsedMaxCols = std::max(parsedMaxCols, parts.size());
                            for (const QString &p : parts) {
                                bool ok = false;
                                const double v = p.toDouble(&ok);
                                mat.values.append((ok && !isOriginMissingDouble(v)) ? v : 0.0);
                            }
                        }
                    }
                }
            } else if (ctag == QLatin1String("row")) {
                ++parsedRowCount;
                int c = 0;
                while (!xr.atEnd()) {
                    const auto t2 = xr.readNext();
                    if (t2 == QXmlStreamReader::EndElement)
                        break;
                    if (t2 == QXmlStreamReader::StartElement) {
                        const QString vtag = xr.name().toString().toLower();
                        if (vtag == QLatin1String("cell") || vtag == QLatin1String("v")) {
                            bool ok = false;
                            const double v = xr.readElementText().trimmed().toDouble(&ok);
                            mat.values.append((ok && !isOriginMissingDouble(v)) ? v : 0.0);
                            ++c;
                        } else {
                            xr.skipCurrentElement();
                        }
                    }
                }
                parsedMaxCols = std::max(parsedMaxCols, c);
            } else {
                xr.skipCurrentElement();
            }
        }

        if (mat.rows <= 0)
            mat.rows = parsedRowCount;
        if (mat.cols <= 0)
            mat.cols = parsedMaxCols;
        if (mat.rows > 0 && mat.cols > 0)
            d_proj.matrices.append(mat);
    }

    void parseNoteElement(QXmlStreamReader &xr, const QString &baseDir, const QString &folderPath)
    {
        OpjuNote note;
        note.baseDir = baseDir;
        const QXmlStreamAttributes attrs = xr.attributes();
        note.name = attrCI(attrs, { "name", "shortName", "id" }).trimmed();
        if (note.name.isEmpty())
            note.name = QStringLiteral("Notes%1").arg(d_proj.notes.size() + 1);
        note.label = attrCI(attrs, { "label", "longName", "title" }).trimmed();
        const QString attrFolder = attrCI(attrs, { "folder", "folderPath" }).trimmed();
        note.folderPath = attrFolder.isEmpty() ? folderPath : attrFolder;
        note.dataRef = attrCI(attrs, { "src", "file", "dataFile", "stream" }).trimmed();
        note.hidden = parseBoolAttr(attrCI(attrs, { "hidden" }), false);

        QString directText;
        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok == QXmlStreamReader::Characters) {
                directText.append(xr.text().toString());
            } else if (tok == QXmlStreamReader::StartElement) {
                const QString ctag = xr.name().toString().toLower();
                if (ctag == QLatin1String("text") || ctag == QLatin1String("content")
                    || ctag == QLatin1String("body")) {
                    note.text = xr.readElementText();
                } else if (ctag == QLatin1String("label") || ctag == QLatin1String("title")) {
                    note.label = xr.readElementText().trimmed();
                } else {
                    xr.skipCurrentElement();
                }
            }
        }
        if (note.text.isEmpty() && !directText.trimmed().isEmpty())
            note.text = directText.trimmed();
        d_proj.notes.append(note);
    }

    void parseGraphElement(QXmlStreamReader &xr, const QString &baseDir, const QString &folderPath)
    {
        Q_UNUSED(baseDir);
        OpjuGraph g;
        const QXmlStreamAttributes attrs = xr.attributes();
        g.name = attrCI(attrs, { "name", "shortName", "id" }).trimmed();
        if (g.name.isEmpty())
            g.name = QStringLiteral("Graph%1").arg(d_proj.graphs.size() + 1);
        g.label = attrCI(attrs, { "label", "longName", "title" }).trimmed();
        const QString attrFolder = attrCI(attrs, { "folder", "folderPath" }).trimmed();
        g.folderPath = attrFolder.isEmpty() ? folderPath : attrFolder;
        g.hidden = parseBoolAttr(attrCI(attrs, { "hidden" }), false);

        OpjuLayer implicitLayer;
        bool hasImplicitLayerContent = false;

        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;
            const QString ctag = xr.name().toString().toLower();
            if (ctag == QLatin1String("layer") || ctag == QLatin1String("graphlayer")) {
                g.layers.append(parseLayerElement(xr));
            } else if (ctag == QLatin1String("plot") || ctag == QLatin1String("curve")
                       || ctag == QLatin1String("dataplot")) {
                implicitLayer.curves.append(parseCurveElement(xr));
                hasImplicitLayerContent = true;
            } else if (ctag == QLatin1String("axis") || ctag == QLatin1String("xaxis")
                       || ctag == QLatin1String("yaxis") || ctag == QLatin1String("bottomaxis")
                       || ctag == QLatin1String("leftaxis") || ctag == QLatin1String("topaxis")
                       || ctag == QLatin1String("rightaxis")) {
                parseAxisElement(xr, implicitLayer);
                hasImplicitLayerContent = true;
            } else if (ctag == QLatin1String("legend")) {
                implicitLayer.legendText = parseLegendOrText(xr);
                hasImplicitLayerContent = true;
            } else if (ctag == QLatin1String("text") || ctag == QLatin1String("annotation")) {
                const QString t = parseLegendOrText(xr);
                if (!t.isEmpty()) {
                    implicitLayer.annotations.append(t);
                    hasImplicitLayerContent = true;
                }
            } else if (ctag == QLatin1String("layers")) {
                while (!xr.atEnd()) {
                    const auto t2 = xr.readNext();
                    if (t2 == QXmlStreamReader::EndElement)
                        break;
                    if (t2 == QXmlStreamReader::StartElement) {
                        const QString t2Name = xr.name().toString().toLower();
                        if (t2Name == QLatin1String("layer")
                            || t2Name == QLatin1String("graphlayer")) {
                            g.layers.append(parseLayerElement(xr));
                        } else {
                            xr.skipCurrentElement();
                        }
                    }
                }
            } else {
                xr.skipCurrentElement();
            }
        }

        if (g.layers.isEmpty() && hasImplicitLayerContent)
            g.layers.append(implicitLayer);
        if (!g.layers.isEmpty())
            d_proj.graphs.append(g);
    }

    OpjuLayer parseLayerElement(QXmlStreamReader &xr)
    {
        OpjuLayer layer;
        layer.title = attrCI(xr.attributes(), { "title", "label" }).trimmed();

        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;
            const QString ctag = xr.name().toString().toLower();
            if (ctag == QLatin1String("plot") || ctag == QLatin1String("curve")
                || ctag == QLatin1String("dataplot")) {
                layer.curves.append(parseCurveElement(xr));
            } else if (ctag == QLatin1String("plots") || ctag == QLatin1String("curves")) {
                while (!xr.atEnd()) {
                    const auto t2 = xr.readNext();
                    if (t2 == QXmlStreamReader::EndElement)
                        break;
                    if (t2 == QXmlStreamReader::StartElement) {
                        const QString t2Name = xr.name().toString().toLower();
                        if (t2Name == QLatin1String("plot") || t2Name == QLatin1String("curve")
                            || t2Name == QLatin1String("dataplot")) {
                            layer.curves.append(parseCurveElement(xr));
                        } else {
                            xr.skipCurrentElement();
                        }
                    }
                }
            } else if (ctag == QLatin1String("axis") || ctag == QLatin1String("xaxis")
                       || ctag == QLatin1String("yaxis") || ctag == QLatin1String("bottomaxis")
                       || ctag == QLatin1String("leftaxis") || ctag == QLatin1String("topaxis")
                       || ctag == QLatin1String("rightaxis")) {
                parseAxisElement(xr, layer);
            } else if (ctag == QLatin1String("axes")) {
                while (!xr.atEnd()) {
                    const auto t2 = xr.readNext();
                    if (t2 == QXmlStreamReader::EndElement)
                        break;
                    if (t2 == QXmlStreamReader::StartElement) {
                        parseAxisElement(xr, layer);
                    }
                }
            } else if (ctag == QLatin1String("legend")) {
                layer.legendText = parseLegendOrText(xr);
            } else if (ctag == QLatin1String("text") || ctag == QLatin1String("annotation")) {
                const QString t = parseLegendOrText(xr);
                if (!t.isEmpty())
                    layer.annotations.append(t);
            } else if (ctag == QLatin1String("title")) {
                layer.title = xr.readElementText().trimmed();
            } else {
                xr.skipCurrentElement();
            }
        }
        return layer;
    }

    OpjuCurve parseCurveElement(QXmlStreamReader &xr)
    {
        OpjuCurve cv;
        const QXmlStreamAttributes attrs = xr.attributes();
        cv.tableName = attrCI(attrs, { "table", "workbook", "dataName", "book" }).trimmed();
        cv.sheetName = attrCI(attrs, { "sheet", "worksheet" }).trimmed();

        const QString xSpec = attrCI(attrs, { "xCol", "xColumn", "x", "xData" }).trimmed();
        const QString ySpec = attrCI(attrs, { "yCol", "yColumn", "y", "yData" }).trimmed();
        const QString errSpec = attrCI(attrs, { "errCol", "yErr", "xErr", "errorCol" }).trimmed();

        if (cv.tableName.startsWith(QLatin1String("T_"))
            || cv.tableName.startsWith(QLatin1String("E_"))) {
            cv.tableName = cv.tableName.mid(2);
        }
        if (!xSpec.isEmpty())
            splitDatasetSpec(xSpec, cv.tableName, cv.sheetName, cv.xColName);
        if (!ySpec.isEmpty())
            splitDatasetSpec(ySpec, cv.tableName, cv.sheetName, cv.yColName);
        if (!errSpec.isEmpty()) {
            QString tTmp = cv.tableName, sTmp = cv.sheetName;
            splitDatasetSpec(errSpec, tTmp, sTmp, cv.errColName);
        }

        const QString typeStr = attrCI(attrs, { "type", "plotType", "style" });
        if (!typeStr.isEmpty())
            cv.plotStyle = parsePlotStyle(typeStr);

        cv.lineColor = parseColor(attrCI(attrs, { "lineColor", "color" }));
        const QString lwStr = attrCI(attrs, { "lineWidth", "width" });
        if (!lwStr.isEmpty())
            cv.lineWidth = std::max(0.1, lwStr.toDouble());
        cv.lineStyle = parseLineStyle(attrCI(attrs, { "lineStyle", "dash" }));

        const QString symStr = attrCI(attrs, { "symbol", "symbolShape", "marker" });
        if (!symStr.isEmpty())
            cv.symbolShape = parseSymbolShape(symStr);
        else if (cv.plotStyle == Graph::Scatter || cv.plotStyle == Graph::LineSymbols)
            cv.symbolShape = 1; // Circle default for scatter/line+symbol

        const QString szStr = attrCI(attrs, { "symbolSize", "markerSize" });
        if (!szStr.isEmpty())
            cv.symbolSize = std::max(2.0, szStr.toDouble());

        cv.symbolColor = parseColor(attrCI(attrs, { "symbolColor", "markerColor" }));
        cv.symbolFillColor = parseColor(attrCI(attrs, { "symbolFillColor", "markerFillColor" }));
        cv.symbolFilled = parseBoolAttr(attrCI(attrs, { "symbolFilled", "symbolFill" }), true);
        cv.fillArea = parseBoolAttr(attrCI(attrs, { "fillArea", "filled" }), false);
        cv.fillColor = parseColor(attrCI(attrs, { "fillColor", "areaColor" }));
        cv.legendText = attrCI(attrs, { "legend", "title", "label" }).trimmed();

        const QString connStr = attrCI(attrs, { "connect", "lineConnect" }).trimmed().toLower();
        if (!connStr.isEmpty()) {
            cv.hasExplicitConnect = true;
            if (connStr == QLatin1String("none") || connStr == QLatin1String("noline"))
                cv.connectType = QwtPlotCurve::NoCurve;
            else if (connStr == QLatin1String("line") || connStr == QLatin1String("lines")
                     || connStr == QLatin1String("straight"))
                cv.connectType = QwtPlotCurve::Lines;
            else if (connStr == QLatin1String("spline") || connStr == QLatin1String("bspline")
                     || connStr == QLatin1String("bezier"))
                cv.connectType = 5;
            else if (connStr == QLatin1String("steph") || connStr == QLatin1String("steps"))
                cv.connectType = QwtPlotCurve::Steps;
            else if (connStr == QLatin1String("stepv"))
                cv.connectType = 6;
            else
                cv.hasExplicitConnect = false;
        }
        if (!cv.hasExplicitConnect && cv.plotStyle == Graph::Scatter)
            cv.connectType = QwtPlotCurve::NoCurve;

        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;
            const QString ctag = xr.name().toString().toLower();
            const QXmlStreamAttributes cattrs = xr.attributes();
            if (ctag == QLatin1String("line")) {
                const QColor lc = parseColor(attrCI(cattrs, { "color" }));
                if (lc.isValid())
                    cv.lineColor = lc;
                const QString lw = attrCI(cattrs, { "width" });
                if (!lw.isEmpty())
                    cv.lineWidth = std::max(0.1, lw.toDouble());
                const QString ls = attrCI(cattrs, { "style" });
                if (!ls.isEmpty())
                    cv.lineStyle = parseLineStyle(ls);
                xr.skipCurrentElement();
            } else if (ctag == QLatin1String("symbol") || ctag == QLatin1String("marker")) {
                const QString sh = attrCI(cattrs, { "shape", "type" });
                if (!sh.isEmpty())
                    cv.symbolShape = parseSymbolShape(sh);
                const QString sz = attrCI(cattrs, { "size" });
                if (!sz.isEmpty())
                    cv.symbolSize = std::max(2.0, sz.toDouble());
                const QColor sc = parseColor(attrCI(cattrs, { "color", "edgeColor" }));
                if (sc.isValid())
                    cv.symbolColor = sc;
                const QColor fc = parseColor(attrCI(cattrs, { "fillColor" }));
                if (fc.isValid())
                    cv.symbolFillColor = fc;
                xr.skipCurrentElement();
            } else if (ctag == QLatin1String("legend") || ctag == QLatin1String("title")) {
                cv.legendText = xr.readElementText().trimmed();
            } else {
                xr.skipCurrentElement();
            }
        }
        return cv;
    }

    void parseAxisElement(QXmlStreamReader &xr, OpjuLayer &layer)
    {
        const QString tag = xr.name().toString().toLower();
        const QXmlStreamAttributes attrs = xr.attributes();
        const QString id = attrCI(attrs, { "id", "name", "orientation", "position", "which" })
                                   .trimmed()
                                   .toLower();

        OpjuAxis *target = &layer.xAxis;
        if (tag == QLatin1String("yaxis") || tag == QLatin1String("leftaxis")
            || id == QLatin1String("y") || id == QLatin1String("left")
            || id == QLatin1String("yleft")) {
            target = &layer.yAxis;
        } else if (tag == QLatin1String("topaxis") || id == QLatin1String("x2")
                   || id == QLatin1String("top") || id == QLatin1String("xtop")) {
            target = &layer.x2Axis;
        } else if (tag == QLatin1String("rightaxis") || id == QLatin1String("y2")
                   || id == QLatin1String("right") || id == QLatin1String("yright")) {
            target = &layer.y2Axis;
        }

        target->configured = true;
        const QString titleAttr = attrCI(attrs, { "title", "label" }).trimmed();
        if (!titleAttr.isEmpty())
            target->title = titleAttr;

        const QString minS = attrCI(attrs, { "min", "from", "start" });
        const QString maxS = attrCI(attrs, { "max", "to", "end" });
        if (!minS.isEmpty() && !maxS.isEmpty()) {
            bool ok1 = false, ok2 = false;
            const double mn = minS.toDouble(&ok1);
            const double mx = maxS.toDouble(&ok2);
            if (ok1 && ok2 && std::isfinite(mn) && std::isfinite(mx) && mn < mx) {
                target->hasRange = true;
                target->minVal = mn;
                target->maxVal = mx;
            }
        }
        const QString stepS = attrCI(attrs, { "step", "inc", "increment" });
        if (!stepS.isEmpty())
            target->step = stepS.toDouble();
        const QString majS = attrCI(attrs, { "majorTicks" });
        if (!majS.isEmpty())
            target->majorTicks = std::max(2, majS.toInt());
        const QString minTS = attrCI(attrs, { "minorTicks" });
        if (!minTS.isEmpty())
            target->minorTicks = std::max(0, minTS.toInt());

        const QString scaleS = attrCI(attrs, { "scale", "scaleType", "type" });
        if (!scaleS.isEmpty())
            target->scaleType = parseScaleType(scaleS);
        target->inverted = parseBoolAttr(attrCI(attrs, { "inverted", "reverse" }), false);
        target->showMajorGrid = parseBoolAttr(attrCI(attrs, { "majorGrid", "grid" }), false);
        target->showMinorGrid = parseBoolAttr(attrCI(attrs, { "minorGrid" }), false);
        target->hidden = parseBoolAttr(attrCI(attrs, { "hidden" }), false);
        if (!attrCI(attrs, { "visible" }).isEmpty())
            target->hidden = !parseBoolAttr(attrCI(attrs, { "visible" }), true);
        target->showLabels = parseBoolAttr(attrCI(attrs, { "showLabels" }), true);

        while (!xr.atEnd()) {
            const auto tok = xr.readNext();
            if (tok == QXmlStreamReader::EndElement)
                break;
            if (tok != QXmlStreamReader::StartElement)
                continue;
            const QString ctag = xr.name().toString().toLower();
            const QXmlStreamAttributes cattrs = xr.attributes();
            if (ctag == QLatin1String("title") || ctag == QLatin1String("label")) {
                target->title = xr.readElementText().trimmed();
            } else if (ctag == QLatin1String("scale") || ctag == QLatin1String("range")) {
                const QString mnS = attrCI(cattrs, { "min", "from", "start" });
                const QString mxS = attrCI(cattrs, { "max", "to", "end" });
                if (!mnS.isEmpty() && !mxS.isEmpty()) {
                    bool ok1 = false, ok2 = false;
                    const double mn = mnS.toDouble(&ok1);
                    const double mx = mxS.toDouble(&ok2);
                    if (ok1 && ok2 && std::isfinite(mn) && std::isfinite(mx) && mn < mx) {
                        target->hasRange = true;
                        target->minVal = mn;
                        target->maxVal = mx;
                    }
                }
                const QString st = attrCI(cattrs, { "type", "scale", "scaleType" });
                if (!st.isEmpty())
                    target->scaleType = parseScaleType(st);
                const QString step = attrCI(cattrs, { "step", "inc" });
                if (!step.isEmpty())
                    target->step = step.toDouble();
                xr.skipCurrentElement();
            } else if (ctag == QLatin1String("grid")) {
                target->showMajorGrid = parseBoolAttr(
                        attrCI(cattrs, { "major", "visible", "enabled" }), true);
                target->showMinorGrid = parseBoolAttr(attrCI(cattrs, { "minor" }), false);
                xr.skipCurrentElement();
            } else {
                xr.skipCurrentElement();
            }
        }
    }

    QString parseLegendOrText(QXmlStreamReader &xr)
    {
        QString txt = attrCI(xr.attributes(), { "text", "value" });
        const QString body = xr.readElementText();
        if (!body.trimmed().isEmpty())
            txt = body;
        return txt.trimmed();
    }

    void resolveExternalStreams()
    {
        for (OpjuWorksheet &ws : d_proj.worksheets) {
            for (OpjuColumn &col : ws.columns) {
                if (!col.inlineBinary.isEmpty()) {
                    QString ignored;
                    decodeBinaryIntoColumn(col.inlineBinary, col, &ignored);
                } else if (!col.dataRef.isEmpty()) {
                    const int idx = d_zip.findEntry(col.dataRef, col.baseDir);
                    if (idx >= 0) {
                        QByteArray stream;
                        QString ignored;
                        if (d_zip.extract(idx, stream, &ignored))
                            decodeBinaryIntoColumn(stream, col, &ignored);
                    }
                }
            }
        }

        for (OpjuMatrix &mat : d_proj.matrices) {
            if (mat.values.isEmpty() && !mat.dataRef.isEmpty()) {
                const int idx = d_zip.findEntry(mat.dataRef, mat.baseDir);
                if (idx >= 0) {
                    QByteArray stream;
                    QString ignored;
                    if (d_zip.extract(idx, stream, &ignored))
                        decodeBinaryIntoMatrix(stream, mat, &ignored);
                }
            }
        }

        for (OpjuNote &note : d_proj.notes) {
            if (note.text.isEmpty() && !note.dataRef.isEmpty()) {
                const int idx = d_zip.findEntry(note.dataRef, note.baseDir);
                if (idx >= 0) {
                    QByteArray stream;
                    QString ignored;
                    if (d_zip.extract(idx, stream, &ignored)) {
                        QByteArray decomp;
                        if (maybeDecompressInnerStream(stream, false, decomp, &ignored))
                            note.text = decodeTextBytes(decomp);
                    }
                }
            }
        }
    }

    const ZipArchive &d_zip;
    OpjuProject &d_proj;
    QSet<int> d_parsedEntries;
    int d_parsedXmlCount = 0;
};

// ---------------------------------------------------------------------------
// ToriGraph Object Reconstruction
// ---------------------------------------------------------------------------
Table *resolveTable(ApplicationWindow *mw, const QHash<QString, Table *> &tableMap,
                    const QString &tableName, const QString &sheetName)
{
    if (!mw)
        return nullptr;
    if (!tableName.isEmpty() && !sheetName.isEmpty()) {
        const QString key = QStringLiteral("%1@%2").arg(tableName, sheetName).toLower();
        if (tableMap.contains(key))
            return tableMap.value(key);
    }
    if (!tableName.isEmpty()) {
        const QString key = tableName.toLower();
        if (tableMap.contains(key))
            return tableMap.value(key);
        if (Table *t = mw->table(tableName))
            return t;
        const QString hyphen = QString(tableName).replace(QLatin1Char('_'), QLatin1Char('-'));
        if (Table *t = mw->table(hyphen))
            return t;
        for (MyWidget *w : mw->windowsList()) {
            if (w && w->inherits("Table")) {
                auto *t = static_cast<Table *>(w);
                if (t->name().compare(tableName, Qt::CaseInsensitive) == 0
                    || t->name().compare(hyphen, Qt::CaseInsensitive) == 0) {
                    return t;
                }
            }
        }
    }
    return nullptr;
}

QString resolveColumnShortName(Table *table, const QString &requested, bool preferX)
{
    if (!table || table->numCols() <= 0)
        return QString();

    if (!requested.isEmpty()) {
        if (table->column(requested))
            return requested;
        for (int c = 0; c < table->numCols(); ++c) {
            Column *col = table->column(c);
            if (!col)
                continue;
            if (col->name().compare(requested, Qt::CaseInsensitive) == 0
                || col->longName().compare(requested, Qt::CaseInsensitive) == 0) {
                return col->name();
            }
        }
    }

    if (preferX) {
        for (int c = 0; c < table->numCols(); ++c) {
            if (table->colPlotDesignation(c) == SciDAVis::X)
                return table->column(c)->name();
        }
        return table->column(0)->name();
    }
    return QString();
}

QString resolveLegendPlaceholders(const QString &raw, const QVector<OpjuCurve> &curves,
                                  ApplicationWindow *mw, const QHash<QString, Table *> &tableMap)
{
    QString out = parseOriginRichText(raw);
    static const QRegularExpression kPctRe(
            QStringLiteral("%\\(\\s*(\\d+)(?:\\s*,\\s*([^\\)]*))?\\)"));
    QRegularExpressionMatchIterator it = kPctRe.globalMatch(out);
    QVector<QPair<QPair<int, int>, QString>> edits;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int idx = m.captured(1).toInt(&ok) - 1;
        if (!ok || idx < 0 || idx >= curves.size()) {
            edits.append({ qMakePair(m.capturedStart(0), m.capturedLength(0)), QString() });
            continue;
        }
        const OpjuCurve &cv = curves[idx];
        const QString mod = m.captured(2).trimmed().toUpper();
        Table *tbl = resolveTable(mw, tableMap, cv.tableName, cv.sheetName);
        const QString yName = resolveColumnShortName(tbl, cv.yColName, false);
        const QString xName = resolveColumnShortName(tbl, cv.xColName, true);
        Column *yCol = (tbl && !yName.isEmpty()) ? tbl->column(yName) : nullptr;
        Column *xCol = (tbl && !xName.isEmpty()) ? tbl->column(xName) : nullptr;

        QString rep;
        if (mod.startsWith(QLatin1String("@X"))) {
            if (xCol && !xCol->longName().isEmpty())
                rep = xCol->longName();
            else if (xCol && !xCol->comment().isEmpty())
                rep = xCol->comment();
            else
                rep = xName;
        } else if (mod == QLatin1String("@U") && yCol) {
            rep = yCol->units();
        } else if (mod == QLatin1String("@C") && yCol) {
            rep = yCol->comment();
        } else if (mod == QLatin1String("@L") && yCol) {
            rep = yCol->longName();
        } else if (mod == QLatin1String("@SN")) {
            rep = yName;
        } else if (mod == QLatin1String("@WS") || mod == QLatin1String("@WL")) {
            rep = (tbl && !tbl->windowLabel().isEmpty() && mod == QLatin1String("@WL"))
                    ? tbl->windowLabel()
                    : cv.tableName;
        } else {
            if (!cv.legendText.isEmpty())
                rep = cv.legendText;
            else if (yCol && !yCol->longName().isEmpty())
                rep = yCol->longName();
            else if (yCol && !yCol->comment().isEmpty())
                rep = yCol->comment();
            else
                rep = yName;
        }
        edits.append({ qMakePair(m.capturedStart(0), m.capturedLength(0)), rep });
    }
    for (int i = edits.size() - 1; i >= 0; --i)
        out.replace(edits[i].first.first, edits[i].first.second, edits[i].second);
    return out;
}

void createFolderSubtree(ApplicationWindow *mw, Folder *parentFolder, const OpjuFolderNode &node,
                         QHash<QString, Folder *> &folderByPath, int &folderCount)
{
    if (!mw || !parentFolder)
        return;
    Folder &f = parentFolder->addChild<Folder>(node.name);
    mw->addFolderListViewItem(&f);
    folderByPath.insert(node.fullPath.toLower(), &f);
    ++folderCount;
    for (const OpjuFolderNode &child : node.children)
        createFolderSubtree(mw, &f, child, folderByPath, folderCount);
}

Folder *ensureFolderPath(ApplicationWindow *mw, Folder *rootFolder, const QString &rawPath,
                         QHash<QString, Folder *> &folderByPath, int &folderCount)
{
    if (!mw || !rootFolder)
        return rootFolder;
    const QString cleaned = rawPath.trimmed();
    if (cleaned.isEmpty() || cleaned == QLatin1String("/"))
        return rootFolder;

    const QString key = cleaned.startsWith(QLatin1Char('/')) ? cleaned.toLower()
                                                             : (QLatin1Char('/') + cleaned.toLower());
    if (folderByPath.contains(key))
        return folderByPath.value(key);

    const QStringList parts = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    Folder *cur = rootFolder;
    QString accum;
    for (const QString &part : parts) {
        accum += QLatin1Char('/') + part;
        const QString k = accum.toLower();
        if (folderByPath.contains(k)) {
            cur = folderByPath.value(k);
        } else {
            Folder &nf = cur->addChild<Folder>(part);
            mw->addFolderListViewItem(&nf);
            folderByPath.insert(k, &nf);
            cur = &nf;
            ++folderCount;
        }
    }
    return cur;
}

void moveWindowToFolder(Folder *rootFolder, Folder *targetFolder, MyWidget *w)
{
    if (!rootFolder || !targetFolder || !w || targetFolder == rootFolder)
        return;
    rootFolder->removeWindow(w);
    targetFolder->addWindow(w);
    targetFolder->setActiveWindow(w);
}

void moveFolderRefWindows(Folder *rootFolder, const OpjuFolderNode &node,
                          const QHash<QString, Folder *> &folderByPath)
{
    Folder *target = folderByPath.value(node.fullPath.toLower(), nullptr);
    if (target && target != rootFolder) {
        for (const auto &ref : node.windowRefs) {
            MyWidget *w = rootFolder->window(ref.second, ref.first.toLatin1().constData());
            if (w)
                moveWindowToFolder(rootFolder, target, w);
        }
    }
    for (const OpjuFolderNode &child : node.children)
        moveFolderRefWindows(rootFolder, child, folderByPath);
}

bool materializeProject(ApplicationWindow *mw, const OpjuProject &proj,
                        ImportOPJU::ImportStats &stats)
{
    if (!mw)
        return false;

    stats.datasetsSkipped += proj.skippedDatasets;
    stats.graphsSkipped += proj.skippedGraphs;
    stats.matricesSkipped += proj.skippedMatrices;
    stats.isCpyuaContainer = stats.isCpyuaContainer || proj.isCpyuaContainer;

    int xoffset = 0;
    QHash<QString, Table *> tableMap;
    QVector<QPair<MyWidget *, QString>> windowFolderAssignments;

    // 1. Import Worksheets / Tables
    int tableVisCount = 0;
    for (const OpjuWorksheet &ws : proj.worksheets) {
        const int colCount = ws.columns.size();
        if (colCount <= 0)
            continue;

        int maxRows = 0;
        for (const OpjuColumn &c : ws.columns) {
            maxRows = std::max(maxRows, c.numericValues.size());
            maxRows = std::max(maxRows, c.textValues.size());
        }
        if (maxRows <= 0)
            maxRows = 30;

        const QString tblName = ws.resolvedName.isEmpty() ? ws.workbookName : ws.resolvedName;
        Table *table = mw->newTable(tblName, maxRows, colCount);
        if (!table)
            continue;

        if (!ws.label.isEmpty())
            table->setWindowLabel(ws.label);

        for (int j = 0; j < colCount; ++j) {
            const OpjuColumn &src = ws.columns[j];
            Column *col = table->column(j);
            if (!col)
                continue;

            if (!src.shortName.isEmpty())
                col->setName(src.shortName);
            if (!src.longName.isEmpty())
                col->setLongName(src.longName);
            if (!src.units.isEmpty())
                col->setUnits(src.units);
            if (!src.comment.isEmpty())
                col->setComment(src.comment);
            if (!src.formula.isEmpty())
                col->setFormula(Interval<int>(0, std::max(0, maxRows - 1)), src.formula);
            if (src.widthPixels > 0)
                table->setColumnWidth(j, src.widthPixels);

            table->setColPlotDesignation(j, src.designation);

            const bool isDateLike = (src.dataType == OpjuDataType::Date
                                     || src.dataType == OpjuDataType::Time
                                     || src.dataType == OpjuDataType::DateTime);
            const QString dateFmt = (src.dataType == OpjuDataType::Date)
                    ? QStringLiteral("yyyy-MM-dd")
                    : ((src.dataType == OpjuDataType::Time) ? QStringLiteral("hh:mm:ss")
                                                            : QStringLiteral("yyyy-MM-dd hh:mm:ss"));
            bool populatedDate = false;
            if (isDateLike && src.hasNumeric && !src.numericValues.isEmpty()) {
                col->setColumnMode(SciDAVis::ColumnMode::Numeric);
                const int nRows = std::min(src.numericValues.size(), maxRows);
                for (int r = 0; r < nRows; ++r) {
                    const double v = src.numericValues[r];
                    if (!isOriginMissingDouble(v))
                        col->setValueAt(r, v);
                }
                col->setColumnMode(SciDAVis::ColumnMode::DateTime);
                if (auto *dt2s = qobject_cast<DateTime2StringFilter *>(col->outputFilter()))
                    dt2s->setFormat(dateFmt);
                populatedDate = true;
            } else if (isDateLike && src.hasText && !src.textValues.isEmpty()) {
                const int nRows = std::min(src.textValues.size(), maxRows);
                QVector<QDateTime> parsedDates(nRows);
                bool anyValidDate = false;
                for (int r = 0; r < nRows; ++r) {
                    const QString txt = src.textValues[r].trimmed();
                    if (txt.isEmpty())
                        continue;
                    QDateTime dt = QDateTime::fromString(txt, Qt::ISODateWithMs);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(txt, Qt::ISODate);
                    if (!dt.isValid() && src.dataType == OpjuDataType::Time) {
                        QTime tm = QTime::fromString(txt, QStringLiteral("hh:mm:ss.zzz"));
                        if (!tm.isValid())
                            tm = QTime::fromString(txt, QStringLiteral("hh:mm:ss"));
                        if (!tm.isValid())
                            tm = QTime::fromString(txt, QStringLiteral("hh:mm"));
                        if (tm.isValid())
                            dt = QDateTime(QDate(1970, 1, 1), tm);
                    }
                    if (!dt.isValid()) {
                        dt = QDateTime::fromString(txt, QStringLiteral("yyyy-MM-dd hh:mm:ss"));
                        if (!dt.isValid()) {
                            const QDate d = QDate::fromString(txt, QStringLiteral("yyyy/MM/dd"));
                            if (d.isValid())
                                dt = QDateTime(d, QTime(0, 0));
                        }
                    }
                    if (dt.isValid()) {
                        parsedDates[r] = dt;
                        anyValidDate = true;
                    }
                }
                if (anyValidDate) {
                    col->setColumnMode(SciDAVis::ColumnMode::DateTime);
                    if (auto *dt2s = qobject_cast<DateTime2StringFilter *>(col->outputFilter()))
                        dt2s->setFormat(dateFmt);
                    for (int r = 0; r < nRows; ++r) {
                        if (parsedDates[r].isValid())
                            col->setDateTimeAt(r, parsedDates[r]);
                    }
                    populatedDate = true;
                }
            }

            if (!populatedDate) {
                const bool useText =
                        (src.dataType == OpjuDataType::Text) || (src.hasText && !src.hasNumeric);
                if (useText) {
                    col->setColumnMode(SciDAVis::ColumnMode::Text);
                    const int nRows = std::min(src.textValues.size(), maxRows);
                    for (int r = 0; r < nRows; ++r) {
                        if (!src.textValues[r].isEmpty())
                            col->setTextAt(r, src.textValues[r]);
                    }
                } else {
                    col->setColumnMode(SciDAVis::ColumnMode::Numeric);
                    const int nRows = std::min(src.numericValues.size(), maxRows);
                    for (int r = 0; r < nRows; ++r) {
                        const double v = src.numericValues[r];
                        if (!isOriginMissingDouble(v))
                            col->setValueAt(r, v);
                    }
                    if (src.decimalPlaces >= 0 || src.numericFormat != 0) {
                        if (auto *flt = dynamic_cast<Double2StringFilter *>(col->outputFilter())) {
                            flt->setNumericFormat(src.numericFormat);
                            if (src.decimalPlaces >= 0)
                                flt->setNumDigits(src.decimalPlaces);
                        }
                    }
                }
            }
        }

        if (ws.hidden) {
            mw->hideWindow(table);
        } else {
            table->showNormal();
            table->move(QPoint(tableVisCount * 24 + xoffset * kObjectXOffset, tableVisCount * 24));
            ++tableVisCount;
        }

        tableMap.insert(tblName.toLower(), table);
        if (!ws.workbookName.isEmpty() && !tableMap.contains(ws.workbookName.toLower()))
            tableMap.insert(ws.workbookName.toLower(), table);
        if (!ws.workbookName.isEmpty() && !ws.sheetName.isEmpty()) {
            tableMap.insert(
                    QStringLiteral("%1@%2").arg(ws.workbookName, ws.sheetName).toLower(), table);
        }
        if (!ws.folderPath.isEmpty())
            windowFolderAssignments.append(qMakePair(static_cast<MyWidget *>(table), ws.folderPath));
        ++stats.tablesImported;
    }
    if (tableVisCount > 0)
        ++xoffset;

    // 2. Import Matrices
    int matVisCount = 0;
    for (const OpjuMatrix &m : proj.matrices) {
        if (m.rows <= 0 || m.cols <= 0) {
            ++stats.matricesSkipped;
            continue;
        }
        Matrix *mat = mw->newMatrix(m.name, m.rows, m.cols);
        if (!mat) {
            ++stats.matricesSkipped;
            continue;
        }
        if (!m.label.isEmpty())
            mat->setWindowLabel(m.label);
        if (!m.formula.isEmpty())
            mat->setFormula(m.formula);
        if (m.hasCoords)
            mat->setCoordinates(m.xStart, m.xEnd, m.yStart, m.yEnd);

        QVector<qreal> cells(m.rows * m.cols, 0.0);
        const int copyCount = std::min(cells.size(), m.values.size());
        for (int i = 0; i < copyCount; ++i)
            cells[i] = std::isfinite(m.values[i]) ? m.values[i] : 0.0;
        mat->setCells(cells);

        if (m.hidden) {
            mw->hideWindow(mat);
        } else {
            mat->showNormal();
            mat->move(QPoint(matVisCount * 28 + xoffset * kObjectXOffset, matVisCount * 28));
            ++matVisCount;
        }
        if (!m.folderPath.isEmpty())
            windowFolderAssignments.append(qMakePair(static_cast<MyWidget *>(mat), m.folderPath));
        ++stats.matricesImported;
    }
    if (matVisCount > 0)
        ++xoffset;

    // 3. Import Notes
    int noteVisCount = 0;
    for (const OpjuNote &n : proj.notes) {
        Note *note = mw->newNote(n.name);
        if (!note)
            continue;
        if (!n.label.isEmpty())
            note->setWindowLabel(n.label);
        note->setText(n.text);
        if (n.hidden) {
            mw->hideWindow(note);
        } else {
            note->move(QPoint(noteVisCount * 20 + xoffset * kObjectXOffset, noteVisCount * 20));
            ++noteVisCount;
        }
        if (!n.folderPath.isEmpty())
            windowFolderAssignments.append(qMakePair(static_cast<MyWidget *>(note), n.folderPath));
        ++stats.notesImported;
    }
    if (noteVisCount > 0)
        ++xoffset;

    // 4. Import 2D MultiLayer Graphs
    int graphVisCount = 0;
    for (const OpjuGraph &g : proj.graphs) {
        MultiLayer *ml = mw->multilayerPlot(g.name);
        if (!ml) {
            ++stats.graphsSkipped;
            continue;
        }
        ml->hide();
        if (!g.label.isEmpty())
            ml->setWindowLabel(g.label);

        int requestedCurves = 0;
        int addedCurves = 0;
        for (const OpjuLayer &layer : g.layers) {
            Graph *graph = ml->addLayer();
            if (!graph)
                continue;

            int autoColor = 0;
            Column *firstXCol = nullptr;
            Column *firstYCol = nullptr;

            for (const OpjuCurve &cv : layer.curves) {
                ++requestedCurves;
                Table *table = resolveTable(mw, tableMap, cv.tableName, cv.sheetName);
                if (!table)
                    continue;
                const QString actualTbl = table->name();
                const QString xShort = resolveColumnShortName(table, cv.xColName, true);
                const QString yShort = resolveColumnShortName(table, cv.yColName, false);
                if (yShort.isEmpty())
                    continue;

                if (!firstXCol && !xShort.isEmpty())
                    firstXCol = table->column(xShort);
                if (!firstYCol)
                    firstYCol = table->column(yShort);

                const int prevCurves = graph->curves();
                if (cv.plotStyle == Graph::ErrorBars) {
                    const QString errShort = resolveColumnShortName(
                            table, cv.errColName.isEmpty() ? yShort : cv.errColName, false);
                    graph->addErrorBars(QStringLiteral("%1_%2").arg(actualTbl, xShort), table,
                                        QStringLiteral("%1_%2").arg(actualTbl, errShort), 1,
                                        std::max(1, static_cast<int>(std::ceil(cv.lineWidth))),
                                        std::max(3, static_cast<int>(std::ceil(cv.symbolSize))),
                                        cv.lineColor.isValid() ? cv.lineColor : QColor(Qt::black),
                                        true, true, true);
                } else if (cv.plotStyle == Graph::Histogram || cv.plotStyle == Graph::Box
                           || cv.plotStyle == Graph::Pie) {
                    graph->insertCurvesList(
                            table, QStringList() << QStringLiteral("%1_%2").arg(actualTbl, yShort),
                            cv.plotStyle,
                            std::max(1, static_cast<int>(std::ceil(cv.lineWidth))),
                            std::max(3, static_cast<int>(std::ceil(cv.symbolSize))));
                } else {
                    if (xShort.isEmpty())
                        continue;
                    graph->insertCurve(table, QStringLiteral("%1_%2").arg(actualTbl, xShort),
                                       QStringLiteral("%1_%2").arg(actualTbl, yShort),
                                       cv.plotStyle);
                }

                if (graph->curves() <= prevCurves)
                    continue;
                ++addedCurves;
                const int curveIdx = graph->curves() - 1;
                if (cv.plotStyle == Graph::ErrorBars)
                    continue;

                CurveLayout cl = graph->initCurveLayout(cv.plotStyle, layer.curves.size());
                const unsigned int fallbackIdx = static_cast<unsigned int>(
                        autoColor++ % static_cast<int>(ColorButton::colors_count));
                cl.lCol = cv.lineColor.isValid() ? ColorButton::colorIndex(cv.lineColor)
                                                 : fallbackIdx;
                cl.lWidth = std::max(0.1, cv.lineWidth);
                cl.lStyle = cv.lineStyle;
                cl.sType = cv.symbolShape;
                cl.sSize = std::max(2, static_cast<int>(std::lround(cv.symbolSize)));
                cl.symCol = cv.symbolColor.isValid() ? ColorButton::colorIndex(cv.symbolColor)
                                                     : cl.lCol;
                cl.symbolFill = cv.symbolFilled;
                cl.fillCol = cv.symbolFillColor.isValid()
                        ? ColorButton::colorIndex(cv.symbolFillColor)
                        : cl.symCol;
                cl.filledArea = (cv.fillArea || cv.plotStyle == Graph::Area
                                 || cv.plotStyle == Graph::VerticalBars
                                 || cv.plotStyle == Graph::HorizontalBars
                                 || cv.plotStyle == Graph::Histogram)
                        ? 1
                        : 0;
                if (cl.filledArea) {
                    cl.aStyle = 0;
                    cl.aCol = cv.fillColor.isValid() ? ColorButton::colorIndex(cv.fillColor)
                                                     : cl.lCol;
                }

                graph->updateCurveLayout(curveIdx, &cl);
                if (cv.hasExplicitConnect || cv.plotStyle != Graph::Scatter)
                    graph->setCurveStyle(curveIdx, cv.connectType);

                // Preserve exact 24-bit RGB colors and fractional line widths on the curve pen
                if (QwtPlotCurve *qwtCurve = graph->curve(curveIdx)) {
                    QPen pen = qwtCurve->pen();
                    if (cv.lineColor.isValid())
                        pen.setColor(cv.lineColor);
                    pen.setWidthF(cl.lWidth);
                    qwtCurve->setPen(pen);
                    if (cv.symbolShape > 0
                        && (cv.symbolColor.isValid() || cv.symbolFillColor.isValid())) {
                        QwtSymbol sym = qwtCurve->symbol();
                        if (cv.symbolColor.isValid()) {
                            QPen sp = sym.pen();
                            sp.setColor(cv.symbolColor);
                            sym.setPen(sp);
                        }
                        if (cv.symbolFilled && cv.symbolFillColor.isValid()) {
                            QBrush sb = sym.brush();
                            sb.setColor(cv.symbolFillColor);
                            sb.setStyle(Qt::SolidPattern);
                            sym.setBrush(sb);
                        }
                        qwtCurve->setSymbol(sym);
                    }
                }
            }

            // Axis titles (with ISIJ-clean fallback from column Long Name / Units)
            const QString xTitle = !layer.xAxis.title.isEmpty()
                    ? resolveLegendPlaceholders(layer.xAxis.title, layer.curves, mw, tableMap)
                    : (firstXCol ? firstXCol->formattedAxisTitle(Column::UnitFormat::ISIJSlash)
                                 : QString());
            const QString yTitle = !layer.yAxis.title.isEmpty()
                    ? resolveLegendPlaceholders(layer.yAxis.title, layer.curves, mw, tableMap)
                    : (firstYCol ? firstYCol->formattedAxisTitle(Column::UnitFormat::ISIJSlash)
                                 : QString());
            if (!xTitle.isEmpty())
                graph->setXAxisTitle(xTitle);
            if (!yTitle.isEmpty())
                graph->setYAxisTitle(yTitle);
            if (!layer.x2Axis.title.isEmpty())
                graph->setTopAxisTitle(
                        resolveLegendPlaceholders(layer.x2Axis.title, layer.curves, mw, tableMap));
            if (!layer.y2Axis.title.isEmpty())
                graph->setRightAxisTitle(
                        resolveLegendPlaceholders(layer.y2Axis.title, layer.curves, mw, tableMap));
            if (!layer.title.isEmpty())
                graph->setTitle(resolveLegendPlaceholders(layer.title, layer.curves, mw, tableMap));

            // Axis scales (Linear / Log10)
            auto applyAxisScale = [&](int qwtAxis, const OpjuAxis &ax) {
                if (!ax.configured)
                    return;
                if (ax.hasRange && std::isfinite(ax.minVal) && std::isfinite(ax.maxVal)
                    && ax.minVal < ax.maxVal) {
                    int st = ax.scaleType;
                    if (st == 1 && (ax.minVal <= 0.0 || ax.maxVal <= 0.0))
                        st = 0; // guard log10 against non-positive bounds
                    graph->setScale(qwtAxis, ax.minVal, ax.maxVal, ax.step, ax.majorTicks,
                                    ax.minorTicks, st, ax.inverted);
                } else if (ax.scaleType == 1) {
                    // Log10 requested without explicit range: keep positive autoscale bounds
                    graph->setScale(qwtAxis, 0.1, 100.0, 0.0, ax.majorTicks, ax.minorTicks, 1,
                                    ax.inverted);
                }
            };
            applyAxisScale(QwtPlot::xBottom, layer.xAxis);
            applyAxisScale(QwtPlot::yLeft, layer.yAxis);
            if (layer.x2Axis.configured)
                applyAxisScale(QwtPlot::xTop, layer.x2Axis);
            if (layer.y2Axis.configured)
                applyAxisScale(QwtPlot::yRight, layer.y2Axis);

            // Grid lines
            if (Grid *grid = graph->grid()) {
                grid->enableX(layer.xAxis.showMajorGrid);
                grid->enableXMin(layer.xAxis.showMinorGrid);
                grid->enableY(layer.yAxis.showMajorGrid);
                grid->enableYMin(layer.yAxis.showMinorGrid);
            }

            // Legend & annotations
            if (!layer.legendText.isEmpty()) {
                const QString leg = resolveLegendPlaceholders(layer.legendText, layer.curves, mw,
                                                              tableMap);
                if (!leg.trimmed().isEmpty())
                    graph->newLegend(leg);
            }
            for (const QString &ann : layer.annotations) {
                const QString resolved = resolveLegendPlaceholders(ann, layer.curves, mw, tableMap);
                if (!resolved.trimmed().isEmpty())
                    graph->newLegend(resolved);
            }

            graph->setAutoscaleFonts(mw->autoScaleFonts);
            graph->setIgnoreResizeEvents(!mw->autoResizeLayers);
        }

        if (!g.hidden) {
            ml->move(QPoint(graphVisCount * 24 + xoffset * kObjectXOffset, graphVisCount * 24));
            ++graphVisCount;
            ml->show();
            ml->arrangeLayers(true, true);
        } else {
            ml->show();
            ml->arrangeLayers(true, true);
            mw->hideWindow(ml);
        }
        if (!g.folderPath.isEmpty())
            windowFolderAssignments.append(qMakePair(static_cast<MyWidget *>(ml), g.folderPath));
        if (requestedCurves > 0 && addedCurves == 0)
            ++stats.graphsSkipped;
        ++stats.graphsImported;
    }

    // 5. Reconstruct Folder Hierarchy
    Folder *projectFolder = mw->projectFolder();
    if (projectFolder) {
        if (!proj.rootFolderName.isEmpty()) {
            if (auto *item = dynamic_cast<FolderListItem *>(mw->folders.topLevelItem(0))) {
                item->setText(0, proj.rootFolderName);
                if (item->folder())
                    item->folder()->setName(proj.rootFolderName);
            }
        }

        QHash<QString, Folder *> folderByPath;
        folderByPath.insert(QStringLiteral("/"), projectFolder);
        for (const OpjuFolderNode &fn : proj.rootSubfolders)
            createFolderSubtree(mw, projectFolder, fn, folderByPath, stats.foldersImported);

        for (const auto &assign : windowFolderAssignments) {
            Folder *target = ensureFolderPath(mw, projectFolder, assign.second, folderByPath,
                                              stats.foldersImported);
            moveWindowToFolder(projectFolder, target, assign.first);
        }
        for (const OpjuFolderNode &fn : proj.rootSubfolders)
            moveFolderRefWindows(projectFolder, fn, folderByPath);

        mw->changeFolder(projectFolder, true);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Native OriginPro 2018-2026 CPYUA Binary .opju Parser
// (Zero-RLE headers + Burtscher FPC 0x05 datasets + 0x1D4 column metadata)
// ---------------------------------------------------------------------------
bool decodeCpyuaZeroRle(const QByteArray &src, qint64 offset, qint64 compLen, QByteArray &out)
{
    out.clear();
    if (offset < 0 || compLen < 0 || offset + compLen > src.size())
        return false;
    const auto *p = reinterpret_cast<const unsigned char *>(src.constData() + offset);
    qint64 i = 0;
    constexpr int kMaxDecoded = 1024 * 1024;
    while (i < compLen) {
        const unsigned char b = p[i++];
        if (b & 0x80U) {
            const int zeros = static_cast<int>(b & 0x7FU);
            if (out.size() + zeros > kMaxDecoded)
                return false;
            out.append(zeros, '\0');
        } else {
            const int lit = static_cast<int>(b);
            if (i + lit > compLen || out.size() + lit > kMaxDecoded)
                return false;
            out.append(reinterpret_cast<const char *>(p + i), lit);
            i += lit;
        }
    }
    return true;
}

bool readLeb128(const unsigned char *buf, qint64 len, qint64 &pos, quint64 &out)
{
    out = 0;
    unsigned int shift = 0;
    while (pos < len && shift < 63) {
        const unsigned char b = buf[pos++];
        out |= static_cast<quint64>(b & 0x7FU) << shift;
        if ((b & 0x80U) == 0)
            return true;
        shift += 7;
    }
    return false;
}

bool readCpyuaConstDouble(const unsigned char *buf, qint64 len, qint64 &pos, double &out)
{
    if (pos >= len)
        return false;
    const unsigned char vc = buf[pos++];
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    if (vc == 0x00U || vc == 0x65U) {
        out = qnan;
        return true;
    }
    if (vc == 0x64U) {
        out = 0.0;
        return true;
    }
    if (vc == 0x50U) {
        if (pos + 8 > len)
            return false;
        quint64 bits = 0;
        for (int b = 0; b < 8; ++b)
            bits |= static_cast<quint64>(buf[pos + b]) << (8 * b);
        pos += 8;
        std::memcpy(&out, &bits, sizeof(double));
        if (isOriginMissingDouble(out))
            out = qnan;
        return true;
    }
    // Compact high-byte double literals: 0x11(1B), 0x1a(2B), 0x23(3B), 0x2c(4B),
    // 0x35(5B), 0x3e(6B), 0x47(7B) -> vc = 9 * nb + 8
    if (vc >= 0x11U && vc <= 0x47U && ((vc - 0x08U) % 9U == 0U)) {
        const int nb = static_cast<int>((vc - 0x08U) / 9U);
        if (pos + nb > len)
            return false;
        unsigned char raw8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (int b = 0; b < nb; ++b)
            raw8[8 - nb + b] = buf[pos + b];
        pos += nb;
        std::memcpy(&out, raw8, sizeof(double));
        if (isOriginMissingDouble(out))
            out = qnan;
        return true;
    }
    return false;
}

bool decodeCpyuaFpcChunk(const unsigned char *buf, qint64 len, qint64 &pos, int count, int preds,
                         QVector<double> &out)
{
    if (preds < 1 || preds > 20 || count < 0)
        return false;
    const quint64 tableSize = 1ULL << preds;
    const quint64 mask = tableSize - 1ULL;
    QVector<quint64> fcm(static_cast<int>(tableSize), 0ULL);
    QVector<quint64> dfcm(static_cast<int>(tableSize), 0ULL);
    quint64 hash1 = 0, hash2 = 0, lastval = 0;
    const double qnan = std::numeric_limits<double>::quiet_NaN();

    int i = 0;
    while (i < count) {
        if (pos >= len)
            return false;
        const unsigned char code = buf[pos++];
        for (int shift : { 0, 4 }) {
            if (i >= count)
                break;
            const unsigned int nib = (code >> shift) & 0x0FU;
            const bool useDfcm = (nib & 0x08U) != 0U;
            int nbytes = static_cast<int>(nib & 0x07U);
            if (nbytes >= 4)
                nbytes += 1;
            if (pos + nbytes > len)
                return false;
            const quint64 pred = useDfcm ? (dfcm[static_cast<int>(hash2)] + lastval)
                                         : fcm[static_cast<int>(hash1)];
            quint64 xorVal = 0;
            for (int b = 0; b < nbytes; ++b)
                xorVal |= static_cast<quint64>(buf[pos + b]) << (8 * b);
            pos += nbytes;
            const quint64 val = pred ^ xorVal;
            fcm[static_cast<int>(hash1)] = val;
            hash1 = ((hash1 << 6) ^ (val >> 48)) & mask;
            const quint64 diff = val - lastval;
            dfcm[static_cast<int>(hash2)] = diff;
            hash2 = ((hash2 << 2) ^ (diff >> 40)) & mask;
            lastval = val;

            double dv = qnan;
            std::memcpy(&dv, &val, sizeof(double));
            out.append(isOriginMissingDouble(dv) ? qnan : dv);
            ++i;
        }
    }
    return true;
}

bool decodeCpyuaNumericPayload(const QByteArray &pay, QVector<double> &outValues)
{
    outValues.clear();
    if (pay.size() < 4 || static_cast<unsigned char>(pay[0]) != 0x05U)
        return false;

    const auto *buf = reinterpret_cast<const unsigned char *>(pay.constData());
    const qint64 len = pay.size();
    qint64 pos = 1;
    quint64 totalRows = 0;
    if (!readLeb128(buf, len, pos, totalRows))
        return false;
    if (totalRows > 50000000ULL)
        return false;
    outValues.reserve(static_cast<int>(std::min<quint64>(totalRows, 1000000ULL)));

    const double qnan = std::numeric_limits<double>::quiet_NaN();
    while (pos + 2 <= len) {
        const quint16 marker = static_cast<quint16>(buf[pos])
                | (static_cast<quint16>(buf[pos + 1]) << 8);
        if (marker == 0xFACEU) {
            pos += 2;
            break;
        }
        if (marker == 0xFFFFU) {
            pos += 2;
            quint64 cnt = 0;
            if (!readLeb128(buf, len, pos, cnt) || pos >= len)
                return false;
            const unsigned char kind = buf[pos++];
            Q_UNUSED(kind);
            quint64 got = 0;
            while (got < cnt && pos < len) {
                quint64 tag = 0;
                if (!readLeb128(buf, len, pos, tag))
                    return false;
                if (tag == 1ULL) {
                    double v = qnan;
                    if (!readCpyuaConstDouble(buf, len, pos, v))
                        return false;
                    outValues.append(v);
                    got += 1;
                } else if (tag & 1ULL) {
                    const quint64 rc = (tag + 1ULL) >> 1;
                    if (pos >= len || rc > 50000000ULL)
                        return false;
                    const int preds = static_cast<int>(buf[pos++]);
                    if (!decodeCpyuaFpcChunk(buf, len, pos, static_cast<int>(rc), preds, outValues))
                        return false;
                    got += rc;
                } else {
                    const quint64 rc = tag >> 1;
                    double v = qnan;
                    if (rc > 50000000ULL || !readCpyuaConstDouble(buf, len, pos, v))
                        return false;
                    for (quint64 k = 0; k < rc; ++k)
                        outValues.append(v);
                    got += rc;
                }
            }
        } else {
            // Single-row literal entry: <kind:1B> <0x00:1B> <constDouble>
            const unsigned char kind = buf[pos];
            if ((kind != 0x00U && kind != 0x01U) || buf[pos + 1] != 0x00U)
                return false;
            pos += 2;
            double v = qnan;
            if (!readCpyuaConstDouble(buf, len, pos, v))
                return false;
            outValues.append(kind == 0x01U ? qnan : v);
        }
    }
    return !outValues.isEmpty() || totalRows == 0;
}

// Best-effort decoder for non-0x05 CPYUA dataset payloads (e.g. text columns).
// Returns true when at least one printable text cell was recovered (D4-8).
bool decodeCpyuaTextPayload(const QByteArray &pay, const QString &encodingHint,
                            QVector<QString> &outTexts)
{
    outTexts.clear();
    if (pay.isEmpty())
        return false;

    int end = pay.size();
    if (end >= 2 && static_cast<unsigned char>(pay[end - 2]) == 0xCEU
        && static_cast<unsigned char>(pay[end - 1]) == 0xFAU) {
        end -= 2; // strip trailing 0xFACE marker
    }
    while (end > 0 && pay[end - 1] == '\0')
        --end;
    if (end <= 0)
        return false;

    int start = 0;
    const auto *buf = reinterpret_cast<const unsigned char *>(pay.constData());
    if (buf[0] < 0x20U && buf[0] != '\t' && buf[0] != '\r' && buf[0] != '\n') {
        qint64 p = 1;
        quint64 totalRows = 0;
        if (readLeb128(buf, end, p, totalRows) && totalRows <= 50000000ULL && p <= end) {
            if (p + 2 <= end && (static_cast<quint16>(buf[p]) | (static_cast<quint16>(buf[p + 1]) << 8)) == 0xFFFFU) {
                qint64 p2 = p + 2;
                quint64 cnt = 0;
                if (readLeb128(buf, end, p2, cnt) && p2 < end)
                    p = p2 + 1;
            }
            start = static_cast<int>(p);
        }
    }
    if (start >= end)
        return false;

    auto isPrintableCell = [](const QString &s) -> bool {
        if (s.isEmpty())
            return false;
        for (const QChar ch : s) {
            const ushort u = ch.unicode();
            if (u == 0xFFFD || (u < 0x20 && u != '\t'))
                return false;
        }
        return true;
    };

    const QByteArray slice = pay.mid(start, end - start);
    QVector<QString> cells;
    int nonEmptyCount = 0;
    if (slice.contains('\0')) {
        const QList<QByteArray> parts = slice.split('\0');
        for (const QByteArray &part : parts) {
            if (part.isEmpty()) {
                cells.append(QString());
                continue;
            }
            const QString s = decodeTextBytes(part, encodingHint);
            if (!isPrintableCell(s))
                return false;
            cells.append(s);
            ++nonEmptyCount;
        }
    } else {
        const QString all = decodeTextBytes(slice, encodingHint);
        const QStringList lines = all.split(QRegularExpression(QStringLiteral("\\r?\\n")));
        for (const QString &ln : lines) {
            if (ln.isEmpty()) {
                cells.append(QString());
                continue;
            }
            if (!isPrintableCell(ln))
                return false;
            cells.append(ln);
            ++nonEmptyCount;
        }
    }
    while (!cells.isEmpty() && cells.last().isEmpty())
        cells.removeLast();
    if (nonEmptyCount <= 0 || cells.isEmpty())
        return false;
    outTexts = cells;
    return true;
}

// Windows code page number -> QTextCodec name hint for decodeTextBytes(). 0 (or an unknown
// page) returns an empty hint = automatic detection (UTF-8, then Shift-JIS/CP932).
QString cpyuaCodecHint(int codePage)
{
    switch (codePage) {
    case 932:
        return QStringLiteral("Shift-JIS");
    case 936:
        return QStringLiteral("GBK");
    case 949:
        return QStringLiteral("EUC-KR");
    case 950:
        return QStringLiteral("Big5");
    case 65001:
        return QStringLiteral("UTF-8");
    default:
        break;
    }
    if (codePage <= 0)
        return QString();
    const QString candidates[] = { QStringLiteral("windows-%1").arg(codePage),
                                   QStringLiteral("CP%1").arg(codePage),
                                   QStringLiteral("IBM%1").arg(codePage) };
    for (const QString &name : candidates) {
        if (QTextCodec::codecForName(name.toUtf8()))
            return name;
    }
    return QString();
}

// `codePage` selects the 8-bit text encoding of column names / comments (Windows code page
// number, e.g. 932 = Shift-JIS); 0 keeps the automatic detection used so far.
bool parseCpyuaOpjuBuffer(const QByteArray &data, OpjuProject &proj, QString *outErr,
                          int codePage = 0)
{
    proj.isCpyuaContainer = true;
    const QString textHint = cpyuaCodecHint(codePage);
    if (data.size() < 64 || !data.startsWith("CPYU")) {
        if (outErr)
            *outErr = QObject::tr("Invalid CPYUA .opju header.");
        return false;
    }

    // Locate standard Origin global header marker: 0x7c 0x00 0x00 0x00 0x0a
    const QByteArray ghMarker("\x7c\x00\x00\x00\x0a", 5);
    const int ghPos = data.indexOf(ghMarker);
    if (ghPos < 0) {
        if (outErr)
            *outErr = QObject::tr("Could not locate Origin global header in CPYUA .opju file.");
        return false;
    }

    qint64 pos = static_cast<qint64>(ghPos) + 5 + 0x7C + 1 + 5;
    if (pos >= data.size()) {
        if (outErr)
            *outErr = QObject::tr("Truncated CPYUA .opju file after global header.");
        return false;
    }

    struct RawCpyuaDataset
    {
        int oneBasedId = 0;
        QString fullName;
        QString tableName;
        QString colName;
        QVector<double> values;
        QVector<QString> textValues;
        bool hasNumeric = false;
        bool isText = false;
        bool skipped = false;
        bool hasHeaderName = false;
        bool hasMetadata = false;
        SciDAVis::PlotDesignation designation = SciDAVis::Y;
        QString longName;
        QString units;
        QString comment;
    };

    QVector<RawCpyuaDataset> datasets;
    static const QRegularExpression kDsNameRe(QStringLiteral("[A-Za-z0-9_@.\\-]{2,}"));

    // 1. Walk sequential dataset list (0x80 zero-RLE header + 0x18 or raw data block).
    //    An unknown tag (`tag != 0x18U`) or non-0x05 block is safely skipped/decoded by size or
    //    bounds-checked resync to the next valid dataset header instead of terminating the walk.
    auto isDatasetHeaderAt = [&data](qint64 p) -> bool {
        if (p < 0 || p + 3 > data.size()
            || static_cast<unsigned char>(data[static_cast<int>(p)]) != 0x80U) {
            return false;
        }
        const int hl = static_cast<unsigned char>(data[static_cast<int>(p + 1)]);
        if (hl < 12 || hl > 220 || p + 2 + hl >= data.size())
            return false;
        QByteArray tmp;
        if (!decodeCpyuaZeroRle(data, p + 2, hl, tmp))
            return false;
        // Dataset headers decode to ~114-120 bytes (Window headers are ~538, Column headers ~468).
        if (tmp.size() < 80 || tmp.size() > 240)
            return false;
        const unsigned char nextTag = static_cast<unsigned char>(data[static_cast<int>(p + 2 + hl)]);
        if (nextTag == 0x18U)
            return true;
        if (tmp.size() >= 4 && static_cast<unsigned char>(tmp[2]) == 0x01U
            && static_cast<unsigned char>(tmp[3]) == 0xFAU)
            return true;
        if (tmp.size() > 0x30) {
            const QString hdrAscii = QString::fromLatin1(tmp.constData() + 0x30, tmp.size() - 0x30);
            if (kDsNameRe.match(hdrAscii).hasMatch())
                return true;
        }
        return false;
    };
    auto isDatasetListEndAt = [&data](qint64 p) -> bool {
        return p >= 0 && p + 6 <= data.size() && data[static_cast<int>(p)] == '\0'
                && data[static_cast<int>(p + 1)] == '\0' && data[static_cast<int>(p + 2)] == '\0'
                && data[static_cast<int>(p + 3)] == '\0' && data[static_cast<int>(p + 4)] == '\0'
                && data[static_cast<int>(p + 5)] == '\x0A';
    };
    // Finds the next valid dataset header strictly after `from` (stopping at the end-of-dataset
    // marker before Window/Column metadata); returns -1 when there is none.
    auto findNextDatasetHeader = [&](qint64 from, qint64 *outEndMarker = nullptr) -> qint64 {
        for (qint64 p = from + 1; p + 3 < data.size(); ++p) {
            if (isDatasetListEndAt(p)) {
                if (outEndMarker)
                    *outEndMarker = p;
                return -1;
            }
            if (isDatasetHeaderAt(p))
                return p;
        }
        return -1;
    };

    int dsSeq = 0;
    qint64 lastGoodEnd = pos; // end of the last processed dataset block
    while (pos + 2 < data.size() && static_cast<unsigned char>(data[static_cast<int>(pos)]) == 0x80U) {
        const qint64 blockStart = pos;
        const int hdrLen = static_cast<unsigned char>(data[static_cast<int>(pos + 1)]);
        QByteArray hdrDec;
        bool blockOk = decodeCpyuaZeroRle(data, pos + 2, hdrLen, hdrDec);
        if (blockOk && hdrDec.size() >= 380) {
            // Reached a Window/Column header block rather than a ~114-120B dataset header.
            break;
        }
        qint64 payloadPos = 0;
        quint64 compSz = 0;
        qint64 blockEnd = 0;
        if (blockOk) {
            const qint64 afterHdr = pos + 2 + hdrLen;
            if (afterHdr >= data.size()) {
                blockOk = false;
            } else {
                const unsigned char tag =
                        static_cast<unsigned char>(data[static_cast<int>(afterHdr)]);
                if (tag == 0x18U
                    || (tag >= 0x10U && tag < 0x80U && afterHdr + 1 + tag < data.size()
                        && static_cast<unsigned char>(data[static_cast<int>(afterHdr + 1 + tag)])
                                == 0x0AU)) {
                    // <tag:1B> <meta:tag B (8B uncomp + 8B comp + ...)> <0x0A> <payload> <4B maskSz> <mask>
                    const qint64 bodyPos = afterHdr + 1;
                    const int descLen = static_cast<int>(tag);
                    quint64 uncompSz = 0;
                    blockOk = bodyPos + descLen + 1 <= data.size()
                            && readU64LE(data, bodyPos, uncompSz)
                            && readU64LE(data, bodyPos + 8, compSz);
                    Q_UNUSED(uncompSz);
                    if (blockOk) {
                        payloadPos = bodyPos + descLen + 1;
                        blockOk = compSz <= static_cast<quint64>(data.size() - payloadPos);
                    }
                    if (blockOk) {
                        quint32 maskSz = 0;
                        blockEnd = payloadPos + static_cast<qint64>(compSz);
                        blockOk = readU32LE(data, blockEnd, maskSz)
                                && blockEnd + 4 + static_cast<qint64>(maskSz) <= data.size();
                        if (blockOk)
                            blockEnd += 4 + static_cast<qint64>(maskSz);
                    }
                } else if (afterHdr + 5 <= data.size()
                           && static_cast<unsigned char>(data[static_cast<int>(afterHdr + 4)])
                                   == 0x0AU) {
                    // Raw Origin block framing: <uint32_le rawSz> <0x0A> <payload> [<4B maskSz> <mask>]
                    quint32 rawSz = 0;
                    blockOk = readU32LE(data, afterHdr, rawSz)
                            && static_cast<qint64>(rawSz) <= (data.size() - (afterHdr + 5));
                    if (blockOk) {
                        payloadPos = afterHdr + 5;
                        compSz = rawSz;
                        blockEnd = payloadPos + static_cast<qint64>(compSz);
                        quint32 maskSz = 0;
                        if (blockEnd + 4 <= data.size() && readU32LE(data, blockEnd, maskSz)
                            && blockEnd + 4 + static_cast<qint64>(maskSz) <= data.size()) {
                            const qint64 withMask = blockEnd + 4 + static_cast<qint64>(maskSz);
                            if (withMask == data.size() || isDatasetHeaderAt(withMask)
                                || isDatasetListEndAt(withMask)) {
                                blockEnd = withMask;
                            }
                        }
                    }
                } else {
                    blockOk = false;
                }
            }
        }
        if (!blockOk) {
            // Unknown tag or unframed block: resync to the next valid dataset header.
            qint64 endMarker = -1;
            const qint64 next = findNextDatasetHeader(blockStart, &endMarker);
            ++dsSeq;
            RawCpyuaDataset skippedDs;
            skippedDs.oneBasedId = dsSeq;
            skippedDs.skipped = true;
            datasets.append(skippedDs);
            if (next < 0) {
                if (endMarker >= 0)
                    lastGoodEnd = endMarker;
                break;
            }
            pos = next;
            lastGoodEnd = pos;
            continue;
        }

        const QByteArray payload = data.mid(static_cast<int>(payloadPos), static_cast<int>(compSz));
        pos = blockEnd;
        lastGoodEnd = pos;

        ++dsSeq;
        RawCpyuaDataset ds;
        ds.oneBasedId = dsSeq;

        // Extract dataset name: in CPYUA 114-120 byte headers, the 22-byte null-terminated name
        // field sits at offset (hdrDec.size() - 53) right before 0xCA 0x10; fall back to regex
        // scan from offset 0x30.
        if (hdrDec.size() >= 64) {
            const int fixedOff = hdrDec.size() - 53;
            int nul = hdrDec.indexOf('\0', fixedOff);
            if (nul < 0 || nul > fixedOff + 22)
                nul = std::min(hdrDec.size(), fixedOff + 22);
            if (nul > fixedOff) {
                const QString cand =
                        decodeTextBytes(hdrDec.mid(fixedOff, nul - fixedOff), textHint).trimmed();
                if (cand.contains(QLatin1Char('_'))) {
                    ds.fullName = cand;
                    ds.hasHeaderName = true;
                }
            }
        }
        if (ds.fullName.isEmpty() && hdrDec.size() > 0x30) {
            const QString hdrText = decodeTextBytes(hdrDec.mid(0x30), textHint);
            const QRegularExpressionMatch m = kDsNameRe.match(hdrText);
            if (m.hasMatch()) {
                ds.fullName = m.captured(0);
                ds.hasHeaderName = true;
            }
        }
        if (ds.fullName.isEmpty())
            ds.fullName = QStringLiteral("Book1_Col%1").arg(dsSeq);

        const int usPos = ds.fullName.lastIndexOf(QLatin1Char('_'));
        if (usPos > 0 && usPos + 1 < ds.fullName.size()) {
            ds.tableName = ds.fullName.left(usPos);
            ds.colName = ds.fullName.mid(usPos + 1);
        } else {
            ds.tableName = QStringLiteral("Book1");
            ds.colName = ds.fullName;
        }

        if (decodeCpyuaNumericPayload(payload, ds.values)) {
            ds.hasNumeric = true;
        } else if (decodeCpyuaTextPayload(payload, textHint, ds.textValues)) {
            ds.isText = true;
        } else {
            // Non-0x05 block with no decodable numeric/text payload: keep dataset slot for ID
            // alignment and continue to subsequent datasets instead of aborting (D4-8).
            ds.skipped = true;
        }
        datasets.append(ds);
    }
    pos = lastGoodEnd;

    if (datasets.isEmpty()) {
        if (outErr)
            *outErr = QObject::tr("No readable datasets found in CPYUA .opju file.");
        return false;
    }

    // 2. Scan post-dataset Window/Column section for 468-byte (0x1D4) column info records
    //    Format: 0x80 <len> <zero-RLE -> 468 bytes starting with 0x10, uint16 ds_id, ..., byte desig, shortName>
    //    Followed optionally by: 0x00 <totalLen> <strLen> "LongName\r\nUnits\r\nComments\0"
    QHash<int, int> idToIndex;
    for (int i = 0; i < datasets.size(); ++i)
        idToIndex.insert(datasets[i].oneBasedId, i);

    const qint64 metaStart = pos;
    QSet<QString> seenCpyuaGraphNames;
    QSet<QString> seenCpyuaMatrixNames;
    QString curWorkbookName;
    int seqShift = 0;

    auto datasetMatchesColumnName = [](const RawCpyuaDataset &cand, const QString &winName,
                                       const QString &sn) -> bool {
        if (cand.skipped || !cand.hasHeaderName || sn.isEmpty())
            return false;
        if (!winName.isEmpty()) {
            const QString expectedFull = winName + QLatin1Char('_') + sn;
            if (cand.fullName.compare(expectedFull, Qt::CaseInsensitive) == 0)
                return true;
            if (cand.fullName.size() >= 20
                && expectedFull.startsWith(cand.fullName, Qt::CaseInsensitive))
                return true;
        }
        if (cand.colName.compare(sn, Qt::CaseInsensitive) == 0)
            return true;
        if (cand.fullName.endsWith(QLatin1Char('_') + sn, Qt::CaseInsensitive))
            return true;
        return cand.fullName.size() >= 20
                && sn.startsWith(cand.colName, Qt::CaseInsensitive);
    };

    for (qint64 p = metaStart; p + 4 < data.size(); ++p) {
        if (static_cast<unsigned char>(data[static_cast<int>(p)]) != 0x80U)
            continue;
        const int ln = static_cast<unsigned char>(data[static_cast<int>(p + 1)]);
        if (ln < 16 || ln > 240 || p + 2 + ln > data.size())
            continue;

        // Check non-column window header blocks (~538B Workbook, ~729B Graph, Matrix) so CPYUA
        // imports report skipped graph/matrix windows instead of silently dropping them.
        if (static_cast<unsigned char>(data[static_cast<int>(p + 2)]) != 0x01U
            || static_cast<unsigned char>(data[static_cast<int>(p + 3)]) != 0x10U) {
            if (ln >= 64) {
                QByteArray winDec;
                if (decodeCpyuaZeroRle(data, p + 2, ln, winDec) && winDec.size() >= 500
                    && winDec.size() <= 920 && winDec[0] == '\0' && winDec[1] == '\0'
                    && static_cast<unsigned char>(winDec[2]) >= 0x20U && winDec.size() > 60
                    && winDec[56] == '\0') {
                    int nameEnd = 2;
                    bool nameCtrlClean = true;
                    while (nameEnd < std::min(winDec.size(), 56) && winDec[nameEnd] != '\0') {
                        const unsigned char ch = static_cast<unsigned char>(winDec[nameEnd]);
                        if (ch < 0x20U || ch == 0x7FU) {
                            nameCtrlClean = false;
                            break;
                        }
                        ++nameEnd;
                    }
                    int tplEnd = 57;
                    bool tplAscii = true;
                    while (tplEnd < std::min(winDec.size(), 96) && winDec[tplEnd] != '\0') {
                        const unsigned char ch = static_cast<unsigned char>(winDec[tplEnd]);
                        if (ch < 0x20U || ch >= 0x7FU) {
                            tplAscii = false;
                            break;
                        }
                        ++tplEnd;
                    }
                    if (nameCtrlClean && nameEnd > 2 && nameEnd < 56 && tplAscii
                        && tplEnd > 57 && tplEnd < 96) {
                        const QString winName =
                                decodeTextBytes(winDec.mid(2, nameEnd - 2), textHint).trimmed();
                        const QString tplName =
                                QString::fromLatin1(winDec.constData() + 57, tplEnd - 57).trimmed();
                        const QByteArray tailSlice = data.mid(
                                static_cast<int>(p + 2 + ln),
                                static_cast<int>(std::min<qint64>(1024, data.size() - (p + 2 + ln))));
                        if (!winName.isEmpty()
                            && (tplName.contains(QLatin1String("MATRIX"), Qt::CaseInsensitive)
                                || tplName.compare(QLatin1String("ORIGINM"), Qt::CaseInsensitive)
                                        == 0)) {
                            seenCpyuaMatrixNames.insert(winName.toLower());
                        } else if (!winName.isEmpty()
                                   && tplName.compare(QLatin1String("ORIGIN"), Qt::CaseInsensitive)
                                           != 0
                                   && (winDec.size() >= 640
                                       || tailSlice.contains("AxesDlgSettings")
                                       || tailSlice.contains("Page>"))) {
                            seenCpyuaGraphNames.insert(winName.toLower());
                        } else if (!winName.isEmpty()) {
                            curWorkbookName = winName;
                        }
                    }
                }
            }
            continue;
        }

        if (ln > 200)
            continue;

        QByteArray colDec;
        if (!decodeCpyuaZeroRle(data, p + 2, ln, colDec))
            continue;
        if (colDec.size() < 400 || colDec.size() > 520
            || static_cast<unsigned char>(colDec[0]) != 0x10U) {
            continue;
        }

        quint16 dsId = 0;
        if (!readU16LE(colDec, 1, dsId))
            continue;

        // Extract short name at offset 9..24 before lookup so resync skips >1 dataset cannot
        // shift 0x1D4 metadata onto a later column.
        int nameEnd = 9;
        while (nameEnd < std::min(colDec.size(), 24) && colDec[nameEnd] != '\0')
            ++nameEnd;
        const QString sn = (nameEnd > 9)
                ? decodeTextBytes(colDec.mid(9, nameEnd - 9), textHint).trimmed()
                : QString();

        int targetIdx = idToIndex.value(static_cast<int>(dsId) - seqShift, -1);
        if (targetIdx >= 0) {
            const RawCpyuaDataset &cand = datasets[targetIdx];
            if (cand.skipped || cand.hasMetadata
                || (!sn.isEmpty() && cand.hasHeaderName
                    && !datasetMatchesColumnName(cand, curWorkbookName, sn))) {
                targetIdx = -1;
            }
        }
        if (targetIdx < 0 && !sn.isEmpty()) {
            const QString expectedFull =
                    curWorkbookName.isEmpty() ? QString() : (curWorkbookName + QLatin1Char('_') + sn);
            int fallbackIdx = -1;
            for (int j = 0; j < datasets.size(); ++j) {
                const RawCpyuaDataset &cand = datasets[j];
                if (cand.skipped || cand.hasMetadata || cand.oneBasedId > static_cast<int>(dsId))
                    continue;
                if (!expectedFull.isEmpty()
                    && cand.fullName.compare(expectedFull, Qt::CaseInsensitive) == 0) {
                    targetIdx = j;
                    break;
                }
                if (fallbackIdx < 0 && datasetMatchesColumnName(cand, curWorkbookName, sn))
                    fallbackIdx = j;
            }
            if (targetIdx < 0)
                targetIdx = fallbackIdx;
            if (targetIdx >= 0)
                seqShift = static_cast<int>(dsId) - datasets[targetIdx].oneBasedId;
        }
        if (targetIdx < 0)
            continue;

        RawCpyuaDataset &ds = datasets[targetIdx];
        if (ds.hasMetadata)
            continue;
        ds.hasMetadata = true;

        const unsigned char desigByte = static_cast<unsigned char>(colDec[8]);
        switch (desigByte) {
        case 0x03U:
            ds.designation = SciDAVis::X;
            break;
        case 0x00U:
            ds.designation = SciDAVis::Y;
            break;
        case 0x05U:
            ds.designation = SciDAVis::Z;
            break;
        case 0x02U:
            ds.designation = SciDAVis::yErr;
            break;
        case 0x06U:
            ds.designation = SciDAVis::xErr;
            break;
        case 0x01U:
        case 0x04U:
            ds.designation = SciDAVis::noDesignation;
            break;
        default:
            ds.designation = SciDAVis::Y;
            break;
        }

        if (!sn.isEmpty())
            ds.colName = sn;

        // Trailing LongName\r\nUnits\r\nComments string immediately after zero-RLE block
        const qint64 tail = p + 2 + ln;
        if (tail + 3 < data.size() && data[static_cast<int>(tail)] == '\0') {
            const int totalLen = static_cast<unsigned char>(data[static_cast<int>(tail + 1)]);
            const int strLen = static_cast<unsigned char>(data[static_cast<int>(tail + 2)]);
            if (totalLen > 0 && strLen > 0 && strLen <= totalLen
                && tail + 3 + strLen <= data.size()) {
                const QString metaStr = decodeTextBytes(
                        data.mid(static_cast<int>(tail + 3), strLen), textHint);
                const QStringList parts = metaStr.split(QStringLiteral("\r\n"));
                if (!parts.isEmpty())
                    ds.longName = parts[0].trimmed();
                if (parts.size() >= 2)
                    ds.units = parts[1].trimmed();
                if (parts.size() >= 3)
                    ds.comment = parts.mid(2).join(QLatin1Char(' ')).trimmed();
            }
        }
    }
    proj.skippedGraphs += seenCpyuaGraphNames.size();
    proj.skippedMatrices += seenCpyuaMatrixNames.size();

    // 3. Group datasets into Worksheets preserving appearance order
    QHash<QString, int> wsIdxByName;
    for (const RawCpyuaDataset &ds : datasets) {
        if (ds.skipped) {
            ++proj.skippedDatasets;
            continue;
        }
        const QString key = ds.tableName.toLower();
        int wsIdx = wsIdxByName.value(key, -1);
        if (wsIdx < 0) {
            OpjuWorksheet ws;
            ws.workbookName = ds.tableName;
            ws.sheetName = QStringLiteral("Sheet1");
            ws.resolvedName = ds.tableName;
            wsIdx = proj.worksheets.size();
            proj.worksheets.append(ws);
            wsIdxByName.insert(key, wsIdx);
        }

        OpjuWorksheet &ws = proj.worksheets[wsIdx];
        bool wsHasNumericX = false;
        for (const OpjuColumn &existing : ws.columns) {
            if (existing.designation == SciDAVis::X) {
                wsHasNumericX = true;
                break;
            }
        }
        OpjuColumn col;
        col.shortName = ds.colName;
        col.longName = ds.longName;
        col.units = ds.units;
        col.comment = ds.comment;
        if (ds.hasMetadata) {
            col.designation = ds.designation;
        } else if (ds.isText) {
            col.designation = SciDAVis::noDesignation;
        } else {
            col.designation = wsHasNumericX ? SciDAVis::Y : SciDAVis::X;
        }
        if (ds.isText) {
            col.dataType = OpjuDataType::Text;
            col.hasText = true;
            col.textValues = ds.textValues;
        } else {
            col.dataType = OpjuDataType::Double;
            col.hasNumeric = true;
            col.numericValues = ds.values;
        }
        ws.columns.append(col);
    }

    if (proj.worksheets.isEmpty()) {
        if (outErr)
            *outErr = QObject::tr("No readable datasets found in CPYUA .opju file.");
        return false;
    }
    return true;
}

} // namespace

bool ImportOPJU::isZipOpjuFile(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(4);
    f.close();
    if (head.size() < 4)
        return false;
    return (head[0] == 'P' && head[1] == 'K'
            && ((head[2] == 0x03 && head[3] == 0x04) || (head[2] == 0x05 && head[3] == 0x06)
                || (head[2] == 0x07 && head[3] == 0x08)));
}

bool ImportOPJU::isCpyuaOpjuFile(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(5);
    f.close();
    return head.startsWith("CPYU");
}

bool ImportOPJU::isSupportedOpjuFile(const QString &fileName)
{
    return isZipOpjuFile(fileName) || isCpyuaOpjuFile(fileName);
}

bool ImportOPJU::importFile(ApplicationWindow *app, const QString &fileName, QString *outError)
{
    ImportStats stats;
    return importFile(app, fileName, outError, &stats, 0);
}

bool ImportOPJU::importFile(ApplicationWindow *app, const QString &fileName, QString *outError,
                            int codePage)
{
    ImportStats stats;
    return importFile(app, fileName, outError, &stats, codePage);
}

bool ImportOPJU::importFile(ApplicationWindow *app, const QString &fileName, QString *outError,
                            ImportStats *outStats, int codePage)
{
    registerOpjuTranslations();
    if (!app) {
        if (outError)
            *outError = QStringLiteral("ApplicationWindow is null.");
        return false;
    }

    app->setStatusBarText(QObject::tr("Importing Origin .opju project ..."));

    OpjuProject project;
    QString err;

    if (isCpyuaOpjuFile(fileName)) {
        QFile f(fileName);
        if (f.size() <= 0 || f.size() > kMaxArchiveBytes || !f.open(QIODevice::ReadOnly)) {
            app->setStatusBarText(QString());
            if (outError)
                *outError = QObject::tr("Failed to read CPYUA .opju file '%1'.").arg(fileName);
            return false;
        }
        const QByteArray raw = f.readAll();
        f.close();
        if (!parseCpyuaOpjuBuffer(raw, project, &err, codePage)) {
            app->setStatusBarText(QString());
            if (outError)
                *outError = err;
            return false;
        }
    } else {
        ZipArchive zip;
        if (!zip.open(fileName, &err)) {
            app->setStatusBarText(QString());
            if (outError)
                *outError = QObject::tr("Invalid .opju file: %1").arg(err);
            return false;
        }

        OpjuXmlParser parser(zip, project);
        if (!parser.parseArchive(&err)) {
            // Fallback: check if the ZIP contains an embedded CPYUA stream
            bool decodedEmbeddedCpyua = false;
            for (int i = 0; i < zip.entries().size(); ++i) {
                QByteArray entryBytes;
                QString ignored;
                if (zip.extract(i, entryBytes, &ignored) && entryBytes.startsWith("CPYU")) {
                    if (parseCpyuaOpjuBuffer(entryBytes, project, &ignored, codePage)) {
                        decodedEmbeddedCpyua = true;
                        break;
                    }
                }
            }
            if (!decodedEmbeddedCpyua) {
                app->setStatusBarText(QString());
                if (outError)
                    *outError = err;
                return false;
            }
        }
    }

    ImportStats stats;
    const bool ok = materializeProject(app, project, stats);
    if (stats.graphsSkipped > 0)
        stats.warnings << QObject::tr(
                                  "%1 graph window(s) were skipped (unsupported binary "
                                  "CPYUA/graph layout)")
                                  .arg(stats.graphsSkipped);
    if (stats.matricesSkipped > 0)
        stats.warnings << QObject::tr("%1 matrix window(s) were skipped").arg(stats.matricesSkipped);
    if (stats.datasetsSkipped > 0)
        stats.warnings << QObject::tr("%1 dataset(s) could not be decoded and were skipped")
                                  .arg(stats.datasetsSkipped);

    const QString joinedWarn = stats.warnings.join(QLatin1Char('\n'));
    app->setProperty("torigraph_last_opju_warnings", joinedWarn);
    if (ok && !stats.warnings.isEmpty()) {
        app->showResults(QStringLiteral("[.opju] %1\n%2\n")
                                 .arg(QFileInfo(fileName).fileName(), joinedWarn),
                         app->logWindow.isVisible());
        app->setStatusBarText(stats.warnings.join(QStringLiteral("; ")));
        if (!ApplicationWindow::isNonInteractiveContext(app))
            QMessageBox::information(app, QObject::tr("Origin .opju Import"), joinedWarn);
    } else {
        app->setStatusBarText(QString());
    }
    if (outStats)
        *outStats = stats;
    if (!ok && outError && outError->isEmpty())
        *outError = QObject::tr("Failed to construct ToriGraph windows from .opju project.");
    return ok;
}
